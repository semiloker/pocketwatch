# States engine — specification

| | |
|---|---|
| Area code | `STE` |
| Status | Done: E1 (capture), E2 (same-boot load), fast mode. Planned: E4, single engine owner, P5 games |
| Milestones | M2 (done), E4, M6 |
| Code | [`engine.c`](../../plugin/src/complications/states/engine.c), [`engine.h`](../../plugin/src/complications/states/engine.h), [`handoff.S`](../../plugin/src/complications/states/handoff.S), [`stream.c`](../../plugin/src/complications/states/stream.c), [`core/freeze.c`](../../plugin/src/core/freeze.c), [`core/flash.c`](../../plugin/src/core/flash.c), [`core/lz.c`](../../plugin/src/core/lz.c) |
| Research | [m1-hardware](../research/m1-hardware.md), [m2-engine](../research/m2-engine.md), [m2-speed](../research/m2-speed.md), [backlog](../backlog.md) |

## A. Overview

- **Purpose:** capture the complete state of a running retail game and put it back later, so the
  game continues from the captured moment as if nothing happened.
- **User benefit:** save anywhere, retry hard parts, keep moments the game cannot save.
- **Scope:** the in-RAM state (P9), the save and load procedures, the suspend-time capture and
  rollback, post-resume repairs (flash drivers, touch-and-go fixes), timings and diagnostics.
- **Non-goals:** where states are stored across reboots (→ [states storage](states-storage.md)),
  how the user triggers them (→ [game shell](game-shell.md), [UI](../ui/README.md)).
- **Current state (VERIFIED, E2 run 7, speed run 7, menu test 2026-10-16):** save → play → load
  works on the PSP-3000 in NFS Most Wanted and Castlevania DXC; quitting to the XMB afterwards is
  safe; system settings survive. One state, held in P9, single-use (discarded after a load).
