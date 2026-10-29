# M7 — the PSP's clock after a long time unused

Question: why does the PSP ask for the date and time again after weeks or a month unused (owner,
2026-10-20), and what can Pocketwatch do so that the clock is right without the owner setting it?
Status: Open (2026-10-20) - research done from sources, implemented in Home (ADR-041); device
tests HM29 pending.

## Background and sources

**Where the clock is.** The time is a count the syscon keeps: `sceSysconReadClock` (command 0x09)
and `sceSysconWriteClock` (0x20), next to the alarm's (0x0A / 0x22) and a small scratchpad
(0x23 / 0x24). **VERIFIED** (uOFW `include/syscon.h:28-43`, `src/syscon/syscon.c:1638-1655`; the
6.60 NIDs 0xF436BB12 / 0x3FB3FD08 in `src/syscon/exports.exp`, matching ARK-4's table
`tools/nids/ark4_660.tsv`).

**What the kernel answers.** `sceRtcGetCurrentTick` is a base read from the syscon at start (and at
every resume) plus the system's microseconds since; while the base is not there it writes
0x00DDDEF8B3A14000 - 1980-01-01 00:00:00 - and returns an error. **VERIFIED** (uOFW
`src/kd/rtc/rtc.c:333-359`, the rtc module reversed for 6.60; the tick decoded with Python). The
module also keeps a separate "secure" tick and a "last reincarnated" time - the moment the clock
was last lost - **INFERRED** from the names and the reads of the syscon scratchpad at offsets 8 and
16 in `sceRtc_driver_852255B8` (uOFW's reversal of these is unfinished).

**Why it is lost.** With no power the syscon's count stops and its state goes: the firmware then
asks for the date and time at the next start. Owners report it after the battery ran flat or was
out for days, and after a PSP sat unused for a while (forum threads, 2009-2015:
<https://racketboy.com/forum/viewtopic.php?t=20885>,
<https://bloodstained.forums.net/thread/604/psp-2000-clock-reset>). **INFERRED**: a PSP left off for
weeks discharges its battery until the syscon is no longer powered (the PSP draws a little even
when off). No Sony documentation of a backup cell was found; RESEARCH REQUIRED (not needed for the
design: whatever keeps the clock, nothing counts without power).

**Setting it.** `sceRtcSetCurrentTick(u64 *tick)` (UTC) in `sceRtc_driver`, kernel only: NID
0x9763C138, 6.60 0x929620CE (uOFW `rtc.c:176`, alias noted there; ARK-4's table maps it). NTPSP
(joel16, BSD-3-Clause, <https://github.com/joel16/NTPSP>) sets the PSP's clock this way from a
small kernel PRX (`plugin/rtc_driver.c`) after an SNTP answer (`app/source/ntp.c`); read, not
copied. **INFERRED** that it works on 6.61 ARK-4 (a published tool; not run here). Home already runs kernel functions through ARK-4's `kuKernelCall` (the syscon's battery
getters, the controller's peek), so it needs no PRX of its own.

**The time zone.** The PSP keeps UTC; the local time adds the registry's zone and summer time
(`sceRtcConvertUtcToLocalTime`). The registry is in flash1 and is not lost with the clock.
**VERIFIED** (PSPSDK `psprtc.h:76-81`; the zone survives: Home's Settings read it).

**A file's time.** The FAT driver stamps every write with the local time of the moment: each file
Pocketwatch writes carries the clock of its last write - `home.log` every 30 s while Home runs,
`pocketwatch.log` in every game, the play time files every 5 minutes of play. Files the PC copies
keep the PC's time (PowerShell's `Copy-Item` keeps the source's LastWriteTime: the deployed PRX
files and EBOOT carry their build's time). **VERIFIED** for the PSP's side: on the owner's card
`home.log` and `pocketwatch.log` were dated 2011-01-17 and `xmb.log` 2011-01-16, the dates the
PSP's clock read (`xmb.log` first line `2011-01-16 09:02:33 start`). **INFERRED** for Copy-Item.

**The owner's PSP now.** Its clock reads January 2011 (the files above; screenshots named
`ULUS10277_20110116_...`), while its 2025 screenshots carry 2025 names: the clock was lost and the
firmware's date screen passed with a date in 2011. **VERIFIED** (the card's contents, 2026-10-20).
So "lost" must also cover a clock that runs but reads an impossible year: Home treats anything
before 2016 as lost (ADR-041).

## Options

| | Source of the time | Right? | Cost |
|---|---|---|---|
| A | The newest time (2016 or later) of Pocketwatch's files on the card, those the PC copied included | at least that late; short by the time the PSP sat unused | 14 `sceIoGetstat` at Home's start |
| B | The internet (SNTP: pool.ntp.org, time.google.com) over the first saved connection | to a fraction of a second | the network's modules (~1 MB of user memory, freed after), 5-20 s, the WLAN switch on |
| C | Keep the syscon powered | - | not in software |
| D | Ask the owner (the firmware's own screen) | right | what the owner asked to avoid |

Chosen (ADR-041): A at once, then B when Wi-Fi is on; D stays the fallback (Settings → Date and
Time in the XMB). Only a clock that is really lost is touched: the kernel refused it or it reads
before 2016 - a date from 2016 on is never changed by A.

## Procedure

**C1 - a lost clock.** A charged PSP, the charger out: take the battery out for a few minutes
(longer if the clock survives: the time it holds is itself a result), put it back, start. With
`start = home`: Home's toast says the clock was set to the last time Pocketwatch saw; Settings →
Date and Time in the XMB shows that date, not 2000 or 1980; the firmware does not ask (or does:
note which). `home.log`: `home clock: lost ... the card's newest time` and `home clock: set to`.

**C2 - the internet's time.** The WLAN switch on, a connection saved in the XMB's Network
Settings: Home → Settings → Date and Time → Set Time from Internet. The row says Connecting...,
then "Set at 14:05"; the clock matches a phone's to the second. `home.log`: `home clock: from the
internet 0:` with each step's result. Then with the switch off: "Wi-Fi switch is off".

## Results

None yet. Note: Home's Internet Browser failed in `sceNetInit` with 0x8002013A (library not
linked) on the owner's PSP, twice with the switch off (`home.log` 2026-10-20); the time's request
loads the network the way NTPSP does (`sceUtilityLoadNetModule`, 4 KB stacks for `sceNetInit`) and
logs each step, so C2 says whether that path works.

## Conclusions

- Nothing on the PSP counts time without power: a "real time" clock needs the internet's time (B);
  without it the best is the last time known (A), which is never earlier than the truth.
- OPEN QUESTION (device, C1): whether the firmware's own date screen still appears once Home has
  set a lost clock (if its check is a flag rather than the time, it will).
- Next: C1 and C2 on the device (HM29).
