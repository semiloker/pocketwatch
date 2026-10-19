// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_ICONS_H
#define HM_ICONS_H

#include "ui/draw.h"
#include "ui/icons.h"

/*
 * Home's own drawn icons, in the white-outline style of the plugin's (ui/icons.c): the sections
 * when the system's pictures are missing (and always in Modern and Minimal), the music's play and
 * pause. Lower numbers are the plugin's PW_ICON_*. Portable.
 */

enum {
    HM_ICON_GAMES = 32, /* a game pad */
    HM_ICON_MUSIC,      /* two beamed notes */
    HM_ICON_PHOTOS,     /* a framed landscape */
    HM_ICON_SYSTEM,     /* "i" in a ring */
    HM_ICON_PLAY,
    HM_ICON_PAUSE,
};

void hm_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha);

/* The icon of a section (HM_TAB_*). */
int hm_icon_of_tab(int tab);

#endif
