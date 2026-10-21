# Pocketwatch

Kernel-mode PRX plugin for PSP CFW (ARK-4 6.61, target PSP-3000). Tagline: "Pause. Save. Play."
Features are called *complications*; the first one is States (savestates).

## Documentation workflow (read before substantive work)

The project's full workflow is imported here and applies to every session:

@docs/AI_WORKFLOW.md

- Start from `docs/README.md` (index, subsystem status) and `docs/project-status.md` (state, next
  actions); resume unfinished work from there and from `docs/roadmap.md`, not from chat history.
- Inspect the relevant spec, research notes and code before assuming anything. The docs are the
  source of truth for approved requirements and architecture (`docs/subsystems/`, `docs/ui/`,
  `docs/architecture/decisions.md`).
- When requirements, decisions, interfaces or status change, update the docs in the same change.
  Record decisions as ADRs and unresolved questions explicitly; label claims VERIFIED / INFERRED /
  PROPOSED / OPEN QUESTION / RESEARCH REQUIRED.
- Do not replace or contradict a rule below without understanding why it exists (most come from
  failures on the device, see `docs/research/`).
- Before finishing: `python tools/check_docs.py` (links, IDs, this import) plus the code checks.

## Commands

- Build (Windows, toolchain in WSL Ubuntu at `~/pspdev`): `scripts\build.cmd` (`-Target check` / `-Target clean`;
  `-Release` for the release build without the debug probe, combos and test items).
  `.ps1` files cannot be run directly on this machine (execution policy); use the `.cmd` wrappers.
