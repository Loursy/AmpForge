#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// This is the main AmpForge plugin - a single plugin that hosts a chain
// of modules (Screamer, Amp, and later Delay/Reverb/...) internally,
// similar to how Guitar Rig / BIAS FX work: one plugin instance, one
// internal signal chain.
#define DISTRHO_PLUGIN_BRAND   "AmpForge"
#define DISTRHO_PLUGIN_NAME    "AmpForge"
#define DISTRHO_PLUGIN_URI     "https://github.com/Loursy/ampforge"
#define DISTRHO_PLUGIN_CLAP_ID "com.ampforge.main"
#define DISTRHO_PLUGIN_CLAP_FEATURES "audio-effect", "distortion", "mono"

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
// Needed for Plugin::getTimePosition(), which Delay/Tremolo/Chorus's tempo
// sync (see ChainParameters.hpp's kSyncDivisions) reads the host's BPM
// from in ChainPlugin::run().
#define DISTRHO_PLUGIN_WANT_TIMEPOS  1
// State is used to carry the Cabinet block's loaded impulse-response file
// path - not representable as a plain automatable float parameter, but
// still needs to travel with the DAW session (see ChainPlugin.cpp's
// initState()/getState()/setState()).
#define DISTRHO_PLUGIN_WANT_STATE      1
#define DISTRHO_PLUGIN_WANT_FULL_STATE 1
// Needed for UI::requestStateFile() (the Cabinet's "Load IR File..." and
// the preset Import button both use it) to actually open a dialog instead
// of silently no-op'ing - see ChainUI.cpp's handleControlBarClick() and
// drawFileLoaderButton().
#define DISTRHO_UI_FILE_BROWSER      1
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_USE_NANOVG        1

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
