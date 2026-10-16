// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/image.h"

#define GIM_MAGIC_LEN 11 /* "MIG.00.1PSP" */
#define CHUNK_ROOT    0x02
#define CHUNK_PICTURE 0x03
#define CHUNK_IMAGE   0x04
#define CHUNK_PALETTE 0x05

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

typedef struct {
    unsigned int image, palette; /* chunk offsets, 0 = not found */
} chunks;

static void walk(const unsigned char *g, unsigned int len, unsigned int pos, unsigned int end,
                 chunks *c, int depth)
{
    while (depth < 4 && pos + 0x10 <= end && end <= len) {
        unsigned int id = rd16(g + pos), size = rd32(g + pos + 4), next = rd32(g + pos + 8),
                     data = rd32(g + pos + 12);

        if (id == CHUNK_ROOT || id == CHUNK_PICTURE) {
            if (size >= data && pos + size <= end)
                walk(g, len, pos + data, pos + size, c, depth + 1);
        } else if (id == CHUNK_IMAGE && c->image == 0) {
            c->image = pos;
        } else if (id == CHUNK_PALETTE && c->palette == 0) {
            c->palette = pos;
        }
        if (next == 0 || pos + next <= pos)
            break;
        pos += next;
    }
}

static unsigned int pixel16(unsigned int v, int fmt)
{
    unsigned int r, g, b, a;

    switch (fmt) {
    case 0: /* 565 */
        r = v & 0x1F, g = (v >> 5) & 0x3F, b = (v >> 11) & 0x1F;
        return 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 2 | g >> 4) << 8) |
               (r << 3 | r >> 2);
    case 1: /* 5551 */
        r = v & 0x1F, g = (v >> 5) & 0x1F, b = (v >> 10) & 0x1F, a = (v >> 15) ? 0xFF : 0;
        return (a << 24) | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
    default: /* 4444 */
        r = v & 0xF, g = (v >> 4) & 0xF, b = (v >> 8) & 0xF, a = (v >> 12) & 0xF;
        return (a * 17 << 24) | (b * 17 << 16) | (g * 17 << 8) | r * 17;
    }
}

/* One palette entry or direct pixel of the given format at p. */
static unsigned int color(const unsigned char *p, int fmt)
{
    return fmt == 3 ? rd32(p) : pixel16(rd16(p), fmt);
}

int pw_gim_decode(const void *gim, unsigned int len, unsigned int *out, unsigned int cap,
                  pw_image *img)
{
    static const char magic[GIM_MAGIC_LEN] = {'M', 'I', 'G', '.', '0', '0',
                                              '.', '1', 'P', 'S', 'P'};
    const unsigned char *g = gim;
    chunks c = {0, 0};
    unsigned int b, fmt, order, w, h, bpp, palign, halign, start, pitch, rows, pal_count = 0;
    const unsigned char *pix, *pal = 0;
    int pal_fmt = 3;

    if (len < 0x20)
        return -1;
    for (int i = 0; i < GIM_MAGIC_LEN; i++)
        if (g[i] != (unsigned char)magic[i])
            return -1;
    walk(g, len, 0x10, len, &c, 0);
    if (c.image == 0 || c.image + 0x10 + 0x24 > len)
        return -2;
    b = c.image + 0x10;
    fmt = rd16(g + b + 4);
    order = rd16(g + b + 6);
    w = rd16(g + b + 8);
    h = rd16(g + b + 10);
    bpp = rd16(g + b + 12);
    palign = rd16(g + b + 14);
    halign = rd16(g + b + 16);
    start = rd32(g + b + 0x1C);
    if (w == 0 || h == 0 || w > 512 || h > 512 || w * h > cap || palign == 0 || halign == 0 ||
        (bpp != 4 && bpp != 8 && bpp != 16 && bpp != 32))
        return -3;
    pitch = (w * bpp / 8 + palign - 1) / palign * palign;
    rows = (h + halign - 1) / halign * halign;
    if (b + start > len || pitch * rows > len - (b + start))
        return -4;
    pix = g + b + start;
    if (fmt == 4 || fmt == 5) {
        unsigned int pb;

        if (c.palette == 0 || c.palette + 0x10 + 0x24 > len)
            return -5;
        pb = c.palette + 0x10;
        pal_fmt = (int)rd16(g + pb + 4);
        pal_count = rd16(g + pb + 8);
        if (pal_fmt > 3 || pal_count == 0 || pal_count > 256 ||
            pb + rd32(g + pb + 0x1C) + pal_count * (pal_fmt == 3 ? 4 : 2) > len)
            return -5;
        pal = g + pb + rd32(g + pb + 0x1C);
    } else if (fmt > 3) {
        return -6; /* idx16/32 and DXT are not used by the icons we read */
    }

    for (unsigned int y = 0; y < h; y++)
        for (unsigned int x = 0; x < w; x++) {
            unsigned int byte = x * bpp / 8, off;

            if (order == 1) { /* swizzled: 16-byte x 8-row blocks, in block-row order */
                unsigned int bx = byte / 16, by = y / 8;

                off = (by * (pitch / 16) + bx) * 128 + (y % 8) * 16 + byte % 16;
            } else {
                off = y * pitch + byte;
            }
            if (fmt == 5) {
                unsigned int i = pix[off];

                out[y * w + x] =
                    i < pal_count ? color(pal + i * (pal_fmt == 3 ? 4 : 2), pal_fmt) : 0;
            } else if (fmt == 4) {
                unsigned int i = (x & 1) ? pix[off] >> 4 : pix[off] & 0xF;

                out[y * w + x] =
                    i < pal_count ? color(pal + i * (pal_fmt == 3 ? 4 : 2), pal_fmt) : 0;
            } else {
                out[y * w + x] = color(pix + off, (int)fmt);
            }
        }
    img->px = out;
    img->w = (int)w;
    img->h = (int)h;
    return 0;
}

int pw_bmp_decode(const void *file, unsigned int len, int index, unsigned int *out,
                  unsigned int cap, pw_image *img)
{
    const unsigned char *f = file;
    unsigned int pos = 0, size, data, w, h, row;
    int bottom_up = 1;

    for (int i = 0;; i++) {
        if (pos + 54 > len || f[pos] != 'B' || f[pos + 1] != 'M')
            return -1;
        size = rd32(f + pos + 2);
        if (i == index)
            break;
        if (size < 54 || size > len - pos)
            return -1;
        pos += size;
    }
    data = rd32(f + pos + 10);
    w = rd32(f + pos + 18);
    h = rd32(f + pos + 22);
    if ((int)h < 0) {
        h = (unsigned int)-(int)h;
        bottom_up = 0;
    }
    if (rd16(f + pos + 28) != 24 || w == 0 || h == 0 || w > 1024 || h > 1024 || w * h > cap)
        return -2;
    row = (w * 3 + 3) & ~3u;
    if (data > len - pos || row * h > len - pos - data)
        return -3;
    for (unsigned int y = 0; y < h; y++) {
        const unsigned char *s = f + pos + data + (bottom_up ? h - 1 - y : y) * row;

        for (unsigned int x = 0; x < w; x++, s += 3)
            out[y * w + x] =
                0xFF000000u | ((unsigned int)s[0] << 16) | ((unsigned int)s[1] << 8) | s[2];
    }
    img->px = out;
    img->w = (int)w;
    img->h = (int)h;
    return 0;
}
