// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PNG_H
#define PW_PNG_H

/*
 * Minimal PNG encoder: 8-bit RGB, filter 0, deflate "stored" blocks (no compression).
 * Output is ~3 bytes per pixel (ICON0 144x80 = 34.8 KB, PIC1 480x272 = 392 KB) but encoding
 * is a straight copy, needs no heap and no tables beyond CRC-32.
 */

/* Fill `rgb` with `width` RGB triplets for row `y`. */
typedef void (*pw_png_row_fn)(void *ctx, unsigned int y, unsigned char *rgb);

/* Exact encoded size for a width x height image. */
unsigned int pw_png_size(unsigned int width, unsigned int height);

/*
 * Encode into out (cap bytes). `scratch` must hold 1 + 3 * width bytes.
 * Returns the number of bytes written, or -1 if cap is too small or the size is invalid.
 */
int pw_png_encode_rgb(unsigned int width, unsigned int height, pw_png_row_fn row, void *ctx,
                      unsigned char *scratch, unsigned char *out, unsigned int cap);

/*
 * Decode a PNG written by pw_png_encode_rgb (stored deflate, filter none) into 32-bit pixels
 * (0xAABBGGRR). Other PNGs are refused. Returns 0 or < 0.
 */
int pw_png_decode_stored(const void *png, unsigned int len, unsigned int *out, unsigned int cap,
                         unsigned int *width, unsigned int *height);

#endif
