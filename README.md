# Pocketwatch

> **P**ause. **S**ave. **P**lay.

Pocketwatch is a plugin for PSP custom firmware that puts time back in your hands:
freeze any game at any moment, keep the moment, and come back to it later.

A pocket watch is a precision instrument you carry everywhere. In watchmaking, every
feature beyond telling the time is called a *complication*. Pocketwatch follows the same
idea: one plugin, a growing set of complications.

## Complications

| Complication | Status | What it does |
|---|---|---|
| **States** | in development | Save, load and delete savestates per game, with slots and thumbnails |

More complications will follow once States is solid.

## Compatibility

| | |
|---|---|
| Firmware | ARK-4 (primary target). Other CFWs are untested. |
| Models | PSP-2000, PSP-3000, PSP Go, PSP Street. PSP-1000 is not supported for States (32 MB RAM). |
| Runlevel | `GAME` |

Do not run Pocketwatch States together with another savestate plugin
(PSPStates / Custom Home states): running two savestate engines at once is not supported.

## Install

1. Download `pocketwatch-<version>.zip` from [Releases](../../releases) and copy its
   `seplugins` folder to the root of the Memory Stick.
2. Add this line to `ms0:/seplugins/PLUGINS.TXT` (`ef0:` on the PSP Go internal storage):

   ```
   GAME, ms0:/seplugins/pocketwatch/pocketwatch.prx, on
   ```

3. Start a game. `ms0:/seplugins/pocketwatch/pocketwatch.log` shows the loaded version.

## Build

Requires the [PSPDEV toolchain](https://pspdev.github.io/) (`PSPDEV` defaults to `~/pspdev`).

```sh
# Linux / macOS / WSL
bash scripts/build.sh            # -> plugin/pocketwatch.prx
bash scripts/build.sh clean
```

```powershell
# Windows, toolchain installed in WSL
.\scripts\build.ps1
.\scripts\deploy.ps1 -Enable     # copy to the PSP over USB and register in PLUGINS.TXT
```

Every push is built by GitHub Actions; pushing a `v*` tag publishes a release with the zip.

## Project layout

```
plugin/            kernel PRX (Makefile, exports.exp)
  src/main.c       module entry, starts the complications
  src/log.c        pocketwatch.log next to the PRX
scripts/           build (WSL/Linux) and deploy (USB) helpers
.github/           CI, issue templates
```

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
