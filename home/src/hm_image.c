// SPDX-License-Identifier: GPL-3.0-or-later
#include "hm_image.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"

unsigned int *hm_png_decode(const void *data, int len, int *w, int *h)
{
    int channels;

    return (unsigned int *)stbi_load_from_memory(data, len, w, h, &channels, 4);
}

void hm_image_free(void *pixels)
{
    stbi_image_free(pixels);
}

int hm_inflate_raw(void *out, int out_len, const void *in, int in_len)
{
    return stbi_zlib_decode_noheader_buffer(out, out_len, in, in_len);
}

void hm_image_fit(const unsigned int *src, int sw, int sh, unsigned int *dst, int dw, int dh)
{
    for (int y = 0; y < dh; y++) {
        int y0 = y * sh / dh, y1 = (y + 1) * sh / dh;

        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < dw; x++) {
            int x0 = x * sw / dw, x1 = (x + 1) * sw / dw;
            unsigned int sum[4] = {0, 0, 0, 0}, n = 0;

            if (x1 <= x0)
                x1 = x0 + 1;
            for (int yy = y0; yy < y1 && yy < sh; yy++)
                for (int xx = x0; xx < x1 && xx < sw; xx++) {
                    unsigned int p = src[yy * sw + xx];

                    for (int k = 0; k < 4; k++)
                        sum[k] += (p >> (8 * k)) & 0xFF;
                    n++;
                }
            if (!n)
                n = 1;
            dst[y * dw + x] =
                (sum[0] / n) | (sum[1] / n) << 8 | (sum[2] / n) << 16 | (sum[3] / n) << 24;
        }
    }
}
