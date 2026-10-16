// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_UI_FB_H
#define PW_UI_FB_H

#include "gfx/image.h"
#include "ui/draw.h"

/*
 * Our own screen over the frozen game (docs/ui/rendering.md): 8888 pages in P11 shown with
 * sceDisplaySetFrameBufferInternal at priority 0; the game's own framebuffer is only read. Like the
 * system's HOME screen, the menu shows the XMB background (the month's sky and the wave), cross-
 * faded from the game's last frame. P11 is the firmware's ME save area during a suspend: hide the
 * screen before one and recapture after it.
 */

#define PW_FB_W 480
#define PW_FB_H 272

int pw_fb_open(const pw_image *sky); /* allocate, capture the game's frame, paint the sky */
void pw_fb_close(void);              /* give the display back to the game and free everything */

/*
 * The canvas to draw the next frame on: the sky with the wave at t_ms, mixed with the game's
 * frame (show 0 = the game only, 256 = the XMB background only).
 */
pw_canvas *pw_fb_begin(int show, unsigned int t_ms);
void pw_fb_present(void); /* show what was drawn since pw_fb_begin (at the next vblank) */

void pw_fb_hide(void);      /* give the display back, keep the buffers (before a suspend) */
void pw_fb_recapture(void); /* the game's frame and the sky again (after a suspend) */

#endif
