// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "battcal.h"
#include "pwtest.h"

PW_TEST(battery_calibration_integrates_the_current_and_skips_gaps)
{
    /* 300 mA for an hour from 100 % to 60 %, a sleep of 10 minutes not counted, the charger
       plugged in for a while (not counted), the last sample at 52 % */
    char text[4096];
    int n = 0;
    hm_battcal c;

    n += hm_battcal_start_line(text + n, (int)sizeof(text) - n, 1760950000ULL, 1790, 12);
    for (int i = 0; i <= 120; i++) /* every 30 s: 3600 s */
        n += hm_battcal_sample_line(text + n, (int)sizeof(text) - n, 1760950000ULL + 30u * i,
                                    100 - i / 3, 4100, -300, 1700 - i * 2, 0);
    n += hm_battcal_sample_line(text + n, (int)sizeof(text) - n, 1760950000ULL + 3600 + 600, 59,
                                3950, -300, 1400, 0); /* after a sleep: the gap is left out */
    n += hm_battcal_sample_line(text + n, (int)sizeof(text) - n, 1760950000ULL + 4230, 59, 4000,
                                500, 1400, 1); /* on the charger: left out */
    n += hm_battcal_sample_line(text + n, (int)sizeof(text) - n, 1760950000ULL + 4260, 52, 3900,
                                -32768, 1380, 0); /* no current: the gauge's drop (20 mAh) */
    CHECK_EQ(hm_battcal_parse(text, (unsigned int)n, &c), 0);
    CHECK_EQ(c.full_mah, 1790);
    CHECK_EQ(c.cycles, 12);
    CHECK_EQ(c.started, 1760950000ULL);
    CHECK_EQ(c.pct_from, 100);
    CHECK_EQ(c.pct_to, 52);
    CHECK_EQ(c.seconds, 3600 + 60); /* the hour and the last minute; not the sleep */
    CHECK_EQ(c.mah, 300 + 20);      /* 300 mA x 1 h, then the gauge's 20 mAh */
    CHECK_EQ(c.avg_ma, 320 * 3600 / 3660);
    CHECK_EQ(c.pct_drop, 40 + 7); /* 100 to 60, then 59 to 52: not the 1 % of the sleep */
    CHECK_EQ(c.est_full_mah, 320 * 100 / 47);
    CHECK(!c.complete);
}

PW_TEST(battery_calibration_needs_a_start_and_keeps_its_result)
{
    const char *bad = "1760950000 90 4000 -300 1500 0\n1760950030 90 4000 -300 1500 0\n";
    char line[96];
    hm_battcal c, r;

    CHECK(hm_battcal_parse(bad, (unsigned int)strlen(bad), &c) < 0); /* no start line */
    memset(&c, 0, sizeof(c));
    c.started = 1760950000ULL;
    c.est_full_mah = 1650;
    c.mah = 1600;
    c.seconds = 18000;
    c.pct_from = 100;
    c.pct_to = 3;
    c.full_mah = 1790;
    hm_battcal_result_line(line, sizeof(line), &c);
    CHECK_STR(line, "1760950000 1650 1600 18000 100 3 1790\n");
    CHECK_EQ(hm_battcal_result_parse(line, (unsigned int)strlen(line), &r), 0);
    CHECK_EQ(r.est_full_mah, 1650);
    CHECK_EQ(r.avg_ma, 320);
    CHECK(r.complete);
}
