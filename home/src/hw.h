// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HW_H
#define HM_HW_H

#include <psptypes.h>

#include "battcal.h"

/* Pocketwatch Home's view of the PSP: the buttons, the clock, the battery, the card. The screen is
   gfx_gu.h's. */

int hm_hw_init(void);             /* controller, HOME, exit callback, the GE */
int hm_hw_home_ours(void);        /* HOME reaches Home (the system's quit dialog is off) */
unsigned int hm_hw_buttons(void); /* HOME included when hm_hw_home_ours */
unsigned int hm_hw_now_ms(void);
int hm_hw_exit_requested(void); /* HOME -> Quit was chosen */

/* "10/18 14:05" in the system's date and time format; the same for a file's time (with the year).
 */
void hm_hw_clock(char *out, int cap, int style); /* hcfg.h HM_CLOCK_* */
/* The screen's backlight off (0) or back as it was (1), through sceDisplay_driver: 0 or < 0. */
int hm_hw_backlight(int on);
void hm_hw_format_time(const ScePspDateTime *t, char *out, int cap);

/* System Information (HOM-REQ-013): "PSP-3000", "6.61", "ARK-4 4.20.69", "00:1F:...". */
void hm_hw_model(char *out, int cap);
void hm_hw_firmware(char *out, int cap);
void hm_hw_cfw(char *out, int cap);
void hm_hw_mac(char *out, int cap);
/* The battery as a thread of hw.c last read it, every second (the power driver's and the syscon's
   calls are slow on the PSP: never in the frame loop). Each -1 when not known; for the main
   thread only. 0, or -1 before the first reading. */
typedef struct {
    int pct, charging, left_min; /* percent, 1 when charging, minutes left (the system's guess) */
    int mv, temp, rcap, full, cycles; /* mV, degrees C, mAh left and full, the gauge's count */
    int online;                       /* external power */
    int ma;                           /* the gauge's current (negative discharging), every 10 s;
                                         PW_BATT_UNKNOWN */
    int est_state, est_min, est_conf; /* ours (core/battmodel.c): PW_BATT_*, minutes left or to
                                         full (-1), PW_BATT_EST_* */
    int avg_ma;                       /* the run's average current, or PW_BATT_UNKNOWN */
    int cal;                          /* HM_CAL_*: the calibration run */
} hm_battery;
int hm_hw_battery(hm_battery *b);

/* Pocketwatch's battery calibration (BAT-REQ-008..010, ADR-042): charge to full, unplug, leave
   Home on until the PSP turns itself off; every 30 s a sample into battery-cal.txt, and at the
   end (the next start after it) the capacity measured into battery.txt. */
enum { HM_CAL_OFF, HM_CAL_CHARGE, HM_CAL_UNPLUG, HM_CAL_RUN };
enum { HM_CAL_NEWS_NONE, HM_CAL_NEWS_DONE, HM_CAL_NEWS_SHORT };
void hm_hw_cal(int start);             /* start (1) or stop (0) */
int hm_hw_cal_result(hm_battcal *out); /* the last one measured: 0, or < 0 none */
int hm_hw_cal_news(void);              /* HM_CAL_NEWS_* once: a run ended */
/* The run going on, else the last one (battery-last.txt): its numbers so far and its charge over
   time (at most max points, oldest first) for the calibration's page (BAT-REQ-011). The points,
   or -1 when there is no run at all. */
#define HM_CAL_PTS 120
int hm_hw_cal_live(hm_battcal *c, unsigned char *pct, int max);

/* What the volume and brightness indicator shows (HOM-REQ-034): impose's volume (0-30), mute
   (0 / 1) and brightness (the SCREEN button's step, 0-3), and the backlight's level (0-100);
   each -1 when not known. Fast (a kernel call each): for the frame loop. */
typedef struct {
    int vol, mute, bright, level;
} hm_hw_av;
void hm_hw_av_read(hm_hw_av *out);
int hm_hw_set_level(int level); /* the backlight's level (1-100): 0, or < 0 */

/* The clock (HOM-REQ-031, ADR-041): lost (a battery left flat) - put back to the newest time of
   Pocketwatch's files - at the start; set from the internet's time (net.c). */
enum { HM_CLOCK_OK, HM_CLOCK_RESTORED, HM_CLOCK_LOST };
int hm_hw_clock_check(void);                   /* HM_CLOCK_*; logged */
int hm_hw_set_utc(unsigned long long unix_us); /* 0 when the clock reads it back */
/* What the first reading's calls cost ("percent 12 us, ..."), for home.log; 0 until then. */
int hm_hw_battery_cost(char *out, int cap);
int hm_hw_cross_enter(void); /* the system's Enter button is X */
/* The XMB's sky to draw (0-11): its theme colour, else the month's; *colour what the registry
   said (-1 not read). */
int hm_hw_sky_index(int *colour);

/* The system's parameters (PSP_SYSTEMPARAM_ID_INT_*): read once, then kept - nothing but Home's
   Settings changes them while Home runs, through hm_hw_set_param (written, read back). The value,
   or -1 when not known; set: 0 when the value read back is v. */
int hm_hw_param(int id);
int hm_hw_set_param(int id, int v);

/* The marker files the XMB plugin reads at the next start (ADR-026): the RTC tick now. */
void hm_hw_touch(const char *path);
void hm_hw_remove(const char *path);
int hm_hw_language(void);         /* PSP_SYSTEMPARAM_LANGUAGE_* */
unsigned int hm_hw_free_mb(void); /* free space on ms0:, MB */

int hm_hw_read_file(const char *path, void *buf, unsigned int cap); /* bytes or < 0 */
/* A line (with its newline) at the end of seplugins/pocketwatch/home.log; from any thread.
   Dropped while the card is lent to the PC (hm_hw_card_lent: USB). */
void hm_hw_log(const char *line);
void hm_hw_card_lent(int lent);

#endif
