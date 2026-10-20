# Risk register

Likelihood and impact: **H**igh / **M**edium / **L**ow (qualitative, with the reason). Status:
*Open*, *Mitigated* (controls in place, residual risk noted), *Closed*. "Owner" is the role that
acts next: **Dev** (implementation sessions) or **Owner** (device tests, decisions). Never delete a
risk; close it with the evidence.

| ID | Risk | L | I | Status |
|---|---|---|---|---|
| [RISK-001](#risk-001--memory-stick-fat-corruption-after-a-rollback) | Memory Stick FAT corruption after a rollback | M | H | Open |
| [RISK-002](#risk-002--flash1-corruption-after-a-rollback) | flash1 corruption after a rollback | L | H | Mitigated |
| [RISK-003](#risk-003--hang-on-quit-after-a-touch-and-go-suspend) | Hang on quit after a touch-and-go suspend | L | H | Mitigated |
| [RISK-004](#risk-004--game-specific-incompatibility) | Game-specific incompatibility | H | M | Open |
| [RISK-005](#risk-005--kernel-ram-and-prx-size-growth) | Kernel RAM / PRX size growth | M | M | Open |
| [RISK-006](#risk-006--cross-boot-load-into-a-different-environment) | Cross-boot load into a different environment | H | H | Open |
| [RISK-007](#risk-007--other-plugins-and-ark-settings) | Other plugins and ARK settings | M | M | Open |
| [RISK-008](#risk-008--ark-4-api-or-nid-table-changes) | ARK-4 API or NID table changes | L | M | Mitigated |
| [RISK-009](#risk-009--home-takeover-side-effects) | HOME takeover side effects | M | M | Open |
| [RISK-010](#risk-010--parsing-sony-assets-on-the-device) | Parsing Sony assets on the device | M | M | Open |
| [RISK-011](#risk-011--power-loss-or-card-removal-during-a-state-write) | Power loss / card removal during a write | L | M | Open |
| [RISK-012](#risk-012--bricking-during-firmware-research) | Bricking during firmware research | L | H | Open |
| [RISK-013](#risk-013--licensing-and-redistribution) | Licensing and redistribution | L | H | Mitigated |
| [RISK-014](#risk-014--intermittent-device-behaviour) | Intermittent device behaviour | M | M | Open |
| [RISK-015](#risk-015--sleep-or-usb-while-the-menu-is-open) | Sleep or USB while the menu is open | M | M | Open |
| [RISK-016](#risk-016--a-game-that-does-not-survive-the-saves-sleep) | A game that does not survive the save's sleep | M | H | Open |
| [RISK-017](#risk-017--arks-exit-combo-fired-by-another-plugin-during-a-save) | ARK's exit combo fired by another plugin during a save | L | H | Open |
| [RISK-018](#risk-018--the-xmb-plugin-stops-the-xmb-from-starting) | The XMB plugin stops the XMB from starting | L | M | Open |
| [RISK-019](#risk-019--the-translation-pool-is-nearly-full) | The translation pool is nearly full | M | L | Open |
| [RISK-020](#risk-020--a-game-started-from-home-is-in-another-setup-than-from-the-xmb) | A game started from Home is in another setup than from the XMB | M | M | Open |
| [RISK-021](#risk-021--starting-in-home-depends-on-ark-4s-config-layout-and-boot-order) | Starting in Home depends on ARK-4's config layout and boot order | L | H | Open |
| [RISK-022](#risk-022--the-xmb-plugin-is-at-its-64-kb-budget) | The XMB plugin is at its 64 KB budget | H | M | Open |
| [RISK-023](#risk-023--a-frozen-game-thread-holds-a-lock-other-than-the-cards) | A frozen game thread holds a lock other than the card's | L | H | Open |
| [RISK-024](#risk-024--another-plugin-holds-a-device-home-does-not-know) | Another plugin holds a device Home does not know | L | M | Open |
| [RISK-025](#risk-025--home-deletes-saved-data-itself) | Home deletes saved data itself | L | H | Open |

## RISK-001 — Memory Stick FAT corruption after a rollback

- **Description:** A load restores the save-time kernel, including the FAT driver's in-memory
  state. If anything wrote to the card between save and load (a game save, a log, a state file, a
  USB copy, another boot), the restored driver may write stale FAT/directory sectors back.
- **Likelihood L:** T1 (2026-10-16, VERIFIED for a real-sleep save) showed the driver sees files
  and allocations made after the capture; touch-and-go saves are not covered yet (T1b).
- **Impact H:** loss of the user's saves and files.
- **Mitigation:** after every load from a slot the restored world checks that it sees the
  marker file that save created (a new name each time) before anything writes to the card; on
  failure card writes stop until reboot and the hub says "Do not save in the game: quit and restart
  the PSP" ([ADR-017](architecture/decisions.md#adr-017--state-files-are-written-after-the-capture)). ARK's `mscache` window is dropped before the check. E4: 100
  cycles with a PC FAT check. Owner: **Owner** (T1b, E4 runs with a card backup).
- **Links:** [m3-menu-e3 phase 2](research/m3-menu-e3.md), [states storage](subsystems/states-storage.md).

## RISK-002 — flash1 corruption after a rollback

- **Description:** Stale lflash map after a rollback corrupts flash1; the PSP resets its settings.
- **Status Mitigated:** flash driver rebuild after every resume ([ADR-006](architecture/decisions.md#adr-006--rebuild-the-flash-drivers-after-every-resume)), load refused without it. Residual: backlog S3 (skipping the rebuild) must never ship without a proof.
- **Evidence:** E2 run 5 (incident), runs 6-7 (fixed).

## RISK-003 — Hang on quit after a touch-and-go suspend

- **Description:** Touch-and-go zeroes the IPL seeds; the next reboot cannot decrypt reboot.bin.
- **Status Mitigated:** seeds saved before every suspend and restored at `0x10000`; fast mode
  disabled when no seeds were seen ([ADR-008](architecture/decisions.md#adr-008--touch-and-go-suspend-by-default)). Evidence: speed run 7.

## RISK-004 — Game-specific incompatibility

- **Description:** Games using P5 (volatile), highmem, video playback during the freeze, interrupt
  handlers writing game RAM, Inferno's cache in P9, or special hardware (camera, GPS, network).
- **Likelihood H:** only two games tested (NFS Most Wanted, Castlevania DXC).
- **Mitigation:** refusal checks (P2 size, P5 use); compatibility matrix on the owner's 22 ISOs in
  M6 ([roadmap](roadmap.md)). Owner: **Owner** (tests), **Dev** (refusals).

## RISK-005 — Kernel RAM and PRX size growth

- **Description:** P1 has ~300 KB free in games and is shared with other plugins; the UI (fonts,
  icons, text) and E3 grow the module.
- **Mitigation:** size budgets in `make check` (PRX 160 KB, loaded 176 KB); every large buffer in
  P9/P11; translations as files (ADR-018). 2026-10-17: the debug profile got its own budget (176 KB / 192 KB,
  test runs only). With the UI styles (ADR-019) the PRX file budget became 192 KB (it costs no RAM);
  kernel RAM went down to 162 KB of 176 KB by moving the sky and SFO buffers to P9.
  2026-10-17 (branch `feature/psp-xmb-overhaul`): Battery, Play Time and their pages first cost
  ~16 KB in the game; they moved to the XMB plugin (ADR-024) together with the in-game Settings
  screen, and the strings only the XMB shows left the game build: release 175 KB loaded, under the
  old budgets again. The game keeps only the play-time counter (P9 block). The XMB plugin has its
  own budget (56 KB file / 64 KB loaded).

## RISK-006 — Cross-boot load into a different environment

- **Description:** Loading a state saved under another plugin set, ISO, ARK config, firmware or
  Pocketwatch build restores a kernel that does not match the hardware/software → crash or data
  damage.
- **Mitigation (PROPOSED, E3):** environment fingerprint checked before any freeze; save-time
  addresses in the header; hardware-describing data (seeds, live switches, resume info) always
  taken from the current boot. Owner: **Dev**.

## RISK-007 — Other plugins and ARK settings

- **Description:** RemoteJoyLite holds USB and disturbs suspends; PSPStates/Custom Home would run
  a second engine; ARK `mscache`/`infernocache`/`highmem` change memory and caching.
- **Mitigation:** detect and refuse (guards, M3); document required settings.

## RISK-008 — ARK-4 API or NID table changes

- **Description:** A new ARK-4 release changes its resolver table or SystemCtrl API.
- **Status Mitigated:** `make check` against the vendored table, boot self-check logs unresolved
  imports; the ARK version goes into the E3 fingerprint.

## RISK-009 — HOME takeover side effects

- **Description:** The system HOME screen may still flash, a game that disabled HOME must keep it
  disabled, impose may re-enable the popup after a resume or a utility dialog, and a custom Quit
  Game may hang if the game's exit callback never returns.
- **Mitigation:** **RESEARCH REQUIRED** before implementation ([game shell](subsystems/game-shell.md)).

## RISK-010 — Parsing Sony assets on the device

- **Description:** PGF/RCO/GIM parsers read firmware files; a malformed or unexpected file (other
  firmware, language packs) could crash the kernel.
- **Mitigation:** bounds-checked parsers, host tests with synthetic files, fallback to the built-in
  8x8 font and plain shapes when an asset is missing ([rendering](ui/rendering.md)).

## RISK-011 — Power loss or card removal during a state write

- **Description:** A battery cut or card removal while writing a state.
- **Mitigation:** STATE.BIN (the commit record) is zeroed first and written last, so an
  interrupted write leaves an empty slot that is never loaded (host test
  `store_interrupted_save_leaves_an_empty_slot`); low-battery guard. Accepted: the slot's previous
  state is lost (keeping it would double the space, ADR-017); the overwrite confirmation says so.

## RISK-012 — Bricking during firmware research

- **Description:** NAND/IPL/SYSCON writes can make the PSP unbootable; later PSP-3000 boards have
  limited service-mode recovery (**RESEARCH REQUIRED**).
- **Mitigation:** gated research track ([ADR-015](architecture/decisions.md#adr-015--independent-firmware-is-a-gated-research-track)); no destructive experiment without an
  owner decision and a tested recovery path.

## RISK-013 — Licensing and redistribution

- **Description:** Copying unlicensed code (PSPStates) or CC BY-SA code (intraFont) into a
  GPL project, or shipping Sony assets.
- **Status Mitigated:** [ADR-001](architecture/decisions.md#adr-001--license-gpl-30-or-later), [ADR-013](architecture/decisions.md#adr-013--sony-assets-are-read-at-runtime-never-redistributed); SPDX headers on every file.

## RISK-014 — Intermittent device behaviour

- **Description:** One first touch-and-go save after a real-sleep save never completed (speed
  run 6), not reproduced since ([backlog](backlog.md#open-questions)).
- **Mitigation:** every save logs "save OK"; watch the debug log; breadcrumbs survive in P9.

## RISK-015 — Sleep or USB while the menu is open

- **Description:** The POWER switch, an auto-sleep or a USB connection while the menu holds the
  freeze, the display and the buttons.
- **Mitigation (PROPOSED):** a power-query handler closes the menu and thaws before the suspend
  proceeds; USB guard (M3).

## RISK-016 — A game that does not survive the save's sleep

- **Description:** The 3rd Birthday (ULES01513, 2026-10-17): the first save of the session (a
  real sleep) never came back; the PSP restarted into the XMB. The log ends at "save: requesting a
  real suspend" ([log](research/logs/2026-10-17-3rd-birthday-t1.log)); two of the frozen threads
  were waiting on the game's file streaming. Root cause unknown (RESEARCH REQUIRED): the game may
  not survive a sleep at all under this ISO/ARK setup, or only our frozen-game sleep.
- **Likelihood M / Impact H:** a lost session and a cold boot; no damage seen (settings intact,
  the next sessions normal).
- **Mitigation:** the isolation protocol M13 in the [testing guide](testing.md#m13); a
  per-game refusal list once the cause is known; the compatibility matrix records it.
  2026-10-19: a frozen thread inside a card read holds the FAT driver (ADR-032); the freeze now
  waits until the card answers, which may also be this case (INFERRED): M13 again.

## RISK-017 — ARK's exit combo fired by another plugin during a save

- **Description:** ARK-4 checks its exit combos inside the controller functions it hijacks, in the
  caller's thread. Pocketwatch no longer calls them (SHL-REQ-012) and the game is frozen during a
  save or load, but another plugin's own thread (a cheat engine, an overlay) that polls them could
  start the exit if the user holds L+R+START/SELECT+DOWN during the few seconds of a save.
- **Likelihood L** (four buttons held on purpose during a save) / **Impact H** (a reboot in the
  middle of the suspend sequence).
- **Mitigation:** none possible from outside ARK; the testing guide (SH1) checks the common case.

## RISK-018 — The XMB plugin stops the XMB from starting

- **Description:** `pocketwatch_xmb.prx` patches `vsh_module` and `sysconf_plugin` as they start
  (ADR-021). A wrong site on a firmware or ARK-4 build other than the one the patterns come from,
  or a plugin that patches the same sites differently, could crash the XMB.
- **Likelihood L** (the patterns are ARK-4's own for 6.60/6.61; any missing or inconsistent site
  leaves the module untouched) / **Impact M** (the XMB does not start until the plugin is off; no
  data is touched).
- **Mitigation:** the checks above; `xmb.log`; recovery by holding START at boot (ARK-4 loads no
  plugins) and removing the `VSH` line; installed only with `deploy -EnableXmb`.

## RISK-019 — The translation pool is nearly full

- **Description:** translations are copied into a 4 KB kernel-RAM pool (ADR-018). With the
  Battery and Play Time strings Russian takes 3.7 KB and Ukrainian 3.6 KB; a file that does not
  fit is refused whole and the hub falls back to English.
- **Likelihood M** (every new feature adds strings) / **Impact L** (English, not a failure).
- **Mitigation:** the host test loads every lang file through the same pool; next lever: raise the
  pool (kernel RAM, RISK-005) or keep the texts of rarely seen pages in P9.

## RISK-020 — A game started from Home is in another setup than from the XMB

- **Description:** Home starts an image the way ARK-4's menu does (Inferno, boot configuration
  3); the XMB's own start may use another configuration. A state's fingerprint covers the memory
  layout and the modules' blocks (`pw_engine_env`), so a state saved after one kind of start may
  be refused after the other ("saved with another setup").
- **Likelihood M** / **Impact M** (a refusal with a message, nothing lost).
- **Mitigation:** testing guide HM4 (save after a start from the XMB, load after a start from
  Home, and back); if they differ, Home starts images the XMB's way (vshctrl's `umdLoadExec`).

## RISK-021 — Starting in Home depends on ARK-4's config layout and boot order

- **Description:** the XMB plugin writes ARK-4's `ARKConfig` (ADR-026). Another ARK-4 build with
  another layout would get a wrong field written; a build that decides the launcher after the
  plugins would ignore it; a Home that crashes at start would come back at every power-on.
- **Likelihood L** (the layout is checked: magic, folder, device, empty launcher and recovery;
  ARK-4's xmbctrl writes the same struct the same way) / **Impact H** (a PSP that does not reach
  the XMB by itself).
- **Mitigation:** `pw_boot_ark_check` before any write, only the launcher field changes; the start
  guard (`boot-pending`) gives the XMB after one failed start; L at power-on gives the XMB, START
  at power-on loads no plugins (ARK-4), R opens ARK-4's recovery; a cold boot rebuilds
  `ARKConfig` from ARK-4's loader (INFERRED from `loader/`, the config is made at each cold boot).
  Device test HM5-HM9.

## RISK-022 — The XMB plugin is at its 64 KB budget

- **Description:** `vsh/pocketwatch_xmb.prx` loads 65 121 of its 65 536 bytes (2026-10-18). Every
  new feature of the XMB side (rows, strings, the start screen) has to fit; `make check` fails
  the build when it does not.
- **Likelihood H** / **Impact M** (a feature has to wait or something has to go).
- **Mitigation:** Home's strings moved to their own tier (`PW_I18N_HOME`, ADR-028), the boot log
  keeps ~5 lines; next candidates: the battery history buffer, the English strings of rows
  only Home shows, or a measured raise of the budget (RESEARCH REQUIRED: how much user memory
  the XMB leaves with ARK-4's plugins). 2026-10-19: 65 173 bytes - Home's settings in
  `config.txt` (`sort`, `slideshow`) are parsed by the shared `core/config.c`, so the XMB plugin
  keeps them when its page writes the file; a Home-only setting should go into a file of Home's
  own (like `wallpaper.txt`) rather than into `config.txt`.

## RISK-023 — A frozen game thread holds a lock other than the card's

- **Description:** ADR-032 makes sure no frozen game thread holds the FAT driver. A thread can
  also be suspended inside other kernel calls that hold a lock while they wait: the module
  manager (a game loading a module), the Media Engine (audio, video), the network. Whatever of
  ours needs that lock while the game is frozen waits for ever.
- **Likelihood L** (the hub and the engine use only the card, the display and the GE while the
  game is frozen) / **Impact H** (a hang, a cold boot).
- **Mitigation:** nothing of ours loads modules or uses the Media Engine while the game is
  frozen; new work that does needs the same answer-or-retry check for its lock.

## RISK-024 — Another plugin holds a device Home does not know

- **Description:** Home's USB takes the port from RemoteJoyLite's device (PID `0x1C9`, ADR-034).
  A plugin with another device (a camera, a gamepad bridge) keeps the port; the storage driver
  stays "in progress" and USB Connection fails in Home while the XMB's may work.
- **Likelihood L** (RemoteJoyLite, usbhostfs and PSPLink use `0x1C9`) / **Impact M** (no USB from
  Home; the XMB's still works).
- **Mitigation:** the failure names another plugin as the cause on the screen; `home.log` gives
  the bus's state before (`before ...`). A PID seen there can be added to the deactivation.

## RISK-025 — Home deletes saved data itself

- **Description:** Home's own Saved Data page (ADR-036) removes a save's files and folder. A
  wrong folder name or path would remove something else, and a deleted save cannot be had back.
- **Likelihood L** / **Impact H** (a game's progress lost).
- **Mitigation:** only folders listed from `PSP/SAVEDATA` that start with the game's disc ID;
  names with `/` or a leading `.` refused; only the files directly in that folder; a question
  (Delete / Cancel) every time; test HM23 on a copy of a save first.

## RISK-026 — Home sets the PSP's clock wrong

- **Description:** Home writes the clock (ADR-041): put back after it was lost, or from an SNTP
  answer. A wrong value - a misread file time, a bad answer, a tick in the wrong unit - would leave
  the PSP's clock wrong (saves, play time, screenshots carry it).
- **Likelihood L** / **Impact M** (fixed in the XMB's Settings → Date and Time; nothing is lost).
- **Mitigation:** only a clock the kernel refuses or that reads before 2016 is put back, and only
  to a file time from 2016 on; SNTP answers checked (server mode, stratum 1-15, synchronised,
  2016 or later); the firmware's own setter (`sceRtcSetCurrentTick`, as NTPSP and System Settings
  do), never the syscon directly; the clock read back after every write and logged
  (`home clock:` in `home.log`); test HM29.

## RISK-027 — The overlay module's thread runs through a save or a load

- **Description:** the game overlay (ADR-043) is a kernel module whose thread is not frozen with
  the game; it keeps writing into the frame through the states engine's freeze, and a load puts
  back a kernel image made with or without the module.
- **Likelihood L** / **Impact M** (a state that does not load, at worst a game that hangs; the
  overlay is off by default and only loaded while turned on).
- **Mitigation:** the module touches only the display's frame (VRAM, part of every state anyway)
  and its own memory; no card writes, no hooks, no interrupts; a load restores the whole kernel
  image, the module's thread with it (INFERRED). Device test PF2 before it is recommended for
  games with states; until then the testing guide says to turn it off for state tests.

## RISK-028 — Home writes the PSP's network connections

- **Description:** a new Wi-Fi connection (ADR-044) is written into the registry on flash1 through
  the firmware's netparam calls; a wrong value (the security, the key's type) makes a connection
  that does not connect, a wrong call could spoil the XMB's list of connections.
- **Likelihood L** / **Impact M** (the XMB's Network Settings shows, fixes and deletes them; no
  other setting is touched).
- **Mitigation:** only when the owner saves a network; only the firmware's own calls (what its
  `pspnet_ap_dialog_dummy` does), never registry keys by hand; a failed step deletes the connection
  it began; that network's earlier one replaced, never another; WEP not made here; a test logs what
  an XMB-made connection holds to compare (HM33).

## RISK-029 — The brightness boost fights another brightness plugin, or costs battery

- **Description:** the boost (ADR-045) sets the backlight whenever the system changes it; another
  plugin doing the same (bright3.prx is on the owner's PSP for VSH, GAME and POPS) would put its
  own back - a flicker; the brightest level draws more power.
- **Likelihood M** / **Impact L**.
- **Mitigation:** off by default; given up after 4 put-backs within 400 ms of ours (logged in Home:
  `home brightness:`), a change while SCREEN is pressed not counted; the screen off is never turned
  on. bright3 is turned off in the owner's PLUGINS.TXT since Own Steps took its place (ADR-047).
