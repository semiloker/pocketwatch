# States storage — specification

| | |
|---|---|
| Area code | `STS` |
| Status | Planned. Helpers Done: PARAM.SFO writer, chnnlsv hashing, PNG encoder (host-tested; on-device S2b spike passed) |
| Milestones | E3 (states on the card, cross-boot load), M5 (native SAVEDATA UX) |
| Code | [`sfo.c`](../../plugin/src/complications/states/sfo.c), [`savedata_hash.c`](../../plugin/src/complications/states/savedata_hash.c), [`core/png.c`](../../plugin/src/core/png.c), [`tools/sfo.py`](../../tools/sfo.py) |
| Research | [m3-menu-e3 phase 2](../research/m3-menu-e3.md#phase-2--e3-states-on-the-memory-stick-loads-after-a-reboot), [m1-hardware S2/S2b](../research/m1-hardware.md#s2--are-hash-less-saves-listed-by-the-xmb) |

## A. Overview

- **Purpose:** keep states on the Memory Stick so they survive a reboot, as native saved data the
  XMB lists, and load them safely in a later boot.
- **User benefit:** many slots per game, states that survive power-off, browse and delete them in
  the XMB with screenshots.
- **Non-goals:** the capture/rollback itself (→ [engine](states-engine.md)); the list UI
  (→ [UI](../ui/README.md)).
- **Current state:** none on the device yet; states live only in RAM and are single-use.
- **Target state:** `ms0:/PSP/SAVEDATA/PW<GAMEID><NN>/` per slot ([ADR-010](../architecture/decisions.md#adr-010--states-are-native-hashed-savedata-folders)).

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `STS-REQ-001` | A state written to the card can be loaded in a later boot of the same game and environment (E3). | P0 | Planned | T3, T6 |
| `STS-REQ-002` | Slot files exist at their full size before the kernel capture and are only overwritten in place afterwards: no cluster allocation and no directory change after the capture. | P0 | Planned | code review + T4 FAT check |
| `STS-REQ-003` | Header and CRC are written last; an interrupted write leaves an invalid slot, never a broken FAT; the previous slot content stays valid until the new one commits. | P0 | Planned | power-cut test |
| `STS-REQ-004` | Before every capture our files are closed and `sceIoSync("ms0:")` has returned. | P0 | Planned | code review |
| `STS-REQ-005` | A load reads the whole state into P9 and verifies CRC and fingerprint while the game still runs; only then freezes. | P0 | Planned | T2, T5 |
| `STS-REQ-006` | The fingerprint covers model/board, firmware, ARK version and SE config (clocks, mscache, infernocache, highmem), module list, P2/P5/P8/P9 info, Pocketwatch build and layout version, apitype, init file, ISO path/size/mtime, disc ID; any mismatch refuses with a native message. | P0 | Planned | T5 |
| `STS-REQ-007` | The header stores the save-time addresses of `g_restore_pending` and `pw_handoff_ctx`; hardware data (IPL seeds, live switch state, resume info) always comes from the current boot. | P0 | Planned | T3 |
| `STS-REQ-008` | After a rollback, game still frozen, before any card write: a freshness probe checks that a file created after the save is visible; on failure no card write happens until reboot and the user is told to quit without saving. | P0 | T1 experiment implemented (debug hub item, `debug/cardtest.c`), device run pending | T1 |
| `STS-REQ-009` | ARK's `mscache` cannot serve stale data after a rollback: its single 4 KB msstor window is dropped by any non-read msstor call (VERIFIED in ARK-4 `msstor_cache.c`), so the freshness probe first makes one; refuse only if T1 shows that is not enough. The owner's ARK has `mscache` on. | P0 | Revised 2026-10-16; T1 checks it | T1, T5 |
| `STS-REQ-010` | Card I/O in 1 MB requests at 64 KB-aligned offsets directly from/to P9; no free-space devctl; short writes are detected and fail the save. | P1 | Planned | log timings |
| `STS-REQ-011` | Each slot is a SAVEDATA folder with a chnnlsv-hashed (mode 0x01) PARAM.SFO (TITLE = game title, SAVEDATA_TITLE = "Save State N", DETAIL = date/time), ICON0.PNG 144x80, PIC1.PNG 480x272 (the screenshot) and the state files. | P1 | Planned (helpers Done) | XMB shows it (S2b method) |
| `STS-REQ-012` | A slot deleted or damaged from the XMB or a PC is detected and shown as empty or broken, never loaded. | P1 | Planned | test with a deleted/truncated folder |
| `STS-NFR-001` | Writing a ~15 MB state adds ≤ 1.5 s to a save (target; prior art claims ~14 MB/s for aligned 1 MB writes, **INFERRED**, M1 measured 2-3 MB/s for small PNG writes). | P1 | Planned | log timings |

## C. Architecture and integration

**PROPOSED layout per slot** (from the E3 design): the variable-size packed data (game RAM,
eDRAM, scratchpad) is written *before* the suspend, while the game is frozen (allocation is fine
before the capture); the fixed-size kernel part (kernel image + P8 + ME save, 9 MB) goes to a file
created once at full size and overwritten in place after the resume; the small header with CRCs
and the commit flag is written last. M5 wraps this into the SAVEDATA folder with SFO and PNGs.

```mermaid
sequenceDiagram
    participant H as Hub
    participant S as Storage
    participant E as Engine
    participant MS as Memory Stick
    H->>S: save to slot N
    S->>E: pack (game frozen)
    S->>MS: write packed data, ensure kernel file at full size
    S->>MS: close all, sceIoSync
    S->>E: suspend + capture (no I/O)
    S->>MS: overwrite kernel file in place, write header + CRC last
    Note over S,MS: load: read all into P9, verify CRC + fingerprint, then freeze and roll back;<br/>after the rollback: freshness probe before any write
```

Interfaces (PROPOSED): `pw_store_save(slot)`, `pw_store_load(slot)`, `pw_store_list(game)`,
`pw_store_delete(slot)`; engine hooks to stage from a P9 buffer and to record save-time addresses.
Ownership: the slot folders; card writes go through the [write gate](core-runtime.md#b-requirements) (`SYS-REQ-012`).

## D. User experience and visual design

The state list follows the Saved Data Utility ([UI spec](../ui/README.md)); "Saving..." /
"Loading..." progress in the native style; refusals as native messages (different environment,
card full, damaged slot).

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| PSPStates / Custom Home states | [PspStates-Experiment](https://github.com/PSP-Archive/PspStates-Experiment) | cross-boot loads work in practice; `.bin` + `.bmp` per slot under `seplugins/` | idea that it is feasible; avoid non-native storage | none — never copy |
| PSPFatSave | [PSPFatSave-Public](https://github.com/Grunwalskii/PSPFatSave-Public) | "the plugin loads at a different kernel address each boot" | store save-time addresses | MIT |
| PPSSPP SavedataParam | [SavedataParam.cpp](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/SavedataParam.cpp) | SFO hash algorithm (mode 0x01) | already implemented | GPL-2.0+ |

## F. Implementation plan

1. **T1 build** (debug): save → write a probe file → load → check visibility and contents after the
   rollback → write a second file → verify both; owner runs a PC FAT check. Decides `STS-REQ-008`
   and the write-gate policy.
2. E3 core: slot files, preallocation, header/CRC, fingerprint, save-time addresses; tests T2-T6.
3. M5: SAVEDATA folder, SFO + PNGs, slot list, delete handling.

Open: whether the FAT driver revalidates the card on resume (T1); what exactly changes between
boots in the kernel image (plugin load addresses: handled by stored addresses; anything else is
RESEARCH REQUIRED, T3).

## G. Testing and acceptance

Tests T0-T7 from [m3-menu-e3](../research/m3-menu-e3.md#phase-2--e3-states-on-the-memory-stick-loads-after-a-reboot): T0 card image + PC disk
check, T1 freshness, T2 same-session load from the card, T3 quit to the XMB and back, T4 other
writes in between + FAT check and hashes, T5 fingerprint refusals, T6 full power-off, T7 (E4) 100
cycles. Acceptance: all pass, PC FAT check clean after T4 and T7.

## H. Risks and decisions

- [RISK-001](../risk-register.md#risk-001--memory-stick-fat-corruption-after-a-rollback), [RISK-006](../risk-register.md#risk-006--cross-boot-load-into-a-different-environment), [RISK-011](../risk-register.md#risk-011--power-loss-or-card-removal-during-a-state-write).
- [ADR-010](../architecture/decisions.md#adr-010--states-are-native-hashed-savedata-folders).
