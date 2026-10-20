// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_CLOCKFIX_H
#define HM_CLOCKFIX_H

/*
 * The clock kept (HOM-REQ-031, HOM-REQ-032, ADR-041, docs/research/m7-clock.md). The PSP's clock
 * is the syscon's count, kept by the battery: a battery left flat for weeks loses it, and the
 * firmware then answers 1980-01-01 with an error (uOFW src/kd/rtc/rtc.c, sceRtcGetCurrentTick)
 * and asks for the date. Nothing counts time without power, so Home puts back the newest time the
 * card knows (every file Pocketwatch writes carries the clock of its last write), and the
 * internet's (SNTP, RFC 4330) when a connection is there. Portable (no PSP headers).
 */

#define HM_US_PER_S     1000000ULL
#define HM_UNIX_AT_TICK 62135596800ULL /* 1970-01-01 in seconds from 0001-01-01, the tick's 0 */
/* 2016-01-01 (Unix seconds): earlier is never real here. The owner's PSP read 2011-01-16 after its
   clock was lost and the firmware's date screen was passed (xmb.log, 2026-10-20). */
#define HM_CLOCK_SANE 1451606400ULL

/* The clock was lost: the RTC refused (rtc < 0) or reads before HM_CLOCK_SANE. A date the owner
   set is never second-guessed beyond that. */
int hm_clock_lost(int rtc, unsigned long long unix_s);

/* The newest of n times (Unix seconds), those before HM_CLOCK_SANE left out; 0 if none. */
unsigned long long hm_clock_newest(const unsigned long long *unix_s, int n);

/* The PSP's tick (microseconds from 0001-01-01) of a Unix time in microseconds, and back. */
unsigned long long hm_tick_of_unix_us(unsigned long long unix_us);
unsigned long long hm_unix_us_of_tick(unsigned long long tick);

/* SNTP: a client's request (48 bytes, version 4, mode 3), and the server's answer read: 0 and
   its transmit time (Unix microseconds), or < 0 when it is not a usable answer (too short, not
   a server, the kiss of death, an unsynchronised server, a time before HM_CLOCK_SANE). */
#define HM_NTP_BYTES 48
void hm_ntp_request(unsigned char *pkt);
int hm_ntp_parse(const unsigned char *pkt, int len, unsigned long long *unix_us);

#endif
