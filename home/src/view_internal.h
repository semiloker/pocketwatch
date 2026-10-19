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

/* The colours of a theme (Modern, Minimal): what text, cards and the chosen row are painted
   with. */
typedef struct {
    unsigned int text, sub, dim; /* a title, a value, a row not chosen */
    unsigned int card, knob;     /* cards, the off part of switches and bars */
    unsigned int sel, ink, ink2; /* the chosen row's pill, its text and value */
    unsigned int head;           /* a group's name */
    int card_a, sel_a;           /* their opacity (0-256) */
    int shade;                   /* how much the background is darkened under the content */
} hm_palette;

void hm_palette_of(const hm_ui *ui, hm_palette *p);

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

/* Two colours mixed (t 0-256 from c0 to c1). */
unsigned int hm_mix(unsigned int c0, unsigned int c1, int t);

/* Drawing moved by dx and faded to alpha (0-256): a page sliding in or out. */
typedef struct {
    hm_gfx ops;
    hm_gfx *in;
    int dx, alpha;
} hm_shift;

hm_gfx *hm_shift_init(hm_shift *s, hm_gfx *in, int dx, int alpha);

/* Where page d is while the stack moves: its offset t (1/256 of a page: < 0 to the left) and
   whether it is drawn at all. */
static inline int hm_page_t(const hm_view *v, int d)
{
    return d * 256 - v->depth_pos;
}

void hm_text_right(hm_gfx *g, pw_font *f, int x1, int y, const char *s, unsigned int color,
                   int alpha);
void hm_text_center(hm_gfx *g, pw_font *f, int cx, int y, const char *s, int max_w,
                    unsigned int color, int alpha);
/* A small arrow (dir 1 right, -1 left) centred at x, y. */
void hm_chevron(hm_gfx *g, int x, int y, int dir, unsigned int color, int alpha);
/* An on/off switch, its left edge at x, centred on y. */
void hm_switch(hm_gfx *g, int x, int y, int on, unsigned int accent, unsigned int knob, int alpha);
/* A game's or a photo's picture, or a dark card with a disc. */
void hm_tile_draw(hm_gfx *g, const unsigned int *icon, int x, int y, int w, int h, int alpha);
/* A track's cover, or a dark card with a note. */
void hm_cover_draw(hm_gfx *g, const unsigned int *cover, int x, int y, int size, int r, int alpha);
/* The system's picture (art->btn) of a button or the drawn glyph, centred. */
void hm_button_pic(hm_gfx *g, const hm_ui *ui, int which, int cx, int cy, int size,
                   unsigned int color, int alpha);
/* An image of the system's art (its rows padded to 16 pixels), centred, at its size. */
void hm_art_draw(hm_gfx *g, const pw_image *img, int cx, int cy, int alpha);
/* The same scaled to fit a box of w x h, centred. */
void hm_art_fit(hm_gfx *g, const pw_image *img, int cx, int cy, int w, int h, int alpha);
/* The Enter / Back / Options hints from the right edge x1 leftwards; returns where they end. */
int hm_hints_right(hm_gfx *g, const hm_ui *ui, const hm_view *v, int x1, int y, unsigned int color,
                   int alpha);
/* The tint of a glyph's square in Modern's lists (by what it stands for). */
unsigned int hm_glyph_tint(int glyph, unsigned int accent);
/* The photo editor's tool bar over the picture (any style). */
void hm_draw_editor(hm_gfx *g, const hm_ui *ui, const hm_view *v);
/* A short note at the bottom centre. */
/* s in at most lines lines of max_w (broken at spaces, the last one cut), lh apart from the
   baseline y; centred on x when center. */
void hm_text_wrap(hm_gfx *g, pw_font *f, int x, int y, int lh, int lines, const char *s, int max_w,
                  int center, unsigned int color, int alpha);
void hm_draw_toast(hm_gfx *g, const hm_ui *ui, const hm_view *v, unsigned int card,
                   unsigned int text);

void hm_draw_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_draw_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_draw_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v);
void hm_prefetch_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v);

#endif
