#!/usr/bin/env bash
# Build the Pocketwatch PRX with the PSPDEV toolchain.
#
#   scripts/build.sh [make-target] [version]
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

make -C "$ROOT/plugin" "${args[@]}"
