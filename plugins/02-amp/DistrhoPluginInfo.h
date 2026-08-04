#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "AmpForge"
#define DISTRHO_PLUGIN_NAME    "AmpForge Amp"
#define DISTRHO_PLUGIN_URI     "https://github.com/atakan/ampforge/amp"
#define DISTRHO_PLUGIN_CLAP_ID "com.ampforge.amp"

#define DISTRHO_PLUGIN_BRAND_ID  Ampf
#define DISTRHO_PLUGIN_UNIQUE_ID Amp1

// No UI yet - first we verify the parameters show up correctly in the
// host (Carla/Reaper) and that the DSP sounds right by ear.
// We'll add the GUI in a later phase.
#define DISTRHO_PLUGIN_HAS_UI        0
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    1
#define DISTRHO_PLUGIN_NUM_OUTPUTS   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS 0
#define DISTRHO_PLUGIN_WANT_STATE    0

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
