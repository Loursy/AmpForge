dpf_add_plugin(ampforge_screamer
  TARGETS jack lv2 vst3
  FILES_DSP
    ScreamerPlugin.cpp)

target_include_directories(ampforge_screamer PUBLIC "." "../../core")
