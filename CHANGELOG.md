# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Continue a game from a state at its start: a `continue.txt` next to the plugin
  (`<disc ID> <slot>`) loads that slot as soon as the game runs, once; the XMB's "Continue from
  Save State" will write it.
- States on the Memory Stick: five slots per game as native saved data
  (`PSP/SAVEDATA/PW<DISCID><NN>`, hashed PARAM.SFO, the game's frame as ICON0/PIC1), listed and
  deletable in the XMB's Saved Data Utility; a state list in the Saved Data Utility's layout with
  overwrite and delete confirmations; loads after quitting the game or a full power-off.
- A state loads only into the setup that made it (disc ID, ISO path and size, the plugin file,
  model, memory layout); after every load from a slot the restored kernel checks that it sees the
  card as it is now before anything writes to it.
- Guards: save and load are refused, with the reason, on low battery without the AC adaptor, with
  USB mass storage on, with the wireless LAN in use, under ARK's high memory, and with Custom Home
  or PSPStates loaded. The hub survives a sleep with the POWER switch.
- HOME opens Pocketwatch's hub instead of the system HOME screen: Resume Game, Save State, Load
  State, Quit Game, in the XMB's look - the month's background and wave, the system font, the
  XMB's own icons and button pictures, clock and battery in the user's formats.
- Quit Game through the game's own exit path, with ARK-4's exit as the fallback.
- Savestate engine: freeze the game, pack game RAM / eDRAM / scratchpad with LZ4, capture the
  kernel during a suspend (touch-and-go after the first save of a session), roll it back on load
  with the flash drivers rebuilt and the IPL's seeds put back.
- Two more looks for the menu, chosen in Settings → Style: Modern (the game blurred behind round
  buttons, cards and switches) and Minimal (only text over the darkened game); Classic (XMB)
  stays the default.
- Settings in the hub: fast save and load on or off, the clock and battery corner, the language
  (any of the translations, or the system's); kept in `config.txt` / `language.txt`.
- The interface in the system language: French, Spanish, German, Italian, Dutch, Portuguese and
  Russian, plus Ukrainian through `language.txt`; translations are text files next to the plugin.
  Long dialog lines wrap like the system's.
- Release build switch (`scripts\build.cmd -Release`) and a device testing guide
  (`docs/testing.md`).
- Developer tooling: ARK-4 NID check, size budgets, host unit tests (ASan + UBSan), host previews
  of every screen, a documentation checker, debug combos, a hardware probe, a throughput benchmark
  and the T1 Memory Stick test.

### Changed
- Relicensed from MIT to GPL-3.0-or-later.
- Registered with runlevel `UMD` (retail games only) instead of `GAME`, which includes homebrew.

### Fixed
- The menu ran at a very low frame rate: the background wave and the drawn shapes cost far less
  per frame now, and every movement runs by time instead of by frame.
- The state lists, dialogs and the option panel animate (glide, grow, fade, slide) like the
  system's; the Classic style uses the XMB's own battery picture (animated while charging) and the
  XMB's option-list look.
- Exiting a game hung on "Please wait...": `module_stop` now stops and joins all plugin threads.
- Quitting after a fast save hung on a black screen: the IPL's seeds are put back after every
  touch-and-go suspend.
