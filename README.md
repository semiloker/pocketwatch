# Pocketwatch

> **P**ause. **S**ave. **P**lay.

**Save states for the PSP, on real hardware.** Pocketwatch is a plugin for the ARK-4 custom
firmware: press HOME in any retail game, save the moment to a slot, and load it later. It comes
with Pocketwatch Home, an XMB-style home screen with games, music, photos and a photo editor.

**[Download](https://github.com/semiloker/pocketwatch/releases) · [How to install](INSTALL.md) ·
[Report a problem](https://github.com/semiloker/pocketwatch/issues/new/choose)**

> **Pre-release.** Tested on one PSP-3000 with ARK-4 on firmware 6.61. Save states are experimental,
> so back up your Memory Stick first.

A pocket watch is a precision instrument you carry everywhere. In watchmaking, every
feature beyond telling the time is called a *complication*. Pocketwatch follows the same
idea: one plugin, a growing set of complications.

## What it does

| Part | What you get |
|---|---|
| **In-game menu** | HOME in a game freezes it and opens Resume Game, Save State (five slots per game), Load State, Screenshot, Quit Game, in three styles: Classic (after the XMB), Modern, Minimal |
| **Pocketwatch Home** | A home screen of its own, optionally the one the PSP starts in: games, a music player, photos with an editor, USB, Wi-Fi connections, battery calibration |
| **XMB pages** | Settings → Pocketwatch and Battery, Game → Play Time |
| **Overlay in games** (experimental) | Frame rate and processor load, volume and brightness bars, a brightness boost |
| **Translations** | English, German, Spanish, French, Italian, Dutch, Portuguese, Russian, Ukrainian |

## Screenshots

Pocketwatch Home, the home screen that comes with Pocketwatch, captured in PPSSPP with demo
content ([where the pictures come from](docs/assets/screenshots/README.md)):

| | |
|---|---|
| ![Music with the track playing](docs/assets/screenshots/home-music.png) | ![Now Playing](docs/assets/screenshots/home-player.png) |
| Music (Modern style) | Now Playing |
| ![An album of photos](docs/assets/screenshots/home-photos.png) | ![The photo editor](docs/assets/screenshots/home-editor.png) |
| Photos | The photo editor |
| ![Settings](docs/assets/screenshots/home-settings.png) | ![Classic style](docs/assets/screenshots/home-classic.png) |
| Settings | Classic style, after the XMB |
| ![Minimal style](docs/assets/screenshots/home-minimal.png) | |
| Minimal style | |

## Complications

| Complication | Status | What it does |
|---|---|---|
| **States** | in development | Save, load and delete savestates per game, with slots and thumbnails |

More complications will follow once States is solid.

## Compatibility

| | |
|---|---|
| Firmware | ARK-4 (primary target). Other CFWs are untested. |
| Models | PSP-3000 (the only tested model). PSP-2000, PSP Go and PSP Street are untested. PSP-1000 is not supported for States (32 MB RAM). |
| Games | Retail games, from disc or ISO. Homebrew does not get the in-game menu. |

Do not run Pocketwatch States together with another savestate plugin
(PSPStates / Custom Home states): running two savestate engines at once is not supported.

## Install

The short version, for a PSP already running ARK-4. **The [install guide](INSTALL.md) has every step,
how to check it worked, and what to do when it does not.**

1. Download `pocketwatch-<version>.zip` from [Releases](https://github.com/semiloker/pocketwatch/releases)
   and copy the `seplugins` and `PSP` folders from inside it to the root of the Memory Stick.
2. Add these lines to `seplugins/PLUGINS.TXT`:

   ```text
   UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on
   VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on
   ```

3. Turn the PSP off and on again. Hold START at power-on if it ever fails to start: ARK-4 then loads
   no plugins.

## Build

Requires the [PSPDEV toolchain](https://pspdev.github.io/) (`PSPDEV` defaults to `~/pspdev`).

```sh
# Linux / macOS / WSL
bash scripts/build.sh            # -> plugin/pocketwatch.prx
bash scripts/build.sh check      # import NIDs vs ARK-4 6.61 + size budgets
bash scripts/build.sh clean
make -C tests                    # host unit tests (ASan + UBSan)
bash scripts/format.sh --check   # clang-format 18.1.8
```

```bat
:: Windows, toolchain installed in WSL (the .cmd wrappers bypass the PowerShell script policy)
scripts\build.cmd
scripts\build.cmd -Target check
scripts\deploy.cmd -Enable       :: copy to the PSP over USB and register in PLUGINS.TXT
```

Every push runs the host tests, the format check, the PSP build and `make check` on GitHub
Actions; pushing a `v*` tag publishes a release with the zip.

## Project layout

```
plugin/                  kernel PRX (Makefile, exports.exp)
  src/core/main.c        module entry, starts the complications
  src/core/log.c         RAM ring-buffer log -> pocketwatch.log next to the PRX
  src/core/selfcheck.c   on-device check that every import resolved
  src/core/platform*.c   OS services, so portable modules also build on the host
  lib/                   third-party code (stb_sprintf)
vsh/                     XMB plugin (Settings -> Pocketwatch / Battery, Game -> Play Time)
home/                    Pocketwatch Home, the home-screen application (EBOOT.PBP)
perf/                    in-game performance overlay module
tests/                   host unit tests
tools/check_prx.py       static import/size checks; NID tables in tools/nids/
scripts/                 build, deploy and format helpers
.github/                 CI, issue templates
```

## Documentation

Design, status and research live in [`docs/`](docs/README.md): start with the
[documentation index](docs/README.md), the [roadmap](docs/roadmap.md) and the
[project status](docs/project-status.md).

## Prior art

Pocketwatch is written from scratch, but stands on the shoulders of:

- [PSPStates](https://github.com/PSP-Archive/PspStates-Experiment) by Dark_Alex - the original PSP savestate plugin
- [Custom Home Mod](https://github.com/PSP-Archive/Custom-Home-Mod) by ABCanG and suloku - in-game HOME menu with PSPStates integration
- [PSPFatSave](https://github.com/Grunwalskii/PSPFatSave-Public) - savestates on the PSP-1000
- [ARK-4](https://github.com/PSP-Archive/ARK-4) and [PSPDEV](https://github.com/pspdev)

## License

Pocketwatch is free software, licensed under the
[GNU General Public License v3.0 or later](LICENSE) (`GPL-3.0-or-later`).
Bundled third-party code keeps its own license (noted in each file's header).
Sony firmware assets (fonts, icons) are never redistributed - they are read at runtime
from the user's own PSP.

Pocketwatch is an independent project, not affiliated with or endorsed by Sony. "PlayStation" and
"PSP" are Sony's trademarks. It contains no games or firmware, and this project does not help anyone
obtain them.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Security problems: [SECURITY.md](SECURITY.md).
