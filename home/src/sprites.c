// SPDX-License-Identifier: GPL-3.0-or-later
#include "sprites.h"

int hm_sprite_clip_rect(hm_sprite *s, int x0, int y0, int x1, int y1)
{
    float du = s->x1 > s->x0 ? (s->u1 - s->u0) / (s->x1 - s->x0) : 0,
          dv = s->y1 > s->y0 ? (s->v1 - s->v0) / (s->y1 - s->y0) : 0;

    if (s->x1 <= s->x0 || s->y1 <= s->y0 || s->x1 <= x0 || s->y1 <= y0 || s->x0 >= x1 ||
        s->y0 >= y1)
        return 0;
    if (s->x0 < x0) {
        s->u0 += (x0 - s->x0) * du;
        s->x0 = (float)x0;
    }
    if (s->y0 < y0) {
        s->v0 += (y0 - s->y0) * dv;
        s->y0 = (float)y0;
    }
    if (s->x1 > x1) {
        s->u1 -= (s->x1 - x1) * du;
        s->x1 = (float)x1;
    }
    if (s->y1 > y1) {
        s->v1 -= (s->y1 - y1) * dv;
        s->y1 = (float)y1;
    }
    return 1;
}

int hm_sprite_clip(hm_sprite *s, int w, int h)
{
    return hm_sprite_clip_rect(s, 0, 0, w, h);
}

int hm_rrect_radius(int w, int h, int r)
{
    int m = (w < h ? w : h) / 2;

    return r < m ? r : m;
}

static void put(hm_sprite *s, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
                float v1)
{
    s->x0 = x0;
    s->y0 = y0;
    s->x1 = x1;
    s->y1 = y1;
    s->u0 = u0;
    s->v0 = v0;
    s->u1 = u1;
    s->v1 = v1;
}

int hm_rrect_sprites(int x, int y, int w, int h, int r, int inside, hm_sprite *out)
{
    /* The stamp: columns 0..r-1 the left corners, r the straight middle, r+1..2r the right. */
    float R = (float)r, X = (float)x, Y = (float)y, X1 = (float)(x + w), Y1 = (float)(y + h),
          S = (float)HM_STAMP_SIDE(r);
    int n = 0;

    put(&out[n++], X, Y, X + R, Y + R, 0, 0, R, R);             /* top left */
    put(&out[n++], X1 - R, Y, X1, Y + R, R + 1, 0, S, R);       /* top right */
    put(&out[n++], X, Y1 - R, X + R, Y1, 0, R + 1, R, S);       /* bottom left */
    put(&out[n++], X1 - R, Y1 - R, X1, Y1, R + 1, R + 1, S, S); /* bottom right */
    if (w > 2 * r) {
        put(&out[n++], X + R, Y, X1 - R, Y + R, R, 0, R + 1, R);       /* top edge */
        put(&out[n++], X + R, Y1 - R, X1 - R, Y1, R, R + 1, R + 1, S); /* bottom edge */
    }
    if (h > 2 * r) {
        put(&out[n++], X, Y + R, X + R, Y1 - R, 0, R, R, R + 1);       /* left edge */
        put(&out[n++], X1 - R, Y + R, X1, Y1 - R, R + 1, R, S, R + 1); /* right edge */
        if (inside && w > 2 * r)
            put(&out[n++], X + R, Y + R, X1 - R, Y1 - R, R, R, R + 1, R + 1);
    }
    return n;
}
