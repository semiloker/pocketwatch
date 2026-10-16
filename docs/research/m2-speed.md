# M2 — making save and load fast

Goal: save and load in about a second each. Research done 2026-10-15 (code review of our engine,
PSPFatSave, PSPStates v2, ARK-4 Adrenaline, uOFW 6.60 sources) plus the first on-device
measurements. Test game for fast iterations: **Castlevania: Dracula X Chronicles (ULUS-10277)**
— gameplay in seconds, position changes are obvious, the engine supports it (no highmem, no
volatile memory).

## 1. Where the time goes today

Measured on the PSP-3000 at 333/166 MHz (ARK-4 `game, overclock, on`), log
[`logs/2026-10-15-speed1-castlevania.log`](logs/2026-10-15-speed1-castlevania.log):

| Phase | Save | Load |
|---|---|---|
| freeze game threads | 2 ms | 3 ms |
| pack (FastLZ + CRC32) / unpack | **5394 ms** | 875 + 102 ms |
| debug checksums, verify, log flush | 562 ms | 750 ms |
| **suspend: request -> our thread runs again** | **6744 ms** | **7424 ms** |
| total | 12.7 s | 9.3 s |

The suspend dominates both. Inside it (uOFW `power/powerSwitch.c`, sources below):

1. `0x100..0x400` queries/phase-2 events, then the **power-lock loop**: every 10 ms `0x401`, then
   `0x402` with veto; it repeats while any handler returns busy or a kernel power lock is held.
   We see it ~22 times (transient; ScePower's battery thread, syscon, ctrl, me_wrapper or a
   kernel power lock can each veto). Not the big cost in Castlevania.
2. `0x1000`, `_scePowerOffCommon` (I/O lines off), `0x400F..0x4000` (drivers save hardware state;
   ME eDRAM -> P11 at 0x4005, GE eDRAM -> P5 at 0x4003; our PHASE0_0 work), D-cache write-back,
   then `sceSysconPowerSuspend`: DDR self-refresh, power off.
3. **The CPU is off for several seconds**: syscon power-down, the RTC alarm (syscon keeps time in
   0.5-1 s units, so +100 ms cannot be honoured; armed at the first `0x401` it is already past
   due), wake, pre-IPL + IPL (KIRK-decrypted from NAND) and `_scePowerResumePoint`.
4. Resume handlers, `0x10000..0x1000F`, a full VBLANK wait, `0x40000`, `0x100000..2` (5 ms
   retries), `0x400000`; then our flash driver rebuild.

PSPFatSave measured the same on a PSP-1000: the suspend window is "bound by the ~2 s firmware
descent, not the alarm" and totals 4.3-6 s; shortening the alarm gained nothing.

The first run's per-phase split used CP0 Count, whose rate is not constant across the suspend
phases (spans added up to more than the wall clock). Spans now use the system timer.

## 2. Throughput of the building blocks

Benchmark (L+R+START+UP), Castlevania's 24 MB of game RAM, game frozen:

| Operation | MB/s | 24 MB takes |
|---|---|---|
| sysclib `memcpy`, cached | 169 | 0.15 s |
| own word copy x8 (-Os) | 107 | 0.23 s |
| `memset` | 354 | — |
| read through the uncached segment | 26.5 | — |
| word checksum (`pw_sum32`) | 88.8 | 0.28 s |
| byte-wise CRC32 | 20.8 | 1.2 s |
| zero-page scan | 172 | 0.15 s |
| FastLZ L1 pack (-Os) | 6.6 | 3.8 s |
| FastLZ L1 unpack (-Os) | 28.5 | 0.9 s |

29 % of the pages are all zero; FastLZ packs the RAM to 41 % (10.2 MB). The arena holds ~14.8 MB,
so the packed state fits but raw RAM (26 MB with eDRAM) does not.

Why FastLZ is slow here (disassembly of our build): at `-Os` the 32-bit reader is not inlined
(two calls per input byte), literal and match copies are byte loops behind a call, the match
compare is byte-wise, the 32 KB hash table is twice the D-cache and is cleared per 64 KB chunk,
and FastLZ L1 probes every byte even in incompressible data. About 60 cycles per input byte.

