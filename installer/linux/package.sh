#!/usr/bin/env bash
# Packages a release tarball from an already-built <BUILD_DIR>/bin/ (see the
# README's "Clone and build" steps - run this after `ninja` there, from
# the repo root: installer/linux/package.sh).
#
# BUILD_DIR defaults to build/ (a native build) but can be overridden -
# installer/linux/docker-build.sh's portable build lands in build-docker/
# instead, to avoid colliding with a native build directory built against
# the host's own (possibly newer, less portable) glibc:
#   BUILD_DIR=build-docker installer/linux/package.sh
#
# Output: <BUILD_DIR>/AmpForge-<version>-linux-x86_64.tar.gz, containing
# install.sh next to a bin/ directory with just the ampforge_main.*
# plugin formats (not the 01-gain/02-amp/03-screamer scaffolding
# plugins, which <BUILD_DIR>/bin/ also has but which aren't the product).
set -euo pipefail

# Defaults to a hardcoded fallback (keep this in sync with
# installer/windows/ampforge.nsi's PRODUCT_VERSION by hand - there's no
# CMake project version yet) but can be overridden, the same way
# BUILD_DIR above can:
#   VERSION=0.3.0 installer/linux/package.sh
# CI (.github/workflows/linux-build.yml) does exactly this on a tag
# push, deriving VERSION from the git tag itself - so a *released*
# tarball's version always matches its tag with no hand-editing here
# required, and this hardcoded fallback only actually matters for an
# untagged local/CI build.
VERSION="${VERSION:-0.2.0}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/${BUILD_DIR:-build}"
BUILD_BIN="$BUILD_DIR/bin"
OUT_NAME="AmpForge-${VERSION}-linux-x86_64"
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

cp "$REPO_ROOT/installer/linux/install.sh" "$STAGE_DIR/"
chmod +x "$STAGE_DIR/install.sh"
cp "$REPO_ROOT/LICENSE" "$REPO_ROOT/THIRD-PARTY-NOTICES.md" "$STAGE_DIR/"

tar -C "$BUILD_DIR" -czf "$BUILD_DIR/${OUT_NAME}.tar.gz" "$OUT_NAME"
rm -rf "$STAGE_DIR"

echo "Wrote ${BUILD_DIR#"$REPO_ROOT/"}/${OUT_NAME}.tar.gz"
