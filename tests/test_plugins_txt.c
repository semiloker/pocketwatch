// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "plugins_txt.h"
#include "pwtest.h"

#define PERF "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx"

PW_TEST(plugins_txt_turns_one_line_on_and_off_and_keeps_the_rest)
{
    static const char crlf[] = "UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on\r\n"
                               "VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on\r\n";
    char out[512], back[512];
    int n, m;

    CHECK_EQ(hm_plugins_get(crlf, (int)strlen(crlf), "UMD", PERF), -1);
    n = hm_plugins_set(crlf, (int)strlen(crlf), "UMD", PERF, 1, out, sizeof(out));
    CHECK(n > 0);
    out[n] = 0;
    CHECK_STR(out, "UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on\r\n"
                   "VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on\r\n"
                   "UMD, " PERF ", on\r\n");
    CHECK_EQ(hm_plugins_get(out, n, "UMD", PERF), 1);
    m = hm_plugins_set(out, n, "UMD", PERF, 0, back, sizeof(back));
    back[m] = 0;
    CHECK(strstr(back, "UMD, " PERF ", off\r\n") != 0);
    CHECK_EQ(hm_plugins_get(back, m, "UMD", PERF), 0);
    CHECK_EQ(hm_plugins_get(back, m, "UMD", "pocketwatch.prx"), 1); /* the others untouched */
    /* off when there is no line: nothing added; a last line without its newline gets one */
    n = hm_plugins_set("GAME, ms0:/x.prx, on", 20, "UMD", PERF, 0, out, sizeof(out));
    out[n] = 0;
    CHECK_STR(out, "GAME, ms0:/x.prx, on\n");
    /* the case of the card's path does not matter; too small a buffer is refused */
    n = hm_plugins_set("umd, MS0:/SEPLUGINS/POCKETWATCH/POCKETWATCH_PERF.PRX, ON\n", 57, "UMD",
                       PERF, 0, out, sizeof(out));
    out[n] = 0;
    CHECK_STR(out, "umd, MS0:/SEPLUGINS/POCKETWATCH/POCKETWATCH_PERF.PRX, off\n");
    CHECK(hm_plugins_set(crlf, (int)strlen(crlf), "UMD", PERF, 1, out, 40) < 0);
}

PW_TEST(plugins_txt_keeps_the_module_s_umd_and_vsh_lines_apart)
{
    static const char t[] = "UMD, ms0:/seplugins/pocketwatch/pocketwatch_perf.prx, on\n";
    char out[256];
    int n =
        hm_plugins_set(t, (int)sizeof(t) - 1, "VSH",
                       "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx", 1, out, (int)sizeof(out));

    CHECK(n > 0);
    out[n] = 0;
    CHECK_STR(out, "UMD, ms0:/seplugins/pocketwatch/pocketwatch_perf.prx, on\n"
                   "VSH, ms0:/seplugins/pocketwatch/pocketwatch_perf.prx, on\n");
    CHECK_EQ(hm_plugins_get(out, n, "VSH", "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx"), 1);
    n = hm_plugins_set(out, n, "umd", "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx", 0,
                       out + 128, 128);
    CHECK(n > 0);
    CHECK_EQ(hm_plugins_get(out + 128, n, "UMD", "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx"),
             0);
    CHECK_EQ(hm_plugins_get(out + 128, n, "VSH", "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx"),
             1);
    CHECK_EQ(
        hm_plugins_get(out + 128, n, "GAME", "ms0:/seplugins/pocketwatch/pocketwatch_perf.prx"),
        -1);
}
