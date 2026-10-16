// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/text.h"
#include "ui/font8x8.h"

#define SHADOW_ALPHA_NEAR 96 /* of 256, at (+1, +1) */
#define SHADOW_ALPHA_FAR  64 /* at (+2, +2) */

unsigned int pw_utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int c = p[0], need, cp;

    if (c < 0x80) {
        *s += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        need = 1;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        need = 2;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        need = 3;
        cp = c & 0x07;
    } else {
        *s += 1;
        return 0xFFFD;
    }
    for (unsigned int i = 1; i <= need; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *s += i;
            return 0xFFFD;
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s += need + 1;
    return cp;
}

unsigned int pw_font_scratch_size(const pw_pgf *pgf)
{
    unsigned int native = pw_pgf_glyph_room(pgf);

    /* decode (2x for overlays) + one horizontally resampled copy in 16-bit */
    return 2 * native + 2 * native + 16;
}

int pw_font_init(pw_font *f, const pw_pgf *pgf, int scale256, void *pool, unsigned int pool_size,
                 void *scratch, unsigned int scratch_size)
{
    if (scale256 <= 0 || scale256 > 256 || scratch_size < pw_font_scratch_size(pgf))
        return -1;
    f->pgf = pgf;
    f->scale = scale256;
    f->pool = pool;
    f->pool_size = pool_size;
    f->pool_used = 0;
    f->scratch = scratch;
    f->scratch_size = scratch_size;
    f->ascent = (pgf->ascender * scale256 / 256 + 63) / 64; /* 26.6 metrics fit in 32 bits */
    f->descent =
        ((pgf->descender < 0 ? -pgf->descender : pgf->descender) * scale256 / 256 + 63) / 64;
    for (int i = 0; i < PW_FONT_SLOTS; i++)
        f->slots[i].code = 0;
    return 0;
}

void pw_font_init_fallback(pw_font *f, int scale)
{
    f->pgf = 0;
    f->scale = scale < 1 ? 1 : scale;
    f->pool = 0;
    f->pool_size = f->pool_used = 0;
    f->scratch = 0;
    f->scratch_size = 0;
    f->ascent = 7 * f->scale;
    f->descent = 1 * f->scale;
    for (int i = 0; i < PW_FONT_SLOTS; i++)
        f->slots[i].code = 0;
}

/* Area-average src (sw x sh) down by scale/256 into dst (dw x dh). tmp holds dw * sh shorts. */
static void resample(const unsigned char *src, int sw, int sh, unsigned char *dst, int dw, int dh,
                     int scale, unsigned short *tmp)
{
    int span = (256 * 256 + scale / 2) / scale; /* source pixels per output pixel, 1/256 */

    for (int y = 0; y < sh; y++)
        for (int x = 0; x < dw; x++) {
            int a = x * span, b = a + span;
            unsigned int acc = 0;

            for (int sx = a >> 8; sx < sw && (sx << 8) < b; sx++) {
                int lo = a > (sx << 8) ? a : (sx << 8),
                    hi = b < ((sx + 1) << 8) ? b : ((sx + 1) << 8);

                acc += (unsigned int)src[y * sw + sx] * (unsigned int)(hi - lo);
            }
            tmp[y * dw + x] = (unsigned short)(acc / (unsigned int)span);
        }
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            int a = y * span, b = a + span;
            unsigned int acc = 0;

            for (int sy = a >> 8; sy < sh && (sy << 8) < b; sy++) {
                int lo = a > (sy << 8) ? a : (sy << 8),
                    hi = b < ((sy + 1) << 8) ? b : ((sy + 1) << 8);

                acc += (unsigned int)tmp[sy * dw + x] * (unsigned int)(hi - lo);
            }
            acc /= (unsigned int)span;
            dst[y * dw + x] = (unsigned char)(acc > 255 ? 255 : acc);
        }
}

static pw_glyph_slot *lookup(pw_font *f, unsigned int code)
{
    unsigned int i = (code * 2654435761u) >> 25; /* 7 bits: PW_FONT_SLOTS == 128 */

    for (unsigned int probe = 0; probe < PW_FONT_SLOTS;
         probe++, i = (i + 1) & (PW_FONT_SLOTS - 1)) {
        pw_glyph_slot *s = &f->slots[i];

        if (s->code == code || s->code == 0)
            return s;
    }
    return 0;
}

