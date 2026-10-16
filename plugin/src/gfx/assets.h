// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ASSETS_H
#define PW_ASSETS_H

#include "gfx/text.h"

/*
 * The system's own assets, read from the user's PSP at plugin start (never shipped, ADR-013):
 * today the Latin UI font flash0:/font/ltn0.pgf, kept with its glyph caches in a small block at
 * the top of P9 (survives suspends; the state's block keeps the bottom of P9). Without the file
 * the fonts fall back to the built-in 8x8 font.
 */

enum {
    PW_FONT_TITLE, /* item titles, dialog text: ltn0 at ~0.8 (the XMB's ~14 px) */
    PW_FONT_BODY,  /* descriptions, hints: ltn0 at ~0.63 (~11 px) */
    PW_FONT_COUNT
};

int pw_assets_init(void); /* plugin start, from a thread that may do file I/O */
void pw_assets_shutdown(void);
pw_font *pw_assets_font(int which);
int pw_assets_native(void); /* 1 when the system font was loaded */

#endif
