// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * LZ4 block codec for the benchmark (lib/lz4, BSD-2-Clause, v1.10.0), built freestanding.
 * LZ4LIB_VISIBILITY static turns every LZ4 entry point into a file-local function, so the
 * compiler drops whatever the two wrappers below do not reach and the PRX stays small.
 */
#include <string.h>

/* Kernel sysclib's memmove is not relied on (see lib/fastlz); LZ4 needs it rarely. */
static void *pw_memmove(void *dst, const void *src, unsigned int n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d < s)
        while (n--)
            *d++ = *s++;
    else
        for (d += n, s += n; n--;)
            *--d = *--s;
    return dst;
}

#define LZ4_FREESTANDING        1
#define LZ4_memcpy(d, s, n)     __builtin_memcpy((d), (s), (n))
#define LZ4_memmove(d, s, n)    pw_memmove((d), (s), (n))
#define LZ4_memset(p, v, n)     memset((p), (v), (n))
#define LZ4LIB_VISIBILITY       static
#define LZ4_MEMORY_USAGE        13 /* 8 KB hash table: fits the 16 KB D-cache */
#define LZ4_FORCE_MEMORY_ACCESS 1  /* packed structs: gcc emits lwl/lwr */
#define LZ4_DISABLE_DEPRECATE_WARNINGS
#include "lz4/lz4.c"

#include "debug/codec_lz4.h"

unsigned int pw_lz4_state_size(void)
{
    return (unsigned int)LZ4_sizeofState();
}

int pw_lz4_pack(const void *src, int len, void *dst, int cap, void *state, int accel)
{
    return LZ4_compress_fast_extState(state, src, dst, len, cap, accel);
}

int pw_lz4_unpack(const void *src, int len, void *dst, int cap)
{
    return LZ4_decompress_safe(src, dst, len, cap);
}
