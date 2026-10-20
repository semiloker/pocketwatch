// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockfix.h"

#include <string.h>

#define NTP_TO_UNIX 2208988800ULL /* 1900-01-01 to 1970-01-01, seconds */

int hm_clock_lost(int rtc, unsigned long long unix_s)
{
    return rtc < 0 || unix_s < HM_CLOCK_SANE;
}

unsigned long long hm_clock_newest(const unsigned long long *unix_s, int n)
{
    unsigned long long best = 0;

    for (int i = 0; i < n; i++)
        if (unix_s[i] >= HM_CLOCK_SANE && unix_s[i] > best)
            best = unix_s[i];
    return best;
}

unsigned long long hm_tick_of_unix_us(unsigned long long unix_us)
{
    return unix_us + HM_UNIX_AT_TICK * HM_US_PER_S;
}

unsigned long long hm_unix_us_of_tick(unsigned long long tick)
{
    return tick > HM_UNIX_AT_TICK * HM_US_PER_S ? tick - HM_UNIX_AT_TICK * HM_US_PER_S : 0;
}

static unsigned int be32(const unsigned char *p)
{
    return (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 | (unsigned int)p[2] << 8 | p[3];
}

void hm_ntp_request(unsigned char *pkt)
{
    memset(pkt, 0, HM_NTP_BYTES);
    pkt[0] = 0 << 6 | 4 << 3 | 3; /* no leap warning, version 4, client */
}

int hm_ntp_parse(const unsigned char *pkt, int len, unsigned long long *unix_us)
{
    unsigned int mode, version, stratum, s, f;
    unsigned long long secs;

    if (len < HM_NTP_BYTES)
        return -1;
    mode = pkt[0] & 7;
    version = pkt[0] >> 3 & 7;
    stratum = pkt[1];
    if ((mode != 4 && mode != 5) || version < 3 || version > 4)
        return -2;
    if (stratum == 0 || stratum > 15 || pkt[0] >> 6 == 3) /* kiss of death, or not synchronised */
        return -3;
    s = be32(pkt + 40); /* the transmit time: when the server answered */
    f = be32(pkt + 44);
    if (s == 0 && f == 0)
        return -4;
    /* era 0 ends in 2036: a time that small is the next era (RFC 4330, section 3) */
    secs = s >= 0x80000000u ? s : (unsigned long long)s + 0x100000000ULL;
    if (secs < NTP_TO_UNIX + HM_CLOCK_SANE)
        return -5;
    *unix_us = (secs - NTP_TO_UNIX) * HM_US_PER_S + ((unsigned long long)f * HM_US_PER_S >> 32);
    return 0;
}
