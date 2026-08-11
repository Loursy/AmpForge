# CMake toolchain file for cross-compiling AmpForge to 64-bit Windows
# with mingw-w64 from Linux.
#
# Usage:
#   mkdir build-windows && cd build-windows
#   cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-mingw64.cmake \
#       -DCMAKE_BUILD_TYPE=Release
#   ninja

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config is a separate binary that ignores CMAKE_FIND_ROOT_PATH
# entirely, so without this DPF's own pkg_check_modules(SDL2 "sdl2")
# (cmake/DPF-plugin.cmake, unconditional on every platform) instead
# happily resolves to whatever SDL2/JACK/etc. .pc files the *host*
# machine has installed for its native Linux build - wrong-platform
# headers and libraries then get fed into the mingw compiler, breaking
# the build with things like "sizeof(Uint64) == 8" static-assert
# failures. The mingw sysroot has no pkgconfig directory of its own, so
# scoping pkg-config to it here just makes SDL2 (and anything else DPF
# probes for this way) correctly report "not found" when cross-compiling,
# matching what actually happens: HAVE_RTAUDIO is set unconditionally for
# WIN32 elsewhere in DPF-plugin.cmake regardless of this.
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_FIND_ROOT_PATH}/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_FIND_ROOT_PATH}")

# DPF's LV2 build (cmake/DPF-plugin.cmake, dpf__add_lv2_ttl_generator)
# actually *runs* a small cross-compiled .exe mid-build to introspect the
# plugin and emit its .ttl metadata, prefixing the invocation with
# CMAKE_CROSSCOMPILING_EMULATOR (empty by default). Set it to `wine`
# explicitly here rather than relying on the target .exe just happening
# to run anyway via a binfmt_misc registration that may or may not exist
# on a given machine (it does on a typical Arch/CachyOS wine install, but
# can't be assumed on a CI runner or another dev's box) - needs the
# `wine` package installed.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)
