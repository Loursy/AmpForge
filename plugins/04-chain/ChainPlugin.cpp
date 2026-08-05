/*
 * AmpForge - Main Chain Plugin (Phase 7)
 *
 * The main AmpForge plugin: a single plugin instance that runs an
 * internal, REORDERABLE chain of modules (Screamer, Amp, Delay, Reverb),
 * the way Guitar Rig / BIAS FX work.
 *
 * The chain order is no longer hardcoded in run() - it lives in an
 * EffectChain object (core/EffectChain.hpp), and each block has its
 * own "Position" parameter. Right now there's no GUI to drag blocks
 * around, so position is set via a plain automatable parameter (0-3,
 * rounded to the nearest whole slot) - the host's automation lane is
 * effectively our reordering UI until we build the real one.
 */

#include "DistrhoPlugin.hpp"
#include "ScreamerBlock.hpp"
#include "AmpBlock.hpp"
#include "DelayBlock.hpp"
#include "ReverbBlock.hpp"
#include "EffectChain.hpp"
#include <cmath>

START_NAMESPACE_DISTRHO

enum Parameters
{
    // Screamer block parameters
    kParamScreamerOn = 0,
    kParamScreamerPosition,
    kParamScreamerDrive,
    kParamScreamerTone,
    kParamScreamerLevel,

    // Amp block parameters (always enabled - no on/off toggle)
    kParamAmpPosition,
    kParamAmpDrive,
    kParamAmpBass,
    kParamAmpMid,
    kParamAmpTreble,
    kParamAmpVolume,

    // Delay block parameters
    kParamDelayOn,
    kParamDelayPosition,
    kParamDelayTime,
    kParamDelayFeedback,
    kParamDelayMix,

    // Reverb block parameters
    kParamReverbOn,
    kParamReverbPosition,
    kParamReverbRoomSize,
    kParamReverbDamping,
    kParamReverbMix,

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

        delayBlock.setDelayTimeMs(300.0f);
        delayBlock.setFeedback(0.3f);
        delayBlock.setMix(0.3f);

        reverbBlock.setRoomSize(0.5f);
        reverbBlock.setDamping(0.5f);
        reverbBlock.setMix(0.3f);

        // Default chain order: Screamer(0) -> Amp(1) -> Delay(2) -> Reverb(3)
        // This matches the fixed order we had before, but now it's just
        // the starting configuration - it can be changed at runtime.
        chain.addBlock(&screamerBlock, 0);
        chain.addBlock(&ampBlock, 1);
        chain.addBlock(&delayBlock, 2);
        chain.addBlock(&reverbBlock, 3);
        chain.rebuildOrder();

        // Amp is always part of the signal path.
        chain.setEnabled(&ampBlock, true);
    }

