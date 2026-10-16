// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>

#include "core/crc32.h"
#include "core/png.h"
#include "pwtest.h"

/* Deterministic test pattern. */
static void pattern(void *ctx, unsigned int y, unsigned char *rgb)
{
    unsigned int width = *(unsigned int *)ctx;

    for (unsigned int x = 0; x < width; x++) {
        rgb[3 * x + 0] = (unsigned char)(x * 7 + y);
        rgb[3 * x + 1] = (unsigned char)(x ^ y);
        rgb[3 * x + 2] = (unsigned char)(y * 3);
    }
}

static unsigned int be32(const unsigned char *p)
{
    return (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 | (unsigned int)p[2] << 8 | p[3];
}

/* Independent bit-by-bit CRC-32, so the test does not trust pw_crc32's table. */
static unsigned int slow_crc32(const unsigned char *p, unsigned int len)
{
    unsigned int c = 0xFFFFFFFFu;

    while (len--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

/* Decode our PNG (stored deflate only) and compare every pixel with the pattern. */
static void check_roundtrip(unsigned int width, unsigned int height)
{
    unsigned int cap = pw_png_size(width, height);
    unsigned char *png = malloc(cap + 16);
    unsigned char *scratch = malloc(1 + 3 * width);
    unsigned char *expect = malloc(3 * width);
    unsigned char *raw = malloc(height * (1 + 3 * width));
    unsigned int ctx = width;
    int size = pw_png_encode_rgb(width, height, pattern, &ctx, scratch, png, cap);

    CHECK_EQ(size, cap);
    CHECK(memcmp(png, "\x89PNG\r\n\x1a\n", 8) == 0);

    /* Walk the chunks and verify every CRC. */
    unsigned int pos = 8, idat_len = 0, idat_pos = 0;
    int saw_iend = 0;
    while (pos + 12 <= (unsigned int)size) {
        unsigned int len = be32(png + pos);
        const unsigned char *type = png + pos + 4;

        CHECK_EQ(be32(png + pos + 8 + len), slow_crc32(type, 4 + len));
        if (memcmp(type, "IHDR", 4) == 0) {
            CHECK_EQ(be32(png + pos + 8), width);
            CHECK_EQ(be32(png + pos + 12), height);
            CHECK(memcmp(png + pos + 16, "\x08\x02\x00\x00\x00", 5) == 0);
        } else if (memcmp(type, "IDAT", 4) == 0) {
            idat_pos = pos + 8;
            idat_len = len;
        } else if (memcmp(type, "IEND", 4) == 0) {
            saw_iend = 1;
        }
        pos += 12 + len;
    }
    CHECK_EQ(pos, size);
    CHECK(saw_iend);

    /* zlib header, stored blocks, Adler-32. */
    const unsigned char *z = png + idat_pos;
    CHECK_EQ((z[0] << 8 | z[1]) % 31, 0);
    unsigned int zp = 2, out = 0, final = 0;
    while (!final && zp + 5 <= idat_len) {
        unsigned int n = z[zp + 1] | z[zp + 2] << 8;
        unsigned int nn = z[zp + 3] | z[zp + 4] << 8;

        final = z[zp] & 1;
        CHECK_EQ(z[zp] >> 1, 0); /* BTYPE 00 */
        CHECK_EQ(n ^ 0xFFFF, nn);
        memcpy(raw + out, z + zp + 5, n);
        out += n;
        zp += 5 + n;
    }
    CHECK(final);
    CHECK_EQ(out, height * (1 + 3 * width));

    unsigned int a = 1, b = 0;
    for (unsigned int i = 0; i < out; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    CHECK_EQ(be32(z + zp), (b << 16) | a);
    CHECK_EQ(zp + 4, idat_len);

    for (unsigned int y = 0; y < height; y++) {
        const unsigned char *row = raw + y * (1 + 3 * width);

        pattern(&ctx, y, expect);
        CHECK_EQ(row[0], 0);
        CHECK(memcmp(row + 1, expect, 3 * width) == 0);
    }

    free(png);
    free(scratch);
    free(expect);
    free(raw);
}

PW_TEST(png_tiny_image_roundtrip)
{
    check_roundtrip(3, 2);
}

PW_TEST(png_icon0_size_roundtrip)
{
    check_roundtrip(144, 80);
}

PW_TEST(png_pic1_spans_many_stored_blocks)
{
    check_roundtrip(480, 272); /* 392 KB of raw data -> 6 deflate blocks */
}

PW_TEST(png_rejects_small_buffer_and_bad_sizes)
{
    unsigned char scratch[1 + 3 * 4], out[64];
    unsigned int w = 4;

    CHECK_EQ(pw_png_encode_rgb(4, 4, pattern, &w, scratch, out, sizeof(out)), -1);
    CHECK_EQ(pw_png_encode_rgb(0, 4, pattern, &w, scratch, out, sizeof(out)), -1);
}

PW_TEST(crc32_standard_check_value)
{
    CHECK_EQ(pw_crc32(0, "123456789", 9), 0xCBF43926u);
    /* Incremental use gives the same result. */
    CHECK_EQ(pw_crc32(pw_crc32(0, "1234", 4), "56789", 5), 0xCBF43926u);
}

static void grad_row(void *ctx, unsigned int y, unsigned char *rgb)
{
    unsigned int w = *(unsigned int *)ctx;

    for (unsigned int x = 0; x < w; x++) {
        rgb[3 * x] = (unsigned char)x;
        rgb[3 * x + 1] = (unsigned char)y;
        rgb[3 * x + 2] = (unsigned char)(x ^ y);
    }
}

static void check_decode(unsigned int w, unsigned int h)
{
    unsigned int cap = pw_png_size(w, h), dw = 0, dh = 0;
    unsigned char *png = malloc(cap), *scratch = malloc(1 + 3 * w);
    unsigned int *px = malloc(w * h * 4);
    int n = pw_png_encode_rgb(w, h, grad_row, &w, scratch, png, cap);

    CHECK(n > 0);
    CHECK_EQ(pw_png_decode_stored(png, (unsigned int)n, px, w * h, &dw, &dh), 0);
    CHECK_EQ(dw, w);
    CHECK_EQ(dh, h);
    CHECK_EQ(px[0], 0xFF000000u);
    CHECK_EQ(px[(h - 1) * w + (w - 1)], 0xFF000000u | (((w - 1) ^ (h - 1)) & 0xFF) << 16 |
                                            ((h - 1) & 0xFF) << 8 | ((w - 1) & 0xFF));
    CHECK(pw_png_decode_stored(png, (unsigned int)n / 2, px, w * h, &dw, &dh) < 0);
    CHECK(pw_png_decode_stored(png, (unsigned int)n, px, w * h - 1, &dw, &dh) < 0);
    free(png);
    free(scratch);
    free(px);
}

PW_TEST(png_decode_stored_roundtrip)
{
    check_decode(144, 80);
    check_decode(480, 272); /* many stored blocks */
}
