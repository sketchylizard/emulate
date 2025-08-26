#!/usr/bin/env bash
set -euo pipefail

# Allow overriding binary, e.g. CLANG_TIDY=clang-tidy-18
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
BUILD_DIR="${1:-build/clang/debug}"   # path with compile_commands.json

if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  echo "compile_commands.json not found in $BUILD_DIR. Configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON"
  exit 2
fi

# Tidy tracked C/C++ sources (you can scope this further to src/ if desired)
mapfile -d '' FILES < <(git ls-files -z -- \
  '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.hxx')

# Run clang-tidy with compile_commands
# (xargs batches files; -p/--path is auto-detected via -p <builddir> style in older versions,
#  but new clang-tidy prefers -p=<builddir>)
printf '%s\0' "${FILES[@]}" \
  | xargs -0 -n 32 -P "$(command -v nproc >/dev/null && nproc || sysctl -n hw.ncpu)" \
    "$CLANG_TIDY" -p="$BUILD_DIR"
