// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/rlz.h"

/*
 * Probability tables, contiguous like the encoder's (RCOMage rlzpack.c RLZ_Info): some indices
 * run past the end of one table into the next (the long-distance tree into the short one), and
 * a decoder must see the same bytes there. The distance groups get room for every bit length.
 */
#define P_LIT        0
#define P_DIST_LONG  (P_LIT + 8 * 0xFF)
#define P_DIST_SHORT (P_DIST_LONG + 0xF0)
#define P_DISTANCES  (P_DIST_SHORT + 0x40)
#define P_LEN_DESC   (P_DISTANCES + 32 * 0x20)
#define P_MATCH_LENS (P_LEN_DESC + 0x40)
#define P_TOTAL      (P_MATCH_LENS + 0xFF)

_Static_assert(P_TOTAL <= PW_RLZ_STATE, "RLZ state too small");

typedef struct {
    const unsigned char *src;
    unsigned int len, pos, over; /* over: bytes read past the end (zeros) */
    unsigned int code, range;
    unsigned char *p;
} dec;

static void normalize(dec *d)
{
    if ((d->range >> 24) == 0) { /* exactly once per primitive, like the encoder's flush */
        unsigned int b = 0;

        if (d->pos < d->len)
            b = d->src[d->pos];
        else
            d->over++;
        d->pos++;
        d->range <<= 8;
        d->code = (d->code << 8) | b;
    }
}

static int bit(dec *d, unsigned int i)
{
    unsigned int prob = d->p[i], bound = (d->range >> 8) * prob;
    int r;

    prob -= prob >> 3;
    if (d->code < bound) {
        d->range = bound;
        prob += 31;
        r = 1;
    } else {
        d->code -= bound;
        d->range -= bound;
        r = 0;
    }
    d->p[i] = (unsigned char)prob;
    normalize(d);
    return r;
}

/* Bits k..0 of a number, in the encoder's write_special order. */
static unsigned int number(dec *d, unsigned int i, int k)
{
    unsigned int v = 0;

    if (k >= 3) {
        v = (v << 1) | (unsigned int)bit(d, i);
        k--;
        if (k >= 3) {
            v = (v << 1) | (unsigned int)bit(d, i);
            k--;
        }
        i++;
        if (k >= 3) { /* direct bits, then one normalization */
            for (int m = k - 2; m > 0; m--) {
                d->range >>= 1;
                if (d->code < d->range) {
                    v = (v << 1) | 1;
                } else {
                    d->code -= d->range;
                    v <<= 1;
                }
            }
            k = 2;
            normalize(d);
        }
    }
    for (; k >= 0; k--, i++)
        v = (v << 1) | (unsigned int)bit(d, i);
    return v;
}

int pw_rlz_decode(const void *src, unsigned int src_len, void *dst, unsigned int cap,
                  unsigned char *state)
{
    const unsigned char *s = src;
    unsigned char *out = dst, last = 0;
    unsigned int n = 0, var_c = 0, mode, real, init;
    dec d;

    if (src_len < 5)
        return -1;
    mode = s[0];
    real = mode & 7;
    init = ((4 - ((mode >> 3) & 3)) << 5) & 0xFF;
    for (int i = 0; i < P_TOTAL; i++)
        state[i] = (unsigned char)init;
    d.src = s;
    d.len = src_len;
    d.pos = 5;
    d.over = 0;
    d.code =
        ((unsigned int)s[1] << 24) | ((unsigned int)s[2] << 16) | ((unsigned int)s[3] << 8) | s[4];
    d.range = 0xFFFFFFFFu;
    d.p = state;

    for (;;) {
        int ones = 0, hi, ret;
        unsigned int length;

        while (ones < 8 && bit(&d, P_LEN_DESC + (unsigned int)ones * 8 + var_c))
            ones++;
        hi = ones - 1; /* position of the length's top bit; -1 for a literal */
        if (hi <= 0) {
            length = hi < 0 ? 0 : 1;
            ret = hi;
        } else {
            unsigned int k = (unsigned int)hi - 1;

            length = (1u << hi) |
                     number(&d, P_MATCH_LENS + ((var_c & 7) + k * 8) * 4 + ((n << k) & 3), (int)k);
            ret = hi;
        }
        if (d.over > 8)
            return -2; /* ran far past the end of the input: damaged */
        if (length == 0xFF)
            break; /* end marker */

        if (length == 0) {
            unsigned int ctx = ((((n & 7) << 8) | last) >> real) & 7, node = 1;

            for (int b = 0; b < 8; b++)
                node = (node << 1) | (unsigned int)bit(&d, P_LIT + ctx * 0xFF + node - 1);
            if (n >= cap)
                return -3;
            out[n++] = last = (unsigned char)node;
            if (var_c > 0)
                var_c--;
            continue;
        }

        unsigned int match = length + 1, bits = match < 4 ? 3 : 5,
                     tree = match < 4 ? P_DIST_SHORT : P_DIST_LONG;
        unsigned int node = 1, dist;
        int sig;

        for (unsigned int b = 0; b < bits; b++)
            node = (node << 1) | (unsigned int)bit(&d, tree + node * 8 + (unsigned int)ret);
        sig = (int)node - (1 << bits) - (match >= 4 ? 12 : 0);
        if (sig < 0 || sig > 30)
            return -4;
        if (sig == 0) /* distance 1: the encoder writes no further bits */
            dist = 1;
        else
            dist = (1u << sig) | number(&d, P_DISTANCES + (unsigned int)(sig - 1) * 0x20, sig - 1);
        if (dist > n || match > cap - n)
            return -5;
        for (unsigned int i = 0; i < match; i++, n++)
            out[n] = out[n - dist];
        last = out[n - 1];
        var_c = 7 - (n & 1);
    }
    return (int)n;
}
