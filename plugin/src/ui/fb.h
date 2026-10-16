// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_FB_H
#define PW_FB_H

#include "ui/draw.h"

/*
 * Our own screen over the frozen game (docs/research/m3-menu-e3.md): 8888 buffers in P11 shown
 * with sceDisplaySetFrameBufferInternal at priority 0; the game's own framebuffer is never
 * written. P11 is the firmware's ME save area during a suspend, so hide the screen before one
 * and recapture after it.
 */

int pw_fb_open(void);   /* allocate, capture the game's frame as a dimmed background */
void pw_fb_close(void); /* give the display back to the game and free everything */

/* The canvas to draw the next frame on; it starts as a copy of the background. */
pw_canvas *pw_fb_begin(void);
void pw_fb_present(void); /* show what was drawn since pw_fb_begin (at the next vblank) */

void pw_fb_hide(void);      /* give the display back, keep the buffers (before a suspend) */
void pw_fb_recapture(void); /* the game's frame again as the background (after a suspend) */

#endif
