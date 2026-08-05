cmake_minimum_required(VERSION 3.15)
project(AmpForge)

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
