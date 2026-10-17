// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_BACKDROP_H
#define PW_BACKDROP_H

#include "gfx/image.h"
#include "ui/draw.h"

/*
 * What the menu is drawn on, per style (ui/ui.h PW_STYLE_*): the XMB's month sky (its wave moves
 * every frame), the frozen game softened and darkened (modern), or the game simply darkened
 * (minimal). Made once when the menu opens or the style changes. Portable.
 */
void pw_backdrop_make(pw_canvas *dst, const pw_canvas *game, const pw_image *sky, int style);

/* 1 when the backdrop moves (the XMB's wave): pw_backdrop_frame must run every frame. */
int pw_backdrop_moves(int style);
void pw_backdrop_frame(pw_canvas *c, unsigned int t_ms, int style);

/* The moving part over rows [y_from, y_to) only. */
void pw_backdrop_frame_rows(pw_canvas *c, unsigned int t_ms, int style, int y_from, int y_to);

#endif
