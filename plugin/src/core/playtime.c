// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/crc32.h"
#include "core/playtime.h"
#include "stb_sprintf.h"

/*
 * The file:
 *   # Pocketwatch play time 1 seq <n>
 *   <id>\t<total s>\t<sessions>\t<longest s>\t<first>\t<last>\t<title>
 *   ...
 *   # crc <crc32 of everything before this line>
 * Dates are "YYYY-MM-DD HH:MM:SS" in the PSP's local time.
 */
#define HEAD "# Pocketwatch play time 1 seq "
#define TAIL "# crc "

static int cmp_dt(const pw_datetime *a, const pw_datetime *b)
{
    const unsigned short x[6] = {a->year, a->month, a->day, a->hour, a->minute, a->second},
                         y[6] = {b->year, b->month, b->day, b->hour, b->minute, b->second};

    for (int i = 0; i < 6; i++)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}

/* Digits at *p into *v; returns 0 when there were none. */
static int number(const char **p, const char *end, unsigned int *v)
{
    unsigned int n = 0;
    int any = 0;

    while (*p < end && **p >= '0' && **p <= '9' && n < 400000000u) {
        n = n * 10 + (unsigned int)(**p - '0');
        (*p)++;
        any = 1;
    }
    *v = n;
    return any;
}

static int parse_dt(const char *s, const char *end, pw_datetime *t)
{
    unsigned int v[6];
    const char *p = s;

    for (int i = 0; i < 6; i++) {
        if (!number(&p, end, &v[i]))
            return 0;
        if (i < 5) {
            if (p >= end || (*p != "-- ::"[i]))
                return 0;
            p++;
        }
    }
    if (v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31 || v[3] > 23 || v[4] > 59 || v[5] > 59)
        return 0;
    memset(t, 0, sizeof(*t));
    t->year = (unsigned short)v[0];
    t->month = (unsigned short)v[1];
    t->day = (unsigned short)v[2];
    t->hour = (unsigned short)v[3];
    t->minute = (unsigned short)v[4];
    t->second = (unsigned short)v[5];
    return p == end;
}

/* The fields of a line, split at tabs. */
static int fields(const char *s, const char *end, const char **f, const char **fe, int max)
{
    int n = 0;

    while (n < max) {
        const char *e = s;

        while (e < end && *e != '\t')
            e++;
        f[n] = s;
        fe[n] = e;
        n++;
        if (e >= end)
            break;
        s = e + 1;
    }
    return n;
}

static void copy_field(char *dst, unsigned int cap, const char *s, const char *e)
{
    unsigned int n = (unsigned int)(e - s);

    if (n > cap - 1)
        n = cap - 1;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80 && n < (unsigned int)(e - s))
        n--; /* never cut a UTF-8 character */
    memcpy(dst, s, n);
    dst[n] = 0;
}

int pw_pt_parse(pw_pt_table *t, const char *text, unsigned int len)
{
    const char *p = text, *end = text + len, *tail = 0;
    unsigned int seq = 0, crc = 0;

    t->n = 0;
    t->seq = 0;
    /* the last line must be the CRC of all before it */
    for (const char *q = text; q + sizeof(TAIL) - 1 <= end; q++)
        if ((q == text || q[-1] == '\n') && memcmp(q, TAIL, sizeof(TAIL) - 1) == 0)
            tail = q;
    if (!tail || len < sizeof(HEAD) - 1 || memcmp(text, HEAD, sizeof(HEAD) - 1) != 0)
        return -1;
    {
        const char *c = tail + sizeof(TAIL) - 1;

        for (; c < end && *c != '\n' && *c != '\r'; c++) {
            int d = *c >= '0' && *c <= '9'   ? *c - '0'
                    : *c >= 'A' && *c <= 'F' ? *c - 'A' + 10
                    : *c >= 'a' && *c <= 'f' ? *c - 'a' + 10
                                             : -1;

            if (d < 0)
                return -1;
            crc = crc * 16 + (unsigned int)d;
        }
    }
    if (pw_crc32(0, (const unsigned char *)text, (unsigned int)(tail - text)) != crc)
        return -1;
    p = text + sizeof(HEAD) - 1;
    if (!number(&p, tail, &seq))
        return -1;
    t->seq = seq;
    while (p < tail && *p != '\n')
        p++;

    while (p < tail) {
        const char *line = ++p, *e, *f[7], *fe[7];
        pw_pt_game g;
        unsigned int v[3];
        int ok = 1;

        while (p < tail && *p != '\n')
            p++;
        e = p > line && p[-1] == '\r' ? p - 1 : p;
        if (e <= line || *line == '#' || t->n >= PW_PT_GAMES_MAX)
            continue;
        if (fields(line, e, f, fe, 7) != 7)
            continue;
        memset(&g, 0, sizeof(g));
        copy_field(g.id, sizeof(g.id), f[0], fe[0]);
        for (int i = 0; i < 3; i++) {
            const char *q = f[1 + i];

            ok &= number(&q, fe[1 + i], &v[i]) && q == fe[1 + i];
        }
        ok &= parse_dt(f[4], fe[4], &g.first) && parse_dt(f[5], fe[5], &g.last);
        if (!ok || !g.id[0])
            continue; /* a damaged line costs that game, not the file */
        g.total_s = v[0];
        g.sessions = v[1];
        g.longest_s = v[2];
        copy_field(g.title, sizeof(g.title), f[6], fe[6]);
        if (!pw_pt_find(t, g.id, 0))
            t->g[t->n++] = g;
    }
    return t->n;
}

