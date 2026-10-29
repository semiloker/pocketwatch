// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_ICONS_H
#define HM_ICONS_H

#include "ui/draw.h"
#include "ui/icons.h"

/*
 * Home's own drawn icons, in the white-outline style of the plugin's (ui/icons.c): the sections
 * when the system's pictures are missing (and always in Modern and Minimal), the settings'
 * categories, the music's controls, the photo editor's tools. Lower numbers are the plugin's
 * PW_ICON_*. Portable.
 */

enum {
    HM_ICON_GAMES = 32, /* a game pad */
    HM_ICON_MUSIC,      /* two beamed notes */
    HM_ICON_PHOTOS,     /* a framed landscape */
    HM_ICON_SYSTEM,     /* "i" in a ring */
    HM_ICON_PLAY,
    HM_ICON_PAUSE,
    HM_ICON_PALETTE, /* appearance */
    HM_ICON_SPEAKER, /* sound */
    HM_ICON_CLOCK,   /* date and time */
    HM_ICON_GLOBE,   /* network, the browser */
    HM_ICON_BATTERY, /* an empty battery outline */
    HM_ICON_USB,     /* the USB trident */
    HM_ICON_PREV,    /* bar and triangle */
    HM_ICON_NEXT,
    HM_ICON_SHUFFLE, /* two crossing arrows */
    HM_ICON_REPEAT,  /* a loop with arrows */
    HM_ICON_REPEAT1, /* the loop with a 1 */
    HM_ICON_TRASH,
    HM_ICON_FOLDER,
    HM_ICON_SLIDERS, /* edit */
    HM_ICON_ROTATE,
    HM_ICON_SUN,      /* brightness */
    HM_ICON_CONTRAST, /* a half-filled circle */
    HM_ICON_DROP,     /* saturation */
    HM_ICON_FILTER,   /* three rings */
    HM_ICON_CHECK,
    HM_ICON_EXIT,     /* a door and an arrow out */
    HM_ICON_SLEEP,    /* Z */
    HM_ICON_LANGUAGE, /* a speech bubble with an A */
    HM_ICON_USER,     /* the nickname */
    HM_ICON_VIGNETTE, /* a thick ring */
    HM_ICON_CHIP,     /* the CPU */
    HM_ICON_STAR,
    HM_ICON_FLIP,   /* two triangles and a line between */
    HM_ICON_HOME,   /* a house */
    HM_ICON_LAYERS, /* themes */
    HM_ICON_LR,     /* the d-pad's left and right: two triangles apart */
    HM_ICON_UD,     /* its up and down */
    HM_ICON_UP,     /* the d-pad's up alone: one triangle */
    HM_ICON_DOWN,
    HM_ICON_EXPOSURE,   /* the photo editor's tools (ADR-060): a ring, plus and minus */
    HM_ICON_HIGHLIGHTS, /* a ring with its upper half filled */
    HM_ICON_SHADOWS,    /* a ring with its lower half filled */
    HM_ICON_VIBRANCE,   /* a drop and a spark */
    HM_ICON_TINT,       /* two overlapping discs */
    HM_ICON_SHARPEN,    /* a diamond */
    HM_ICON_WARMTH,     /* a thermometer */
    HM_ICON_CROP,       /* two crop corners */
    HM_ICON_LAST
};

void hm_icon_draw(pw_canvas *c, int icon, int cx, int cy, int size, unsigned int color, int alpha);

/* The icon of a section (HM_TAB_*). */
int hm_icon_of_tab(int tab);

#endif
