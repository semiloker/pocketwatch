// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_XMBART_H
#define HM_XMBART_H

#include "home_view.h"

/*
 * The system's own pictures for Home's Classic style (ADR-033): the XMB's category icons from
 * flash0:/vsh/resource/topmenu_icon.rco, the button pictures and the battery from
 * system_plugin_fg.rco, the month skies from 01-12_03g.bmp - read from the user's PSP at start,
 * never shipped (ADR-013), decoded with the plugin's readers (gfx/rco.c, gfx/rlz.c,
 * gfx/image.c). Portable: the caller reads the files (the host previews too).
 */

/* Whole file into malloc'd memory (*len its size), or NULL. */
typedef unsigned char *(*hm_art_read)(const char *path, unsigned int *len);

/* The icons and buttons into a (missing ones stay px NULL); returns how many were found. The
   pixels are 16-byte aligned, rows of a multiple of 16 (the GE's textures). */
int hm_art_load(hm_art *a, hm_art_read read, const char *icons_rco, const char *fg_rco);

/* Sky index (0-11) of the file sky_bmp into sky (60 x 34, malloc'd); 0, or -1 no file, -2 no
   memory, -11..-13 not a sky the decoder takes (gfx/image.h pw_bmp_decode - 10). */
int hm_art_sky(pw_image *sky, hm_art_read read, const char *sky_bmp, int index);

#endif
