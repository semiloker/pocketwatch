// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <string.h>

#include "gfx_gu.h"
#include "sprites.h"
#include "stb_sprintf.h"
#include "ui/draw.h"
#include "ui/icons.h"
#include "ui/shapes.h"

#define SCR_W  480
#define SCR_H  272
#define BUF_W  512
#define FRAME  (BUF_W * SCR_H * 4) /* bytes of one frame in VRAM */
#define SLICE  64                  /* wide 1:1 images go in columns: the GE's texture cache */
#define STAMPS 96                  /* text lines, corners and icons kept as textures */
#define KEY    112

static unsigned int __attribute__((aligned(64))) g_list[128 * 1024];
static unsigned int g_frame; /* counts frames: what a stamp was last used in */
static hm_gfx g_ops;

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

/* One textured sprite, clipped to the screen. */
static void sprite(hm_sprite s, unsigned int color)
{
    tvert *v;

    if (!hm_sprite_clip(&s, SCR_W, SCR_H))
        return;
    v = sceGuGetMemory(2 * sizeof(tvert));
    v[0] = (tvert){s.u0, s.v0, color, s.x0, s.y0, 0};
    v[1] = (tvert){s.u1, s.v1, color, s.x1, s.y1, 0};
    sceGuDrawArray(GU_SPRITES, TFMT, 2, 0, v);
}

/* ---- stamps: what the CPU renderer draws once, kept as alpha textures ---- */

typedef struct {
    char key[KEY];
    int w, h, stride; /* the texture's size and row length (a multiple of 16 texels) */
    int ox, oy;       /* where the drawing's origin is in it */
    unsigned int *px, used;
} stamp;

static stamp g_stamps[STAMPS];

typedef void (*rasterise)(pw_canvas *c, const void *arg);

/* The stamp for key, made by r (white on black, w x h, the origin at ox, oy) when missing. */
static stamp *get_stamp(const char *key, int w, int h, int ox, int oy, rasterise r, const void *arg)
{
    stamp *s = 0, *old = 0;
    pw_canvas c;

    for (int i = 0; i < STAMPS; i++) {
        if (g_stamps[i].px && strcmp(g_stamps[i].key, key) == 0) {
            g_stamps[i].used = g_frame;
            return &g_stamps[i];
        }
        if (!g_stamps[i].px && !s)
            s = &g_stamps[i];
        if (g_stamps[i].px && g_stamps[i].used != g_frame && (!old || g_stamps[i].used < old->used))
            old = &g_stamps[i];
    }
    if (!s) { /* the least recently used, never one this frame's list still points at */
        if (!old)
            return 0;
        free(old->px);
        old->px = 0;
        s = old;
    }
    w = w < 1 ? 1 : w > 512 ? 512 : w;
    h = h < 1 ? 1 : h > 512 ? 512 : h;
    s->stride = (w + 15) & ~15;
    s->px = memalign(16, (size_t)(s->stride * h * 4));
    if (!s->px)
        return 0;
    memset(s->px, 0, (size_t)(s->stride * h * 4));
    c.px = s->px;
    c.w = w;
    c.h = h;
    c.stride = s->stride;
    r(&c, arg);
    for (int i = 0; i < s->stride * h; i++) { /* coverage (the red channel) becomes alpha */
        unsigned int a = s->px[i] & 0xFF;

        s->px[i] = a << 24 | 0x00FFFFFF;
    }
    sceKernelDcacheWritebackRange(s->px, (unsigned int)(s->stride * h * 4));
    strncpy(s->key, key, KEY - 1);
    s->key[KEY - 1] = 0;
    s->w = w;
    s->h = h;
    s->ox = ox;
    s->oy = oy;
    s->used = g_frame;
    return s;
}

static void draw_stamp(const stamp *s, float x, float y, unsigned int color)
{
    hm_sprite sp = {x - s->ox, y - s->oy, x - s->ox + s->w, y - s->oy + s->h,
                    0,         0,         (float)s->w,      (float)s->h};

    bind(s->px, s->w, s->h, s->stride, 0);
    sprite(sp, color);
}

/* ---- the operations ---- */

static void fill(hm_gfx *g, int x, int y, int w, int h, unsigned int color, int alpha)
{
    cvert *v;
    hm_sprite s = {(float)x, (float)y, (float)(x + w), (float)(y + h), 0, 0, 0, 0};

    (void)g;
    if (alpha <= 0 || !hm_sprite_clip(&s, SCR_W, SCR_H))
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
    bind(s->px, s->w, s->h, s->stride, 0);
    n = hm_rrect_sprites(x, y, w, h, a.r, line == 0, parts);
    for (int i = 0; i < n; i++)
        sprite(parts[i], ge_color(color, alpha));
}

static void image(hm_gfx *g, const unsigned int *px, int w, int h, int stride, int x, int y, int dw,
                  int dh, int alpha)
{
    hm_sprite s = {(float)x, (float)y, (float)(x + dw), (float)(y + dh), 0, 0, (float)w, (float)h};

    (void)g;
    if (!px || alpha <= 0 || w <= 0 || h <= 0)
        return;
    bind(px, w, h, stride, dw != w || dh != h);
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

static void backdrop(hm_gfx *g, const unsigned int *bg, const unsigned int *prev, int mix)
{
    if (!bg) {
        fill(g, 0, 0, SCR_W, SCR_H, PW_RGB(0, 0, 0), 256);
        return;
    }
    sceGuDisable(GU_BLEND); /* the bottom layer is opaque: nothing to read back */
    screen_image(prev && mix < 256 ? prev : bg, 256);
    sceGuEnable(GU_BLEND);
    if (prev && mix < 256 && mix > 0)
        screen_image(bg, mix);
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

/* FNV-1a: the whole string in the key, whatever its length. */
static unsigned int hash(const char *s)
{
    unsigned int h = 2166136261u;

    while (*s)
        h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

static void text(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w, unsigned int color,
                 int alpha)
{
    char key[KEY];
    text_arg a = {f, s, max_w, 3, 0};
    int w;
    stamp *st;

    (void)g;
    if (!s || !s[0] || alpha <= 0)
        return;
    stbsp_snprintf(key, sizeof(key), "t%p/%d/%08x/%.80s", (void *)f, max_w, hash(s), s);
    w = pw_text_width(f, s);
    if (max_w > 0 && w > max_w)
        w = max_w;
    a.y = f->ascent + 3; /* the baseline in the stamp: room above for accents */
    st = get_stamp(key, w + 8, f->ascent + f->descent + 7, a.x, a.y, text_line, &a);
    if (st)
        draw_stamp(st, (float)x, (float)y, ge_color(color, alpha));
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
        pw_icon_draw(c, a->which, mid, mid, a->size, PW_RGB(255, 255, 255), 256);
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

/* ---- frames ---- */

int hm_gu_init(void)
{
    g_ops.fill = fill;
    g_ops.rrect = rrect;
    g_ops.image = image;
    g_ops.backdrop = backdrop;
    g_ops.text = text;
    g_ops.icon = icon;
    g_ops.button = button;

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

hm_gfx *hm_gu_begin(void)
{
    g_frame++;
    g_textured = -1;
    sceGuStart(GU_DIRECT, g_list);
    return &g_ops;
}

unsigned int hm_gu_end(void)
{
    unsigned int t0, t1;

    sceGuFinish();
    t0 = sceKernelGetSystemTimeLow();
    sceGuSync(0, 0);
    t1 = sceKernelGetSystemTimeLow();
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    return t1 - t0;
}

void hm_gu_written(const void *p, unsigned int bytes)
{
    sceKernelDcacheWritebackRange(p, bytes);
}
