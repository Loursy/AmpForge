#!/usr/bin/env bash
# Cross-compiles AmpForge's Windows plugins (VST3/CLAP/LV2) with mingw-w64
# from Linux (see cmake/toolchain-mingw64.cmake) and packages them into the
# NSIS installer (installer/windows/ampforge.nsi) - the same steps a
# developer would otherwise run by hand. CI
# (.github/workflows/windows-build.yml) runs this exact script, so a
# release built there is reproducible on a dev machine too - mirrors
# installer/linux/docker-build.sh's role for the Linux build.
#
# No standalone .exe comes out of this - the standalone app pulls in
# SDL2/RtAudio for its audio backend, and cross-compiling those for
# Windows isn't set up yet (see README's Project status section), so
# only the VST3/CLAP/LV2 formats the installer packages are built.
#
# Requires a mingw-w64 cross-compiler (x86_64-w64-mingw32-gcc/g++), CMake
# 3.24+ (plugins/04-chain/CMakeLists.txt's WHOLE_ARCHIVE workaround needs
# it), Ninja, NSIS's makensis, and Wine (DPF's LV2 build actually runs a
# small cross-compiled .exe mid-build to generate .ttl metadata - see
# cmake/toolchain-mingw64.cmake's CMAKE_CROSSCOMPILING_EMULATOR comment) -
# e.g. on Debian/Ubuntu:
#   sudo apt install mingw-w64 nsis cmake ninja-build wine
#
# VERSION defaults to installer/windows/ampforge.nsi's own hardcoded
# PRODUCT_VERSION (keep that in sync with installer/linux/package.sh's own
# VERSION default and the git tag by hand - there's no CMake project
# version yet), but can be overridden the same way package.sh's VERSION
# can:
#   VERSION=0.3.0 installer/windows/build.sh
# CI does exactly this on a tag push, deriving VERSION from the git tag
# itself, so a released installer's version always matches its tag with
# no hand-editing of ampforge.nsi required.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-windows"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/toolchain-mingw64.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

NSIS_ARGS=()
if [[ -n "${VERSION:-}" ]]; then
    NSIS_ARGS+=("-DPRODUCT_VERSION=${VERSION}")
fi
makensis "${NSIS_ARGS[@]}" "$REPO_ROOT/installer/windows/ampforge.nsi"

echo "Wrote ${BUILD_DIR#"$REPO_ROOT/"}/AmpForge-${VERSION:-<see ampforge.nsi>}-Setup.exe"
