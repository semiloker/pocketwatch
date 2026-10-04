// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_RING_H
#define PW_RING_H

/* Byte ring buffer. Not thread-safe by itself: callers serialize access (see core/log.c). */
typedef struct {
    char *buf;
    unsigned int cap;
    unsigned int head;    /* next write position */
    unsigned int len;     /* bytes currently stored */
    unsigned int dropped; /* pushes rejected because there was no room */
} pw_ring;

void pw_ring_init(pw_ring *r, char *buf, unsigned int cap);

/* Store all n bytes or nothing (keeps records whole). Returns 0, or -1 if it does not fit. */
int pw_ring_push(pw_ring *r, const char *data, unsigned int n);

/* Move up to max of the oldest bytes into out. Returns the number of bytes moved. */
unsigned int pw_ring_pop(pw_ring *r, char *out, unsigned int max);

#endif
