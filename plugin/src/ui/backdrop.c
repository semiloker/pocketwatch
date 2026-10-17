// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/backdrop.h"
#include "ui/ui.h"
#include "ui/xmbbg.h"

#define BLUR_RADIUS 7
#define MODERN_DIM  120 /* of 256: the game stays recognisable behind the cards */
#define MINIMAL_DIM 60
#define SHADE_TOP   140 /* modern: darker towards the bottom, where the controls are */

static void copy(pw_canvas *dst, const pw_canvas *src)
{
    for (int y = 0; y < dst->h && y < src->h; y++)
        memcpy(dst->px + y * dst->stride, src->px + y * src->stride,
               (unsigned int)(dst->w < src->w ? dst->w : src->w) * 4);
}

void pw_backdrop_make(pw_canvas *dst, const pw_canvas *game, const pw_image *sky, int style)
{
    if (style == PW_STYLE_MODERN && game) {
        copy(dst, game);
        pw_draw_blur(dst, BLUR_RADIUS);
        pw_draw_darken(dst, MODERN_DIM);
        for (int y = SHADE_TOP; y < dst->h; y++)
            pw_draw_blend(dst, 0, y, dst->w, 1, PW_RGB(0, 0, 0),
                          (y - SHADE_TOP) * 150 / (dst->h - SHADE_TOP));
    } else if (style == PW_STYLE_MINIMAL && game) {
        copy(dst, game);
        pw_draw_darken(dst, MINIMAL_DIM);
    } else {
        pw_xmbbg_sky(dst, sky);
    }
}

int pw_backdrop_moves(int style)
{
    return style != PW_STYLE_MODERN && style != PW_STYLE_MINIMAL;
}

void pw_backdrop_frame(pw_canvas *c, unsigned int t_ms, int style)
{
    if (pw_backdrop_moves(style))
        pw_xmbbg_wave(c, t_ms);
}

void pw_backdrop_frame_rows(pw_canvas *c, unsigned int t_ms, int style, int y_from, int y_to)
{
    if (pw_backdrop_moves(style))
        pw_xmbbg_wave_rows(c, t_ms, y_from, y_to);
}
