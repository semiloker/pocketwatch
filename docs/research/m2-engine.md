# M2 — savestate engine ladder (PSP-3000, ARK-4 6.61)

Each rung is proven on the device before the next one is built. Design inputs: M1 results
(`m1-hardware.md`) and PSPFatSave's sysevent approach (MIT).

| Rung | What it proves | Status |
|---|---|---|
| E1 | real suspend + RTC auto-wake + kernel copy at PHASE0_0, no restore | done (run 1) |
| E2 | load in the same boot (restore kernel + RAM + VRAM) | run 5 built: two root causes fixed |
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

## E2 — save and load within one boot

Code: `plugin/src/complications/states/engine.c` (+ `handoff.S`, `stream.c`, `core/freeze.c`,
`core/flash.c`). Debug-build triggers in a game: **L+R+SELECT+RIGHT** save, **L+R+SELECT+LEFT**
load, **L+R+SELECT+UP** drop the state and write the held log lines (then a probe dump).

Memory (since run 5): one P9 block (24 MB) = header, FastLZ workspace, 200-byte apply blob, kernel
image 4 MB + P8 1 MB + ME eDRAM save 4 MB, then the arena (~14.8 MB). Nothing in P11 (the firmware's
ME eDRAM save area during a suspend, see below). Nothing large in kernel RAM (module: 53 KB loaded).

SAVE
1. Refuse unless P2 is 24 MB (no highmem) and P5 (volatile) is unused.
2. Freeze the game's threads (`core/freeze.c`), wait for the GE to finish or stall.
3. FastLZ-compress game RAM (`0x88800000`, 24 MB), eDRAM (`0x84000000`, 2 MB) and the
   scratchpad (`0x00010000`, 16 KB) into the arena; debug builds also CRC the raw data.
4. Flush the log, switch it to RAM-only, request a suspend. At `0x401` arm the RTC alarm; at
   PHASE0_0 drain DMA and copy the 4 MB kernel image. `pw_sysevent_shim` (handoff.S) recorded the
   dispatcher's registers just before, inside the image.
5. After resume: thaw. The state stays in RAM; log lines stay in RAM too, because any Memory Stick
   write would make the saved kernel's file-system view stale.

LOAD
1. Freeze the game, wait for the GE.
2. Set `g_restore_pending = 1` *inside the staged kernel image*, copy the apply blob to P11.
3. Request a suspend. At PHASE0_0: drain DMA, mask interrupts, jump to the blob (uncached alias):
   it copies the image over `0x88000000`, flushes both caches, reloads the save-time registers and
   returns 0 into the save-time dispatcher.
4. The system finishes the *save's* suspend, sleeps, the RTC wakes it, every driver resumes from
   the restored kernel. The save-time thread wakes inside `pw_engine_save`, sees
   `g_restore_pending`, decompresses RAM/eDRAM/scratchpad, invalidates the caches, re-initialises
   the flash drivers, thaws the save-time frozen threads and logs the result (CRC check of game RAM
   against the save in debug builds).

Test protocol (NFS Most Wanted or any ISO):
1. In gameplay: L+R+SELECT+RIGHT, release. ~2 s black screen, game continues.
2. Play 20-30 s (drive somewhere else). Do not save the game in between.
3. L+R+SELECT+LEFT, release. Black screen, then the game must be back at the saved moment.
4. Optionally load again (the state stays valid).
5. L+R+SELECT+UP to write the log, then exit the game normally.

### Results

**Run 1 — 2026-10-05, NFS Most Wanted.** Raw log:
[`logs/2026-10-05-e2-run1-nfs.log`](logs/2026-10-05-e2-run1-nfs.log).

