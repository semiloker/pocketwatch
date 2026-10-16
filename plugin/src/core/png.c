// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/crc32.h"
#include "core/png.h"

#define STORED_MAX 65535u /* largest deflate stored block */

static unsigned int raw_size(unsigned int width, unsigned int height)
{
    return height * (1 + 3 * width); /* filter byte + RGB per row */
}

static unsigned int zlib_size(unsigned int raw)
{
    unsigned int blocks = raw == 0 ? 1 : (raw + STORED_MAX - 1) / STORED_MAX;

    return 2 + raw + 5 * blocks + 4; /* header, data, block headers, Adler-32 */
}

unsigned int pw_png_size(unsigned int width, unsigned int height)
{
    /* signature + IHDR chunk + IDAT chunk + IEND chunk */
    return 8 + (12 + 13) + (12 + zlib_size(raw_size(width, height))) + 12;
}

static unsigned char *put_be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
    return p + 4;
}

/* Writes the CRC of a chunk whose type starts at `type` and whose data ends at `end`. */
static unsigned char *finish_chunk(unsigned char *type, unsigned char *end)
{
    return put_be32(end, pw_crc32(0, type, (unsigned int)(end - type)));
}

typedef struct {
    unsigned char *p;    /* write position */
    unsigned int left;   /* bytes left in the current stored block */
    unsigned int remain; /* raw bytes still to be emitted overall */
    unsigned int adler_a, adler_b;
} Stored;

static void stored_open_block(Stored *s)
{
    unsigned int n = s->remain < STORED_MAX ? s->remain : STORED_MAX;
    int last = n == s->remain;

    s->p[0] = (unsigned char)last; /* BFINAL, BTYPE = 00 */
    s->p[1] = (unsigned char)n;
    s->p[2] = (unsigned char)(n >> 8);
    s->p[3] = (unsigned char)~n;
    s->p[4] = (unsigned char)(~n >> 8);
    s->p += 5;
    s->left = n;
}

/* Adler-32 with the modulo deferred every 5552 bytes (the largest run that cannot overflow). */
static void adler_update(Stored *s, const unsigned char *data, unsigned int len)
{
    unsigned int a = s->adler_a, b = s->adler_b;

    while (len > 0) {
        unsigned int k = len < 5552 ? len : 5552;

        len -= k;
        while (k--) {
            a += *data++;
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
    }
    s->adler_a = a;
    s->adler_b = b;
}

static void stored_write(Stored *s, const unsigned char *data, unsigned int len)
{
    while (len > 0) {
        unsigned int n;

        if (s->left == 0)
            stored_open_block(s);
        n = len < s->left ? len : s->left;
        memcpy(s->p, data, n);
        adler_update(s, data, n);
        s->p += n;
        s->left -= n;
        s->remain -= n;
        data += n;
        len -= n;
    }
}

int pw_png_encode_rgb(unsigned int width, unsigned int height, pw_png_row_fn row, void *ctx,
                      unsigned char *scratch, unsigned char *out, unsigned int cap)
{
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    unsigned char *p = out, *type;
    unsigned int raw;
    Stored s;

    if (width == 0 || height == 0 || width > 4096 || height > 4096)
        return -1;
    raw = raw_size(width, height);
    if (cap < pw_png_size(width, height))
        return -1;

    memcpy(p, signature, sizeof(signature));
    p += sizeof(signature);

    p = put_be32(p, 13);
    type = p;
    memcpy(p, "IHDR", 4);
    p = put_be32(p + 4, width);
    p = put_be32(p, height);
    p[0] = 8; /* bit depth */
    p[1] = 2; /* colour type: RGB */
    p[2] = 0; /* compression */
    p[3] = 0; /* filter */
    p[4] = 0; /* no interlace */
    p = finish_chunk(type, p + 5);

    p = put_be32(p, zlib_size(raw));
    type = p;
    memcpy(p, "IDAT", 4);
    p += 4;
    p[0] = 0x78; /* zlib: deflate, 32 KB window */
    p[1] = 0x01; /* no preset dictionary, fastest; (0x78 << 8 | 0x01) % 31 == 0 */
    s.p = p + 2;
    s.left = 0;
    s.remain = raw;
    s.adler_a = 1;
    s.adler_b = 0;
    for (unsigned int y = 0; y < height; y++) {
        scratch[0] = 0; /* filter: none */
        row(ctx, y, scratch + 1);
        stored_write(&s, scratch, 1 + 3 * width);
    }
    p = put_be32(s.p, (s.adler_b << 16) | s.adler_a);
    p = finish_chunk(type, p);

    p = put_be32(p, 0);
    type = p;
    memcpy(p, "IEND", 4);
    p = finish_chunk(type, p + 4);

    return (int)(p - out);
}

static unsigned int be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) |
           p[3];
}

int pw_png_decode_stored(const void *png, unsigned int len, unsigned int *out, unsigned int cap,
                         unsigned int *width, unsigned int *height)
{
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    const unsigned char *p = png, *end = p + len;
    unsigned int w = 0, h = 0, row = 0, x = 0, left = 0, last = 0, filter_next = 1;
    int zhdr = 2;

    if (len < 8 || memcmp(p, signature, 8) != 0)
        return -1;
    p += 8;
    while (p + 12 <= end) {
        unsigned int n = be32(p);
        const unsigned char *type = p + 4, *d = p + 8;

        if (n > (unsigned int)(end - d) - 4)
            return -2;
        if (memcmp(type, "IHDR", 4) == 0) {
            if (n < 13 || d[8] != 8 || d[9] != 2 || d[12] != 0)
                return -3; /* 8-bit RGB, not interlaced: what pw_png_encode_rgb writes */
            w = be32(d);
            h = be32(d + 4);
            if (w == 0 || h == 0 || w > 4096 || h > 4096 || w * h > cap)
                return -3;
        } else if (memcmp(type, "IDAT", 4) == 0 && w) {
            for (unsigned int i = 0; i < n; i++) {
                unsigned int c = d[i];

                if (zhdr > 0) { /* the 2-byte zlib header */
                    zhdr--;
                    continue;
                }
                if (left == 0) { /* a stored block header: 1 + LEN + NLEN, possibly split */
                    if (last)
                        break; /* after the final block comes the Adler-32 */
                    if (n - i < 5)
                        return -4; /* block header split across IDAT chunks: not ours */
                    if ((c & 6) != 0)
                        return -4; /* compressed block: not written by our encoder */
                    last = c & 1;
                    left = d[i + 1] | (d[i + 2] << 8);
                    i += 4;
                    continue;
                }
                left--;
                if (row >= h)
                    continue;
                if (filter_next) {
                    if (c != 0)
                        return -5; /* only filter "none" */
                    filter_next = 0;
                    continue;
                }
                {
                    unsigned int k = x / 3, ch = x % 3;
                    unsigned int *px = out + row * w + k;

                    if (ch == 0)
                        *px = 0xFF000000u | c;
                    else
                        *px |= c << (8 * ch);
                }
                if (++x == 3 * w) {
                    x = 0;
                    row++;
                    filter_next = 1;
                }
            }
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }
        p = d + n + 4;
    }
    if (row != h || h == 0)
        return -6;
    *width = w;
    *height = h;
    return 0;
}
