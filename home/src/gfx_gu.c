// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <string.h>

#include "gfx_gu.h"
#include "hm_icons.h"
#include "sprites.h"
#include "stb_sprintf.h"
#include "ui/draw.h"
#include "ui/shapes.h"
#include "ui/xmbbg.h"

#define SCR_W 480
#define SCR_H 272
#define BUF_W 512
#define FRAME (BUF_W * SCR_H * 4) /* bytes of one frame in VRAM */
/* Backgrounds in VRAM as 5650 (ADR-030): the GE read 522 KB of RAM a frame for an 8888 one, and
   took 11-13 ms a frame on the PSP (home.log, 2026-10-18). Two frames + three of these: 1.86 MB. */
#define BG16      (BUF_W * SCR_H * 2)
#define BG_SLOTS  3
#define VRAM_GE   0x04000000u /* the GE's address of VRAM */
#define VRAM_CPU  0x44000000u /* the CPU's, uncached */
#define SLICE     64          /* wide 1:1 images go in columns: the GE's texture cache */
#define STAMPS    256         /* text lines, corners and icons kept as textures */
#define STAMP_RAM (3u << 20)  /* bytes of 8-bit stamps at most */
#define KEY       112
#define WARM_US   3000 /* stamps made ahead of need, per frame, while the GE draws */
#define WAVE_STEP 8    /* px between the wave's columns of vertices */
#define WAVE_COLS (SCR_W / WAVE_STEP + 1)

static unsigned int __attribute__((aligned(64))) g_list[128 * 1024];
static unsigned int __attribute__((aligned(16))) g_clut[256]; /* index i: white, alpha i */
static unsigned int g_frame; /* counts frames: what a stamp was last used in */
static hm_gfx g_ops;
static unsigned int g_warm_us;                         /* spent making stamps ahead this frame */
static const unsigned int *volatile g_bg_of[BG_SLOTS]; /* the 8888 picture each VRAM slot holds */
static unsigned int g_made; /* stamps made this frame (needed now or ahead) */
static void *g_draw;        /* the VRAM frame drawn into next (relative) */

/* ---- vertices (texture, colour, position: the GE's order) ---- */

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} tvert;

typedef struct {
    unsigned int color;
    float x, y, z;
} cvert;

#define TFMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define CFMT (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

/* PW_RGB with alpha 0-256 as the GE's 8888 (R in the low byte, as PW_RGB). */
static unsigned int ge_color(unsigned int rgb, int alpha)
{
    unsigned int a = alpha >= 256 ? 255u : alpha <= 0 ? 0u : (unsigned int)alpha * 255u / 256u;

    return (rgb & 0x00FFFFFF) | a << 24;
}

static int pow2(int n)
{
    int p = 1;

    while (p < n && p < 512)
        p <<= 1;
    return p;
}

static int g_textured = -1; /* the GE's texture state, to send it only when it changes */

static void texturing(int on)
{
    if (on != g_textured) {
        if (on)
            sceGuEnable(GU_TEXTURE_2D);
        else
            sceGuDisable(GU_TEXTURE_2D);
        g_textured = on;
    }
}

static void bind(const void *px, int w, int h, int stride, int linear)
{
    texturing(1);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, pow2(w), pow2(h), stride, px);
    sceGuTexFilter(linear ? GU_LINEAR : GU_NEAREST, linear ? GU_LINEAR : GU_NEAREST);
}

static int g_cy0, g_cy1 = SCR_H; /* the rows drawn on (the clip operation) */

/* One textured sprite, clipped to the screen (and the rows of a clip). */
static void sprite(hm_sprite s, unsigned int color)
{
    tvert *v;

    if (!hm_sprite_clip_rect(&s, 0, g_cy0, SCR_W, g_cy1))
        return;
    v = sceGuGetMemory(2 * sizeof(tvert));
    v[0] = (tvert){s.u0, s.v0, color, s.x0, s.y0, 0};
    v[1] = (tvert){s.u1, s.v1, color, s.x1, s.y1, 0};
    sceGuDrawArray(GU_SPRITES, TFMT, 2, 0, v);
}