static const pw_glyph_slot *glyph(pw_font *f, unsigned int code)
{
    pw_glyph_slot *s = lookup(f, code);
    pw_pgf_glyph g;
    unsigned int native, need;
    int dw, dh;

    if (s == 0 || code > 0xFFFF)
        return 0;
    if (s->code == code)
        return s;

    s->code = (unsigned short)code;
    s->missing = 1;
    s->w = s->h = 0;
    s->adv = 0;
    native = pw_pgf_glyph_room(f->pgf);
    if (pw_pgf_render(f->pgf, code, &g, f->scratch, 2 * native) < 0)
        return s;
    dw = (g.w * f->scale + 255) / 256;
    dh = (g.h * f->scale + 255) / 256;
    need = (unsigned int)(dw * dh);
    s->adv = (unsigned short)((g.adv * f->scale / 256) & 0xFFFF);
    s->left = (signed char)((g.left * f->scale + (g.left >= 0 ? 128 : -128)) / 256);
    s->top = (signed char)((g.top * f->scale + (g.top >= 0 ? 128 : -128)) / 256);
    if (f->pool_used + need > f->pool_size || f->pool_used + need > 0xFFFF)
        return s; /* no room: keep the advance, draw nothing */
    s->off = (unsigned short)f->pool_used;
    s->w = (unsigned char)dw;
    s->h = (unsigned char)dh;
    if (f->scale == 256) {
        for (unsigned int i = 0; i < need; i++)
            f->pool[s->off + i] = f->scratch[i];
    } else {
        resample(f->scratch, g.w, g.h, f->pool + s->off, dw, dh, f->scale,
                 (unsigned short *)(void *)(f->scratch + 2 * native + ((2 * native) & 1)));
    }
    f->pool_used += need;
    s->missing = 0;
    return s;
}

static int advance_fallback(const pw_font *f)
{
    return 8 * f->scale;
}

int pw_text_covers(const pw_font *f, const char *s)
{
    while (*s) {
        unsigned int code = pw_utf8_next(&s);

        if (code == ' ')
            continue;
        if (f->pgf == 0 ? code < 32 || code > 126 : !pw_pgf_has(f->pgf, code))
            return 0;
    }
    return 1;
}

int pw_text_width(pw_font *f, const char *s)
{
    int w64 = 0;

    while (*s) {
        unsigned int code = pw_utf8_next(&s);

        if (f->pgf == 0) {
            w64 += advance_fallback(f) * 64;
            continue;
        }
        const pw_glyph_slot *g = glyph(f, code);

        if (g && g->missing && code != '?')
            g = glyph(f, '?');
        if (g)
            w64 += g->adv;
    }
    return (w64 + 32) / 64;
}

static void draw_glyph(pw_canvas *c, pw_font *f, const pw_glyph_slot *g, int x, int y,
                       unsigned int color, int alpha)
{
    pw_draw_mask(c, x + g->left, y - g->top, f->pool + g->off, g->w, g->h, g->w, color, alpha);
}

static int draw_run(pw_canvas *c, pw_font *f, int x, int y, const char *s, unsigned int color,
                    int alpha)
{
    int pen64 = x * 64, x0 = x;

    while (*s) {
        unsigned int code = pw_utf8_next(&s);

        if (f->pgf == 0) {
            char one[2] = {(char)(code < 32 || code > 126 ? '?' : code), 0};

            pw_draw_text_alpha(c, pen64 / 64, y - 7 * f->scale, one, color, f->scale, alpha);
            pen64 += advance_fallback(f) * 64;
            continue;
        }
        const pw_glyph_slot *g = glyph(f, code);

        if (g && g->missing && code != '?')
            g = glyph(f, '?');
        if (g == 0)
            continue;
        if (!g->missing && g->w > 0)
            draw_glyph(c, f, g, (pen64 + 32) / 64, y, color, alpha);
        pen64 += g->adv;
    }
    return (pen64 + 32) / 64 - x0;
}

int pw_text_draw(pw_canvas *c, pw_font *f, int x, int y, const char *s, unsigned int color,
                 int alpha, int shadow)
{
    if (shadow) {
        draw_run(c, f, x + 2, y + 2, s, PW_RGB(0, 0, 0), alpha * SHADOW_ALPHA_FAR / 256);
        draw_run(c, f, x + 1, y + 1, s, PW_RGB(0, 0, 0), alpha * SHADOW_ALPHA_NEAR / 256);
    }
    return draw_run(c, f, x, y, s, color, alpha);
}

int pw_text_draw_fit(pw_canvas *c, pw_font *f, int x, int y, const char *s, int max_w,
                     unsigned int color, int alpha, int shadow)
{
    char cut[160];
    const char *p = s;
    int dots = pw_text_width(f, "..."), n = 0;

    if (pw_text_width(f, s) <= max_w)
        return pw_text_draw(c, f, x, y, s, color, alpha, shadow);
    while (*p) { /* the longest prefix (whole characters) that leaves room for the dots */
        const char *q = p;
        int len;

        pw_utf8_next(&q);
        len = (int)(q - s);
        if (len + 4 > (int)sizeof(cut))
            break;
        for (int i = 0; i < len; i++)
            cut[i] = s[i];
        cut[len] = 0;
        if (pw_text_width(f, cut) + dots > max_w)
            break;
        n = len;
        p = q;
    }
    cut[n] = '.';
    cut[n + 1] = '.';
    cut[n + 2] = '.';
    cut[n + 3] = 0;
    return pw_text_draw(c, f, x, y, cut, color, alpha, shadow);
}
