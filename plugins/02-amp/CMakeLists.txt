dpf_add_plugin(ampforge_amp
  TARGETS jack lv2 vst3
  FILES_DSP
    AmpPlugin.cpp)

target_include_directories(ampforge_amp PUBLIC "." "../../core")
