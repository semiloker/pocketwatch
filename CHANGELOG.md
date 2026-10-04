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

### Changed
- Relicensed from MIT to GPL-3.0-or-later.
