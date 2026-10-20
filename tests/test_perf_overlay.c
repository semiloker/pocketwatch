// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "overlay.h"
#include "pwtest.h"

PW_TEST(perf_overlay_lines_leave_out_what_is_not_known)
{
    pw_perf_stats s = {60, 43, 333, 166, 12345, 83, -224, 4105, 20, 151};
    char l[2][PW_PERF_COLS + 1];

    CHECK_EQ(pw_perf_lines(&s, PW_PERF_OFF, l), 0);
    CHECK_EQ(pw_perf_lines(&s, PW_PERF_FPS, l), 1);
    CHECK_STR(l[0], "60 FPS");
    CHECK_EQ(pw_perf_lines(&s, PW_PERF_FULL, l), 2);
    CHECK_STR(l[0], "60 FPS  CPU 43%  333/166 MHz  RAM 12345 KB free");
    CHECK_STR(l[1], "BAT 83%  -224 mA  4.10 V  20 C  2:31 left");
    s.fps = -1;
    s.cpu_pct = s.free_kb = s.bat_ma = s.bat_mv = s.bat_temp = s.bat_min = PW_PERF_NA;
    s.cpu_mhz = 0;
    pw_perf_lines(&s, PW_PERF_FULL, l);
    CHECK_STR(l[0], "-- FPS");
    CHECK_STR(l[1], "BAT 83%");
    CHECK_EQ(pw_perf_next(PW_PERF_OFF), PW_PERF_FPS);
    CHECK_EQ(pw_perf_next(PW_PERF_FULL), PW_PERF_OFF);
    CHECK_EQ(pw_perf_parse("mode = fps\n", 11), PW_PERF_FPS);
    CHECK_EQ(pw_perf_parse("mode = full\n", 12), PW_PERF_FULL);
    CHECK_EQ(pw_perf_parse("", 0), PW_PERF_FULL);
}

PW_TEST(perf_overlay_draws_white_letters_in_a_black_outline_in_every_format)
{
    static unsigned int fb32[20 * 16];
    static unsigned short fb16[20 * 16];
    pw_perf_mask m;

    pw_perf_render("|", &m); /* the font's '|': column 3 and 4 lit in rows 0-6 (0x18) */
    CHECK_EQ(m.w, 10);
    memset(fb32, 0x55, sizeof(fb32));
    pw_perf_draw(fb32, 20, 3, 20, 16, 2, 3, &m);
    /* font pixel (3, 0) is mask (4, 1), drawn at (6, 4) */
    CHECK_EQ(fb32[4 * 20 + 6], 0xFFFFFFFFu);
    CHECK_EQ(fb32[4 * 20 + 5], 0xFF000000u); /* its outline */
    CHECK_EQ(fb32[3 * 20 + 6], 0xFF000000u); /* above the first row */
    CHECK_EQ(fb32[4 * 20 + 1], 0x55555555u); /* outside: untouched */
    CHECK_EQ(fb32[15 * 20 + 6], 0x55555555u);
    memset(fb16, 0x55, sizeof(fb16));
    pw_perf_draw(fb16, 20, 1, 20, 16, 2, 3, &m); /* 5551: black keeps its alpha bit */
    CHECK_EQ(fb16[4 * 20 + 6], 0xFFFF);
    CHECK_EQ(fb16[4 * 20 + 5], 0x8000);
    /* clipped at the frame's edge: nothing written outside */
    memset(fb32, 0x55, sizeof(fb32));
    pw_perf_draw(fb32, 20, 3, 6, 16, 2, 3, &m);
    CHECK_EQ(fb32[4 * 20 + 6], 0x55555555u);
}
