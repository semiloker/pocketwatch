// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HW_H
#define HM_HW_H

#include <psptypes.h>

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
} hm_battery;
int hm_hw_battery(hm_battery *b);
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

#endif
