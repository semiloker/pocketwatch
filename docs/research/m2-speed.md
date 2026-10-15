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
