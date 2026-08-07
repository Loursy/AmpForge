dpf_add_plugin(ampforge_main
  TARGETS jack lv2 vst3 clap
  FILES_DSP
    ChainPlugin.cpp
  FILES_UI
    ChainUI.cpp)

target_include_directories(ampforge_main PUBLIC "." "../../core")
