// SPDX-License-Identifier: GPL-3.0-or-later
#include "sound.h"
#include "pwtest.h"

static short g_buf[HM_SND_MAX];

PW_TEST(home_sounds_are_whole_chunks_quiet_and_end_in_silence)
{
    for (int s = 0; s < HM_SNDS; s++) {
        int n = hm_sound_make(s, g_buf, HM_SND_MAX), peak = 0;
        long long energy = 0;

        CHECK(n > 0);
        CHECK_EQ(n % HM_SND_CHUNK, 0);
        CHECK(n <= HM_SND_MAX);
        for (int i = 0; i < n; i++) {
            int v = g_buf[i] < 0 ? -g_buf[i] : g_buf[i];

            peak = v > peak ? v : peak;
            energy += (long long)v * v;
        }
        CHECK(energy > 0);
        CHECK(peak < 32767 / 5);                  /* key sounds stay well under full scale */
        CHECK(g_buf[0] > -300 && g_buf[0] < 300); /* they rise: no click at the start */
        CHECK(g_buf[n - 1] == 0);                 /* nor at the end */
    }
}

PW_TEST(home_sounds_refuse_what_does_not_fit)
{
    CHECK_EQ(hm_sound_make(-1, g_buf, HM_SND_MAX), -1);
    CHECK_EQ(hm_sound_make(HM_SNDS, g_buf, HM_SND_MAX), -1);
    CHECK_EQ(hm_sound_make(HM_SND_START, g_buf, HM_SND_CHUNK), -1);
}
