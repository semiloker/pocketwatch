// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "photo_edit.h"
#include "ui/draw.h"

static int clamp8(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* ---- the model ---- */

void hm_edit_init(hm_edit *e)
{
    memset(e, 0, sizeof(*e));
    e->strength = 100;
}

int hm_edit_tools(int tab)
{
    return tab == HM_EDT_ADJUST ? HM_ADJS : tab == HM_EDT_FILTERS ? HM_FILTERS : HM_CROPS;
}

int hm_edit_one_sided(int adj)
{
    return adj == HM_ADJ_VIGNETTE || adj == HM_ADJ_SHARPEN;
}

int hm_edit_pick_filter(hm_edit *e, int filter)
{
    if (filter < 0 || filter >= HM_FILTERS || filter == e->filter)
        return 0;
    e->filter = filter;
    e->strength = 100; /* a new look at its full strength */
    return 1;
}

int hm_edit_step(hm_edit *e, int tab, int tool, int dir)
{
    int was;

    dir = dir > 0 ? 1 : -1;
    if (tab == HM_EDT_ADJUST && tool >= 0 && tool < HM_ADJS) {
        was = e->adj[tool];
        e->adj[tool] =
            clampi(was + dir * HM_ADJ_STEP, hm_edit_one_sided(tool) ? 0 : -HM_ADJ_MAX, HM_ADJ_MAX);
        return e->adj[tool] != was;
    }
    if (tab == HM_EDT_FILTERS) {
        if (tool != e->filter)
            return hm_edit_pick_filter(e, tool);
        if (e->filter == HM_FL_NONE)
            return 0;
        was = e->strength;
        e->strength = clampi(was + dir * HM_STR_STEP, 0, 100);
        return e->strength != was;
    }
    switch (tool) {
    case HM_CROP_ROTATE:
        e->rotate = (e->rotate + (dir > 0 ? 1 : 3)) % 4;
        return 1;
    case HM_CROP_MIRROR:
        e->mirror = !e->mirror;
        return 1;
    case HM_CROP_ASPECT:
        e->aspect = (e->aspect + (dir > 0 ? 1 : HM_ARS - 1)) % HM_ARS;
        return 1;
    default:
        return 0;
    }
}

int hm_edit_is_set(const hm_edit *e, int tab, int tool)
{
    if (tab == HM_EDT_ADJUST)
        return tool >= 0 && tool < HM_ADJS && e->adj[tool] != 0;
    if (tab == HM_EDT_FILTERS)
        return tool == e->filter && tool != HM_FL_NONE && e->strength > 0;
    return tool == HM_CROP_ROTATE   ? e->rotate != 0
           : tool == HM_CROP_MIRROR ? e->mirror != 0
           : tool == HM_CROP_ASPECT ? e->aspect != 0
                                    : 0;
}

int hm_edit_reset(hm_edit *e, int tab, int tool)
{
    int was = hm_edit_is_set(e, tab, tool);

    if (tab == HM_EDT_ADJUST && tool >= 0 && tool < HM_ADJS)
        e->adj[tool] = 0;
    else if (tab == HM_EDT_FILTERS) {
        e->filter = HM_FL_NONE;
        e->strength = 100;
    } else if (tool == HM_CROP_ROTATE)
        e->rotate = 0;
    else if (tool == HM_CROP_MIRROR)
        e->mirror = 0;
    else if (tool == HM_CROP_ASPECT)
        e->aspect = 0;
    return was;
}

int hm_edit_changed(const hm_edit *e)
{
    for (int i = 0; i < HM_ADJS; i++)
        if (e->adj[i])
            return 1;
    return (e->filter != HM_FL_NONE && e->strength > 0) || e->rotate || e->mirror || e->aspect;
}

/* ---- the colours ---- */

/* A look: what it adds to the Adjust tab at full strength (sliders' units), how grey it makes
   the picture (0-256), a sepia tone (0-256) and how far it lifts the blacks (0-255). */
typedef struct {
    signed char exposure, contrast, highlights, shadows, sat, vib, warmth, tint;
    short grey, sepia, lift;
} look;

static const look k_looks[HM_FILTERS] = {
    [HM_FL_NONE] = {0},
    [HM_FL_MONO] = {.contrast = 8, .grey = 256},
    [HM_FL_SEPIA] = {.contrast = 4, .grey = 256, .sepia = 256, .lift = 10},
    [HM_FL_VIVID] = {.contrast = 12, .sat = 25, .vib = 30, .shadows = 10},
    [HM_FL_FADE] = {.contrast = -25, .sat = -30, .highlights = -15, .lift = 28},
    [HM_FL_COOL] = {.warmth = -30, .tint = -5, .sat = -10, .highlights = 5},
    [HM_FL_NOIR] = {.contrast = 35, .exposure = -10, .shadows = -20, .grey = 256},
    [HM_FL_RETRO] = {.warmth = 25, .sat = -35, .contrast = -10, .lift = 22, .highlights = -10},
};

/* What is the same for every pixel, as tables: each channel's tone curve (exposure, contrast,
   highlights, shadows, the lifted blacks, warmth, tint, sepia), the saturation and the vibrance
   by how colourful a pixel already is. */
typedef struct {
    unsigned char r[256], g[256], b[256];
    short vib[256]; /* added to sat, by a pixel's max - min */
    int sat;        /* 256: as it is, 0: grey */
    int plain;      /* nothing per pixel but the tables */
} tables;

/* 256 x 2^(n / d), n >= 0, to an eighth of a stop. */
static int pow2_q8(int n, int d)
{
    static const short k8[8] = {256, 279, 304, 332, 362, 395, 431, 470};
    int e8 = n * 8 / d;

    return k8[e8 & 7] << (e8 >> 3);
}

/* The scales (ADR-060, #10): a press of 10 is a step one sees - about a sixth of a stop of
   exposure, 6 % more contrast, a twelfth of the way for highlights and shadows - and 100 a strong
   one; the curve's ends stay where they are (no clipped whites from exposure). */
static void make_tables(const hm_edit *e, tables *t)
{
    const look *k = &k_looks[e->filter >= 0 && e->filter < HM_FILTERS ? e->filter : 0];
    int s = e->filter == HM_FL_NONE ? 0 : clampi(e->strength, 0, 100);
    int ex = clampi(e->adj[HM_ADJ_EXPOSURE] + k->exposure * s / 100, -100, 100),
        co = clampi(e->adj[HM_ADJ_CONTRAST] + k->contrast * s / 100, -100, 100),
        hi = clampi(e->adj[HM_ADJ_HIGHLIGHTS] + k->highlights * s / 100, -100, 100),
        sh = clampi(e->adj[HM_ADJ_SHADOWS] + k->shadows * s / 100, -100, 100),
        sa = clampi(e->adj[HM_ADJ_SATURATION] + k->sat * s / 100, -100, 100),
        vi = clampi(e->adj[HM_ADJ_VIBRANCE] + k->vib * s / 100, -100, 100),
        wa = clampi(e->adj[HM_ADJ_WARMTH] + k->warmth * s / 100, -100, 100) * 70 / 100,
        ti = clampi(e->adj[HM_ADJ_TINT] + k->tint * s / 100, -100, 100) * 60 / 100,
        grey = k->grey * s / 100, sep = k->sepia * s / 100, lift = k->lift * s / 100;
    /* exposure: up to 2.5 stops brighter (rolled off towards white), 1.7 darker */
    int gain = ex >= 0 ? pow2_q8(ex, 40) : 65536 / pow2_q8(-ex, 60),
        contrast = co >= 0 ? 256 + co * 4 : 256 + co * 2;

    t->sat = sa >= 0 ? 256 + sa * 3 : 256 + sa * 256 / 100;
    t->sat = t->sat * (256 - grey) >> 8;
    for (int m = 0; m < 256; m++) /* vibrance: most for the dull colours, none for the vivid */
        t->vib[m] = (short)((vi * 4 * (255 - m) / 255) * (256 - grey) >> 8);
    t->plain = t->sat == 256 && vi == 0;
    for (int i = 0; i < 256; i++) {
        int v = gain >= 256 ? 255 * i * gain / (255 * 256 + (gain - 256) * i) : i * gain >> 8;

        v = 128 + (v - 128) * contrast / 256;
        if (v > 128) /* the bright half, more towards white */
            v += hi * 128 * (v - 128) / (100 * 127);
        else /* the dark half, more towards black */
            v += sh * 128 * (128 - v) / (100 * 128);
        v = clamp8(v);
        v = lift + v * (255 - lift) / 255;
        t->r[i] = (unsigned char)clamp8(v + wa + ti / 2 + (38 * sep >> 8));
        t->g[i] = (unsigned char)clamp8(v - ti + (14 * sep >> 8));
        t->b[i] = (unsigned char)clamp8(v - wa + ti / 2 - (22 * sep >> 8));
    }
}

/* Sharpening (an unsharp mask of the four neighbours), amount 0..100: rows kept as they were. */
static int sharpen(unsigned int *px, int w, int h, int stride, int amount)
{
    unsigned int *prev, *cur;
    int k = amount * 384 / 100; /* /256 of the difference added: 1.5x at 100 */

    if (k <= 0 || w < 3 || h < 3)
        return 0;
    prev = malloc((size_t)w * 8);
    if (!prev)
        return -1;
    cur = prev + w;
    memcpy(prev, px, (size_t)w * 4);
    for (int y = 1; y < h - 1; y++) {
        unsigned int *row = px + y * stride;
        const unsigned int *next = row + stride;

        memcpy(cur, row, (size_t)w * 4);
        for (int x = 1; x < w - 1; x++) {
            unsigned int p = cur[x], out = p & 0xFF000000u;

            for (int sh = 0; sh < 24; sh += 8) {
                int c = (int)(p >> sh & 0xFF),
                    n = (int)(prev[x] >> sh & 0xFF) + (int)(next[x] >> sh & 0xFF) +
                        (int)(cur[x - 1] >> sh & 0xFF) + (int)(cur[x + 1] >> sh & 0xFF);

                out |= (unsigned int)clamp8(c + ((c * 4 - n) * k >> 8)) << sh;
            }
            row[x] = out;
        }
        memcpy(prev, cur, (size_t)w * 4);
    }
    free(prev);
    return 0;
}

int hm_edit_colours(const hm_edit *e, unsigned int *px, int w, int h, int stride)
{
    tables t;
    /* the vignette: darker towards the corners, by the squared distance (1/64 of the corner's), as
       a table */
    int vig = clampi(e->adj[HM_ADJ_VIGNETTE], 0, 100) * 256 / 100, cx = w / 2, cy = h / 2;
    long long norm = (long long)cx * cx + (long long)cy * cy;
    unsigned int inv = norm > 0 ? (unsigned int)((64ull << 16) / (unsigned long long)norm) : 0;
    int shade[65];

    make_tables(e, &t);
    for (int k = 0; k <= 64; k++)
        shade[k] = 256 - vig * (k > 8 ? (k - 8) * (k - 8) : 0) / (56 * 56); /* 100: black corners */
    for (int y = 0; y < h; y++) {
        unsigned int *row = px + y * stride;
        long long dy = (long long)(y - cy) * (y - cy);

        for (int x = 0; x < w; x++) {
            unsigned int p = row[x];
            int r = (int)(p & 0xFF), g = (int)(p >> 8 & 0xFF), b = (int)(p >> 16 & 0xFF);

            if (!t.plain) {
                int l = (r * 77 + g * 150 + b * 29) >> 8,
                    mx = r > g ? (r > b ? r : b) : (g > b ? g : b),
                    mn = r < g ? (r < b ? r : b) : (g < b ? g : b), s = t.sat + t.vib[mx - mn];

                if (s < 0)
                    s = 0;
                r = clamp8(l + ((r - l) * s >> 8));
                g = clamp8(l + ((g - l) * s >> 8));
                b = clamp8(l + ((b - l) * s >> 8));
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
    return sharpen(px, w, h, stride, clampi(e->adj[HM_ADJ_SHARPEN], 0, 100));
}

void hm_edit_histogram(const unsigned int *px, int w, int h, int stride,
                       unsigned char out[HM_HIST_BINS])
{
    unsigned int bins[HM_HIST_BINS], most = 0;

    memset(bins, 0, sizeof(bins));
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned int p = px[y * stride + x];
            int l =
                ((int)(p & 0xFF) * 77 + (int)(p >> 8 & 0xFF) * 150 + (int)(p >> 16 & 0xFF) * 29) >>
                8;

            bins[l >> 2]++;
        }
    for (int i = 0; i < HM_HIST_BINS; i++)
        most = bins[i] > most ? bins[i] : most;
    for (int i = 0; i < HM_HIST_BINS; i++)
        out[i] = most ? (unsigned char)(bins[i] * 255 / most) : 0;
}

/* ---- turns, mirror, crop ---- */

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

void hm_edit_crop(int w, int h, int aspect, int *x, int *y, int *cw, int *ch)
{
    static const unsigned char k[HM_ARS][2] = {{0, 0}, {1, 1}, {4, 3}, {3, 2}, {16, 9}};
    int aw, ah;

    *x = *y = 0;
    *cw = w;
    *ch = h;
    if (aspect <= HM_AR_ORIGINAL || aspect >= HM_ARS || w <= 0 || h <= 0)
        return;
    aw = h > w ? k[aspect][1] : k[aspect][0]; /* in the picture's orientation */
    ah = h > w ? k[aspect][0] : k[aspect][1];
    if ((long long)w * ah > (long long)h * aw) { /* wider than the ratio: the sides go */
        *cw = h * aw / ah;
        *x = (w - *cw) / 2;
    } else {
        *ch = w * ah / aw;
        *y = (h - *ch) / 2;
    }
}

void hm_edit_crop_in_place(unsigned int *px, int w, int x, int y, int cw, int ch)
{
    for (int r = 0; r < ch; r++) /* (each row moves to an earlier place: never over one unread) */
        memmove(px + r * cw, px + (y + r) * w + x, (size_t)cw * 4);
}

/* ---- the editor's screen ---- */

/* Bilinear: 8-bit weights between the four pixels around each sample. */
static unsigned int lerp2(unsigned int a, unsigned int b, int f)
{
    unsigned int rb = (a & 0x00FF00FFu) * (unsigned int)(256 - f) +
                      (b & 0x00FF00FFu) * (unsigned int)f,
                 ag = (a >> 8 & 0x00FF00FFu) * (unsigned int)(256 - f) +
                      (b >> 8 & 0x00FF00FFu) * (unsigned int)f;

    return (rb >> 8 & 0x00FF00FFu) | (ag & 0xFF00FF00u);
}

unsigned int *hm_edit_fit(const unsigned int *src, int w, int h, int fw, int fh, int *ow, int *oh)
{
    unsigned int *out, sx, sy;
    int dw = fw, dh = h * fw / w;

    if (w <= 0 || h <= 0 || fw <= 0 || fh <= 0)
        return 0;
    if (dh > fh) {
        dh = fh;
        dw = w * fh / h;
    }
    if (dw < 1)
        dw = 1;
    if (dh < 1)
        dh = 1;
    out = malloc((size_t)dw * (size_t)dh * 4);
    if (!out)
        return 0;
    sx = ((unsigned int)(w - 1) << 16) / (unsigned int)(dw > 1 ? dw - 1 : 1); /* 16.16 */
    sy = ((unsigned int)(h - 1) << 16) / (unsigned int)(dh > 1 ? dh - 1 : 1);
    for (int j = 0; j < dh; j++) {
        unsigned int fy = (unsigned int)j * sy;
        int y0 = (int)(fy >> 16), y1 = y0 + 1 < h ? y0 + 1 : y0, wy = (int)(fy >> 8 & 0xFF);
        const unsigned int *r0 = src + y0 * w, *r1 = src + y1 * w;

        for (int i = 0; i < dw; i++) {
            unsigned int fx = (unsigned int)i * sx;
            int x0 = (int)(fx >> 16), x1 = x0 + 1 < w ? x0 + 1 : x0, wx = (int)(fx >> 8 & 0xFF);

            out[j * dw + i] =
                lerp2(lerp2(r0[x0], r0[x1], wx), lerp2(r1[x0], r1[x1], wx), wy) | 0xFF000000u;
        }
    }
    *ow = dw;
    *oh = dh;
    return out;
}

unsigned int *hm_edit_frame(const hm_edit *e, const unsigned int *src, int w, int h, int *fw,
                            int *fh)
{
    unsigned int *t, *fit;
    int ow, oh, x, y, cw, ch;

    t = hm_edit_turn(src, w, h, e->rotate, e->mirror, &ow, &oh);
    if (!t)
        return 0;
    hm_edit_crop(ow, oh, e->aspect, &x, &y, &cw, &ch);
    if (cw != ow || ch != oh)
        hm_edit_crop_in_place(t, ow, x, y, cw, ch);
    fit = hm_edit_fit(t, cw, ch, HM_ED_CW - 16, HM_ED_CH - 8, fw, fh);
    free(t);
    return fit;
}

int hm_edit_compose(const hm_edit *e, const unsigned int *frame, int fw, int fh,
                    const unsigned int *thumb, int tw, int th, int tab, int original,
                    unsigned int *bg, unsigned char hist[HM_HIST_BINS])
{
    pw_canvas c = {bg, HM_ED_SW, HM_ED_SH, 512};
    unsigned int *at;
    int r = 0;

    pw_draw_fill(&c, 0, 0, HM_ED_SW, HM_ED_SH, HM_ED_DARK);
    memset(hist, 0, HM_HIST_BINS);
    if (!frame || fw <= 0 || fh <= 0 || fw > HM_ED_CW || fh > HM_ED_CH)
        return -1;
    /* the picture into the canvas, then its colours there (rows of 512): no copy of it per step */
    at = bg + (HM_ED_CY + (HM_ED_CH - fh) / 2) * 512 + HM_ED_CX + (HM_ED_CW - fw) / 2;
    for (int y = 0; y < fh; y++)
        memcpy(at + y * 512, frame + y * fw, (size_t)fw * 4);
    if (!original && hm_edit_colours(e, at, fw, fh, 512) < 0)
        r = -1;
    hm_edit_histogram(at, fw, fh, 512, hist);
    if (tab == HM_EDT_FILTERS && thumb && tw > 0 && th > 0) { /* each look on the thumbnail */
        for (int i = 0; i < HM_FILTERS; i++) {
            unsigned int *dst = bg + (HM_ED_STRIP - th / 2) * 512 +
                                hm_edit_strip_x(HM_FILTERS, HM_ED_TPITCH, i) - tw / 2;
            hm_edit k;

            hm_edit_init(&k);
            k.filter = i;
            for (int y = 0; y < th; y++)
                memcpy(dst + y * 512, thumb + y * tw, (size_t)tw * 4);
            hm_edit_colours(&k, dst, tw, th, 512);
        }
    }
    return r;
}