/* ---- stamps: what the CPU renderer draws once, kept as 8-bit alpha textures ---- */

typedef struct {
    unsigned int hash, used, bytes;
    char key[KEY];
    int w, h, stride;  /* the texture's size and row length (a multiple of 16 texels) */
    int ox, oy;        /* where the drawing's origin is in it */
    unsigned char *px; /* coverage 0-255: an index into g_clut */
} stamp;

static stamp g_stamps[STAMPS];
static unsigned int g_stamp_bytes;
static unsigned int *g_scratch; /* the CPU renderer's canvas for one stamp */
static int g_scratch_px;

typedef void (*rasterise)(pw_canvas *c, const void *arg);

/* FNV-1a */
static unsigned int hash(const char *s)
{
    unsigned int h = 2166136261u;

    while (*s)
        h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

static stamp *find_stamp(const char *key, unsigned int h)
{
    for (int i = 0; i < STAMPS; i++)
        if (g_stamps[i].px && g_stamps[i].hash == h && strcmp(g_stamps[i].key, key) == 0) {
            g_stamps[i].used = g_frame;
            return &g_stamps[i];
        }
    return 0;
}

/* Room for bytes more: a free slot, the least recently used ones dropped - never one this
   frame's list still points at. */
static stamp *free_slot(unsigned int bytes)
{
    for (;;) {
        stamp *free_one = 0, *old = 0;

        for (int i = 0; i < STAMPS; i++) {
            if (!g_stamps[i].px) {
                if (!free_one)
                    free_one = &g_stamps[i];
            } else if (g_stamps[i].used != g_frame && (!old || g_stamps[i].used < old->used)) {
                old = &g_stamps[i];
            }
        }
        if (free_one && g_stamp_bytes + bytes <= STAMP_RAM)
            return free_one;
        if (!old)
            return 0;
        g_stamp_bytes -= old->bytes;
        free(old->px);
        old->px = 0;
    }
}

/* Make the stamp for key with r (white on black, w x h, the drawing's origin at ox, oy). */
static stamp *make_stamp(const char *key, unsigned int h_key, int w, int h, int ox, int oy,
                         rasterise r, const void *arg)
{
    stamp *s;
    pw_canvas c;
    int stride, n;

    w = w < 1 ? 1 : w > 512 ? 512 : w;
    h = h < 1 ? 1 : h > 512 ? 512 : h;
    stride = (w + 15) & ~15;
    n = stride * h;
    if (n > g_scratch_px) {
        free(g_scratch);
        g_scratch = memalign(16, (size_t)n * 4);
        g_scratch_px = g_scratch ? n : 0;
        if (!g_scratch)
            return 0;
    }
    if (!(s = free_slot((unsigned int)n)) || !(s->px = memalign(16, (size_t)n)))
        return 0;
    memset(g_scratch, 0, (size_t)n * 4);
    c.px = g_scratch;
    c.w = w;
    c.h = h;
    c.stride = stride;
    r(&c, arg);
    for (int i = 0; i < n; i++) /* coverage (the red channel) becomes the alpha index */
        s->px[i] = (unsigned char)(g_scratch[i] & 0xFF);
    sceKernelDcacheWritebackRange(s->px, (unsigned int)n);
    s->hash = h_key;
    strncpy(s->key, key, KEY - 1);
    s->key[KEY - 1] = 0;
    s->w = w;
    s->h = h;
    s->stride = stride;
    s->ox = ox;
    s->oy = oy;
    s->used = g_frame;
    s->bytes = (unsigned int)n;
    g_stamp_bytes += s->bytes;
    g_made++;
    return s;
}

static stamp *get_stamp(const char *key, int w, int h, int ox, int oy, rasterise r, const void *arg)
{
    unsigned int k = hash(key);
    stamp *s = find_stamp(key, k);

    return s ? s : make_stamp(key, k, w, h, ox, oy, r, arg);
}

static void bind8(const stamp *s)
{
    texturing(1);
    sceGuTexMode(GU_PSM_T8, 0, 0, 0);
    sceGuTexImage(0, pow2(s->w), pow2(s->h), s->stride, s->px);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
}

static void draw_stamp(const stamp *s, float x, float y, unsigned int color)
{
    hm_sprite sp = {x - s->ox, y - s->oy, x - s->ox + s->w, y - s->oy + s->h,
                    0,         0,         (float)s->w,      (float)s->h};

    bind8(s);
    sprite(sp, color);
}

/* ---- the operations ---- */

static void fill(hm_gfx *g, int x, int y, int w, int h, unsigned int color, int alpha)
{
    cvert *v;
    hm_sprite s = {(float)x, (float)y, (float)(x + w), (float)(y + h), 0, 0, 0, 0};

    (void)g;
    if (alpha <= 0 || !hm_sprite_clip_rect(&s, 0, g_cy0, SCR_W, g_cy1))
        return;
    texturing(0);
    v = sceGuGetMemory(2 * sizeof(cvert));
    v[0] = (cvert){ge_color(color, alpha), s.x0, s.y0, 0};
    v[1] = (cvert){ge_color(color, alpha), s.x1, s.y1, 0};
    sceGuDrawArray(GU_SPRITES, CFMT, 2, 0, v);
}

typedef struct {
    int r, line;
} corner_arg;

static void corner(pw_canvas *c, const void *arg)
{
    const corner_arg *a = arg;
    int side = HM_STAMP_SIDE(a->r);

    pw_shape_rrect(c, 0, 0, PW_FX(side), PW_FX(side), PW_FX(a->r), a->line, PW_RGB(255, 255, 255),
                   256);
}

static void rrect(hm_gfx *g, int x, int y, int w, int h, int r, int line, unsigned int color,
                  int alpha)
{
    hm_sprite parts[9];
    corner_arg a;
    char key[32];
    stamp *s;
    int n;

    if (alpha <= 0 || w <= 0 || h <= 0)
        return;
    a.r = hm_rrect_radius(w, h, r);
    a.line = line;
    if (a.r < 1) {
        fill(g, x, y, w, h, color, alpha);
        return;
    }
    stbsp_snprintf(key, sizeof(key), "r%d/%d", a.r, line);
    s = get_stamp(key, HM_STAMP_SIDE(a.r), HM_STAMP_SIDE(a.r), 0, 0, corner, &a);
    if (!s)
        return;
    bind8(s);
    n = hm_rrect_sprites(x, y, w, h, a.r, line == 0, parts);
    for (int i = 0; i < n; i++)
        sprite(parts[i], ge_color(color, alpha));
}

static void image(hm_gfx *g, const unsigned int *px, int w, int h, int stride, int x, int y, int dw,
                  int dh, int alpha)
{
    int scaled = dw != w || dh != h;
    /* Scaled, the GE filters between texels: half a texel in from the edges, so it never reads
       past the image (the next row, or memory after it) - a thin line at the bottom otherwise. */
    float in = scaled ? 0.5f : 0.0f;
    hm_sprite s = {(float)x, (float)y, (float)(x + dw), (float)(y + dh),
                   in,       in,       (float)w - in,   (float)h - in};

    (void)g;
    if (!px || alpha <= 0 || w <= 0 || h <= 0)
        return;
    bind(px, w, h, stride, scaled);
    sprite(s, ge_color(PW_RGB(255, 255, 255), alpha));
}

/* A full-screen 1:1 picture in columns (each a sprite the GE's texture cache holds). */
static void screen_image(const unsigned int *px, int alpha)
{
    bind(px, SCR_W, SCR_H, BUF_W, 0);
    for (int x = 0; x < SCR_W; x += SLICE) {
        int x1 = x + SLICE < SCR_W ? x + SLICE : SCR_W;
        hm_sprite s = {(float)x, 0, (float)x1, SCR_H, (float)x, 0, (float)x1, SCR_H};

        sprite(s, ge_color(PW_RGB(255, 255, 255), alpha));
    }
}

/* The VRAM copy of an 8888 background, or NULL (then the slow RAM texture is drawn). */
static const void *vram_bg(const unsigned int *bg)
{
    for (int i = 0; i < BG_SLOTS; i++)
        if (bg && g_bg_of[i] == bg)
            return (const void *)(VRAM_GE + 2u * FRAME + (unsigned int)i * BG16);
    return 0;
}

static void layer(const unsigned int *px, int alpha)
{
    const void *v = vram_bg(px);

    if (!v) {
        screen_image(px, alpha);
        return;
    }
    texturing(1);
    sceGuTexMode(GU_PSM_5650, 0, 0, 0);
    sceGuTexImage(0, 512, 512, BUF_W, v);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    for (int x = 0; x < SCR_W; x += SLICE) {
        int x1 = x + SLICE < SCR_W ? x + SLICE : SCR_W;
        hm_sprite s = {(float)x, 0, (float)x1, SCR_H, (float)x, 0, (float)x1, SCR_H};

        sprite(s, ge_color(PW_RGB(255, 255, 255), alpha));
    }
}

static void backdrop(hm_gfx *g, const unsigned int *bg, const unsigned int *prev, int mix)
{
    if (!bg) {
        fill(g, 0, 0, SCR_W, SCR_H, PW_RGB(0, 0, 0), 256);
        return;
    }
    sceGuDisable(GU_BLEND); /* the bottom layer is opaque: nothing to read back */
    layer(prev && mix < 256 ? prev : bg, 256);
    sceGuEnable(GU_BLEND);
    if (prev && mix < 256 && mix > 0)
        layer(bg, mix);
}

/*
 * The XMB's wave as ui/xmbbg.c draws it on the CPU, in triangle strips: across the screen a
 * column of vertices every 8 px, down each column a vertex wherever the ribbon's brightness bends
 * (its profile is piecewise linear), the GE shading linearly between them. White over the sky
 * with that alpha is the CPU's lighten.
 */
static void band(const float *top, float d0, int a0, float d1, int a1, int alpha)
{
    cvert *v = sceGuGetMemory(2 * WAVE_COLS * sizeof(cvert));
    unsigned int c0 = ge_color(PW_RGB(255, 255, 255), a0 * alpha / 256),
                 c1 = ge_color(PW_RGB(255, 255, 255), a1 * alpha / 256);

    for (int i = 0; i < WAVE_COLS; i++) {
        float x = (float)(i * WAVE_STEP);

        v[2 * i] = (cvert){c0, x, top[i] + d0, 0};
        v[2 * i + 1] = (cvert){c1, x, top[i] + d1, 0};
    }
    sceGuDrawArray(GU_TRIANGLE_STRIP, CFMT, 2 * WAVE_COLS, 0, v);
}

static void wave(hm_gfx *g, unsigned int t_ms, int alpha)
{
    float top[WAVE_COLS], top2[WAVE_COLS];

    (void)g;
    if (alpha <= 0)
        return;
    for (int i = 0; i < WAVE_COLS; i++) {
        int t1, t2;

        pw_xmbbg_crests(t_ms, i * WAVE_STEP, &t1, &t2);
        top[i] = (float)t1 / 16.0f;
        top2[i] = (float)t2 / 16.0f;
    }
    texturing(0);
    sceGuShadeModel(GU_SMOOTH);
    band(top, -1, 0, 0, PW_XMBBG_EDGE_A, alpha);
    band(top, 0, PW_XMBBG_EDGE_A, PW_XMBBG_BAND_D, PW_XMBBG_BAND_A, alpha);
    band(top, PW_XMBBG_BAND_D, PW_XMBBG_BAND_A, PW_XMBBG_BODY_D, 0, alpha);
    band(top2, -PW_XMBBG_RIBBON_D, 0, 0, PW_XMBBG_RIBBON_A, alpha);
    band(top2, 0, PW_XMBBG_RIBBON_A, PW_XMBBG_RIBBON_D, 0, alpha);
    sceGuShadeModel(GU_FLAT);
}

/* 4 x 4 ordered dither, so the blurred art keeps its smooth gradients in 16 bits. */
static const unsigned char k_bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

void hm_gu_upload_bg(int slot, const unsigned int *px)
{
    unsigned short *out;

    if (slot < 0 || slot >= BG_SLOTS || !px)
        return;
    g_bg_of[slot] = 0; /* not drawn from VRAM while it changes */
    out = (unsigned short *)(VRAM_CPU + 2u * FRAME + (unsigned int)slot * BG16);
    for (int y = 0; y < SCR_H; y++) {
        const unsigned int *row = px + y * BUF_W;
        unsigned short *o = out + y * BUF_W;

        for (int x = 0; x < SCR_W; x++) {
            unsigned int p = row[x], d = k_bayer[(y & 3) * 4 + (x & 3)];
            unsigned int r = (p & 0xFF) + (d >> 1), gg = ((p >> 8) & 0xFF) + (d >> 2),
                         b = ((p >> 16) & 0xFF) + (d >> 1);

            r = r > 255 ? 255 : r;
            gg = gg > 255 ? 255 : gg;
            b = b > 255 ? 255 : b;
            o[x] = (unsigned short)((r >> 3) | (gg >> 2) << 5 | (b >> 3) << 11);
        }
    }
    g_bg_of[slot] = px;
}

typedef struct {
    pw_font *f;
    const char *s;
    int max_w, x, y;
} text_arg;

static void text_line(pw_canvas *c, const void *arg)
{
    const text_arg *a = arg;

    if (a->max_w > 0)
        pw_text_draw_fit(c, a->f, a->x, a->y, a->s, a->max_w, PW_RGB(255, 255, 255), 256, 0);
    else
        pw_text_draw(c, a->f, a->x, a->y, a->s, PW_RGB(255, 255, 255), 256, 0);
}

/* The text stamp of s as drawn with f and max_w: found, or made (when make). */
static stamp *text_stamp(pw_font *f, const char *s, int max_w, int make)
{
    char key[KEY];
    unsigned int k;
    text_arg a = {f, s, max_w, 3, 0};
    stamp *st;
    int w;

    stbsp_snprintf(key, sizeof(key), "t%p/%d/%08x/%.80s", (void *)f, max_w, hash(s), s);
    k = hash(key);
    if ((st = find_stamp(key, k)) != 0 || !make)
        return st;
    w = pw_text_width(f, s);
    if (max_w > 0 && w > max_w)
        w = max_w;
    a.y = f->ascent + 3; /* the baseline in the stamp: room above for accents */
    return make_stamp(key, k, w + 8, f->ascent + f->descent + 7, a.x, a.y, text_line, &a);
}

static void text(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w, unsigned int color,
                 int alpha)
{
    stamp *st;

    (void)g;
    if (!s || !s[0] || alpha <= 0)
        return;
    if ((st = text_stamp(f, s, max_w, 1)) != 0)
        draw_stamp(st, (float)x, (float)y, ge_color(color, alpha));
}

/* A text stamp made before it is needed, within WARM_US a frame (hm_gu_finish .. hm_gu_end, while
   the GE draws). 1 when it is ready. */
static int warm(hm_gfx *g, pw_font *f, const char *s, int max_w)
{
    unsigned int t0;
    int ok;

    (void)g;
    if (!s || !s[0])
        return 1;
    if (text_stamp(f, s, max_w, 0))
        return 1;
    if (g_warm_us >= WARM_US)
        return 0;
    t0 = sceKernelGetSystemTimeLow();
    ok = text_stamp(f, s, max_w, 1) != 0;
    g_warm_us += sceKernelGetSystemTimeLow() - t0;
    return ok;
}

typedef struct {
    int kind, which, size;
} glyph_arg;

static void glyph(pw_canvas *c, const void *arg)
{
    const glyph_arg *a = arg;
    int mid = c->w / 2;

    if (a->kind)
        pw_button_draw(c, a->which, mid, mid, a->size, PW_RGB(255, 255, 255), 256);
    else
        hm_icon_draw(c, a->which, mid, mid, a->size, PW_RGB(255, 255, 255), 256);
}

static void glyph_draw(int kind, int which, int cx, int cy, int size, unsigned int color, int alpha)
{
    glyph_arg a = {kind, which, size};
    int side = 2 * size + 8;
    char key[32];
    stamp *s;

    if (alpha <= 0 || size <= 0)
        return;
    stbsp_snprintf(key, sizeof(key), "%c%d/%d", kind ? 'b' : 'i', which, size);
    s = get_stamp(key, side, side, side / 2, side / 2, glyph, &a);
    if (s)
        draw_stamp(s, (float)cx, (float)cy, ge_color(color, alpha));
}

static void icon(hm_gfx *g, int which, int cx, int cy, int size, unsigned int color, int alpha)
{
    (void)g;
    glyph_draw(0, which, cx, cy, size, color, alpha);
}

static void button(hm_gfx *g, int which, int cx, int cy, int size, unsigned int color, int alpha)
{
    (void)g;
    glyph_draw(1, which, cx, cy, size, color, alpha);
}

/* The sprites are cut on the CPU (hm_sprite_clip_rect, texture coordinates moved with them), as
   they are cut to the screen: no GE state to keep in step. */
static void clip(hm_gfx *g, int y0, int y1)
{
    (void)g;
    g_cy0 = y0 < 0 ? 0 : y0;
    g_cy1 = y1 > SCR_H ? SCR_H : y1;
}

/* ---- frames ---- */

int hm_gu_init(void)
{
    g_ops.fill = fill;
    g_ops.rrect = rrect;
    g_ops.image = image;
    g_ops.backdrop = backdrop;
    g_ops.wave = wave;
    g_ops.text = text;
    g_ops.icon = icon;
    g_ops.button = button;
    g_ops.warm = warm;
    g_ops.clip = clip;
    for (int i = 0; i < 256; i++)
        g_clut[i] = (unsigned int)i << 24 | 0x00FFFFFF;
    sceKernelDcacheWritebackRange(g_clut, sizeof(g_clut));

    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)FRAME, BUF_W);
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_FLAT);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return 0;
}

