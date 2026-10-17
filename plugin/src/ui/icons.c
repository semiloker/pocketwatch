// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/icons.h"
#include "ui/shapes.h"

/* Scale a design coordinate (a 64-unit box centred on 0) to PW_FX around the centre. */
#define S(v) ((v) * size * 16 / 64)

static void tray(pw_canvas *c, int x, int y, int size, int up, unsigned int color, int alpha)
{
    int t = S(5); /* line thickness */

    /* open tray: bottom and two short sides */
    pw_shape_capsule(c, x + S(-24), y + S(8), x + S(-24), y + S(22), t / 2, color, alpha);
    pw_shape_capsule(c, x + S(-24), y + S(22), x + S(24), y + S(22), t / 2, color, alpha);
    pw_shape_capsule(c, x + S(24), y + S(8), x + S(24), y + S(22), t / 2, color, alpha);
    /* arrow */
    pw_shape_capsule(c, x, y + S(-24), x, y + S(10), t / 2, color, alpha);
    if (up) {
        pw_shape_capsule(c, x, y + S(-24), x + S(-12), y + S(-12), t / 2, color, alpha);
        pw_shape_capsule(c, x, y + S(-24), x + S(12), y + S(-12), t / 2, color, alpha);
    } else {
        pw_shape_capsule(c, x, y + S(10), x + S(-12), y + S(-2), t / 2, color, alpha);
        pw_shape_capsule(c, x, y + S(10), x + S(12), y + S(-2), t / 2, color, alpha);
    }
}

static void draw_direct(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color,
                        int alpha)
{
    int x = PW_FX(cx), y = PW_FX(cy), t = S(5);

    switch (icon) {
    case PW_ICON_WATCH:
        pw_shape_ring(c, x, y + S(4), S(24), t, 0, color, alpha);
        pw_shape_rrect(c, x + S(-5), y + S(-30), S(10), S(7), S(2), 0, color, alpha);
        pw_shape_capsule(c, x, y + S(4), x, y + S(-12), t / 2, color, alpha);
        pw_shape_capsule(c, x, y + S(4), x + S(11), y + S(10), t / 2, color, alpha);
        break;
    case PW_ICON_RESUME:
        pw_shape_rrect(c, x + S(-28), y + S(-28), S(56), S(56), S(10), t, color, alpha);
        pw_shape_triangle(c, x + S(-9), y + S(-15), x + S(-9), y + S(15), x + S(16), y, color,
                          alpha);
        break;
    case PW_ICON_SAVE:
        tray(c, x, y, size, 0, color, alpha);
        break;
    case PW_ICON_LOAD:
        tray(c, x, y, size, 1, color, alpha);
        break;
    case PW_ICON_QUIT:
        pw_shape_ring(c, x, y + S(3), S(21), t, S(9), color, alpha);
        pw_shape_capsule(c, x, y + S(-26), x, y + S(-2), t / 2, color, alpha);
        break;
    case PW_ICON_GEAR: { /* a ring with eight teeth and a hole */
        static const signed char teeth[8][4] = {
            {0, -19, 0, -26},   {0, 19, 0, 26},       {-19, 0, -26, 0}, {19, 0, 26, 0},
            {13, -13, 18, -18}, {-13, -13, -18, -18}, {13, 13, 18, 18}, {-13, 13, -18, 18}};

        pw_shape_ring(c, x, y, S(15), S(8), 0, color, alpha);
        for (int i = 0; i < 8; i++)
            pw_shape_capsule(c, x + S(teeth[i][0]), y + S(teeth[i][1]), x + S(teeth[i][2]),
                             y + S(teeth[i][3]), S(4), color, alpha);
        break;
    }
    case PW_ICON_DISC:
        pw_shape_disc(c, x, y, S(32), color, alpha);
        break;
    case PW_ICON_CARD:
        pw_shape_rrect(c, x + S(-16), y + S(-26), S(32), S(52), S(5), t, color, alpha);
        pw_shape_capsule(c, x + S(-6), y + S(-16), x + S(6), y + S(-16), t / 2, color, alpha);
        break;
    default:
        break;
    }
}