- SAVE worked: 6 game threads frozen in 107 ms (none forced), GE idle, game RAM 24 MB -> 12.7 MB,
  eDRAM 2 MB -> 1.3 MB, scratchpad 16 KB. Packing (FastLZ + CRC at the game's CPU clock) took
  **6.2 s** - to be optimised. The user saw a short freeze, then 2-3 s of black screen.
- LOAD failed: about 10 s of black screen, then the PSP was back in the XMB. The log has nothing
  about it: while a state is held, lines stay in RAM, and the session ended before they were
  written. The timing fits "rollback + resume + restore happened, then the game crashed", but
  that is unproven.
- Differences from PSPFatSave's working load: we restored the scratchpad (it does not), and we
  wrote nothing until the very end.

Changes for run 2:
- The scratchpad is no longer restored (only compared, to learn whether games use it).
- After the rollback every step is logged and written immediately (safe: nothing was written
  since the save, so card and restored kernel agree). States become single-use.
- Breadcrumbs (`S_*` stages) in the P11 record; at the next boot the plugin reports a dead
  session's last stage and traces (`post_mortem`), including records from the run-1 build
  (`PWS1` layout) if that memory survived.
- If decompression fails the game stays frozen instead of running on corrupt RAM.

**Run 2 — 2026-10-05, NFS Most Wanted:** the load again ended in the XMB. No data: the user
restarted NFS (as instructed, to trigger the post-mortem), which truncated the log, and the P11
record did not survive the trip through the XMB. Lessons and changes for run 3:
- The log now appends sessions (`==== new session ====`) and only restarts past 256 KB.
- The P11 post-mortem is not reliable across a return to the XMB; the card log is the channel.
- Partition 8 (1 MB at `0x8BB00000`, 343 KB used since boot by something we have not identified)
  is outside the kernel range but holds live data. It is now copied with the kernel image at the
  save's PHASE0_0 and put back at the load's PHASE0_0, before the kernel. PSPFatSave never had to:
  the PSP-1000 has no P8.
- finish_load logs before each step, and for 5 s after the thaw logs how many game threads are
  still alive.

**Run 3 — 2026-10-05, NFS Most Wanted.** Raw log:
[`logs/2026-10-05-e2-run3-nfs.log`](logs/2026-10-05-e2-run3-nfs.log).
- SAVE fine again (6 threads frozen, one forced: a video was playing, `PspMpegStreamer::Thread`;
  game RAM 24 MB -> 9.1 MB, eDRAM -> 1.0 MB, P8 1 MB copied; packing 5.1 s).
- LOAD: back to the XMB, and **not a single line after the rollback**, although finish_load now
  writes its first line as soon as the save-time thread runs. The user saw only a black screen,
  never the frozen game frame. So the failure is before the display comes back on: in the kernel
  apply itself or in the early resume phases of the restored kernel, not in the RAM restore and not
  in the game.

Experiments for run 4 (the next build):
- The kernel snapshot is taken with interrupts masked (a torn snapshot was one suspect).
- The RTC wake-up for LOAD is armed 1.5 s ahead instead of 100 ms: the uncached apply loop runs
  after the alarm is set and may outlast 100 ms.
- Flash re-initialisation is off for same-session loads (`FLASH_REINIT_ON_LOAD`).
- **Self-test, L+R+START+RIGHT:** at PHASE0_0, copy the live kernel and write it straight back
  through the blob. No world switch: if the game survives, the blob, cache flushes and context
  restore are sound and the problem is the saved world.
- **Frozen save + load, L+R+START+LEFT:** save, keep the game frozen, load at once. The smallest
  possible difference between the saved world and the one being replaced.

**Run 4 — 2026-10-15, NFS Most Wanted:** the self-test alone (L+R+START+RIGHT, no world switch)
gave ~10 s of black screen and the XMB, exactly like every load. So the rollback *mechanics* were
broken, not the saved world. Analysis (code review against PSPFatSave, uOFW 6.60 sources) found two
independent faults, each fatal on its own, present in every load since the first E2 build:

1. **The apply blob lived in P11, which the firmware overwrites during every suspend.** On the
   PSP-2000/3000, P11 (0x8BC00000, 4 MB) is the ME eDRAM save area: at event `0x4005` (PHASE0_5,
   before our PHASE0_0) `me_wrapper` has the Media Engine dump its eDRAM there. The jump at
   PHASE0_0 landed in ME data. The state header and arena region 1 (part of the packed game RAM)
   were destroyed the same way, which is also why the P11 "post-mortem" record never survived.
   PSPFatSave never hit this: it runs the blob from game RAM on a PSP-1000.
2. **Interrupts were masked with `sceKernelCpuSuspendIntr`**, which clears the Allegrex interrupt
   controller register (IC, `mtic`). The blob restores only CP0 Status, so after it returned the
   whole rest of the suspend, and the resumed system, ran with every interrupt off. PSPFatSave
   masks with Status.IE, which the blob's Status reload undoes.

Changes for run 5:
- Everything that must survive a suspend moved to one P9 block; P11 is not used at all.
- The ME eDRAM image in P11 is now part of the snapshot: copied (uncached: the ME wrote it, not the
  CPU) at the save's PHASE0_0 and put back at the load's PHASE0_0, so the restored kernel reboots
  the ME from the matching image. PSPStates does the same (it restores 0x8BC00000 with the kernel).
- Interrupts for the apply are masked through Status.IE; the save's capture keeps a matched
  `sceKernelCpuSuspendIntr`/`ResumeIntr` pair and logs the IC state found at PHASE0_0.
- The blob runs cached from P9 (after D-cache write-back + I-cache invalidate): ~tens of ms instead
  of ~1 s of uncached fetches. Its I-cache flush is loadcore's (`cache 0x4` twice per index) rather
  than the pre-IPL's tag stores, and it records its own run time in `pw_handoff_ctx[13]`.
- Guards: the packed arena and the kernel image (+ P8 + ME) are checksummed at save; LOAD refuses a
  damaged state before the point of no return, and at PHASE0_0 the blob and the image are checked
  again right before the jump. A failed guard skips the apply (the system just wakes up) and logs
  which part was damaged instead of crashing.
- The self-test runs two suspends per press: the blob with nothing to copy (jump, cache flushes,
  context + interrupt restore), then with the 4 MB kernel copy; the card log is flushed before each.
- After a load (debug builds) flash0 is read once to learn whether the flash drivers need the
  PSPFatSave-style re-initialisation (still off).

Not adopted yet (from the same review): PSPStates/Adrenaline restore at `_scePowerResumePoint` /
the final suspend call site instead of PHASE0_0, from the scratchpad or VRAM. Kept as plan B if the
PHASE0_0 route still fails with the faults above fixed. PSPFatSave also checks that no frozen thread
holds the `SceMediaEngineRpc` mutex before suspending; ours does not yet.

Test protocol for run 5 (NFS Most Wanted):
1. In gameplay: L+R+START+RIGHT (self-test). Two short black screens; the game must carry on.
2. L+R+START+LEFT (frozen save + load). Black screens, then the game must carry on.
3. L+R+SELECT+RIGHT (save), play 20-30 s, L+R+SELECT+LEFT (load): the game must jump back.
4. Exit the game normally; after any drop to the XMB just connect USB (the log keeps sessions).
