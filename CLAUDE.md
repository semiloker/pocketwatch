# Pocketwatch

Kernel-mode PRX plugin for PSP CFW (ARK-4). Tagline: "Pause. Save. Play."
Features are called *complications*; the first one is States (savestates).

## Commands

- Build (Windows, toolchain in WSL Ubuntu at `~/pspdev`): `.\scripts\build.ps1` / `.\scripts\build.ps1 -Target clean`
- Build (Linux/WSL): `bash scripts/build.sh`
- Deploy to the USB-connected PSP: `.\scripts\deploy.ps1` (add `-Enable` to register in `PLUGINS.TXT`)
- On-device log: `<drive>:\seplugins\pocketwatch\pocketwatch.log`

## Conventions

- Kernel libs only (`USE_KERNEL_LIBS = 1`, no newlib): libc comes from SysclibForKernel.
  There is no `snprintf`; format through `prnt` (see `plugin/src/log.c`).
- Public symbols use the `pw_` prefix; one module per `.c`/`.h` pair in `plugin/src/`.
- New source files must be added to `OBJS` in `plugin/Makefile`.
- `.ps1` files must stay ASCII-only (Windows PowerShell 5.1 reads them as ANSI).
- License is GPL-3.0-or-later: start every new source file with `// SPDX-License-Identifier: GPL-3.0-or-later`
  (third-party files keep their own SPDX line). Never copy code from PSPStates (no license) or intraFont (CC BY-SA).
- Commit after each logical step (Conventional Commits). No `Co-Authored-By` or other AI attribution lines.
