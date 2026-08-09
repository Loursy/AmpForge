dpf_add_plugin(ampforge_main
  TARGETS jack lv2 vst3 clap
  USE_FILE_BROWSER
  FILES_DSP
    ChainPlugin.cpp
  FILES_UI
    ChainUI.cpp)

target_include_directories(ampforge_main PUBLIC "." "../../core")

# dpf_add_plugin() compiles FILES_DSP once into an ampforge_main-dsp target
# that every format target (jack/lv2/vst3/clap) links against, so NAM only
# gets compiled once here, not once per format - but despite its name,
# dpf__add_static_library() (DPF-plugin.cmake) makes ampforge_main-dsp a
# real STATIC archive, not an OBJECT library. nam_core's OBJECT-ness (see
# the root CMakeLists.txt's comment on it) only protects its self-
# registering translation units (NAM/container.cpp etc.) from being pruned
# by the *first* archiving step - their .o files still end up bundled
# inside libampforge_main-dsp.a itself, which is a second, ordinary STATIC
# archive. When *that* archive is linked into each format's final .so, the
# exact same "linker only pulls in .o files it can prove are needed"
# pruning happens all over again at this outer layer, silently dropping
# every self-registering NAM .o a second time - verified by adding trace
# logging to NamBlock::loadModel() and watching it fail with "No config
# parser registered for architecture: SlimmableContainer" (A2 models; other
# architectures, e.g. LSTM, aren't referenced by name in AmpForge code
# either, so they'd fail identically depending on which their .o happens to
# land relative to symbols that *are* referenced) despite the build itself
# succeeding cleanly and nam_core being wired up correctly.
#
# CMake 3.24's LINK_LIBRARY WHOLE_ARCHIVE feature exists for exactly this -
# it forces every .o a library contains into the final link regardless of
# whether anything visibly references it, and (unlike hand-written
# -Wl,--whole-archive/-Wl,--no-whole-archive) CMake guarantees the platform-
# correct flags land bracketing just this one archive, not stray onto
# whatever static libraries happen to link nearby.
#
# It has to be applied at each *consumer* of ampforge_main-dsp, not
# declared once on ampforge_main-dsp's own INTERFACE pointing at itself -
# tried that first, and CMake silently drops a library's LINK_LIBRARY
# reference to itself (verified: it never made it into the generated
# build.ninja at all, whole-archive or otherwise). So this names each
# format target dpf_add_plugin() creates above by hand instead - jack's
# executable is "${NAME}-jack" (dpf__build_jack, DPF-plugin.cmake), the
# other three are "${NAME}-<format>" (dpf__build_lv2/vst3/clap). Guarded
# with if(TARGET ...) since dpf_add_plugin() only creates the targets
# actually listed in TARGETS above, and to stay harmless if a future DPF
# version renames one of them.
target_link_libraries(ampforge_main-dsp PUBLIC nam_core)
if(CMAKE_VERSION VERSION_LESS "3.24")
  message(WARNING
    "CMake ${CMAKE_VERSION} < 3.24: can't apply the LINK_LIBRARY "
    "WHOLE_ARCHIVE workaround for NAM's self-registering architectures "
    "(NAM/container.cpp etc.) - .nam files using those architectures "
    "(e.g. the A2 SlimmableContainer) will fail to load at runtime with "
    "\"No config parser registered for architecture: ...\" even though "
    "this build succeeds. Upgrade CMake to 3.24+ to fix.")
else()
  foreach(_ampforge_main_consumer IN ITEMS
      ampforge_main-jack ampforge_main-lv2 ampforge_main-vst3 ampforge_main-clap)
    if(TARGET ${_ampforge_main_consumer})
      target_link_libraries(${_ampforge_main_consumer} PRIVATE
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,ampforge_main-dsp>")
    endif()
  endforeach()
endif()
