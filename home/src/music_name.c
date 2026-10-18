// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "music.h"

static void copy(char *out, int cap, const char *s, int n)
{
    if (n > cap - 1)
        n = cap - 1;
    if (n < 0)
        n = 0;
    memcpy(out, s, (size_t)n);
    out[n] = 0;
}

void hm_music_name(const char *file, char *title, int tcap, char *artist, int acap)
{
    const char *slash = strrchr(file, '/'), *name = slash ? slash + 1 : file,
               *dot = strrchr(name, '.'), *end = dot ? dot : name + strlen(name), *dash = 0;

    for (const char *p = name; p + 3 <= end; p++) /* the first " - " splits title and artist */
        if (p[0] == ' ' && p[1] == '-' && p[2] == ' ') {
            dash = p;
            break;
        }
    if (dash) {
        copy(title, tcap, name, (int)(dash - name));
        copy(artist, acap, dash + 3, (int)(end - dash - 3));
    } else {
        copy(title, tcap, name, (int)(end - name));
        artist[0] = 0;
    }
}

unsigned int hm_music_id3_size(const unsigned char *h)
{
    unsigned int n;

    if (h[0] != 'I' || h[1] != 'D' || h[2] != '3' || (h[6] | h[7] | h[8] | h[9]) & 0x80)
        return 0;
    n = (unsigned int)h[6] << 21 | (unsigned int)h[7] << 14 | (unsigned int)h[8] << 7 | h[9];
    return n + 10 + (h[5] & 0x10 ? 10 : 0); /* the header, and a footer when there is one */
}
