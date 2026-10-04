// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/ring.h"

void pw_ring_init(pw_ring *r, char *buf, unsigned int cap)
{
    r->buf = buf;
    r->cap = cap;
    r->head = 0;
    r->len = 0;
    r->dropped = 0;
}

int pw_ring_push(pw_ring *r, const char *data, unsigned int n)
{
    unsigned int first;

    if (n == 0)
        return 0;
    if (n > r->cap - r->len) {
        r->dropped++;
        return -1;
    }

    first = r->cap - r->head;
    if (first > n)
        first = n;
    memcpy(r->buf + r->head, data, first);
    memcpy(r->buf, data + first, n - first);

    r->head = (r->head + n) % r->cap;
    r->len += n;
    return 0;
}

unsigned int pw_ring_pop(pw_ring *r, char *out, unsigned int max)
{
    unsigned int n, tail, first;

    if (r->len == 0 || max == 0)
        return 0;

    n = r->len < max ? r->len : max;
    tail = (r->head + r->cap - r->len) % r->cap;
    first = r->cap - tail;
    if (first > n)
        first = n;
    memcpy(out, r->buf + tail, first);
    memcpy(out + first, r->buf, n - first);

    r->len -= n;
    return n;
}
