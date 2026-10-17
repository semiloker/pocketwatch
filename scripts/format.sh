#!/usr/bin/env bash
# Format our C sources with clang-format (CI pins 18.1.8: `pipx install clang-format==18.1.8`).
#
#   scripts/format.sh          rewrite files in place
#   scripts/format.sh --check  fail if any file is not formatted
#
# Third-party code under plugin/lib/ is left untouched.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

cd "$ROOT"
mapfile -t files < <(git ls-files 'plugin/src/*.c' 'plugin/src/*.h' 'vsh/src/*.c' 'vsh/src/*.h' 'tests/*.c' 'tests/*.h')

if [ "${1:-}" = "--check" ]; then
    "$CLANG_FORMAT" --dry-run --Werror "${files[@]}"
    echo "format OK (${#files[@]} files)"
else
    "$CLANG_FORMAT" -i "${files[@]}"
fi
