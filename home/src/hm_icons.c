// SPDX-License-Identifier: GPL-3.0-or-later
#include "hm_icons.h"
#include "home_view.h"
#include "ui/shapes.h"

/* Scale a design coordinate (a 64-unit box centred on 0) to PW_FX around the centre. */
#define S(v) ((v) * size * 16 / 64)

/* A line of points (design units) as capsules. */
static void path(pw_canvas *c, int x, int y, int size, const signed char *p, int n, int t,
                 unsigned int color, int alpha)
{
    for (int i = 0; i + 3 < 2 * n; i += 2)
        pw_shape_capsule(c, x + S(p[i]), y + S(p[i + 1]), x + S(p[i + 2]), y + S(p[i + 3]), t,
                         color, alpha);
}

/* The left half of a disc (a fan of triangles). */
static void half_disc(pw_canvas *c, int x, int y, int r, unsigned int color, int alpha)
{
    static const signed char k[9][2] = {{0, -64},  {-24, -59}, {-45, -45}, {-59, -24}, {-64, 0},
                                        {-59, 24}, {-45, 45},  {-24, 59},  {0, 64}};

    for (int i = 0; i < 8; i++)
        pw_shape_triangle(c, x, y, x + k[i][0] * r / 64, y + k[i][1] * r / 64,
                          x + k[i + 1][0] * r / 64, y + k[i + 1][1] * r / 64, color, alpha);
}

