#!/usr/bin/env bash
set -euo pipefail

# --- Config (change if needed) ---
VENV_DIR=".venv"
# Conan profile names you created
CLANG_PROFILE="${CLANG_PROFILE:-clang20}"   # e.g. clang20 (compiler.version=20, libcxx=libc++)
GCC_PROFILE="${GCC_PROFILE:-gcc}"           # e.g. gcc (compiler.version=13+, libcxx=libstdc++11)
# Output directories
OUT_CLANG_DBG="build/clang/debug"
OUT_CLANG_REL="build/clang/release"
OUT_GCC_DBG="build/gcc/debug"
OUT_GCC_REL="build/gcc/release"
# -------------------------------

os="$(uname -s)"

echo "[INFO] Removing old venv (if any)..."
rm -rf "$VENV_DIR"

echo "[INFO] Creating virtual environment..."
python3 -m venv "$VENV_DIR" --upgrade-deps
# activate for this shell
source "$VENV_DIR/bin/activate"

echo "[INFO] Upgrading pip + installing Conan..."
python -m pip install -U pip wheel
python -m pip install "conan>=2.0.0"

echo "[INFO] Conan version: $(conan --version)"

# Helper: safe conan install
do_conan_install () {
  local profile="$1" btype="$2" out="$3"
  echo "[INFO] conan install: profile=$profile, type=$btype, out=$out"
  conan install . \
    -pr:h="$profile" -pr:b="$profile" \
    -s:h build_type="$btype" \
    --build=missing \
    --output-folder="$out"
}

case "$os" in
  Linux|Darwin)
    echo "[INFO] Detected $os → configuring clang + gcc"
    # clang (if profile exists)
    if conan profile show "$CLANG_PROFILE" >/dev/null 2>&1; then
      do_conan_install "$CLANG_PROFILE" Debug   "$OUT_CLANG_DBG"
      do_conan_install "$CLANG_PROFILE" Release "$OUT_CLANG_REL"
    else
      echo "[WARN] Conan profile '$CLANG_PROFILE' not found; skipping clang."
    fi
    # gcc (if profile exists)
    if conan profile show "$GCC_PROFILE" >/dev/null 2>&1; then
      do_conan_install "$GCC_PROFILE" Debug   "$OUT_GCC_DBG"
      do_conan_install "$GCC_PROFILE" Release "$OUT_GCC_REL"
    else
      echo "[WARN] Conan profile '$GCC_PROFILE' not found; skipping gcc."
    fi
    ;;
  *)
    echo "[ERROR] Unknown UNIX-like OS '$os'. If you're on Windows, use the PowerShell script."
    exit 2
    ;;
esac

echo
echo "[INFO] Done."
echo "[INFO] Activate the venv with:"
echo "      source $VENV_DIR/bin/activate      # bash/zsh"
echo "      source $VENV_DIR/bin/activate.fish # fish"
echo
echo "[INFO] Configure/build examples:"
echo "      cmake --preset clang-debug   && ninja -C $OUT_CLANG_DBG    || true"
echo "      cmake --preset gcc-debug     && ninja -C $OUT_GCC_DBG      || true"
