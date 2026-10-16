// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/draw.h"
#include "ui/font8x8.h"

static int clip(const pw_canvas *c, int *x, int *y, int *w, int *h)
{
    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    if (*x + *w > c->w)
        *w = c->w - *x;
    if (*y + *h > c->h)
        *h = c->h - *y;
    return *w > 0 && *h > 0;
}

void pw_draw_fill(pw_canvas *c, int x, int y, int w, int h, unsigned int color)
{
    if (!clip(c, &x, &y, &w, &h))
        return;
    for (int j = 0; j < h; j++) {
        unsigned int *row = c->px + (y + j) * c->stride + x;

        for (int i = 0; i < w; i++)
            row[i] = color;
    }
}

static unsigned int mix(unsigned int dst, unsigned int src, int alpha)
{
    unsigned int out = 0xFF000000u;

    for (int shift = 0; shift < 24; shift += 8) {
        int d = (int)((dst >> shift) & 0xFF), s = (int)((src >> shift) & 0xFF);

        out |= (unsigned int)(d + (((s - d) * alpha) >> 8)) << shift;
    }
    return out;
}

void pw_draw_blend(pw_canvas *c, int x, int y, int w, int h, unsigned int color, int alpha)
{
    if (alpha <= 0 || !clip(c, &x, &y, &w, &h))
        return;
    if (alpha > 256)
        alpha = 256;
    for (int j = 0; j < h; j++) {
        unsigned int *row = c->px + (y + j) * c->stride + x;

        for (int i = 0; i < w; i++)
            row[i] = mix(row[i], color, alpha);
    }
}

int pw_draw_text_width(const char *s, int scale)
{
    int n = 0;

    while (s[n])
        n++;
    return n * 8 * scale;
}

int pw_draw_text(pw_canvas *c, int x, int y, const char *s, unsigned int color, int scale)
{
    int x0 = x;

    for (; *s; s++, x += 8 * scale) {
        int ch = (unsigned char)*s;
        const unsigned char *glyph = pw_font8x8[(ch < 32 || ch > 126 ? ' ' : ch) - 32];

        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if (glyph[row] & (0x80 >> col))
                    pw_draw_fill(c, x + col * scale, y + row * scale, scale, scale, color);
    }
    return x - x0;
}

/* Expand an n-bit channel to 8 bits. */
static unsigned int widen(unsigned int v, int bits)
{
    return bits == 4 ? v * 17 : bits == 5 ? (v << 3) | (v >> 2) : (v << 2) | (v >> 4);
}

static unsigned int to8888(unsigned int p, int fmt)
{
    unsigned int r, g, b;

    switch (fmt) {
    case PW_FMT_565:
        r = widen(p & 0x1F, 5), g = widen((p >> 5) & 0x3F, 6), b = widen((p >> 11) & 0x1F, 5);
        break;
    case PW_FMT_5551:
        r = widen(p & 0x1F, 5), g = widen((p >> 5) & 0x1F, 5), b = widen((p >> 10) & 0x1F, 5);
        break;
    case PW_FMT_4444:
        r = widen(p & 0xF, 4), g = widen((p >> 4) & 0xF, 4), b = widen((p >> 8) & 0xF, 4);
        break;
    default:
        r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
        break;
    }
    return PW_RGB(r, g, b);
}

void pw_draw_from_frame(pw_canvas *c, const void *src, int src_stride, int fmt, int bright)
{
    const unsigned short *s16 = src;
    const unsigned int *s32 = src;

    for (int y = 0; y < c->h; y++) {
        unsigned int *row = c->px + y * c->stride;

        for (int x = 0; x < c->w; x++) {
            unsigned int p = fmt == PW_FMT_8888 ? s32[y * src_stride + x] : s16[y * src_stride + x];
            unsigned int q = to8888(p, fmt);

            if (bright < 256)
                q = 0xFF000000u | ((((q & 0xFF00FF) * (unsigned int)bright) >> 8) & 0xFF00FF) |
                    ((((q & 0x00FF00) * (unsigned int)bright) >> 8) & 0x00FF00);
            row[x] = q;
        }
    }
}
