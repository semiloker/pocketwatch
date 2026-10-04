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
_Pending._
