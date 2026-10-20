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
}

PW_TEST(perf_cfg_reads_the_mode_the_indicator_and_the_boost)
{
    static const char t[] = "mode = off\r\nosd = all\nboost = 50\n";
    pw_perf_cfg c;

    pw_perf_cfg_parse(t, (int)sizeof(t) - 1, &c);
    CHECK_EQ(c.mode, PW_PERF_OFF);
    CHECK_EQ(c.osd, PW_OSD_ALL);
    CHECK_EQ(c.boost, 50);
    pw_perf_cfg_parse("mode = fps\n", 11, &c); /* (the file of before: the rest as before) */
    CHECK_EQ(c.mode, PW_PERF_FPS);
    CHECK_EQ(c.osd, PW_OSD_HOME);
    CHECK_EQ(c.boost, 0);
    pw_perf_cfg_parse("mode = full\nosd = off\nboost = 300\nmodes = off\n", 46, &c);
    CHECK_EQ(c.mode, PW_PERF_FULL);
    CHECK_EQ(c.osd, PW_OSD_OFF);
    CHECK_EQ(c.boost, 0); /* (out of range) */
    pw_perf_cfg_parse("", 0, &c);
    CHECK_EQ(c.mode, PW_PERF_FULL);
}

PW_TEST(brightness_boost_follows_the_system_and_gives_up_on_a_fight)
{
    pw_bright b;

    CHECK_EQ(pw_bright_boost(60, 50), 80);
    CHECK_EQ(pw_bright_boost(60, 100), 100);
    CHECK_EQ(pw_bright_boost(0, 100), 0); /* the screen off stays off */
    CHECK_EQ(pw_bright_boost(72, 0), 72);
    pw_bright_init(&b);
    CHECK_EQ(pw_bright_step(&b, 60, 50, 1000), 80);   /* the system's 60: ours 80 */
    CHECK_EQ(pw_bright_step(&b, 80, 50, 1100), -1);   /* ours holds */
    CHECK_EQ(pw_bright_step(&b, 44, 50, 5000), 72);   /* SCREEN: the next step, boosted */
    CHECK_EQ(pw_bright_step(&b, 0, 50, 9000), -1);    /* the screen off: left off */
    CHECK_EQ(pw_bright_step(&b, 44, 50, 9500), 72);   /* and on again */
    CHECK_EQ(pw_bright_step(&b, 72, 100, 9600), 100); /* the boost raised */
    CHECK_EQ(pw_bright_step(&b, 100, 0, 9700), 44);   /* and off: the system's level back */
    CHECK_EQ(pw_bright_step(&b, 44, 0, 9800), -1);
    CHECK_EQ(b.fights, 0);
    /* another plugin putting its level back at once, again and again: given up */
    pw_bright_init(&b);
    for (int i = 0; i < 2 * PW_BRIGHT_FIGHTS; i++)
        pw_bright_step(&b, 60, 50, 20000 + 100u * i);
    CHECK(b.fights >= PW_BRIGHT_FIGHTS);
    CHECK_EQ(pw_bright_step(&b, 60, 50, 30000), -1);
}

PW_TEST(indicator_shows_what_changed_for_a_while)
{
    pw_osd o;
    char t[PW_PERF_COLS + 1];

    memset(&o, 0, sizeof(o));
    CHECK_EQ(pw_osd_step(&o, 18, 0, 2, 100), 0); /* the first reading: only remembered */
    CHECK_EQ(pw_osd_shown(&o, 200), PW_OSD_NONE);
    CHECK_EQ(pw_osd_step(&o, 19, 0, 2, 1000), 1);
    CHECK_EQ(pw_osd_shown(&o, 1000 + PW_OSD_MS - 1), PW_OSD_VOLUME);
    CHECK_EQ(pw_osd_shown(&o, 1000 + PW_OSD_MS), PW_OSD_NONE);
    pw_osd_text(&o, 72, t);
    CHECK_STR(t, "VOL 19");
    CHECK_EQ(pw_osd_step(&o, 19, 1, 2, 3000), 1);
    pw_osd_text(&o, 72, t);
    CHECK_STR(t, "MUTE");
    CHECK_EQ(pw_osd_step(&o, 19, 1, 3, 5000), 1);
    CHECK_EQ(pw_osd_shown(&o, 5100), PW_OSD_BRIGHT);
    pw_osd_text(&o, 92, t);
    CHECK_STR(t, "BRIGHT 4/4 92%");
    CHECK_EQ(pw_osd_step(&o, 19, 1, 3, 5200), 0);
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

PW_TEST(indicator_box_is_drawn_inside_the_frame)
{
    static unsigned int fb[480 * 272];
    pw_perf_mask m;
    int x0 = (480 - PW_OSD_W) / 2, y0 = 272 - PW_OSD_H - 16;

    pw_perf_render("VOL 18", &m);
    memset(fb, 0x55, sizeof(fb));
    pw_osd_draw(fb, 480, 3, 480, 272, &m, 18, 30);
    CHECK_EQ(fb[y0 * 480 + x0], 0xFFFFFFFFu);              /* the edge */
    CHECK_EQ(fb[(y0 + 2) * 480 + x0 + 2], 0xFF000000u);    /* inside */
    CHECK_EQ(fb[(y0 - 1) * 480 + x0], 0x55555555u);        /* above it: untouched */
    CHECK_EQ(fb[(y0 + 10) * 480 + x0 + 265], 0xFF404040u); /* the last step: not lit */
}
