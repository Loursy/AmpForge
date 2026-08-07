/*
 * AmpForge - Screamer Plugin (Phase 4)
 * Same pattern as AmpPlugin: this is a thin wrapper that connects
 * DPF's parameter/host system to ScreamerBlock. The real DSP logic
 * lives in core/ScreamerBlock.hpp.
 */

#include "DistrhoPlugin.hpp"
#include "ScreamerBlock.hpp"

START_NAMESPACE_DISTRHO

enum Parameters
{
    kParamDrive = 0,
    kParamTone,
    kParamLevel,
    kParamCount
};

class ScreamerPlugin : public Plugin
{
public:
    ScreamerPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        screamerBlock.setDrive(1.0f);
        screamerBlock.setTone(0.5f);
        screamerBlock.setLevel(0.0f);
    }

protected:
    const char* getLabel() const override        { return "AmpForgeScreamer"; }
    const char* getDescription() const override   { return "Tube Screamer style overdrive pedal module for AmpForge."; }
    const char* getMaker() const override          { return "Atakan"; }
    const char* getLicense() const override        { return "GPL-3.0-or-later"; }
    uint32_t getVersion() const override            { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override            { return d_cconst('S', 'c', 'r', 'm'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamDrive:
            parameter.name       = "Drive";
            parameter.symbol     = "drive";
            parameter.unit       = "x";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 1.0f;
            parameter.ranges.max = 20.0f;
            break;
        case kParamTone:
            parameter.name       = "Tone";
            parameter.symbol     = "tone";
            parameter.unit       = "";
            parameter.ranges.def = 0.5f;
            parameter.ranges.min = 0.05f; // avoid a value near 0, which would make the filter mute almost everything
            parameter.ranges.max = 1.0f;
            break;
        case kParamLevel:
            parameter.name       = "Level";
            parameter.symbol     = "level";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -24.0f;
            parameter.ranges.max = 12.0f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamDrive: return driveValue;
        case kParamTone:  return toneValue;
        case kParamLevel: return levelValue;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamDrive: driveValue = value; screamerBlock.setDrive(value); break;
        case kParamTone:  toneValue = value;  screamerBlock.setTone(value);  break;
        case kParamLevel: levelValue = value; screamerBlock.setLevel(value); break;
        }
    }

    void sampleRateChanged(double newSampleRate) override
    {
        screamerBlock.setSampleRate(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
            out[i] = screamerBlock.processSample(in[i]);
    }

private:
    ampforge::ScreamerBlock screamerBlock;
    float driveValue = 1.0f, toneValue = 0.5f, levelValue = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE(ScreamerPlugin)
};

Plugin* createPlugin()
{
    return new ScreamerPlugin();
}

END_NAMESPACE_DISTRHO
