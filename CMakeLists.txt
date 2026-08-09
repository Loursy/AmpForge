# 3.17, not something lower - GIT_SUBMODULES_RECURSE below (on the DPF
# FetchContent_Declare) was only added in CMake 3.17. Understating this let
# an older CMake (e.g. 3.16.3, still what Ubuntu 20.04's apt ships) silently
# mis-parse that unrecognized keyword instead of refusing to configure -
# the actual symptom was a confusing "could not create work tree dir ''"
# from the FetchContent git clone step, not a clear version error.
cmake_minimum_required(VERSION 3.17)
project(AmpForge)

# core/*.hpp (shared by every plugin, including the 01-gain/02-amp/03-screamer
# scaffolding ones that don't link nam_core and so don't otherwise get a C++
# standard bumped onto them - see nam_core's own cxx_std_20 requirement
# below) uses std::clamp throughout, a C++17 addition to <algorithm> - with
# no minimum standard set anywhere, every target was silently compiling
# against whatever the host compiler's own unstated default happens to be.
# That held up only by accident: GCC defaulted to C++17 from GCC 11 onward,
# so it went unnoticed on any GCC 11+ toolchain (including this repo's own
# Docker build image and every dev machine tested on so far) - GCC 10 (still
# Debian 11's default `gcc`, and prior to that, plain std::clamp isn't even
# declared) fails outright with "'clamp' is not a member of 'std'" despite
# <algorithm> being included, since the feature itself is version-gated
# inside libstdc++'s own header, not just missing an include. Setting this
# explicitly removes the dependency on the compiler's default entirely.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# --- Fetch NeuralAmpModelerCore the same way, for the chain plugin's NAM block ---
# NeuralAmpModelerCore itself uses two git submodules (Dependencies/eigen and
# Dependencies/AudioDSPTools), but only Eigen is actually used by the core DSP
# library (AudioDSPTools is only needed by its own render/benchmark tools, which
# we don't build) - so GIT_SUBMODULES is scoped to just that one path instead of
# a full recursive submodule checkout.
#
# Deliberately FetchContent_Populate() rather than FetchContent_MakeAvailable():
# MakeAvailable would add_subdirectory() into NeuralAmpModelerCore's own
# CMakeLists.txt, which unconditionally builds its tools/ executables - one of
# which (render) needs Dependencies/AudioDSPTools, which we intentionally didn't
# fetch. We don't use NAM's own CMake build at all (see the manual nam_core
# target below), so Populate() - fetch the source, run none of its CMake - is
# exactly what's needed here.
FetchContent_Declare(
  NeuralAmpModelerCore
  GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
  GIT_TAG        3cde95c354d5ba6da01316cad90b05cfc4855053
  GIT_SUBMODULES "Dependencies/eigen"
)
FetchContent_GetProperties(NeuralAmpModelerCore)
if(NOT neuralampmodelercore_POPULATED)
  FetchContent_Populate(NeuralAmpModelerCore)
endif()

# NeuralAmpModelerCore doesn't provide its own CMake library target anyway -
# its own tools/CMakeLists.txt builds its executables by globbing
# NAM/*.cpp + NAM/*/*.cpp directly, so we do the same and wrap it in a lib
# the chain plugin links against.
#
# OBJECT, not STATIC: each supported architecture (WaveNet, ConvNet, LSTM,
# Linear, the A2 SlimmableContainer, ...) self-registers its .nam config
# parser via a file-scope static object's constructor (see e.g.
# NAM/container.cpp's `_register_SlimmableContainer`) - nothing else in the
# program calls a function from that translation unit directly. A real
# STATIC library only pulls in the .o files a linker pass finds it actually
# needs to resolve a symbol, so every one of those self-registering .o files
# would get silently dropped and their models would fail to load at runtime
# ("No config parser registered for architecture: ...") despite the build
# succeeding cleanly. OBJECT libraries don't have that problem - CMake
# passes their .o files straight through to whatever links this target,
# the same as if they were that target's own sources, so nothing gets
# eliminated for being "unreferenced".
file(GLOB NAM_SOURCES_TOP "${neuralampmodelercore_SOURCE_DIR}/NAM/*.cpp")
file(GLOB NAM_SOURCES_SUB "${neuralampmodelercore_SOURCE_DIR}/NAM/*/*.cpp")
add_library(nam_core OBJECT ${NAM_SOURCES_TOP} ${NAM_SOURCES_SUB})
target_include_directories(nam_core PUBLIC
  "${neuralampmodelercore_SOURCE_DIR}"
  "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
  "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann")
# AmpForge's DSP pipeline is float, not NAM's default double.
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT)
# NAM requires C++20 - scoped to this target (and whatever links it) only, so
# plugins 01-03 stay on the compiler's default standard.
target_compile_features(nam_core PUBLIC cxx_std_20)
# nam_core is a static library that ends up linked into several different
# final shared objects (one .so per plugin format) as well as the jack
# standalone executable. GCC's default TLS model optimization guesses
# "local-exec" for internal-linkage thread_local variables (NAM/dsp.cpp has
# one, gPrewarmOnResetDefault), which only works when the variable ends up
# directly in the final executable - linking that into a -shared target then
# fails ("local-exec is incompatible with -shared"). global-dynamic always
# works regardless of the final link target, at a small TLS-access cost that
# doesn't matter here (it's only touched when changing the prewarm-on-reset
# default, never per-sample on the audio thread).
if(NOT MSVC)
  target_compile_options(nam_core PRIVATE -ftls-model=global-dynamic)
endif()
# nam_core also needs to be position-independent: without -fPIC, GCC assumes
# internal-linkage code (like the thread_local above) will only ever end up
# in the main executable and silently downgrades it to the local-exec TLS
# model regardless of -ftls-model - which is what actually caused the link
# failure above (-ftls-model alone wasn't enough).
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_subdirectory(plugins/01-gain)
add_subdirectory(plugins/02-amp)
add_subdirectory(plugins/03-screamer)
add_subdirectory(plugins/04-chain)
