// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_BATTCAL_H
#define HM_BATTCAL_H

/*
 * Pocketwatch's own battery calibration (BAT-REQ-008..010, ADR-042): the battery's gauge says
 * what it thinks the capacity is; a run from full to the PSP turning itself off, with the current
 * measured every 30 s, says what the battery really gives. The run is a text file of samples
 * (battery-cal.txt), so it survives the PSP turning off at its end; this reads it. Portable.
 *
 *   start <unix s> <full mAh> <cycles>
 *   <unix s> <percent> <mV> <mA> <remaining mAh> <external power 0/1>
 */

#define HM_BATTCAL_GAP_S 120 /* a longer gap between samples is not counted (the PSP slept) */

typedef struct {
    int samples;                /* discharging samples read */
    unsigned int seconds;       /* the time they cover (gaps left out) */
    int mah;                    /* drawn: the current integrated over that time */
    int pct_from, pct_to;       /* the charge at the first and the last sample */
    int pct_drop;               /* the charge lost over the counted time (not over the gaps) */
    int full_mah;               /* the gauge's full capacity when the run started (-1 not known) */
    int cycles;                 /* the gauge's cycle count then (-1) */
    int avg_ma;                 /* mah over seconds */
    int est_full_mah;           /* mah scaled to 100 % by pct_drop (30 or more), or -1 */
    int complete;               /* from 95 % or more down to 10 % or less: a whole run */
    unsigned long long started; /* Unix seconds */
} hm_battcal;

/* 0, or < 0 when the text is not a run (no start line, no two discharging samples). */
int hm_battcal_parse(const char *text, unsigned int len, hm_battcal *out);

/* The run's charge over its time for a chart: the discharging samples' percent, at most max of
   them (picked evenly, the last one always), oldest first. Returns how many. */
int hm_battcal_curve(const char *text, unsigned int len, unsigned char *pct, int max);

/* One line of each kind, for the writer (hw.c): bytes written. */
int hm_battcal_start_line(char *out, int cap, unsigned long long unix_s, int full_mah, int cycles);
int hm_battcal_sample_line(char *out, int cap, unsigned long long unix_s, int pct, int mv, int ma,
                           int rcap, int online);

/* The result kept (battery.txt): "<unix s> <est full mAh> <mAh> <seconds> <from> <to> <full>". */
int hm_battcal_result_line(char *out, int cap, const hm_battcal *c);
int hm_battcal_result_parse(const char *text, unsigned int len, hm_battcal *out);

#endif
