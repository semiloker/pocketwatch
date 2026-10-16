// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/pgf.h"

#define HEADER_REV2  392u
#define HEADER_REV3  412u
#define TABLE_ENTRY  8u /* {s32 H, s32 V} */
#define GLYPH_FIXED  (14 + 7 + 7 + 7 + 7 + 6 + 7 + 9)
#define FLAG_DIM     0x04u
#define FLAG_XADJ    0x08u
#define FLAG_YADJ    0x10u
#define FLAG_ADV     0x20u
#define MODE_MASK    0x03u
#define MODE_ROWS    1u
#define MODE_COLUMNS 2u
#define MODE_OVERLAY 3u
#define OVERLAY_MAX  3

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

/* Bytes taken by a bit stream of n entries of bpe bits, padded to 32 bits. */
static unsigned int stream_bytes(unsigned int n, unsigned int bpe)
{
    unsigned long long bits = (unsigned long long)n * bpe;

    if (bits > 0x7FFFFFFFull)
        return 0xFFFFFFFFu;
    return (unsigned int)(((bits + 31) & ~31ull) / 8);
}

/* LSB-first bit reader over [off, off + len) of the file. */
typedef struct {
    const unsigned char *d;
    unsigned int end_bit; /* absolute bit index just past the readable range */
    unsigned int pos;     /* absolute bit index */
    int bad;
} bits;

static void bits_init(bits *b, const pw_pgf *f, unsigned int byte_off, unsigned int bit_pos)
{
    b->d = f->data;
    b->end_bit = f->size * 8u;
    b->pos = byte_off * 8u + bit_pos;
    b->bad = byte_off > f->size;
}

static unsigned int bits_get(bits *b, int n)
{
    unsigned int v = 0;

    if (b->bad || b->pos + (unsigned int)n > b->end_bit || b->pos + (unsigned int)n < b->pos) {
        b->bad = 1;
        return 0;
    }
    for (int i = 0; i < n; i++, b->pos++)
        v |= (unsigned int)((b->d[b->pos >> 3] >> (b->pos & 7)) & 1) << i;
    return v;
}

static int sext7(unsigned int v)
{
    return v >= 64 ? (int)v - 128 : (int)v;
}

int pw_pgf_open(pw_pgf *f, const void *data, unsigned int size)
{
    const unsigned char *d = data;
    unsigned int header, pos, len;

    f->data = d;
    f->size = size;
    if (size < HEADER_REV2 || d[4] != 'P' || d[5] != 'G' || d[6] != 'F' || d[7] != '0')
        return -1;
    f->revision = (int)rd32(d + 0x08);
    if (f->revision != 2 && f->revision != 3)
        return -2;
    header = f->revision == 3 ? HEADER_REV3 : HEADER_REV2;
    if (size < header || d[0x22] != 4) /* 4 bits per pixel */
        return -3;

    unsigned int charmap_len = rd32(d + 0x10);
    f->glyphs = rd32(d + 0x14);
    f->charmap_bpe = rd32(d + 0x18);
    f->charptr_bpe = rd32(d + 0x1C);
    f->size_h = (int)rd32(d + 0x24);
    f->res_h = (int)rd32(d + 0x2C);
    f->first = rd16(d + 0xB6);
    f->last = rd16(d + 0xB8);
    f->ascender = (int)rd32(d + 0xD4);
    f->descender = (int)rd32(d + 0xD8);
    f->max_w = (int)rd16(d + 0xFC);
    f->max_h = (int)rd16(d + 0xFE);
    f->dim_len = d[0x102];
    f->xadj_len = d[0x103];
    f->yadj_len = d[0x104];
    f->adv_len = d[0x105];
    unsigned int shadowmap_len = rd32(d + 0x16C);
    f->shadowmap_bpe = rd32(d + 0x170);
    if (f->charmap_bpe == 0 || f->charmap_bpe > 32 || f->charptr_bpe == 0 || f->charptr_bpe > 32 ||
        f->shadowmap_bpe > 32 || f->last < f->first || charmap_len < f->last - f->first + 1 ||
        f->max_w <= 0 || f->max_h <= 0 || f->max_w > 127 || f->max_h > 127)
        return -4;

    pos = header;
    f->tables_off = pos;
    pos += (f->dim_len + f->xadj_len + f->yadj_len + f->adv_len) * TABLE_ENTRY;
    pos += stream_bytes(shadowmap_len, f->shadowmap_bpe);
    if (f->revision == 3) {
        unsigned int comp1 = rd16(d + 0x18C), comp2 = rd16(d + 0x194);

        pos += (comp1 + comp2) * 4u; /* {u16 start, u16 count} each */
    }
    f->charmap_off = pos;
    len = stream_bytes(charmap_len, f->charmap_bpe);
    if (len == 0xFFFFFFFFu || pos + len < pos)
        return -5;
    pos += len;
    f->charptr_off = pos;
    len = stream_bytes(f->glyphs, f->charptr_bpe);
    if (len == 0xFFFFFFFFu || pos + len < pos)
        return -5;
    pos += len;
    f->fontdata_off = pos;
    return pos <= size ? 0 : -6;
}

