#!/usr/bin/env bash
# Builds AmpForge's Linux plugins inside the fixed Ubuntu 22.04 image
# (installer/linux/Dockerfile) instead of against whatever glibc the host
# happens to have - see that Dockerfile's comment for why that matters.
# This is what CI (.github/workflows/linux-build.yml) uses too, so a local
# run here reproduces exactly what a release build does.
#
# Output lands in build-docker/bin, the same layout a native `ninja` build
# leaves in build/bin, so it can be packaged the same way:
#   BUILD_DIR=build-docker installer/linux/package.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE_TAG="ampforge-linux-build"

docker build -t "$IMAGE_TAG" -f "$REPO_ROOT/installer/linux/Dockerfile" "$REPO_ROOT"

# Runs as the calling user (not root) so build-docker/ ends up owned by you,
# not root, on the host; HOME=/tmp because that uid has no /etc/passwd entry
# inside the container, and CMake/git otherwise fall back to an unwritable
# HOME of "/" for their cache/config files.
docker run --rm \
    -v "$REPO_ROOT:/workspace" \
    -u "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e BUILD_DIR="${BUILD_DIR:-build-docker}" \
    "$IMAGE_TAG"
