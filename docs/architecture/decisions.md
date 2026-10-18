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
  the PSP and it is already there"). Extends ADR-025.
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
