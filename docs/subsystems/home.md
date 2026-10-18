# Pocketwatch Home — specification

| | |
|---|---|
| Area code | `HOM` |
| Status | First version implemented 2026-10-18 (Modern style; branch `feature/psp-xmb-overhaul`): builds, host-tested (disc reader, previews; **VERIFIED** on the owner's 22 images - 21 ISO, 1 CSO - with `home/src/disc.c` + `hm_image.c` on the PC: every title, disc ID, ICON0 144x80 and PIC1 480x272 read); device test HM1-HM4 pending |
| Code | [`home/`](../../home/): [`main.c`](../../home/src/main.c) (the loop), [`home_view.c`](../../home/src/home_view.c) (Modern drawing, portable), [`library.c`](../../home/src/library.c) (games, loader thread), [`disc.c`](../../home/src/disc.c) (ISO 9660 / CSO, portable), [`launch.c`](../../home/src/launch.c) (ARK-4's start), [`hw.c`](../../home/src/hw.c) |

## A. Overview

- **Purpose:** a home screen of Pocketwatch's own - the "full UI" ([m7-full-ui](../research/m7-full-ui.md),
  ADR-025): the games, what Pocketwatch knows about them, the battery and the card, drawn by
  Pocketwatch's renderer.
- **Current state:** a homebrew application (`ms0:/PSP/GAME/PocketwatchHome/EBOOT.PBP`, our own
  ICON0 and PIC1) with three sections - Games, Play Time, System - in the Modern style.
- **Since ADR-026:** the PSP can start in Home instead of the XMB (XMB → Settings → Pocketwatch
  → Start screen), and Home also lists the homebrew and PS1 games of `PSP/GAME`.
- **Target state:** Classic and Minimal styles too; UMD and PSN games; USB in Home; a game's save
  states and Continue from a state in Home.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `HOM-REQ-001` | Games: every `.iso` / `.cso` in `ms0:/ISO` and one folder level below, titled and identified from its own PARAM.SFO (TITLE, DISC_ID), sorted by title; a file that is not a PSP image is listed by its name. | P1 | Implemented | `disc_*` host tests, testing guide HM1 |
| `HOM-REQ-002` | Each game shows its ICON0 in the row and, once the choice rests for 350 ms, its PIC1 as the background (blurred, darkened), cross-faded; images load on a thread, nearest first, so the row never waits. | P1 | Implemented | previews `home-games*.png`, HM1 |
| `HOM-REQ-003` | Under the title: the disc ID, play time (PLT files) and the number of save states (`PW<disc ID><slot>` folders) - only what is known. | P1 | Implemented | HM1 |
| `HOM-REQ-004` | Enter starts the game the way ARK-4's own menu does (Inferno, apitype 0x123, EBOOT.OLD for a patched image); a failed start returns to Home. | P1 | Implemented | HM2 |
| `HOM-REQ-005` | Play Time: the total and every game with time, most first, with bars; System: battery, time left (the system's figure), free space, the number of games, the version, Exit to the XMB. L / R switch sections. | P1 | Implemented | previews `home-playtime.png`, `home-system.png`, HM3 |
| `HOM-REQ-006` | The three styles (Classic, Modern, Minimal) as in the hub, from config.txt. | P2 | Planned (Modern only) | - |
| `HOM-REQ-007` | The PSP can start in Home instead of the XMB: Settings → Pocketwatch → Start screen; ARK-4's menu and settings untouched; the XMB comes instead when L is held at power-on, when Home asked for it, or when Home's last start never reached its first screen ([m7-boot-shell](../research/m7-boot-shell.md), ADR-026). | P1 | Implemented 2026-10-18; device test HM5-HM9 pending | `boot_*` host tests, HM5-HM9 |
| `HOM-REQ-008` | Home lists every `ms0:/PSP/GAME/<folder>/EBOOT.PBP` that is homebrew (CATEGORY `MG`) or a PS1 game (`ME`) with its own icon (kept in shape) and PIC1, after the PSP games, and starts it as ARK-4's menu does (apitype 0x141 / 0x144). | P1 | Implemented; reader VERIFIED on the owner's 9 EBOOTs on the PC; starting: HM7 pending | `disc_finds_the_parts_of_an_eboot_pbp`, HM7 |
| `HOM-REQ-009` | Exit to the XMB and HOME → Quit in Home lead to the XMB even when Home is the start screen (marker `xmb-once`); after its first frame Home clears the start guard (`boot-pending`). | P1 | Implemented; HM6, HM8 pending | HM6, HM8 |
| `HOM-NFR-001` | Motion by time (`ui/anim.h`), 60 Hz while nothing loads; texts in the user's language. | P1 | Implemented (device fps not measured) | HM1 |

## C. Architecture

A user-mode application (newlib, `build.mak`, BUILD_PRX) built from the plugin's portable code
(`ui/draw.c`, `ui/shapes.c`, `ui/icons.c`, `gfx/pgf.c`, `gfx/text.c`, `ui/i18n.c`,
`core/playtime.c`, `complications/states/sfo.c`) plus `home/src`. Two 8888 frames in RAM shown in
turn (`sceDisplaySetFrameBuf`); the system font `flash0:/font/ltn0.pgf`; PNG and the CSO's raw
deflate through the vendored `stb_image.h` (public domain / MIT, `home/lib/`). Translations:
`seplugins/pocketwatch/lang/` as the plugins use them. Launch: `SystemCtrlForUser`
(`home/src/stubs.S`).

## D. User experience

Modern ([styles](../ui/styles.md)): tabs at the top left with a gliding pill, the clock and
battery pill at the top right; the chosen icon large with a focus ring, the next ones smaller to
its right; the title, the meta line and a white Play pill below; hints at the bottom right.

## E. Reference material

| Reference | What | Use | License |
|---|---|---|---|
| ARK-4 `extras/menus/arkMenu/src/iso.cpp` | how to start an ISO | adopt (the calls) | GPL-3.0 |
| cxmb | what a theme can change | the research note | GPL-3.0 |
| stb_image 2.30 | PNG, inflate | vendored | public domain / MIT |

## F. Implementation plan

1. ~~First version, Modern~~ (2026-10-18).
2. Device test HM1-HM4.
3. Classic and Minimal; the game's states and Continue from a state; launcher mode.

## G. Testing

Host: `make -C tests run T=disc`, previews `home-*.png`. Device: testing guide HM1-HM4.

## H. Risks and decisions

[ADR-025](../architecture/decisions.md#adr-025--a-full-ui-of-our-own-is-pocketwatch-home-a-homebrew-home-screen);
[RISK-020](../risk-register.md#risk-020--a-game-started-from-home-is-in-another-setup-than-from-the-xmb).
