// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>

#include "core/bootpick.h"
#include "pwtest.h"

/* An ARKConfig as ARK-4 fills it on a PSP-3000 (common/include/ark.h). */
static void ark_config(unsigned char *cfg)
{
    memset(cfg, 0, PW_ARK_CONFIG_BUF);
    cfg[0] = 0x55;
    cfg[1] = 0x1E;
    cfg[2] = 0x0B;
    cfg[3] = 0xB0;
    strcpy((char *)cfg + PW_ARK_PATH_OFF, "ms0:/PSP/SAVEDATA/ARK_01234/");
    strcpy((char *)cfg + PW_ARK_EXPLOIT_OFF, "cIPL");
    cfg[PW_ARK_MODE_OFF] = 0x04; /* PSP_ORIG */
}

PW_TEST(boot_pick_goes_home_only_when_everything_agrees)
{
    pw_boot_in in = {1, 0, 0, 0, 1, 1};
    const char *why = 0;

    CHECK_EQ(pw_boot_pick(&in, &why), 1);
    CHECK(strcmp(why, "Pocketwatch Home") == 0);
    in.want_home = 0;
    CHECK_EQ(pw_boot_pick(&in, &why), 0);
    CHECK(strcmp(why, "start = xmb") == 0);
    in.want_home = 1;
    in.xmb_once = 1;
    CHECK_EQ(pw_boot_pick(&in, 0), 0);
    in.xmb_once = 0;
    in.held = 1;
    CHECK_EQ(pw_boot_pick(&in, 0), 0);
    in.held = 0;
    in.pending = 1; /* the crash guard: a Home that never came up is not started again */
    CHECK_EQ(pw_boot_pick(&in, &why), 0);
    CHECK(strcmp(why, "Home did not start last time") == 0);
    in.pending = 0;
    in.home_present = 0;
    CHECK_EQ(pw_boot_pick(&in, 0), 0);
    in.home_present = 1;
    in.ark_ok = 0;
    CHECK_EQ(pw_boot_pick(&in, 0), 0);
}

PW_TEST(boot_ark_config_checked_before_it_is_written)
{
    unsigned char cfg[PW_ARK_CONFIG_BUF];

    ark_config(cfg);
    CHECK_EQ(pw_boot_ark_check(cfg), 0);
    CHECK_EQ(pw_boot_ark_layout(cfg), 0);
    CHECK(strcmp(pw_boot_ark_folder(cfg), "ms0:/PSP/SAVEDATA/ARK_01234/") == 0);
    CHECK_EQ(pw_boot_ark_set(cfg, PW_BOOT_HOME_FILE), 0);
    CHECK(strcmp((char *)cfg + PW_ARK_LAUNCH_OFF, "PWHOME.PBP") == 0);
    CHECK_EQ(cfg[PW_ARK_MODE_OFF], 0x04); /* nothing next to the launcher moved */
    CHECK_EQ(pw_boot_ark_check(cfg), -5); /* a launcher set: never written over */
    CHECK_EQ(pw_boot_ark_layout(cfg), 0); /* but the layout is still as expected */
    CHECK(pw_boot_ark_set(cfg, "A_NAME_OF_TWENTY.PBP") < 0);

    ark_config(cfg);
    cfg[0] ^= 1; /* another layout or no ARK-4 */
    CHECK_EQ(pw_boot_ark_check(cfg), -1);
    ark_config(cfg);
    memset(cfg + PW_ARK_PATH_OFF, 'A', PW_ARK_PATH_LEN); /* not terminated */
    CHECK_EQ(pw_boot_ark_check(cfg), -2);
    ark_config(cfg);
    strcpy((char *)cfg + PW_ARK_PATH_OFF, "flash0:/ARK/");
    CHECK_EQ(pw_boot_ark_check(cfg), -3);
    ark_config(cfg);
    cfg[PW_ARK_MODE_OFF] = 0x09; /* PS Vita, Adrenaline */
    CHECK_EQ(pw_boot_ark_check(cfg), -4);
    ark_config(cfg);
    cfg[PW_ARK_RECOV_OFF] = 1;
    CHECK_EQ(pw_boot_ark_check(cfg), -5);
}

PW_TEST(boot_xmb_once_marker_only_counts_right_after_home)
{
    const unsigned long long s = 1000000ull, now = 63900000000ull * s; /* an RTC tick */
    char text[32];

    snprintf(text, sizeof(text), "%llu", now - 8 * s); /* Home quit 8 s ago */
    CHECK_EQ(pw_boot_marker_fresh(text, now), 1);
    snprintf(text, sizeof(text), "%llu", now - 3600 * s); /* left from an hour ago */
    CHECK_EQ(pw_boot_marker_fresh(text, now), 0);
    snprintf(text, sizeof(text), "%llu", now + 5 * s); /* from the future: the clock moved */
    CHECK_EQ(pw_boot_marker_fresh(text, now), 0);
    CHECK_EQ(pw_boot_marker_fresh("1", now), 0); /* the first version's marker */
    CHECK_EQ(pw_boot_marker_fresh("", now), 0);
}
