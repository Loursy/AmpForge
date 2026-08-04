dpf_add_plugin(ampforge_gain
  TARGETS jack lv2 vst3
  FILES_DSP
    GainPlugin.cpp)

target_include_directories(ampforge_gain PUBLIC ".")
