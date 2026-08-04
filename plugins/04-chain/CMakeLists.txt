dpf_add_plugin(ampforge_main
  TARGETS jack lv2 vst3
  FILES_DSP
    ChainPlugin.cpp)

target_include_directories(ampforge_main PUBLIC "." "../../core")
