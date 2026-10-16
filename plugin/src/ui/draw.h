// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_DRAW_H
#define PW_DRAW_H

/*
 * Software drawing on a 32-bit canvas in the PSP's 8888 layout (0xAABBGGRR: red in the low byte).
 * Portable (host-tested); the menu draws here and fb.c puts the result on screen. Everything is
 * clipped to the canvas.
 */

typedef struct {
    unsigned int *px;
    int w, h;   /* visible size */
    int stride; /* pixels per row */
} pw_canvas;

#define PW_RGB(r, g, b) (0xFF000000u | ((unsigned int)(b) << 16) | ((g) << 8) | (r))

/* PSP display pixel formats (PSP_DISPLAY_PIXEL_FORMAT_*). */
enum { PW_FMT_565 = 0, PW_FMT_5551 = 1, PW_FMT_4444 = 2, PW_FMT_8888 = 3 };

void pw_draw_fill(pw_canvas *c, int x, int y, int w, int h, unsigned int color);

/* Mix color into the rectangle: alpha 0 leaves it, 256 replaces it. */
void pw_draw_blend(pw_canvas *c, int x, int y, int w, int h, unsigned int color, int alpha);

/* 8x8 text scaled by scale (1, 2, ...), transparent background. Returns the width drawn. */
int pw_draw_text(pw_canvas *c, int x, int y, const char *s, unsigned int color, int scale);
int pw_draw_text_width(const char *s, int scale);

/*
 * Copy a frame in one of the PSP display formats (src_stride pixels per row) into the canvas,
 * scaled by bright (0-256; 256 = unchanged). Rows/columns beyond the canvas are ignored.
 */
void pw_draw_from_frame(pw_canvas *c, const void *src, int src_stride, int fmt, int bright);

#endif
