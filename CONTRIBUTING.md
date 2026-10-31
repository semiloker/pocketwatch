# Contributing to Pocketwatch

Bug reports with logs and tests on other PSP models or games help the most: only one PSP-3000 has
run this so far. To report something, [open an issue](https://github.com/semiloker/pocketwatch/issues/new/choose).

## Rules that are not negotiable

- **No Sony files or firmware.** No fonts, icons, RCOs, PRXs, memory dumps or screenshots of unclear
  license, in the repository or attached to issues. Pocketwatch reads what it needs at runtime from
  the user's own PSP.
- **No games.** No ISOs, and no links to where to get them.
- **Original code only.** Never copy code from PSPStates (it has no license) or intraFont (CC BY-SA,
  which is not compatible). Pocketwatch is `GPL-3.0-or-later`, and contributions are under the same
  license. Start a new source file with `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Build and test

You need the [PSPDEV toolchain](https://pspdev.github.io/) for the PSP build. The host tests need
only a C compiler.

```sh
bash scripts/build.sh            # builds every module
bash scripts/build.sh check      # imports against ARK-4's NID table, size budgets
make -C tests                    # host unit tests (ASan + UBSan)
bash scripts/format.sh --check   # clang-format 18.1.8
python tools/check_docs.py       # documentation links and IDs
```

CI runs all of these on every pull request.

## Before you change something

- Read [`docs/README.md`](docs/README.md), then the specification of the part you are changing.
  [`CLAUDE.md`](CLAUDE.md) lists the project's rules; most of them come from a failure on a real PSP,
  so find out why a rule exists before changing it.
- Find the cause of a device problem before changing code. Say what was tested on a PSP and what only
  builds.
- Portable logic must not include PSP headers, and needs a test in `tests/`.
- A change to behaviour or a decision updates the docs in the same pull request.
- Use one commit per finished piece of work, with [Conventional Commits](https://www.conventionalcommits.org/)
  messages, and name the issue (`Fixes #N`).

## Sharing a log

Logs are in `seplugins/pocketwatch/` on the Memory Stick. They name your game's file
(`game: ... from ms0:/ISO/...`), so you may want to remove that part before you post one.
