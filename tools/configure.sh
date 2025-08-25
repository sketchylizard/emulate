#!/usr/bin/env bash
set -euo pipefail
#
# Usage:
#   tools/configure.sh clang debug   # default
#   tools/configure.sh clang release
#   tools/configure.sh gcc   debug
#   tools/configure.sh gcc   release
#
# Then build:
#   ninja -C build/clang/debug
# or
#   cmake --build --preset clang-debug
#

toolchain="${1:-clang}"
cfg="${2:-debug}"

case "$toolchain" in
  clang) compiler=clang ;;
  gcc)   compiler=gcc ;;
  *) echo "toolchain must be 'clang' or 'gcc'"; exit 2 ;;
esac

case "$cfg" in
  debug|release) ;;
  *) echo "config must be 'debug' or 'release'"; exit 2 ;;
esac

# Map to compiler versions and build dirs
if [[ "$compiler" == "clang" ]]; then
  cc="clang-18"; cxx="clang++-18"
  out="build/clang/${cfg}"
  conan_compiler="clang"
  conan_version="18"
else
  cc="gcc-11"; cxx="g++-11"
  out="build/gcc/${cfg}"
  conan_compiler="gcc"
  conan_version="11"
fi

build_type=$( [[ "$cfg" == "debug" ]] && echo "Debug" || echo "Release" )

echo "[configure] toolchain=$compiler version=$conan_version cfg=$build_type out=$out"
mkdir -p "$out"

# 1) Conan: generate toolchain + deps into the build dir
#    We pin compiler + version and C++ standard for reproducibility.
conan install . \
  -s compiler="${conan_compiler}" \
  -s compiler.version="${conan_version}" \
  -s compiler.cppstd=20 \
  -s build_type="${build_type}" \
  -of="${out}" \
  --build=missing

# 2) CMake configure via preset (expects the toolchain file Conan just created)
preset="${compiler}-${cfg}"
echo "[configure] cmake --preset ${preset}"
cmake --preset "${preset}"

echo "[configure] done. Build with: ninja -C ${out}"
