// SPDX-License-Identifier: GPL-3.0-or-later
#include "pwtest.h"
#include "ui/volume.h"

/* Feed the same values every frame from t0 to t1 (16 ms frames); returns the last result. */
static int run(pw_volume *v, int level, int muted, unsigned int t0, unsigned int t1)
{
    int r = 0;

    for (unsigned int t = t0; t <= t1; t += 16)
        r = pw_volume_update(v, level, muted, 0, t);
    return r;
}

PW_TEST(volume_first_reading_shows_nothing)
{
    pw_volume v = {0};

    pw_volume_reset(&v);
    CHECK_EQ(pw_volume_update(&v, 12, 0, 0, 1000), 0);
    CHECK_EQ(run(&v, 12, 0, 1016, 3000), 0);
    CHECK_EQ(v.alpha, 0);
    CHECK_EQ(v.shown, 12 * 256);
}

PW_TEST(volume_change_shows_glides_and_fades)
{
    pw_volume v = {0};

    pw_volume_reset(&v);
    pw_volume_update(&v, 10, 0, 0, 0);
    CHECK_EQ(pw_volume_update(&v, 11, 0, 0, 16), 1);
    CHECK(v.alpha > 0 && v.alpha < 256);             /* fading in */
    CHECK(v.shown > 10 * 256 && v.shown < 11 * 256); /* on its way */
    CHECK(v.flash > 0 && v.up == 1);
    run(&v, 11, 0, 32, 400);
    CHECK_EQ(v.alpha, 256);
    CHECK_EQ(v.shown, 11 * 256);
    CHECK_EQ(v.flash, 0);
    run(&v, 11, 0, 416, 16 + PW_VOLUME_HOLD_MS + PW_VOLUME_FADE_MS / 2);
    CHECK(v.alpha > 0 && v.alpha < 256); /* fading out */
    /* gone: one more frame is asked for, then nothing */
    CHECK_EQ(run(&v, 11, 0, 16 + PW_VOLUME_HOLD_MS + PW_VOLUME_FADE_MS / 2 + 16,
                 16 + PW_VOLUME_HOLD_MS + PW_VOLUME_FADE_MS + 16),
             1);
    CHECK_EQ(v.alpha, 0);
    CHECK_EQ(pw_volume_update(&v, 11, 0, 0, 5000), 0);
}

PW_TEST(volume_press_at_the_end_and_mute_show_it)
{
    pw_volume v = {0};

    pw_volume_reset(&v);
    pw_volume_update(&v, PW_VOLUME_MAX, 0, 0, 0);
    CHECK_EQ(pw_volume_update(&v, PW_VOLUME_MAX + 5, 0, 1, 16), 1); /* clamped, but pressed */
    CHECK_EQ(v.level, PW_VOLUME_MAX);
    run(&v, PW_VOLUME_MAX, 0, 32, 3000); /* gone again */
    CHECK_EQ(pw_volume_update(&v, PW_VOLUME_MAX, 1, 0, 3016), 1);
    CHECK_EQ(v.muted, 1);
    CHECK_EQ(pw_volume_update(&v, 3, 1, 0, 3032), 1);
    CHECK_EQ(v.up, 0);
}

PW_TEST(brightness_bar_is_the_volume_bar_on_a_scale_of_the_steps)
{
    pw_volume v = {0};

    v.max = PW_BRIGHT_STEPS;
    pw_volume_reset(&v);
    CHECK_EQ(pw_volume_update(&v, 1, 0, 0, 1000), 0); /* the first reading: nothing */
    CHECK_EQ(run(&v, 5, 0, 1016, 2000), 1);           /* the fifth step (#6) */
    CHECK_EQ(v.level, 5);
    CHECK(v.shown > 4 * 256 && v.alpha == 256); /* the bar full: 5 of 5 (#11) */
    CHECK_EQ(run(&v, 9, 0, 2016, 2100), 1);
    CHECK_EQ(v.level, PW_BRIGHT_STEPS); /* (cut at its own top) */
}

PW_TEST(brightness_step_is_the_nearest_of_the_firmware_levels)
{
    /* the PSP-3000's 44 60 72 84 and ARK-4's fifth above them (#11): 1 of 5 is a fifth */
    CHECK_EQ(pw_backlight_step(44), 1);
    CHECK_EQ(pw_backlight_step(60), 2);
    CHECK_EQ(pw_backlight_step(72), 3);
    CHECK_EQ(pw_backlight_step(84), 4);
    CHECK_EQ(pw_backlight_step(100), 5);
    CHECK_EQ(pw_backlight_step(90), 5);
    CHECK_EQ(pw_backlight_step(51), 1); /* a level between: the nearest */
    CHECK_EQ(pw_backlight_step(53), 2);
    CHECK_EQ(pw_backlight_step(77), 3);
    CHECK_EQ(pw_backlight_step(20), 1);
}
