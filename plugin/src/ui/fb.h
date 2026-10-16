// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_UI_FB_H
#define PW_UI_FB_H

#include "ui/draw.h"

/*
 * Our own screen over the frozen game (docs/ui/rendering.md): 8888 pages in P11 shown with
 * sceDisplaySetFrameBufferInternal at priority 0; the game's own framebuffer is only read. P11 is
 * the firmware's ME save area during a suspend, so hide the screen before one and recapture after.
 */

#define PW_FB_W 480
#define PW_FB_H 272

int pw_fb_open(void);   /* allocate, capture the game's frame (bright and dimmed) */
void pw_fb_close(void); /* give the display back to the game and free everything */

/*
 * The canvas to draw the next frame on. It starts as the game's frame mixed between bright
 * (dim 0) and the menu's dimmed background (dim 256): fades go through this.
 */
pw_canvas *pw_fb_begin(int dim);
void pw_fb_present(void); /* show what was drawn since pw_fb_begin (at the next vblank) */

void pw_fb_hide(void);      /* give the display back, keep the buffers (before a suspend) */
void pw_fb_recapture(void); /* the game's frame again as the background (after a suspend) */

#endif
