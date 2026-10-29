# M7 — booting the PSP straight into Pocketwatch (no XMB first)

Question: how can the PSP show Pocketwatch's own UI as soon as it is switched on, instead of the
XMB with Pocketwatch Home as an application to start (owner, 2026-10-18: "I just boot my PSP and
it is already there")?
Status: Answered (2026-10-18), implemented
([ADR-026](../architecture/decisions.md#adr-026--the-psp-can-start-in-pocketwatch-home-chosen-by-the-xmb-plugin-through-ark-4s-launcher-field),
ADR-027); **VERIFIED on the device** (run 2: the PSP starts in Home). HM6-HM9 pending.

## Background and sources

All file references are to ARK-4 (GPL-3.0-or-later,
[PSP-Archive/ARK-4](https://github.com/PSP-Archive/ARK-4)), commit `7c03c37` (2026-08-02), read
locally. The owner's PSP runs an ARK-4 whose `SETTINGS.TXT` has the line `always, launcher, off`
(read from the card 2026-10-18), so it has the launcher setting.

**How ARK-4 boots the VSH** (VERIFIED from source, `core/systemctrl/src/loadercore.c`,
`InitKernelStartModule`, lines 221-295; ARK-4 hooks the start of every boot module):

1. `sceImpose_Driver` starts → `checkControllerInput()` (START held: no plugins; SELECT held: no
   settings; `exitgame.c:283-294`) and `loadSettings()` (`SETTINGS.TXT`, `plugin.c:585`).
2. init asks to start `sceMediaSync` → before starting it, ARK-4 loads xmbctrl and **the VSH
   plugins** (`LoadPlugins`, lines 277-286), running each plugin's `module_start` there and then.
3. `sceMediaSync` itself starts → the module manager's prologue calls the start-module handlers
   (ARK-4 `modulemanager.c:185-197`; uOFW `src/kd/modulemgr/modulemgr.c:1160-1169`: `_StartModule`
   → `_PrologueModule`), ARK-4's own (`PSPOnModuleStart`) among them, which runs
   `processSettings()` (`core/compat/psp/syspatch.c:166-190`): with `launcher, on` it writes
   `"VBOOT.PBP"` into `ARKConfig.launcher`, **otherwise it clears that field** (unless it holds
   `"PROSHELL"`).
4. `vsh_module` (the XMB) starts → if `ARKConfig.recovery`, `ARKConfig.launcher[0]` or the
   launcher setting is set, ARK-4 makes vsh_module's entry a stub and calls
   `sctrlArkExitLauncher()` (lines 232-238), which starts `<ARK folder><launcher>` as a homebrew
   (`sctrlKernelLoadExecVSHWithApitype(0x141, ...)`, key `"game"`, `exitgame.c:65-131`). The XMB
   never runs. If that file is missing, it reboots into the VSH (`sctrlKernelExitVSH`).

So **a VSH plugin's `module_start` runs before the launcher field is decided** (step 2, a write
there is cleared in step 3 - the first build made exactly that mistake, see Results), but **its
start-module handler runs in step 3 right after ARK-4's** (handlers are chained, newest first,
each calling the older one), and that is still before the XMB starts. The launcher field is all
ARK-4 reads to decide. ARK-4's own xmbctrl already writes that field from
user mode: it reads the config with `sctrlArkGetConfig` (SystemCtrlForUser, NID `0xB00B1E55`,
`core/systemctrl/exports.exp:57`), sets `launcher = "PROSHELL"` and writes it back with
`sctrlArkSetConfig` (SystemCtrlPrivate `0x6EAFC03D`, `exports.exp:248`) through `kuKernelCall`
(`extras/modules/xmbctrl/main.c:280-305`; `sctrl_hen.c:251-258` - both are a plain `memcpy` of
`ARKConfig`, `common/include/ark.h:143-150`: magic, `arkpath[108]`, `exploit_id[12]`,
`launcher[20]`, `exec_mode`, `recovery`).

**What a launcher becomes after a game.** A game's exit reboots into the VSH; steps 1-4 run again,
so with the field set again the launcher comes back instead of the XMB (VERIFIED: the same code
path; ARK-4's own menu relies on it, `extras/menus/arkMenu/src/iso.cpp:249-256`).

**Documentation.** ARK-4's wiki: the `launcher` setting "replaces the XMB with a custom menu
launcher" ([settings](https://github.com/PSP-Archive/ARK-4/wiki/settings)); added in release r6
with neur0n's vMenu as a sample
([r6 notes](https://newreleases.io/project/github/PSP-Archive/ARK-4/release/r6)); recovery: hold
R while switching on; hold START / SELECT during boot to disable plugins / settings
([Recovery](https://github.com/PSP-Archive/ARK-4/wiki/Recovery)). Other launchers listed by ARK-4
(ONEMenu, vMenu, yMenu, gMenu, 138Menu, pyMenu) "may not work well with modern ARK or real PSP
hardware" ([Other Launchers](https://github.com/PSP-Archive/ARK-4/wiki/Other-Launchers)). iR Shell
replaced the XMB on Dark_AleX firmwares up to 5.00 M33 and was last updated in 2010
([gamebrew](https://www.gamebrew.org/wiki/IRShell_PSP)). No current (2025-2026) project boots a
real PSP into a modern home screen of its own (web search 2026-10-18; INFERRED from its absence).

## The routes

| Route | How | For | Against |
|---|---|---|---|
| A. ARK-4's launcher mode | `always, launcher, on` in `SETTINGS.TXT`, Home copied over `ARK_01234/VBOOT.PBP` | no code | replaces ARK-4's own menu (also its "exit to launcher" combo); every ARK-4 update puts it back; the XMB only by editing `SETTINGS.TXT`; no guard against a broken launcher |
| **B. The XMB plugin sets the launcher field** (chosen) | at its `module_start` (step 3): `launcher = "PWHOME.PBP"` when `config.txt` says `start = home` | ARK-4's own start path; ARK-4's menu and settings untouched; a setting in our XMB page; "the XMB this once"; a crash guard | depends on ARK-4's `ARKConfig` layout and boot order (checked before writing, RISK-021) |
| C. Start Home from our own vsh_module hook | LoadExec from the start-module handler | - | our own reboot code at the most delicate moment; B does the same through ARK-4's tested code |
| D. Draw our UI over a running XMB | the VSH plugin owns the screen and the buttons, the XMB runs underneath | keeps every XMB function | RESEARCH REQUIRED: the VSH plugin has 64 KB (our budget) while backgrounds need 0.5 MB each; drawing over paf's GE frames; feeding the XMB's input |

## The design (B), as implemented

`vsh/src/xboot.c`: `pwx_boot_decide` (first thing in `module_start`: config, markers, the
decision, `boot.txt`), `pwx_boot_module` (from the start-module handler, after the older
handlers, for `sceMediaSync`: the write - ADR-027); decision in portable
`plugin/src/core/bootpick.c` (host tests `boot_*`):

- `config.txt`: `start = xmb` (default) or `start = home`; XMB → Settings → Pocketwatch → Start
  screen writes it.
- The XMB this time when: `seplugins/pocketwatch/xmb-once` holds an RTC tick at most 120 s old
  (Home's Exit and HOME → Quit write it; the plugin removes it at every start, so it never
  outlives the start it was meant for - ADR-027), **L** is held, `boot-pending` is still there (the last start of
  Home never showed its first screen: Home removes it after its first frame), `PWHOME.PBP` is not
  in ARK-4's folder, or ARK-4's config is not as expected (magic `0xB00B1E55`, an `ms0:/` or `ef0:/`
  folder, a PSP, no launcher or recovery set already - otherwise ARK-4's own mode wins).
- Otherwise: write `boot-pending`; then, once ARK-4's `processSettings` has run, read the config
  again, check that no launcher or recovery is set, set `launcher = "PWHOME.PBP"` and write it back
  the way xmbctrl does. ARK-4 starts `ms0:/PSP/SAVEDATA/ARK_01234/PWHOME.PBP` (the deploy script
  copies Home there too). `boot.txt` keeps the last decisions with the time and the reason; if the
  XMB starts anyway, the plugin adds "... but the XMB started", removes `boot-pending` (not Home's
  fault) and `xmb.log` gives what was written and what the field held.
- Home lists ISO/CSO images and now the EBOOTs of `PSP/GAME` (homebrew `MG`, PS1 `ME`), starting
  them as ARK-4's menu does (`extras/menus/arkMenu/src/eboot.cpp:246-305`: apitype 0x141 key
  `game`, 0x144 key `pops`).

**Ways back to the XMB** (all of them, in order of reach): Home → System → Exit to the XMB; HOME →
Quit in Home; hold **L** while switching on; hold **START** while switching on (no plugins at all,
ARK-4); hold **R** (ARK-4's recovery); `start = xmb` in Settings or in `config.txt` over USB from
the XMB.

**What stays in the XMB** while Home is the start screen: System Settings, music / video / photo,
network, the USB connection, UMD discs and PSN EBOOTs (Home does not list them yet), and ARK-4's
VSH menu. Each is one "Exit to the XMB" away.

## Host evidence (2026-10-18)

The PBP reader (`home/src/disc.c`, `hm_pbp_part`) on the owner's card, read only: all 9 EBOOTs of
`PSP/GAME` parsed. Homebrew `MG` (Homebrew Sorter, PSP Filer, pspIdent, raylib test, BatterySteve)
all carry the default DISC_ID `UCJS10041` - so play time and states are matched for images only.
PS1 games `ME` (Final Fantasy Tactics SCUS94221, Metal Gear Solid SLUS00594, Mizzurna Falls
SLPS01783) have 80 x 80 icons - kept square in the row, not stretched.

## Procedure

Testing guide HM5-HM9. Safety: nothing is written to flash or to ARK-4's files; the only state
is our `config.txt` and two marker files. Boot time was not measured (INFERRED: like ARK-4's own
launcher mode - the VSH's modules up to vsh_module, then one reboot into Home).

## Results

**Run 1 — 2026-10-18, build `df27d0d`.** Raw files:
[`logs/2026-10-18-boot-shell-run1.log`](logs/2026-10-18-boot-shell-run1.log). The owner set Start
screen → Pocketwatch Home (`config.txt` 05:26:48 by the PSP's clock) and restarted: the XMB.
`boot.txt` (05:28:00): `start: XMB (the XMB asked from Home), ARK-4 config check 0` - the layout
check passed and `PWHOME.PBP` was there, but an `xmb-once` marker was found. Only Home writes it
(Exit, HOME → Quit); between the setting (05:26:48) and that start Home had been run and left,
or the marker was older - the first build had no way to tell. Two faults, both fixed by ADR-027:

1. the marker had no time, so one left from an earlier visit to Home counted at a later
   power-on;
2. reading the boot order again for this run (step 3 above): the first build wrote the launcher
   field in `module_start`, which ARK-4's `processSettings` clears a moment later - even without
   the marker the XMB would have come. VERIFIED from source (uOFW and ARK-4, as cited); not yet
   on the device.

**Run 2 — 2026-10-18, build `1fefdef`.** Raw files:
[`logs/2026-10-18-boot-shell-run2.log`](logs/2026-10-18-boot-shell-run2.log). **VERIFIED: the
PSP started in Pocketwatch Home** (`boot.txt` 06:41:24 `start: Pocketwatch Home`; the owner saw
Home come up). 57 s later HOME → the system's quit dialog left Home and the XMB came, as designed
then; the owner expected Pocketwatch's own screen there (HOME now opens Home's own panel,
HOM-REQ-010, ADR-028). The owner also found Home's screen slow (ADR-028).

## Conclusions

- VERIFIED from source: route B uses only what ARK-4 itself uses, at the moment ARK-4 itself
  decides.
- VERIFIED (run 2): the write from the start-module handler holds on the owner's ARK-4 build -
  the PSP starts in Home.
- OPEN: boot time to Home's first screen (not measured yet).
- Next: device test; then UMD and PSN games, USB in Home, route D as research.
