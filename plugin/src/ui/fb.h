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

#define PW_FB_W      480
#define PW_FB_H      272
#define PW_FB_STRIDE 512

/* Allocate, capture the game's frame, make the backdrop for style (ui/ui.h PW_STYLE_*; sky is the
   XMB style's month picture). */
int pw_fb_open(const pw_image *sky, int style);
void pw_fb_set_style(int style); /* a new look from the Settings screen: the backdrop again */
void pw_fb_close(void);          /* give the display back to the game and free everything */

/*
 * The canvas to draw the next frame on: the sky with the wave at t_ms, mixed with the game's
 * frame (show 0 = the game only, 256 = the XMB background only).
 */
pw_canvas *pw_fb_begin(int show, unsigned int t_ms);
void pw_fb_present(void); /* show what was drawn since pw_fb_begin (at the next vblank) */

void pw_fb_hide(void);      /* give the display back, keep the buffers (before a suspend) */
void pw_fb_recapture(void); /* the game's frame and the sky again (after a suspend) */

/* Debug: the page on the screen now as shot-NNN.png next to the log (SELECT in a menu). */
int pw_fb_screenshot(void);

/* The game's frame as captured (PW_FB_W x PW_FB_H, PW_FB_STRIDE pixels per row), or NULL. */
const unsigned int *pw_fb_game(void);

#endif
