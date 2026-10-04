# M2 — savestate engine ladder (PSP-3000, ARK-4 6.61)

Each rung is proven on the device before the next one is built. Design inputs: M1 results
(`m1-hardware.md`) and PSPFatSave's sysevent approach (MIT).

| Rung | What it proves | Status |
|---|---|---|
| E1 | real suspend + RTC auto-wake + kernel copy at PHASE0_0, no restore | in progress |
| E2 | load in the same boot (restore kernel + RAM + VRAM) | |
| E3 | load after a reboot (environment fingerprint) | |
| E4 | file-system integrity gate (100 cycles + FAT check) | |

## E1 — capture during a real suspend

Debug build, in a game hold **L + R + SELECT + RIGHT** and let go
(`plugin/src/debug/spike_suspend.c`):

1. 4 MB staging buffer from P9.
2. A sysevent handler (mask `0x00FFFF00`, registered at plugin start) records every event ID.
   - `0x401` (POWER_LOCK_PHASE1): arm an RTC alarm for now + 100 ms.
   - `0x4000` (SUSPEND PHASE0_0): wait for the DMA controllers to go idle, copy
     `0x88000000..0x88400000` to the staging buffer, write back the D-cache.
   - `0x400000` (RESUME_COMPLETED): clear the alarm, flag the resume.
3. `scePowerRequestSuspend()`; the screen turns off and should come back by itself.
4. After resume: log the request result, wall-clock time to resume, copy duration (CP0 cycles),
   the full event trace, the snapshot CRC, and whether sysmem's code in the snapshot matches
   live memory (it never changes, so it must).

Nothing is restored; for the game it is a quick sleep/wake.

### Results

**Run 1 — 2026-10-04, NFS Most Wanted (ULUS-10036).** Raw log:
[`logs/2026-10-04-e1-nfs-most-wanted.log`](logs/2026-10-04-e1-nfs-most-wanted.log).

- `scePowerRequestSuspend` returned 0; **the RTC alarm woke the PSP-3000 by itself**
  (`resumed=1 alarm=1`). The user saw about 2 s of black screen and the game continued.
- **Kernel capture works**: `captured=1`, DMA controllers idle (`dmac_left=0`), and the first
  64 KB of the snapshot (sysmem's code) equal live memory. The 4 MB copy + D-cache write-back
  took 8.0 M CP0 cycles (tens of ms).
- Wall clock request -> resume: **6.1 s**. The trace explains it:
  `100 102 210..200 400`, then **`401 402` repeated ~22 times**, then `1000 400F..4000`
  (capture at `4000`), then resume `10000 10001 ...`. 0x402 after 0x401 means a power-lock
  holder refused and the firmware retried. The game streams from the disc constantly
  (`DVDUMD_READ` thread), the likely holder. The real engine freezes game threads first, which
  should end the retries; E1 run 2 measures the phase timings.
- The 96-event buffer overflowed during the resume phases; now 256 events, repeats folded, and
  each event timestamped (CP0 Count, calibrated against the kernel clock).
- The self-check flagged `sceRtc 0x3F7AD767` as unresolved, a false positive: a kernel module
  importing a *user* library gets `jr $ra; syscall N` stubs, which the check took for the
  placeholder. The classifier is now a tested function (`core/stubinfo.c`) that reports direct,
  syscall and unresolved stubs separately.
