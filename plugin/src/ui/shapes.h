// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SHAPES_H
#define PW_SHAPES_H

#include "ui/draw.h"

/*
 * Anti-aliased shapes for Pocketwatch's own icons (drawn in the XMB's white-outline style until
 * the system's RCO icons are read, docs/ui/rendering.md). Coordinates and sizes are in 1/16 px
 * (PW_FX), coverage is computed from the exact distance to the shape. Portable.
 */

#define PW_FX(px) ((px) * 16)

void pw_shape_capsule(pw_canvas *c, int x0, int y0, int x1, int y1, int radius, unsigned int color,
                      int alpha);
void pw_shape_disc(pw_canvas *c, int cx, int cy, int radius, unsigned int color, int alpha);
/* Ring of the given line thickness; gap > 0 leaves an opening of that half-width at the top. */
void pw_shape_ring(pw_canvas *c, int cx, int cy, int radius, int thick, int gap, unsigned int color,
                   int alpha);
/* Rounded rectangle: thick 0 = filled, else an outline of that thickness. */
void pw_shape_rrect(pw_canvas *c, int x, int y, int w, int h, int radius, int thick,
                    unsigned int color, int alpha);
void pw_shape_triangle(pw_canvas *c, int x0, int y0, int x1, int y1, int x2, int y2,
                       unsigned int color, int alpha);

#endif
