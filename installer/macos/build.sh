#!/usr/bin/env bash
# Builds AmpForge's macOS plugins natively (no cross-compile needed, unlike
# installer/windows/build.sh - DPF targets macOS directly from a macOS
# host/runner via its own CoreAudio/CoreMIDI/Cocoa backends). Mirrors
# installer/linux/docker-entrypoint.sh's plain CMake + Ninja build; this is
# what CI (.github/workflows/macos-build.yml) uses too, so a local run here
# reproduces exactly what a release build does.
#
# Output lands in build-macos/bin, ready for installer/macos/package.sh.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/${BUILD_DIR:-build-macos}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR"
