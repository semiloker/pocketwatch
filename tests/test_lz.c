// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "core/lz.h"
#include "pwtest.h"

static unsigned long long g_state[0x8000 / 8];

PW_TEST(lz_state_fits_the_stream_workspace)
{
    CHECK(pw_lz_state_size() <= sizeof(g_state));
}

PW_TEST(lz_roundtrip_repetitive)
{
    unsigned char src[20000], packed[20000], out[20000];
    int n;

    for (unsigned int i = 0; i < sizeof(src); i++)
        src[i] = (unsigned char)("savestate:"[i % 10] + (i / 4096));
    n = pw_lz_pack(src, sizeof(src), packed, sizeof(packed), g_state);
    CHECK(n > 0 && n < (int)sizeof(src) / 4);
    CHECK_EQ(pw_lz_unpack(packed, n, out, sizeof(out)), (int)sizeof(src));
    CHECK(memcmp(src, out, sizeof(src)) == 0);
}

PW_TEST(lz_incompressible_does_not_fit_a_smaller_buffer)
{
    unsigned char src[4096], packed[4096];
    unsigned int x = 1;

    for (unsigned int i = 0; i < sizeof(src); i++) {
        x = x * 1103515245u + 12345u;
        src[i] = (unsigned char)(x >> 16);
    }
    CHECK_EQ(pw_lz_pack(src, sizeof(src), packed, sizeof(src) - 1, g_state), 0);
}

PW_TEST(lz_rejects_corrupt_input_and_short_output)
{
    unsigned char src[8192], packed[8192], out[8192];
    int n;

    memset(src, 'A', sizeof(src));
    memcpy(src + 100, "pocketwatch", 11);
    n = pw_lz_pack(src, sizeof(src), packed, sizeof(packed), g_state);
    CHECK(n > 0);
    CHECK(pw_lz_unpack(packed, n, out, sizeof(out) - 1) < 0); /* output too small */
    packed[0] = 0xFF;                                         /* corrupt the first token */
    packed[1] = 0xFF;
    packed[2] = 0xFF;
    CHECK(pw_lz_unpack(packed, n, out, sizeof(out)) != (int)sizeof(src));
}
