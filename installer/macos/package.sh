#!/usr/bin/env bash
# Packages a release zip from an already-built <BUILD_DIR>/bin/ (run
# installer/macos/build.sh first, or use its BUILD_DIR override the same
# way). Mirrors installer/linux/package.sh's role for Linux - zip rather
# than tar.gz since that's what macOS users expect to double-click-extract
# in Finder.
#
# Output: <BUILD_DIR>/AmpForge-<version>-macos.zip, containing install.sh
# next to a bin/ directory with just the ampforge_main.* plugin formats
# (not the 01-gain/02-amp/03-screamer scaffolding plugins, which
# <BUILD_DIR>/bin/ also has but which aren't the product).
set -euo pipefail

# Defaults to a hardcoded fallback (keep this in sync with
# installer/linux/package.sh's and installer/windows/ampforge.nsi's own
# VERSION/PRODUCT_VERSION by hand - there's no CMake project version yet)
# but can be overridden, the same way BUILD_DIR above can:
#   VERSION=0.5.0 installer/macos/package.sh
# CI (.github/workflows/macos-build.yml) does exactly this on a tag push,
# deriving VERSION from the git tag itself - so a *released* zip's version
# always matches its tag with no hand-editing here required, and this
# hardcoded fallback only actually matters for an untagged local/CI build.
VERSION="${VERSION:-0.5.0}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/${BUILD_DIR:-build-macos}"
BUILD_BIN="$BUILD_DIR/bin"
OUT_NAME="AmpForge-${VERSION}-macos"
STAGE_DIR="$BUILD_DIR/${OUT_NAME}"

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

cp "$REPO_ROOT/installer/macos/install.sh" "$STAGE_DIR/"
chmod +x "$STAGE_DIR/install.sh"
cp "$REPO_ROOT/LICENSE" "$REPO_ROOT/THIRD-PARTY-NOTICES.md" "$STAGE_DIR/"

(cd "$BUILD_DIR" && zip -r -q "${OUT_NAME}.zip" "$OUT_NAME")
rm -rf "$STAGE_DIR"

echo "Wrote ${BUILD_DIR#"$REPO_ROOT/"}/${OUT_NAME}.zip"
