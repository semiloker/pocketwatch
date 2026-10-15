// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * LZ4 block codec, see lz.h. The vendored lz4.c is compiled into this file: LZ4LIB_VISIBILITY
 * static makes every LZ4 entry point file-local, so whatever the wrappers below do not reach is
 * dropped. Portable (no PSP headers): the host tests build the very same file.
 */
#include <string.h>

#include "core/lz.h"

/* The kernel's sysclib memmove is not relied on (see lib/lz4/README.pocketwatch); LZ4 rarely
   needs it. */
static void *lz_memmove(void *dst, const void *src, unsigned int n)
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
#define LZ4_memmove(d, s, n)    lz_memmove((d), (s), (n))
#define LZ4_memset(p, v, n)     memset((p), (v), (n))
#define LZ4LIB_VISIBILITY       static
#define LZ4_MEMORY_USAGE        13 /* 8 KB hash table: fits the 16 KB D-cache */
#define LZ4_FORCE_MEMORY_ACCESS 1  /* packed structs: gcc emits lwl/lwr on MIPS */
#define LZ4_DISABLE_DEPRECATE_WARNINGS
#include "lz4/lz4.c"

unsigned int pw_lz_state_size(void)
{
    return (unsigned int)LZ4_sizeofState();
}

int pw_lz_pack(const void *src, int len, void *dst, int cap, void *state)
{
    return LZ4_compress_fast_extState(state, src, dst, len, cap, 1);
}

int pw_lz_unpack(const void *src, int len, void *dst, int cap)
{
    return LZ4_decompress_safe(src, dst, len, cap);
}
