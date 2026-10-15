# Pocketwatch

Kernel-mode PRX plugin for PSP CFW (ARK-4 6.61, target PSP-3000). Tagline: "Pause. Save. Play."
Features are called *complications*; the first one is States (savestates).
Approved architecture and roadmap: `C:\Users\smlkr\.claude\plans\playful-riding-pixel.md`.

## Commands

- Build (Windows, toolchain in WSL Ubuntu at `~/pspdev`): `scripts\build.cmd` (`-Target check` / `-Target clean`).
  `.ps1` files cannot be run directly on this machine (execution policy); use the `.cmd` wrappers.
- Build + static checks (Linux/WSL): `bash scripts/build.sh` then `bash scripts/build.sh check`
- Host unit tests: `make -C tests` (in WSL; `make -C tests run T=<name filter>`)
- Format: `bash scripts/format.sh` (`--check` in CI; clang-format 18.1.8, set `CLANG_FORMAT` to override)
- Deploy to the USB-connected PSP: `scripts\deploy.cmd` (add `-Enable` to register in `PLUGINS.TXT`)
- On-device log: `<drive>:\seplugins\pocketwatch\pocketwatch.log` (first lines include the import self-check)
- Debug-build combos in a game: L+R+SELECT+RIGHT = save state to RAM, LEFT = load it,
  UP = drop the state, write the held log lines and dump memory/modules/threads
- From PowerShell, call WSL with `wsl -d Ubuntu --exec bash <script>`; inline `bash -c '...$VAR...'`
  gets mangled by the outer shell.

## Conventions

- Kernel libs only (`USE_KERNEL_LIBS = 1`, no newlib): libc comes from SysclibForKernel; formatting uses
  the vendored `plugin/lib/stb_sprintf` (NOFLOAT, NOUNALIGNED).
- Every new kernel import must pass `make check`: NIDs from randomized libraries need an entry in ARK-4's
  table (`tools/nids/ark4_660.tsv`) or in `tools/nids/allowlist.tsv` with evidence.
- Portable logic must not include PSP headers; OS access goes through `core/platform.h` so it is
  host-testable. Add tests in `tests/test_*.c` and list new portable sources in `tests/Makefile`.
- Never do file I/O in sysevent/suspend paths: use `pw_log_set_deferred(1)` and flush later.
- Data that must survive a suspend lives in P9 only: the firmware overwrites P11 (ME eDRAM save,
  event 0x4005) and P5 (GE eDRAM save, PHASE0_3) during every suspend. Around the apply blob mask
  interrupts with CP0 Status.IE, never `sceKernelCpuSuspendIntr` (the blob does not restore IC).
- While the engine holds a state, nothing may write to the Memory Stick (the saved kernel's FAT
  view would go stale): logging is deferred until `pw_engine_discard`.
- Public symbols use the `pw_` prefix; modules live in `plugin/src/<area>/` (core, gfx, ui, complications/*).
- New source files must be added to `OBJS` in `plugin/Makefile`.
- `.ps1` files must stay ASCII-only (Windows PowerShell 5.1 reads them as ANSI).
- License is GPL-3.0-or-later: start every new source file with `// SPDX-License-Identifier: GPL-3.0-or-later`
  (third-party files keep their own SPDX line). Never copy code from PSPStates (no license) or intraFont (CC BY-SA).
- One commit per substantial finished chunk (code + tests + docs together), not per micro-step
  (Conventional Commits). No `Co-Authored-By` or other AI attribution lines.
