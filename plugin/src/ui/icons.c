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

void pw_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha)
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
    case PW_ICON_CARD:
        pw_shape_rrect(c, x + S(-16), y + S(-26), S(32), S(52), S(5), t, color, alpha);
        pw_shape_capsule(c, x + S(-6), y + S(-16), x + S(6), y + S(-16), t / 2, color, alpha);
        break;
    default:
        break;
    }
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
