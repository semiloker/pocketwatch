# States storage — specification

| | |
|---|---|
| Area code | `STS` |
| Status | In Progress: implemented and host-tested (2026-10-16), device tests pending ([testing guide](../testing.md#4-states-on-the-memory-stick)) |
| Milestones | E3 (states on the card, cross-boot load), M5 (native SAVEDATA UX) |
| Code | [`store.c`](../../plugin/src/complications/states/store.c), [`core/game.c`](../../plugin/src/core/game.c), [`ui/slots.c`](../../plugin/src/ui/slots.c), [`sfo.c`](../../plugin/src/complications/states/sfo.c), [`savedata_hash.c`](../../plugin/src/complications/states/savedata_hash.c), [`core/png.c`](../../plugin/src/core/png.c), engine E3 API in [`engine.c`](../../plugin/src/complications/states/engine.c) |
| Tests | [`tests/test_store.c`](../../tests/test_store.c) (fake engine and game, `ms0:/` in a temp dir), [`tests/test_png.c`](../../tests/test_png.c) |
| Research | [m3-menu-e3 phase 2](../research/m3-menu-e3.md#phase-2--e3-states-on-the-memory-stick-loads-after-a-reboot), [m1-hardware S2/S2b](../research/m1-hardware.md#s2--are-hash-less-saves-listed-by-the-xmb) |

## A. Overview

- **Purpose:** keep states on the Memory Stick so they survive a reboot, as native saved data the
  XMB lists, and load them safely in a later boot.
- **User benefit:** five slots per game, states that survive power-off, browse and delete them in
  the XMB (Game → Saved Data Utility) with the screenshot.
- **Non-goals:** the capture/rollback itself (→ [engine](states-engine.md)).
- **Current state:** implemented. A save goes to RAM first (the engine), then the hub writes it to
  the chosen slot. A load from a slot reads it into the engine's P9 block, checks it, then rolls
  back. States of games without a disc ID (PSN EBOOTs, homebrew) stay in RAM only.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `STS-REQ-001` | A state written to the card can be loaded in a later boot of the same game and environment (E3). | P0 | Implemented | T3, T6 |
| `STS-REQ-002` | ~~Slot files exist at full size before the capture; no allocation after it.~~ Superseded by T1 (2026-10-16, VERIFIED with a real-sleep save): after a rollback the FAT driver sees files and allocations made after the capture, so the slot files are written after the capture. Guarded by `STS-REQ-008`. | — | Superseded | T1, T1b |
| `STS-REQ-003` | STATE.BIN (the commit record) is zeroed first and written last; an interrupted write leaves an empty slot, never a loadable broken one. | P0 | Implemented | `store_interrupted_save_leaves_an_empty_slot`, power-cut test |
| `STS-REQ-004` | ~~`sceIoSync` before every capture.~~ Not needed: our files are closed before any save starts and none are written before the capture. | — | Superseded | code review |
| `STS-REQ-005` | A load reads the whole state into P9 and verifies checksums and fingerprint before the freeze; a failure leaves the game running untouched. | P0 | Implemented | `store_detects_damaged_files`, T5 |
| `STS-REQ-006` | The fingerprint refuses another game (disc ID), another ISO (path, size), another Pocketwatch build (`PW_ENGINE_BUILD` + version string), another model or devkit, another memory layout (P9 staging, P8, ME save area, the assets and PARAM.SFO blocks); checked from STATE.BIN before the state in RAM is dropped. | P0 | Implemented (narrower than first proposed: no ARK config or module-list hash) | `store_refuses_another_game_iso_or_build`, T5 |
| `STS-REQ-007` | The header stores the save-time addresses of `g_restore_pending` and `pw_handoff_ctx`; hardware data (IPL seeds, live switch state, resume info) always comes from the current boot. | P0 | Implemented | T3 |
| `STS-REQ-008` | After a rollback from a slot, game frozen, before any card write: the restored world checks that the marker file created by that save (a new name per save, `PWxxxxxx.BIN`), STATE.BIN and DATA.BIN's size are visible. On failure card writes stop until reboot and the hub says "Do not save in the game: quit and restart the PSP." | P0 | Implemented | `store_check_after_load_sees_the_marker`, T1b |
| `STS-REQ-009` | ARK's `mscache` cannot serve stale data: its 4 KB msstor window is dropped by a getstat on `msstor0p1:/` before the check (VERIFIED in ARK-4 `msstor_cache.c`, T1). | P0 | Implemented | T1 |
| `STS-REQ-010` | Card I/O in 1 MB requests directly from/to P9; short writes fail the save with "The Memory Stick could not be written or read." | P1 | Implemented | log timings |
| `STS-REQ-011` | Each slot is a SAVEDATA folder `ms0:/PSP/SAVEDATA/PW<DISCID><NN>/` with a chnnlsv-hashed PARAM.SFO (TITLE = game title, SAVEDATA_TITLE = "Save State N", DETAIL = "Pocketwatch"), ICON0.PNG 144x80, PIC1.PNG 480x272 (the game frame) and the state files. | P1 | Implemented | `store_save_writes_native_savedata_folder`, XMB check |
| `STS-REQ-012` | A slot deleted or damaged from the XMB or a PC is shown as empty or refused, never loaded. | P1 | Implemented | `store_detects_damaged_files`, `store_delete_removes_the_folder` |
| `STS-NFR-001` | Writing a ~18 MB state adds ≤ 1.5 s to a save (target; INFERRED ~14 MB/s for aligned 1 MB writes). | P1 | Unmeasured | log: `store: state written` vs the save timing |

## C. Architecture and integration

Files per slot (all written after the engine's capture, in this order):

| File | Content |
|---|---|
| STATE.BIN | zeroed first (the slot is empty until the end), rewritten last: magic `PWST`, version, part lengths, header checksum, environment copy, disc ID, ISO path + size, local save time, build string, marker stamp |
| DATA.BIN | engine header (`StateHeader`), kernel image (+ P8 + ME save), packed game RAM / eDRAM / scratchpad |
| ICON0.PNG, PIC1.PNG | the frozen game frame (144x80 area-averaged, 480x272), stored-deflate PNG |
| PARAM.SFO | built and chnnlsv-hashed once at plugin start for all five slots (chnnlsv unloaded again) |
| PWxxxxxx.BIN | marker with a new name per save (the previous one is removed) |

```mermaid
sequenceDiagram
    participant H as Hub
    participant E as Engine
    participant S as Store
    participant MS as Memory Stick
    H->>H: pick a slot (state list)
    H->>E: save into RAM (load hook = store check, part of the snapshot)
    E-->>H: SAVED (game still frozen)
    H->>S: pw_store_save(slot)
    S->>MS: STATE.BIN zeroed, DATA.BIN, PNGs, PARAM.SFO, marker, STATE.BIN last
    Note over H,MS: load: pick a slot, pw_store_load reads DATA.BIN into P9 (skipped if RAM holds it),<br/>pw_engine_adopt checks it, pw_store_arm, rollback; restored world: marker check, then the game runs
```

Interfaces: `pw_store_init/shutdown`, `pw_store_scan`, `pw_store_save`, `pw_store_load`,
`pw_store_delete`, `pw_store_arm` + `pw_store_after_load` (engine load hook),
`pw_store_check_failed`, `pw_store_reason`. Engine side: `pw_engine_export/import/adopt`,
`pw_engine_env_now`, `pw_engine_set_env_extra` (extra[0] = the assets block address).

## D. User experience and visual design

The state list follows the Saved Data Utility (selected icon 144x80 at (27,97) with a white
frame, the others 81x45 above and below, details right of it); empty slots show a plus and
"New Save State (N)"; △ deletes after a confirmation. Saving/Loading show "Saving... 42%".
Refusals use `pw_store_reason`: another game or ISO, another setup, damaged, card error, card
locked.

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| PSPStates / Custom Home states | [PspStates-Experiment](https://github.com/PSP-Archive/PspStates-Experiment) | cross-boot loads work in practice | idea only | none — never copy |
| PSPFatSave | [PSPFatSave-Public](https://github.com/Grunwalskii/PSPFatSave-Public) | "the plugin loads at a different kernel address each boot" | store save-time addresses | MIT |
| PPSSPP SavedataParam / PSPSaveDialog | [SavedataParam.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/SavedataParam.cpp) | SFO hash (mode 0x01), list layout | implemented independently | GPL-2.0+ |

## F. Open points

- T1 was VERIFIED with a state saved through a real sleep. States saved touch-and-go (fast mode,
  every save after the first of a session) roll back without a power cycle; the marker check
  guards them until T1b confirms ([testing guide](../testing.md#t1b)).
- What else differs between boots in the kernel image (RESEARCH REQUIRED, T3/T6); the IPL seeds
  were identical in every boot logged so far (INFERRED: per console).
- The ISO's clusters are not fingerprinted: an ISO defragmented or copied back between save and
  load has the same path and size (RISK-006).

## G. Testing and acceptance

Host: `make -C tests run T=store`. Device: [testing guide](../testing.md) sections 4-6 (T1b-T7).
Acceptance: all pass, PC FAT check clean after T4 and T7.

## H. Risks and decisions

- [RISK-001](../risk-register.md#risk-001--memory-stick-fat-corruption-after-a-rollback), [RISK-006](../risk-register.md#risk-006--cross-boot-load-into-a-different-environment), [RISK-011](../risk-register.md#risk-011--power-loss-or-card-removal-during-a-state-write).
- [ADR-010](../architecture/decisions.md#adr-010--states-are-native-hashed-savedata-folders), [ADR-017](../architecture/decisions.md#adr-017--state-files-are-written-after-the-capture).
