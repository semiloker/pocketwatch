// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "overlay.h"
#include "pwtest.h"

PW_TEST(perf_overlay_rows_are_a_labelled_card_and_leave_out_the_unknown)
{
    pw_perf_stats s = {60, 43, 333, 166, 12345, 83, -224, 4105, 20, 151};
    char l[PW_PERF_ROWS][PW_PERF_COLS + 1];

    CHECK_EQ(pw_perf_rows(&s, PW_PERF_OFF, l), 0);
    CHECK_EQ(pw_perf_rows(&s, PW_PERF_FPS, l), 1);
    CHECK_STR(l[0], "60 FPS");
    CHECK_EQ(pw_perf_rows(&s, PW_PERF_FULL, l), 6); /* a short row each */
    CHECK_STR(l[0], "60 FPS");
    CHECK_STR(l[1], "CPU 43%");
    CHECK_STR(l[2], "333 MHz");
    CHECK_STR(l[3], "RAM 12.0M");
    CHECK_STR(l[4], "BAT 83% -224");
    CHECK_STR(l[5], "4.10V 20C");
    s.fps = -1;
    s.cpu_pct = s.free_kb = s.bat_ma = s.bat_mv = s.bat_temp = s.bat_min = PW_PERF_NA;
    s.cpu_mhz = 0;
    CHECK_EQ(pw_perf_rows(&s, PW_PERF_FULL, l), 2); /* only FPS (unknown) and BAT% left */
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
    pw_perf_cfg_parse("osd = everywhere\n", 17, &c); /* the XMB too (ADR-059) */
    CHECK_EQ(c.osd, PW_OSD_EVERY);
}

PW_TEST(brightness_boost_follows_the_system_and_gives_up_on_a_fight)
{
    pw_bright b;

    CHECK_EQ(pw_bright_boost(60, 50), 80);
    CHECK_EQ(pw_bright_boost(60, 100), 100);
    CHECK_EQ(pw_bright_boost(0, 100), 0); /* the screen off stays off */
    CHECK_EQ(pw_bright_boost(72, 0), 72);
    pw_bright_init(&b);
    CHECK_EQ(pw_bright_step(&b, 60, 50, 0, 0, 1000), 80);   /* the system's 60: ours 80 */
    CHECK_EQ(pw_bright_step(&b, 80, 50, 0, 0, 1100), -1);   /* ours holds */
    CHECK_EQ(pw_bright_step(&b, 44, 50, 0, 0, 5000), 72);   /* SCREEN: the next step, boosted */
    CHECK_EQ(pw_bright_step(&b, 0, 50, 0, 0, 9000), -1);    /* the screen off: left off */
    CHECK_EQ(pw_bright_step(&b, 44, 50, 0, 0, 9500), 72);   /* and on again */
    CHECK_EQ(pw_bright_step(&b, 72, 100, 0, 0, 9600), 100); /* the boost raised */
    CHECK_EQ(pw_bright_step(&b, 100, 0, 0, 0, 9700), 44);   /* and off: the system's level back */
    CHECK_EQ(pw_bright_step(&b, 44, 0, 0, 0, 9800), -1);
    CHECK_EQ(b.fights, 0);
    /* another plugin putting its level back at once, again and again: given up */
    pw_bright_init(&b);
    for (int i = 0; i < 2 * PW_BRIGHT_FIGHTS; i++)
        pw_bright_step(&b, 60, 50, 0, 0, 20000 + 100u * i);
    CHECK(b.fights >= PW_BRIGHT_FIGHTS);
    CHECK_EQ(pw_bright_step(&b, 60, 50, 0, 0, 30000), -1);
    /* the same put-backs while the owner presses SCREEN: no fight */
    pw_bright_init(&b);
    for (int i = 0; i < 2 * PW_BRIGHT_FIGHTS; i++)
        pw_bright_step(&b, 60, 50, 0, 1, 20000 + 100u * i);
    CHECK_EQ(b.fights, 0);
}

