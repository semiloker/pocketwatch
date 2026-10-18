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
- **Since 2026-10-19:** USB, Settings (Pocketwatch's and the system's), System Information, a
  game's states (Continue) and its Saved Data in Home - every element of the XMB is accounted
  for in [m7-xmb-inventory](../research/m7-xmb-inventory.md).
- **Target state:** Classic and Minimal styles too; photos and music; PSN games.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `HOM-REQ-001` | Games: every `.iso` / `.cso` in `ms0:/ISO` and one folder level below, titled and identified from its own PARAM.SFO (TITLE, DISC_ID), sorted by title - or the last played first (Settings → Sort Games, `config.txt` `sort`); a file that is not a PSP image is listed by its name. PSN games' EBOOTs (CATEGORY `EG`) are listed with the PSP games and start as ARK-4's menu starts them. | P1 | Implemented | `disc_*` host tests, testing guide HM1 |
| `HOM-REQ-002` | Each game shows its ICON0 in the row and, once the choice rests for 350 ms, its PIC1 as the background (blurred, darkened), cross-faded; images load on a thread, nearest first, so the row never waits. | P1 | Implemented | previews `home-games*.png`, HM1 |
| `HOM-REQ-003` | Under the title: the disc ID, play time (PLT files) and the number of save states (`PW<disc ID><slot>` folders) - only what is known. | P1 | Implemented | HM1 |
| `HOM-REQ-004` | Enter starts the game the way ARK-4's own menu does (Inferno, apitype 0x123, EBOOT.OLD for a patched image); a failed start returns to Home. | P1 | Implemented | HM2 |
| `HOM-REQ-005` | Play Time: the total and every game with time, most first, with bars; System: battery, time left (the system's figure), free space, the number of games, the version, Exit to the XMB. L / R switch sections. | P1 | Implemented | previews `home-playtime.png`, `home-system.png`, HM3 |
| `HOM-REQ-006` | The three styles (Classic, Modern, Minimal) as in the hub, from config.txt. | P2 | Planned (Modern only) | - |
| `HOM-REQ-007` | The PSP can start in Home instead of the XMB: Settings → Pocketwatch → Start screen; ARK-4's menu and settings untouched; the XMB comes instead when L is held at power-on, when Home asked for it, or when Home's last start never reached its first screen ([m7-boot-shell](../research/m7-boot-shell.md), ADR-026). | P1 | Implemented 2026-10-18; run 1 gave the XMB (ADR-027 fixes both causes); HM5-HM9 pending | `boot_*` host tests, HM5-HM9 |
| `HOM-REQ-008` | Home lists every `ms0:/PSP/GAME/<folder>/EBOOT.PBP` that is homebrew (CATEGORY `MG`) or a PS1 game (`ME`) with its own icon (kept in shape) and PIC1, after the PSP games, and starts it as ARK-4's menu does (apitype 0x141 / 0x144). | P1 | Implemented; reader VERIFIED on the owner's 9 EBOOTs on the PC; starting: HM7 pending | `disc_finds_the_parts_of_an_eboot_pbp`, HM7 |
| `HOM-REQ-009` | Exit to the XMB and HOME → Quit in Home lead to the XMB even when Home is the start screen (marker `xmb-once`); after its first frame Home clears the start guard (`boot-pending`). | P1 | Implemented; HM6, HM8 pending | HM6, HM8 |
| `HOM-REQ-011` | USB Connection in Home (HOME panel and System): the storage drivers loaded once, the connection made at once and made again by itself when a cable is in but the computer has not taken the card after 3 s (no second press); the card is the computer's meanwhile (no file I/O by Home); afterwards the library is read again. | P1 | Implemented 2026-10-19; host test `usb_*`, checked in PPSSPP (its USB is simulated); HM12 pending | `usb_*`, HM12 |
| `HOM-REQ-012` | Settings section: Pocketwatch's settings (start screen, in-game style, fast save and load, clock and battery, language - `config.txt` / `language.txt`, as the XMB page) and the system's: nickname (the firmware keyboard), system language, date and time format, time zone, summer time, WLAN power save (`sceUtilitySetSystemParam*`), backlight brightness and AVLS (impose through KUBridge). Left / right change a value; every write is read back. | P1 | Implemented 2026-10-19; checked in PPSSPP (values read, the keyboard opens); HM13 pending | preview `home-settings.png`, HM13 |
| `HOM-REQ-013` | System section: battery (voltage, temperature, capacity, charge cycles), time left, free space, games, nickname, model, system software, ARK-4's version, MAC address, Pocketwatch's version, USB, Exit to the XMB. | P1 | Implemented 2026-10-19; HM13 pending | HM13 |
| `HOM-REQ-014` | Triangle on a game image → Saved Data: the firmware's own saved-data list of that game (folders that start with its disc ID), with details and delete. | P1 | Implemented 2026-10-19; HM14 pending | HM14 |
| `HOM-REQ-015` | Triangle on a game image: its save states (PW<disc ID><slot>) with their pictures and times, newest first; Enter continues the game from that state (continue.txt, M7 V1). | P1 | Implemented 2026-10-19; HM15 pending | preview `home-game-options.png`, HM15 |
| `HOM-REQ-016` | Photos: the pictures (PNG, JPEG, BMP) of `PICTURE/` and `DCIM/` and their folders, newest first, with thumbnails kept in shape; the chosen one blurred behind the row; Enter shows it whole, left / right go through them, Triangle starts or stops a slideshow (round again; Settings → Slideshow Speed: 5, 3 or 10 s a picture, `config.txt` `slideshow`), Square sets it as Home's wallpaper ([m7-xmb-inventory](../research/m7-xmb-inventory.md)). Pictures over 2.6 Mpixel or 6 MB are listed without a picture. | P2 | Implemented 2026-10-19; checked in PPSSPP with the owner's screenshots and made-up PNGs (slideshow, wallpaper); HM17 pending | HM17 |
| `HOM-REQ-017` | Network: the system's browser (HTML viewer) and connecting to an access point (netconf). | P3 | Planned | - |
| `HOM-REQ-018` | Music: the MP3s of `MUSIC/` and its folders ("Title - Artist.mp3" read as such), played by the firmware's decoder on a thread of its own - on while Home is browsed, the next track following; Enter plays or pauses, left / right skip, Square stops; what plays with its time and a bar. Stopped for USB and before a game. | P2 | Implemented 2026-10-19; checked in PPSSPP (silent test files: start, skip, the next track at the end); HM16 pending | `music_*` host tests, HM16 |
| `HOM-REQ-019` | Sounds: Home's own key sounds - a move, a section, open, back, a game starting, a refused setting - made at start from a few numbers each ([`home/src/sound_synth.c`](../../home/src/sound_synth.c): nothing of Sony's, no file), played on a thread of their own through one 44.1 kHz mono channel beside the music, a new one cutting the last short within 5.8 ms; Settings → Key Sounds (`sounds.txt`). | P3 | Implemented 2026-10-19; host test `home_sounds_*`; run in PPSSPP with its sound off (night): no stall, 60/60, the setting kept; heard: HM18 pending | `home_sounds_*`, HM18 |
| `HOM-REQ-020` | Triangle on any game → Information (as the XMB's): disc ID, version, the system software it needs, the file's size, play time, the file's name; Back returns to the game's options. Homebrew and PS1 games get Information only (their saves and states are not under a PSP disc ID). | P2 | Implemented 2026-10-19; checked in PPSSPP (homebrew); HM15 pending | HM15 |
| `HOM-REQ-021` | Wallpaper: a picture chosen in Photos (Square) is Home's background in every section but Games (where the chosen game's art is) and Photos, kept as `seplugins/pocketwatch/wallpaper.bin` (512 x 272, made once) and `wallpaper.txt` (its path, its name in Settings); Settings → Wallpaper → Pocketwatch goes back to Pocketwatch's own. The card is never read for it while the rows are built (USB). | P2 | Implemented 2026-10-19; checked in PPSSPP (set, kept after a restart, back to the default); HM17 pending | HM17 |
| `HOM-REQ-010` | HOME in Home opens Home's own panel (Close, Exit to the XMB, Sleep, Turn Off), never the system's quit dialog; HOME or Back closes it. | P1 | Implemented 2026-10-18 (ADR-028); HM11 pending | preview `home-panel.png`, HM11 |
| `HOM-NFR-001` | 60 frames a second on the PSP (drawn by the GE, ADR-028), no late frame when moving (stamps made ahead, ADR-029), motion by time (`ui/anim.h`); texts in the user's language; the frame rate and late frames in `home.log`. | P1 | Device, build `5811f00`: 45-49 fps, GE 11-13 ms a frame, one 2.7 s frame ([log](../research/logs/2026-10-19-home-fps-ge1.log)); fixed by ADR-029/030 (8-bit stamps, backgrounds in VRAM, free space off the loop); 60/60 in PPSSPP; HM10 again pending | `home.log`, HM10 |

## C. Architecture

A user-mode application (newlib, `build.mak`, BUILD_PRX) built from the plugin's portable code
(`ui/draw.c`, `ui/shapes.c`, `ui/icons.c`, `gfx/pgf.c`, `gfx/text.c`, `ui/i18n.c`,
`core/playtime.c`, `complications/states/sfo.c`) plus `home/src`. The screens draw through
`hm_gfx` (ADR-028): the GE on the PSP (`gfx_gu.c`, two 8888 frames in VRAM, textures in RAM, text
and corners as stamps rasterised once by the CPU renderer), the CPU renderer for the host
previews (`gfx_cpu.c`); the system font `flash0:/font/ltn0.pgf`; PNG and the CSO's raw
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
