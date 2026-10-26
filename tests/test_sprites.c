// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "pwtest.h"
#include "sprites.h"
#include "ui/draw.h"
#include "ui/shapes.h"

#define W 160
#define H 96

/* What the GE does with a sprite of the stamp: nearest texel, the stamp's alpha times alpha. */
static void blit(pw_canvas *c, const unsigned int *stamp, int side, hm_sprite s, unsigned int rgb,
                 int alpha)
{
    for (int y = (int)s.y0; y < (int)s.y1; y++)
        for (int x = (int)s.x0; x < (int)s.x1; x++) {
            float fu = s.u0 + (x + 0.5f - s.x0) * (s.u1 - s.u0) / (s.x1 - s.x0),
                  fv = s.v0 + (y + 0.5f - s.y0) * (s.v1 - s.v0) / (s.y1 - s.y0);
            int u = (int)fu, v = (int)fv;
            unsigned int cov;

            if (u < 0 || v < 0 || u >= side || v >= side)
                continue;
            cov = stamp[v * side + u] & 0xFF;
            pw_draw_blend(c, x, y, 1, 1, rgb, (int)(cov * (unsigned int)alpha / 255));
        }
}

static int max_diff(const unsigned int *a, const unsigned int *b, int n)
{
    int worst = 0;

    for (int i = 0; i < n; i++)
        for (int k = 0; k < 3; k++) {
            int d = (int)((a[i] >> (8 * k)) & 0xFF) - (int)((b[i] >> (8 * k)) & 0xFF);

            if (d < 0)
                d = -d;
            if (d > worst)
                worst = d;
        }
    return worst;
}

/* The GE backend's rounded rectangle (a stamp cut into nine) against the CPU renderer's. */
static void compare(int x, int y, int w, int h, int r, int line)
{
    static unsigned int direct[W * H], cut[W * H], stamp[64 * 64];
    pw_canvas d = {direct, W, H, W}, c = {cut, W, H, W};
    int rr = hm_rrect_radius(w, h, r), side = HM_STAMP_SIDE(rr), n;
    pw_canvas st = {stamp, side, side, side};
    hm_sprite parts[9];

    for (int i = 0; i < W * H; i++)
        direct[i] = cut[i] = PW_RGB(30, 60, 90);
    pw_shape_rrect(&d, PW_FX(x), PW_FX(y), PW_FX(w), PW_FX(h), PW_FX(rr), line,
                   PW_RGB(250, 240, 200), 200);
    memset(stamp, 0, sizeof(stamp));
    pw_shape_rrect(&st, 0, 0, PW_FX(side), PW_FX(side), PW_FX(rr), line, PW_RGB(255, 255, 255),
                   256);
    n = hm_rrect_sprites(x, y, w, h, rr, line == 0, parts);
    for (int i = 0; i < n; i++) {
        hm_sprite s = parts[i];

        if (hm_sprite_clip(&s, W, H))
            blit(&c, stamp, side, s, PW_RGB(250, 240, 200), 200);
    }
    CHECK(max_diff(direct, cut, W * H) <= 3);
}

PW_TEST(sprites_rounded_rect_from_a_stamp_matches_the_cpu)
{
    compare(10, 8, 120, 30, 15, 0);           /* the Play pill */
    compare(6, 10, 140, 24, 12, 0);           /* the tab pill */
    compare(4, 4, 152, 88, 9, PW_FX(2));      /* the focus ring */
    compare(20, 30, 18, 10, 2, PW_FX(1) + 4); /* the battery */
    compare(-20, 40, 100, 40, 14, 0);         /* half off the screen */
}

PW_TEST(sprites_clip_moves_the_texture_with_the_edge)
{
    hm_sprite s = {-10, 5, 90, 25, 0, 0, 200, 40}; /* a 2x scale: 10 px are 20 texels */

    CHECK_EQ(hm_sprite_clip(&s, 480, 272), 1);
    CHECK(s.x0 == 0 && s.u0 == 20);
    s = (hm_sprite){470, 260, 490, 280, 0, 0, 20, 20};
    CHECK_EQ(hm_sprite_clip(&s, 480, 272), 1);
    CHECK(s.x1 == 480 && s.u1 == 10 && s.y1 == 272 && s.v1 == 12);
    s = (hm_sprite){480, 0, 500, 10, 0, 0, 1, 1};
    CHECK_EQ(hm_sprite_clip(&s, 480, 272), 0);
    CHECK_EQ(hm_rrect_radius(18, 10, 12), 5);
}

/* The clip operation (a list cut above the hints, under the header): the rows of the GE's sprites
   go with their texture, as at the screen's edges. */
PW_TEST(sprites_clip_to_rows_cuts_top_and_bottom)
{
    hm_sprite s = {10, 40, 110, 80, 0, 0, 50, 20}; /* a half scale: 2 px a texel */

    CHECK_EQ(hm_sprite_clip_rect(&s, 0, 50, 480, 70), 1);
    CHECK(s.y0 == 50 && s.v0 == 5 && s.y1 == 70 && s.v1 == 15);
    CHECK(s.x0 == 10 && s.u0 == 0 && s.x1 == 110 && s.u1 == 50);
    s = (hm_sprite){10, 251, 110, 270, 0, 0, 1, 1}; /* wholly under the hints: nothing */
    CHECK_EQ(hm_sprite_clip_rect(&s, 0, 44, 480, 251), 0);
    s = (hm_sprite){10, 250, 110, 270, 0, 0, 1, 1}; /* its first row is still in */
    CHECK_EQ(hm_sprite_clip_rect(&s, 0, 44, 480, 251), 1);
    CHECK(s.y1 == 251);
    s = (hm_sprite){10, 20, 110, 44, 0, 0, 1, 1}; /* ends where the rows start: nothing */
    CHECK_EQ(hm_sprite_clip_rect(&s, 0, 44, 480, 251), 0);
}