/* Everything this backend relies on; again at every frame, since the firmware's dialogs (OSK,
   savedata) draw with the same GE between our frames. */
static void state(void)
{
    sceGuDrawBuffer(GU_PSM_8888, g_draw, BUF_W);
    sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuShadeModel(GU_FLAT);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexOffset(0, 0);
    sceGuTexScale(1, 1);
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(256 / 8, g_clut);
}

hm_gfx *hm_gu_begin(void)
{
    g_frame++;
    g_textured = -1;
    g_warm_us = 0;
    g_made = 0;
    g_cy0 = 0;
    g_cy1 = SCR_H;
    sceGuStart(GU_DIRECT, g_list);
    state();
    return &g_ops;
}

void hm_gu_finish(void)
{
    sceGuFinish(); /* the GE starts on the list; the CPU is free until hm_gu_end */
}

unsigned int hm_gu_end(void)
{
    unsigned int t0 = sceKernelGetSystemTimeLow(), t1;

    hm_gu_sync();
    t1 = sceKernelGetSystemTimeLow();
    hm_gu_present();
    return t1 - t0;
}

void hm_gu_sync(void)
{
    sceGuSync(0, 0);
}

void hm_gu_present(void)
{
    sceDisplayWaitVblankStart();
    g_draw = sceGuSwapBuffers();
}

void hm_gu_stats(unsigned int *made, unsigned int *count, unsigned int *bytes)
{
    int n = 0;

    for (int i = 0; i < STAMPS; i++)
        n += g_stamps[i].px != 0;
    *made = g_made;
    *count = (unsigned int)n;
    *bytes = g_stamp_bytes;
}

void hm_gu_written(const void *p, unsigned int bytes)
{
    sceKernelDcacheWritebackRange(p, bytes);
}

void hm_gu_drop_stamps(void)
{
    for (int i = 0; i < STAMPS; i++) {
        free(g_stamps[i].px);
        g_stamps[i].px = 0;
    }
    g_stamp_bytes = 0;
    free(g_scratch);
    g_scratch = 0;
    g_scratch_px = 0;
}