static int put_dt(char *out, int cap, const pw_datetime *d)
{
    return stbsp_snprintf(out, cap, "%04u-%02u-%02u %02u:%02u:%02u", d->year, d->month, d->day,
                          d->hour, d->minute, d->second);
}

int pw_pt_format(const pw_pt_table *t, char *out, unsigned int cap)
{
    int n = stbsp_snprintf(out, (int)cap, HEAD "%u\n", t->seq);

    for (int i = 0; i < t->n && n > 0 && (unsigned int)n < cap; i++) {
        const pw_pt_game *g = &t->g[i];
        char first[24], last[24], title[sizeof(g->title)];

        put_dt(first, sizeof(first), &g->first);
        put_dt(last, sizeof(last), &g->last);
        for (unsigned int k = 0; k < sizeof(title); k++) /* a tab or newline would split it */
            title[k] = g->title[k] == '\t' || g->title[k] == '\n' || g->title[k] == '\r'
                           ? ' '
                           : g->title[k];
        title[sizeof(title) - 1] = 0;
        n += stbsp_snprintf(out + n, (int)(cap - (unsigned int)n), "%s\t%u\t%u\t%u\t%s\t%s\t%s\n",
                            g->id, g->total_s, g->sessions, g->longest_s, first, last, title);
    }
    if (n <= 0 || (unsigned int)n + 16 > cap)
        return -1;
    n += stbsp_snprintf(out + n, (int)(cap - (unsigned int)n), TAIL "%08X\n",
                        pw_crc32(0, (const unsigned char *)out, (unsigned int)n));
    return (unsigned int)n < cap ? n : -1;
}

pw_pt_game *pw_pt_find(pw_pt_table *t, const char *id, int create)
{
    int victim = 0;

    for (int i = 0; i < t->n; i++)
        if (strcmp(t->g[i].id, id) == 0)
            return &t->g[i];
    if (!create)
        return 0;
    if (t->n < PW_PT_GAMES_MAX) {
        victim = t->n++;
    } else {
        for (int i = 1; i < t->n; i++)
            if (cmp_dt(&t->g[i].last, &t->g[victim].last) < 0)
                victim = i;
    }
    memset(&t->g[victim], 0, sizeof(t->g[victim]));
    strncpy(t->g[victim].id, id, sizeof(t->g[victim].id) - 1);
    return &t->g[victim];
}

int pw_pt_order(const pw_pt_table *t, int *order, int max)
{
    int n = 0;

    for (int i = 0; i < t->n && n < max; i++)
        order[n++] = i;
    for (int i = 1; i < n; i++) { /* insertion sort: a hundred at most */
        int k = order[i], j = i - 1;

        while (j >= 0 && t->g[order[j]].total_s < t->g[k].total_s) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = k;
    }
    return n;
}

void pw_pt_base_of(const pw_pt_game *g, pw_pt_base *b)
{
    memset(b, 0, sizeof(*b));
    if (!g)
        return;
    b->total_s = g->total_s;
    b->sessions = g->sessions;
    b->longest_s = g->longest_s;
    b->first = g->first;
    b->known = 1;
}

void pw_pt_apply(pw_pt_game *g, const pw_pt_base *base, unsigned int session_s,
                 const pw_datetime *start, const pw_datetime *now, const char *title)
{
    g->total_s = base->total_s + session_s;
    g->sessions = base->sessions + 1;
    g->longest_s = session_s > base->longest_s ? session_s : base->longest_s;
    g->first = base->known ? base->first : *start;
    g->last = *now;
    if (title && title[0]) {
        strncpy(g->title, title, sizeof(g->title) - 1);
        g->title[sizeof(g->title) - 1] = 0;
    }
}
