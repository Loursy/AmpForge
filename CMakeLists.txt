cmake_minimum_required(VERSION 3.15)
project(AmpForge)

# Default to an optimized build when the generator wasn't told otherwise -
# an unset CMAKE_BUILD_TYPE compiles with no -O flags at all, which is
# nowhere near real-time-safe for the DSP in these plugins (e.g. the
# tuner's autocorrelation alone was measured to exceed a full audio-thread
# core's real-time budget unoptimized, vs ~15% of one optimized). Only
# applies to single-config generators (Makefiles/Ninja); multi-config
# generators (Xcode/Visual Studio) pick the config at build time instead.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# --- Fetch DPF automatically instead of using a git submodule ---
# This means anyone can just `git clone` this repo normally (no
# --recursive needed) and run `cmake ..` - CMake will download DPF
# itself on first configure. Pinned to a specific commit so builds
# stay reproducible.
include(FetchContent)
FetchContent_Declare(
  DPF
  GIT_REPOSITORY https://github.com/DISTRHO/DPF.git
  GIT_TAG        4238e1c7f0351bbe488d79f0899c540543ac7583
  GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(DPF)

add_subdirectory(plugins/01-gain)
add_subdirectory(plugins/02-amp)
add_subdirectory(plugins/03-screamer)
add_subdirectory(plugins/04-chain)
