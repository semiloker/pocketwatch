// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/crc32.h"
#include "pwtest.h"

PW_TEST(crc32_check_value)
{
    CHECK_EQ(pw_crc32(0, "123456789", 9), 0xCBF43926u);
    CHECK_EQ(pw_crc32(0, "", 0), 0);
}

PW_TEST(crc32_chains)
{
    CHECK_EQ(pw_crc32(pw_crc32(0, "1234", 4), "56789", 5), 0xCBF43926u);
}

PW_TEST(sum32_detects_changes_and_order)
{
    unsigned int a[64], b[64];

    for (unsigned int i = 0; i < 64; i++)
        a[i] = b[i] = i * 2654435761u;
    CHECK_EQ(pw_sum32(a, sizeof(a)), pw_sum32(b, sizeof(b)));
    b[17] ^= 1;
    CHECK(pw_sum32(a, sizeof(a)) != pw_sum32(b, sizeof(b)));
    b[17] ^= 1;
    b[3] = a[4]; /* two words swapped: a plain sum would not notice */
    b[4] = a[3];
    CHECK(pw_sum32(a, sizeof(a)) != pw_sum32(b, sizeof(b)));
}

PW_TEST(sum32_ignores_tail_bytes)
{
    unsigned int a[2] = {1, 2};

    CHECK_EQ(pw_sum32(a, 8), pw_sum32(a, 11)); /* only whole words count */
    CHECK_EQ(pw_sum32(a, 0), 0);
}
