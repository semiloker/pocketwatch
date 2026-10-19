// SPDX-License-Identifier: GPL-3.0-or-later
#include "hm_icons.h"
#include "home_view.h"
#include "ui/shapes.h"

/* Scale a design coordinate (a 64-unit box centred on 0) to PW_FX around the centre. */
#define S(v) ((v) * size * 16 / 64)

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
