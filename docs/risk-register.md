# Risk register

Likelihood and impact: **H**igh / **M**edium / **L**ow (qualitative, with the reason). Status:
*Open*, *Mitigated* (controls in place, residual risk noted), *Closed*. "Owner" is the role that
acts next: **Dev** (implementation sessions) or **Owner** (device tests, decisions). Never delete a
risk; close it with the evidence.

| ID | Risk | L | I | Status |
|---|---|---|---|---|
| [RISK-001](#risk-001--memory-stick-fat-corruption-after-a-rollback) | Memory Stick FAT corruption after a rollback | M | H | Open |
| [RISK-002](#risk-002--flash1-corruption-after-a-rollback) | flash1 corruption after a rollback | L | H | Mitigated |
| [RISK-003](#risk-003--hang-on-quit-after-a-touch-and-go-suspend) | Hang on quit after a touch-and-go suspend | L | H | Mitigated |
| [RISK-004](#risk-004--game-specific-incompatibility) | Game-specific incompatibility | H | M | Open |
| [RISK-005](#risk-005--kernel-ram-and-prx-size-growth) | Kernel RAM / PRX size growth | M | M | Open |
| [RISK-006](#risk-006--cross-boot-load-into-a-different-environment) | Cross-boot load into a different environment | H | H | Open |
| [RISK-007](#risk-007--other-plugins-and-ark-settings) | Other plugins and ARK settings | M | M | Open |
| [RISK-008](#risk-008--ark-4-api-or-nid-table-changes) | ARK-4 API or NID table changes | L | M | Mitigated |
| [RISK-009](#risk-009--home-takeover-side-effects) | HOME takeover side effects | M | M | Open |
| [RISK-010](#risk-010--parsing-sony-assets-on-the-device) | Parsing Sony assets on the device | M | M | Open |
| [RISK-011](#risk-011--power-loss-or-card-removal-during-a-state-write) | Power loss / card removal during a write | L | M | Open |
| [RISK-012](#risk-012--bricking-during-firmware-research) | Bricking during firmware research | L | H | Open |
| [RISK-013](#risk-013--licensing-and-redistribution) | Licensing and redistribution | L | H | Mitigated |
| [RISK-014](#risk-014--intermittent-device-behaviour) | Intermittent device behaviour | M | M | Open |
| [RISK-015](#risk-015--sleep-or-usb-while-the-menu-is-open) | Sleep or USB while the menu is open | M | M | Open |

## RISK-001 — Memory Stick FAT corruption after a rollback

- **Description:** A load restores the save-time kernel, including the FAT driver's in-memory
  state. If anything wrote to the card between save and load (a game save, a log, a state file, a
  USB copy, another boot), the restored driver may write stale FAT/directory sectors back.
- **Likelihood L:** T1 (2026-10-16, VERIFIED for a real-sleep save) showed the driver sees files
  and allocations made after the capture; touch-and-go saves are not covered yet (T1b).
- **Impact H:** loss of the user's saves and files.
- **Mitigation:** after every load from a slot the restored world checks that it sees the
  marker file that save created (a new name each time) before anything writes to the card; on
  failure card writes stop until reboot and the hub says "Do not save in the game: quit and restart
  the PSP" ([ADR-017](architecture/decisions.md#adr-017--state-files-are-written-after-the-capture)). ARK's `mscache` window is dropped before the check. E4: 100
  cycles with a PC FAT check. Owner: **Owner** (T1b, E4 runs with a card backup).
- **Links:** [m3-menu-e3 phase 2](research/m3-menu-e3.md), [states storage](subsystems/states-storage.md).

## RISK-002 — flash1 corruption after a rollback

- **Description:** Stale lflash map after a rollback corrupts flash1; the PSP resets its settings.
- **Status Mitigated:** flash driver rebuild after every resume ([ADR-006](architecture/decisions.md#adr-006--rebuild-the-flash-drivers-after-every-resume)), load refused without it. Residual: backlog S3 (skipping the rebuild) must never ship without a proof.
- **Evidence:** E2 run 5 (incident), runs 6-7 (fixed).

## RISK-003 — Hang on quit after a touch-and-go suspend

- **Description:** Touch-and-go zeroes the IPL seeds; the next reboot cannot decrypt reboot.bin.
- **Status Mitigated:** seeds saved before every suspend and restored at `0x10000`; fast mode
  disabled when no seeds were seen ([ADR-008](architecture/decisions.md#adr-008--touch-and-go-suspend-by-default)). Evidence: speed run 7.

## RISK-004 — Game-specific incompatibility

- **Description:** Games using P5 (volatile), highmem, video playback during the freeze, interrupt
  handlers writing game RAM, Inferno's cache in P9, or special hardware (camera, GPS, network).
- **Likelihood H:** only two games tested (NFS Most Wanted, Castlevania DXC).
- **Mitigation:** refusal checks (P2 size, P5 use); compatibility matrix on the owner's 22 ISOs in
  M6 ([roadmap](roadmap.md)). Owner: **Owner** (tests), **Dev** (refusals).

## RISK-005 — Kernel RAM and PRX size growth

- **Description:** P1 has ~300 KB free in games and is shared with other plugins; the UI (fonts,
  icons, text) and E3 grow the module.
- **Mitigation:** size budgets in `make check` (debug 112 KB, release must stay small); every large
  buffer in P9/P11, allocated on menu open; assets cached in P11 only while the menu is open.

## RISK-006 — Cross-boot load into a different environment

- **Description:** Loading a state saved under another plugin set, ISO, ARK config, firmware or
  Pocketwatch build restores a kernel that does not match the hardware/software → crash or data
  damage.
- **Mitigation (PROPOSED, E3):** environment fingerprint checked before any freeze; save-time
  addresses in the header; hardware-describing data (seeds, live switches, resume info) always
  taken from the current boot. Owner: **Dev**.

## RISK-007 — Other plugins and ARK settings

- **Description:** RemoteJoyLite holds USB and disturbs suspends; PSPStates/Custom Home would run
  a second engine; ARK `mscache`/`infernocache`/`highmem` change memory and caching.
- **Mitigation:** detect and refuse (guards, M3); document required settings.

## RISK-008 — ARK-4 API or NID table changes

- **Description:** A new ARK-4 release changes its resolver table or SystemCtrl API.
- **Status Mitigated:** `make check` against the vendored table, boot self-check logs unresolved
  imports; the ARK version goes into the E3 fingerprint.

## RISK-009 — HOME takeover side effects

- **Description:** The system HOME screen may still flash, a game that disabled HOME must keep it
  disabled, impose may re-enable the popup after a resume or a utility dialog, and a custom Quit
  Game may hang if the game's exit callback never returns.
- **Mitigation:** **RESEARCH REQUIRED** before implementation ([game shell](subsystems/game-shell.md)).

## RISK-010 — Parsing Sony assets on the device

- **Description:** PGF/RCO/GIM parsers read firmware files; a malformed or unexpected file (other
  firmware, language packs) could crash the kernel.
- **Mitigation:** bounds-checked parsers, host tests with synthetic files, fallback to the built-in
  8x8 font and plain shapes when an asset is missing ([rendering](ui/rendering.md)).

## RISK-011 — Power loss or card removal during a state write

- **Description:** A battery cut or card removal while writing a state.
- **Mitigation:** STATE.BIN (the commit record) is zeroed first and written last, so an
  interrupted write leaves an empty slot that is never loaded (host test
  `store_interrupted_save_leaves_an_empty_slot`); low-battery guard. Accepted: the slot's previous
  state is lost (keeping it would double the space, ADR-017); the overwrite confirmation says so.

## RISK-012 — Bricking during firmware research

- **Description:** NAND/IPL/SYSCON writes can make the PSP unbootable; later PSP-3000 boards have
  limited service-mode recovery (**RESEARCH REQUIRED**).
- **Mitigation:** gated research track ([ADR-015](architecture/decisions.md#adr-015--independent-firmware-is-a-gated-research-track)); no destructive experiment without an
  owner decision and a tested recovery path.

## RISK-013 — Licensing and redistribution

- **Description:** Copying unlicensed code (PSPStates) or CC BY-SA code (intraFont) into a
  GPL project, or shipping Sony assets.
- **Status Mitigated:** [ADR-001](architecture/decisions.md#adr-001--license-gpl-30-or-later), [ADR-013](architecture/decisions.md#adr-013--sony-assets-are-read-at-runtime-never-redistributed); SPDX headers on every file.

## RISK-014 — Intermittent device behaviour

- **Description:** One first touch-and-go save after a real-sleep save never completed (speed
  run 6), not reproduced since ([backlog](backlog.md#open-questions)).
- **Mitigation:** every save logs "save OK"; watch the debug log; breadcrumbs survive in P9.

## RISK-015 — Sleep or USB while the menu is open

- **Description:** The POWER switch, an auto-sleep or a USB connection while the menu holds the
  freeze, the display and the buttons.
- **Mitigation (PROPOSED):** a power-query handler closes the menu and thaws before the suspend
  proceeds; USB guard (M3).