void hm_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha)
{
    int x = PW_FX(cx), y = PW_FX(cy), t = S(5);

    switch (icon) {
    case HM_ICON_GAMES:
        pw_shape_rrect(c, x + S(-28), y + S(-14), S(56), S(28), S(13), t, color, alpha);
        pw_shape_capsule(c, x + S(-20), y, x + S(-8), y, t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-14), y + S(-6), x + S(-14), y + S(6), t / 2, color, alpha);
        pw_shape_disc(c, x + S(10), y + S(-3), S(4), color, alpha);
        pw_shape_disc(c, x + S(18), y + S(4), S(4), color, alpha);
        break;
    case HM_ICON_MUSIC:
        pw_shape_disc(c, x + S(-13), y + S(15), S(8), color, alpha);
        pw_shape_disc(c, x + S(15), y + S(9), S(8), color, alpha);
        pw_shape_capsule(c, x + S(-6), y + S(15), x + S(-6), y + S(-17), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(22), y + S(9), x + S(22), y + S(-23), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-6), y + S(-17), x + S(22), y + S(-23), t * 3 / 4, color, alpha);
        break;
    case HM_ICON_PHOTOS:
        pw_shape_rrect(c, x + S(-27), y + S(-21), S(54), S(42), S(6), t, color, alpha);
        pw_shape_triangle(c, x + S(-19), y + S(14), x + S(-4), y + S(-6), x + S(10), y + S(14),
                          color, alpha);
        pw_shape_triangle(c, x + S(2), y + S(14), x + S(12), y + S(2), x + S(21), y + S(14), color,
                          alpha);
        pw_shape_disc(c, x + S(12), y + S(-9), S(5), color, alpha);
        break;
    case HM_ICON_SYSTEM:
        pw_shape_ring(c, x, y, S(26), t, 0, color, alpha);
        pw_shape_disc(c, x, y + S(-11), S(4), color, alpha);
        pw_shape_capsule(c, x, y + S(-1), x, y + S(13), t * 3 / 5, color, alpha);
        break;
    case HM_ICON_PLAY:
        pw_shape_triangle(c, x + S(-10), y + S(-16), x + S(-10), y + S(16), x + S(17), y, color,
                          alpha);
        break;
    case HM_ICON_PAUSE:
        pw_shape_rrect(c, x + S(-13), y + S(-15), S(9), S(30), S(2), 0, color, alpha);
        pw_shape_rrect(c, x + S(4), y + S(-15), S(9), S(30), S(2), 0, color, alpha);
        break;
    case HM_ICON_PALETTE:
        pw_shape_ring(c, x, y, S(25), t, 0, color, alpha);
        pw_shape_disc(c, x + S(-11), y + S(-9), S(5), color, alpha);
        pw_shape_disc(c, x + S(4), y + S(-14), S(5), color, alpha);
        pw_shape_disc(c, x + S(15), y + S(-2), S(5), color, alpha);
        pw_shape_disc(c, x + S(-12), y + S(8), S(5), color, alpha);
        break;
    case HM_ICON_SPEAKER: {
        static const signed char w1[] = {10, -9, 15, 0, 10, 9}, w2[] = {17, -17, 25, 0, 17, 17};

        pw_shape_rrect(c, x + S(-26), y + S(-8), S(12), S(16), S(2), 0, color, alpha);
        pw_shape_triangle(c, x + S(-16), y + S(-8), x + S(2), y + S(-21), x + S(2), y + S(21),
                          color, alpha);
        pw_shape_triangle(c, x + S(-16), y + S(-8), x + S(2), y + S(21), x + S(-16), y + S(8),
                          color, alpha);
        path(c, x, y, size, w1, 3, t / 2, color, alpha);
        path(c, x, y, size, w2, 3, t / 2, color, alpha);
        break;
    }
    case HM_ICON_CLOCK:
        pw_shape_ring(c, x, y, S(25), t, 0, color, alpha);
        pw_shape_capsule(c, x, y, x, y + S(-15), t / 2, color, alpha);
        pw_shape_capsule(c, x, y, x + S(11), y + S(7), t / 2, color, alpha);
        break;
    case HM_ICON_GLOBE:
        pw_shape_ring(c, x, y, S(25), t * 4 / 5, 0, color, alpha);
        pw_shape_capsule(c, x + S(-24), y, x + S(24), y, t / 3, color, alpha);
        pw_shape_capsule(c, x + S(-19), y + S(-12), x + S(19), y + S(-12), t / 3, color, alpha);
        pw_shape_capsule(c, x + S(-19), y + S(12), x + S(19), y + S(12), t / 3, color, alpha);
        pw_shape_rrect(c, x + S(-10), y + S(-24), S(20), S(48), S(10), t * 2 / 3, color, alpha);
        break;
    case HM_ICON_BATTERY:
        pw_shape_rrect(c, x + S(-26), y + S(-13), S(46), S(26), S(5), t, color, alpha);
        pw_shape_rrect(c, x + S(21), y + S(-6), S(6), S(12), S(2), 0, color, alpha);
        pw_shape_rrect(c, x + S(-19), y + S(-6), S(10), S(12), S(1), 0, color, alpha);
        pw_shape_rrect(c, x + S(-7), y + S(-6), S(10), S(12), S(1), 0, color, alpha);
        break;
    case HM_ICON_USB:
        pw_shape_capsule(c, x, y + S(20), x, y + S(-16), t / 2, color, alpha);
        pw_shape_triangle(c, x + S(-7), y + S(-14), x + S(7), y + S(-14), x, y + S(-26), color,
                          alpha);
        pw_shape_capsule(c, x, y + S(6), x + S(-13), y + S(-4), t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-13), y + S(-4), x + S(-13), y + S(-9), t / 2, color, alpha);
        pw_shape_disc(c, x + S(-13), y + S(-11), S(5), color, alpha);
        pw_shape_capsule(c, x, y + S(12), x + S(13), y + S(2), t / 2, color, alpha);
        pw_shape_rrect(c, x + S(8), y + S(-8), S(10), S(10), S(1), 0, color, alpha);
        pw_shape_disc(c, x, y + S(21), S(6), color, alpha);
        break;
    case HM_ICON_PREV:
    case HM_ICON_NEXT: {
        int d = icon == HM_ICON_PREV ? -1 : 1;

        pw_shape_rrect(c, x + S(d > 0 ? 13 : -19), y + S(-16), S(6), S(32), S(2), 0, color, alpha);
        pw_shape_triangle(c, x + S(10 * d), y, x + S(-16 * d), y + S(-17), x + S(-16 * d),
                          y + S(17), color, alpha);
        break;
    }
    case HM_ICON_SHUFFLE: {
        static const signed char a[] = {-24, 14, -10, 14, 8, -12, 18, -12},
                                 b[] = {-24, -12, -10, -12, 8, 14, 18, 14};

        path(c, x, y, size, a, 4, t / 2, color, alpha);
        path(c, x, y, size, b, 4, t / 2, color, alpha);
        pw_shape_triangle(c, x + S(16), y + S(-19), x + S(16), y + S(-5), x + S(27), y + S(-12),
                          color, alpha);
        pw_shape_triangle(c, x + S(16), y + S(7), x + S(16), y + S(21), x + S(27), y + S(14), color,
                          alpha);
        break;
    }
    case HM_ICON_REPEAT:
    case HM_ICON_REPEAT1:
        pw_shape_rrect(c, x + S(-22), y + S(-13), S(44), S(26), S(10), t / 2 + 4, color, alpha);
        pw_shape_triangle(c, x + S(4), y + S(-20), x + S(4), y + S(-6), x + S(14), y + S(-13),
                          color, alpha);
        pw_shape_triangle(c, x + S(-4), y + S(6), x + S(-4), y + S(20), x + S(-14), y + S(13),
                          color, alpha);
        if (icon == HM_ICON_REPEAT1)
            pw_shape_capsule(c, x, y + S(-5), x, y + S(5), t / 2, color, alpha);
        break;
    case HM_ICON_TRASH:
        pw_shape_rrect(c, x + S(-15), y + S(-10), S(30), S(32), S(5), t, color, alpha);
        pw_shape_capsule(c, x + S(-21), y + S(-15), x + S(21), y + S(-15), t / 2, color, alpha);
        pw_shape_rrect(c, x + S(-6), y + S(-22), S(12), S(7), S(2), t / 2 + 4, color, alpha);
        pw_shape_capsule(c, x + S(-5), y + S(-3), x + S(-5), y + S(14), t / 3, color, alpha);
        pw_shape_capsule(c, x + S(5), y + S(-3), x + S(5), y + S(14), t / 3, color, alpha);
        break;
    case HM_ICON_FOLDER:
        pw_shape_rrect(c, x + S(-26), y + S(-20), S(22), S(12), S(4), 0, color, alpha);
        pw_shape_rrect(c, x + S(-26), y + S(-14), S(52), S(34), S(5), t, color, alpha);
        break;
    case HM_ICON_SLIDERS:
        for (int i = -1; i <= 1; i++) {
            static const signed char knob[3] = {-9, 11, -2};

            pw_shape_capsule(c, x + S(-23), y + S(i * 14), x + S(23), y + S(i * 14), t / 3, color,
                             alpha);
            pw_shape_disc(c, x + S(knob[i + 1]), y + S(i * 14), S(6), color, alpha);
        }
        break;
    case HM_ICON_ROTATE:
        pw_shape_ring(c, x, y + S(3), S(19), t, S(9), color, alpha);
        pw_shape_triangle(c, x + S(3), y + S(-25), x + S(3), y + S(-7), x + S(15), y + S(-16),
                          color, alpha);
        break;
    case HM_ICON_SUN: {
        static const signed char r[8][2] = {{0, -1}, {1, -1}, {1, 0},  {1, 1},
                                            {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};

        pw_shape_disc(c, x, y, S(10), color, alpha);
        for (int i = 0; i < 8; i++) {
            int k = r[i][0] && r[i][1] ? 13 : 18, l = r[i][0] && r[i][1] ? 19 : 26;

            pw_shape_capsule(c, x + S(r[i][0] * k), y + S(r[i][1] * k), x + S(r[i][0] * l),
                             y + S(r[i][1] * l), t / 2, color, alpha);
        }
        break;
    }
    case HM_ICON_CONTRAST:
        pw_shape_ring(c, x, y, S(24), t, 0, color, alpha);
        half_disc(c, x, y, S(24), color, alpha);
        break;
    case HM_ICON_DROP:
        pw_shape_disc(c, x, y + S(8), S(15), color, alpha);
        pw_shape_triangle(c, x + S(-13), y + S(1), x + S(13), y + S(1), x, y + S(-25), color,
                          alpha);
        break;
    case HM_ICON_FILTER:
        pw_shape_ring(c, x + S(-8), y + S(-6), S(13), t * 2 / 3, 0, color, alpha);
        pw_shape_ring(c, x + S(8), y + S(-6), S(13), t * 2 / 3, 0, color, alpha);
        pw_shape_ring(c, x, y + S(8), S(13), t * 2 / 3, 0, color, alpha);
        break;
    case HM_ICON_CHECK: {
        static const signed char p[] = {-18, 0, -5, 13, 19, -13};

        path(c, x, y, size, p, 3, t * 3 / 4, color, alpha);
        break;
    }
    case HM_ICON_EXIT: {
        static const signed char door[] = {-2, -22, -24, -22, -24, 22, -2, 22};

        path(c, x, y, size, door, 4, t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-10), y, x + S(16), y, t / 2, color, alpha);
        pw_shape_triangle(c, x + S(12), y + S(-10), x + S(12), y + S(10), x + S(26), y, color,
                          alpha);
        break;
    }
    case HM_ICON_SLEEP: {
        static const signed char z[] = {-14, -14, 14, -14, -14, 14, 14, 14};

        path(c, x, y, size, z, 4, t * 3 / 5, color, alpha);
        break;
    }
    case HM_ICON_LANGUAGE: {
        static const signed char a[] = {-11, 9, 0, -12, 11, 9};

        pw_shape_rrect(c, x + S(-26), y + S(-22), S(52), S(38), S(9), t * 2 / 3, color, alpha);
        pw_shape_triangle(c, x + S(-12), y + S(14), x + S(2), y + S(14), x + S(-14), y + S(26),
                          color, alpha);
        path(c, x, y + S(-2), size, a, 3, t / 2, color, alpha);
        pw_shape_capsule(c, x + S(-6), y + S(2), x + S(6), y + S(2), t / 3, color, alpha);
        break;
    }
    case HM_ICON_USER:
        pw_shape_disc(c, x, y + S(-9), S(11), color, alpha);
        pw_shape_rrect(c, x + S(-20), y + S(6), S(40), S(20), S(10), 0, color, alpha);
        break;
    case HM_ICON_VIGNETTE:
        pw_shape_rrect(c, x + S(-26), y + S(-18), S(52), S(36), S(6), t / 2 + 4, color, alpha);
        pw_shape_ring(c, x, y, S(12), t, 0, color, alpha);
        break;
    case HM_ICON_CHIP:
        pw_shape_rrect(c, x + S(-16), y + S(-16), S(32), S(32), S(4), t, color, alpha);
        pw_shape_rrect(c, x + S(-6), y + S(-6), S(12), S(12), S(2), 0, color, alpha);
        for (int i = -1; i <= 1; i++) {
            pw_shape_capsule(c, x + S(i * 9), y + S(-24), x + S(i * 9), y + S(-18), t / 3, color,
                             alpha);
            pw_shape_capsule(c, x + S(i * 9), y + S(18), x + S(i * 9), y + S(24), t / 3, color,
                             alpha);
            pw_shape_capsule(c, x + S(-24), y + S(i * 9), x + S(-18), y + S(i * 9), t / 3, color,
                             alpha);
            pw_shape_capsule(c, x + S(18), y + S(i * 9), x + S(24), y + S(i * 9), t / 3, color,
                             alpha);
        }
        break;
    case HM_ICON_STAR:
        pw_shape_triangle(c, x, y + S(-25), x + S(-15), y + S(20), x + S(24), y + S(-8), color,
                          alpha);
        pw_shape_triangle(c, x, y + S(-25), x + S(15), y + S(20), x + S(-24), y + S(-8), color,
                          alpha);
        pw_shape_triangle(c, x + S(-15), y + S(20), x + S(15), y + S(20), x, y + S(6), color,
                          alpha);
        break;
    case HM_ICON_UP:
    case HM_ICON_DOWN: {
        int d = icon == HM_ICON_UP ? -1 : 1;

        pw_shape_triangle(c, x + S(-24), y + S(-12 * d), x + S(24), y + S(-12 * d), x,
                          y + S(24 * d), color, alpha);
        break;
    }
    case HM_ICON_LR:
    case HM_ICON_UD: { /* (x, y swapped for up / down) */
        int v = icon == HM_ICON_UD;

        for (int d = -1; d <= 1; d += 2) {
            int tip = S(26 * d), base = S(6 * d), w = S(15);

            if (v)
                pw_shape_triangle(c, x - w, y + base, x + w, y + base, x, y + tip, color, alpha);
            else
                pw_shape_triangle(c, x + base, y - w, x + base, y + w, x + tip, y, color, alpha);
        }
        break;
    }
    case HM_ICON_FLIP:
        pw_shape_capsule(c, x, y + S(-24), x, y + S(24), t / 3, color, alpha);
        pw_shape_triangle(c, x + S(-5), y + S(-16), x + S(-5), y + S(16), x + S(-25), y + S(16),
                          color, alpha);
        pw_shape_triangle(c, x + S(5), y + S(-16), x + S(5), y + S(16), x + S(25), y + S(16), color,
                          alpha * 120 / 256);
        break;
    case HM_ICON_HOME:
        pw_shape_triangle(c, x + S(-26), y + S(-2), x + S(26), y + S(-2), x, y + S(-24), color,
                          alpha);
        pw_shape_rrect(c, x + S(-18), y + S(-4), S(36), S(26), S(3), t, color, alpha);
        pw_shape_rrect(c, x + S(-5), y + S(8), S(10), S(14), S(2), 0, color, alpha);
        break;
    case HM_ICON_LAYERS: {
        static const signed char top[] = {0, -22, 24, -10, 0, 2, -24, -10, 0, -22},
                                 low[] = {-24, 2, 0, 14, 24, 2};

        path(c, x, y, size, top, 5, t / 2, color, alpha);
        path(c, x, y + S(4), size, low, 3, t / 2, color, alpha);
        path(c, x, y + S(14), size, low, 3, t / 2, color, alpha);
        break;
    }
    default:
        pw_icon_draw(c, icon, cx, cy, size, color, alpha);
        break;
    }
}

int hm_icon_of_tab(int tab)
{
    static const int k[HM_TABS] = {HM_ICON_GAMES, HM_ICON_MUSIC, HM_ICON_PHOTOS,
                                   PW_ICON_WATCH, PW_ICON_GEAR,  HM_ICON_SYSTEM};

    return tab >= 0 && tab < HM_TABS ? k[tab] : PW_ICON_WATCH;
}
