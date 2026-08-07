/*
 * AmpForge - Amp Plugin (Phase 3)
 * This file does not contain the actual DSP logic - that lives in
 * core/AmpBlock.hpp. All we do here is wire DPF's parameter/host system
 * to AmpBlock. This separation is deliberate: when we add new blocks
 * (Screamer, Delay, ...) we'll repeat this same "thin wrapper" pattern
 * for each one, while the real DSP logic stays in core/, independent
 * of the plugin format.
 */

#include "DistrhoPlugin.hpp"
#include "AmpBlock.hpp"

START_NAMESPACE_DISTRHO

enum Parameters
{
    kParamDrive = 0,
    kParamBass,
    kParamMid,
    kParamTreble,
    kParamVolume,
    kParamCount
};

class AmpPlugin : public Plugin
{
public:
    AmpPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        // Apply the default values to AmpBlock too, so the first sound is clean
        ampBlock.setDriveDB(0.0f);
        ampBlock.setBassDB(0.0f);
        ampBlock.setMidDB(0.0f);
        ampBlock.setTrebleDB(0.0f);
        ampBlock.setVolumeDB(0.0f);
    }

protected:
    const char* getLabel() const override        { return "AmpForgeAmp"; }
    const char* getDescription() const override   { return "First real amp module of the AmpForge project: drive + 3-band tone stack."; }
    const char* getMaker() const override          { return "Atakan"; }
    const char* getLicense() const override        { return "GPL-3.0-or-later"; }
    uint32_t getVersion() const override            { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override            { return d_cconst('A', 'm', 'p', '1'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamDrive:
            parameter.name       = "Drive";
            parameter.symbol     = "drive";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 36.0f; // higher values push more clipping/distortion
            break;
        case kParamBass:
            parameter.name       = "Bass";
            parameter.symbol     = "bass";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamMid:
            parameter.name       = "Mid";
            parameter.symbol     = "mid";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamTreble:
            parameter.name       = "Treble";
            parameter.symbol     = "treble";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamVolume:
            parameter.name       = "Volume";
            parameter.symbol     = "volume";
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
        case kParamDrive:  return driveDB;
        case kParamBass:   return bassDB;
        case kParamMid:    return midDB;
        case kParamTreble: return trebleDB;
        case kParamVolume: return volumeDB;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamDrive:  driveDB = value;  ampBlock.setDriveDB(value);  break;
        case kParamBass:   bassDB = value;   ampBlock.setBassDB(value);   break;
        case kParamMid:    midDB = value;    ampBlock.setMidDB(value);    break;
        case kParamTreble: trebleDB = value; ampBlock.setTrebleDB(value); break;
        case kParamVolume: volumeDB = value; ampBlock.setVolumeDB(value); break;
        }
    }

    // Called when the host reports the sample rate (or when the plugin is first loaded)
    void sampleRateChanged(double newSampleRate) override
    {
        ampBlock.setSampleRate(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
            out[i] = ampBlock.processSample(in[i]);
    }

private:
    ampforge::AmpBlock ampBlock;

    // We keep a local copy of the parameter values so getParameterValue()
    // can answer the host quickly (simpler than adding getters to AmpBlock).
    float driveDB = 0.0f, bassDB = 0.0f, midDB = 0.0f, trebleDB = 0.0f, volumeDB = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE(AmpPlugin)
};

Plugin* createPlugin()
{
    return new AmpPlugin();
}

END_NAMESPACE_DISTRHO