Allegrex facts that matter: single issue, in order; 16 KB I + 16 KB D cache, 64-byte lines; L1
hit ~3 cycles, random RAM access ~72 cycles, sequential reads ~290 MB/s; `lwl/lwr` for unaligned
words; `cache 0x18` (create dirty exclusive) allocates a destination line without reading it
(the firmware's own memset/memmove use it); `lv.q/sv.q` move 16 bytes (VFPU). Instruction count,
not memory bandwidth, is the limit for compression.

## 3. Options, ranked

| # | Idea | Expected gain | Risk | Status |
|---|---|---|---|---|
| 1 | **Touch-and-go suspend** (`scePowerRequestSuspendTouchAndGo`, scePower_driver 0x2875994B -> 0xD1FFF513): all suspend/resume events and handlers run, syscon never powers down, no IPL. Put back the I/O lines `_scePowerOffCommon` turned off. | ~4-6 s per save and per load | no NAND writes; worst case a hang or a dead peripheral until reboot | built: L+R+SELECT+TRIANGLE (one suspend), L+R+SELECT+SQUARE (fast mode for save/load) |
| 2 | **LZ4 block** (BSD-2, freestanding, 8 KB table, -O2) instead of FastLZ -Os; FILL records for uniform pages | pack 5.4 s -> ~1-1.5 s, unpack 1 s -> ~0.3-0.5 s (estimates) | none (format change only) | benchmark variants built |
| 3 | Drop the byte-wise CRC32 from the frozen path (release: none; debug: word checksum over packed records) | ~1.2 s save | none | to do |
| 4 | Debug-only work out of the frozen path: per-chunk RAM sums, arena/image verification at load start, synchronous log lines | ~0.5-0.9 s each way | none | partly done (finish_load logs after the thaw) |
| 5 | Find the transient power-lock veto (dispatch `0x402` ourselves with break-on-error and log the handler name; check kernel power locks; bisect GAME plugins: bright3, hold, cwcheat, battery_icon, RemoteJoyLite) | up to ~1 s per suspend in some games | none (diagnostic) | to do |
| 6 | Arm the RTC alarm at PHASE0_0 instead of the first `0x401` (normal suspend only) | small (PSPFatSave: nothing below ~4.3 s) | a missed wake | superseded by 1 |
| 7 | Load like PSPStates v2: Memory Stick read before freezing, only decompression while frozen | matters for E3 (states on the card) | none | E3 design |
| 8 | Hand-written MIPS for the LZ4 decoder (port libdragon's `lz4_dec_fast.S`, Unlicense) | maybe 2x on unpack | low | only if C LZ4 unpack < ~80 MB/s |
| 9 | Media Engine as a second CPU for compression | ~2x on an already ~1 s phase | high (game audio lives on the ME) | not worth it now |
| 10 | Raw PLL overclock beyond ARK | — | — | not needed: already 333/166 |

Rejected: bypassing the kernel power-lock check (a NAND/idstorage write could be cut: flash
risk); our own suspend/resume dispatch without scePower (touch-and-go already does it right);
"compress after the thaw" (no room for a raw 26 MB copy next to the 9 MB kernel image).

Touch-and-go details (uOFW `powerSwitch.c` 0x2530-0x2804): after PHASE0_0 and the D-cache
write-back it saves the context (`sceSuspendForKernel_67B59042`), zeroes the resume info except
the baryon clock, rewrites the reset vector and longjmps into the resume path
(`sceSuspendForKernel_B2C9640B(1)`). Our LOAD blob returns into the *save's* suspend, so the
resume follows the save's mode: a no-sleep load needs a no-sleep save (the engine records it).
Missing from a real wake and handled by the engine: the pad enables at `0xBC100078/0xBC10007C`
and GPIO outputs `0x02800000` (snapshot at `0x1000`, put back at `0x10000`). Not handled: the
zeroed battery info makes the battery driver report "no battery" until its next poll (it then
re-detects; it never forces a suspend in that state).

Expected result if 1-4 work: save ~0.5 s freeze/GE + ~1.3 s pack + ~1 s suspend events = ~3 s;
load ~0.2 s + ~1 s suspend events + ~0.4 s unpack = ~1.6 s. The remaining suspend-event time
(drivers' own handlers, VBLANK waits) is then the floor.

## 4. Experiment plan (Castlevania)

1. **Bench** (L+R+START+UP): FastLZ -Os vs -O2 vs LZ4 accel 1/4, uniform-page share, copy with
   `cache 0x18`. Picks the codec.
2. **Touch-and-go alone** (L+R+SELECT+TRIANGLE): one no-sleep suspend. Check screen, sound,
   buttons, Memory Stick LED/access, battery icon, Wi-Fi switch, then the log's timing split.
3. **Fast mode on** (L+R+SELECT+SQUARE), then the self-test (L+R+SELECT+RIGHT).
4. Fast mode on: save (L+R+START+RIGHT), move, load (L+R+START+LEFT). Quit the game normally,
   check the XMB settings.

Sources: uOFW `power` branch `src/kd/power/powerSwitch.c`, `battery.c`; uOFW `ctrl.c`,
`me_wrapper.c`, `syscon.c`, `iofilemgr.c`; PSPFatSave-Public `OriginalCommitsHistory`
(v405-v698), `utils.c`, `fatsave.c`; PspStates-Experiment v2 (`lslibrary.c`, `utils.c`); ARK-4
`core/compat/pentazemin/adrenaline.c`, `core/systemctrl/src/cpuclock.c`; lz4 v1.10.0;
pspdev/vfpu-docs reference outputs (`mem_cycles.txt`, `inst_cycles.txt`); libdragon
`src/compress/lz4_dec_fast.S`.

## 5. Results

**Speed run 2 — 2026-10-15, Castlevania.** Raw log:
[`logs/2026-10-15-speed2-castlevania.log`](logs/2026-10-15-speed2-castlevania.log).

Benchmark on the game's 24 MB:

| Codec | Pack | Unpack | Size |
|---|---|---|---|
| FastLZ L1 -Os (engine until now) | 3.74 s (6.7 MB/s) | 0.87 s | 41 % |
| FastLZ L1 -O2 | 3.59 s | 0.75 s | 41 % |
| **LZ4 accel 1 -O2** | **1.39 s (18 MB/s)** | **0.51 s (49 MB/s)** | **40 %** |
| LZ4 accel 4 -O2 | 1.02 s | 0.47 s | 42 % |

30 % of the pages are one repeated word (FILL candidates); the scan costs 72 ms. A copy that
allocates destination lines with `cache 0x18` reaches 122 MB/s against 108 MB/s without.

**Touch-and-go works.** One no-sleep suspend: request -> back in 2084 ms (system timer): queries
`0x100..0x401` 679 ms, power-lock retries 339 ms (x30), suspend phases 657 ms, resume phases
534 ms, nothing unaccounted. The I/O enables went from `0308AAD2/020000D9` to `01080002/00000098`
across the suspend and were put back; the user saw the game, sound and buttons come back. The
normal-mode save and load in the same session (fast mode was toggled on and straight off again)
measured the real sleep at ~4.5-5.5 s and the flash driver rebuild at 785 ms.

Changes for speed run 3:
- LZ4 (core/lz.c, -O2, 8 KB table) replaces FastLZ in the stream; chunks that are one repeated
  word become FILL records; the worst case reserved per chunk is now the raw size. FastLZ removed.
- Fast mode (touch-and-go) is the default; L+R+SELECT+SQUARE switches it off.
- No flash driver rebuild after a touch-and-go save (no power cycle); still after every rollback
  and after a save that really slept.
- Heavy diagnostics (`DIAG`: raw CRC, per-chunk RAM sums and diffs, flash0 read) off. The save's
  log lines stay in RAM from the start of the save (no card write while frozen). The load's
  pending flag is set in the image at PHASE0_0 after the guard (no second 9 MB checksum).
- Each trace logs its slowest events, to find the handlers behind the remaining ~2 s.

**Speed run 3 - 2026-10-15, Castlevania (fast mode, LZ4).** Raw log:
[`logs/2026-10-15-speed3-castlevania.log`](logs/2026-10-15-speed3-castlevania.log).

| | Speed run 1 | Speed run 3 |
|---|---|---|
| save, button to game running | 12.7 s | **4.0 s** (pack 1.52 s, sums 0.13 s, suspend 2.35 s) |
| load, button to game running | 9.3 s | **3.9 s** (verify 0.25 s, suspend 3.07 s incl. flash rebuild 0.78 s, unpack 0.49 s, eDRAM 0.07 s) |

Benchmark of the engine stream: LZ4 + FILL packs 24 MB in 1.39 s and unpacks in 0.50 s (41 %,
98 FILL chunks). The suspend is now the floor; its slowest events (system timer, gap after the
event): `0x200` 591 ms, `0x400A` 583 ms, `0x402` x30 retries ~340 ms; on resume `0x100000`
268 ms, `0x1000A` 122 ms, `0x40000` 100 ms (the VBLANK wait).

**Problem: after save + load the PSP went to sleep when the game was quit.** A touch-and-go
suspend zeroes the resume info (`ScePowerResumeInfo`) that a real wake gets from the IPL, and
`sceSysconResume` (event 0x10008) rebuilds the syscon driver's cached state from it: with zeroes
it believes HOLD is on, the Memory Stick and Wi-Fi are unpowered, no headphones, and the battery
driver sees no battery. Fix: before every suspend the engine reads the real state
(`sceSysconGetHoldSwitch`, `GetWlanSwitch`, `GetHPConnect`, `GetMsPowerCtrl`, `GetWlanPowerCtrl`,
`GetHRPowerCtrl`, `GetPowerSupplyStatus`, `BatteryGetStatusCap`, `BatteryGetFullCap`) and at the
first resume event writes it into the resume info; bits no getter reports (UMD switch, two unknown
key bits, `baryonStatus2`) come from the resume info of the last real wake in this boot when
there was one (else Adrenaline's 0xFFFF key word). The log shows the info found and what was
filled. Unconfirmed that this was the cause of the sleep; the next run checks the exit.

**Speed run 4 - 2026-10-15, Castlevania.** Raw log:
[`logs/2026-10-15-speed4-castlevania.log`](logs/2026-10-15-speed4-castlevania.log). Two sessions,
both the same: the save froze the game for good (no sound, no input; HOME still worked) and the
PSP slept when the game was quit. The log stops at the save's first lines because the save kept
its lines in RAM. The only new code was the live-state capture before the suspend, which issued
real syscon commands (power supply and battery reads) with the game frozen; a hung syscon command
also keeps syscon's `0x402` handler vetoing every suspend. Changes:
- The capture reads only the syscon driver's cached variables (no syscon traffic). Battery and
  supply values, the UMD switch and `baryonStatus2` come from the resume info of a real wake.
- Fast mode suspends touch-and-go only after this boot has seen a real wake: the first save of a
  session sleeps for real (slow), later ones do not; a load hands the real info back to the
  rolled-back kernel.
- Debug builds write the save's log until the suspend request again, so a hang shows where it
  stopped; an operation cut short by quitting the game keeps its P9 record for the post-mortem.

**Speed run 5 - 2026-10-15, Castlevania.** Raw log:
[`logs/2026-10-15-speed5-castlevania.log`](logs/2026-10-15-speed5-castlevania.log). First save with a
real sleep (8.0 s), second save touch-and-go (4.3 s), load touch-and-go (4.2 s): all worked, and
the real wake's resume info was captured (`size 212 wake 8C1 key FFCF7FFF supply 22 batt 1271/1790
mAh`). Quitting the game still put the PSP to sleep.

Analysis (uOFW `powerSwitch.c` worker thread): a suspend can only come from a queued request. A
"POWER switch pressed" event sets `curHardwarePowerSwitchRequest`, which then waits for all power
locks to be released - exactly what happens when a game is quit. A suspend operation resets the
request when it returns, so in speed run 2 the real-sleep saves that followed the touch-and-go
test consumed it, while in runs 3-5 the last operation was a touch-and-go and the request stayed
queued. Working hypothesis: the touch-and-go resume produces a spurious POWER-switch event.
Mitigation: `scePowerSetPowerSwMode(0)` (a plain variable in scePower) from the touch-and-go
request until 3 s after the game runs again (kernel alarm); a load releases it again because the
rollback rewinds scePower's variable. The debug probe logs every change of `scePowerIsRequest()`.

**Speed run 6 - 2026-10-15, Castlevania.** Raw log:
[`logs/2026-10-15-speed6-castlevania.log`](logs/2026-10-15-speed6-castlevania.log). With the POWER
switch guard and the third-party `hold.prx` disabled for the test, quitting the game still left a
black, dead PSP. The probe never saw a queued scePower request: the request hypothesis (speed run
5) was wrong and the guard is removed again. Separately, the first touch-and-go save after the
real-sleep save never completed (no log lines, no state); the following ones did. Still open.

**Root cause (found by reading uOFW, not yet confirmed on the device): the IPL's seeds.**
- Every suspend backs up the 4 KB reset-vector SRAM at `0xBFC00000` into scePower's resume block
  and zeroes words `0x200..0x21C` of the backup (`powerSwitch.c`, before the suspend handlers).
- A real wake runs `_scePowerResumePoint`: it reads the 8 words the IPL has just written at
  `0xBFC00200`, writes the backup back and then the 8 fresh words on top.
- Touch-and-go writes the backup back as it is, so `0xBFC00200..0x21F` end up zero.
- Those words are the seeds the IPL leaves for memlmd. Quitting the game reboots the kernel:
  `sceKernelLoadRebootBin` calls `memlmd_F26A33C3(0, 0xBFC00200)`, which rejects all-zero seeds;
  loadexec then disables interrupts, clears memory and loops forever - a black screen that looks
  like sleep or power-off. A later real wake rewrites the seeds, which is why speed run 2 (real
  sleeps after the touch-and-go test) quit normally.

Fix: the engine reads the 8 words before every suspend request (into the P9 header, so a load's
rollback cannot lose them) and writes them back at the first resume event of every touch-and-go;
fast mode is refused when the live seeds are already zero. Boot, pre-suspend and resume values are
logged.

