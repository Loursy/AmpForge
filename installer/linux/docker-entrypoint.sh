#!/usr/bin/env bash
# Runs inside the image built from installer/linux/Dockerfile, with the repo
# bind-mounted at /workspace (see docker-build.sh). Just a normal CMake +
# Ninja build - BUILD_DIR defaults to build-docker rather than build/ so it
# can't collide with (or get confused with) a native build already sitting
# in build/ from the host's own toolchain.
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-docker}"

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR"
