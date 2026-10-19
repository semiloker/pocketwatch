// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_VIEW_INTERNAL_H
#define HM_VIEW_INTERNAL_H

#include "hm_icons.h"
#include "home_view.h"
#include "ui/shapes.h"

/* What the three styles of home_view.c share (view_classic.c, view_modern.c, view_minimal.c). */

#define C_TEXT  PW_RGB(255, 255, 255)
#define C_SUB   PW_RGB(176, 182, 196)
#define C_DIM   PW_RGB(222, 225, 232)
#define C_CARD  PW_RGB(28, 30, 40)
#define C_KNOB  PW_RGB(70, 75, 92)
#define C_ON    PW_RGB(0, 114, 206)
#define C_INK   PW_RGB(16, 17, 22)
#define C_INK2  PW_RGB(84, 88, 100) /* a value on a white pill */
#define C_BLACK PW_RGB(0, 0, 0)

static inline int hm_clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static inline int hm_lerp(int a, int b, int t) /* t 0-256 */
{
    return a + (b - a) * t / 256;
}

/* 256 at p = 0 (the cursor), 0 one step (256) or more away. */
static inline int hm_near(int p)
{
    return p >= 256 || p <= -256 ? 0 : 256 - (p < 0 ? -p : p);
}

void hm_text_right(hm_gfx *g, pw_font *f, int x1, int y, const char *s, unsigned int color,
                   int alpha);
void hm_text_center(hm_gfx *g, pw_font *f, int cx, int y, const char *s, int max_w,
                    unsigned int color, int alpha);
/* A small arrow (dir 1 right, -1 left) centred at x, y. */
void hm_chevron(hm_gfx *g, int x, int y, int dir, unsigned int color, int alpha);
/* An on/off switch, its left edge at x, centred on y. */
void hm_switch(hm_gfx *g, int x, int y, int on, int alpha);
/* A game's or a photo's picture, or a dark card with a disc. */
void hm_tile_draw(hm_gfx *g, const unsigned int *icon, int x, int y, int w, int h, int alpha);
/* The system's picture (art->btn) of a button or the drawn glyph, centred. */
void hm_button_pic(hm_gfx *g, const hm_ui *ui, int which, int cx, int cy, int size,
                   unsigned int color, int alpha);
/* An image of the system's art (its rows padded to 16 pixels), centred, at its size. */
void hm_art_draw(hm_gfx *g, const pw_image *img, int cx, int cy, int alpha);
/* The Enter / Back / Options hints from the right edge x1 leftwards; returns where they end. */
int hm_hints_right(hm_gfx *g, const hm_ui *ui, const hm_view *v, int x1, int y, unsigned int color,
                   int alpha);

void hm_draw_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_draw_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_draw_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v);

#endif
