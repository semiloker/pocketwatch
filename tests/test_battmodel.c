// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/battmodel.h"
#include "pwtest.h"

#define U PW_BATT_UNKNOWN

static pw_batt_sample sample(unsigned int t_s, int percent, int ma, int rcap)
{
    pw_batt_sample s = {t_s * 1000, percent, 3900, ma, 30, rcap, 1800, 50, 200, 1, 0, 0};

    pw_batt_validate(&s);
    return s;
}

PW_TEST(battery_validation_turns_misreads_into_unknown)
{
    pw_batt_sample s = {0, 140, 12000, -9000, 300, 9000, 1800, -1, -5, 1, 3, 0};

    pw_batt_validate(&s);
    CHECK_EQ(s.percent, U);
    CHECK_EQ(s.mv, U);
    CHECK_EQ(s.ma, U);
    CHECK_EQ(s.temp_c, U);
    CHECK_EQ(s.rcap_mah, U); /* more than full */
    CHECK_EQ(s.full_mah, 1800);
    CHECK_EQ(s.cycles, U);
    CHECK_EQ(s.fw_minutes, U);
    CHECK_EQ(s.charging, U);
    CHECK_EQ(s.online, 0);

    pw_batt_sample none = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

    pw_batt_validate(&none); /* no battery: nothing is a measurement, not zeros */
    CHECK_EQ(none.percent, U);
    CHECK_EQ(none.mv, U);
    CHECK_EQ(none.rcap_mah, U);
}

PW_TEST(battery_estimate_needs_history_then_uses_the_current)
{
    pw_batt_history h;
    pw_batt_estimate e;

    pw_batt_history_reset(&h);
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.state, PW_BATT_STATE_UNKNOWN);
    CHECK_EQ(e.minutes, -1);

    pw_batt_sample s = sample(0, 80, -400, 1200);

    pw_batt_history_add(&h, &s);
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.state, PW_BATT_DISCHARGING);
    CHECK_EQ(e.confidence, PW_BATT_EST_SYSTEM); /* the system's 200 min until we know more */
    CHECK_EQ(e.minutes, 200);

    for (unsigned int t = 15; t <= 90; t += 15) {
        s = sample(t, 80, -400, 1200);
        pw_batt_history_add(&h, &s);
    }
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.confidence, PW_BATT_EST_ROUGH);
    CHECK_EQ(e.minutes, 1200 * 60 / 400); /* 3 h */
    CHECK_EQ(e.avg_ma, 400);

    for (unsigned int t = 105; t <= 330; t += 15) {
        s = sample(t, 79, -400, 1190);
        pw_batt_history_add(&h, &s);
    }
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.confidence, PW_BATT_EST_GOOD);
}

PW_TEST(battery_slope_when_there_is_no_current)
{
    pw_batt_history h;
    pw_batt_estimate e;
    pw_batt_sample s;

    pw_batt_history_reset(&h);
    for (unsigned int t = 0; t <= 600; t += 15) { /* 60 % -> 57 % in ten minutes */
        s = sample(t, 60 - (int)(t / 200), U, U);
        s.fw_minutes = U;
        pw_batt_history_add(&h, &s);
    }
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.confidence, PW_BATT_EST_ROUGH);
    CHECK_EQ(e.minutes, 57 * 10 / 3);
}

PW_TEST(battery_runs_restart_on_power_changes_gaps_and_loads)
{
    pw_batt_history h;
    pw_batt_estimate e;
    pw_batt_sample s;

    pw_batt_history_reset(&h);
    for (unsigned int t = 0; t <= 120; t += 15) {
        s = sample(t, 70, -300, 1000);
        pw_batt_history_add(&h, &s);
    }
    CHECK_EQ(h.n, 9);
    s = sample(135, 70, 500, 1000); /* plugged in */
    s.online = 1;
    s.charging = 1;
    pw_batt_history_add(&h, &s);
    CHECK_EQ(h.n, 1);
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.state, PW_BATT_CHARGING);
    CHECK_EQ(e.minutes, -1); /* no history of the charge yet */

    s = sample(100, 70, 500, 1000); /* time went back: a state was loaded */
    s.online = s.charging = 1;
    pw_batt_history_add(&h, &s);
    CHECK_EQ(h.n, 1);
    s = sample(100 + 6 * 60, 71, 500, 1010); /* six minutes later */
    s.online = s.charging = 1;
    pw_batt_history_add(&h, &s);
    CHECK_EQ(h.n, 1);

    s = sample(1000, 100, 0, 1800); /* full, still plugged in */
    s.online = 1;
    s.charging = 0;
    pw_batt_history_add(&h, &s);
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.state, PW_BATT_FULL);
    CHECK_EQ(e.minutes, -1);
}

PW_TEST(battery_charging_time_to_full)
{
    pw_batt_history h;
    pw_batt_estimate e;
    pw_batt_sample s;

    pw_batt_history_reset(&h);
    for (unsigned int t = 0; t <= 90; t += 15) {
        s = sample(t, 50, 900, 900);
        s.online = s.charging = 1;
        pw_batt_history_add(&h, &s);
    }
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.state, PW_BATT_CHARGING);
    CHECK_EQ(e.confidence, PW_BATT_EST_ROUGH);
    CHECK_EQ(e.minutes, (1800 - 900) * 60 / 900);
}

PW_TEST(battery_capacity_slope_when_there_is_no_current)
{
    pw_batt_history h;
    pw_batt_estimate e;
    pw_batt_sample s;

    pw_batt_history_reset(&h);
    for (unsigned int t = 0; t <= 300; t += 15) { /* 1500 -> 1480 mAh in five minutes */
        s = sample(t, 83, U, 1500 - (int)(t / 15));
        s.fw_minutes = U;
        pw_batt_history_add(&h, &s);
    }
    pw_batt_estimate_now(&h, &e);
    CHECK_EQ(e.confidence, PW_BATT_EST_GOOD);
    CHECK_EQ(e.minutes, 1480 * 5 / 20); /* 20 mAh in 5 min: 6 h 10 min */
    CHECK_EQ(e.avg_ma, 20 * 3600 / 300);
}
