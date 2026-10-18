// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>

#include "core/crc32.h"
#include "core/playtime.h"
#include "pwtest.h"

static pw_pt_table T, U;
static char text[PW_PT_TEXT_MAX];

static pw_datetime dt(int y, int mo, int d, int h, int mi)
{
    pw_datetime t;

    memset(&t, 0, sizeof(t));
    t.year = (unsigned short)y;
    t.month = (unsigned short)mo;
    t.day = (unsigned short)d;
    t.hour = (unsigned short)h;
    t.minute = (unsigned short)mi;
    return t;
}

PW_TEST(playtime_round_trip_and_crc)
{
    pw_datetime a = dt(2026, 10, 17, 14, 5), b = dt(2026, 10, 17, 15, 30);
    pw_pt_base base;
    int n;

    memset(&T, 0, sizeof(T));
    T.seq = 7;
    pw_pt_base_of(NULL, &base);
    pw_pt_apply(pw_pt_find(&T, "ULUS10277", 1), &base, 5400, &a, &b,
                "Castlevania The Dracula X Chronicles");
    pw_pt_apply(pw_pt_find(&T, "ULES01513", 1), &base, 60, &a, &a, "The 3rd\tBirthday");
    n = pw_pt_format(&T, text, sizeof(text));
    CHECK(n > 0);
    CHECK_EQ(pw_pt_parse(&U, text, (unsigned int)n), 2);
    CHECK_EQ(U.seq, 7u);
    CHECK_EQ(U.g[0].total_s, 5400u);
    CHECK_EQ(U.g[0].sessions, 1u);
    CHECK_EQ(U.g[0].longest_s, 5400u);
    CHECK_EQ(U.g[0].first.hour, 14);
    CHECK_EQ(U.g[0].last.minute, 30);
    CHECK(strcmp(U.g[0].title, "Castlevania The Dracula X Chronicles") == 0);
    CHECK(strcmp(U.g[1].title, "The 3rd Birthday") == 0); /* a tab in a title does not split */

    text[40] ^= 1; /* any change: the whole file is refused, the other copy will be used */
    CHECK(pw_pt_parse(&U, text, (unsigned int)n) < 0);
    CHECK_EQ(U.n, 0);
    text[40] ^= 1;
    CHECK(pw_pt_parse(&U, text, (unsigned int)n - 5) < 0); /* cut short */
}

PW_TEST(playtime_sessions_add_to_the_boot_s_base_once)
{
    pw_datetime start = dt(2026, 10, 18, 9, 0), now = dt(2026, 10, 18, 10, 0);
    pw_pt_game *g;
    pw_pt_base base;

    memset(&T, 0, sizeof(T));
    g = pw_pt_find(&T, "ULUS10277", 1);
    g->total_s = 1000;
    g->sessions = 3;
    g->longest_s = 900;
    g->first = dt(2026, 10, 1, 12, 0);
    pw_pt_base_of(g, &base);
    /* saved three times in one session: always base + the session so far */
    pw_pt_apply(g, &base, 600, &start, &now, "X");
    pw_pt_apply(g, &base, 1200, &start, &now, "X");
    pw_pt_apply(g, &base, 1800, &start, &now, "X");
    CHECK_EQ(g->total_s, 2800u);
    CHECK_EQ(g->sessions, 4u);
    CHECK_EQ(g->longest_s, 1800u);
    CHECK_EQ(g->first.day, 1); /* known before: its first date stays */

    /* a game never seen: its first date is this session's start, nothing earlier is claimed */
    pw_pt_base_of(NULL, &base);
    g = pw_pt_find(&T, "NPJH50000", 1);
    pw_pt_apply(g, &base, 30, &start, &now, "New");
    CHECK_EQ(g->first.hour, 9);
    CHECK_EQ(g->total_s, 30u);
}

PW_TEST(playtime_bad_lines_cost_only_themselves_and_a_full_table_drops_the_oldest)
{
    char buf[1024];
    int n;

    snprintf(buf, sizeof(buf),
             "# Pocketwatch play time 1 seq 2\n"
             "ULUS10277\t100\t1\t100\t2026-10-17 14:05:00\t2026-10-17 15:30:00\tGood\n"
             "BROKEN\tx\t1\t1\t2026-10-17 14:05:00\t2026-10-17 15:30:00\tBad number\n"
             "ULES01513\t50\t1\t50\t2026-13-17 14:05:00\t2026-10-17 15:30:00\tBad month\n");
    n = (int)strlen(buf);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "# crc %08X\n",
                  pw_crc32(0, buf, (unsigned int)n));
    CHECK_EQ(pw_pt_parse(&U, buf, (unsigned int)n), 1);
    CHECK(strcmp(U.g[0].id, "ULUS10277") == 0);

    memset(&T, 0, sizeof(T));
    for (int i = 0; i < PW_PT_GAMES_MAX; i++) {
        char id[16];
        pw_pt_game *g;

        snprintf(id, sizeof(id), "GAME%05d", i);
        g = pw_pt_find(&T, id, 1);
        g->last = dt(2026, 1 + i % 12, 1 + i % 28, 0, 0);
        g->total_s = (unsigned int)i;
    }
    T.g[37].last = dt(2001, 1, 1, 0, 0);
    CHECK(pw_pt_find(&T, "NEWGAME01", 1) == &T.g[37]); /* the least recently played made room */
    CHECK_EQ(T.n, PW_PT_GAMES_MAX);

    int order[PW_PT_GAMES_MAX];

    CHECK_EQ(pw_pt_order(&T, order, PW_PT_GAMES_MAX), PW_PT_GAMES_MAX);
    CHECK_EQ(T.g[order[0]].total_s, (unsigned int)(PW_PT_GAMES_MAX - 1));
    n = pw_pt_format(&T, text, sizeof(text));
    CHECK(n > 0); /* a full table fits its text buffer */
}
