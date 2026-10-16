// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/rco.h"
#include "gfx/rlz.h"

#define TABLES_AT    0xA4u
#define PACKED_HDR   0x0Cu /* {packed, unpacked, longest text} before compressed tables */
#define ENTRY_SIZE   0x28u
#define IMG_INFO     0x10u /* format, compression, packed size, offset (+ unpacked size) */
#define H_COMPRESS   0x0C
#define H_IMG_TABLE  0x24
#define H_LABEL_DATA 0x40
#define H_LABEL_LEN  0x44
#define H_IMG_DATA   0x80

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

/* A pointer into the table area for a file offset, or NULL when [off, off+n) is outside it. */
static const unsigned char *at(const pw_rco *r, unsigned int off, unsigned int n)
{
    if (off < TABLES_AT || off - TABLES_AT > r->tables_len || n > r->tables_len - (off - TABLES_AT))
        return 0;
    return r->tables + (off - TABLES_AT);
}

int pw_rco_open(pw_rco *r, const void *file, unsigned int size, void *buf, unsigned int cap,
                unsigned char *state, pw_rco_inflate_fn inflate)
{
    const unsigned char *f = file;
    unsigned int comp;

    r->file = f;
    r->size = size;
    if (size < TABLES_AT + PACKED_HDR || f[0] != 0 || f[1] != 'P' || f[2] != 'R' || f[3] != 'F')
        return -1;
    comp = rd32(f + H_COMPRESS) >> 4;
    r->img_table = rd32(f + H_IMG_TABLE);
    r->label_off = rd32(f + H_LABEL_DATA);
    r->label_len = rd32(f + H_LABEL_LEN);
    r->img_data = rd32(f + H_IMG_DATA);
    if (comp == PW_RCO_NONE) {
        r->tables = f + TABLES_AT;
        r->tables_len = size - TABLES_AT;
        return 0;
    }
    unsigned int packed = rd32(f + TABLES_AT), unpacked = rd32(f + TABLES_AT + 4);
    int got;

    if (packed > size - TABLES_AT - PACKED_HDR || unpacked > cap)
        return -2;
    if (comp == PW_RCO_RLZ)
        got = pw_rlz_decode(f + TABLES_AT + PACKED_HDR, packed, buf, cap, state);
    else if (comp == PW_RCO_ZLIB && inflate)
        got = inflate(f + TABLES_AT + PACKED_HDR, packed, buf, cap);
    else
        return -3;
    if (got < 0 || (unsigned int)got < unpacked)
        return -4;
    r->tables = buf;
    r->tables_len = unpacked;
    return 0;
}

static int label_is(const pw_rco *r, unsigned int off, const char *want)
{
    const unsigned char *l = at(r, r->label_off, r->label_len);
    unsigned int i = 0;

    if (l == 0 || off >= r->label_len)
        return 0;
    for (; off + i < r->label_len && want[i]; i++)
        if (l[off + i] != (unsigned char)want[i])
            return 0;
    return want[i] == 0 && off + i < r->label_len && l[off + i] == 0;
}

int pw_rco_find_image(const pw_rco *r, const char *label, pw_rco_image *img)
{
    const unsigned char *head = at(r, r->img_table, ENTRY_SIZE);
    unsigned int count, cur;

    if (head == 0)
        return -1;
    count = rd32(head + 0x10);
    cur = r->img_table + (rd32(head + 0x08) ? rd32(head + 0x08) : ENTRY_SIZE);
    for (unsigned int i = 0; i < count && i < 4096; i++) {
        const unsigned char *e = at(r, cur, ENTRY_SIZE + IMG_INFO + 4);
        unsigned int next;

        if (e == 0)
            return -2;
        if (label_is(r, rd32(e + 0x04), label)) {
            img->format = (int)rd16(e + ENTRY_SIZE);
            img->compression = (int)rd16(e + ENTRY_SIZE + 2);
            img->packed = rd32(e + ENTRY_SIZE + 4);
            img->offset = r->img_data + rd32(e + ENTRY_SIZE + 8);
            img->unpacked = img->compression ? rd32(e + ENTRY_SIZE + 12) : img->packed;
            if (img->offset > r->size || img->packed > r->size - img->offset)
                return -3;
            return 0;
        }
        next = rd32(e + 0x14);
        if (next == 0)
            break;
        cur += next;
    }
    return -4;
}

int pw_rco_read_image(const pw_rco *r, const pw_rco_image *img, void *dst, unsigned int cap,
                      unsigned char *state, pw_rco_inflate_fn inflate)
{
    const unsigned char *src = r->file + img->offset;

    if (img->unpacked > cap)
        return -1;
    if (img->compression == PW_RCO_NONE) {
        unsigned char *d = dst;

        for (unsigned int i = 0; i < img->packed; i++)
            d[i] = src[i];
        return (int)img->packed;
    }
    if (img->compression == PW_RCO_RLZ)
        return pw_rlz_decode(src, img->packed, dst, cap, state);
    if (img->compression == PW_RCO_ZLIB && inflate)
        return inflate(src, img->packed, dst, cap);
    return -2;
}
