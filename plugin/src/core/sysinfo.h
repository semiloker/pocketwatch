// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SYSINFO_H
#define PW_SYSINFO_H

#include "core/platform.h"

/*
 * What the system UI shows in its corner and how it formats it: the clock in the user's date and
 * time format (registry /CONFIG/DATE), the theme colour (/CONFIG/SYSTEM/XMB/THEME color_mode), the
 * battery. The registry is read once at plugin start (read only), the clock and battery live.
 */

typedef struct {
    int date_format; /* 0 YYYY/MM/DD, 1 MM/DD/YYYY, 2 DD/MM/YYYY */
    int time_format; /* 0 24-hour, 1 12-hour */
    int color_mode;  /* 0 = by month ("Original"), 1..12 = that month's colour */
    int read_ok;
} pw_sysinfo;

void pw_sysinfo_init(void); /* plugin start */
const pw_sysinfo *pw_sysinfo_get(void);

/* The XMB's corner clock ("10/16 14:05", "16/10 2:05 PM", ...). Returns the month 1..12. */
int pw_sysinfo_clock(char *out, unsigned int cap);

/* Battery percent, or -1 without a battery; *charging set when on external power and charging. */
int pw_sysinfo_battery(int *charging);

/* A date and time in the user's format ("2026/10/16 14:05"). */
void pw_sysinfo_format_date(const pw_datetime *t, char *out, unsigned int cap);

/* The sky picture to use: 0..11. */
int pw_sysinfo_sky_index(int month);

#endif
