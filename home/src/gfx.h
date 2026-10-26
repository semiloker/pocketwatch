// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_GFX_H
#define HM_GFX_H

#include "gfx/text.h"

/*
 * What Pocketwatch Home's screens are drawn with (ADR-028): a handful of operations, two
 * implementations. On the PSP the GE draws them (gfx_gu.c: textures, alpha blending and scaling in
 * hardware, the CPU only builds a display list); on the host the CPU renderer does
 * (gfx_cpu.c, for the previews). Both draw the same pixels for text, corners and icons: the GE
 * backend rasterises those once with the CPU renderer and keeps them as textures.
 * Colours are PW_RGB (R in the low byte), alpha 0-256, coordinates in screen pixels.
 */

typedef struct hm_gfx hm_gfx;

struct hm_gfx {
    /* A rectangle of colour. */
    void (*fill)(hm_gfx *g, int x, int y, int w, int h, unsigned int color, int alpha);
    /* A rounded rectangle; line 0 fills it, else its outline is line/16 px thick (PW_FX). */
    void (*rrect)(hm_gfx *g, int x, int y, int w, int h, int r, int line, unsigned int color,
                  int alpha);
    /* An image (w x h, rows of stride pixels, alpha on top) scaled into dw x dh at x, y. */
    void (*image)(hm_gfx *g, const unsigned int *px, int w, int h, int stride, int x, int y, int dw,
                  int dh, int alpha);
    /* The screen's background: 480 x 272 images of stride 512, prev fading into bg (mix 0-256).
       bg NULL: black. */
    void (*backdrop)(hm_gfx *g, const unsigned int *bg, const unsigned int *prev, int mix);
    /* Text at the baseline y; max_w > 0 cuts it with "..." to that width. */
    void (*text)(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w, unsigned int color,
                 int alpha);
    /* The XMB's wave (ui/xmbbg.c's ribbon) over what is drawn, at time t_ms, alpha 0-256. */
    void (*wave)(hm_gfx *g, unsigned int t_ms, int alpha);
    /* A drawn icon (hm_icons.h HM_ICON_*, ui/icons.h PW_ICON_*) or a button glyph (PW_BTN_*),
       centred. */
    void (*icon)(hm_gfx *g, int icon, int cx, int cy, int size, unsigned int color, int alpha);
    void (*button)(hm_gfx *g, int button, int cx, int cy, int size, unsigned int color, int alpha);
    /* Prepare the text as text() would draw it, for a later frame: within a time budget; 1 when
       it is ready (the CPU backend: always). */
    int (*warm)(hm_gfx *g, pw_font *f, const char *s, int max_w);
    /* What follows draws only on the rows y0 to y1 - 1 (a list cut at its edges, not under the
       hints or the header); clip(g, 0, HM_H) draws everywhere again. Not for backdrop / wave. */
    void (*clip)(hm_gfx *g, int y0, int y1);
};

#endif
