#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// Plugin identity - brand/name, how it will show up in hosts/formats
#define DISTRHO_PLUGIN_BRAND   "AmpForge"
#define DISTRHO_PLUGIN_NAME    "AmpForge Gain"
#define DISTRHO_PLUGIN_URI     "https://github.com/Loursy/ampforge/gain"
#define DISTRHO_PLUGIN_CLAP_ID "com.ampforge.gain"

// Every plugin needs its own unique 4-character ID
#define DISTRHO_PLUGIN_BRAND_ID  Ampf
#define DISTRHO_PLUGIN_UNIQUE_ID Gain

// No UI yet - we're just verifying the audio processing logic for now
#define DISTRHO_PLUGIN_HAS_UI        0
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    1
#define DISTRHO_PLUGIN_NUM_OUTPUTS   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS 0
#define DISTRHO_PLUGIN_WANT_STATE    0

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
