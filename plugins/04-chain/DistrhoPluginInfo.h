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

// No UI yet - we're validating that the chain (Screamer -> Amp) works
// correctly and sounds right before we build the reorderable GUI.
#define DISTRHO_PLUGIN_HAS_UI        0
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    1
#define DISTRHO_PLUGIN_NUM_OUTPUTS   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS 0
#define DISTRHO_PLUGIN_WANT_STATE    0

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
