// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_BATTMODEL_H
#define PW_BATTMODEL_H

/*
 * The battery service's model (BAT-REQ-001..006, docs/subsystems/battery.md): what the system
 * reports, checked for plausibility, a short history, and estimates that say how much they can be
 * trusted. Values the system did not give (or gave out of range) are PW_BATT_UNKNOWN, never 0.
 * Portable: core/battery.c reads the PSP and feeds it.
 */

#define PW_BATT_UNKNOWN (-32768)
#define PW_BATT_HISTORY 64 /* samples kept (one every 15 s: 16 minutes) */

typedef struct {
    unsigned int t_ms; /* system time of the reading */
    int percent;       /* 0..100 (scePower) */
    int mv;            /* battery voltage, mV (syscon) */
    int ma;            /* current, mA as syscon gives it (its sign is not trusted) */
    int temp_c;        /* battery temperature, degrees C */
    int rcap_mah;      /* remaining capacity, mAh (scePower; syscon's RCap is another unit) */
    int full_mah;      /* capacity when full now, mAh */
    int cycles;        /* charge cycles the battery counted */
    int fw_minutes;    /* the system's own estimate of the time left */
    int present;       /* 1 battery in, 0 none, PW_BATT_UNKNOWN */
    int charging;      /* 1 / 0 / PW_BATT_UNKNOWN */
    int online;        /* external power: 1 / 0 / PW_BATT_UNKNOWN */
} pw_batt_sample;

/* Out-of-range values become PW_BATT_UNKNOWN (a misread is not shown as a measurement). */
void pw_batt_validate(pw_batt_sample *s);

/* The samples of the current run: a change of charging or of external power, a gap of more than
   five minutes or time running backwards (a state was loaded) starts a new run. */
typedef struct {
    pw_batt_sample s[PW_BATT_HISTORY];
    int n, head; /* head: where the next one goes */
} pw_batt_history;

void pw_batt_history_reset(pw_batt_history *h);
void pw_batt_history_add(pw_batt_history *h, const pw_batt_sample *s);
const pw_batt_sample *pw_batt_history_last(const pw_batt_history *h); /* NULL when empty */

enum {
    PW_BATT_NONE,        /* no battery */
    PW_BATT_DISCHARGING, /* on the battery */
    PW_BATT_CHARGING,
    PW_BATT_FULL, /* on external power, not charging */
    PW_BATT_STATE_UNKNOWN,
};

enum {
    PW_BATT_EST_NONE,   /* not enough history yet: "Estimating..." */
    PW_BATT_EST_SYSTEM, /* the system's own figure (no history of ours yet) */
    PW_BATT_EST_ROUGH,  /* a few minutes of history */
    PW_BATT_EST_GOOD,   /* steady current over five minutes or more */
};

typedef struct {
    int state;      /* PW_BATT_* */
    int minutes;    /* left (discharging) or to full (charging), or -1 */
    int confidence; /* PW_BATT_EST_* */
    int avg_ma;     /* average current of the run (absolute), or PW_BATT_UNKNOWN */
    int span_s;     /* seconds of history the estimate is based on */
} pw_batt_estimate;

void pw_batt_estimate_now(const pw_batt_history *h, pw_batt_estimate *e); /* h may be NULL */
int pw_batt_state(const pw_batt_sample *s);                               /* PW_BATT_* */

#endif
