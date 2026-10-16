// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ASSETS_H
#define PW_ASSETS_H

#include "gfx/image.h"
#include "gfx/text.h"

/*
 * The system's own assets, read from the user's PSP at plugin start (never shipped, ADR-013) and
 * kept for the session in one block at the top of P9 (survives suspends; the state's block keeps
 * the bottom of P9):
 *   - the Latin UI font flash0:/font/ltn0.pgf with its glyph caches;
 *   - the XMB's icons from flash0:/vsh/resource/topmenu_icon.rco (RLZ) and the button pictures
 *     from system_plugin_fg.rco;
 *   - the month skies flash0:/vsh/resource/01-12_03g.bmp (or 01-12.bmp).
 * Anything missing falls back (8x8 font, drawn icons, a plain gradient) and is logged once.
 */

enum {
    PW_FONT_TITLE, /* item titles, dialog text: ltn0 at ~0.8 (the XMB's ~14 px) */
    PW_FONT_BODY,  /* descriptions, hints, clock: ltn0 at ~0.63 (~11 px) */
    PW_FONT_COUNT
};

enum {
    PW_ART_CAT_GAME, /* the Game category (controller) */
    PW_ART_UMD,      /* UMD disc: Resume Game */
    PW_ART_UMD_GLOW,
    PW_ART_FOLDER, /* saved data folder: Save State */
    PW_ART_FOLDER_GLOW,
    PW_ART_UPDATE, /* disc with arrows: Load State */
    PW_ART_UPDATE_GLOW,
    PW_ART_MS, /* Memory Stick: card test, storage */
    PW_ART_MS_GLOW,
    PW_ART_SETTINGS, /* wrench + PSP: Settings */
    PW_ART_SETTINGS_GLOW,
    PW_ART_BTN_CROSS,
    PW_ART_BTN_CIRCLE,
    PW_ART_BTN_TRIANGLE,
    PW_ART_BTN_SQUARE,
    PW_ART_COUNT
};

int pw_assets_init(void); /* plugin start, from a thread that may do file I/O */
void pw_assets_shutdown(void);
pw_font *pw_assets_font(int which);
int pw_assets_native(void);            /* 1 when the system font was loaded */
const pw_image *pw_assets_art(int id); /* NULL when the system did not provide it */

/* The sky picture 0..11 (60x34), decoded on demand; NULL without the BMP. */
const pw_image *pw_assets_sky(int index);

#endif
