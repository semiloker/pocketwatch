// SPDX-License-Identifier: GPL-3.0-or-later
#include "hm_image.h"

/* JPEG out through the vendored stb_image_write (public domain / MIT, home/lib/stb_image_write.h):
   the photo editor's copies (HOM-REQ-026). */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#include "stb_image_write.h"

int hm_jpeg_encode(const unsigned int *px, int w, int h, int quality, hm_write_fn write,
                   void *ctx)
{
    /* PW_RGB words are R, G, B, A bytes in memory: stb's four-component layout */
    return stbi_write_jpg_to_func(write, ctx, w, h, 4, px, quality) ? 0 : -1;
}
