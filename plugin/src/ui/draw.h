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

/* A helper called for every pixel (ADR-029): inlined even at -Os, where a call per pixel cost a
   frame 10-35 % more (host bench, make -C tests bench BENCH_OPT=-Os). */
#define PW_PIXEL static inline __attribute__((always_inline))

/* PSP display pixel formats (PSP_DISPLAY_PIXEL_FORMAT_*). */
enum { PW_FMT_565 = 0, PW_FMT_5551 = 1, PW_FMT_4444 = 2, PW_FMT_8888 = 3 };

void pw_draw_fill(pw_canvas *c, int x, int y, int w, int h, unsigned int color);

/* Mix color into the rectangle: alpha 0 leaves it, 256 replaces it. */
void pw_draw_blend(pw_canvas *c, int x, int y, int w, int h, unsigned int color, int alpha);

/* 8x8 text scaled by scale (1, 2, ...), transparent background. Returns the width drawn. */
int pw_draw_text(pw_canvas *c, int x, int y, const char *s, unsigned int color, int scale);
int pw_draw_text_width(const char *s, int scale);
/* The same with coverage scaled by alpha (0-256). */
int pw_draw_text_alpha(pw_canvas *c, int x, int y, const char *s, unsigned int color, int scale,
                       int alpha);

/* Blend color through an 8-bit coverage mask (w x h, stride bytes per row), scaled by alpha. */
void pw_draw_mask(pw_canvas *c, int x, int y, const unsigned char *mask, int w, int h, int stride,
                  unsigned int color, int alpha);

/* Blend 32-bit pixels with their own alpha (not premultiplied), scaled by alpha (0-256), with the
   top-left corner at (x, y). */
void pw_draw_image(pw_canvas *c, const unsigned int *px, int w, int h, int x, int y, int alpha);
/* The same, stretched (nearest neighbour) to dw x dh. */
void pw_draw_image_scaled(pw_canvas *c, const unsigned int *px, int w, int h, int x, int y, int dw,
                          int dh, int alpha);

/* Linear mix of two whole canvases of the same size into dst: t 0 = a, 256 = b. */
void pw_draw_lerp(pw_canvas *dst, const unsigned int *a, const unsigned int *b, int t);

/* Soften the whole canvas: a box blur of the given radius across and down (twice ~ Gaussian). */
void pw_draw_blur(pw_canvas *c, int radius);

/* Scale every pixel's colour by k/256 (0 black .. 256 unchanged). */
void pw_draw_darken(pw_canvas *c, int k);

/*
 * Copy a frame in one of the PSP display formats (src_stride pixels per row) into the canvas,
 * scaled by bright (0-256; 256 = unchanged). Rows/columns beyond the canvas are ignored.
 */
void pw_draw_from_frame(pw_canvas *c, const void *src, int src_stride, int fmt, int bright);

#endif