- **Target state:** multiple loads per state, states fed from and to the card (E3), a 100-cycle
  integrity gate (E4), saves and loads of ~1-2 s (speed backlog).

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `STE-REQ-001` | A save captures game RAM (P2, 24 MB), eDRAM (2 MB), scratchpad (16 KB), the kernel image `0x88000000-0x88400000`, P8 (1 MB) and the ME eDRAM save (P11, 4 MB). | P0 | Done | E2 run 7 log |
| `STE-REQ-002` | A load returns the game to the saved moment in the same boot; all game threads run afterwards. | P0 | Done | E2 run 7, menu test 2026-10-16 |
| `STE-REQ-003` | Save refuses when P2 is not 24 MB (highmem) or the game uses P5; the refusal is logged (native message: M3b). | P0 | Done | `check_environment()` |
| `STE-REQ-004` | No load runs without the flash driver rebuild; a load refuses when the drivers cannot be found. | P0 | Done | E2 runs 6-7, [ADR-006](../architecture/decisions.md#adr-006--rebuild-the-flash-drivers-after-every-resume) |
| `STE-REQ-005` | Everything that must survive a suspend lives in P9; nothing is kept in P11 or P5 across one. | P0 | Done | [ADR-004](../architecture/decisions.md#adr-004--state-data-lives-in-p9-only) |
| `STE-REQ-006` | A damaged state is refused before the point of no return (arena and image checksums); the blob and image are checked again at PHASE0_0 and a failed check skips the apply. | P0 | Done | code review, E2 run 5 |
| `STE-REQ-007` | After a touch-and-go suspend the IPL seeds are put back; quitting the game afterwards reaches the XMB. | P0 | Done | speed run 7 |
| `STE-REQ-008` | Callers that froze the game themselves use `pw_engine_save_held` / `pw_engine_load_held`; the engine neither freezes nor thaws for them. | P0 | Done | menu test 2026-10-16 |
| `STE-REQ-009` | While a state is held the engine writes nothing to the Memory Stick (log lines deferred until `pw_engine_discard`). | P0 | Done | code (`pw_log_set_deferred`) |
| `STE-REQ-010` | Each save and load logs its phase timings, suspend trace summary and outcome ("save OK" / "LOAD complete"). | P1 | Done | device logs |
| `STE-REQ-011` | Only one engine operation at a time: a second request (menu vs debug probe) is refused, not interleaved. | P1 | Planned | code review + host test of the lock |
| `STE-REQ-012` | A state can be loaded more than once (needs [STS](states-storage.md) or a proven-safe policy for card writes after a load). | P1 | Planned | T1/T2 |
| `STE-REQ-013` | Games that use P5 (volatile memory) are supported. | P2 | Planned | compatibility matrix |
| `STE-NFR-001` | Save, button to game running, fast mode after the first save of a boot: ≤ 4.5 s on Castlevania (target ≤ 2 s after S1-S7). | P1 | Done (4.2 s) | device log |
| `STE-NFR-002` | Load, button to game running: ≤ 4 s (target ≤ 2 s). | P1 | Done (3.9 s) | device log |
| `STE-NFR-003` | First save of a boot (real sleep): ≤ 8 s (target ≤ 4.5 s with S4). | P2 | Done (7.8 s) | device log |
| `STE-NFR-004` | File-system integrity: 100 save/load cycles with game saves in between leave the card's FAT and files intact (E4). | P1 | Planned | E4 protocol + PC FAT check |
| `STE-NFR-005` | No engine buffer in kernel RAM beyond the module's static data; the engine adds no polling thread (sysevent handler only). | P0 | Done | `make check`, code |

## C. Architecture and integration

**Memory layout of the P9 block** (`engine.c`, `HEADER_AT` ...): `StateHeader` at 0, codec
workspace at 0x8000, apply blob at 0x10000 (4 KB), kernel image at 0x20000 (4 MB) followed by P8
(1 MB) and the ME eDRAM save (4 MB), then the packed arena (~14.8 MB). The header holds timings,
traces, breadcrumbs (`S_*` stages), checksums, IPL seeds, I/O-enable snapshots and resume info.

**Save** ([architecture §5](../01-system-architecture.md#5-key-flows)): environment check →
freeze (unless held) → wait for the GE → pack P2/eDRAM/scratchpad through
[`stream.c`](../../plugin/src/complications/states/stream.c) (64 KB chunks, LZ4 or FILL or raw) →
checksums → deferred log → suspend request (touch-and-go when fast mode is on, a real wake was seen
and the IPL seeds are present) → in the sysevent handler: RTC alarm at `0x401` (real sleep), seeds
and I/O enables at `0x1000`, capture at PHASE0_0 → resume: seeds/I-O/resume-info fixes at
`0x10000`, flash driver rebuild at RESUME_COMPLETED (skipped after touch-and-go) → thaw (unless
held) → `PW_ENGINE_SAVED`.

**Load:** verify the arena/image checksums → freeze (unless held) → set `g_restore_pending` in the
staged image → suspend → at PHASE0_0 the blob copies the image (and P8, ME save) back and returns
into the save-time dispatcher → the system finishes the *save's* suspend and resumes the restored
kernel → `finish_load` in the save-time thread unpacks RAM/eDRAM, rebuilds the flash drivers
(remounting flash0/flash1), thaws (unless held), checks thread liveness for 5 s in debug builds →
`pw_engine_save` returns `PW_ENGINE_LOADED`. `pw_engine_load` returns only on failure.

**Interfaces:** [`engine.h`](../../plugin/src/complications/states/engine.h) (`pw_engine_save`,
`_load`, `_save_held`, `_load_held`, `_has_state`, `_discard`, debug: `_selftest`,
`_save_load_frozen`, `_set_fast`, `_tg_test`). Used by the menu
([`ui/menu.c`](../../plugin/src/ui/menu.c)) and the debug probe.

**Shared resources:** P9 block (owned), sysevent handler (owned), game threads (borrowed from the
caller when held), the Memory Stick (forbidden while holding a state).

**Failure handling:** every failed save thaws and releases the staging; a failed load (suspend
refused, guard failed) thaws and logs crumbs and traces; a damaged P9 block is discarded.

## D. User experience and visual design

Not applicable (no own UI). User-visible effects: a short black screen during a real sleep (first
save of a boot, and loads of a state saved with one); a frozen frame otherwise. Messages come from
the UI.

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| PSPFatSave | <https://github.com/Grunwalskii/PSPFatSave-Public> | sysevent-based capture, flash driver rebuild, PSP-1000 timings | adopted the approach; adapted to P9/64 MB | MIT |
| PSPStates v2 (Dark_Alex) | <https://github.com/PSP-Archive/PspStates-Experiment> | restore at `_scePowerResumePoint`, ME image restore | ideas only (plan B) | none — never copy |
| uOFW 6.60 power/me_wrapper | <https://github.com/uofw/uofw> | suspend phases, P11 as ME save area | reference for analysis | GPL-3.0 |
| ARK-4 / Adrenaline | <https://github.com/PSP-Archive/ARK-4> | resume info defaults for touch-and-go | reference | GPL-3.0 |

## F. Implementation plan

1. **Engine owner lock** (`STE-REQ-011`) when HOME lands (M3a): one `pw_engine_begin/end` around
   every operation; the probe and the hub both go through it.
2. **E3 hooks** for [STS](states-storage.md): stage from/to file buffers in P9, save-time addresses
   in the header, freshness probe after the rollback.
3. **Split diagnostics** (`diag.c`: traces, crumbs, timing logs) out of `engine.c` before E3
   (system map finding A5).
4. Speed items S1-S7 between milestones ([backlog](../backlog.md)).

Blockers: T1 for `STE-REQ-012`.

## G. Testing and acceptance

| Requirement | Test | Acceptance criterion | Needs |
|---|---|---|---|
| STE-REQ-001/002 | Save in gameplay, play 20-30 s, load (menu or L+R+START+RIGHT/LEFT) | game back at the saved moment, threads alive, quit to XMB works, settings intact | PSP, an ISO |
| STE-REQ-006 | Debug: corrupt one byte of the arena before a load | load refused before the freeze, game unaffected | debug hook (to add) |
| STE-REQ-007 | Fast-mode save + load, quit to the XMB | XMB reached, next game boots | PSP |
| STE-NFR-001/002 | Timings from the log | within the numbers above | log |
| STE-NFR-004 | E4: 100 cycles with game saves between | PC FAT check clean every 10 cycles, file hashes unchanged | PSP, PC, card image |
| Self-tests | L+R+SELECT+RIGHT (rollback mechanics), L+R+SELECT+LEFT (frozen save + load) | game continues | debug build |

## H. Risks and decisions

- Risks: [RISK-001](../risk-register.md#risk-001--memory-stick-fat-corruption-after-a-rollback), [RISK-002](../risk-register.md#risk-002--flash1-corruption-after-a-rollback), [RISK-003](../risk-register.md#risk-003--hang-on-quit-after-a-touch-and-go-suspend), [RISK-004](../risk-register.md#risk-004--game-specific-incompatibility), [RISK-014](../risk-register.md#risk-014--intermittent-device-behaviour).
- Decisions: [ADR-004](../architecture/decisions.md#adr-004--state-data-lives-in-p9-only), [ADR-005](../architecture/decisions.md#adr-005--capture-and-roll-back-the-kernel-at-phase0_0-of-a-system-suspend), [ADR-006](../architecture/decisions.md#adr-006--rebuild-the-flash-drivers-after-every-resume), [ADR-007](../architecture/decisions.md#adr-007--lz4-chunk-codec-with-fill-records), [ADR-008](../architecture/decisions.md#adr-008--touch-and-go-suspend-by-default).
- Open questions: the one first touch-and-go save that never completed (speed run 6); which
  handlers cost ~0.56 s each at `0x200`/`0x400A` (S1); whether a frozen thread can hold the
  `SceMediaEngineRpc` mutex (PSPFatSave checks it, we do not yet).