static int table_value(const pw_pgf *f, unsigned int table, unsigned int index, int *h)
{
    unsigned int lens[4] = {f->dim_len, f->xadj_len, f->yadj_len, f->adv_len};
    unsigned int off = f->tables_off;

    for (unsigned int t = 0; t < table; t++)
        off += lens[t] * TABLE_ENTRY;
    if (index >= lens[table])
        return -1;
    off += index * TABLE_ENTRY;
    *h = (int)rd32(f->data + off);
    return 0;
}

static int glyph_bit_offset(const pw_pgf *f, unsigned int code, unsigned int *bit)
{
    bits b;
    unsigned int index, ptr;

    if (code < f->first || code > f->last)
        return -1;
    bits_init(&b, f, f->charmap_off, (code - f->first) * f->charmap_bpe);
    index = bits_get(&b, (int)f->charmap_bpe);
    if (b.bad || index >= f->glyphs)
        return -1;
    bits_init(&b, f, f->charptr_off, index * f->charptr_bpe);
    ptr = bits_get(&b, (int)f->charptr_bpe);
    if (b.bad || ptr > (0xFFFFFFFFu >> 5))
        return -1;
    *bit = ptr * 32u;
    return 0;
}

int pw_pgf_has(const pw_pgf *f, unsigned int code)
{
    unsigned int bit;

    return glyph_bit_offset(f, code, &bit) == 0;
}

static int render(const pw_pgf *f, unsigned int code, pw_pgf_glyph *g, unsigned char *out,
                  unsigned char *scratch, unsigned int room, int depth)
{
    bits b;
    unsigned int bit, flags, mode, n;
    int adv = 0;

    if (glyph_bit_offset(f, code, &bit) < 0)
        return -1;
    bits_init(&b, f, f->fontdata_off, bit);
    bits_get(&b, 14); /* record size in bytes (the shadow glyph follows) */
    g->w = (int)bits_get(&b, 7);
    g->h = (int)bits_get(&b, 7);
    g->left = sext7(bits_get(&b, 7));
    g->top = sext7(bits_get(&b, 7));
    flags = bits_get(&b, 6);
    bits_get(&b, 7); /* shadow flags */
    bits_get(&b, 9); /* shadow id */
    for (unsigned int t = 0; t < 4; t++) {
        unsigned int flag = FLAG_DIM << t;
        int h;

        if (flags & flag) {
            if (table_value(f, t, bits_get(&b, 8), &h) < 0)
                return -2;
        } else {
            h = (int)bits_get(&b, 32);
            bits_get(&b, 32); /* V */
        }
        if (t == 3)
            adv = h;
    }
    g->adv = adv;
    n = (unsigned int)(g->w * g->h);
    if (b.bad || n > room) /* the header's max size is not a hard limit (descenders exceed it) */
        return -2;

    for (unsigned int i = 0; i < n; i++)
        out[i] = 0;
    mode = flags & MODE_MASK;
    if (mode == MODE_OVERLAY) {
        unsigned int parts[OVERLAY_MAX];

        if (depth > 0)
            return -3;
        for (int i = 0; i < OVERLAY_MAX; i++)
            parts[i] = bits_get(&b, 16);
        if (b.bad)
            return -2;
        for (int i = 0; i < OVERLAY_MAX; i++) {
            pw_pgf_glyph sub;

            if (parts[i] == 0 || parts[i] == code)
                continue;
            if (render(f, parts[i], &sub, scratch, 0, room, depth + 1) < 0)
                continue;
            int dx = sub.left - g->left, dy = g->top - sub.top;

            for (int y = 0; y < sub.h; y++)
                for (int x = 0; x < sub.w; x++) {
                    int ox = x + dx, oy = y + dy;
                    unsigned char a = scratch[y * sub.w + x];

                    if (ox >= 0 && oy >= 0 && ox < g->w && oy < g->h && a > out[oy * g->w + ox])
                        out[oy * g->w + ox] = a;
                }
        }
        return 0;
    }
    if (mode != MODE_ROWS && mode != MODE_COLUMNS)
        return -2;

    /* 4-bit RLE: n < 8 repeats the next value n+1 times, n >= 8 is 16-n literal values. */
    for (unsigned int i = 0; i < n && !b.bad;) {
        unsigned int run = bits_get(&b, 4), count, value = 0;
        int literal = run >= 8;

        count = literal ? 16 - run : run + 1;
        if (!literal)
            value = bits_get(&b, 4);
        for (unsigned int k = 0; k < count && i < n; k++, i++) {
            unsigned int v = literal ? bits_get(&b, 4) : value, x, y;

            if (mode == MODE_ROWS) {
                x = i % (unsigned int)g->w;
                y = i / (unsigned int)g->w;
            } else {
                y = i % (unsigned int)g->h;
                x = i / (unsigned int)g->h;
            }
            out[y * (unsigned int)g->w + x] = (unsigned char)(v * 17);
        }
    }
    return b.bad ? -2 : 0;
}

unsigned int pw_pgf_glyph_room(const pw_pgf *f)
{
    return (unsigned int)((f->max_w + 16) * (f->max_h + 16));
}

int pw_pgf_render(const pw_pgf *f, unsigned int code, pw_pgf_glyph *g, unsigned char *alpha,
                  unsigned int cap)
{
    unsigned int half = cap / 2;

    if (half < (unsigned int)(f->max_w * f->max_h))
        return -4;
    return render(f, code, g, alpha, alpha + half, half, 0);
}
