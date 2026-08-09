# Build environment for portable AmpForge Linux plugins.
#
# Building on whatever glibc the host toolchain happens to link against
# (e.g. current Arch/rolling ships 2.43) bakes that exact symbol version
# into the resulting .so - a plugin host running in an older environment,
# like Bitwig's flatpak sandbox on a freshly-released Fedora, then fails
# to load it at all ("version `GLIBC_2.43' not found"), even though the
# host distro itself is new. Symbol versioning only goes one way: a
# binary linked against glibc 2.35 loads fine on a system with 2.43, but
# not the reverse. Building inside this fixed Ubuntu 22.04 image instead
# keeps the emitted binaries' glibc requirement pinned at 2.35, which
# covers every mainstream distro and flatpak/snap runtime from the last
# few years - see installer/linux/docker-build.sh for how to invoke this.
#
# Don't drop this further to Debian 11/glibc 2.31 expecting even wider
# coverage - tried it: DPF's own WebViewImpl.cpp (always compiled in, even
# though AmpForge's UI never uses it) calls shm_open/shm_unlink/
# pthread_create/pthread_join, which only link without an explicit -lrt on
# glibc 2.34+ (that's when those symbols merged into libc itself). Older
# than that fails at link time unless DPF's own build is patched, which is
# out of scope for this Dockerfile. 22.04 is the oldest base that builds
# clean as-is.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Same package list as the README's "Install prerequisites" section for
# Debian/Ubuntu, so this image tracks the same dependency set a human
# building natively on Ubuntu would install.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates pkg-config \
        libx11-dev libxext-dev libxcursor-dev libxrandr-dev libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

COPY installer/linux/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# The actual source isn't baked into the image - docker-build.sh bind-mounts
# the repo here at run time, so editing source doesn't require an image
# rebuild, only the (much rarer) toolchain/dependency list does.
WORKDIR /workspace

ENTRYPOINT ["docker-entrypoint.sh"]
