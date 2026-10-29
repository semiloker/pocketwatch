// SPDX-License-Identifier: GPL-3.0-or-later
#include "savedata.h"

void hm_saves_first_line(const char *s, unsigned int n, char *out, int cap)
{
    int k = 0;

    for (unsigned int i = 0; i < n && s[i] && k < cap - 1; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            if (k > 0)
                break;
            continue; /* (leading blank lines) */
        }
        out[k++] = s[i];
    }
    while (k > 0 && out[k - 1] == ' ')
        k--;
    out[k] = 0;
}
