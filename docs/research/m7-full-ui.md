# M7 — how far can Pocketwatch replace the XMB's look and screens?

Question: what is the way to "a full UI of our own" (owner, 2026-10-18: "there is cxmb for
custom themes - study how to make our own full UI, and write a first modern one").
Status: Answered (2026-10-18): three routes, the launcher route chosen for the full UI
([ADR-025](../architecture/decisions.md#adr-025--a-full-ui-of-our-own-is-pocketwatch-home-a-homebrew-home-screen));
the first version is built, device test pending.

## Background and sources

**cxmb** (Custom XMB, by Poison, GPL-3.0-or-later; sources:
[ErikPshat/cxmb](https://github.com/ErikPshat/cxmb) - an export of the original
code.google.com/p/cxmb - and [PSP-Archive/CXMB](https://github.com/PSP-Archive/CXMB)).
VERIFIED from `main.c`: cxmb hooks the `lflash` (flash0) driver's IoOpen / IoRead / IoLseek /
IoIoctl / IoClose / IoGetstat. When the XMB opens a flash0 file whose name is in the selected
theme's header table (`ctf.h`: `CtfHeader { char name[64]; u32 start, size; }`), the open is
redirected to the theme file on the Memory Stick (`/PSP/THEME/*.ctf`) and reads and seeks are
confined to that file's `start..start+size`. A CTF theme is therefore a bundle of replacement
flash0 files - in practice the `vsh/resource/*.rco` resources (icons, the wave, backgrounds,
layout and animation parameters stored in the RCOs) and fonts. cxmb itself draws nothing.

What follows (INFERRED from the mechanism and the RCO format, VERIFIED for reading in
`gfx/rco.c`): a theme can change how the XMB looks - every picture, the colours, fonts, sounds and
whatever placement the RCOs define - but not what it does: the screens, the navigation and the
logic stay `vsh_module`'s, `paf`'s and the plugins'. Making a theme needs Sony's own RCO files
as the base (a CTF is built from them), which the project may not redistribute.

**ARK-4** (GPL-3.0-or-later) offers two more ways, both VERIFIED from its sources:
- a VSH plugin can add items and fill pages of the XMB - our XMB plugin already does
  (ADR-021, ADR-024, `vsh/`), and ARK's own VSH menu draws over the XMB;
- *launcher mode* (`SETTINGS.TXT`: `always, launcher, on`): systemctrl starts a homebrew
  menu (`ARK_01234/VBOOT.PBP`) instead of `vsh_module` (`core/systemctrl/src/loadercore.c`,
  `InitKernelStartModule`: `sctrlArkExitLauncher()`), and ARK's menu starts games itself
  (`extras/menus/arkMenu/src/iso.cpp`, `Iso::executeISO`: key "umdemu",
  `sctrlSESetDiscType(GAME)`, `sctrlSESetBootConfFileIndex(3)` (Inferno),
  `sctrlSESetUmdFile(image)`, `sctrlKernelLoadExecVSHWithApitype(0x123, image, param)` with
  `disc0:/PSP_GAME/SYSDIR/EBOOT.BIN`, or EBOOT.OLD for a patched image). These calls are in
  `SystemCtrlForUser`, so a user-mode application can make them.

## The three routes

| Route | What it can be | Limits |
|---|---|---|
| **Theme** (cxmb + a CTF) | the real XMB with our pictures, colours, fonts, icons | no new screens or behaviour; built on Sony's RCOs (a builder that changes the owner's own files on the PC would be needed; RESEARCH REQUIRED: writing RCOs) |
| **Pages in the XMB** (our VSH plugin) | our rows in the XMB's lists (done), or panels drawn over the XMB | the XMB stays the frame; drawing our own panels over it is RESEARCH REQUIRED |
| **Our own home screen** (a homebrew application; ARK can start it instead of the XMB) | everything: layout, motion, the three styles, the game library, our data | the XMB's own functions (music, video, network settings) stay in the XMB; a game's start goes through ARK's Inferno path (not the XMB's), see RISK-020 |

## Conclusions

- The full UI is the third route: **Pocketwatch Home** ([home](../subsystems/home.md)), started
  from Game → Memory Stick, later as ARK's launcher. First version (2026-10-18, Modern style):
  the games on the card with their ICON0 and PIC1, play time and save states, Play Time and
  System sections, starting a game the way ARK's menu does. **VERIFIED** on the PC (2026-10-18):
  the Home's disc reader and PNG decoder read all 22 images of the owner's card (21 ISO, 1 CSO:
  Metal Gear Solid Peace Walker) - every TITLE, DISC_ID, ICON0 (144x80) and PIC1 (480x272).
  Device test HM1-HM4 pending.
- The theme route stays open for the XMB's own look (a "Pocketwatch theme" builder), after Home.
- Next: device test; then the Classic and Minimal styles of Home, Home as ARK's launcher, a
  state list per game in Home, RCO icons from flash0 in Home.
