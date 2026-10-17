// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ICONS_H
#define PW_ICONS_H

#include "ui/draw.h"

/*
 * Pocketwatch's own icons in the XMB's white-outline style, drawn with anti-aliased shapes. They
 * stand in until the system's icons are read from flash0 (docs/research/m4-native-icons.md).
 * Portable.
 */

enum {
    PW_ICON_WATCH,  /* the hub's category icon */
    PW_ICON_RESUME, /* play triangle in a frame */
    PW_ICON_SAVE,   /* tray, arrow down */
    PW_ICON_LOAD,   /* tray, arrow up */
    PW_ICON_QUIT,   /* power symbol */
    PW_ICON_CARD,   /* Memory Stick (debug test) */
    PW_ICON_GEAR,   /* Settings */
    PW_ICON_DISC,   /* a filled disc of diameter size (round buttons) */
};

/* Draw icon centred at (cx, cy), fitting a size x size box, coverage scaled by alpha (0-256). */
void pw_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha);

/* Memory for rasterised icons (NULL: draw from the shapes every time). The contents are dropped
   whenever this is called again. */
void pw_icon_cache(void *mem, unsigned int cap);

/* The PSP's button symbols for hints: a ring with the symbol inside. */
enum { PW_BTN_CROSS, PW_BTN_CIRCLE, PW_BTN_TRIANGLE, PW_BTN_SQUARE };
void pw_button_draw(pw_canvas *c, int button, int cx, int cy, int size, unsigned int color,
                    int alpha);

#endif
