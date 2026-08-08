#!/usr/bin/env bash
# Packages a release tarball from an already-built build/bin/ (see the
# README's "Clone and build" steps - run this after `ninja` there, from
# the repo root: installer/linux/package.sh).
#
# Output: build/AmpForge-<version>-linux-x86_64.tar.gz, containing
# install.sh next to a bin/ directory with just the ampforge_main.*
# plugin formats (not the 01-gain/02-amp/03-screamer scaffolding
# plugins, which build/bin/ also has but which aren't the product).
set -euo pipefail

VERSION="0.1.0" # no CMake project version yet - keep this in sync with installer/windows/ampforge.nsi's PRODUCT_VERSION by hand.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_BIN="$REPO_ROOT/build/bin"
OUT_NAME="AmpForge-${VERSION}-linux-x86_64"
STAGE_DIR="$REPO_ROOT/build/${OUT_NAME}"

if [[ ! -d "$BUILD_BIN" ]]; then
    echo "error: $BUILD_BIN not found - build the project first (see README)." >&2
    exit 1
fi

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/bin"

for item in ampforge_main.vst3 ampforge_main.clap ampforge_main.lv2 ampforge_main; do
    if [[ -e "$BUILD_BIN/$item" ]]; then
        cp -r "$BUILD_BIN/$item" "$STAGE_DIR/bin/"
    fi
done

cp "$REPO_ROOT/installer/linux/install.sh" "$STAGE_DIR/"
chmod +x "$STAGE_DIR/install.sh"
cp "$REPO_ROOT/LICENSE" "$STAGE_DIR/"

tar -C "$REPO_ROOT/build" -czf "$REPO_ROOT/build/${OUT_NAME}.tar.gz" "$OUT_NAME"
rm -rf "$STAGE_DIR"

echo "Wrote build/${OUT_NAME}.tar.gz"
