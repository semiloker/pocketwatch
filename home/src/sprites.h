// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_SPRITES_H
#define HM_SPRITES_H

/*
 * The geometry of the GE backend (gfx_gu.c), portable so the host can check it against the CPU
 * renderer: textured rectangles ("sprites") clipped to the screen, and a rounded rectangle as
 * nine sprites cut from a stamp - the same rounded rectangle drawn once, (2r + 1) px square, by
 * the CPU renderer: its four quarters are the corners, its middle row and column stretched are
 * the straight edges and the inside.
 */

typedef struct {
    float x0, y0, x1, y1; /* on the screen */
    float u0, v0, u1, v1; /* in the texture, texels */
} hm_sprite;

/* Cut s to the w x h screen, moving its texture coordinates with it. 0 when nothing is left. */
int hm_sprite_clip(hm_sprite *s, int w, int h);
/* The same to the rectangle x0, y0 - x1, y1 (the end excluded). */
int hm_sprite_clip_rect(hm_sprite *s, int x0, int y0, int x1, int y1);

/* The stamp's side for radius r. */
#define HM_STAMP_SIDE(r) (2 * (r) + 1)

/* The radius a w x h rounded rectangle can have (r cut to half the shorter side). */
int hm_rrect_radius(int w, int h, int r);

/* The sprites of a w x h rounded rectangle at x, y from the stamp of radius r (already cut by
   hm_rrect_radius, r >= 1): up to nine; the inside only when inside is set (a filled shape).
   Returns how many. */
int hm_rrect_sprites(int x, int y, int w, int h, int r, int inside, hm_sprite *out);

#endif