PW_TEST(brightness_steps_are_pocketwatchs_own_levels)
{
    static const char t[] = "steps = on\nlevels = 15 35 55 75 100\n";
    pw_perf_cfg c;
    pw_bright b;
    pw_steps st = {0, 0, 0};

    pw_perf_cfg_parse(t, (int)sizeof(t) - 1, &c);
    CHECK_EQ(c.steps, 1);
    CHECK_EQ(c.nlevels, 5);
    CHECK_EQ(c.levels[4], 100);
    pw_perf_cfg_parse("levels = 50 40\n", 15, &c); /* (not rising: the default ones) */
    CHECK_EQ(c.steps, 0);
    CHECK_EQ(c.nlevels, 8);
    CHECK_EQ(c.levels[0], 20);
    CHECK_EQ(pw_steps_nearest(&c, 72), 5); /* 70 */
    /* a tap: the next step on release; a long press (the screen off): none */
    CHECK_EQ(pw_steps_button(&st, 1, 8, 1000), 0);
    CHECK_EQ(pw_steps_button(&st, 0, 8, 1200), 1);
    CHECK_EQ(st.cur, 1);
    pw_steps_button(&st, 1, 8, 2000);
    CHECK_EQ(pw_steps_button(&st, 0, 8, 2000 + PW_STEPS_TAP_MS), 0);
    st.cur = 7;
    pw_steps_button(&st, 1, 8, 3000);
    pw_steps_button(&st, 0, 8, 3100);
    CHECK_EQ(st.cur, 0); /* round */
    /* the step's level whatever the system sets; the screen off stays off */
    pw_bright_init(&b);
    CHECK_EQ(pw_bright_step(&b, 72, 0, 85, 0, 1000), 85);
    CHECK_EQ(pw_bright_step(&b, 85, 0, 100, 1, 1100), 100); /* the next step */
    CHECK_EQ(pw_bright_step(&b, 44, 0, 100, 1, 1200), 100); /* impose's own: ours again */
    CHECK_EQ(pw_bright_step(&b, 0, 0, 100, 0, 5000), -1);
    CHECK_EQ(pw_bright_step(&b, 100, 0, 0, 0, 6000), -1); /* steps off: the system's */
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
    pw_osd_text(&o, t);
    CHECK_STR(t, "VOL 19");
    CHECK_EQ(pw_osd_step(&o, 19, 1, 2, 3000), 1);
    pw_osd_text(&o, t);
    CHECK_STR(t, "MUTE");
    CHECK_EQ(pw_osd_step(&o, 19, 1, 3, 5000), 1);
    CHECK_EQ(pw_osd_shown(&o, 5100), PW_OSD_BRIGHT);
    pw_osd_text(&o, t);
    CHECK_STR(t, "3"); /* the step (#11) as given */
    CHECK_EQ(pw_osd_step(&o, 19, 1, 3, 5200), 0);
    CHECK_EQ(pw_osd_step(&o, 19, 1, 5, 5300), 1); /* the fifth step (#6) */
    pw_osd_text(&o, t);
    CHECK_STR(t, "5");
}