/*
 * The cache: each icon at each size is rasterised once (white on black, the red channel kept as
 * coverage) and then drawn like a glyph, a blend per pixel instead of the shapes' distances. Its
 * memory is the caller's (ui/fb.c: next to the screen's pages, so it is dropped with them after a
 * suspend).
 */
#define CACHE_MAX 32

static struct {
    unsigned char *mem;
    unsigned int cap, used;
    int n;
    struct {
        short icon, size, w, h;
        unsigned int off;
    } e[CACHE_MAX];
} g_cache;

void pw_icon_cache(void *mem, unsigned int cap)
{
    g_cache.mem = mem;
    g_cache.cap = cap & ~3u;
    g_cache.used = 0;
    g_cache.n = 0;
}

static int cached(int icon, int size)
{
    int w = size + 8, h = size + 8; /* strokes and their ramp stay inside */
    unsigned int need = (unsigned int)(w * h), tmp_at;
    unsigned int *tmp;
    unsigned char *mask;

    for (int i = 0; i < g_cache.n; i++)
        if (g_cache.e[i].icon == icon && g_cache.e[i].size == size)
            return i;
    tmp_at = (g_cache.cap - need * 4) & ~3u; /* scratch at the far end, while making it */
    if (g_cache.n >= CACHE_MAX || need * 4 > g_cache.cap || g_cache.used + need > tmp_at)
        return -1;
    tmp = (unsigned int *)(void *)(g_cache.mem + tmp_at);
    for (unsigned int k = 0; k < need; k++)
        tmp[k] = PW_RGB(0, 0, 0);
    {
        pw_canvas cv = {tmp, w, h, w};

        draw_direct(&cv, icon, w / 2, h / 2, size, PW_RGB(255, 255, 255), 256);
    }
    mask = g_cache.mem + g_cache.used;
    for (unsigned int k = 0; k < need; k++)
        mask[k] = (unsigned char)(tmp[k] & 0xFF);
    g_cache.e[g_cache.n].icon = (short)icon;
    g_cache.e[g_cache.n].size = (short)size;
    g_cache.e[g_cache.n].w = (short)w;
    g_cache.e[g_cache.n].h = (short)h;
    g_cache.e[g_cache.n].off = g_cache.used;
    g_cache.used += (need + 3) & ~3u;
    return g_cache.n++;
}

void pw_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha)
{
    int i = g_cache.mem && size > 0 && size <= 96 ? cached(icon, size) : -1;

    if (i < 0) {
        draw_direct(c, icon, cx, cy, size, color, alpha);
        return;
    }
    pw_draw_mask(c, cx - g_cache.e[i].w / 2, cy - g_cache.e[i].h / 2,
                 g_cache.mem + g_cache.e[i].off, g_cache.e[i].w, g_cache.e[i].h, g_cache.e[i].w,
                 color, alpha);
}

void pw_button_draw(pw_canvas *c, int button, int cx, int cy, int size, unsigned int color,
                    int alpha)
{
    int x = PW_FX(cx), y = PW_FX(cy), t = S(6);

    pw_shape_ring(c, x, y, S(28), t, 0, color, alpha);
    switch (button) {
    case PW_BTN_CROSS:
        pw_shape_capsule(c, x + S(-12), y + S(-12), x + S(12), y + S(12), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(12), y + S(-12), x + S(-12), y + S(12), t / 2, color, alpha);
        break;
    case PW_BTN_CIRCLE:
        pw_shape_ring(c, x, y, S(13), t, 0, color, alpha);
        break;
    case PW_BTN_TRIANGLE:
        pw_shape_capsule(c, x, y + S(-14), x + S(-13), y + S(10), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-13), y + S(10), x + S(13), y + S(10), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(13), y + S(10), x, y + S(-14), t / 2, color, alpha);
        break;
    case PW_BTN_SQUARE:
        pw_shape_rrect(c, x + S(-12), y + S(-12), S(24), S(24), S(2), t, color, alpha);
        break;
    default:
        break;
    }
}
