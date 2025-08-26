#!/usr/bin/env bash
set -euo pipefail

# Allow overriding the clang-format binary, e.g. CLANG_FORMAT=clang-format-18
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

# Lint only tracked source files (skip build/, externals, etc.)
mapfile -d '' FILES < <(git ls-files -z -- \
  '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.hxx')

# Show diff if any (non-fatal)
"$CLANG_FORMAT" -style=file --dry-run --Werror "${FILES[@]}"
