// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_XMBBG_H
#define PW_XMBBG_H

#include "gfx/image.h"
#include "ui/draw.h"

/*
 * The XMB's background as the firmware draws it on the PSP: the month's sky picture
 * (flash0:/vsh/resource/01-12_03g.bmp or 01-12.bmp, twelve 60x34 BMPs) stretched over the screen,
 * with the translucent white wave across it (docs/ui/README.md, docs/research/m4-native-look.md).
 * The wave here is drawn procedurally (the firmware renders a 3D model with the GE). Portable.
 */

/* Fill c with the stretched sky (bilinear). Without a sky picture, a dark blue-grey gradient. */
void pw_xmbbg_sky(pw_canvas *c, const pw_image *sky);

/* Lay the wave over rows y0..y1 of c (already holding the sky), at time t_ms. */
void pw_xmbbg_wave(pw_canvas *c, unsigned int t_ms);

/* The same over rows [y_from, y_to) only (the menu updates half the wave per frame). */
void pw_xmbbg_wave_rows(pw_canvas *c, unsigned int t_ms, int y_from, int y_to);

/* Rows the wave can touch (for partial redraws). */
#define PW_XMBBG_WAVE_TOP    96
#define PW_XMBBG_MAX_W       512 /* widest canvas the wave handles */
#define PW_XMBBG_WAVE_BOTTOM 272

#endif
