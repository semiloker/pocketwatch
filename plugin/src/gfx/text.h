// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_TEXT_H
#define PW_TEXT_H

#include "gfx/pgf.h"
#include "ui/draw.h"

/*
 * Text in the system font at the XMB's sizes: glyphs are decoded from a PGF once, scaled down by
 * area averaging (the XMB draws ltn0 at ~0.8 and ~0.63 of its 18 px em) and kept in a cache.
 * Without a PGF the built-in 8x8 font stands in, so the menu always has text. Portable.
 */

#define PW_FONT_SLOTS 128 /* cached glyphs per font (open addressing on the char code) */

typedef struct {
    unsigned short code; /* 0 = empty slot; PGF char codes are 16-bit */
    unsigned char w, h;
    signed char left, top; /* pixels, scaled */
    unsigned short adv;    /* 1/64 px, scaled */
    unsigned short off;    /* bitmap offset in the pool (pools are < 64 KB) */
    unsigned char missing;
} pw_glyph_slot;

typedef struct {
    const pw_pgf *pgf; /* NULL: built-in 8x8 font */
    int scale;         /* of the PGF's native size, 1/256 (PGF) or integer factor (8x8) */
    unsigned char *pool;
    unsigned int pool_size, pool_used;
    unsigned char *scratch; /* native glyph decode, 2 * max_w * max_h + resampling rows */
    unsigned int scratch_size;
    int ascent, descent; /* pixels above / below the baseline */
    pw_glyph_slot slots[PW_FONT_SLOTS];
} pw_font;

/* Bytes of scratch pw_font_init needs for this PGF. */
unsigned int pw_font_scratch_size(const pw_pgf *pgf);

int pw_font_init(pw_font *f, const pw_pgf *pgf, int scale256, void *pool, unsigned int pool_size,
                 void *scratch, unsigned int scratch_size);
void pw_font_init_fallback(pw_font *f, int scale);

/* Decode a UTF-8 sequence; returns the code point (U+FFFD on bad input) and advances *s. */
unsigned int pw_utf8_next(const char **s);

int pw_text_width(pw_font *f, const char *utf8);

/* Draw at the baseline y; alpha 0-256 scales the colour's coverage. Returns the advance. With
   shadow, the XMB's soft dark shadow is drawn first (offset down-right). */
int pw_text_draw(pw_canvas *c, pw_font *f, int x, int y, const char *utf8, unsigned int color,
                 int alpha, int shadow);

/* The same, cut to max_w pixels with "..." when it is wider. */
int pw_text_draw_fit(pw_canvas *c, pw_font *f, int x, int y, const char *utf8, int max_w,
                     unsigned int color, int alpha, int shadow);

#endif
