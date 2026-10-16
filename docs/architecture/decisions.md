# Architecture decisions

Accepted decisions are never edited except for their status line; a change of mind is a new ADR
that supersedes the old one ([template](../templates/decision-record.md)). Dates are when the
decision was taken; the evidence column links the research behind it.

| ADR | Decision | Status |
|---|---|---|
| [ADR-001](#adr-001--license-gpl-30-or-later) | GPL-3.0-or-later | Accepted 2026-10-04 |
| [ADR-002](#adr-002--target-ark-4-on-661-psp-3000) | Target ARK-4 on 6.61, PSP-3000 | Accepted 2026-10-04 |
| [ADR-003](#adr-003--kernel-libraries-only) | Kernel libraries only | Accepted 2026-10-04 |
| [ADR-004](#adr-004--state-data-lives-in-p9-only) | State data lives in P9 only | Accepted 2026-10-15 |
| [ADR-005](#adr-005--capture-and-roll-back-the-kernel-at-phase0_0-of-a-system-suspend) | Kernel capture/rollback at PHASE0_0 | Accepted 2026-10-04 |
| [ADR-006](#adr-006--rebuild-the-flash-drivers-after-every-resume) | Flash driver rebuild after every resume | Accepted 2026-10-15 |
| [ADR-007](#adr-007--lz4-chunk-codec-with-fill-records) | LZ4 chunk codec with FILL records | Accepted 2026-10-15 |
| [ADR-008](#adr-008--touch-and-go-suspend-by-default) | Touch-and-go suspend by default | Accepted 2026-10-16 |
| [ADR-009](#adr-009--complication-framework-and-ui-layering) | Complication framework and UI layering | Accepted 2026-10-04 (not implemented) |
| [ADR-010](#adr-010--states-are-native-hashed-savedata-folders) | States are native, hashed SAVEDATA folders | Accepted 2026-10-04 |
| [ADR-011](#adr-011--own-overlay-framebuffer-never-the-games) | Own overlay framebuffer | Accepted 2026-10-16 |
| [ADR-012](#adr-012--home-opens-the-hub-and-replaces-the-system-home-screen) | HOME opens the hub | Accepted 2026-10-04, reaffirmed 2026-10-16 |
| [ADR-013](#adr-013--sony-assets-are-read-at-runtime-never-redistributed) | Sony assets read at runtime only | Accepted 2026-10-04 |
| [ADR-014](#adr-014--runlevel-umd-retail-games-only) | Runlevel `UMD` | Accepted 2026-10-04 |
| [ADR-015](#adr-015--independent-firmware-is-a-gated-research-track) | Independent firmware is a gated research track | Proposed 2026-10-16 |
| [ADR-016](#adr-016--hub-concept-a-by-default) | Hub concept A by default | Accepted 2026-10-16 (owner may override) |

## ADR-001 — License GPL-3.0-or-later

- **Status:** Accepted (2026-10-04)
- **Context:** The plan reuses ARK-4 hooks (GPL-3.0) and possibly PPSSPP code (GPL-2.0-or-later);
  MIT/BSD/PD code (PSPFatSave, LZ4, stb_sprintf) is compatible.
- **Decision:** The project is GPL-3.0-or-later; every new file starts with the SPDX line;
  third-party files keep theirs.
- **Alternatives considered:** MIT (the first license) — would forbid porting GPL code.
- **Consequences:** PSPStates (no license) and intraFont (CC BY-SA) may be studied, never copied.
- **Evidence:** commit `44f3c28`.

## ADR-002 — Target ARK-4 on 6.61, PSP-3000

- **Status:** Accepted (2026-10-04)
- **Context:** The owner's device is a PSP-3000 on ARK-4 6.61. The engine depends on exact
  firmware behaviour (suspend phases, memory map).
- **Decision:** Only this combination is supported and tested until v0.1.0.
- **Alternatives considered:** all 64 MB models at once — no test hardware.
- **Consequences:** NIDs are checked against ARK-4's 6.60/6.61 table; other models are untested
  (the README says so).
- **Evidence:** [M1](../research/m1-hardware.md) (`sceKernelGetModel() = 3`).

## ADR-003 — Kernel libraries only

- **Status:** Accepted (2026-10-04)
- **Context:** A GAME plugin shares ~300 KB of kernel RAM; newlib is large and user-mode.
- **Decision:** `USE_KERNEL_LIBS = 1`, libc from SysclibForKernel, formatting with the vendored
  `stb_sprintf` (NOFLOAT); every import must resolve through ARK-4 (`make check`).
- **Consequences:** No floating point formatting, no malloc; big buffers come from P9/P11.
- **Evidence:** [M1](../research/m1-hardware.md) (P1 free 319 KB in a game).

## ADR-004 — State data lives in P9 only

- **Status:** Accepted (2026-10-15)
- **Context:** E2 runs 1-4 failed because the apply blob and the header lived in P11, which the
  firmware overwrites with the Media Engine's eDRAM at suspend event `0x4005`; P5 is the GE's
  eDRAM save area at PHASE0_3.
- **Decision:** Everything that must survive a suspend lives in one P9 block (`0x8A000000`).
  P11 is scratch only (the menu's screen), released before every suspend.
- **Alternatives considered:** P11 (overwritten), game RAM like PSPFatSave on the PSP-1000 (it is
  being saved), kernel RAM (no room).
- **Evidence:** [E2 run 4 analysis](../research/m2-engine.md), uOFW 6.60 `me_wrapper`.

## ADR-005 — Capture and roll back the kernel at PHASE0_0 of a system suspend

- **Status:** Accepted (2026-10-04)
- **Context:** A load must replace the whole kernel state consistently. During a system suspend
  every driver has saved its hardware state and the power code will rebuild the hardware on
  resume.
- **Decision:** Save copies the 4 MB kernel image (+ P8 + ME save) at PHASE0_0; load jumps at the
  load's PHASE0_0 to a blob that copies the image back and returns into the save-time dispatcher
  (`handoff.S`). Interrupts are masked with CP0 Status.IE only.
- **Alternatives considered:** restore at `_scePowerResumePoint` (PSPStates/Adrenaline) — kept as
  plan B, not needed; `sceKernelCpuSuspendIntr` — clears the IC register, which the blob does not
  restore (E2 run 4).
- **Evidence:** [E1](../research/m2-engine.md#e1--capture-during-a-real-suspend), E2 runs 5-7.

## ADR-006 — Rebuild the flash drivers after every resume

- **Status:** Accepted (2026-10-15)
- **Context:** After a rollback the lflash logical-to-physical map in kernel RAM no longer matched
  the NAND; the next flash1 write corrupted it and the PSP reset its settings (E2 run 5).
- **Decision:** At RESUME_COMPLETED call lflash and flashfat `df_exit` + `df_init` in place; after
  a rollback also unassign/reassign flash0:/flash1:. A load refuses to run if the drivers cannot be
  found. (A touch-and-go save never lost power and leaves them alone.)
- **Alternatives considered:** skip the rebuild (corrupts flash1), `sceIoDelDrv` (kills open
  handles).
- **Evidence:** E2 runs 5-7, PSPFatSave v512-v520 history (MIT).

## ADR-007 — LZ4 chunk codec with FILL records

- **Status:** Accepted (2026-10-15), supersedes FastLZ
- **Decision:** 64 KB chunks; a chunk of one repeated word is a FILL record, others LZ4 (lib/lz4
  1.10.0 at -O2, 8 KB hash table) or raw when LZ4 does not shrink them.
- **Alternatives considered:** FastLZ (2.7x slower packing), Media Engine offload (rejected),
  hand-written decoder (backlog S7).
- **Evidence:** [m2-speed](../research/m2-speed.md), on-device benchmark.

## ADR-008 — Touch-and-go suspend by default

- **Status:** Accepted (2026-10-16)
- **Context:** A real sleep costs ~3.6 s per save/load. `scePowerRequestSuspendTouchAndGo` runs
  every suspend/resume event without sleeping, but zeroes the resume info and the IPL seeds at
  `0xBFC00200`; without the seeds, quitting the game hangs on a black screen.
- **Decision:** Fast mode on by default: the first save of a boot sleeps for real (to learn real
  resume info), later ones are touch-and-go; the seeds are saved before every suspend and put back
  at `0x10000`; resume info is filled from cached syscon getters (no syscon commands before a
  suspend).
- **Alternatives considered:** always sleep (slow); dropping the first real sleep (backlog S4,
  needs a test).
- **Evidence:** [m2-speed](../research/m2-speed.md) runs 3-7, commit `7a676bb`.

## ADR-009 — Complication framework and UI layering

- **Status:** Accepted (2026-10-04), **not implemented** (the phase-1 menu hard-codes its items)
- **Decision:** Each complication is a folder exposing
  `pw_complication { id, title, icon, init, build_items, on_menu_open, on_menu_close, shutdown }`,
  registered in one table; the hub builds its items from the registry. UI layering: theme
  (all native metrics in one table) → widgets → hub/screens; rendering in `gfx/` (framebuffer,
  draw, PGF text, images, animation).
- **Consequences:** New features never edit the engine or the menu loop.
- **Evidence:** owner priority 3 ([overview](../00-project-overview.md)); spec
  [complications](../subsystems/complications.md).

## ADR-010 — States are native, hashed SAVEDATA folders

- **Status:** Accepted (2026-10-04)
- **Decision:** Each state is `ms0:/PSP/SAVEDATA/PW<GAMEID><NN>/` with a chnnlsv-hashed (mode 0x01)
  PARAM.SFO, ICON0.PNG, PIC1.PNG (the screenshot) and the state data; the XMB lists and deletes
  them natively.
- **Alternatives considered:** plain files under `seplugins/` (PSPStates' way) — not native;
  hash-less SFO — the XMB shows "Corrupted Data" (S2).
- **Evidence:** [M1 S2/S2b](../research/m1-hardware.md).

## ADR-011 — Own overlay framebuffer, never the game's

- **Status:** Accepted (2026-10-16)
- **Decision:** The menu draws into its own 8888 pages (stride 512) in P11 and shows them with
  `sceDisplaySetFrameBufferInternal(0, ...)`; the game's framebuffer is only read (dimmed
  background). The pages are released before a suspend and redrawn after.
- **Alternatives considered:** drawing into the game's buffer (PSPFatSave) — a state saved with the
  menu open would contain the menu, four pixel formats, double buffering.
- **Evidence:** [m3-menu-e3](../research/m3-menu-e3.md), device test 2026-10-16 (P11 scan-out works).

## ADR-012 — HOME opens the hub and replaces the system HOME screen

- **Status:** Accepted (2026-10-04, plan), reaffirmed by the owner 2026-10-16
- **Context:** Phase 1 used a tap of NOTE (♪) as a temporary trigger because replacing the system
  HOME screen needs our own Quit Game. The owner expects HOME (PS) and an XMB look.
- **Decision:** HOME opens Pocketwatch's hub; the system HOME popup is disabled while the plugin
  runs (respecting a game that disabled HOME itself); the hub offers Quit Game.
- **Consequences:** M3 shell work moves before E3 (roadmap); implementation details are
  **RESEARCH REQUIRED** ([in-game shell spec](../subsystems/game-shell.md)).

## ADR-013 — Sony assets are read at runtime, never redistributed

- **Status:** Accepted (2026-10-04)
- **Decision:** Fonts (`flash0:/font/*.pgf`), icons (`flash0:/vsh/resource/*.rco`), sounds are read
  from the user's PSP when needed; none is committed or shipped. Reference screenshots of unclear
  license are linked, not stored.

## ADR-014 — Runlevel UMD (retail games only)

- **Status:** Accepted (2026-10-04)
- **Context:** ARK-4's `GAME` runlevel also loads plugins into homebrew; the M1 homebrew run used a
  highmem layout the engine does not support.
- **Decision:** `PLUGINS.TXT` line `UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on`.
- **Evidence:** [M1 run 1](../research/m1-hardware.md), `scripts/deploy.ps1`.

## ADR-015 — Independent firmware is a gated research track

- **Status:** Proposed (2026-10-16)
- **Context:** The owner asked whether an independent firmware/software stack for PSP hardware is
  feasible. It is a different level of ambition from a plugin, with brick risk.
- **Decision (proposed):** Treat it as research with go/no-go gates `FW-G1`..`FW-G7`; nothing that
  writes the NAND, IPL or SYSCON without an owner decision and a tested recovery path; the plugin
  roadmap does not wait for it.
- **Evidence:** [feasibility study](../firmware/independent-firmware-feasibility.md).

## ADR-016 — Hub concept A by default

- **Status:** Accepted (2026-10-16) as the default; the owner was asked to choose and asked to go on
  building ("давай далі працювати над меню, інтерфейсом") without picking, so the recommended
  concept was built. A later owner choice supersedes this ADR.
- **Context:** Two PROPOSED layouts ([concepts](../assets/concepts/README.md)): A, the XMB's item
  column alone; B, a full cross with complications as categories.
- **Decision:** A for v0.1: every action is one Up/Down away, and there is a single complication.
  No "Pocketwatch" branding in the hub (the category icon is a watch).
- **Alternatives considered:** B — worth it with several complications (ADR-009 registry).
- **Consequences:** `ui/views.c` draws A; B would reuse the same rows with a category bar.
