// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "clockfix.h"
#include "pwtest.h"

PW_TEST(clock_lost_only_when_the_rtc_refuses_or_reads_before_2016)
{
    unsigned long long t[4] = {0, 1700000000ULL, 1760950000ULL, 1451606399ULL};

    CHECK(hm_clock_lost(-1, 1760950000ULL)); /* not present (rtc.c: 1980 and an error) */
    CHECK(hm_clock_lost(0, 315532800ULL));   /* 1980-01-01 */
    CHECK(!hm_clock_lost(0, 1760950000ULL)); /* 2025 */
    CHECK(!hm_clock_lost(0, HM_CLOCK_SANE)); /* a date the owner set is kept */
    CHECK_EQ(hm_clock_newest(t, 4), 1760950000ULL);
    CHECK_EQ(hm_clock_newest(t, 1), 0);     /* nothing usable */
    CHECK_EQ(hm_clock_newest(t + 3, 1), 0); /* the last second of 2015 is not real either */
    CHECK(hm_clock_lost(0, 1295136000ULL)); /* 2011-01-16: what the owner's PSP read */
    /* the tick's epoch: 0001-01-01; 1970-01-01 is 62135596800 s later */
    CHECK_EQ(hm_tick_of_unix_us(0), 62135596800000000ULL);
    CHECK_EQ(hm_unix_us_of_tick(hm_tick_of_unix_us(1760950000123456ULL)), 1760950000123456ULL);
}

static void answer(unsigned char *p, unsigned int secs, unsigned int frac)
{
    memset(p, 0, HM_NTP_BYTES);
    p[0] = 0 << 6 | 4 << 3 | 4; /* server */
    p[1] = 2;
    p[40] = (unsigned char)(secs >> 24);
    p[41] = (unsigned char)(secs >> 16);
    p[42] = (unsigned char)(secs >> 8);
    p[43] = (unsigned char)secs;
    p[44] = (unsigned char)(frac >> 24);
}

PW_TEST(ntp_answer_gives_unix_time_and_bad_answers_are_refused)
{
    unsigned char p[HM_NTP_BYTES];
    unsigned long long us = 0;
    /* 2025-10-20 08:46:40 UTC = Unix 1760950000 = NTP 3969938800 */
    unsigned int ntp = 1760950000u + 2208988800u;

    hm_ntp_request(p);
    CHECK_EQ(p[0], 0x23); /* version 4, client */
    CHECK_EQ(p[40], 0);
    answer(p, ntp, 0x80000000u); /* and half a second */
    CHECK_EQ(hm_ntp_parse(p, HM_NTP_BYTES, &us), 0);
    CHECK_EQ(us, 1760950000500000ULL);
    CHECK(hm_ntp_parse(p, 47, &us) < 0); /* short */
    p[1] = 0;                            /* kiss of death */
    CHECK(hm_ntp_parse(p, HM_NTP_BYTES, &us) < 0);
    answer(p, ntp, 0);
    p[0] = 3 << 6 | 4 << 3 | 4; /* the server is not synchronised */
    CHECK(hm_ntp_parse(p, HM_NTP_BYTES, &us) < 0);
    answer(p, ntp, 0);
    p[0] = 0 << 6 | 4 << 3 | 3; /* a client's packet, not an answer */
    CHECK(hm_ntp_parse(p, HM_NTP_BYTES, &us) < 0);
    answer(p, 2208988800u + 315532800u, 0); /* 1980: never real */
    CHECK(hm_ntp_parse(p, HM_NTP_BYTES, &us) < 0);
    answer(p, 1000u, 0); /* era 1 (after 2036-02-07): 2^32 s more */
    CHECK_EQ(hm_ntp_parse(p, HM_NTP_BYTES, &us), 0);
    CHECK_EQ(us, (0x100000000ULL + 1000u - 2208988800ULL) * 1000000ULL);
}
