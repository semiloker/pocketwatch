// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugins_txt.h"

#include <string.h>

/* Does [s, e) contain path, ignoring case (FAT paths)? */
static int has(const char *s, const char *e, const char *path)
{
    int n = (int)strlen(path);

    for (; e - s >= n; s++) {
        int i = 0;

        while (i < n && (s[i] | 0x20) == (path[i] | 0x20))
            i++;
        if (i == n)
            return 1;
    }
    return 0;
}

/* The last field of [s, e): "on" (1), "off" (0), else -1. */
static int state(const char *s, const char *e)
{
    const char *c = e;

    while (c > s && c[-1] != ',')
        c--;
    while (c < e && (*c == ' ' || *c == '\t'))
        c++;
    if (e - c >= 2 && (c[0] | 0x20) == 'o' && (c[1] | 0x20) == 'n' &&
        (e - c == 2 || c[2] == ' ' || c[2] == '\t'))
        return 1;
    if (e - c >= 3 && (c[0] | 0x20) == 'o' && (c[1] | 0x20) == 'f' && (c[2] | 0x20) == 'f')
        return 0;
    return -1;
}

static int put(char *out, int cap, int n, const char *s, int len)
{
    if (n < 0 || n + len > cap)
        return -1;
    memcpy(out + n, s, (size_t)len);
    return n + len;
}

int hm_plugins_set(const char *text, int len, const char *path, int on, char *out, int cap)
{
    const char *p = text, *end = text + len;
    int n = 0, found = 0, crlf = 0;

    while (p < end && n >= 0) {
        const char *e = p, *le;

        while (e < end && *e != '\n')
            e++;
        le = e > p && e[-1] == '\r' ? e - 1 : e;
        crlf |= le != e;
        if (has(p, le, path) && !found) {
            const char *c = le;

            while (c > p && c[-1] != ',')
                c--;
            found = 1;
            n = put(out, cap, n, p, (int)(c - p));
            n = put(out, cap, n, on ? " on" : " off", on ? 3 : 4);
        } else {
            n = put(out, cap, n, p, (int)(le - p));
        }
        n = put(out, cap, n, crlf ? "\r\n" : "\n", crlf ? 2 : 1);
        p = e + 1;
    }
    if (!found && on) {
        n = put(out, cap, n, "UMD, ", 5);
        n = put(out, cap, n, path, (int)strlen(path));
        n = put(out, cap, n, crlf ? ", on\r\n" : ", on\n", crlf ? 6 : 5);
    }
    return n;
}

int hm_plugins_get(const char *text, int len, const char *path)
{
    const char *p = text, *end = text + len;

    while (p < end) {
        const char *e = p, *le;

        while (e < end && *e != '\n')
            e++;
        le = e > p && e[-1] == '\r' ? e - 1 : e;
        if (has(p, le, path))
            return state(p, le) == 1 ? 1 : 0;
        p = e + 1;
    }
    return -1;
}
