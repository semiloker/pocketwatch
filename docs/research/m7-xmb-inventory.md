# M7 — every element of the PSP's XMB, and its place in Pocketwatch Home

Question: what does the XMB of system software 6.61 offer, element by element, and which of those
can Pocketwatch Home (ADR-025, the start screen since ADR-026) take over - with which firmware
interface, at what risk (owner, 2026-10-19: "study every element of the main menu so each one can
move to the new interface")?
Status: Answered (2026-10-19) as a plan; implementation status per row below.

## Background and sources

- The XMB's structure: the owner's PSP-3000 on 6.61 with ARK-4; our XMB plugin's own work on
  `vsh_module` and `sysconf_plugin` (ADR-021, ADR-024, `vsh/src/xmb.c`); the PSP's manual
  structure as Sony documents it (INFERRED where a row says so: from the manual's menu tree and
  common knowledge of 6.xx, not from a dump of this PSP's menus).
- Firmware interfaces, VERIFIED in PSPSDK headers (`psputility_sysparam.h`, `psputility_savedata.h`,
  `psputility_osk.h`, `psputility_netconf.h`, `psputility_htmlviewer.h`, `pspimpose_driver.h`,
  `pspusb.h`, `pspmp3.h`) and ARK-4's sources (`SystemCtrlForUser`, KUBridge, arkMenu).
- What Home already does: [home](../subsystems/home.md).

Labels in the "how" column: **API** a documented firmware call Home can make (user mode, or a
kernel function through KUBridge's `kuKernelCall`); **SYS DIALOG** the firmware's own utility
dialog drawn inside Home (savedata, OSK, netconf, HTML viewer: the same screens the XMB shows);
**OURS** Pocketwatch's own code; **XMB** stays in the XMB (Home's "Exit to the XMB").

## The XMB, category by category

### Settings

| Element (6.61) | What it does | In Home: how | Status |
|---|---|---|---|
| USB Connection | USB mass storage | API (`sceUsb*`, drivers via `kuKernelLoadModule`) - with automatic reconnect | Implemented (HOM-REQ-011) |
| System Update | firmware update | XMB - never from Home (CFW: an update removes ARK-4) | Dropped (safety) |
| Video Settings | UMD Video / video playback options (language, subtitles, screen size) | needs a video player first | XMB (Planned with video, RESEARCH REQUIRED) |
| Photo Settings | slideshow speed | OURS, with the photo viewer | Planned (HOM-REQ-016) |
| System Settings → System Language | the UI language | API `sceUtilitySetSystemParamInt(LANGUAGE)` | Implemented (HOM-REQ-012) |
| System Settings → Nickname | the PSP's name | SYS DIALOG (OSK) + API `SetSystemParamString(NICKNAME)` | Implemented (HOM-REQ-012) |
| System Settings → System Information | MAC address, system software, nickname | API (`sceKernelDevkitVersion`, `sceWlanGetEtherAddr`, KUBridge model, ARK-4's version) | Implemented (HOM-REQ-013) |
| System Settings → Format Memory Stick | erases the card | XMB - destructive | Dropped (safety) |
| System Settings → Restore Default Settings | resets the registry | XMB - destructive | Dropped (safety) |
| System Settings → Character Set, USB Charge, UMD Cache | small toggles | Character Set: XMB; USB Charge: ARK-4's own setting (`usbcharge`); UMD Cache: ARK-4 `noumd` makes it moot | XMB |
| Date & Time Settings → Date Format, Time Format, Time Zone, Summer Time | how the clock is shown | API `SetSystemParamInt(DATE_FORMAT / TIME_FORMAT / TIMEZONE / DAYLIGHTSAVINGS)` | Implemented (HOM-REQ-012) |
| Date & Time Settings → Date & Time | sets the clock | needs `sceRtcSetCurrentTick` (kernel, RESEARCH REQUIRED: what the XMB writes besides the RTC) | XMB |
| Power Save Settings → Backlight Brightness | 4 levels | API (impose `BACKLIGHT_BRIGHTNESS`, through KUBridge) | Implemented (HOM-REQ-012) |
| Power Save Settings → Backlight Auto-Off | off after 10 s..5 min | API (`sceImposeSetBacklightOffTime`) | Implemented (HOM-REQ-012) |
| Power Save Settings → Auto Sleep, WLAN Power Save | sleep timer; WLAN | WLAN: API `SetSystemParamInt(WLAN_POWERSAVE)`; Auto Sleep: registry (RESEARCH REQUIRED) | WLAN implemented; Auto Sleep XMB |
| Sound Settings → AVLS | volume limit | API (impose `AVLS`) | Implemented (HOM-REQ-012) |
| Sound Settings → Key Tones | click sounds | registry (RESEARCH REQUIRED) | XMB |
| Theme Settings → Theme, Color, Wallpaper | the XMB's look | OURS: Home's own look (styles, accent colour, the game's art as background) | Planned (HOM-REQ-006) |
| Security Settings | parental control, passwords | XMB (registry, password) | XMB |
| Network Settings | access points, ad hoc channel | SYS DIALOG (netconf) connects; editing entries stays in the XMB; ad hoc channel: API | Planned (HOM-REQ-017) |
| Remote Play / RSS Channel / Print Settings | PS3 pairing, feeds, PictBridge | services of a past era | XMB |

### Photo, Music, Video

| Element | What it does | In Home: how | Status |
|---|---|---|---|
| Photo → Memory Stick | browse and view pictures (`PICTURE/`, `DCIM/`) | OURS: list, thumbnails, full screen (JPEG/PNG through stb_image) | Planned (HOM-REQ-016) |
| Music → Memory Stick | play MP3/ATRAC from `MUSIC/` | OURS: `sceMp3` (firmware decoder, `PSP_MODULE_AV_MP3`) + `sceAudio`, playing on while Home is browsed | Planned (HOM-REQ-018) |
| Music → UMD Music / Video → UMD Video | UMD media | ARK-4's `noumd` (the owner's setting) disables the drive | XMB |
| Video → Memory Stick | play MP4/PMF | RESEARCH REQUIRED (`sceMpeg`/AVC; the XMB's player is part of `vsh`) | XMB |

### Game

| Element | What it does | In Home: how | Status |
|---|---|---|---|
| Memory Stick (games) | ISO/CSO (ARK-4), PSN, PS1, homebrew | OURS: ISO/CSO, homebrew, PS1 | Implemented (HOM-REQ-001, -008); PSN EBOOTs Planned |
| Saved Data Utility | the saves, with details and delete | SYS DIALOG: `sceUtilitySavedata` in list-delete mode - the XMB's own screen | Implemented (HOM-REQ-014) |
| (Triangle on a game) Information / Delete | the game's details | OURS: the game's save states with their pictures, "Continue from this state" (M7 V1) | Implemented (HOM-REQ-015) |
| Game Sharing / UMD | send a demo / play a disc | drive off (`noumd`); Game Sharing is a dead protocol | XMB |

### Network, PlayStation Network

| Element | What it does | In Home: how | Status |
|---|---|---|---|
| Internet Browser | web pages | SYS DIALOG: `sceUtilityHtmlViewer` (the system's browser) | Planned (HOM-REQ-017) |
| Online Instruction Manuals, RSS Channel, Internet Radio, Remote Play | Sony's online services | servers shut down or PS3-bound | Dropped (no service) |
| PlayStation Store, Account Management, Information Board | PSN | PSP's PSN services closed (2016) | Dropped (no service) |

### Always on screen

| Element | In Home | Status |
|---|---|---|
| Clock, battery | the status pill | Implemented |
| The month's colour, the wave | Home: the game's own art; the Classic style (HOM-REQ-006) brings the wave | Planned |
| Background music, sounds | key sounds of our own | Planned (HOM-REQ-019) |
| HOME button (in a game: Quit Game) | the in-game hub (states) | Implemented (SHL) |
| HOME in the XMB (power options) | Home's HOME panel: Close, USB, Exit to the XMB, Sleep, Turn Off | Implemented (HOM-REQ-010) |

## Conclusions

- Everything the XMB does for a player of games on a card can live in Home: games, saves,
  states, USB, the settings that matter, system information. What stays in the XMB is
  destructive (format, restore, update), tied to dead services, or to the UMD drive ARK-4 turned
  off - each one "Exit to the XMB" away.
- Order: first the settings and the system's own dialogs (small, native, safe), the game's
  states; then photos and music; the browser last (network modules, memory).
- RESEARCH REQUIRED before Home writes the registry directly (Auto Sleep, Key Tones, Date & Time):
  not done - only documented firmware setters are used.
