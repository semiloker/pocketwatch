// SPDX-License-Identifier: GPL-3.0-or-later
#include "battcal.h"

#include <string.h>

#include "stb_sprintf.h"

#define UNKNOWN_MA (-32768) /* PW_BATT_UNKNOWN: the current was not read */

/* A number at *p (spaces before it skipped); 0 when there is none. */
static int number(const char **p, const char *end, long long *v)
{
    const char *s = *p;
    int neg = 0, digits = 0;

    while (s < end && *s == ' ')
        s++;
    if (s < end && *s == '-')
        neg = 1, s++;
    for (*v = 0; s < end && *s >= '0' && *s <= '9'; s++, digits++)
        *v = *v * 10 + (*s - '0');
    if (neg)
        *v = -*v;
    *p = s;
    return digits > 0;
}

static const char *line_end(const char *p, const char *end)
{
    while (p < end && *p != '\n')
        p++;
    return p;
}

typedef struct {
    long long t;
    int pct, mv, ma, rcap, online;
} sample;

static int read_sample(const char *p, const char *end, sample *s)
{
    long long v[6];

    for (int i = 0; i < 6; i++)
        if (!number(&p, end, &v[i]))
            return 0;
    s->t = v[0];
    s->pct = (int)v[1];
    s->mv = (int)v[2];
    s->ma = (int)v[3];
    s->rcap = (int)v[4];
    s->online = (int)v[5];
    return 1;
}

int hm_battcal_parse(const char *text, unsigned int len, hm_battcal *out)
{
    const char *p = text, *end = text + len;
    long long mas = 0, extra_mah = 0; /* mA x s from the current; mAh from the gauge's drop */
    sample last = {0, 0, 0, 0, 0, 0};
    int have_last = 0, started = 0;

    memset(out, 0, sizeof(*out));
    out->full_mah = out->cycles = out->est_full_mah = -1;
    while (p < end) {
        const char *e = line_end(p, end);
        sample s;

        if (e - p >= 6 && memcmp(p, "start ", 6) == 0) {
            const char *q = p + 6;
            long long t, full, cycles;

            if (number(&q, e, &t) && number(&q, e, &full) && number(&q, e, &cycles)) {
                out->started = (unsigned long long)t;
                out->full_mah = (int)full;
                out->cycles = (int)cycles;
                started = 1;
            }
        } else if (started && read_sample(p, e, &s) && !s.online && s.pct >= 0 && s.pct <= 100) {
            if (have_last) {
                long long dt = s.t - last.t;

                if (dt > 0 && dt <= HM_BATTCAL_GAP_S) {
                    if (s.ma != UNKNOWN_MA && last.ma != UNKNOWN_MA && (s.ma || last.ma)) {
                        int a = last.ma < 0 ? -last.ma : last.ma, b = s.ma < 0 ? -s.ma : s.ma;

                        mas += (long long)(a + b) * dt / 2;
                    } else if (s.rcap >= 0 && last.rcap > s.rcap) {
                        extra_mah += last.rcap - s.rcap;
                    }
                    out->seconds += (unsigned int)dt;
                    out->pct_drop += last.pct - s.pct;
                }
            } else {
                out->pct_from = s.pct;
            }
            out->pct_to = s.pct;
            out->samples++;
            last = s;
            have_last = 1;
        }
        p = e + 1;
    }
    if (!started || out->samples < 2)
        return -1;
    out->mah = (int)(mas / 3600 + extra_mah);
    out->avg_ma = out->seconds ? (int)((long long)out->mah * 3600 / out->seconds) : 0;
    if (out->pct_drop >= 30)
        out->est_full_mah = out->mah * 100 / out->pct_drop;
    out->complete = out->pct_from >= 95 && out->pct_to <= 10;
    return 0;
}

int hm_battcal_start_line(char *out, int cap, unsigned long long unix_s, int full_mah, int cycles)
{
    return stbsp_snprintf(out, cap, "start %llu %d %d\n", unix_s, full_mah, cycles);
}

int hm_battcal_sample_line(char *out, int cap, unsigned long long unix_s, int pct, int mv, int ma,
                           int rcap, int online)
{
    return stbsp_snprintf(out, cap, "%llu %d %d %d %d %d\n", unix_s, pct, mv, ma, rcap, online);
}

int hm_battcal_result_line(char *out, int cap, const hm_battcal *c)
{
    return stbsp_snprintf(out, cap, "%llu %d %d %u %d %d %d\n", c->started, c->est_full_mah, c->mah,
                          c->seconds, c->pct_from, c->pct_to, c->full_mah);
}

int hm_battcal_result_parse(const char *text, unsigned int len, hm_battcal *out)
{
    const char *p = text, *end = text + len;
    long long v[7];

    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 7; i++)
        if (!number(&p, end, &v[i]))
            return -1;
    out->started = (unsigned long long)v[0];
    out->est_full_mah = (int)v[1];
    out->mah = (int)v[2];
    out->seconds = (unsigned int)v[3];
    out->pct_from = (int)v[4];
    out->pct_to = (int)v[5];
    out->full_mah = (int)v[6];
    out->cycles = -1;
    out->avg_ma = out->seconds ? (int)((long long)out->mah * 3600 / out->seconds) : 0;
    out->pct_drop = out->pct_from - out->pct_to;
    out->complete = out->pct_from >= 95 && out->pct_to <= 10;
    out->samples = 2;
    return 0;
}