protected:
    const char* getLabel() const override        { return "AmpForge"; }
    const char* getDescription() const override   { return "AmpForge: a reorderable amp simulator chain - Screamer, Amp, Delay and Reverb, each with its own position in the signal path."; }
    const char* getMaker() const override          { return "Atakan"; }
    const char* getLicense() const override        { return "MIT"; }
    uint32_t getVersion() const override            { return d_version(0, 3, 0); }
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
        case kParamScreamerPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name       = "Screamer Position";
            parameter.symbol     = "screamer_position";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 3.0f;
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
        case kParamAmpPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name       = "Amp Position";
            parameter.symbol     = "amp_position";
            parameter.ranges.def = 1.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 3.0f;
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
        case kParamDelayOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name       = "Delay On";
            parameter.symbol     = "delay_on";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamDelayPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name       = "Delay Position";
            parameter.symbol     = "delay_position";
            parameter.ranges.def = 2.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 3.0f;
            break;
        case kParamDelayTime:
            parameter.name       = "Delay Time";
            parameter.symbol     = "delay_time";
            parameter.unit       = "ms";
            parameter.ranges.def = 300.0f;
            parameter.ranges.min = 10.0f;
            parameter.ranges.max = 1500.0f;
            break;
        case kParamDelayFeedback:
            parameter.name       = "Delay Feedback";
            parameter.symbol     = "delay_feedback";
            parameter.ranges.def = 0.3f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 0.95f;
            break;
        case kParamDelayMix:
            parameter.name       = "Delay Mix";
            parameter.symbol     = "delay_mix";
            parameter.ranges.def = 0.3f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamReverbOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name       = "Reverb On";
            parameter.symbol     = "reverb_on";
            parameter.ranges.def = 0.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamReverbPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name       = "Reverb Position";
            parameter.symbol     = "reverb_position";
            parameter.ranges.def = 3.0f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 3.0f;
            break;
        case kParamReverbRoomSize:
            parameter.name       = "Reverb Room Size";
            parameter.symbol     = "reverb_room_size";
            parameter.ranges.def = 0.5f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamReverbDamping:
            parameter.name       = "Reverb Damping";
            parameter.symbol     = "reverb_damping";
            parameter.ranges.def = 0.5f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        case kParamReverbMix:
            parameter.name       = "Reverb Mix";
            parameter.symbol     = "reverb_mix";
            parameter.ranges.def = 0.3f;
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 1.0f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamScreamerOn:       return screamerOn ? 1.0f : 0.0f;
        case kParamScreamerPosition: return screamerPosition;
        case kParamScreamerDrive:    return screamerDrive;
        case kParamScreamerTone:     return screamerTone;
        case kParamScreamerLevel:    return screamerLevel;
        case kParamAmpPosition:      return ampPosition;
        case kParamAmpDrive:         return ampDrive;
        case kParamAmpBass:          return ampBass;
        case kParamAmpMid:           return ampMid;
        case kParamAmpTreble:        return ampTreble;
        case kParamAmpVolume:        return ampVolume;
        case kParamDelayOn:          return delayOn ? 1.0f : 0.0f;
        case kParamDelayPosition:    return delayPosition;
        case kParamDelayTime:        return delayTime;
        case kParamDelayFeedback:    return delayFeedback;
        case kParamDelayMix:         return delayMix;
        case kParamReverbOn:         return reverbOn ? 1.0f : 0.0f;
        case kParamReverbPosition:   return reverbPosition;
        case kParamReverbRoomSize:   return reverbRoomSize;
        case kParamReverbDamping:    return reverbDamping;
        case kParamReverbMix:        return reverbMix;
        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamScreamerOn:
            screamerOn = value > 0.5f;
            chain.setEnabled(&screamerBlock, screamerOn);
            break;
        case kParamScreamerPosition:
            screamerPosition = value;
            chain.setPosition(&screamerBlock, static_cast<int>(std::round(value)));
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
        case kParamAmpPosition:
            ampPosition = value;
            chain.setPosition(&ampBlock, static_cast<int>(std::round(value)));
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
        case kParamDelayOn:
            delayOn = value > 0.5f;
            chain.setEnabled(&delayBlock, delayOn);
            break;
        case kParamDelayPosition:
            delayPosition = value;
            chain.setPosition(&delayBlock, static_cast<int>(std::round(value)));
            break;
        case kParamDelayTime:
            delayTime = value;
            delayBlock.setDelayTimeMs(value);
            break;
        case kParamDelayFeedback:
            delayFeedback = value;
            delayBlock.setFeedback(value);
            break;
        case kParamDelayMix:
            delayMix = value;
            delayBlock.setMix(value);
            break;
        case kParamReverbOn:
            reverbOn = value > 0.5f;
            chain.setEnabled(&reverbBlock, reverbOn);
            break;
        case kParamReverbPosition:
            reverbPosition = value;
            chain.setPosition(&reverbBlock, static_cast<int>(std::round(value)));
            break;
        case kParamReverbRoomSize:
            reverbRoomSize = value;
            reverbBlock.setRoomSize(value);
            break;
        case kParamReverbDamping:
            reverbDamping = value;
            reverbBlock.setDamping(value);
            break;
        case kParamReverbMix:
            reverbMix = value;
            reverbBlock.setMix(value);
            break;
        }
    }

    void sampleRateChanged(double newSampleRate) override
    {
        screamerBlock.setSampleRate(newSampleRate);
        ampBlock.setSampleRate(newSampleRate);
        delayBlock.setSampleRate(newSampleRate);
        reverbBlock.setSampleRate(newSampleRate);
    }

    // The chain's order and which blocks are enabled now lives in
    // `chain` (see EffectChain.hpp) instead of being hardcoded here.
    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
            out[i] = chain.processSample(in[i]);
    }

private:
    ampforge::ScreamerBlock screamerBlock;
    ampforge::AmpBlock ampBlock;
    ampforge::DelayBlock delayBlock;
    ampforge::ReverbBlock reverbBlock;
    ampforge::EffectChain chain;

    bool screamerOn = true;
    float screamerPosition = 0.0f;
    float screamerDrive = 1.0f, screamerTone = 0.5f, screamerLevel = 0.0f;

    float ampPosition = 1.0f;
    float ampDrive = 0.0f, ampBass = 0.0f, ampMid = 0.0f, ampTreble = 0.0f, ampVolume = 0.0f;

    bool delayOn = false;
    float delayPosition = 2.0f;
    float delayTime = 300.0f, delayFeedback = 0.3f, delayMix = 0.3f;

    bool reverbOn = false;
    float reverbPosition = 3.0f;
    float reverbRoomSize = 0.5f, reverbDamping = 0.5f, reverbMix = 0.3f;

    DISTRHO_DECLARE_NON_COPYABLE(ChainPlugin)
};

Plugin* createPlugin()
{
    return new ChainPlugin();
}

END_NAMESPACE_DISTRHO