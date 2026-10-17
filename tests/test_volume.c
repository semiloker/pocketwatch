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
    pw_volume v;

    pw_volume_reset(&v);
    CHECK_EQ(pw_volume_update(&v, 12, 0, 0, 1000), 0);
    CHECK_EQ(run(&v, 12, 0, 1016, 3000), 0);
    CHECK_EQ(v.alpha, 0);
    CHECK_EQ(v.shown, 12 * 256);
}

PW_TEST(volume_change_shows_glides_and_fades)
{
    pw_volume v;

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
    pw_volume v;

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
