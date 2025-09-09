#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

# --- Config (change if needed) ---
$VenvDir = ".venv"
$MSVCProfile = $env:MSVC_PROFILE; if (-not $MSVCProfile) { $MSVCProfile = "msvc" }
$ClangProfile = $env:CLANG_PROFILE; if (-not $ClangProfile) { $ClangProfile = "clang20" }
$OutMsvcDbg = "build/msvc/debug"
$OutMsvcRel = "build/msvc/release"
$OutClangDbg = "build/clang/debug"
$OutClangRel = "build/clang/release"
# ---------------------------------

Write-Host "[INFO] Ensure you're in a Developer PowerShell (so cl/link SDK are available)."

if (Test-Path $VenvDir) {
  Remove-Item -Recurse -Force $VenvDir
}
Write-Host "[INFO] Creating venv..."
py -3 -m venv $VenvDir --upgrade-deps

# Activate for this process
$activate = Join-Path $VenvDir "Scripts\Activate.ps1"
. $activate

Write-Host "[INFO] Installing Conan..."
python -m pip install --upgrade pip wheel
python -m pip install "conan>=2.0.0"
conan --version | Write-Host

function Do-ConanInstall([string]$Profile, [string]$BType, [string]$OutDir) {
  Write-Host "[INFO] conan install: profile=$Profile, type=$BType, out=$OutDir"
  conan install . -pr:h=$Profile -pr:b=$Profile -s:h build_type=$BType --build=missing --output-folder=$OutDir
}

# MSVC (if profile exists)
try { conan profile show $MSVCProfile | Out-Null; $hasMsvc = $true } catch { $hasMsvc = $false }
if ($hasMsvc) {
  Do-ConanInstall $MSVCProfile "Debug"   $OutMsvcDbg
  Do-ConanInstall $MSVCProfile "Release" $OutMsvcRel
} else {
  Write-Host "[WARN] Conan profile '$MSVCProfile' not found; skipping msvc."
}

# clang on Windows (optional)
try { conan profile show $ClangProfile | Out-Null; $hasClang = $true } catch { $hasClang = $false }
if ($hasClang) {
  Do-ConanInstall $ClangProfile "Debug"   $OutClangDbg
  Do-ConanInstall $ClangProfile "Release" $OutClangRel
} else {
  Write-Host "[INFO] No clang profile on Windows; skipping."
}

Write-Host ""
Write-Host "[INFO] Done. To activate later, run: . .\.venv\Scripts\Activate.ps1"
Write-Host "Build examples:"
Write-Host "  cmake --preset msvc-debug   ; cmake --build --preset msvc-debug"
Write-Host "  cmake --preset clang-debug  ; cmake --build --preset clang-debug"
