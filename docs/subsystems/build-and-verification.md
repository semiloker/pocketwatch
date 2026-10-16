# Build and verification — specification

| | |
|---|---|
| Area code | `BLD` |
| Status | Done |
| Code | [`scripts/`](../../scripts/), [`plugin/Makefile`](../../plugin/Makefile), [`tools/check_prx.py`](../../tools/check_prx.py), [`tools/nids/`](../../tools/nids/), [`tests/`](../../tests/), [`.github/workflows/build.yml`](../../.github/workflows/build.yml), [`tools/check_docs.py`](../../tools/check_docs.py) |

## A. Overview

- **Purpose:** reproducible builds on Windows (toolchain in WSL) and Linux, static checks that catch
  what would only fail on the device (unresolved NIDs, size), host unit tests, formatting, CI and
  releases, deployment to the USB-connected PSP, documentation consistency.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `BLD-REQ-001` | `scripts\build.cmd` (Windows) and `bash scripts/build.sh` (Linux/WSL) build `plugin/pocketwatch.prx`; `-Target check` / `check` runs the static checks. | P0 | Done | CI, local |
| `BLD-REQ-002` | `make check` fails when an import NID from a randomized library is neither in ARK-4's table (`tools/nids/ark4_660.tsv`) nor in `tools/nids/allowlist.tsv` with evidence. | P0 | Done | CI |
| `BLD-REQ-003` | `make check` fails when the PRX exceeds `MAX_PRX_SIZE` (112 KB) or the loaded sections exceed 128 KB. | P0 | Done | CI |
| `BLD-REQ-004` | Host tests (`make -C tests`) run every portable module under ASan + UBSan. | P0 | Done | CI (53 tests) |
| `BLD-REQ-005` | `scripts/format.sh --check` enforces clang-format 18.1.8. | P1 | Done | CI |
| `BLD-REQ-006` | Every push runs host tests, format check, PSP build and `make check`; a `v*` tag publishes a release zip. | P1 | Done | GitHub Actions |
| `BLD-REQ-007` | `scripts\deploy.cmd [-Enable]` copies the PRX to the PSP and registers it in `PLUGINS.TXT` with runlevel `UMD`. | P1 | Done | local |
| `BLD-REQ-008` | `python tools/check_docs.py` verifies internal links and anchors, requirement/ADR/risk IDs, index coverage and the `CLAUDE.md` import. | P1 | Done (2026-10-16) | local; CI step PROPOSED |
| `BLD-REQ-009` | The release profile (`DEBUG=0`) builds and passes `make check`. | P1 | Done (manual) | CI step PROPOSED |

## C. Architecture and integration

`plugin/Makefile` (PSPSDK build.mak, `USE_KERNEL_LIBS = 1`, `-Os -Werror`, `DEBUG ?= 1`) →
`check_prx.py` (parses the ELF/PRX imports and sections) → CI. Tests: `tests/Makefile` builds the
portable sources listed there with `tests/platform_host.c`. Version string from `git describe`.

## D-E. UX, references

Not applicable.

## F. Implementation plan

PROPOSED: add `python3 tools/check_docs.py` and a `DEBUG=0` build + check to CI.

## G. Testing and acceptance

CI green on every push; locally the four commands in `CLAUDE.md`.

## H. Risks and decisions

`.ps1` files are ASCII-only (PowerShell 5.1 reads ANSI); `.cmd` wrappers bypass the execution
policy. [RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth) is enforced here.
