/*
 * AmpForge - Main Chain Plugin (Phase 5)
 *
 * This is the first version of the "real" AmpForge plugin: a single
 * plugin instance that runs an internal chain of modules, the way
 * Guitar Rig / BIAS FX work - you load ONE plugin in your DAW, and
 * everything (amp, pedals, ...) lives inside it.
 *
 * Current chain order (fixed for now): Screamer -> Amp
 * Once we add the GUI, the user will be able to reorder and add/remove
 * blocks visually. For now we hardcode the order and just expose an
 * on/off switch for the Screamer, so we can validate that chaining two
 * blocks together actually works and sounds right.
 */

#include "DistrhoPlugin.hpp"
#include "ScreamerBlock.hpp"
#include "AmpBlock.hpp"

START_NAMESPACE_DISTRHO

enum Parameters
{
    // Screamer block parameters
    kParamScreamerOn = 0,
    kParamScreamerDrive,
    kParamScreamerTone,
    kParamScreamerLevel,

    // Amp block parameters
    kParamAmpDrive,
    kParamAmpBass,
    kParamAmpMid,
    kParamAmpTreble,
    kParamAmpVolume,

    kParamCount
};

class ChainPlugin : public Plugin
{
public:
    ChainPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        screamerBlock.setDrive(1.0f);
        screamerBlock.setTone(0.5f);
        screamerBlock.setLevel(0.0f);

        ampBlock.setDriveDB(0.0f);
        ampBlock.setBassDB(0.0f);
        ampBlock.setMidDB(0.0f);
        ampBlock.setTrebleDB(0.0f);
        ampBlock.setVolumeDB(0.0f);
    }

protected:
    const char* getLabel() const override        { return "AmpForge"; }
    const char* getDescription() const override   { return "AmpForge: chained amp simulator - Screamer overdrive into an amp with a 3-band tone stack."; }
    const char* getMaker() const override          { return "Atakan"; }
    const char* getLicense() const override        { return "MIT"; }
    uint32_t getVersion() const override            { return d_version(0, 1, 0); }
    int64_t getUniqueId() const override            { return d_cconst('A', 'm', 'p', 'M'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamScreamerOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name       = "Screamer On";
            parameter.symbol     = "screamer_on";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamScreamerDrive:
            parameter.name       = "Screamer Drive";
            parameter.symbol     = "screamer_drive";
            parameter.unit       = "x";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 1.0f;
            parameter.ranges.max = 20.0f;
            break;
        case kParamScreamerTone:
            parameter.name       = "Screamer Tone";
            parameter.symbol     = "screamer_tone";
            parameter.ranges.def = 0.5f;
            parameter.ranges.min = 0.05f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamScreamerLevel:
            parameter.name       = "Screamer Level";
            parameter.symbol     = "screamer_level";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -24.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamAmpDrive:
            parameter.name       = "Amp Drive";
            parameter.symbol     = "amp_drive";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 36.0f;
            break;
        case kParamAmpBass:
            parameter.name       = "Amp Bass";
            parameter.symbol     = "amp_bass";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamAmpMid:
            parameter.name       = "Amp Mid";
            parameter.symbol     = "amp_mid";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamAmpTreble:
            parameter.name       = "Amp Treble";
            parameter.symbol     = "amp_treble";
            parameter.unit       = "dB";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = -12.0f;
            parameter.ranges.max = 12.0f;
            break;
        case kParamAmpVolume:
            parameter.name       = "Amp Volume";
            parameter.symbol     = "amp_volume";
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
        case kParamScreamerOn:     return screamerOn ? 1.0f : 0.0f;
        case kParamScreamerDrive:  return screamerDrive;
        case kParamScreamerTone:   return screamerTone;
        case kParamScreamerLevel:  return screamerLevel;
        case kParamAmpDrive:       return ampDrive;
        case kParamAmpBass:        return ampBass;
        case kParamAmpMid:         return ampMid;
        case kParamAmpTreble:      return ampTreble;
        case kParamAmpVolume:      return ampVolume;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamScreamerOn:
            screamerOn = value > 0.5f;
            break;
        case kParamScreamerDrive:
            screamerDrive = value;
            screamerBlock.setDrive(value);
            break;
        case kParamScreamerTone:
            screamerTone = value;
            screamerBlock.setTone(value);
            break;
        case kParamScreamerLevel:
            screamerLevel = value;
            screamerBlock.setLevel(value);
            break;
        case kParamAmpDrive:
            ampDrive = value;
            ampBlock.setDriveDB(value);
            break;
        case kParamAmpBass:
            ampBass = value;
            ampBlock.setBassDB(value);
            break;
        case kParamAmpMid:
            ampMid = value;
            ampBlock.setMidDB(value);
            break;
        case kParamAmpTreble:
            ampTreble = value;
            ampBlock.setTrebleDB(value);
            break;
        case kParamAmpVolume:
            ampVolume = value;
            ampBlock.setVolumeDB(value);
            break;
        }
    }

    void sampleRateChanged(double newSampleRate) override
    {
        screamerBlock.setSampleRate(newSampleRate);
        ampBlock.setSampleRate(newSampleRate);
    }

    // This is the actual chain: Screamer (if enabled) feeds into Amp.
    // Once we add reordering, this fixed sequence will be replaced by
    // a loop over a user-configurable vector<AudioBlock*>.
    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
        {
            float x = in[i];

            if (screamerOn)
                x = screamerBlock.processSample(x);

            x = ampBlock.processSample(x);

            out[i] = x;
        }
    }

private:
    ampforge::ScreamerBlock screamerBlock;
    ampforge::AmpBlock ampBlock;

    bool screamerOn = true;
    float screamerDrive = 1.0f, screamerTone = 0.5f, screamerLevel = 0.0f;
    float ampDrive = 0.0f, ampBass = 0.0f, ampMid = 0.0f, ampTreble = 0.0f, ampVolume = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE(ChainPlugin)
};

Plugin* createPlugin()
{
    return new ChainPlugin();
}

END_NAMESPACE_DISTRHO
