# Unit tests for the header-only DSP in core/.
#
# These deliberately don't link DPF or nam_core: everything under core/ is
# header-only and free of plugin-framework dependencies, so the whole
# suite builds in a couple of seconds and runs anywhere a compiler does -
# including CI, where the point is to catch a broken DSP change long
# before anyone builds a plugin and loads it into a DAW.

add_executable(ampforge_tests
    main.cpp
    test_biquad.cpp
    test_blocks.cpp
    test_effectchain.cpp
    test_pitchdetector.cpp)

target_include_directories(ampforge_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../core")

target_compile_features(ampforge_tests PRIVATE cxx_std_17)

# The Windows CI cross-compiles this and runs it under Wine, where the MinGW
# runtime DLLs (libgcc_s_seh-1.dll, libstdc++-6.dll) aren't on the loader
# path: the run dies with "Importing dlls ... failed" before a single test
# executes. Static linking makes the binary self-contained, so it runs from
# any directory - on the CI runner and on a plain Windows machine alike.
# Scoped to the tests on purpose: the plugins keep their normal dynamic
# runtime, which is what a host expects to load.
if (MINGW)
    target_link_options(ampforge_tests PRIVATE -static)
endif ()

add_test(NAME ampforge_core_tests COMMAND ampforge_tests)
