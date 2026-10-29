// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/battmodel.h"

#define U PW_BATT_UNKNOWN

#define GAP_MS        (5 * 60 * 1000)
#define MIN_SPAN_S    60  /* a rough estimate needs a minute of history */
#define GOOD_SPAN_S   300 /* a good one five */
#define MIN_MA        30  /* below this the current says nothing (resting, or a misread) */
#define SLOPE_SPAN_S  600 /* without a current: ten minutes and two points of charge */
#define SLOPE_MIN_PCT 2
#define CAP_SPAN_S    120 /* without a current: two minutes and 5 mAh of capacity */
#define CAP_MIN_MAH   5

static int in(int v, int lo, int hi)
{
    return v != U && v >= lo && v <= hi ? v : U;
}

static int flag(int v)
{
    return v == 0 || v == 1 ? v : U;
}

void pw_batt_validate(pw_batt_sample *s)
{
    /* Ranges a PSP battery can have: the pack is a 3.6 V Li-ion cell (charged ~4.2 V, cut off
       above 3.0 V), 1200-2200 mAh packs exist; anything else is a misread. */
    s->percent = in(s->percent, 0, 100);
    s->mv = in(s->mv, 2500, 4500);
    s->ma = in(s->ma, -4000, 4000);
    s->temp_c = in(s->temp_c, -20, 80);
    s->full_mah = in(s->full_mah, 100, 6000);
    s->rcap_mah = in(s->rcap_mah, 0, 6000);
    if (s->rcap_mah != U && s->full_mah != U && s->rcap_mah > s->full_mah + 50)
        s->rcap_mah = U;
    s->cycles = in(s->cycles, 0, 20000);
    s->fw_minutes = in(s->fw_minutes, 0, 60 * 24);
    s->present = flag(s->present);
    s->charging = flag(s->charging);
    s->online = flag(s->online);
    if (s->present == 0) { /* no battery: nothing of it is a measurement */
        s->percent = s->mv = s->ma = s->temp_c = s->rcap_mah = s->full_mah = s->cycles = U;
        s->fw_minutes = U;
    }
}

void pw_batt_history_reset(pw_batt_history *h)
{
    h->n = h->head = 0;
}

const pw_batt_sample *pw_batt_history_last(const pw_batt_history *h)
{
    return h && h->n ? &h->s[(h->head + PW_BATT_HISTORY - 1) % PW_BATT_HISTORY] : 0;
}

/* The i-th sample of the run, oldest first. */
static const pw_batt_sample *at(const pw_batt_history *h, int i)
{
    return &h->s[(h->head + PW_BATT_HISTORY - h->n + i) % PW_BATT_HISTORY];
}

void pw_batt_history_add(pw_batt_history *h, const pw_batt_sample *s)
{
    const pw_batt_sample *last = pw_batt_history_last(h);

    if (last && (s->charging != last->charging || s->online != last->online ||
                 (int)(s->t_ms - last->t_ms) < 0 || s->t_ms - last->t_ms > GAP_MS))
        pw_batt_history_reset(h); /* a new run */
    h->s[h->head] = *s;
    h->head = (h->head + 1) % PW_BATT_HISTORY;
    if (h->n < PW_BATT_HISTORY)
        h->n++;
}

int pw_batt_state(const pw_batt_sample *s)
{
    if (s->present == 0)
        return PW_BATT_NONE;
    if (s->charging == 1)
        return PW_BATT_CHARGING;
    if (s->online == 1)
        return PW_BATT_FULL;
    if (s->online == 0)
        return PW_BATT_DISCHARGING;
    return PW_BATT_STATE_UNKNOWN;
}

/* Average absolute current of the run's samples that have one; count: how many. */
static int average_ma(const pw_batt_history *h, int *count)
{
    long sum = 0;
    int n = 0;

    for (int i = 0; i < h->n; i++) {
        int ma = at(h, i)->ma;

        if (ma != U) {
            sum += ma < 0 ? -ma : ma;
            n++;
        }
    }
    *count = n;
    return n ? (int)(sum / n) : U;
}

void pw_batt_estimate_now(const pw_batt_history *h, pw_batt_estimate *e)
{
    const pw_batt_sample *last = pw_batt_history_last(h), *first = last ? at(h, 0) : 0;
    int with_ma = 0;

    e->state = last ? pw_batt_state(last) : PW_BATT_STATE_UNKNOWN;
    e->minutes = -1;
    e->confidence = PW_BATT_EST_NONE;
    e->avg_ma = U;
    e->span_s = last ? (int)((last->t_ms - first->t_ms) / 1000) : 0;
    if (!last || (e->state != PW_BATT_DISCHARGING && e->state != PW_BATT_CHARGING))
        return;
    e->avg_ma = average_ma(h, &with_ma);

    if (e->state == PW_BATT_CHARGING) {
        /* the rest of the capacity at the current rate: optimistic near the end (the charger
           slows down), so never better than rough */
        if (e->span_s >= MIN_SPAN_S && with_ma >= 3 && e->avg_ma != U && e->avg_ma >= MIN_MA &&
            last->rcap_mah != U && last->full_mah != U && last->full_mah >= last->rcap_mah) {
            e->minutes = (last->full_mah - last->rcap_mah) * 60 / e->avg_ma;
            e->confidence = PW_BATT_EST_ROUGH;
        }
        return;
    }

    /* On the battery: what is left at the run's average current... */
    if (e->span_s >= MIN_SPAN_S && with_ma >= 3 && e->avg_ma != U && e->avg_ma >= MIN_MA &&
        last->rcap_mah != U) {
        e->minutes = last->rcap_mah * 60 / e->avg_ma;
        e->confidence = e->span_s >= GOOD_SPAN_S ? PW_BATT_EST_GOOD : PW_BATT_EST_ROUGH;
        return;
    }
    /* ...or what the remaining capacity lost over two minutes (no current reading)... */
    if (e->span_s >= CAP_SPAN_S && first->rcap_mah != U && last->rcap_mah != U &&
        first->rcap_mah - last->rcap_mah >= CAP_MIN_MAH) {
        e->minutes = last->rcap_mah * (e->span_s / 60) / (first->rcap_mah - last->rcap_mah);
        e->avg_ma = (first->rcap_mah - last->rcap_mah) * 3600 / e->span_s;
        e->confidence = e->span_s >= GOOD_SPAN_S ? PW_BATT_EST_GOOD : PW_BATT_EST_ROUGH;
        return;
    }
    /* ...or the slope of the percentage over ten minutes... */
    if (e->span_s >= SLOPE_SPAN_S && first->percent != U && last->percent != U &&
        first->percent - last->percent >= SLOPE_MIN_PCT) {
        e->minutes = last->percent * (e->span_s / 60) / (first->percent - last->percent);
        e->confidence = PW_BATT_EST_ROUGH;
        return;
    }
    /* ...or, until there is enough of ours, the system's figure. */
    if (last->fw_minutes != U) {
        e->minutes = last->fw_minutes;
        e->confidence = PW_BATT_EST_SYSTEM;
    }
}
