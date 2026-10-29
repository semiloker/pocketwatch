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

- **Status:** Accepted (2026-10-04), implemented 2026-10-17 (`complications/`, the hub iterates the table)
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

## ADR-017 — State files are written after the capture

- **Status:** Accepted (2026-10-16), after test T1.
- **Context:** The E3 design (STS-REQ-002/004) preallocated the slot files before the kernel
  capture so that a restored kernel's FAT view would already contain them. T1 (2026-10-16,
  VERIFIED on the PSP-3000, real-sleep save and load) showed that after a rollback the FAT driver
  sees a file written after the capture, with the right contents, and that a new allocation in the
  restored world does not collide with it ([m3-menu-e3](../research/m3-menu-e3.md#t1-device-protocol)).
- **Decision:** the engine saves into RAM as before; the hub then writes the whole slot (DATA.BIN,
  PNGs, PARAM.SFO, a marker with a new name per save, STATE.BIN last). After every load from a
  slot, the restored world checks that it sees that save's marker before anything writes to the
  card; on failure card writes stop until reboot and the user is told not to save in the game.
- **Alternatives considered:** preallocation (needs the state's size before the capture and two
  passes over the card); a temp folder + rename (keeps the old slot until the new one commits, but
  FAT directory renames are untested here and double the space during the write).
- **Consequences:** simpler and faster saves; an interrupted save loses the slot's previous state
  (the overwrite confirmation says so, RISK-011). Touch-and-go saves (no power cycle) are covered
  by T1 only through the marker check until test T1b runs ([testing guide](../testing.md#t1b)).

## ADR-018 — Translations are files; the texts taken live in kernel RAM

- **Status:** Accepted (2026-10-17).
- **Context:** UI-REQ-008 asks for the system language. Tables for eight languages in the PRX would
  add ~16 KB to a kernel module already near its budget (RISK-005). A translation kept in P9 would
  be pointed at by every state's kernel image, so a state could not load after a language change.
- **Decision:** English is built in; `plugin/lang/<code>.txt` (UTF-8, "key = text") is installed
  next to the PRX and read once at plugin start for the system language (or the code in
  `language.txt`, which allows languages the PSP lacks, e.g. Ukrainian). The texts taken are copied
  into a 4 KB pool in kernel RAM: a state restores its own language. A line whose printf conversions
  differ from the English string's is ignored (it would crash the formatter); a file whose letters
  the system font lacks is refused as a whole (English instead).
- **Alternatives considered:** built-in tables (size); P9 storage (states tied to the language);
  Sony's own strings from the RCO resources (they do not contain our texts).
- **Consequences:** translations can be fixed without a rebuild; `tests/test_i18n.c` checks every
  shipped file is complete and format-safe. Japanese, Korean and Chinese fall back to English until
  their system fonts are loaded.

## ADR-019 — Optional UI styles next to the native look

- **Status:** Accepted (2026-10-17), on the owner's request ("make more variants of UI like ps5
  or modern ui, minimalistic").
- **Context:** owner priority 1 is the native look (ADR-012, M4), and it stays the reference. The
  owner also wants modern, minimal variants to choose from.
- **Decision:** Classic (XMB) remains the default; Modern (a console control-centre style over the
  blurred game) and Minimal (text only over the darkened game) are selectable in Settings → Style
  and saved in `config.txt`. Each screen function dispatches to the style's file with the same
  parameters, so the hub and the complications do not change ([styles](../ui/styles.md)). The
  new styles use no Sony pictures beyond the font and the button symbols, and are not named after
  any product.
- **Alternatives considered:** a theme file of colours and sizes (too weak: the styles differ in
  layout, not only in colours); separate builds per style (the owner could not switch on the PSP).
- **Consequences:** +12 KB of code; kernel RAM did not grow (14 KB of buffers moved to P9 at the
  same time); the PRX file budget was raised to 192 KB (RISK-005). Every new screen needs three
  drawings, or falls back to the XMB one.

## ADR-020 — Continue from a state through the game's options menu

- **Status:** Accepted (2026-10-17), the owner's choice among the three variants of
  [vsh-integration](../subsystems/vsh-integration.md#d-variants-owner-chose-b-2026-10-17).
- **Context:** M7 lets the user start a game straight in a state. States are already native saved
  data (M5); continuing needs a way to pick the state in the XMB and a way to load it once the
  game runs.
- **Decision:** variant B: the game's own options menu (△ on its icon) gets "Continue from Save
  State" and then a slot list; the VSH side writes `continue.txt` and launches the game the way
  ARK-4's vshctrl does; the GAME side loads the slot once the game runs through a general
  complication hook (`startup_pending` / `startup` in `pw_complication`), so other complications
  can use the same moment later.
- **Alternatives considered:** A, a "Save States" item in the Game category (the most screen work
  and its own renderer in the VSH); C, a prompt at every game start (no XMB code, but not in the
  XMB and it would ask on every start).
- **Consequences:** the GAME side works without the XMB part (a hand-written `continue.txt`,
  test V1). The XMB part must chain after ARK-4's xmbctrl, which already patches the same
  `vsh_module` call sites, and a VSH crash needs ARK's recovery menu to undo (RISK register at
  implementation). The complication initialisers now use designated fields.

## ADR-021 — The XMB entry is a user-mode VSH plugin chained after ARK-4's xmbctrl

- **Status:** Accepted (2026-10-17), on the owner's request ("add a button in the XMB, in the
  Settings section or elsewhere, to configure the plugin").
- **Context:** the plugin's settings could only be changed in a game (hub → Settings). The XMB's
  native way to show settings is a Settings page; ARK-4's xmbctrl already adds "Custom Firmware
  Settings" by patching `vsh_module` and `sysconf_plugin` (GPL-3.0-or-later, so its mechanism and
  patterns can be reused with attribution). Both are user modules, so the hooks must live in user
  memory: a second PRX, separate from the kernel game plugin.
- **Decision:** `vsh/` builds `pocketwatch_xmb.prx` (VSH runlevel, user mode, no newlib, ~32 KB)
  that adds "Pocketwatch" after System Settings and fills System Settings' page with our four rows
  (the XMB's own option list), saving to `config.txt` / `language.txt` with the game side's own
  `core/config.c` and `ui/i18n.c` compiled again. Every patched site is located before xmbctrl's
  handler and wrapped after it (vsh-integration §C2), so xmbctrl keeps working and is not
  required. Any missing or inconsistent site leaves that module unpatched.
- **Alternatives considered:** a homebrew app in Game → Memory Stick (not in Settings, our own
  renderer, a reboot to open it); an item in xmbctrl's own CFW Settings list (xmbctrl reads a
  fixed table, no way in); patching before xmbctrl (its handler would overwrite our call sites
  with calls of the firmware's functions and bypass ours).
- **Consequences:** a second file to deploy and a `VSH` line in PLUGINS.TXT (`scripts\deploy.cmd
  -EnableXmb`); a crash in the XMB would need the START-at-boot recovery; the same chained hooks
  are the base for the game's △ menu entry (ADR-020). Device behaviour is unverified until X1.

## ADR-022 — Battery readings from the battery's own gauge, with labelled estimates

- **Status:** Accepted (2026-10-17), branch `feature/psp-xmb-overhaul`, on the owner's brief
  ("battery intelligence ... do not invent a raw sensor reading").
- **Context:** the system shows a percentage and an estimate whose basis is unknown to the user.
  The PSP's battery pack has its own gauge that syscon reads (voltage, current, temperature,
  remaining and full capacity, cycle count: uOFW `include/syscon.h`); the kernel plugin can call
  it.
- **Decision:** the Battery page shows the system's figures and the gauge's readings side by side
  under their own headings; values out of range are "Not available"; the time left is our estimate
  (remaining mAh / average current of the run, or the percentage slope) marked "about", the
  firmware's figure marked "(system)" until ours exists, "Estimating..." before either. The
  history (64 readings, 15 s apart) lives in P9.
- **Alternatives considered:** a voltage-to-percentage curve (needs per-pack calibration the
  project does not have; the gauge already does it); the firmware's estimate alone (no basis
  shown); reading every second (needless syscon traffic).
- **Consequences:** units are INFERRED until device test B1; health in percent waits for the design
  capacity (BAT-REQ-007).

## ADR-023 — Play time from the shell's tick, kept in P9, written to two alternating files

- **Status:** Accepted (2026-10-17), branch `feature/psp-xmb-overhaul`.
- **Context:** play time must exclude the hub, saves, loads and sleeps, must survive a state load
  (which rolls kernel RAM back to the save), reboots and a write cut short, and must not invent
  history.
- **Decision:** the shell's once-a-second tick (only while the hub is closed and the engine idle)
  adds steps of at most 2.5 s to counters in a P9 block (not part of a state; its address is in the
  fingerprint). The game's record at the boot's start plus this session is written every 5 minutes
  of play, after each hub visit (at the next tick, never while the hub opens) and at Quit Game, to `playtime-a.txt` / `playtime-b.txt` in turn
  (sequence number and CRC; the newer complete one wins). The table of all games exists only in
  P11 while written or shown.
- **Alternatives considered:** wall-clock differences (the owner's clock is set to 2011 and can
  change; sleeps would count); counters in kernel RAM (a load would undo the time played since the
  save); one file rewritten in place (a power loss mid-write would lose every game's history).
- **Consequences:** up to 5 minutes are lost when the game ends without Quit Game (PLT-REQ-007);
  homebrew and PS1 games are not counted (the plugin does not load there).

## ADR-024 — Battery, Play Time and Settings live in the XMB, not in the game's hub

- **Status:** Accepted (2026-10-17), the owner: "battery, play time, settings should be in the
  XMB's main menu, not on the PS button". Supersedes the in-game placement of ADR-022/023's pages
  and of CPL-REQ-006's screen.
- **Context:** the XMB plugin (ADR-021) can add items to the XMB's columns and fill System
  Settings' page with rows; it works on the owner's PSP (xmb.log 2026-10-17: vsh_module and
  sysconf_plugin patched, item added). The first device reading of the battery also showed that
  the syscon RCap is not mAh (74 at 83 % of 1790 mAh): the capacities come from scePower.
- **Decision:** XMB → Settings → "Pocketwatch" (settings) and "Battery" after System Settings;
  XMB → Game → "Play Time" after Saved Data Utility. Each opens System Settings' page with our rows:
  settings as option lists; readings as rows with one value each (the XMB's own list shows the
  value). In the XMB the battery is read from scePower (user mode) and the battery's current and
  cycle count through ARK-4's kuKernelCall; a thread takes a reading every 15 s from the XMB's
  start, so the page can estimate. The game keeps counting play time (P9 counters, the files) and
  reading config.txt; its hub has Resume Game, Save State, Load State and Quit Game only. Strings
  only the XMB shows are left out of the game build (`PW_I18N_XMB`).
- **Alternatives considered:** keeping the pages in the hub as well (two places to maintain, and
  ~16 KB of kernel RAM); drawing our own screens over the XMB (RESEARCH REQUIRED, the XMB's
  native lists already fit read-only rows).
- **Consequences:** settings take effect at the next game start; the Battery page shows readings
  as of opening it (open it again to refresh); the in-game info pages and the hub's Settings screen
  are gone (UI-REQ-017 withdrawn); the game build is back under its old budgets (175 KB loaded).

## ADR-025 — A full UI of our own is Pocketwatch Home, a homebrew home screen

- **Status:** Accepted (2026-10-18), on the owner's request ("study cxmb, how to make our own full
  UI, write a first modern one").
- **Context:** cxmb (GPL-3.0) only redirects the XMB's flash0 files to a theme bundle: a theme
  changes the XMB's pictures, colours and fonts, not its screens; it is built from Sony's RCOs.
  The XMB's own engine cannot be replaced by a plugin. ARK-4 can start a homebrew menu instead of
  the XMB (launcher mode), and its own menu shows how a user-mode program starts an ISO
  ([m7-full-ui](../research/m7-full-ui.md)).
- **Decision:** the full UI is **Pocketwatch Home** (`home/`): a homebrew application built from
  Pocketwatch's portable renderer, fonts from flash0, its own ICON0/PIC1; first in the Modern style
  (games with their own art, play time, save states, battery, card), starting games the way
  ARK-4's menu does. It runs from Game → Memory Stick; launcher mode later (HOM-REQ-007).
- **Alternatives considered:** a CTF theme (changes the look of the real XMB but adds nothing,
  and needs Sony's files as its base); panels drawn over the XMB by the VSH plugin (the XMB stays
  the frame; RESEARCH REQUIRED); patching more of `vsh_module` (fragile, per-firmware).
- **Consequences:** a third binary (EBOOT.PBP) to build, test and ship; a state saved after a
  start from the XMB may be refused after a start from Home (another boot configuration,
  RISK-020); the XMB keeps its own media and network functions.

## ADR-026 — The PSP can start in Pocketwatch Home, chosen by the XMB plugin through ARK-4's launcher field

- **Status:** Accepted (2026-10-18), on the owner's request ("not an app I have to start: I boot
  the PSP and it is already there"). Extends ADR-025. When the field is written and how the
  xmb-once marker is read: superseded by ADR-027.
- **Context:** ARK-4 starts `<ARK folder><ARKConfig.launcher>` instead of the XMB when that field
  is set as vsh_module starts; VSH plugins run before that moment, after ARK-4 has set or cleared
  the field ([m7-boot-shell](../research/m7-boot-shell.md), with file and line references).
- **Decision:** a setting, XMB → Settings → Pocketwatch → **Start screen** (`config.txt`
  `start = xmb | home`, default XMB). With Home chosen, the XMB plugin's `module_start` writes
  `launcher = "PWHOME.PBP"` into ARK-4's config (`sctrlArkGetConfig`, then `sctrlArkSetConfig`
  through `kuKernelCall`, as ARK-4's xmbctrl does) - only after checking the config's layout and
  that no launcher or recovery is set, only when `PWHOME.PBP` is in ARK-4's folder, and never when
  L is held, Home asked for the XMB (`xmb-once`) or Home's last start did not reach its first
  screen (`boot-pending`). Home lists and starts PSP/GAME homebrew and PS1 games too.
- **Alternatives considered:** ARK-4's own `launcher, on` with Home as `VBOOT.PBP` (replaces
  ARK-4's menu, lost on every ARK-4 update, no way to the XMB but editing `SETTINGS.TXT`, no
  guard); starting Home from our own vsh_module hook (our reboot code at the most delicate point);
  drawing over a running XMB (RESEARCH REQUIRED, memory). Rejected for these reasons.
- **Consequences:** the XMB's own functions (System Settings, media, network, USB, UMD) are one
  "Exit to the XMB" away; the plugin depends on ARK-4's `ARKConfig` layout (RISK-021); a third
  place Home is installed (`ARK_01234/PWHOME.PBP`, by the deploy script).

## ADR-027 — The launcher field is written after ARK-4's processSettings; the XMB-once marker carries its time

- **Status:** Accepted (2026-10-18), after device run 1 of the start screen
  ([m7-boot-shell](../research/m7-boot-shell.md#results)). Supersedes ADR-026 in these two points.
- **Context:** ARK-4 starts the VSH plugins just before sceMediaSync; sceMediaSync's own start then
  runs `processSettings`, which clears `ARKConfig.launcher` when ARK-4's launcher setting is off. A
  write from `module_start` is therefore undone. The xmb-once marker of the first build had no
  time, so a marker left from an earlier visit to Home sent a later power-on to the XMB (run 1).
- **Decision:** `module_start` only decides (file I/O, `boot.txt`); the write happens in the
  start-module handler for `sceMediaSync`, after the older handlers (ARK-4's among them) returned,
  with the config read again and checked again (no launcher, no recovery). Home writes the RTC
  tick into `xmb-once`; the plugin removes the marker at every start and obeys it only if it is at
  most 120 s old. `boot.txt` keeps the last decisions with the time; an XMB that starts although
  Home was chosen is logged and does not count against Home (`boot-pending` removed).
- **Alternatives considered:** writing from vsh_module's own start handler (too late:
  `InitKernelStartModule` decides before vsh_module's prologue); writing again at every module
  start until vsh_module (would also undo vshctrl's own clearing for a mounted UMD ISO);
  distinguishing a cold boot from a reboot instead of a time (no documented way from user mode).
- **Consequences:** the XMB plugin has 147 bytes left of its 64 KB budget; the handler does no file
  I/O (the rule of `vsh/src/main.c`), so what it did reaches the card only if the XMB starts.

## ADR-028 — Pocketwatch Home draws with the GE; HOME opens Home's own panel

- **Status:** Accepted (2026-10-18), after the owner found Home slow ("the menu lags a lot, make it
  60 fps") and HOME leading to the XMB (run 2, [m7-boot-shell](../research/m7-boot-shell.md)).
- **Context:** Home drew every frame with the CPU renderer the hub uses: the full-screen
  background (a copy or a cross-fade), scaled icons, alpha shapes and text, into RAM, then
  showed it. The hub's own frames cost 17-19 ms of drawing on the PSP (Modern, 2026-10-17,
  [styles](../ui/styles.md)) - more than one 16.7 ms vblank - and Home's screen is larger work.
  Unlike the in-game hub, Home owns the whole PSP, the GE included. In Home, HOME brought the
  system's quit dialog, then (xmb-once) the XMB.
- **Decision:** Home's screens draw through `hm_gfx` (`home/src/gfx.h`), a handful of operations
  with two backends: on the PSP the GE (`gfx_gu.c`: two 8888 frames in VRAM, a display list a
  frame, every image a texture, alpha blending and bilinear scaling in hardware); on the host
  the CPU renderer (`gfx_cpu.c`, the previews). Text lines, rounded corners and drawn icons are
  rasterised once by the CPU renderer into small alpha textures ("stamps", kept by what they
  show, 96 of them), so they look the same as in the previews; a rounded rectangle is nine
  sprites cut from its stamp (`sprites.c`, host-tested against the CPU renderer). The clock,
  battery and card are read once a second, the free space every 10 s. Home writes its frame
  rate to `seplugins/pocketwatch/home.log` every 30 s and before a game. HOME is Home's: the
  system's HOME dialog is turned off (`sceImposeSetHomePopup(0)` through `kuKernelCall`) and
  HOME, read in kernel mode, opens a Pocketwatch panel: Close, Exit to the XMB, Sleep, Turn Off.
  Strings get a third tier (`PW_I18N_HOME`): the XMB plugin no longer carries Home's.
- **Alternatives considered:** a faster CPU path (333 MHz, copying only what changes) - the hub
  shows the ceiling; drawing only changed regions - every animation touches most of the screen;
  the GE for the in-game hub too - the game owns the GE and its state there (not in this ADR).
- **Consequences:** Home's look is defined twice (the GE backend must match the CPU one; the
  sprite test guards the corners); the display list takes 512 KB of Home's RAM; VRAM holds the
  two frames, textures stay in RAM. The in-game hub keeps the CPU renderer.

## ADR-029 — 60 frames a second in every menu: measured on the host, checked in PPSSPP

- **Status:** Accepted (2026-10-19), on the owner's request ("stable 60 fps in any mode and menu;
  run the performance tests yourself").
- **Context:** the in-game hub draws with the CPU at the game's clock (333 MHz: ARK-4's
  `overclock` setting, VERIFIED in the engine's timing logs, so a higher clock is no way out). Its
  last device numbers were 17-19 ms of drawing (Modern) and 18-21 ms (Classic) against a 16.7 ms
  vblank (2026-10-17). The host bench (`make -C tests bench`), now with the system font
  (`FONT=`) and the plugin's own `-Os` (`BENCH_OPT=-Os`), split a frame: at `-Os` the per-pixel
  helpers were calls (10-35 % slower than `-O2`); the clock corner cost a fifth of a Modern
  frame although it changes once a minute; the XMB style renewed half of its wave every frame.
  Building the pixel files at `-O2` would have cost 3 KB of kernel RAM over the budget (RISK-005).
- **Decision:**
  - the per-pixel helpers are `PW_PIXEL` (always inlined, `ui/draw.h`) - `-O2`'s speed at
    `-Os`'s size;
  - the hub keeps the status corner as drawn (`hub.c`, in the screen's spare page when the style
    has no moving backdrop) and redraws it only when the clock, battery, fade or backdrop change;
  - the XMB style renews a quarter of its wave a frame (the crest drifts at most 0.12 px a frame,
    so a band lags its neighbour by under half a pixel);
  - the savedata PARAM.SFO writer no longer keeps a 3 KB table of zeros in kernel RAM;
  - Pocketwatch Home's stamps are 8-bit textures with one palette (a quarter of the memory, up to
    256 of them, 3 MB), and the titles and rows the next frames may need are made while the GE
    draws the current frame, within 3 ms (`hm_prefetch`); scaled images are sampled half a texel
    in from their edges;
  - the GE backend is checked in PPSSPP on the PC (Home started from the build, the window
    captured) before a device round.
- **Result (host bench, `-Os`, µs a frame):** Classic 324 → 155, Modern 231 → 169, Minimal
  57 → 48. With the device/host ratio of 2026-10-17 (~80x) every style lands under 16.7 ms
  (INFERRED; device test H13 decides). Kernel RAM: release 174 KB of 176, debug 193 KB of 192+16.
- **Alternatives considered:** the GE for the hub (the game's GE state and VRAM would have to be
  saved around every frame and before every save - RESEARCH REQUIRED, risk to the states); dirty
  rectangles in the CPU renderer (every primitive would need an origin; later if H13 says so);
  rendering the wave at half resolution (visible).

## ADR-030 — Home's backgrounds live in VRAM as dithered 5650; the free space is counted off the frame loop

- **Status:** Accepted (2026-10-19), from the first device numbers of the GE version
  ([log](../research/logs/2026-10-19-home-fps-ge1.log), build `5811f00`): 45-49 fps, CPU 2.7-4.8
  ms + **GE 11-13 ms** a frame, and one frame of **2.7 s**.
- **Context:** VERIFIED from that log: the GE, not the CPU, held the frame. Each frame read the
  full-screen 8888 background from RAM (522 KB, twice during a cross-fade) and the 32-bit text
  stamps from RAM. The 2.7 s frame matches `sceIoDevctl(0x02425818)` (free space: the FAT is
  counted), called from the frame loop when System opened (INFERRED: the only call that long in
  the loop; it went with it).
- **Decision:** a finished background is copied by the loader into one of three VRAM slots as
  5650 with a 4 x 4 ordered dither (two 8888 frames + three slots = 1.86 MB of the 2 MB), and the
  GE draws it from there (`hm_gu_upload_bg`); the RAM picture remains the fallback while a slot
  is rewritten. Text stamps were made 8-bit already (ADR-029). The free space is counted by the
  loader thread when it has nothing else to do, and again after USB; System shows "..." until
  then.
- **Alternatives considered:** 16-bit framebuffers (more VRAM free, but the whole UI banded); a
  block copy of the background (`sceGuCopyImage`) instead of a sprite (still needs it in VRAM;
  the sprite also does the cross-fade); keeping 8888 in VRAM (only one fits beside two frames).
- **Consequences:** a faint dither pattern on dark gradients (seen in PPSSPP at 3x; not at 1x);
  VRAM is full - icons and stamps stay in RAM.

## ADR-031 — Nothing slow in Home's frame loop: the battery is read by a thread, the system's parameters are kept

- **Status:** Accepted (2026-10-19), from the owner's second device round of the GE version
  ([log](../research/logs/2026-10-19-home-hm10-run2.log), build `78e3690`).
- **Context:** VERIFIED from that log: the GE now waits 0.4-8.5 ms a frame (11-13 ms before
  ADR-030), yet most 30 s windows made 30-36 fps and two made 5-7 fps. Their late frames are few
  (30-82 in 30 s) and long: about 250-350 ms each, worked out from the frames, the late count and
  the window, with the CPU part of the frame carrying the time - one stall about every second,
  more while settings were changed. INFERRED: the rows refresh, once a second and after every
  change in Settings or press in Music, called the power driver (battery percent, life time,
  voltage, temperature, two capacities), the syscon (the battery's cycle count, a command to the
  battery itself), the registry (eight `sceUtilityGetSystemParamInt`, the nickname twice) and the
  card (`language.txt`) - nothing else in the frame runs once a second. Which call is slow is not
  known: PPSSPP answers them all at once.
- **Decision:** the frame loop makes no call into the power driver, the syscon, the registry or
  the card. `hw.c` reads the battery in a thread of its own (priority 0x28: below the main
  thread, above the loader) every second, the cycle count once, and the screen copies the last
  reading (two buffers; the main thread is never preempted by the reader). The system's
  parameters are read once and kept (`hm_hw_param`), read again only after Home writes one
  (`hm_hw_set_param` reads it back); Settings keeps the nickname and `language.txt` the same way.
  `home.log` gets the longest rows refresh of each window (`rows ... us`) and once the first
  refresh's and every battery call's cost (`home reads: ...`), so the next round names the slow
  call.
- **Alternatives considered:** reading the battery every few seconds in the loop (the same stall,
  rarer); building only the visible section's rows (System's stall stays); a thread for all the
  rows (the texts and the settings' state shared with the main thread: locks around every
  change).
- **Consequences:** a parameter changed by someone else while Home runs is seen at Home's next
  start - nothing else changes them meanwhile (the XMB is not running). The battery's numbers can
  be a second old. The impose values (brightness, AVLS) are still read every second in the loop
  (the screen key changes the brightness): `rows` shows whether they cost anything.

## ADR-032 — A frozen game must leave the card free: the card answers after every freeze

- **Status:** Accepted (2026-10-19), from the owner's third device round
  ([log](../research/logs/2026-10-19-home-hm10-run3.log), build `c3f3699`).
- **Context:** VERIFIED from that log: in Need for Speed: Most Wanted (ULUS10036) HOME froze the
  game for good - the picture stopped, no button did anything; the session's log ends at
  `shell: HOME opens Pocketwatch` without the `shell: sky ...` line that the first opening of
  every session writes (the next session of the same game opened the hub normally). INFERRED from
  the code: `pw_freeze_game` suspends every game thread that is WAITING, taking WAITING for a safe
  point. A thread inside a plain `sceIoRead` - or Inferno reading the ISO for it
  (ARK-4 `core/inferno/isoread.c`: `sceIoRead` on the ISO in the caller's thread, under its
  `EcsUmd9660DeviceFile` semaphore) - is WAITING for the transfer while it holds the FAT driver;
  suspended there, it never lets go. The shell's next card access - that log line, written at
  once because the log is not deferred - waits for ever, and the shell thread is the one that
  reads HOME. A streaming game like this one reads all the time, so a press often lands in a
  read. RISK-016's log (The 3rd Birthday: "two of the frozen threads were waiting on the game's
  file streaming") may be the same case at the save's sleep (INFERRED, not verified).
- **Decision:** after every freeze the card must answer: a kernel thread of ours stats
  `ms0:/PSP`; when that takes longer than 50 ms, the freeze is undone, the game runs until the
  stat answers (at most 500 ms) and is frozen again. After 2 s of that nothing is frozen
  (`card_busy`): the shell writes `shell: HOME ignored: the game kept the Memory Stick busy` and
  the next press tries again. Every freeze goes through it - the hub, the engine's saves and
  loads, the debug tools. `shell: open:` gives the number of undone freezes.
- **Alternatives considered:** deferring the log while the hub is open (the hub's state list and
  saves still read and write the card: the same hang one step later); suspending only threads
  that wait on objects of their own (games wait in the kernel all the time - the vblank, the
  buttons, the audio - and the object's owner is not known reliably); reading the wait object's
  name (driver-specific, nothing documented).
- **Consequences:** a freeze can take up to 2 s longer in a game that streams; the stat reads the
  card (no write: nothing changes for the save's rule "nothing may write between the freeze and
  the resume"). Locks other than the card's (the module manager, the Media Engine) are not
  covered: RISK-023.

## ADR-033 — Home draws the three styles; Classic is the XMB, with the system's own pictures

- **Status:** Accepted (2026-10-19), from the owner's third device round
  ([log](../research/logs/2026-10-19-home-hm10-run3.log)): "when I change the style, nothing happens" in Home,
  and Music, Photos, Play Time and Settings "look bad, not like they should".
- **Context:** VERIFIED from the code: Home drew only Modern (HOM-REQ-006 "planned"); its
  Settings row Style changed only the in-game menu's look. Every section but Games was the same
  dark card of rows (the previews `home-*.png` of that build). The project's first priority is
  the native look 1:1 (the plan; Classic is the default style, ADR-019).
- **Decision:** Home follows `config.txt`'s `style` like the in-game menu, and changes at once
  when Settings → Style changes. Three files draw the same `hm_view`: `view_classic.c`,
  `view_modern.c`, `view_minimal.c` (`home_view.c` dispatches and keeps what they share).
  - **Classic** is the XMB: the month's sky (the theme colour from the registry, as the game
    plugin reads it, through KUBridge) and its wave, drawn by the GE as triangle strips from the
    same crest and brightness profile as `ui/xmbbg.c`; the system's category icons
    (`topmenu_icon.rco` AO, AM, AL, AJ, BX), button pictures and battery
    (`system_plugin_fg.rco`) read from flash0 at start with the plugin's own RCO, RLZ, GIM and
    BMP readers (never shipped, ADR-013; drawn ones when missing); the categories in a row at the
    open one's (109, 70); games and photos as the XMB's game list (the chosen icon 144 x 80, the
    others 81 x 45, the earlier ones above the categories, the PIC1 sharp behind it); music and
    play time as items with an icon and a line; Settings and System as the XMB's option list
    (the categories slide away, the focus bar), a choice's options in the XMB's panel on the
    right. Left / right switch categories, up / down move, Enter opens a choice - the XMB's keys.
    Metrics: `ui/views.c` (480 x 272 captures) and VitaXMB (docs/research/m4-native-look.md).
  - **Modern**: the sections as icon pills (the chosen one opening into a white pill with its
    name, gliding); lists whose chosen row is a white pill gliding under the text, values on the
    right, on/off as switches, groups named in the accent colour, a scroll bar; Play Time with
    the games' icons and bars; Music as a card of what plays (art, time, bar, play state) beside
    the tracks; Photos as a grid (up / down a row).
  - **Minimal**: words on the darkened background, one column whose chosen line rests in the
    middle with a short rule under it.
- **Alternatives considered:** a separate Home style setting (the owner changed the one there is
  and expected Home to follow); drawing the XMB's wave on the CPU into a texture (the hub's
  Classic cost 11-16 fps on the PSP that way, docs/ui/styles.md); shipping our own copies of the
  icons (Sony's files are never redistributed, ADR-013).
- **Consequences:** reading the two RCO files and the sky costs Home some milliseconds at start
  (`home art: ... ms` in `home.log`); about 150 KB of heap for the pictures. The GE draws ~600
  more vertices a frame for the wave (60/60 in PPSSPP). In Classic, left / right no longer skip a
  music track (up / down choose one, Enter plays it), as in the XMB. Changing the style rebuilds
  the default background and the games' backgrounds (sharp PIC1 in Classic, blurred otherwise).

## ADR-034 — Home's USB shares the port with another plugin's device

- **Status:** Accepted (2026-10-19), from the owner's fourth device round
  ([log](../research/logs/2026-10-19-home-round4.log), build `b5cab03`).
- **Context:** VERIFIED from that log: six presses of USB Connection in Home, each
  `start bus 80243001, storage 80243006`, no activation; the XMB's own USB Connection worked.
  `0x80243001` is "already done" and `0x80243006` "driver in progress" (uOFW
  `src/kd/usbacc/usbacc.c`; PSP homebrew headers, e.g. xerpi/psp-uvc-usb-video-class
  `include/usb.h`; Yoti/psp_nandtool: "USBStor in use"). The owner's `PLUGINS.TXT` loads
  RemoteJoyLite for VSH and for GAME - and Home, a homebrew, runs as GAME. RemoteJoyLite's
  `UsbStart` (PSP-Archive/RemoteJoyLite `RemoteJoyLite_psp/usb.c`) starts the bus, its own
  driver and activates its device (PID `0x1C9`); its hook on `sceUsbStart("USBStor_Driver")`
  deactivates that device first (`hook_usb.c`, `UsbSuspend`) and `sceUsbStop` of the storage
  driver activates it again. Home took the bus's "already done" for a failure and stopped
  everything; the storage driver met the other device still active (INFERRED: its
  deactivation had not finished, or the hook was not there).
- **Decision:** the bus's "already done" means it runs: Home goes on and leaves that bus running
  at the end. When the bus reports an active device before the storage driver starts, Home
  deactivates PID `0x1C9` itself. The storage driver's "in progress" is tried again on every poll
  (every 250 ms) for up to 3 s, then given up with a message that names another plugin
  (RemoteJoyLite) as the likely holder. At the end Home stops only what it started and activates
  the other device again (harmless if its own hook did). `home.log` gets the bus's owner, its
  state before, the other device's deactivation and the storage driver's tries.
- **Alternatives considered:** asking the owner to turn RemoteJoyLite off for games (it is
  useful in games; the XMB copes, so Home must too); unloading the other plugin's module (not
  ours to stop); deactivating every PID (only `0x1C9` is known; others stay a message).
- **Consequences:** USB from Home works beside RemoteJoyLite, usbhostfs and PSPLink-style
  devices (INFERRED until HM12 passes on the owner's PSP); a device with another PID still
  fails, now with a message and a log line that names it.

## ADR-035 — Home's sections are stacks of pages; Settings and System in categories

- **Status:** Accepted (2026-10-20), from the owner's fourth device round
  ([log](../research/logs/2026-10-19-home-round4.log)): Settings and System were "one line
  without categories" - wanted: categories with menus to go into, swipes left and right between
  them, the console's settings, colours, the battery's percent, a choice of background; and
  "more customisation, a few unique themes".
- **Context:** VERIFIED from the code (`b5cab03`): each section was one list of rows; a game's
  options, Information and the HOME panel were overlays; no section could open a page of its
  own. Home's look had only the style.
- **Decision:**
  - **Pages.** Every section is a stack of up to four pages (`hm_view.page[]`, `depth`;
    `pages.c` builds them, `main.c` keeps the stack and the keys). `depth_pos` follows `depth`
    by time; Modern and Minimal draw every page within one step of it, shifted by its distance
    and faded (the leaving one faster), through a drawing wrapper (`hm_shift`) - the new page
    slides in from the right, the old one out to the left, and back. A page keeps its cursor
    while one above it is open. L / R change sections on any page (Modern, Minimal); Back closes
    a page; in Classic left closes it, as in the XMB.
  - **Classic** keeps the XMB's own motions: a category's settings are the option list (the
    categories and the items slide left, the chosen item's icon stays); a game's Triangle page is
    the XMB's option panel on the right; tiles (saved data, albums, photos) are the game list.
    The Settings items wear the system's own Settings icons from `topmenu_icon.rco` (BY theme,
    BW photo, BX system, BZ clock, CA power, CB sound, CD network, BU USB, EU browser, BH disc,
    BI Memory Stick; each label decoded alone from 6.60's file and looked at, 2026-10-20).
  - **Settings**: seven categories - Appearance (style, theme, accent colour, background, sky
    colour, wave, battery percentage, animations), Pocketwatch (start screen, sorting, fast save
    and load, the hub's clock), Music and Photos (repeat, shuffle, slideshow), Sound and Display
    (key sounds, brightness, AVLS), Date and Time, System (Pocketwatch's and the system's
    language, nickname), Network (WLAN power save). **System**: USB Connection, Internet Browser,
    Battery, Memory Stick, About This PSP, Power (sleep, turn off), Exit to the XMB.
  - **Home's own settings** live in `seplugins/pocketwatch/home.txt` (`hcfg.c`): accent (nine
    colours), theme (Dark, Glass, OLED Black, Paper - Modern's and Minimal's surfaces through
    `hm_palette`), background (as the style, the XMB's sky, the accent's gradient, plain, the
    wallpaper), sky colour (the XMB's theme colour or any month's), wave, battery percent,
    motion (normal, quick, gentle), repeat, shuffle. Not `config.txt`: its parser is the XMB
    plugin's too, at its 64 KB budget (RISK-022).
- **Alternatives considered:** one long list with group names (what the owner rejected); a
  separate screen per category with its own loop (no shared motion, every key twice); putting
  Home's settings into `config.txt` (the XMB plugin would carry their parsing).
- **Consequences:** `main.c` and the views were rewritten around pages; new strings for Home
  only (its translation pool grew to 16 KB); a theme can make the text dark (Paper), so every
  Modern and Minimal colour now comes from the palette. Previews render every page in every style
  and Modern in every theme (`home-*.png`).

## ADR-036 — Home does its own saved data, music player and photo albums and editor

- **Status:** Accepted (2026-10-20); its seeking superseded by
  [ADR-058](#adr-058--the-player-seeks-with-the-mp3-librarys-own-call-on-one-handle), its editor by
  [ADR-060](#adr-060--the-photo-editor-is-laid-out-as-a-professional-editor). From the same round: Triangle → Saved Data showed "the
  default PSP menu", not Pocketwatch's, and that menu "looks bad"; Music had no cover ("something
  instead of it") and no way to seek; Photos had no albums by game and nothing to edit with.
- **Decision:**
  - **Saved data** (`savedata.c`): the game's folders of `PSP/SAVEDATA` (not Pocketwatch's
    `PW...`), each with its `PARAM.SFO` title and details, time and size; ICON0s made by the
    loader thread as jobs nearest the cursor first, the chosen one's PIC1 behind the list. A
    save's page names it all and deletes it (a question first; its files, then its folder).
    Save states have a page of their own (continue from one, delete one).
  - **Music**: a thread reads every track's ID3v2 tag after the list is made (v2.2-2.4; title,
    artist, album; Latin-1, UTF-16, UTF-8 → UTF-8) with a few small reads per file, and where its
    picture is (APIC); the loader decodes the playing track's picture (else `cover.jpg` /
    `folder.jpg` beside it) into a 128 x 128 cover and a blurred background. Seeking re-opens
    the decoder at the frame nearest the time (the file's average rate; a frame is taken only
    when the next one follows it). Left / right: the previous / next track, held: seek 5 s
    steps; back from more than 3 s played restarts the track. Repeat (off, all, one) and shuffle
    (never the same track twice in a row; back follows the history). Now Playing is a page.
  - **Photos**: albums by folder - All Photos, then each folder of `PICTURE/`, `DCIM/` and
    `PSP/SCREENSHOT/` (prxshot's), a folder named by a disc ID titled with that game's name; a
    photo deleted with a question. The editor (`photo_edit.c`): quarter turns, mirror, eight
    filters, brightness, contrast, saturation, warmth, vignette, on a preview the size of the
    screen; Save a Copy decodes the picture again (at most 1600 px on its longest side), applies
    the same and writes `<name>_edit.jpg` beside it (`stb_image_write`, public domain / MIT,
    quality 92).
- **Alternatives considered:** the firmware's Saved Data dialog (what the owner rejected: not
  Pocketwatch's look); `sceMp3ResetPlayPositionByFrame` for seeking (its behaviour with the
  streamed input is not documented); a JPEG encoder of our own (stb's is small and proven).
- **Consequences:** deleting saved data is now Home's: a bug there loses a game's saves (the
  folder name is checked, only files of that folder are removed, a question comes first;
  RISK-025). Reading tags opens every track once after the start (a few ms each, on a
  low-priority thread, paused for USB). An edited copy needs memory for the picture twice.

## ADR-037 — Screenshots from the hub, into the game's album

- **Status:** Accepted (2026-10-20), from the owner's fourth round: "add to our XMB taking a
  screenshot in a game" (Photos by game).
- **Context:** the owner's card has screenshots by prxshot (`PSP/SCREENSHOT/<disc ID>/pic_*.bmp`,
  on NOTE) and by another tool (`PICTURE/<disc ID>/<disc ID>_<date>.png`). The hub already
  captures the game's last frame for its backdrop (`pw_fb_game`), and its debug build writes the
  screen as a PNG (`pw_fb_screenshot`).
- **Decision:** a complication, Screenshot, after Load State: the captured frame (the game as it
  was when HOME was pressed, without the menu) as a stored PNG (`core/png.c`, 392 KB, no
  compression - no code to add to the kernel budget) in `ms0:/PICTURE/<disc ID>/<disc ID>_<date
  and time>.png`; a message names the folder. Home lists that folder as the game's album
  (ADR-036); the XMB's Photo shows it too.
- **Alternatives considered:** a button combination in the game (another thing to hook in the
  game's input; HOME already freezes it and captures the frame); BMP like prxshot (larger, and the
  PNG writer is already in the plugin); compressing (the kernel plugin has 2 KB of its budget
  left).
- **Consequences:** 1.4 KB of the kernel plugin's budget (194 552 of 196 608 bytes loaded,
  debug build); 392 KB of temporary memory while it writes.

## ADR-038 — What other PSP plugins do, in Home: screen off on HOLD, a sleep timer, favourites, the aurora

- **Status:** Accepted (2026-10-20), from the owner's request to "study which plugins improve the
  PSP and make a better, lighter Pocketwatch". The owner's `PLUGINS.TXT` (2026-10-19) runs Hold+
  (`hold.prx`: the screen off while a game or music plays), dayviewer (the weekday in the XMB's
  clock), category_lite (the games in categories), prxshot (screenshots: ADR-037), bright3,
  battery_icon, power.prx (CPU clocks), CXMB (themes).
- **Decision:** inside Home, where it costs nothing outside it:
  - **Screen off with HOLD** (Settings → Sound and Display; off, while music plays - the
    default - or always): after half a second of HOLD the backlight goes off
    (`sceDisplaySetBrightness(0, 0)` through KUBridge, the pre-6.x NID translated by ARK-4's
    table; the level read before with `sceDisplayGetBrightness`) and the CPU drops to 222 MHz
    (the Media Engine decodes the music); Home draws nothing meanwhile. HOLD off: the light and
    the clock come back. Never left dark when Home ends.
  - **Sleep timer** (Music and Photos: off, 15, 30, 60, 90 min, not kept): at its end the music
    stops and the PSP sleeps.
  - **The clock** shows the date and time (as before), the time only, or the weekday and time.
  - **Favourites**: Square on a game stars it (`favorites.txt`, the games' files); favourites come
    first, a star on their picture in every style.
  - **Aurora**: a background - three soft lights in the accent's colours drifting slowly over the
    night, three scaled 64 x 64 textures a frame (the GE filters them smooth).
- **Alternatives considered:** a HUD over games (missyhud: frame rate, battery) - it means drawing
  into every game's frame from the kernel plugin, at its 2 KB budget, a device-only test; per-game
  CPU clocks (ARK-4's own `overclock` setting does it); categories as their own section (the
  favourites and the sorting cover the owner's need for now: OPEN QUESTION for the next round).
- **Consequences:** two more KUBridge calls into `sceDisplay_driver` (RESEARCH REQUIRED on the
  device: that 0 turns the backlight off on a PSP-3000 and the level read back restores it -
  HM26); `home.txt` gets `hold` and `clock`.

## ADR-039 — Home's own keyboard; the d-pad seeks; a game's screenshots by date and game

- **Status:** Accepted (2026-10-20), from the owner's fifth round (build `cee6786`, spoken
  feedback, no log): the nickname opened "the default keyboard" and its "keep or cancel the
  text?" question - both should be Pocketwatch's, "in games and in the menu"; no way to seek in
  Music and no hint of what to press; covers "load very long", changing tracks lags; a
  screenshot's name did not fit and did not say which game it came from; the photo grid "goes
  somewhere" after the third press of right; one second of Castlevania showed nothing in Play
  Time.
- **Decision:**
  - **Keyboard** (`home/src/keyboard.c`, portable, HOM-REQ-030): three pages - Latin, Cyrillic
    (Ukrainian letters when Home's language is `uk`), signs - five rows each, the last one
    Shift (one capital), the next page, Space, Erase, Done. The PSP keyboard's own buttons:
    Square erases, Triangle a space, L / R move the caret, SELECT the next page, START done.
    Back with changes asks "Save the changes?" - Save, Discard, Keep Editing - in the same
    panel; Back without changes closes it. Drawn once (`hm_draw_keyboard`) in each style's
    colours. It replaces `sceUtilityOsk` for the nickname; the firmware's keyboard is no longer
    used by Home.
  - **In games** the keyboard stays the firmware's: a game draws `sceUtilityOsk` itself, and
    replacing it means a module that answers the game's `sceUtilityOsk*` calls and draws into
    the game's frame; the kernel plugin has 2 KB of its budget left (ADR-037). OPEN QUESTION:
    the options and the probe are in [m7-game-keyboard](../research/m7-game-keyboard.md).
  - **Music keys:** left / right seek (10 s a press, then 5 s steps while held), up / down the
    previous / next track; the hints name both (d-pad icons in every style). This replaces
    ADR-036's "tap left / right: track, hold: seek" (a tap acted only on release, and the hold
    was not discoverable). The track asked for is tagged first, so its cover does not wait for
    every other file's tag; `home.log` says what a cover and a track's start cost (`home cover:`,
    `home music: a track started in N ms`) - the slowness is measured before it is changed.
  - **Photos:** a picture in a game's folder is named by when it was taken ("2026/10/20 14:05")
    under the game's title (All Photos too); others by their file name without the extension.
    A game's page lists its Photos (the album) when it has any. Modern's grids scroll by whole
    rows (the scroll followed the selection's position divided by the columns: a quarter row
    at every step to the right).
  - **Play Time:** every game with recorded time, seconds when under a minute (the page showed
    a minute or more only).
- **Alternatives considered:** keeping the firmware's keyboard with Home's colours (it cannot be
  restyled); tap = track, hold = seek with hints only (the owner asked how to seek at all);
  showing the file name in full on two lines (it says nothing about the game).
- **Consequences:** Home's strings gain the keyboard's, the seek and track hints and `dur_s`;
  Cyrillic keys need the system font's Cyrillic (the translations already do). Text input in
  the Internet Browser is still the firmware's (it is the firmware's own page).

## ADR-040 — Modern's photo grids: two rows of albums, a scroll of its own; slow frames logged

- **Status:** Accepted (2026-10-20), from the owner's sixth round (build `ead1320`, Modern,
  `home.log`): Photos still "jerky, jumps, I cannot look through a folder", and "micro freezes".
  Supersedes ADR-039's grid scrolling.
- **Decision:**
  - **Albums: four columns of 100 x 56 cards, two rows on screen.** The 136 x 76 cards left one
    row on screen: the second row was drawn under the hints' edge with no opacity, while the
    scroll kept the chosen row second, so right after the third album the choice went off
    screen (VERIFIED in PPSSPP with the owner's 15 photos in 7 folders, `view_modern.c`).
  - **A grid scrolls by its own motion** (`hm_page.grid_pos`, eased in `main.c` to the chosen
    row): ADR-039's scroll followed `selpos` and moved only while `selpos` crossed into the
    next row, so a move down waited for three quarters of the motion and then jumped, while the
    focus ring was already there.
  - **Inside an album two rows show whole** (the third peeks out): the last row stops second,
    not faded under the edge.
  - **Slow frames named:** each frame of 100 ms or more adds `[ms page keys rows state draw
    warm ge stamps]` (ms per part) to the next `home running:` line, so the next round says what
    a freeze was. RESEARCH REQUIRED: the round-6 log has 500-1035 ms frames (one in 30 s) and
    10-30 late frames in 30 s, but no part named; no fix is guessed (AI workflow, section 7).
- **Alternatives considered:** keeping the large cards with one row on screen (the next row
  would not show at all); moving the focus ring by the eased position too (the grid's motion
  was the jump, not the ring).
- **Consequences:** HM25 is run again; previews show seven albums in two rows.

## ADR-041 — Home keeps the clock: the last time known, then the internet's

- **Status:** Accepted (2026-10-20), from the owner's request: after weeks unused the PSP asks for
  the date again; "our own variant that sets the right date and time in real time". The owner
  asked for the clock to be set (an explicit decision for these writes; RISK-026).
- **Decision:** at Home's start, before anything is written, the clock is checked: lost when the
  kernel refuses it or it reads before 2016 (the owner's PSP read 2011-01-16 after the firmware's
  date screen was passed). A lost clock is set to the newest time, from 2016 on, of 14 files -
  those Pocketwatch writes (each carries the clock of its last write) and those the PC copies
  (their build's time) - with `sceRtcSetCurrentTick` through KUBridge, read back and logged; a
  toast says so. Then, when the WLAN switch is on, Home asks an SNTP server (pool.ntp.org, then
  time.google.com) over the first connection saved in the XMB, on a thread of its own, and sets the
  exact time; Settings → Date and Time → Set Time from Internet does the same by hand and shows
  the last answer. A date from 2016 on is never changed by the restore. Research:
  [m7-clock](../research/m7-clock.md).
- **Alternatives considered:** a kernel PRX of our own for the setter (NTPSP's way; KUBridge
  already gives Home kernel calls); the syscon's clock written directly (bypasses the rtc module's
  base); a time file written by Pocketwatch (the files' own times already are one); asking the
  owner (what was to be avoided).
- **Consequences:** the restored time is short by the time the PSP sat unused until the internet's
  time comes; the network's modules are loaded the way NTPSP loads them, because Home's browser
  path failed in `sceNetInit` on the owner's PSP (RESEARCH REQUIRED, HM29). Whether the firmware's
  own date screen still appears after Home set the clock is an OPEN QUESTION (C1).

## ADR-042 — Pocketwatch measures the battery itself (calibration run)

- **Status:** Accepted (2026-10-20), from the owner's request: "our own variant for the battery,
  so it can really be calibrated and its data, charge and time left understood".
- **Decision:** Home's Battery page shows the gauge's current (read every 10 s by Home's battery
  thread), our estimate of the time left or to full (`core/battmodel.c`: the run's average current,
  "about"; the system's figure until there is history), the capacity left and full, voltage,
  temperature, cycles; and a calibration run: Start → charge to full → unplug → leave Home on until
  the PSP turns itself off. Home's battery thread writes a sample every 30 s to
  `battery-cal.txt` and keeps the PSP from sleeping on its own; the next start computes the
  capacity delivered (the current integrated, gaps over 120 s and their charge left out) and keeps
  it in `battery.txt`: "Measured by Pocketwatch: N mAh (P % of the gauge's full)", how long it
  lasted and at what average current. Research:
  [m7-battery-calibration](../research/m7-battery-calibration.md).
- **Alternatives considered:** writing the battery's gauge (out of bounds); a voltage curve (the
  voltage sags under load; needs many runs); the XMB's Battery page (the XMB plugin has no room,
  RISK-022).
- **Consequences:** a run takes hours and needs Home on; the gauge itself may re-learn its full
  capacity from the same run (INFERRED, compared in HM30).

## ADR-043 — A performance overlay over Home and in games, as a module of its own

- **Status:** Accepted (2026-10-20), from the owner's request: "an overlay over Pocketwatch's menu
  and in games, a performance overlay, to understand it - all the data of the PSP".
- **Decision:** over Home: Settings → Pocketwatch → Performance Overlay shows Home's frame rate,
  its CPU and GE time a frame, its longest frame, the clocks, the memory and the battery, once a
  second. In games: `pocketwatch_perf.prx` (perf/), a kernel module of its own that ARK-4 loads
  only while its PLUGINS.TXT line is on; Settings → Pocketwatch → Overlay in Games (Off / FPS /
  Full) turns that line on or off and writes `perf.txt`. At every vblank it writes its lines into
  the frame about to be shown - the hub's while the hub is open - and once a second it measures:
  the frames the game showed, the CPU's busy share (the idle threads' run time), the clocks, user
  memory, the battery. L + R + SELECT held a second cycles Off / FPS / Full. Spec:
  [perf-overlay](../subsystems/perf-overlay.md); research:
  [m7-perf-overlay](../research/m7-perf-overlay.md).
- **Alternatives considered:** inside the game plugin (no room: 2 KB left, ADR-037); always loaded
  (its RAM in every game); hooking the game's `sceDisplaySetFrameBuf` (exact frames, but a patch in
  every game's path for a diagnostic).
- **Consequences:** a new build target (`perf/`), copied by `scripts/deploy.ps1`; nothing changes
  for a game while it is off. The game's GE load is not shown (OPEN QUESTION); states with the
  overlay on are RESEARCH REQUIRED (RISK-027, PF2).

## ADR-044 — Wi-Fi in Home: the PSP's own connections, made and tested from Home

- **Status:** Accepted (2026-10-20), from the owner's round-7 report: "Wi-Fi does not work, it says
  not available; and if it did, how would I connect - there is no section, I cannot choose or type
  a password".
- **Context:** the PSP had no saved connection (`profile=0`), and Home's network never came up:
  `sceNetInit` 0x8002013A. Cause (VERIFIED, [m7-wifi](../research/m7-wifi.md)): Home's own build -
  newlib's glue pulled three `sceNetInet` stubs at the end of the link, `psp-fixup-imports` ("stubs
  out of order") built the `sceNetInet` table over `sceNet`'s and `sceWlanDrv`'s stubs, so loading
  the inet library unlinked `sceNetInit`. Fixed in `home/Makefile` (`-Wl,-u` takes them with the
  others); `tools/check_prx.py` fails when a stub is in two tables.
- **Decision:** Settings → Wi-Fi Connections: the saved connections (the XMB's list: name,
  network, security; Test Connection - connect, the address, the internet's time set while there -
  and Delete) and Search for Networks (each network once, strongest first: signal, security,
  "Saved"). Enter on one: open - saved and tested; WPA / WPA2 - its password on Home's keyboard
  (8-63 characters or 64 hex digits; never logged nor kept), saved and tested; WEP - the XMB's
  Network Settings. A connection is made the firmware's way (`sceUtility_netparam_internal`:
  Create, Set, Copy, as the firmware's `pspnet_ap_dialog_dummy` does; the same calls through
  KUBridge when they are not linked for a user program): its name the SSID, DHCP, no proxy, that
  network's earlier one replaced. Every job on a thread of its own (`home net:` lines). The search's
  three calls are `IMPORT_FUNC`s of the SDK's own `sceNetApctl` table, linked right after it
  (`home/src/apctl_scan.S`). The owner's request is the decision for these writes of the PSP's
  network settings (the registry on flash1, as the XMB's Network Settings writes them).
- **Alternatives considered:** sending the owner to the XMB's Network Settings (it scans and asks
  the password too, but the owner wants it in Pocketwatch); writing the registry keys
  (`/CONFIG/NETWORK/INFRASTRUCTURE/<n>`, two dozen keys - one wrong breaks the XMB's list); the
  netconf dialog (connects only through saved ones).
- **Consequences:** Home writes network settings (RISK-028). Not verified on a device: the security
  values (3 WPA-TKIP, 4 WPA-AES) and the key type (1 a passphrase) - a test of a connection made
  in the XMB logs `secure=` and `wpa_type=` to compare (HM33). PPSSPP: the search works, saving
  is refused (it has no netparam calls).

## ADR-045 — A volume and brightness indicator in Home and in games; a brightness boost

- **Status:** Accepted (2026-10-20), owner: "add more screen brightness; show a brightness overlay
  when I press it, in the menu and in games, and the sound too".
- **Decision:** Home and the overlay module (perf/) read impose's volume (0-30) and mute and the
  SCREEN button's step (0-3) every 100 ms; a change shows for 1.5 s - over Home a pill at the
  bottom (icon, bar, "18" / "Muted" / "3/4 92%"), in a game a black box with a white edge at the
  bottom middle ("VOL 18", "MUTE", "BRIGHT 3/4 92%", a bar of 30 or 4 steps). The boost: whenever
  the system sets the backlight (SCREEN, the charger, a wake) it is set to that level plus the
  boost's share of the way to 100 (Brighter 50 %, Maximum 100 %); off gives the system's level
  back; the screen off stays off; another plugin setting its own back 4 times within 400 ms of
  ours makes it give up (logged). Settings → Sound and Display → Brightness Boost, Volume and
  Brightness (Off / In Home - the default / Home and Games). `perf.txt` keeps them for both
  (`mode`, `osd`, `boost`); the module's PLUGINS.TXT line is on while a game needs it. The logic is
  portable (`perf/src/overlay.c`), Home links it too.
- **Alternatives considered:** Pocketwatch's own SCREEN steps (a hook in impose's button path);
  a fifth step (the same, and the system's values per step are not known); a plugin of its own
  (one more PLUGINS.TXT line and thread for the same readings).
- **Consequences:** with the indicator in games or a boost the module runs in every game
  (RISK-027, PF2, PF3); the boost costs battery and may meet another brightness plugin (bright3 on
  the owner's PSP: RISK-029). The system's level for each step: OPEN QUESTION (the indicator shows
  it, PF3 / HM34).

## ADR-046 — After USB, the FAT driver's cached tables are dropped before Home writes

- **Status:** Superseded by ADR-048 (2026-10-21: the call is refused to a user program). Accepted
  2026-10-20, from the owner's round 7: "the overlay does not work in the
  game" - `pocketwatch_perf.prx` was not on the card.
- **Context:** VERIFIED (the card, chkdsk, [log](../research/logs/2026-10-20-round7-home.log)): the
  deploy copied it (hash checked) while Home lent the card over USB; when USB ended Home appended
  its USB line to `home.log` from the PSP's cached copy of `seplugins/pocketwatch/`, which put the
  folder back as before the deploy: the new file's entry gone (chkdsk: one lost chain of 32 KB, its
  cluster), the other PRXs' entries back to the earlier deploy's times.
- **Decision:** `hm_usb_stop` drops the cache after deactivating: `sceIoDevctl("fatms0:",
  0x0240D81E)` (oslib `src/usb.c`, Custom Home `usb.c`; PPSSPP `sceIo.cpp` names it "invalidate MS
  driver file table cache"); nothing writes before it; its result in the USB line ("cache
  dropped").
- **Alternatives considered:** unassigning and assigning `ms0:` again (fails while a file is open;
  the library's threads hold some); no write after USB (the next one would still see the stale
  tables).
- **Consequences:** test HM32. The lost 32 KB on the owner's card is harmless (`chkdsk /F` makes it
  FOUND.000).

## ADR-047 — Pocketwatch's own brightness steps take bright3's place; the overlay module in the XMB

- **Status:** Accepted (2026-10-20), owner (round 8): "I have some custom plugin for more
  brightness - remove it, study it and adapt it into Pocketwatch"; "in the XMB, when I change the
  brightness, no slider is shown"; the performance overlay "looks terrible, not understandable".
- **Context:** the plugin is `bright3.prx` (VSH, GAME, POPS lines) with `bright3.ini`: a list of
  backlight levels 0-100 ("28 40 50 60 70 80 90" on the owner's card) that the SCREEN button goes
  through instead of the system's four. Learnt from its settings file and the system calls it
  imports only (the display's brightness setter, a start-module handler); no code of it is taken
  (it has no licence).
- **Decision:** Settings → Sound and Display → Brightness Boost gains **Own Steps**: a tap of
  SCREEN (released within 600 ms) is the next of `perf.txt`'s `levels` (default 20 30 40 50 60 70
  85 100; the owner's card: bright3's list and 100), round; a long press stays the system's
  (the screen off); whatever the system sets (the charger, a wake, impose's own step) the step's
  level is put back; the indicator says "5/8 70%". A change while SCREEN is pressed is not counted
  as another plugin's fight (rapid taps had looked like one). The overlay module also gets a VSH
  line (on while the indicator is Home and Games, or a boost or the steps): in the XMB it shows only
  the brightness (the XMB has its own volume bar) and keeps the boost or the steps; no numbers there.
  bright3's three lines are turned off in the owner's PLUGINS.TXT (its files kept). The numbers in
  games are short words on a black box; Home's overlay is a card: the frame rate big, then Frame,
  Slowest, Processor, Free memory, Battery with their values. (The in-XMB part was withdrawn by
  ADR-049: the module runs only in a real game, never in the XMB.)
- **Alternatives considered:** keeping bright3 beside the boost (two plugins setting the backlight:
  RISK-029); a hook in impose's SCREEN handling (a patch in the system's path for a list of
  numbers); the XMB plugin drawing the indicator (it is at its 64 KB budget).
- **Consequences:** the module runs in the XMB too (its thread at every vblank; hold START at
  power-on starts with no plugins if it misbehaves); test PF4; PLUGINS.TXT's `pocketwatch_perf.prx`
  lines are two (`hm_plugins_set` takes the runlevel).

## ADR-048 — After USB, Home starts again before it writes anything

- **Status:** Accepted (2026-10-21), from the owner's round 9: "Pocketwatch does not start with the
  console, in games neither the hub nor the overlay".
- **Context:** VERIFIED (`home.log`, chkdsk 2026-10-21): ADR-046's `sceIoDevctl("fatms0:",
  0x0240D81E)` answered 0x800200D1 to Home (a user program; oslib and Custom Home called it from
  kernel mode). So Home's line after USB was written from the stale tables once more: the folder
  put back, `pocketwatch.prx` and `pocketwatch_xmb.prx` pointing at clusters the PC had freed
  ("first allocation unit is not valid"), the PC's copies lost (5 chains, 384 KB) - the game plugin
  and the XMB plugin no longer loaded (no hub, no start in Home). Not the PSP, not another plugin.
- **Decision:** when USB ends Home writes nothing - no log line, no settings, no stamps - and starts
  itself again (`hm_launch_eboot` of `PSP/GAME/PocketwatchHome/EBOOT.PBP`; the XMB if that fails):
  the start reboots the kernel, its FAT driver with it, which reads the card afresh. Leaving Home
  during USB writes nothing either. The card stays lent (Home's writers off) until then.
- **Alternatives considered:** the same devctl in kernel mode through KUBridge (its effect on the
  6.61 FAT driver is not documented; a third loss would cost the owner's card again); unassigning
  `ms0:` (fails while a file is open).
- **Consequences:** a few seconds' restart after every USB session; the USB line of its end is no
  longer in `home.log` (its start's still is). The owner's card needs `chkdsk /F` and a deploy
  once. HM32 again.

## ADR-049 — The overlay module runs only in a real game (never in the XMB or Home)

- **Status:** Accepted (2026-10-21); its "no VSH line" superseded for the brightness bar by
  [ADR-059](#adr-059--the-brightness-is-a-vertical-bar-at-the-left-with-everywhere-the-overlay-module-shows-it-in-the-xmb)
  (only while the indicator is Everywhere). From the owner's round 9: after the card was fixed
  and the module finally loaded for real, "Pocketwatch does not start with the console; in games neither
  the hub nor the overlay".
- **Context:** VERIFIED (`home.log`, `boot.txt`, PPSSPP). The overlay was "never seen in games"
  because its file kept being wiped by the FAT bug (ADR-048); once the card was repaired the module
  loaded for the first time, at the XMB (its VSH line, ADR-047) and when Home launched. Its kernel
  thread reads and writes the display framebuffer; during the XMB→Home boot handoff this crashed
  Home before its first frame (`boot.txt`: Home start, then the guard's "Home did not start"; no
  line in `home.log`). The same build of Home starts perfectly in PPSSPP, so the module, not Home's
  code, was the cause. The console power-offs the owner also saw were the earlier corrupt-PRX phase
  (ADR-048), fixed by `chkdsk /F`.
- **Decision:** the module does nothing outside a real disc/ISO game: `module_start` checks
  `is_disc_apitype(sceKernelInitApitype())` (the game plugin's own check, `core/game.c`) and starts
  its thread only then - in Home and the XMB it loads but never touches the display, the controller
  or the syscon, so it cannot affect boot. No VSH line (the XMB brightness indicator and in-XMB
  steps are withdrawn); Home's Settings manage the one game (UMD) line. The thread also waits ~2 s
  into a game before its first framebuffer write. Confirmed on the device by elimination: with the
  module's lines off, the console boots Home reliably (round 9 follow-up).
- **Alternatives considered:** detecting the XMB by module name at run time (what crashed was the
  boot handoff, before any such check ran); a user-mode overlay (cannot write another process's
  framebuffer); keeping the in-XMB brightness (not worth a kernel module in the boot path -
  bright3, the owner's own plugin, remains the safe way to get XMB brightness).
- **Consequences:** the overlay, the indicator and Own Steps work in games and in Home (Home draws
  and applies them itself, no module); in the XMB there is no Pocketwatch brightness. The in-game
  module is still experimental (never validated on the device); the apitype guard means its worst
  case is one game misbehaving, never a boot that fails. Test PF1/PF3/PF4 in one game; the module's
  lines stay off until the owner turns a feature on. RISK-031. The overlay draws into every
  framebuffer the game shows (`pw_fbset`), not one: the PSP double-buffers, so a single-buffer
  write appeared every other frame - the flicker the owner saw (2026-10-21, PRF-REQ-011).

## ADR-050 — A new dark "Nebula" theme, built in stages

- **Status:** Accepted (2026-10-23), from the owner's request (with reference screenshots): a new
  dark theme for Home - a starry night sky with wave ribbons, card tiles, a compact performance
  pill, a control-centre HOME panel (toggles and brightness/volume sliders) and settings with a
  left sidebar and switches; "optimise the render, test on PPSSPP".
- **Context:** much of this is close to Home's Modern style already (cards, section pills,
  switches). A whole new look is a large, multi-step effort; it is built in stages, each checked in
  PPSSPP, not in one change.
- **Decision:** stage 1 (done) is the background: `hm_make_nebula` (home_view.c) draws a near-black
  sky, a deterministic star scatter and four gentle wave ribbons, computed once (the cheapest
  background, no per-frame cost); a new `background = nebula` option (hcfg), paired with a dark
  theme (OLED). Stage 2 (done): the performance overlay is a compact green pill on a second row at
  the top left (the FPS and the frame time, a drawn dot between); Modern makes room below it when it
  is on. Stage 3 (done): the settings are two panes in Modern - a left sidebar of the categories,
  the highlighted category's options on the right (`draw_settings`, view_modern.c); the focused
  pane is highlighted, d-pad up/down move within it, Enter/right enters the options or opens
  Wi-Fi/Calibration, left/Back returns to the sidebar, left/right change a choice (`settings_keys`,
  main.c; the page carries the options in `hm_page.opt`, built by `build_cat_opts`, pages.c).
  Classic and Minimal keep the drill-down (the sidebar row opens a page of its own), so only Modern
  reads the new `hm_page.is_settings` flag. Stage 4 (done): a game with no ICON0 gets a coloured
  card with its initial in Modern - a deterministic hue per title (`game_card`, view_modern.c), so
  the Games row reads as colourful cards on the dark sky; games with real art keep it, Classic and
  Minimal keep the plain disc card. Stage 5 (done): the HOME panel is a control centre - each
  action a row with its icon in a tinted square (Close, USB, Exit to the XMB, Sleep, Restart, Turn
  Off; `panel_glyph`), a battery bar across the bottom (`panel`, view_modern.c); Restart relaunches
  Home (`PANEL_RESTART`, main.c). A brightness slider is the top row (`PANEL_BRIGHT`, `panel_slider`;
  left/right adjust the 0-3 system backlight through `hm_bright_get`/`hm_bright_set`, the same impose
  level Settings' Brightness row uses, ADR-045; Classic and Minimal show the level as text). The
  volume slider and the Wi-Fi/CPU toggles the reference shows are still deferred: the PSP has no safe
  user-mode volume setter, and the toggles need their own device-tested flows. Stage 6 (done): the Games section's root is a landing launcher in
  Modern - a Resume hero for the most recently played game (`hm_page.hero`, the max `last` in the
  library; its art, title, line and Play), a status card beside it (the clock, a battery bar and the
  card's free space `hm_view.free_mb`), and a row of section tiles below (Games, Music, Photos,
  Settings, System) (`dash`, view_modern.c). Focus is `hm_page.opt_sel` (0 the hero, 1..5 the
  tiles): on the hero Enter plays and Triangle opens its page; on a tile Enter opens that section
  (the Games tile pushes the full library, the plain carousel at depth 1). The games root carries a
  new `hm_page.is_dash` flag set only for Modern at depth 0 (`hm_page_build`, pages.c); `open_tab`
  clears the two-pane/launcher focus so a section starts fresh. Classic and Minimal keep the plain
  carousel/list. Each stage is a Modern-style refinement, never a change that can affect boot or
  the plugins.
- **Alternatives considered:** one big rewrite (untestable in steps, risky); bundling the look into
  a single preset theme value (deferred until the card/panel styling is done).
- **Consequences:** `background = nebula` is selectable now (owner review in PPSSPP). The rest is
  tracked here and in [project-status](../project-status.md); never present an unbuilt stage as done.

## ADR-051 — The in-game overlay never writes to a framebuffer it does not own

- **Status:** Accepted (2026-10-25); the exit-power-off fix below is **superseded by
  [ADR-053](#adr-053--the-in-game-overlay-thread-paces-on-the-thread-manager-so-it-stops-on-exit)**
  (the power-off was the overlay's thread blocking in the display driver, not a framebuffer write).
  The freeze root cause and the VRAM-only-writes rule here still hold. From the owner's device
  report (build `fcab0aa`): HOME in a game blurred then froze the console for good, needing a
  reboot; it happened with RemoteJoyLite off too. See
  [m7-ingame-freeze](../research/m7-ingame-freeze.md).
- **Context:** VERIFIED by inspection. The performance overlay (`perf/src/main.c`, a kernel thread,
  ADR-043) drew the FPS/OSD every vblank onto whatever buffer was *displayed* when it differed from
  the game's own (`top != game`) — intended to cover "the hub's frame". But the hub displays its
  own framebuffer, in the plugin's kernel RAM, with a width/stride the overlay only guessed from
  the display. Writing there is a kernel write into another module's memory; it faults, and a fault
  in a kernel thread hangs the PSP. The blur (the hub's backdrop) rendered, then the overlay's next
  vblank faulted on the hub's buffer. New since ADR-032, so HOME regressed after the overlay shipped.
- **Decision (revised after round 11b):** the first attempt only drew when `top == game`; on the
  device that made the overlay **invisible** (the owner's games display from a back buffer, so
  `top != game` during normal play too). The reliable distinction is the **memory region**: a game
  framebuffer is in VRAM (`0x04000000-0x041FFFFF`), while the hub's page is in a kernel partition
  (`PW_MEM_PARTITION_SPARE`). So the overlay tracks and draws onto **only VRAM buffers** (`is_vram`,
  `perf/src/main.c`) and never reads the displayed buffer at all; the hub's page can never match, so
  it is never written. While the hub is open the game's buffers are off-screen, so the overlay shows
  nothing but faults nothing.
- **Exit power-off (same round):** with the overlay on, quitting a game black-screened and powered
  the PSP off. `module_stop` waited for the thread with no timeout, but the thread blocks in
  `sceDisplayWaitVblankStart`; while the game exits the display is torn down, vblank stops, the
  thread never ends, the exit hangs and the PSP powers off. Fixed: `module_stop` waits briefly then
  `sceKernelTerminateDeleteThread`.
- **Alternatives considered:** `top == game` (invisible, above); a shared "hub open" flag between
  the two PRXs (no reliable channel); pausing by frame activity (a one-frame race on open). The VRAM
  region test needs no cross-module coordination and no displayed-buffer guesswork.
- **Consequences:** the FPS pill shows in games again and never over the hub, the freeze and the
  exit power-off are gone (games that render to RAM, not VRAM, would show no overlay — rare). Device
  test PF-FREEZE: HOME opens in several games with the overlay on, quitting to the XMB works, with
  and without RemoteJoyLite. The overlay stays off on the owner's card until that passes.

## ADR-052 — A start-up animation for Home

- **Status:** Accepted (2026-10-25), from the owner's request ("add some animation when startup
  console").
- **Context:** Home faded in from black (`hm_view.appear`) but had no identity on boot.
- **Decision:** a brief wordmark overlay drawn last in `hm_draw` (`hm_view.intro`, 0-256): the watch
  mark in an accent square, "Pocketwatch" and the tagline "Pause. Save. Play." hold ~650 ms, then
  fade out ~450 ms to the launcher. Any button skips to the fade-out. The reference time `t_boot` is
  captured at the main loop's start, not `t_open` (which can reset mid-session). It is Home-only and
  costs nothing after it ends (`intro == 0`).
- **Consequences:** every Home start (cold boot or a relaunch) shows the ~1.1 s intro; it is
  skippable. Preview `home-intro`. If relaunches (USB, Restart, return from a game) make it feel
  repetitive, a cold-boot-only gate can be added later.

## ADR-053 — The in-game overlay thread paces on the thread manager so it stops on exit

- **Status:** Accepted (2026-10-26), supersedes the exit-power-off fix of
  [ADR-051](#adr-051--the-in-game-overlay-never-writes-to-a-framebuffer-it-does-not-own). From the
  owner's device report and `pocketwatch.log` (build `9929fff`): the overlay shows in games and HOME
  no longer freezes (ADR-051's VRAM-only writes worked), but quitting a game to the XMB still
  **black-screened and powered the PSP off**, with the overlay on. See
  [m7-ingame-freeze](../research/m7-ingame-freeze.md), rounds 11c–11d.
- **Context (root cause, VERIFIED by log + code):** the log ends every quit at `shell: quit: the
  game's exit callback was invoked` with all eight IPL seeds present (not the seed hang), then the
  reboot dies. The power-off is **independent of drawing** — a build whose overlay drew nothing
  (`PocketwatchPerf 0%` CPU in the hub) still powered off — so it is not a framebuffer write. The
  cause is the overlay **thread**: it paced on `sceDisplayWaitVblankStart`, a display-driver call,
  and was the only Pocketwatch thread that blocks in a driver. On the game's exit the kernel unloads
  the module (`module_stop`), but the thread is stuck inside the display driver as that driver is
  torn down for the reboot, so it cannot be woken and **outlives `module_stop`** — a thread left
  running in unloaded memory, which the plugin's own `module_stop` warns is fatal
  (`plugin/src/core/main.c`: "no thread of ours may outlive this function"). The reboot then faults
  and the firmware powers the PSP off. This is why the ADR-051 `module_stop` edits (terminate, then
  no-terminate) made no difference: the thread is unstoppable while it sits in the display driver.
- **Decision:** the overlay thread **paces on `sceKernelDelayThread` (~one frame), never
  `sceDisplayWaitVblankStart`**, and re-checks `g_run` immediately after the delay — exactly the
  shape of every other Pocketwatch thread (`shell_thread`, `main_thread`, the probe). It is then
  never blocked in a driver, so `module_stop` (`g_run = 0`, bounded wait, then `TerminateThread` as
  a safe last resort) ends it within a frame, before the reboot. The FPS counter samples buffer
  flips each loop, which a 15 ms pace still catches; the small overlay card draws onto every tracked
  buffer regardless of vblank phase (ADR-051), so losing vblank sync does not make it flicker.
- **Also reverted (invisible overlay):** ADR-053's first attempt gated drawing on reading the
  displayed buffer (`sceDisplayGetFrameBufferInternal`); on the device that call did not return a
  usable address and the overlay vanished (device rounds 11a and 11c — both times the displayed
  buffer was read; it was visible only in 11b, when it was not). The overlay no longer reads the
  displayed buffer: it writes the FPS/OSD onto every tracked VRAM buffer (ADR-051's rule), visible
  in play and off-screen while the hub is open.
- **Alternatives considered:** a module exit callback (`sceKernelRegisterExitCallback`, as
  `home/src/hw.c`) to stop the thread before reboot — more machinery and uncertain delivery to a
  kernel plugin; gating draws on the displayed buffer's region — tried, hid the overlay (above);
  `module_stop`'s terminate alone — a thread in a display syscall cannot be ended in time. Pacing on
  the thread manager matches the rest of the codebase and needs no new state.
- **Consequences:** the overlay shows in games and the quit reboots to the XMB cleanly. Device test
  **PF-FREEZE** (unchanged target): with the overlay on, HOME opens in several games, the FPS pill
  shows in the game (not the hub), and quitting to the XMB powers up the XMB with no black
  screen/power-off. **INFERRED until that device round confirms it.**

## ADR-054 — Home's player: the keys move through what the screen shows

- **Status:** Superseded by [ADR-057](#adr-057--the-players-keys-are-fixed-each-button-shows-its-key)
  (2026-10-29). Accepted 2026-10-27. Owner, after using the player on the PSP: the d-pad's keys
  "all seem to do the same", the interface and the controls are hard to understand and "work
  very badly".
- **Context (code):** since 2026-10-20 left / right sought on every music page (the track list
  too, where up / down choose a track) and up / down changed the track on Now Playing; the
  player drew five buttons (shuffle, previous, play, next, repeat) that no key could choose, and
  the track list's card drew three more that were only pictures. What the screen showed was not
  what the keys did.
- **Decision:** Now Playing has a focus, like every other screen of Home: the time bar or one of
  the five buttons. Up / down go between the bar and the buttons; on the bar left / right seek (a
  press 10 s, held 5 s steps); on the buttons left / right choose one; Enter presses the chosen
  one (on the bar: pause). The chosen button sits in a ring that glides between them, its name and
  state under the row ("Shuffle: On", "Repeat: All", "Next Track"), and the hints say what the
  keys do now. The player opens on Play / Pause. START pauses on both music pages; Square and
  Triangle stay shuffle and repeat on Now Playing. The track list: up / down choose, Enter plays,
  Triangle opens Now Playing, Square stops; left / right do nothing there (Classic: back, as in the
  XMB). The list's card shows a "Now Playing" button instead of controls it could not press. With
  nothing playing, Now Playing offers only Play (the first track) and Back. All three styles
  draw the same five buttons (`hm_player`, `home_view.c`).
- **Alternatives considered:** the PSP's own player (left / right the tracks, held a seek) -
  that is what the owner found unclear on 2026-10-20 (no way to seek seen); a control panel
  opened with SELECT (the PSP player's) - one more mode to learn. The focus shows itself.
- **Consequences:** previews `home-now-playing` (Next chosen), `home-now-no-cover` (the bar
  chosen), each style; PPSSPP (the GE path, 59 fps): the ring, the names and the seek (0:05 to
  0:17 with a press) - **VERIFIED in PPSSPP**, device test HM35.

## ADR-055 — Text centred by the font, lists cut at their edges, no writes on a key press

- **Status:** Accepted (2026-10-27). Owner: some text "higher, some lower, some not in the middle";
  the previews of every screen (`make -C tests preview`) showed rows of lists and grids drawn
  under the hints and over the header's line, a photo's name under its buttons, two-line rows
  with 2-3 px between the lines; `home.log` (2026-10-20..27): a 90-120 ms frame at every start, a
  1.9 s key press on a game's page, 95-116 ms frames while changing settings (55 fps, 100 late
  frames in 30 s).
- **Decision:**
  - **Centring:** a line of text in a pill, a row, a button or a key has its baseline at the
    box's middle plus half the font's capital height (`hm_mid`, `pw_font_cap`: H's top in the
    scaled font), not at offsets tuned by eye to the previews' font (PPSSPP's Liberation Sans;
    the PSP draws ltn0). The hints' button pictures sit on their words' middle the same way.
    Two-line rows centre the pair by the fonts. Modern, Classic and the shared overlays (toast,
    indicator, keyboard, editor, viewer, panel, sheet) use it.
  - **Clip:** `hm_gfx` gains `clip(y0, y1)`: until `clip(0, HM_H)` only those rows are drawn. The
    GE backend cuts its sprites on the CPU (`hm_sprite_clip_rect`, the texture coordinates moved
    with the edge, as at the screen's edges - no GE state); the CPU backend draws into a
    sub-canvas of those rows. Modern's lists, the settings' sidebar and the photo grids are cut
    above the hints and under the header; their edge rows fade by their words, so no text is half
    under the hints. Classic's option list ends above its hints.
  - **No card I/O on a key press:** the settings' files (`home.txt`, `config.txt`) are written
    800 ms after the last change (`hs_save_pending`), at once before USB, a game, sleep, power off
    and leaving; a game's page counts its states in one pass over SAVEDATA (`hm_states_count`)
    and leaves their pictures to the states' own page; PLUGINS.TXT and `perf.txt` are read at the
    start, behind the start-up animation, not by the first frame.
  - Also: the HOME panel darker behind it (Classic's band nearly opaque), Classic's panel shows
    the brightness as a bar, the settings' sidebar 158 px (the names were cut), the launcher's long
    game names on two lines, a photo's name above its buttons when they leave too little room.
- **Alternatives considered:** a GE scissor (`sceGuScissor`) for the clip - GE state to keep in
  step with the CPU renderer's previews, which would need their own clip anyway; drawing fewer
  rows - a list that scrolls must show its edge rows going. A writer thread for the files - a
  second thread on the Memory Stick for a write the user makes a few times a minute.
- **Consequences:** host tests 146 (`sprites_clip_to_rows_cuts_top_and_bottom`); previews of every
  screen; PPSSPP (GE): settings, music, the player at 59-60 fps. A change of a setting made just
  before the battery is pulled is lost (the window is under a second). **INFERRED** until the
  device shows it: the start's first frame and the settings' frames under 100 ms (HM36, `home.log`
  `slow:`). `home.log` names the PSP font's capitals and the advance of U and l (the previews'
  Liberation Sans has a wide U and l, PPSSPP's file - whether ltn0 does is RESEARCH REQUIRED, the
  next log's `home font:` line).

## ADR-056 — The system's pictures in Home: the kernel reads what the user open is refused

- **Status:** Accepted (2026-10-27), RESEARCH REQUIRED until the device's `home.log` shows it.
- **Context (VERIFIED by `home.log`, every start since 2026-10-20):** Home's `sceIoOpen` of
  `flash0:/vsh/resource/topmenu_icon.rco` and `01-12_03g.bmp` returns 0x8001000D (EACCES) while
  `system_plugin_fg.rco` opens, and the game plugin reads all three in kernel mode
  (`plugin/src/gfx/assets.c`); so Classic in Home had 5 of its pictures, none of the XMB's
  category icons and no sky. Why the user open is refused is not known (ARK-4's homebrew runlevel
  loads no VSH plugin - `core/systemctrl/src/plugin.c` - so not a theme plugin; ARK-4's
  systemctrl hooks no flash0 open - RESEARCH REQUIRED).
- **Decision:** when the user open fails with anything but ENOENT, Home opens, sizes and reads the
  file through `IoFileMgrForKernel` with KUBridge's `kuKernelCall` (`hm_hw_kernel_size` /
  `hm_hw_kernel_read`, `hw.c`), as it already reads the syscon and sets the clock; read only,
  flash0 only. `home art:` names each read and whether the kernel took it (`kernel ok` /
  `kernel refused`).
- **Alternatives considered:** copying the files to the Memory Stick (Sony's files on the card,
  a copy per firmware); asking the game plugin for them (it is not loaded in Home).
- **Consequences:** device test HM37: Classic's category icons and the month's sky in Home.

## ADR-057 — The player's keys are fixed; each button shows its key

- **Status:** Accepted (2026-10-29). Supersedes the keys of ADR-054.
- **Context (owner, 2026-10-29, the player on the PSP):** "the up arrow and right arrow have the
  same function, the down arrow and left too". **Cause (code):** ADR-054's focus. Up / down moved
  it between the time bar and the buttons, and left / right sought on one and chose a button on
  the other. So what a key did depended on an earlier key, and two keys could do the same thing.
  Also, the d-pad's auto-repeat (every 100 ms after 400 ms) reached the seek, so a held left /
  right sought 10 s every 100 ms instead of the planned 5 s steps.
- **Decision:** Now Playing has no focus. Each key does one thing, always:

  | Key | Does |
  |---|---|
  | ◄ / ► | seek 10 s back / on per press; held, 5 s every 220 ms after 450 ms |
  | ▲ / ▼ | the track before / after (as ▲ / ▼ move in the track list); ▲ more than 3 s in: this track from its start |
  | Enter | play / pause (nothing playing: the first track) |
  | □ / △ | shuffle on / off; repeat off / all / one |
  | START | play / pause (both music pages) |

  Only presses count for the d-pad on this page (its repeats would also skip a track every
  100 ms). Under the five buttons there is a row of their keys (□ ▲ Enter ▼ △, 16 px; at 12 px
  Square read as Circle and the arrows as dots). The button a key works gets a ring for 0.8 s, and
  what it did ("Next Track", "Repeat: All") replaces the key row meanwhile. The time bar shows its
  knob while left / right seek. The hints: ◄► Seek, ▲▼ Track, Enter Play / Pause.
- **Alternatives considered:** keeping ADR-054's focus and drawing it more clearly. The owner
  found two models of focus unclear in a row (2026-10-20, 2026-10-27). The PSP's own player
  (◄ / ► the tracks, held a seek): seeking hidden behind a hold was what the owner could not find
  on 2026-10-20.
- **Consequences:** `main.c now_keys`, `home_view.c hm_player`, new icons `HM_ICON_UP` /
  `HM_ICON_DOWN`; the strings `hm_choose` and `hm_controls` dropped. Previews
  `home-now-playing` (▼ just pressed), `home-now-no-cover` (seeking, the key row) in each style.
  Device test HM38 replaces HM35.

## ADR-058 — The player seeks with the MP3 library's own call, on one handle

- **Status:** Accepted (2026-10-29), [issue #5](https://github.com/semiloker/pocketwatch/issues/5).
  Supersedes the seeking of
  [ADR-036](#adr-036--home-does-its-own-saved-data-music-player-and-photo-albums-and-editor).
- **Context (owner, 2026-10-29, the player on the PSP):** left / right "shows +10 s, then goes back
  to 0:00". **Cause:** VERIFIED from the owner's card and the code: `home.txt` has
  `repeat = one`, and the time shown is the decoded samples, which only a track's start sets to 0.
  So after the seek the track ended and repeat one started it again (with repeat all it would have
  been the next track). ADR-036's seek reserved a **second** decoder handle at the frame's byte
  while the first was still reserved, initialised it, released the first and decoded with the
  second; the time showed +10 s (the second handle was made), then the next decode returned
  nothing, which `play()` takes for the end of the file. Why the PSP's library gives no sound from
  that second handle is not known (PPSSPP keeps the two handles apart and plays it); seeking had
  never been run on the device before (HM24 pending).
- **Decision:** seek with `sceMp3ResetPlayPositionByFrame(handle, frame)` on the one handle
  (frame = the time x the rate / the samples a frame: 1152 for MPEG-1, 576 for MPEG-2 and 2.5,
  from `sceMp3GetMaxOutputSample`). The library finds the frame's byte from the first frame's bit
  rate and asks for the stream again from there (VERIFIED as a model only: PPSSPP's
  `sceMp3ResetPlayPositionByFrame`, rebuilt from the real library's behaviour; the export is in
  PSPSDK's `pspmp3.h`). A decode that gives nothing in the first four tries after a seek is not
  the file's end: the player goes back once to the frame before the seek and logs it, so a seek
  that fails costs the seek, never the track. `home.log` records the first seek of each track
  (`home music: seek to N s, frame F of T (S a frame): 0`) and every refusal or failure. A track's
  length uses the same samples a frame (an MPEG-2 file showed twice its length, INFERRED from the
  formula).
- **Alternatives considered:** the second handle released first, then reopened at the byte (one
  handle at a time, but the same mid-file initialisation, whose failure is not understood); a
  hand-written decoder (backlog S7). ADR-036 had rejected the library's call because "its
  behaviour with the streamed input is not documented"; the device has now shown the reopening
  fails, and PPSSPP's model shows the call re-asks for the stream from the new position.
- **Consequences:** `home/src/music.c` (`seek_to`, `play`); the byte search after a seek
  (`hm_mp3_find_frame`, `hm_mp3_frame_len`) and its host test are gone. On a variable bit rate
  file the time after a seek is approximate (the library's own estimate). Device test HM38 with
  `home.log`: a seek line ending in `0`, no "no sound after the seek" line, and the track going on
  from the new time with repeat one.

## ADR-059 — The brightness is a vertical bar at the left; with Everywhere the overlay module shows it in the XMB

- **Status:** Accepted (2026-10-29), [issue #7](https://github.com/semiloker/pocketwatch/issues/7);
  its step counter superseded by
  [ADR-061](#adr-061--the-brightness-bars-follow-the-backlights-level-the-in-game-menu-has-its-own).
  Supersedes, for the brightness bar only and only while the owner turns it on, the "no VSH line"
  of [ADR-049](#adr-049--the-overlay-module-runs-only-in-a-real-game-never-in-the-xmb-or-home).
- **Context (owner, 2026-10-29):** "in the XMB and in games, add the same brightness bar, but on
  the left and vertical". In games the module showed the brightness in the volume's box at the
  bottom middle; in the XMB nothing shows a brightness change (ADR-047's owner report too).
  ADR-049 took the module out of the XMB after it crashed Home's start during the boot's handoff
  from the XMB to Home. Since then ADR-053 found why the module's thread outlived its module at a
  reboot: it was blocked in the display driver (`sceDisplayWaitVblankStart`) while the driver was
  torn down; it now paces on `sceKernelDelayThread` (INFERRED: the same cause as ADR-049's crash,
  which also came at a reboot - the handoff to Home - with the thread then in the same call).
  ARK-4 loads a `VSH` line only when the apitype is 0x200 or more (VERIFIED: ARK-4
  `core/systemctrl/src/plugin.c`, `isVshRunlevel`); Home is ARK-4's launcher, a homebrew (0x141),
  so a VSH line never loads into Home.
- **Decision:**
  - **One bar, three places:** the brightness is a vertical pill at the left - its step ("3/5") on
    top, the steps lit from the bottom, a sun under them - in Home (the GE, Home's accent), in
    games and in the XMB (the module: black box, white edge, white and grey steps, our own 13 px
    sun). The volume keeps its pill at the bottom middle (the XMB and the hub have their own
    volume bar, so the module draws none there).
  - **Everywhere:** Settings → Sound and Display → Volume and Brightness gets a fourth choice,
    Everywhere (`perf.txt osd = everywhere`): Home and games as Home and Games, plus the XMB. Only
    then does Home turn on the module's line `VSH, ms0:/seplugins/pocketwatch/pocketwatch_perf.prx`
    (and off for any other choice; ADR-047's old line goes off as before).
  - **In the XMB the module does the least:** its thread first sleeps 5 s in
    `sceKernelDelayThread` (not even `perf.txt` is read before), so it is asleep through the
    boot's handoff to Home and `module_stop` ends it at once; then it reads `perf.txt` and ends
    unless the indicator is Everywhere; it never shows the numbers, never runs the L + R + SELECT
    combo and never sets the backlight (no boost, no Own Steps: the XMB's brightness stays the
    system's); it writes only the bar, into the XMB's own VRAM buffers (ADR-051).
- **Alternatives considered:** the bar in pocketwatch_xmb.prx (the XMB plugin; 295 bytes left of
  its 64 KB, RISK-022); a new VSH module of its own (the same framebuffer writes and boot path as
  this one, and a third PLUGINS.TXT line); the bar in games only (the owner asked for the XMB
  too).
- **Consequences:** `perf/src/main.c` (`g_xmb`, `XMB_GRACE_MS`), `perf/src/overlay.c`
  (`pw_osd_draw_vbar`, `pw_osd_text` gives "3/5"), `home/src/home_view.c` (`osd_bright`),
  `home/src/hsettings.c` (the choice, the VSH line), the string `hs_osd_every`. The module's 64-bit
  division went (its helper was 1.3 KB; the CPU share now divides a second's microseconds in
  32 bits), so it is 14.9 KB with the bar. RISK-031 is back in the boot path while Everywhere is
  on: device test PF5 only after PF-FREEZE passes; recovery: hold START at power-on, then any other
  choice in Home's Settings (or the line `off` over USB). Previews `home-osd` (the bar),
  `home-osd-volume`.

## ADR-060 — The photo editor is laid out as a professional editor

- **Status:** Accepted (2026-10-29), [issue #8](https://github.com/semiloker/pocketwatch/issues/8);
  its step and scales superseded by
  [ADR-062](#adr-062--the-editors-steps-are-ones-the-eye-sees-and-a-step-only-recolours).
  Supersedes the editor of
  [ADR-036](#adr-036--home-does-its-own-saved-data-music-player-and-photo-albums-and-editor).
- **Context (owner, 2026-10-29):** "rework the full photo edit to a real professional style".
  ADR-036's editor was one row of nine icons over the photo filling the screen, the chosen tool's
  name and a value from -5 to 5, and a line of key hints: no view of the picture apart from the
  controls, no way to compare, a filter all or nothing, five steps per slider.
- **Decision:** the layout of the mobile editors people know (Lightroom, Snapseed), at 480 x 272:
  - **Top bar:** Cancel (Back), "Edit Photo", Save a Copy (an accent pill with the Enter glyph;
    START too).
  - **Canvas:** the picture fitted into 480 x 150 under the bar on the editor's dark background,
    a brightness histogram at its top right; hold Square to see the picture as it is ("Original").
  - **Readout and slider:** the chosen tool's name and value ("+20") over a ruler with a tick
    every tenth, the span from zero (the middle; the left end for one-sided tools) in the accent;
    a choice (rotate, mirror, aspect) shows a dot per choice instead.
  - **Three tabs** under it, L / R between them, each with its strip (left / right):
    **Adjust** - Exposure (0.5x to 2x), Contrast, Highlights and Shadows (the bright or the dark
    half of the tone curve only), Saturation, Vibrance (most for dull colours), Warmth, Tint,
    Vignette, Sharpen (an unsharp mask of the four neighbours), -100..100 in steps of 5 (vignette
    and sharpen 0..100), a dot under each changed tool; **Filters** - a thumbnail of each of the
    eight looks, taken as soon as it is chosen, up / down its strength 0..100 (a look is a set of
    Adjust values, a grey amount, a sepia tone and lifted blacks, scaled by the strength);
    **Crop** - Rotate (quarter turns), Mirror, Aspect Ratio (Original, Square, 4:3, 3:2, 16:9,
    centred, in the picture's orientation).
  - Triangle resets the chosen tool; Back with changes asks for a second press within 2.5 s.
  - The screen behind the controls (the canvas, the thumbnails) is drawn by the portable
    `hm_edit_compose` into the background both renderers use; the controls are `hm_gfx` drawing
    (`hm_draw_editor`), so the GE and the previews show the same. Integer only, a table per change,
    no division per pixel; a change redraws the canvas (the picture scaled to the canvas first,
    bilinear, then its colours), Save a Copy applies the same to the picture itself (at most
    1600 px on its longest side, turned, cropped).
- **Alternatives considered:** keeping the full-screen photo with the controls over it (the
  controls then cover a third of the picture, and a canvas is what the reference editors show); a
  curves editor (no room for its handles on 272 px, and the PSP has no touch); per-channel
  histograms (three overlapping charts at 64 x 22 px read as noise).
- **Consequences:** `home/src/photo_edit.c` (the model, the colours, crop, sharpen, histogram,
  `hm_edit_compose`, `hm_edit_fit`), `home/src/home_view.c` (`hm_draw_editor`), `home/src/main.c`
  (the editor's state and keys, the crop in Save a Copy), eight new icons, sixteen strings (the
  old "Filter" and key hint dropped). Previews `home-editor`, `home-editor-compare`,
  `home-editor-filters`, `home-editor-crop` in each style. HOM-REQ-040; device test HM39. The
  speed of a change on the PSP is not measured yet (HM39 asks).

## ADR-061 — The brightness bars follow the backlight's level; the in-game menu has its own

- **Status:** Accepted (2026-10-29), [#6](https://github.com/semiloker/pocketwatch/issues/6),
  [#9](https://github.com/semiloker/pocketwatch/issues/9). Supersedes the step counter ("3/5")
  of [ADR-059](#adr-059--the-brightness-is-a-vertical-bar-at-the-left-with-everywhere-the-overlay-module-shows-it-in-the-xmb).
  What the bars show (the level as a number and a fill) superseded by [ADR-063](#adr-063--the-brightness-bars-fill-by-the-screen-step-home-draws-the-in-game-menus-bar); the
  level stays their source.
- **Context (owner, 2026-10-29, HM34 / PF3 on the PSP):** the bar climbed to 4/5 and stopped; the
  next SCREEN press made the screen brighter with the bar still at 4/5, the press after showed 1/5.
  So impose's brightness step reads the same at the PSP-3000's fourth and fifth levels (INFERRED
  from the report; the fifth exists because ARK-4 lifts `scePowerGetBacklightMaximum` to 4). And:
  "I asked for it on the left and vertical in the XMB menu when a game is launched; in the menu it
  needs to look more like the sound volume bar" - the in-game menu (the hub), which the overlay
  module never draws over (ADR-051), had no brightness bar at all.
- **Decision:**
  - **The level, not the step:** every brightness bar shows the backlight's level 0-100
    (`sceDisplayGetBrightness`, which changes at each step, the fifth too) as a number ("72", as
    the volume shows "18") and a fill: Home a continuous bar in the vertical pill (the volume
    pill stood up), the overlay module 20 steps of 5 %. The screen going off (level 0) is no
    change. With the boost or Own Steps the level shown is the one set.
  - **The in-game menu:** its volume bar (`pw_volume`, `pw_view_volume` in each style) gets a
    scale (`max`): the brightness bar is the same object on a scale of 100, fed from the
    backlight every frame, drawn upright at the left with a sun - Classic the strip of steps (20),
    Modern the pill mirrored to the left edge, Minimal an upright line and the number.
- **Alternatives considered:** learning the five levels' values as they appear (the first press
  after a start would show nothing sure); a separate drawer for the brightness in the hub (the
  game plugin's debug build has 1.8 KB left; sharing the volume bar's code cost 1.3 KB).
- **Consequences:** `ui/volume.{c,h}` (`max`, `PW_BRIGHT_MAX`), `ui/ui.c` (`bright_tick`),
  `ui/views*.c`, `perf/src/overlay.c` (`pw_osd_text` gives the level), Home's `osd_bright`.
  Previews `hub-bright` (each style), `home-osd`. The game plugin's debug build now has 588
  bytes of kernel RAM left (RISK-005). Device tests H19 (the menu), HM34, PF3.

## ADR-062 — The editor's steps are ones the eye sees, and a step only recolours

- **Status:** Accepted (2026-10-29), [#10](https://github.com/semiloker/pocketwatch/issues/10).
  Supersedes the step of 5 and the scales of
  [ADR-060](#adr-060--the-photo-editor-is-laid-out-as-a-professional-editor).
- **Context (owner, HM39 on the PSP):** the Adjust tools - light, colour, vignette, sharpen - did
  not visibly change the photo; Filters and Crop did. `home.log`: 83-114 ms per key in the editor.
  **Cause (VERIFIED on the host):** the edit reaches the picture (an experiment: vignette 40
  darkens the canvas' corner from `A05A78` to `65384B`), but a press of 5 on ADR-060's scales
  moved a mid tone by one to four levels in 255 (exposure +5 about 3 %, highlights +5 about one
  level, vignette +5 7/256 at the corner), so a few presses showed nothing, each 85 ms late;
  Filters and Crop are whole changes. And every step remade the canvas' picture: the full
  480 x 272 turned, cropped and scaled again before its colours.
- **Decision:** steps of 10; scales on which a press is a step one sees and 100 a strong edit:
  exposure in stops (up to 2.5 brighter with a soft shoulder that keeps white white, 1.7 darker),
  contrast a slope of 1 + 4 % a point up, highlights and shadows up to 128 levels over their
  whole half, saturation 3 % a point, vibrance twice ADR-060's, warmth 70 and tint 60 levels at
  100, the vignette to black corners, sharpening 1.5x the edge at 100. The looks re-tuned to the
  new scales. The canvas' picture (`hm_edit_frame`: turned, cropped, fitted) is kept between
  steps and remade only when the Crop tab changes; a step copies it into the screen buffer and
  recolours it there (`hm_edit_compose`), Compare uses one made once at the start.
- **Alternatives considered:** acceleration while a key is held (keeps fine steps but the first
  presses still show nothing); a smaller preview (the canvas is already 1.8x smaller than the
  screen picture).
- **Consequences:** `photo_edit.c` (`make_tables`, `hm_edit_frame`, `hm_edit_compose`), `main.c`
  (`ed_frame`, `ed_orig`, `ed_geom`); host test `photo_edit_one_press_is_a_step_one_sees`. HM39
  again: each press changes the photo, and the editor's `slow:` entries in `home.log` should be
  gone (RESEARCH REQUIRED: the step's time on the PSP).

## ADR-063 — The brightness bars fill by the SCREEN step; Home draws the in-game menu's bar

- **Status:** Accepted (2026-10-29), [#11](https://github.com/semiloker/pocketwatch/issues/11).
  Supersedes what the bars show in [ADR-061](#adr-061--the-brightness-bars-follow-the-backlights-level-the-in-game-menu-has-its-own) (the level as a number and a fill); the
  level stays their source, never impose's step.
- **Context (owner, 2026-10-29, after round 12's build):** "I really love how the brightness bar
  looks in the XMB menu when a game is launched [the in-game menu; the owner's style is Modern],
  do the same in the default menu [Pocketwatch Home, the PSP's start screen: `start = home`]. But
  the fill of the bar should be its size / the number of brightness steps, because 1/5 is a 50 %
  fill." ADR-061 filled every bar by the level 0-100. The PSP-3000's firmware levels are 44 60
  72 84 (BetterBright's README, the "OEM" levels of the 3000/Go,
  <https://github.com/hobbo91/BetterBright>; INFERRED for this PSP: the owner's "1/5 is 50 %"
  fits 44), and ARK-4's fifth step is brighter than 84 (#6). Home drew its own bar: a wider
  black pill, the level on top, an accent fill.
- **Decision:**
  - **The step, k of n:** a level is the step it is nearest to (`pw_backlight_step` in
    `ui/volume.h`: 1 below 52, 2 below 66, 3 below 78, 4 up to 84, 5 above), n = 5; with Own
    Steps the nearest of `perf.txt`'s levels, n their count (`pw_osd_bright`). Every bar fills
    k/n: the in-game menu (`pw_volume` on a scale of `PW_BRIGHT_STEPS`; Classic's 20 steps, four
    to each), Home, the overlay module (n steps, k lit; games, and the XMB with Everywhere).
    Where a number shows (Minimal, the module's box) it is the step, "3".
  - **Home draws the in-game menu's bar** in each style (`osd_bright` in `home_view.c`, the sizes
    and colours of `pw_view_volume` upright): Classic the dark strip of 20 steps, the one just
    reached glowing; Modern the slim pill that swells at a change; Minimal a line and the step's
    number. The fill glides to the step (`pw_ease`, as the menu's).
  - **The table is checked on the PSP:** Home writes the levels it saw and the step each was
    given with its stats line (`home brightness: level=step 44=1/5 ...`), no card write at the
    press.
- **Alternatives considered:** impose's step for the first four and the level only for the
  fifth (two readings for what the table gives alone); learning the levels as they appear
  (ADR-061: the first press after a start would show nothing sure); the in-game menu reading
  `perf.txt` for Own Steps (kernel RAM: 532 bytes left in the debug build) - there the bar shows
  the nearest of the system's five (OPEN QUESTION: worth it only if the owner turns Own Steps on).
- **Consequences:** `ui/volume.h` (`PW_BRIGHT_STEPS` moved here from `overlay.h`,
  `pw_backlight_step`), `ui/ui.c`, `ui/views*.c` (the brightness is `max > 0`),
  `perf/src/overlay.{c,h}` (`pw_osd_bright`; `pw_osd`'s `bright` is the step), `perf/src/main.c`,
  Home's `main.c` (`av_frame`, `seen_level`) and `home_view.c`. The game plugin's debug build has
  532 bytes of kernel RAM left (RISK-005). Host tests
  `brightness_step_is_the_nearest_of_the_firmware_levels`,
  `brightness_bar_step_is_the_systems_or_pocketwatchs`; previews `home-osd` and `hub-bright` in
  each style. Device tests HM34, PF3, H19 again.
