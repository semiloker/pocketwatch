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

    CHECK_EQ(hm_plugins_get(crlf, (int)strlen(crlf), PERF), -1);
    n = hm_plugins_set(crlf, (int)strlen(crlf), PERF, 1, out, sizeof(out));
    CHECK(n > 0);
    out[n] = 0;
    CHECK_STR(out, "UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on\r\n"
                   "VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on\r\n"
                   "UMD, " PERF ", on\r\n");
    CHECK_EQ(hm_plugins_get(out, n, PERF), 1);
    m = hm_plugins_set(out, n, PERF, 0, back, sizeof(back));
    back[m] = 0;
    CHECK(strstr(back, "UMD, " PERF ", off\r\n") != 0);
    CHECK_EQ(hm_plugins_get(back, m, PERF), 0);
    CHECK_EQ(hm_plugins_get(back, m, "pocketwatch.prx"), 1); /* the others untouched */
    /* off when there is no line: nothing added; a last line without its newline gets one */
    n = hm_plugins_set("GAME, ms0:/x.prx, on", 20, PERF, 0, out, sizeof(out));
    out[n] = 0;
    CHECK_STR(out, "GAME, ms0:/x.prx, on\n");
    /* the case of the card's path does not matter; too small a buffer is refused */
    n = hm_plugins_set("umd, MS0:/SEPLUGINS/POCKETWATCH/POCKETWATCH_PERF.PRX, ON\n", 57, PERF, 0,
                       out, sizeof(out));
    out[n] = 0;
    CHECK_STR(out, "umd, MS0:/SEPLUGINS/POCKETWATCH/POCKETWATCH_PERF.PRX, off\n");
    CHECK(hm_plugins_set(crlf, (int)strlen(crlf), PERF, 1, out, 40) < 0);
}
