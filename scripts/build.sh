#!/usr/bin/env bash
# Build Pocketwatch (plugin/ for games, vsh/ for the XMB, home/ the Home application) with PSPDEV.
#
#   scripts/build.sh [make-target] [version] [debug: 1 = debug build (default), 0 = release]
#
# PSPDEV defaults to ~/pspdev when it is not already set (the pspdev Docker image sets it).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

if ! command -v psp-config >/dev/null 2>&1; then
    echo "error: PSP toolchain not found (PSPDEV=$PSPDEV). See https://pspdev.github.io/" >&2
    exit 1
fi

args=("${1:-all}")
if [ -n "${2:-}" ]; then
    args+=("VERSION=$2")
fi
if [ -n "${3:-}" ]; then
    args+=("DEBUG=$3")
fi

make -C "$ROOT/plugin" "${args[@]}"
# The XMB plugin (vsh/): the same target and version; it has no debug profile.
vargs=("${1:-all}")
if [ -n "${2:-}" ]; then
    vargs+=("VERSION=$2")
fi
make -C "$ROOT/vsh" "${vargs[@]}"
# Pocketwatch Home (home/): the same targets ("check": imports and its budgets).
hargs=("${1:-all}")
if [ -n "${2:-}" ]; then
    hargs+=("VERSION=$2")
fi
make -C "$ROOT/home" "${hargs[@]}"
# The performance overlay for games (perf/, ADR-043): a kernel module of its own, loaded only when
# Home turns its PLUGINS.TXT line on.
pargs=("${1:-all}")
if [ -n "${2:-}" ]; then
    pargs+=("VERSION=$2")
fi
make -C "$ROOT/perf" "${pargs[@]}"
