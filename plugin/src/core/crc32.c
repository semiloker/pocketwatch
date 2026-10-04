// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/crc32.h"

static unsigned int g_table[256];
static int g_table_ready;

static void build_table(void)
{
    for (unsigned int n = 0; n < 256; n++) {
        unsigned int c = n;

        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        g_table[n] = c;
    }
    g_table_ready = 1;
}

unsigned int pw_crc32(unsigned int crc, const void *data, unsigned int len)
{
    const unsigned char *p = data;

    if (!g_table_ready)
        build_table();
    crc = ~crc;
    while (len--)
        crc = g_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
