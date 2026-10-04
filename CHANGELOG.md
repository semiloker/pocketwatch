# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Kernel PRX skeleton for the `GAME` runlevel that writes its version to `pocketwatch.log`.
- Build scripts for Linux/WSL and Windows, USB deploy script.
- GitHub Actions build with release packaging on `v*` tags.
- RAM ring-buffer logger with deferred flush (safe for sysevent/suspend paths).
- Boot-time self-check that logs imports the CFW could not resolve.
- `make check`: import NIDs verified against ARK-4's 6.61 resolver table, PRX size and memory budgets.
- Host unit tests (ASan + UBSan), clang-format check, `.cmd` wrappers for Windows.
- Debug build profile (`DEBUG=1`) with a hardware probe: memory partitions, modules and threads
  dumped to the log on L+R+SELECT+UP.
- `tools/sfo.py` (PARAM.SFO reader/writer, byte-exact with real saves) and the S2 savedata spike.
- PARAM.SFO writer in C (byte-identical to `tools/sfo.py`), PNG encoder, CRC-32, extra-RAM
  allocator, ARK SystemCtrl stubs, and SFO hashing through the firmware's chnnlsv.prx.
- Spike S2b (L+R+SELECT+LEFT in debug builds): verifies the hashing against ARK_01234 and writes
  hashed test saves.
- Savestate engine, in-RAM edition (E2): freeze the game, compress game RAM/eDRAM/scratchpad into
  spare RAM, capture the kernel during a real suspend; load rolls the kernel back through an
  assembly handoff and restores everything after the wake-up. Debug triggers L+R+SELECT+RIGHT/LEFT.
- Chunked FastLZ streams over a two-region arena, flash driver re-initialisation after a load.

### Changed
- Relicensed from MIT to GPL-3.0-or-later.
- Registered with runlevel `UMD` (retail games only) instead of `GAME`, which includes homebrew.

### Fixed
- Exiting a game hung on "Please wait...": `module_stop` now stops and joins all plugin threads.
