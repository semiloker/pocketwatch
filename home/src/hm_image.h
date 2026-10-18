// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_IMAGE_H
#define HM_IMAGE_H

/* PNG and raw deflate for Pocketwatch Home, through the vendored stb_image (public domain / MIT,
   home/lib/stb_image.h). Pixels come out as PW_RGB words (R in the low byte, alpha on top). */

/* Decoded pixels (free with hm_image_free) and their size, or NULL: PNG, JPEG, BMP. */
unsigned int *hm_png_decode(const void *data, int len, int *w, int *h);
/* The size of an encoded image without decoding it; 0, or < 0 when it is not one. */
int hm_image_info(const void *data, int len, int *w, int *h);
void hm_image_free(void *pixels);

/* Raw deflate (no zlib header), as in CSO blocks: bytes produced, or < 0. */
int hm_inflate_raw(void *out, int out_len, const void *in, int in_len);

/* Area-average resize of src (sw x sh) into dst (dw x dh), each with its width as stride. */
void hm_image_fit(const unsigned int *src, int sw, int sh, unsigned int *dst, int dw, int dh);

#endif
