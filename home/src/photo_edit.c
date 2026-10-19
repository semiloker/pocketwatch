// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "photo_edit.h"

static int clamp8(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

int hm_edit_step(hm_edit *e, int tool, int dir)
{
    int *v = 0, lo = -HM_ED_RANGE, hi = HM_ED_RANGE, was;

    switch (tool) {
    case HM_ED_ROTATE:
        e->rotate = (e->rotate + (dir > 0 ? 1 : 3)) % 4;
        return 1;
    case HM_ED_MIRROR:
        e->mirror = !e->mirror;
        return 1;
    case HM_ED_FILTER:
        e->filter = (e->filter + (dir > 0 ? 1 : HM_FILTERS - 1)) % HM_FILTERS;
        return 1;
    case HM_ED_LIGHT:
        v = &e->light;
        break;
    case HM_ED_CONTRAST:
        v = &e->contrast;
        break;
    case HM_ED_SAT:
        v = &e->sat;
        break;
    case HM_ED_WARMTH:
        v = &e->warmth;
        break;
    case HM_ED_VIGNETTE:
        v = &e->vignette;
        lo = 0;
        break;
    default:
        return 0;
    }
    was = *v;
    *v = *v + dir < lo ? lo : *v + dir > hi ? hi : *v + dir;
    return *v != was;
}

int hm_edit_changed(const hm_edit *e)
{
    return e->rotate || e->mirror || e->filter || e->light || e->contrast || e->sat || e->warmth ||
           e->vignette;
}

/* Per-channel tables for what is the same for every pixel: brightness, contrast, warmth and the
   filter's tone curve; saturation and the monochrome filters mix with the pixel's luma. */
typedef struct {
    unsigned char r[256], g[256], b[256];
    int sat;  /* 256: as it is, 0: grey, more: richer */
    int tint; /* 0 none, 1 sepia, 2 cool */
    int grey; /* the filter is monochrome */
} tables;

static void make_tables(const hm_edit *e, tables *t)
{
    int light = e->light * 14, contrast = 256 + e->contrast * 30, warm = e->warmth * 6;

    t->sat = 256 + e->sat * 40;
    t->tint = 0;
    t->grey = 0;
    switch (e->filter) {
    case HM_FL_MONO:
        t->grey = 1;
        break;
    case HM_FL_SEPIA:
        t->grey = 1;
        t->tint = 1;
        break;
    case HM_FL_VIVID:
        t->sat += 110;
        contrast += 40;
        break;
    case HM_FL_FADE:
        t->sat -= 70;
        contrast -= 50;
        light += 18;
        break;
    case HM_FL_COOL:
        warm -= 22;
        t->sat -= 20;
        break;
    case HM_FL_NOIR:
        t->grey = 1;
        contrast += 110;
        light -= 10;
        break;
    case HM_FL_RETRO:
        warm += 18;
        t->sat -= 60;
        contrast -= 20;
        break;
    default:
        break;
    }
    if (t->sat < 0)
        t->sat = 0;
    for (int i = 0; i < 256; i++) {
        int v = (i - 128) * contrast / 256 + 128 + light;

        if (e->filter == HM_FL_FADE || e->filter == HM_FL_RETRO) /* lifted blacks */
            v = 24 + v * 216 / 255;
        t->r[i] = (unsigned char)clamp8(v + warm);
        t->g[i] = (unsigned char)clamp8(v + warm / 4);
        t->b[i] = (unsigned char)clamp8(v - warm);
    }
}

void hm_edit_colours(const hm_edit *e, unsigned int *px, int w, int h, int stride)
{
    tables t;
    /* the vignette: darker towards the corners, by the squared distance (1/1024 of the half
       diagonal's square), as a table over its range */
    int vig = e->vignette * 30, cx = w / 2, cy = h / 2;
    long long norm = (long long)cx * cx + (long long)cy * cy;
    unsigned int inv = norm > 0 ? (unsigned int)((64ull << 16) / (unsigned long long)norm) : 0;
    int shade[65];

    make_tables(e, &t);
    for (int k = 0; k <= 64; k++) /* k/64 of the corner's distance squared */
        shade[k] = 256 - vig * (k > 16 ? (k - 16) * (k - 16) : 0) / (48 * 48);
    for (int y = 0; y < h; y++) {
        unsigned int *row = px + y * stride;
        long long dy = (long long)(y - cy) * (y - cy);

        for (int x = 0; x < w; x++) {
            unsigned int p = row[x];
            int r = (int)(p & 0xFF), g = (int)(p >> 8 & 0xFF), b = (int)(p >> 16 & 0xFF),
                l = (r * 77 + g * 150 + b * 29) >> 8;

            if (t.grey) {
                r = g = b = l;
                if (t.tint == 1) { /* sepia: a warm brown */
                    r = clamp8(l + 38);
                    g = clamp8(l + 14);
                    b = clamp8(l - 22);
                }
            } else if (t.sat != 256) {
                r = clamp8(l + ((r - l) * t.sat >> 8));
                g = clamp8(l + ((g - l) * t.sat >> 8));
                b = clamp8(l + ((b - l) * t.sat >> 8));
            }
            r = t.r[r];
            g = t.g[g];
            b = t.b[b];
            if (vig) {
                unsigned long long d = (unsigned long long)((long long)(x - cx) * (x - cx) + dy);
                int k = (int)((d * inv) >> 16), s = shade[k > 64 ? 64 : k]; /* (no division) */

                r = r * s >> 8;
                g = g * s >> 8;
                b = b * s >> 8;
            }
            row[x] =
                (p & 0xFF000000u) | (unsigned int)r | (unsigned int)g << 8 | (unsigned int)b << 16;
        }
    }
}

void hm_edit_turned_size(int w, int h, int rotate, int *ow, int *oh)
{
    *ow = rotate & 1 ? h : w;
    *oh = rotate & 1 ? w : h;
}

unsigned int *hm_edit_turn(const unsigned int *px, int w, int h, int rotate, int mirror, int *ow,
                           int *oh)
{
    unsigned int *out;

    hm_edit_turned_size(w, h, rotate & 3, ow, oh);
    out = malloc((size_t)w * (size_t)h * 4);
    if (!out)
        return 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sx = mirror ? w - 1 - x : x, dx, dy;

            switch (rotate & 3) { /* (x, y) of the mirrored picture to where it turns */
            case 1:
                dx = h - 1 - y;
                dy = x;
                break;
            case 2:
                dx = w - 1 - x;
                dy = h - 1 - y;
                break;
            case 3:
                dx = y;
                dy = w - 1 - x;
                break;
            default:
                dx = x;
                dy = y;
                break;
            }
            out[dy * *ow + dx] = px[y * w + sx];
        }
    return out;
}
