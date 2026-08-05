#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// This is the main AmpForge plugin - a single plugin that hosts a chain
// of modules (Screamer, Amp, and later Delay/Reverb/...) internally,
// similar to how Guitar Rig / BIAS FX work: one plugin instance, one
// internal signal chain.
#define DISTRHO_PLUGIN_BRAND   "AmpForge"
#define DISTRHO_PLUGIN_NAME    "AmpForge"
#define DISTRHO_PLUGIN_URI     "https://github.com/atakan/ampforge"
#define DISTRHO_PLUGIN_CLAP_ID "com.ampforge.main"

#define DISTRHO_PLUGIN_BRAND_ID  Ampf
#define DISTRHO_PLUGIN_UNIQUE_ID Main

// GUI is now enabled - NanoVG gives us vector-style drawing (rounded
// rects, gradients, smooth shapes) which is what we want for a modern,
// fluid pedalboard look, instead of raw pixel-level OpenGL calls.
#define DISTRHO_PLUGIN_HAS_UI        1
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    1
#define DISTRHO_PLUGIN_NUM_OUTPUTS   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS 1
#define DISTRHO_PLUGIN_WANT_STATE    0
#define DISTRHO_UI_FILE_BROWSER      0
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_USE_NANOVG        1

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
