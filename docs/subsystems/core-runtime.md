# Core runtime — specification

| | |
|---|---|
| Area code | `SYS` |
| Status | Done: lifecycle, log, platform shim, memory blocks, freeze, flash rebuild, codec, CRC, PNG, import self-check. Planned: config store, game identity, Memory Stick write gate, power locks |
| Milestones | M0, M2; config/identity/gate with M3-E3 |
| Code | [`plugin/src/core/`](../../plugin/src/core/) |
| Research | [m1-hardware](../research/m1-hardware.md), [m2-engine](../research/m2-engine.md) |

## A. Overview

- **Purpose:** the services every complication needs and none should reimplement: module
  lifecycle, logging that is safe in suspend paths, OS access behind a host-testable shim, big
  memory blocks outside kernel RAM, freezing the game, repairing the flash drivers after a resume,
  compression, checksums, the boot-time import check.
- **Non-goals:** anything complication-specific; UI.
- **Current state:** all listed services exist and are used by the engine and the menu
  (VERIFIED by the device runs of M1/M2/M3 phase 1).

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `SYS-REQ-001` | `module_start` returns immediately; all work runs in plugin threads. | P0 | Done | code ([main.c](../../plugin/src/core/main.c)) |
| `SYS-REQ-002` | `module_stop` stops and joins every plugin thread within 500 ms (terminate after), unregisters handlers, frees memory, does no file I/O. | P0 | Done | M1 incident fix, quit tests |
| `SYS-REQ-003` | The log buffers lines in a RAM ring; deferred mode keeps them in RAM (sysevent paths, while a state is held); sessions are appended after `==== new session ====`; the file restarts past 256 KB. | P0 | Done | host tests (`test_log`, `test_ring`) |
| `SYS-REQ-004` | At boot every import stub is classified (direct, syscall, unresolved) and unresolved ones are logged. | P0 | Done | `test_stubinfo`, device logs |
| `SYS-REQ-005` | Buffers larger than a few KB come from P9 (or P11 as scratch) via `pw_mem_*`, never kernel RAM. | P0 | Done | code review, `make check` memory budget |
| `SYS-REQ-006` | `pw_freeze_game` suspends every user thread (attr `0x80000000`), preferring the WAITING state, forcing the rest after the timeout; `pw_thaw_game` resumes exactly those, in order. | P0 | Done | E2 logs (threads frozen/forced) |
| `SYS-REQ-007` | `pw_flash_reinit` rebuilds lflash + flashfat in place (and remounts flash0/flash1 after a rollback) from RESUME_COMPLETED without logging. | P0 | Done | E2 runs 6-7 |
| `SYS-REQ-008` | LZ4 block codec with caller-provided state, decoding bounds-checked (`< 0` on corrupt input). | P0 | Done | `test_lz` |
| `SYS-REQ-009` | Portable modules include no PSP header and are host-tested under ASan/UBSan. | P0 | Done | `make -C tests` |
| `SYS-REQ-010` | A config store: per-complication key/value settings persisted on the card, read at start, written only when no state is held. | P1 | Planned | host tests + device |
| `SYS-REQ-011` | Game identity: game ID (`disc0:/UMD_DATA.BIN`), title (PARAM.SFO), ISO path from ARK. | P1 | Planned | device log |
| `SYS-REQ-012` | Memory Stick write gate: every card writer asks `pw_ms_write_allowed()`; denied while a state is held unless the [storage](states-storage.md) policy allows it, or after a failed freshness probe. | P0 | Partial: the log can be blocked until reboot (`pw_log_set_card_blocked`, used after a failed T1); a general gate is Planned | code review + T1 |
| `SYS-REQ-013` | Power lock helper for operations that must not be interrupted by a user suspend (state write). | P1 | Planned | device test |
| `SYS-NFR-001` | Kernel RAM: module loaded sections ≤ 128 KB; PRX ≤ 112 KB (debug). | P0 | Done (2026-10-16: debug PRX 99,998 B, loaded 102,516 B; release PRX 90,642 B, loaded 95,013 B) | `make check` |

## C. Architecture and integration

| Module | Interface | Notes |
|---|---|---|
| `main.c` | `module_start/stop` | starts `PocketwatchMain`: log init, self-check, engine init, debug probe, menu thread |
| `log.c` + `ring.c` | [`log.h`](../../plugin/src/core/log.h) | file next to the PRX; `pw_log_set_deferred` / `pw_log_flush` |
| `platform_psp.c` | [`platform.h`](../../plugin/src/core/platform.h) | IRQ lock, file write/size; host version in `tests/platform_host.c` |
| `mem.c` | [`mem.h`](../../plugin/src/core/mem.h) | P9 first, P11 fallback; 64-byte aligned |
| `freeze.c` | [`freeze.h`](../../plugin/src/core/freeze.h) | up to 64 threads |
| `flash.c` | [`flash.h`](../../plugin/src/core/flash.h) | drivers found through ARK; report struct logged later |
| `lz.c` | [`lz.h`](../../plugin/src/core/lz.h) | LZ4 1.10.0 at -O2, 8 KB table |
| `crc32.c`, `png.c` | headers in `core/` | PNG: stored-deflate RGB encoder for ICON0/PIC1 (M5) |
| `selfcheck.c`, `stubinfo.c` | [`selfcheck.h`](../../plugin/src/core/selfcheck.h) | |
| `stubs/*.S` | ARK SystemCtrl, scePower_driver, sceSyscon_driver imports | every NID passes `make check` |

## D. User experience and visual design

Not applicable.

## E. Reference material

PSPFatSave (MIT) for the flash rebuild; uOFW for thread and sysmem behaviour; ARK-4 for the NID
resolver. See the [states engine spec](states-engine.md#e-reference-material).

## F. Implementation plan

1. `SYS-REQ-012` write gate before E3 (system map finding A2).
2. Config store (`core/config.c`, portable parser + PSP I/O) with the Settings complication.
3. Game identity (`core/game.c`) for E3 fingerprints and M5 folder names.
4. Power lock helper with E3's card writes.

## G. Testing and acceptance

Host tests cover log, ring, stubinfo, lz, crc32, png, sfo, stream, ui (53 tests, 2026-10-16). Device
coverage comes from the engine and menu protocols. New services ship with host tests for their
portable part.

## H. Risks and decisions

- [RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth) (size), [RISK-008](../risk-register.md#risk-008--ark-4-api-or-nid-table-changes) (ARK API).
- [ADR-003](../architecture/decisions.md#adr-003--kernel-libraries-only) (kernel libs only), [ADR-006](../architecture/decisions.md#adr-006--rebuild-the-flash-drivers-after-every-resume) (flash rebuild).