- Build + static checks (Linux/WSL): `bash scripts/build.sh` then `bash scripts/build.sh check`
- Host unit tests: `make -C tests` (in WSL; `make -C tests run T=<name filter>`)
- Format: `bash scripts/format.sh` (`--check` in CI; clang-format 18.1.8, set `CLANG_FORMAT` to override)
- Deploy to the USB-connected PSP: `scripts\deploy.cmd` (add `-Enable` to register in `PLUGINS.TXT`,
  `-EnableXmb` for the XMB plugin). `vsh/` builds `pocketwatch_xmb.prx` (user mode, VSH runlevel,
  Settings -> Pocketwatch, ADR-021; log `seplugins/pocketwatch/xmb.log`); build.sh builds both. Its
  patches must keep the chaining rule: find every site before the previous start-module handler
  (ARK-4's xmbctrl) runs, wrap what is there after it. A broken VSH plugin: hold START at boot.
- `home/` builds Pocketwatch Home (`home/EBOOT.PBP`, a homebrew application, ADR-025; deploy copies
  it to `PSP/GAME/PocketwatchHome/` and to `PSP/SAVEDATA/ARK_01234/PWHOME.PBP`). Its art:
  `python tools/make_home_art.py home/res` (ours, never Sony's). Previews: `home-*.png` from
  `make -C tests preview`. Home draws through `hm_gfx` (ADR-028): the GE on the PSP
  (`home/src/gfx_gu.c`), the CPU renderer for previews (`gfx_cpu.c`) - every operation needs both,
  drawing the same pixels. Home has the three styles too (ADR-033: `view_classic.c` - the XMB,
  with the system's pictures from flash0 via `xmbart.c` - `view_modern.c`, `view_minimal.c`);
  previews: `home-*.png`, `modern-home-*.png`, `minimal-home-*.png` (`RCO=` for the system's
  icons; Modern in each theme: `modern-home-theme-*.png`). Every section is a stack of pages
  (ADR-035): `pages.c` builds them (rows, tiles, what Enter does), `main.c` keeps the stack and
  the keys; Home's own settings (theme, accent, background, music modes) are `home.txt`
  (`hcfg.c`), never `config.txt` (the XMB plugin's parser, RISK-022). Frame rate: `seplugins/pocketwatch/home.log`. Nothing slow in its frame
  loop (ADR-031): the battery comes from `hw.c`'s thread, the system's parameters from
  `hm_hw_param` (kept). Home's strings are the
  `PW_I18N_HOME` tier (after `PW_S_HOME_FIRST`); the XMB plugin builds without them.
- `perf/` builds `pocketwatch_perf.prx`, the performance overlay in games (ADR-043): a kernel
  module of its own that ARK-4 loads only while its PLUGINS.TXT line is on - Home → Settings →
  Pocketwatch → Overlay in Games writes that line and `perf.txt`; in a game L+R+SELECT held 1 s
  cycles Off / FPS / Full; deploy copies it. Home has its own overlay (`home.txt perf`).
- The overlay module also shows the volume and brightness indicator in games and keeps the
  brightness boost (ADR-045): `perf.txt` `mode = off|fps|full`, `osd = off|home|all`,
  `boost = 0-100`, written by Home's Settings (Sound and Display), its PLUGINS.TXT line on while a
  game needs one of them; Home shows the indicator and applies the boost itself. Own Steps
  (ADR-047, `steps = on`, `levels = ...`): SCREEN taps go through Pocketwatch's levels (bright3's
  job, which is off now). The module runs ONLY in a real disc/ISO game (ADR-049:
  `is_disc_apitype` in `module_start`) - never in Home or the XMB, so it cannot affect boot; one
  game (UMD) line, no VSH line. In-game it is still experimental; validate in one game before
  trusting it. For XMB brightness there is no Pocketwatch option - bright3 remains the safe way.
- Wi-Fi in Home (ADR-044): Settings → Wi-Fi Connections (search, a password on Home's keyboard,
  the firmware's netparam calls; `home net:` lines). Imports: never an `IMPORT_START` of a library
  PSPSDK already has, and a library's stubs must be linked together - psp-fixup-imports' "stubs
  out of order" built Home's `sceNetInet` table over `sceNet`'s (0x8002013A); `make check` fails
  on a stub in two tables. Extra NIDs of an SDK library: `IMPORT_FUNC`s linked right after it
  (`home/src/apctl_scan.S`). After USB Home writes nothing and starts again (ADR-048): the FAT driver's stale tables
  broke the card's PRXs once (a user program may not drop them, 0x800200D1).
- Home keeps the clock (ADR-041): at its start, before any write, a lost clock (the kernel refuses
  it, or it reads before 2016) is set to the newest file time of 2016 or later; Settings → Date and
  Time → Set Time from Internet (SNTP on a thread); `home clock:` lines in `home.log`. Home's
  battery calibration (ADR-042): `battery-cal.txt` (the run), `battery.txt` (the result).
- Performance (ADR-029): `make -C tests bench FONT=<a .pgf> BENCH_OPT=-Os` is a frame of each
  hub style as the PSP draws it, plus Home's stamp cost. Per-pixel helpers are `PW_PIXEL`
  (always inlined; the plugin is `-Os` and its kernel RAM budget has no room for `-O2`). Check
  Home's GE path in PPSSPP before a device round (`C:\Program Files\PPSSPP`, run
  `home\EBOOT.PBP`, capture the window).
- Start screen (ADR-026, ADR-027): `config.txt` `start = home`: the XMB plugin decides in
  `module_start` (`boot.txt`) and writes ARK-4's launcher field (`PWHOME.PBP`) from its start-module
  handler for sceMediaSync, after the older handlers - a write in `module_start` is cleared by
  ARK-4's processSettings. Never write any other `ARKConfig` field, and only after
  `pw_boot_ark_check`. The XMB instead: hold L at power-on, Home's Exit / HOME -> Quit (`xmb-once`
  with an RTC tick, valid 120 s), a start of Home that never showed a frame (`boot-pending`); hold
  START at power-on for no plugins at all. The XMB plugin is at its 64 KB budget.
- Device test protocols for every feature: `docs/testing.md`.
- On-device log: `<drive>:\seplugins\pocketwatch\pocketwatch.log` (first lines include the import self-check)
- In a game, HOME opens Pocketwatch's hub instead of the system HOME screen (Resume Game / Save
  State / Load State / Quit Game; debug builds add Memory Stick Test = T1). Settings (style, fast
  save and load, clock and battery, language) are changed in the XMB and live in
  `seplugins/pocketwatch/config.txt` and `language.txt`. Styles: Classic (XMB, default), Modern, Minimal (`docs/ui/styles.md`); every
  `pw_view_*` must have all three (previews write `modern-` / `minimal-` PNGs too). Save/Load open a list of
  five slots per game (`ms0:/PSP/SAVEDATA/PW<DISCID><NN>/`, triangle deletes). Enter/Back follow the
  system setting; HOME or Back closes it. If the game or a plugin had already turned the system HOME
  screen off, a short tap of NOTE opens the hub instead. The game is frozen and sees no buttons.
- Continue from a state at the game's start (M7 GAME side, test V1): put `continue.txt` with one
  line `<disc ID> <slot>` (e.g. `ULUS10277 2`) into `seplugins\pocketwatch\`; read and deleted at
  plugin start.
- Host previews of every menu screen as PNGs: `make -C tests preview FONT=<path to a .pgf>`
  (in WSL; never commit renders made with a Sony font); `UI_LANG=ru` renders a translation.
- UI text goes through `pw_tr(PW_S_...)` (`ui/i18n.h`, English built in); translations are
  `plugin/lang/<code>.txt`, deployed to `seplugins/pocketwatch/lang/`; `language.txt` there with
  one code overrides the system language. A new string needs a key in every lang file (the host
  test fails otherwise) with the same printf conversions as the English one.
- Frame cost of the menu by part (host, proportions only): `make -C tests bench`. The hub logs its
  real frame rate on the PSP when it closes (`hub: ... fps`). Motion must run by time (`ui/anim.h`),
  never by frame; no division or square root per pixel in drawing code (`ui/shapes.c`, `ui/draw.c`).
- Debug builds: SELECT in any menu screen writes `shot-NNN.png` (the exact screen) next to the log.
- Debug-build combos in a game: L+R+START+RIGHT = save state to RAM, L+R+START+LEFT = load it;
  L+R+SELECT+UP = drop the state, write the held log lines and dump memory/modules/threads;
  L+R+SELECT+RIGHT = rollback self-test, L+R+SELECT+LEFT = frozen save + immediate load;
  L+R+START+UP = throughput benchmark on the frozen game's RAM (no suspend);
  L+R+SELECT+TRIANGLE = one touch-and-go suspend (no real sleep), L+R+SELECT+SQUARE = toggle
  touch-and-go ("fast mode", on by default) for save/load/self-test
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
- A load must never run without the flash driver rebuild (`pw_flash_reinit` at RESUME_COMPLETED):
  a rolled-back lflash map corrupts flash1 and the PSP loses its system settings (E2 run 5).
- A touch-and-go suspend must be followed by putting the IPL's seeds back (8 words at 0xBFC00200,
  saved before every suspend request): without them the next reboot - quitting the game - cannot
  decrypt reboot.bin and the PSP hangs on a black screen (speed runs 3-7, docs/research/m2-speed.md).
- Card writes while a state is held are allowed since T1 (the FAT driver sees them after a
  rollback, ADR-017), but only after the save: nothing may write between the save's freeze and its
  resume (log deferred). Every load from a slot must run with `pw_store_arm` + the store's load hook
  (set before the save, it is part of the snapshot): the marker check is what protects the card
  until T1b covers touch-and-go saves.
- A state loads only into the exact environment that made it (`pw_engine_env`: build string and
  version, the PRX file's CRC, model, P9/P8/ME layout, the assets and PARAM.SFO block addresses): never relax it
  without a device test (T5).
- Public symbols use the `pw_` prefix; modules live in `plugin/src/<area>/` (core, gfx, ui, complications/*).
- Background work of a complication goes through its `tick` (once a second from the shell, hub
  closed, engine idle) and `on_quit`; never its own thread. State that must survive a load
  (counters) lives in a P9 block whose address goes into the fingerprint (`pw_engine_set_env_extra`,
  [3] play time). Play time files: `seplugins/pocketwatch/playtime-a.txt` / `-b.txt`.
- Settings, Battery and Play Time pages are in the XMB (ADR-024), not in the hub: XMB → Settings →
  Pocketwatch / Battery, XMB → Game → Play Time (`vsh/src/xsettings.c` builds their rows). Strings
  only the XMB shows come after `PW_S_XMB_FIRST` in `ui/i18n.h` (the game build drops them).
- A hub feature is a complication: its folder under `complications/` with a `pw_complication`
  (`complications/complication.h`) and one line in `complications/table.c`; the hub (`ui/hub.c`)
  must not learn about any feature.
- New source files must be added to `OBJS` in `plugin/Makefile`.
- `.ps1` files must stay ASCII-only (Windows PowerShell 5.1 reads them as ANSI).
- License is GPL-3.0-or-later: start every new source file with `// SPDX-License-Identifier: GPL-3.0-or-later`
  (third-party files keep their own SPDX line). Never copy code from PSPStates (no license) or intraFont (CC BY-SA).
- One commit per substantial finished chunk (code + tests + docs together), not per micro-step
  (Conventional Commits). No `Co-Authored-By` or other AI attribution lines.