PW_TEST(brightness_bar_step_is_the_systems_or_pocketwatchs)
{
    static const char t[] = "steps = on\nlevels = 28 40 50 60 70 80 90 100\n";
    pw_perf_cfg c;
    int n = 0;

    pw_perf_cfg_parse("", 0, &c); /* the system's five (#11): 44 is the first of 5 */
    CHECK_EQ(pw_osd_bright(&c, 44, &n), 1);
    CHECK_EQ(n, PW_BRIGHT_STEPS);
    CHECK_EQ(pw_osd_bright(&c, 84, &n), 4);
    CHECK_EQ(pw_osd_bright(&c, 100, &n), 5);
    pw_perf_cfg_parse(t, (int)sizeof(t) - 1, &c); /* Own Steps: of its levels */
    CHECK_EQ(pw_osd_bright(&c, 28, &n), 1);
    CHECK_EQ(n, 8);
    CHECK_EQ(pw_osd_bright(&c, 70, &n), 5);
    CHECK_EQ(pw_osd_bright(&c, 100, &n), 8);
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

PW_TEST(brightness_bar_is_vertical_at_the_left_lit_from_the_bottom)
{
    static unsigned int fb[480 * 272];
    pw_perf_mask m;
    int x0 = PW_VBAR_X, y0 = (272 - PW_VBAR_H) / 2, cx = x0 + PW_VBAR_W / 2, lit = 0, grey = 0;

    pw_perf_render("2", &m);
    memset(fb, 0x55, sizeof(fb));
    pw_osd_draw_vbar(fb, 480, 3, 480, 272, &m, 2, 5);   /* step 2 of the SCREEN button's 5 */
    CHECK_EQ(fb[y0 * 480 + x0], 0xFFFFFFFFu);           /* the edge */
    CHECK_EQ(fb[(y0 + 2) * 480 + x0 + 2], 0xFF000000u); /* inside */
    CHECK_EQ(fb[(y0 - 1) * 480 + x0], 0x55555555u);     /* above: untouched */
    CHECK_EQ(fb[(y0 + 20) * 480 + x0 + PW_VBAR_W], 0x55555555u); /* right of it: untouched */
    CHECK_EQ(fb[(y0 + PW_VBAR_H - 14) * 480 + cx], 0xFFFFFFFFu); /* the sun's middle */
    for (int y = y0; y < y0 + PW_VBAR_H; y++) {                  /* the steps' column */
        unsigned int p = fb[y * 480 + cx - 4];

        lit += p == 0xFFFFFFFFu;
        grey += p == 0xFF404040u;
    }
    CHECK(lit > 0 && grey > lit); /* two of five lit, three not */
    /* the lowest step lit, the highest not */
    {
        int bot = y0 + PW_VBAR_H - 8 - 13 - 6 - 1, top = y0 + 6 + PW_PERF_H + 6 + 2;

        CHECK_EQ(fb[bot * 480 + cx - 4], 0xFFFFFFFFu);
        CHECK_EQ(fb[top * 480 + cx - 4], 0xFF404040u);
    }
}

PW_TEST(fbset_keeps_distinct_buffers_and_drops_the_oldest)
{
    pw_fbset s;
    int has_old = 0, has_new = 0;

    memset(&s, 0, sizeof(s));
    pw_fbset_add(&s, 0x04000000, 512, 3);
    pw_fbset_add(&s, 0x04088000, 512, 3);
    pw_fbset_add(&s, 0x04000000, 512, 3); /* the same one again: not counted twice */
    CHECK_EQ(s.n, 2);
    pw_fbset_add(&s, 0, 512, 3); /* a null address is ignored */
    CHECK_EQ(s.n, 2);
    pw_fbset_add(&s, 0x1, 480, 0);
    pw_fbset_add(&s, 0x2, 480, 0);
    CHECK_EQ(s.n, PW_FB_MAX);      /* four distinct */
    pw_fbset_add(&s, 0x3, 480, 0); /* a fifth: the oldest (0x04000000) drops */
    CHECK_EQ(s.n, PW_FB_MAX);
    for (int i = 0; i < s.n; i++) {
        if (s.fb[i] == 0x04000000)
            has_old = 1;
        if (s.fb[i] == 0x3)
            has_new = 1;
    }
    CHECK_EQ(has_old, 0);
    CHECK_EQ(has_new, 1);
    pw_fbset_add(&s, 0x2, 1024, 1); /* an address already kept: its width/format refreshed */
    for (int i = 0; i < s.n; i++)
        if (s.fb[i] == 0x2) {
            CHECK_EQ(s.w[i], 1024);
            CHECK_EQ(s.f[i], 1);
        }
}
