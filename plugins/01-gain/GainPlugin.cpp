/*
 * AmpForge - Gain Plugin (Phase 1)
 * Goal: learn DPF's basic structure and verify the build chain works.
 * This plugin takes a single parameter (Gain, in dB) and boosts/cuts
 * the incoming signal accordingly. This file will later become the
 * foundation of the Amp module.
 */

#include "DistrhoPlugin.hpp"
#include <cmath>

START_NAMESPACE_DISTRHO

// Parameters are defined here. Just one for now: Gain.
// We'll add entries to this enum as we add Bass/Mid/Treble/Volume later.
enum Parameters
{
    kParamGain = 0,
    kParamCount
};

class GainPlugin : public Plugin
{
public:
    GainPlugin()
        : Plugin(kParamCount, 0, 0) // (parameter count, program count, state count)
        , fGainDB(0.0f)             // 0 dB by default = no change
    {
    }

protected:
    // --- Plugin metadata ---
    const char* getLabel() const override       { return "AmpForgeGain"; }
    const char* getDescription() const override  { return "Base gain plugin for the AmpForge project."; }
    const char* getMaker() const override         { return "Atakan"; }
    const char* getLicense() const override       { return "GPL-3.0-or-later"; }
    uint32_t getVersion() const override           { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override           { return d_cconst('A', 'm', 'p', 'G'); }

    // --- Parameter definition ---
    // DPF calls this to ask us what parameters exist, then reports
    // them to the host (Carla, Reaper, etc.).
    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index != kParamGain)
            return;

        parameter.hints      = kParameterIsAutomatable;
        parameter.name       = "Gain";
        parameter.symbol     = "gain";
        parameter.unit       = "dB";
        parameter.ranges.def = 0.0f;
        parameter.ranges.min = -24.0f;
        parameter.ranges.max = 24.0f;
    }

    // Called whenever the host asks for the current parameter value
    float getParameterValue(uint32_t index) const override
    {
        return (index == kParamGain) ? fGainDB : 0.0f;
    }

    // Called when the user turns the knob (or automation plays back)
    void setParameterValue(uint32_t index, float value) override
    {
        if (index == kParamGain)
            fGainDB = value;
    }

    // --- The actual audio processing happens here ---
    // This function is called for every audio buffer, on the real-time
    // audio thread. That means no malloc/new, file I/O, or logging here
    // (those are not "RT-safe" and would cause clicks/dropouts).
    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // Convert dB to a linear multiplier: linear = 10^(dB/20)
        const float linearGain = std::pow(10.0f, fGainDB / 20.0f);

        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
            out[i] = in[i] * linearGain;
    }

private:
    float fGainDB;

    DISTRHO_DECLARE_NON_COPYABLE(GainPlugin)
};

Plugin* createPlugin()
{
    return new GainPlugin();
}

END_NAMESPACE_DISTRHO
