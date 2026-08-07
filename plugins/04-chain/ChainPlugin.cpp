/*
 * AmpForge - Main Chain Plugin (Phase 8)
 *
 * The main AmpForge plugin: a single plugin instance running a
 * reorderable chain of 10 modules, the way Guitar Rig / BIAS FX work.
 *
 * Default chain order (a typical pedalboard layout):
 *   0: Noise Gate  1: Compressor  2: Wah       3: Screamer  4: Distortion
 *   5: Amp         6: Chorus      7: Phaser    8: Tremolo   9: Delay
 *   10: Reverb
 *
 * Every block has a "Position" parameter (0-9) controlling where it
 * sits in the chain - this is what EffectChain (core/EffectChain.hpp)
 * uses to decide processing order. Until we build the GUI, position
 * is set via plain automatable parameters from the host.
 */

#include "DistrhoPlugin.hpp"
#include "ChainParameters.hpp"
#include "ChainPresets.hpp"
#include "ScreamerBlock.hpp"
#include "DistortionBlock.hpp"
#include "AmpBlock.hpp"
#include "DelayBlock.hpp"
#include "ReverbBlock.hpp"
#include "CompressorBlock.hpp"
#include "NoiseGateBlock.hpp"
#include "ChorusBlock.hpp"
#include "PhaserBlock.hpp"
#include "TremoloBlock.hpp"
#include "WahBlock.hpp"
#include "EffectChain.hpp"
#include <cmath>

START_NAMESPACE_DISTRHO

// clang-format on


class ChainPlugin : public Plugin
{
public:
    ChainPlugin()
        : Plugin(kParamCount, kProgramCount, 0)
    {
        gateBlock.setThresholdDB(-50.0f);
        gateBlock.setAttackMs(5.0f);
        gateBlock.setReleaseMs(150.0f);

        compBlock.setThresholdDB(-18.0f);
        compBlock.setRatio(4.0f);
        compBlock.setAttackMs(10.0f);
        compBlock.setReleaseMs(100.0f);
        compBlock.setMakeupGainDB(0.0f);

        wahBlock.setPedalPosition(0.5f);
        wahBlock.setQ(3.0f);

        screamerBlock.setDrive(1.0f);
        screamerBlock.setTone(0.5f);
        screamerBlock.setLevel(0.0f);

        distortionBlock.setDrive(4.0f);
        distortionBlock.setTone(0.5f);
        distortionBlock.setLevel(0.0f);

        ampBlock.setDriveDB(0.0f);
        ampBlock.setBassDB(0.0f);
        ampBlock.setMidDB(0.0f);
        ampBlock.setTrebleDB(0.0f);
        ampBlock.setVolumeDB(0.0f);

        chorusBlock.setRateHz(1.0f);
        chorusBlock.setDepthMs(5.0f);
        chorusBlock.setMix(0.5f);

        phaserBlock.setRateHz(0.5f);
        phaserBlock.setDepth(0.7f);
        phaserBlock.setMix(0.5f);

        tremoloBlock.setRateHz(5.0f);
        tremoloBlock.setDepth(0.5f);

        delayBlock.setDelayTimeMs(300.0f);
        delayBlock.setFeedback(0.3f);
        delayBlock.setMix(0.3f);

        reverbBlock.setRoomSize(0.5f);
        reverbBlock.setDamping(0.5f);
        reverbBlock.setMix(0.3f);

        // Default pedalboard order:
        // Gate(0) -> Comp(1) -> Wah(2) -> Screamer(3) -> Distortion(4) ->
        // Amp(5) -> Chorus(6) -> Phaser(7) -> Tremolo(8) -> Delay(9) ->
        // Reverb(10)
        chain.addBlock(&gateBlock, 0);
        chain.addBlock(&compBlock, 1);
        chain.addBlock(&wahBlock, 2);
        chain.addBlock(&screamerBlock, 3);
        chain.addBlock(&distortionBlock, 4);
        chain.addBlock(&ampBlock, 5);
        chain.addBlock(&chorusBlock, 6);
        chain.addBlock(&phaserBlock, 7);
        chain.addBlock(&tremoloBlock, 8);
        chain.addBlock(&delayBlock, 9);
        chain.addBlock(&reverbBlock, 10);
        chain.rebuildOrder();

        // Amp is always part of the signal path.
        chain.setEnabled(&ampBlock, true);

        // Everything else starts disabled except Screamer, which matches
        // the previous default (a basic overdrive-into-amp sound out of the box).
        chain.setEnabled(&gateBlock, false);
        chain.setEnabled(&compBlock, false);
        chain.setEnabled(&wahBlock, false);
        chain.setEnabled(&screamerBlock, true);
        chain.setEnabled(&distortionBlock, false);
        chain.setEnabled(&chorusBlock, false);
        chain.setEnabled(&phaserBlock, false);
        chain.setEnabled(&tremoloBlock, false);
        chain.setEnabled(&delayBlock, false);
        chain.setEnabled(&reverbBlock, false);

        // Defensive initialization: some hosts (notably the JACK/PipeWire
        // standalone target) can start processing audio before they call
        // sampleRateChanged(). Every block that allocates its buffers in
        // setSampleRate() would otherwise start out with empty buffers.
        // Calling it here with whatever sample rate the host reports at
        // construction time guarantees the buffers exist from sample one.
        const double initialSampleRate = getSampleRate();
        gateBlock.setSampleRate(initialSampleRate);
        compBlock.setSampleRate(initialSampleRate);
        wahBlock.setSampleRate(initialSampleRate);
        screamerBlock.setSampleRate(initialSampleRate);
        distortionBlock.setSampleRate(initialSampleRate);
        ampBlock.setSampleRate(initialSampleRate);
        chorusBlock.setSampleRate(initialSampleRate);
        phaserBlock.setSampleRate(initialSampleRate);
        tremoloBlock.setSampleRate(initialSampleRate);
        delayBlock.setSampleRate(initialSampleRate);
        reverbBlock.setSampleRate(initialSampleRate);
    }

protected:
    const char* getLabel() const override        { return "AmpForge"; }
    const char* getDescription() const override   { return "AmpForge: a full reorderable pedalboard-and-amp chain."; }
    const char* getMaker() const override          { return "Atakan"; }
    const char* getLicense() const override        { return "MIT"; }
    uint32_t getVersion() const override            { return d_version(0, 4, 0); }
    int64_t getUniqueId() const override            { return d_cconst('A', 'm', 'p', 'M'); }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        // --- Noise Gate ---
        case kParamGateOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Gate On"; parameter.symbol = "gate_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamGatePosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Gate Position"; parameter.symbol = "gate_position";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamGateThreshold:
            parameter.name = "Gate Threshold"; parameter.symbol = "gate_threshold"; parameter.unit = "dB";
            parameter.ranges.def = -50.0f; parameter.ranges.min = -80.0f; parameter.ranges.max = 0.0f;
            break;
        case kParamGateAttack:
            parameter.name = "Gate Attack"; parameter.symbol = "gate_attack"; parameter.unit = "ms";
            parameter.ranges.def = 5.0f; parameter.ranges.min = 0.5f; parameter.ranges.max = 50.0f;
            break;
        case kParamGateRelease:
            parameter.name = "Gate Release"; parameter.symbol = "gate_release"; parameter.unit = "ms";
            parameter.ranges.def = 150.0f; parameter.ranges.min = 10.0f; parameter.ranges.max = 1000.0f;
            break;

        // --- Compressor ---
        case kParamCompOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Compressor On"; parameter.symbol = "comp_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamCompPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Compressor Position"; parameter.symbol = "comp_position";
            parameter.ranges.def = 1.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamCompThreshold:
            parameter.name = "Compressor Threshold"; parameter.symbol = "comp_threshold"; parameter.unit = "dB";
            parameter.ranges.def = -18.0f; parameter.ranges.min = -60.0f; parameter.ranges.max = 0.0f;
            break;
        case kParamCompRatio:
            parameter.name = "Compressor Ratio"; parameter.symbol = "comp_ratio"; parameter.unit = ":1";
            parameter.ranges.def = 4.0f; parameter.ranges.min = 1.0f; parameter.ranges.max = 20.0f;
            break;
        case kParamCompAttack:
            parameter.name = "Compressor Attack"; parameter.symbol = "comp_attack"; parameter.unit = "ms";
            parameter.ranges.def = 10.0f; parameter.ranges.min = 0.5f; parameter.ranges.max = 100.0f;
            break;
        case kParamCompRelease:
            parameter.name = "Compressor Release"; parameter.symbol = "comp_release"; parameter.unit = "ms";
            parameter.ranges.def = 100.0f; parameter.ranges.min = 10.0f; parameter.ranges.max = 1000.0f;
            break;
        case kParamCompMakeup:
            parameter.name = "Compressor Makeup Gain"; parameter.symbol = "comp_makeup"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 24.0f;
            break;

        // --- Wah ---
        case kParamWahOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Wah On"; parameter.symbol = "wah_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamWahPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Wah Chain Position"; parameter.symbol = "wah_chain_position";
            parameter.ranges.def = 2.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamWahPedal:
            parameter.name = "Wah Pedal"; parameter.symbol = "wah_pedal";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamWahQ:
            parameter.name = "Wah Resonance"; parameter.symbol = "wah_q";
            parameter.ranges.def = 3.0f; parameter.ranges.min = 0.5f; parameter.ranges.max = 10.0f;
            break;

        // --- Screamer ---
        case kParamScreamerOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Screamer On"; parameter.symbol = "screamer_on";
            parameter.ranges.def = 1.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamScreamerPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Screamer Position"; parameter.symbol = "screamer_position";
            parameter.ranges.def = 3.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamScreamerDrive:
            parameter.name = "Screamer Drive"; parameter.symbol = "screamer_drive"; parameter.unit = "x";
            parameter.ranges.def = 1.0f; parameter.ranges.min = 1.0f; parameter.ranges.max = 20.0f;
            break;
        case kParamScreamerTone:
            parameter.name = "Screamer Tone"; parameter.symbol = "screamer_tone";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.05f; parameter.ranges.max = 1.0f;
            break;
        case kParamScreamerLevel:
            parameter.name = "Screamer Level"; parameter.symbol = "screamer_level"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -24.0f; parameter.ranges.max = 12.0f;
            break;

        // --- Distortion (a harder, more aggressive second gain stage
        // alongside Screamer's overdrive - see core/DistortionBlock.hpp) ---
        case kParamDistortionOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Distortion On"; parameter.symbol = "distortion_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamDistortionPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Distortion Position"; parameter.symbol = "distortion_position";
            parameter.ranges.def = 4.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamDistortionDrive:
            parameter.name = "Distortion Drive"; parameter.symbol = "distortion_drive"; parameter.unit = "x";
            parameter.ranges.def = 4.0f; parameter.ranges.min = 1.0f; parameter.ranges.max = 30.0f;
            break;
        case kParamDistortionTone:
            parameter.name = "Distortion Tone"; parameter.symbol = "distortion_tone";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.05f; parameter.ranges.max = 1.0f;
            break;
        case kParamDistortionLevel:
            parameter.name = "Distortion Level"; parameter.symbol = "distortion_level"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -24.0f; parameter.ranges.max = 12.0f;
            break;
        case kParamDistortionBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Distortion Bypass"; parameter.symbol = "distortion_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Amp ---
        case kParamAmpPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Amp Position"; parameter.symbol = "amp_position";
            parameter.ranges.def = 5.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamAmpDrive:
            parameter.name = "Amp Drive"; parameter.symbol = "amp_drive"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 36.0f;
            break;
        case kParamAmpBass:
            parameter.name = "Amp Bass"; parameter.symbol = "amp_bass"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -12.0f; parameter.ranges.max = 12.0f;
            break;
        case kParamAmpMid:
            parameter.name = "Amp Mid"; parameter.symbol = "amp_mid"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -12.0f; parameter.ranges.max = 12.0f;
            break;
        case kParamAmpTreble:
            parameter.name = "Amp Treble"; parameter.symbol = "amp_treble"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -12.0f; parameter.ranges.max = 12.0f;
            break;
        case kParamAmpVolume:
            parameter.name = "Amp Volume"; parameter.symbol = "amp_volume"; parameter.unit = "dB";
            parameter.ranges.def = 0.0f; parameter.ranges.min = -24.0f; parameter.ranges.max = 12.0f;
            break;

        // --- Chorus ---
        case kParamChorusOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Chorus On"; parameter.symbol = "chorus_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamChorusPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Chorus Position"; parameter.symbol = "chorus_position";
            parameter.ranges.def = 6.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamChorusRate:
            parameter.name = "Chorus Rate"; parameter.symbol = "chorus_rate"; parameter.unit = "Hz";
            parameter.ranges.def = 1.0f; parameter.ranges.min = 0.05f; parameter.ranges.max = 5.0f;
            break;
        case kParamChorusDepth:
            parameter.name = "Chorus Depth"; parameter.symbol = "chorus_depth"; parameter.unit = "ms";
            parameter.ranges.def = 5.0f; parameter.ranges.min = 0.5f; parameter.ranges.max = 20.0f;
            break;
        case kParamChorusMix:
            parameter.name = "Chorus Mix"; parameter.symbol = "chorus_mix";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Phaser ---
        case kParamPhaserOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Phaser On"; parameter.symbol = "phaser_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamPhaserPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Phaser Position"; parameter.symbol = "phaser_position";
            parameter.ranges.def = 7.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamPhaserRate:
            parameter.name = "Phaser Rate"; parameter.symbol = "phaser_rate"; parameter.unit = "Hz";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.05f; parameter.ranges.max = 5.0f;
            break;
        case kParamPhaserDepth:
            parameter.name = "Phaser Depth"; parameter.symbol = "phaser_depth";
            parameter.ranges.def = 0.7f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamPhaserMix:
            parameter.name = "Phaser Mix"; parameter.symbol = "phaser_mix";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Tremolo ---
        case kParamTremoloOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Tremolo On"; parameter.symbol = "tremolo_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamTremoloPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Tremolo Position"; parameter.symbol = "tremolo_position";
            parameter.ranges.def = 8.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamTremoloRate:
            parameter.name = "Tremolo Rate"; parameter.symbol = "tremolo_rate"; parameter.unit = "Hz";
            parameter.ranges.def = 5.0f; parameter.ranges.min = 0.5f; parameter.ranges.max = 15.0f;
            break;
        case kParamTremoloDepth:
            parameter.name = "Tremolo Depth"; parameter.symbol = "tremolo_depth";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Delay ---
        case kParamDelayOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Delay On"; parameter.symbol = "delay_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamDelayPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Delay Position"; parameter.symbol = "delay_position";
            parameter.ranges.def = 9.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamDelayTime:
            parameter.name = "Delay Time"; parameter.symbol = "delay_time"; parameter.unit = "ms";
            parameter.ranges.def = 300.0f; parameter.ranges.min = 10.0f; parameter.ranges.max = 1500.0f;
            break;
        case kParamDelayFeedback:
            parameter.name = "Delay Feedback"; parameter.symbol = "delay_feedback";
            parameter.ranges.def = 0.3f; parameter.ranges.min = 0.0f; parameter.ranges.max = 0.95f;
            break;
        case kParamDelayMix:
            parameter.name = "Delay Mix"; parameter.symbol = "delay_mix";
            parameter.ranges.def = 0.3f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Reverb ---
        case kParamReverbOn:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Reverb On"; parameter.symbol = "reverb_on";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamReverbPosition:
            parameter.hints |= kParameterIsInteger;
            parameter.name = "Reverb Position"; parameter.symbol = "reverb_position";
            parameter.ranges.def = 10.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 10.0f;
            break;
        case kParamReverbRoomSize:
            parameter.name = "Reverb Room Size"; parameter.symbol = "reverb_room_size";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamReverbDamping:
            parameter.name = "Reverb Damping"; parameter.symbol = "reverb_damping";
            parameter.ranges.def = 0.5f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamReverbMix:
            parameter.name = "Reverb Mix"; parameter.symbol = "reverb_mix";
            parameter.ranges.def = 0.3f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;

        // --- Bypass switches ---
        case kParamGateBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Gate Bypass"; parameter.symbol = "gate_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamCompBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Compressor Bypass"; parameter.symbol = "comp_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamWahBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Wah Bypass"; parameter.symbol = "wah_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamScreamerBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Screamer Bypass"; parameter.symbol = "screamer_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamChorusBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Chorus Bypass"; parameter.symbol = "chorus_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamPhaserBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Phaser Bypass"; parameter.symbol = "phaser_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamTremoloBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Tremolo Bypass"; parameter.symbol = "tremolo_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamDelayBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Delay Bypass"; parameter.symbol = "delay_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        case kParamReverbBypass:
            parameter.hints |= kParameterIsBoolean;
            parameter.name = "Reverb Bypass"; parameter.symbol = "reverb_bypass";
            parameter.ranges.def = 0.0f; parameter.ranges.min = 0.0f; parameter.ranges.max = 1.0f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index)
        {
        case kParamGateOn:         return gateOn ? 1.0f : 0.0f;
        case kParamGatePosition:   return gatePosition;
        case kParamGateThreshold:  return gateThreshold;
        case kParamGateAttack:     return gateAttack;
        case kParamGateRelease:    return gateRelease;

        case kParamCompOn:         return compOn ? 1.0f : 0.0f;
        case kParamCompPosition:   return compPosition;
        case kParamCompThreshold:  return compThreshold;
        case kParamCompRatio:      return compRatio;
        case kParamCompAttack:     return compAttack;
        case kParamCompRelease:    return compRelease;
        case kParamCompMakeup:     return compMakeup;

        case kParamWahOn:          return wahOn ? 1.0f : 0.0f;
        case kParamWahPosition:    return wahPosition;
        case kParamWahPedal:       return wahPedal;
        case kParamWahQ:           return wahQ;

        case kParamScreamerOn:       return screamerOn ? 1.0f : 0.0f;
        case kParamScreamerPosition: return screamerPosition;
        case kParamScreamerDrive:    return screamerDrive;
        case kParamScreamerTone:     return screamerTone;
        case kParamScreamerLevel:    return screamerLevel;

        case kParamDistortionOn:       return distortionOn ? 1.0f : 0.0f;
        case kParamDistortionPosition: return distortionPosition;
        case kParamDistortionDrive:    return distortionDrive;
        case kParamDistortionTone:     return distortionTone;
        case kParamDistortionLevel:    return distortionLevel;
        case kParamDistortionBypass:   return distortionBypass ? 1.0f : 0.0f;

        case kParamAmpPosition:      return ampPosition;
        case kParamAmpDrive:         return ampDrive;
        case kParamAmpBass:          return ampBass;
        case kParamAmpMid:           return ampMid;
        case kParamAmpTreble:        return ampTreble;
        case kParamAmpVolume:        return ampVolume;

        case kParamChorusOn:         return chorusOn ? 1.0f : 0.0f;
        case kParamChorusPosition:   return chorusPosition;
        case kParamChorusRate:       return chorusRate;
        case kParamChorusDepth:      return chorusDepth;
        case kParamChorusMix:        return chorusMix;

        case kParamPhaserOn:         return phaserOn ? 1.0f : 0.0f;
        case kParamPhaserPosition:   return phaserPosition;
        case kParamPhaserRate:       return phaserRate;
        case kParamPhaserDepth:      return phaserDepth;
        case kParamPhaserMix:        return phaserMix;

        case kParamTremoloOn:        return tremoloOn ? 1.0f : 0.0f;
        case kParamTremoloPosition:  return tremoloPosition;
        case kParamTremoloRate:      return tremoloRate;
        case kParamTremoloDepth:     return tremoloDepth;

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

        case kParamGateBypass:        return gateBypass ? 1.0f : 0.0f;
        case kParamCompBypass:        return compBypass ? 1.0f : 0.0f;
        case kParamWahBypass:         return wahBypass ? 1.0f : 0.0f;
        case kParamScreamerBypass:    return screamerBypass ? 1.0f : 0.0f;
        case kParamChorusBypass:      return chorusBypass ? 1.0f : 0.0f;
        case kParamPhaserBypass:      return phaserBypass ? 1.0f : 0.0f;
        case kParamTremoloBypass:     return tremoloBypass ? 1.0f : 0.0f;
        case kParamDelayBypass:       return delayBypass ? 1.0f : 0.0f;
        case kParamReverbBypass:      return reverbBypass ? 1.0f : 0.0f;

        default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        switch (index)
        {
        case kParamGateOn:
            gateOn = value > 0.5f; chain.setEnabled(&gateBlock, gateOn && !gateBypass); break;
        case kParamGatePosition:
            gatePosition = value; chain.setPosition(&gateBlock, static_cast<int>(std::round(value))); break;
        case kParamGateThreshold:
            gateThreshold = value; gateBlock.setThresholdDB(value); break;
        case kParamGateAttack:
            gateAttack = value; gateBlock.setAttackMs(value); break;
        case kParamGateRelease:
            gateRelease = value; gateBlock.setReleaseMs(value); break;

        case kParamCompOn:
            compOn = value > 0.5f; chain.setEnabled(&compBlock, compOn && !compBypass); break;
        case kParamCompPosition:
            compPosition = value; chain.setPosition(&compBlock, static_cast<int>(std::round(value))); break;
        case kParamCompThreshold:
            compThreshold = value; compBlock.setThresholdDB(value); break;
        case kParamCompRatio:
            compRatio = value; compBlock.setRatio(value); break;
        case kParamCompAttack:
            compAttack = value; compBlock.setAttackMs(value); break;
        case kParamCompRelease:
            compRelease = value; compBlock.setReleaseMs(value); break;
        case kParamCompMakeup:
            compMakeup = value; compBlock.setMakeupGainDB(value); break;

        case kParamWahOn:
            wahOn = value > 0.5f; chain.setEnabled(&wahBlock, wahOn && !wahBypass); break;
        case kParamWahPosition:
            wahPosition = value; chain.setPosition(&wahBlock, static_cast<int>(std::round(value))); break;
        case kParamWahPedal:
            wahPedal = value; wahBlock.setPedalPosition(value); break;
        case kParamWahQ:
            wahQ = value; wahBlock.setQ(value); break;

        case kParamScreamerOn:
            screamerOn = value > 0.5f; chain.setEnabled(&screamerBlock, screamerOn && !screamerBypass); break;
        case kParamScreamerPosition:
            screamerPosition = value; chain.setPosition(&screamerBlock, static_cast<int>(std::round(value))); break;
        case kParamScreamerDrive:
            screamerDrive = value; screamerBlock.setDrive(value); break;
        case kParamScreamerTone:
            screamerTone = value; screamerBlock.setTone(value); break;
        case kParamScreamerLevel:
            screamerLevel = value; screamerBlock.setLevel(value); break;

        case kParamDistortionOn:
            distortionOn = value > 0.5f; chain.setEnabled(&distortionBlock, distortionOn && !distortionBypass); break;
        case kParamDistortionPosition:
            distortionPosition = value; chain.setPosition(&distortionBlock, static_cast<int>(std::round(value))); break;
        case kParamDistortionDrive:
            distortionDrive = value; distortionBlock.setDrive(value); break;
        case kParamDistortionTone:
            distortionTone = value; distortionBlock.setTone(value); break;
        case kParamDistortionLevel:
            distortionLevel = value; distortionBlock.setLevel(value); break;
        case kParamDistortionBypass:
            distortionBypass = value > 0.5f; chain.setEnabled(&distortionBlock, distortionOn && !distortionBypass); break;

        case kParamAmpPosition:
            ampPosition = value; chain.setPosition(&ampBlock, static_cast<int>(std::round(value))); break;
        case kParamAmpDrive:
            ampDrive = value; ampBlock.setDriveDB(value); break;
        case kParamAmpBass:
            ampBass = value; ampBlock.setBassDB(value); break;
        case kParamAmpMid:
            ampMid = value; ampBlock.setMidDB(value); break;
        case kParamAmpTreble:
            ampTreble = value; ampBlock.setTrebleDB(value); break;
        case kParamAmpVolume:
            ampVolume = value; ampBlock.setVolumeDB(value); break;

        case kParamChorusOn:
            chorusOn = value > 0.5f; chain.setEnabled(&chorusBlock, chorusOn && !chorusBypass); break;
        case kParamChorusPosition:
            chorusPosition = value; chain.setPosition(&chorusBlock, static_cast<int>(std::round(value))); break;
        case kParamChorusRate:
            chorusRate = value; chorusBlock.setRateHz(value); break;
        case kParamChorusDepth:
            chorusDepth = value; chorusBlock.setDepthMs(value); break;
        case kParamChorusMix:
            chorusMix = value; chorusBlock.setMix(value); break;

        case kParamPhaserOn:
            phaserOn = value > 0.5f; chain.setEnabled(&phaserBlock, phaserOn && !phaserBypass); break;
        case kParamPhaserPosition:
            phaserPosition = value; chain.setPosition(&phaserBlock, static_cast<int>(std::round(value))); break;
        case kParamPhaserRate:
            phaserRate = value; phaserBlock.setRateHz(value); break;
        case kParamPhaserDepth:
            phaserDepth = value; phaserBlock.setDepth(value); break;
        case kParamPhaserMix:
            phaserMix = value; phaserBlock.setMix(value); break;

        case kParamTremoloOn:
            tremoloOn = value > 0.5f; chain.setEnabled(&tremoloBlock, tremoloOn && !tremoloBypass); break;
        case kParamTremoloPosition:
            tremoloPosition = value; chain.setPosition(&tremoloBlock, static_cast<int>(std::round(value))); break;
        case kParamTremoloRate:
            tremoloRate = value; tremoloBlock.setRateHz(value); break;
        case kParamTremoloDepth:
            tremoloDepth = value; tremoloBlock.setDepth(value); break;

        case kParamDelayOn:
            delayOn = value > 0.5f; chain.setEnabled(&delayBlock, delayOn && !delayBypass); break;
        case kParamDelayPosition:
            delayPosition = value; chain.setPosition(&delayBlock, static_cast<int>(std::round(value))); break;
        case kParamDelayTime:
            delayTime = value; delayBlock.setDelayTimeMs(value); break;
        case kParamDelayFeedback:
            delayFeedback = value; delayBlock.setFeedback(value); break;
        case kParamDelayMix:
            delayMix = value; delayBlock.setMix(value); break;

        case kParamReverbOn:
            reverbOn = value > 0.5f; chain.setEnabled(&reverbBlock, reverbOn && !reverbBypass); break;
        case kParamReverbPosition:
            reverbPosition = value; chain.setPosition(&reverbBlock, static_cast<int>(std::round(value))); break;
        case kParamReverbRoomSize:
            reverbRoomSize = value; reverbBlock.setRoomSize(value); break;
        case kParamReverbDamping:
            reverbDamping = value; reverbBlock.setDamping(value); break;
        case kParamReverbMix:
            reverbMix = value; reverbBlock.setMix(value); break;

        case kParamGateBypass:
            gateBypass = value > 0.5f; chain.setEnabled(&gateBlock, gateOn && !gateBypass); break;
        case kParamCompBypass:
            compBypass = value > 0.5f; chain.setEnabled(&compBlock, compOn && !compBypass); break;
        case kParamWahBypass:
            wahBypass = value > 0.5f; chain.setEnabled(&wahBlock, wahOn && !wahBypass); break;
        case kParamScreamerBypass:
            screamerBypass = value > 0.5f; chain.setEnabled(&screamerBlock, screamerOn && !screamerBypass); break;
        case kParamChorusBypass:
            chorusBypass = value > 0.5f; chain.setEnabled(&chorusBlock, chorusOn && !chorusBypass); break;
        case kParamPhaserBypass:
            phaserBypass = value > 0.5f; chain.setEnabled(&phaserBlock, phaserOn && !phaserBypass); break;
        case kParamTremoloBypass:
            tremoloBypass = value > 0.5f; chain.setEnabled(&tremoloBlock, tremoloOn && !tremoloBypass); break;
        case kParamDelayBypass:
            delayBypass = value > 0.5f; chain.setEnabled(&delayBlock, delayOn && !delayBypass); break;
        case kParamReverbBypass:
            reverbBypass = value > 0.5f; chain.setEnabled(&reverbBlock, reverbOn && !reverbBypass); break;
        }
    }

    void sampleRateChanged(double newSampleRate) override
    {
        gateBlock.setSampleRate(newSampleRate);
        compBlock.setSampleRate(newSampleRate);
        wahBlock.setSampleRate(newSampleRate);
        screamerBlock.setSampleRate(newSampleRate);
        distortionBlock.setSampleRate(newSampleRate);
        ampBlock.setSampleRate(newSampleRate);
        chorusBlock.setSampleRate(newSampleRate);
        phaserBlock.setSampleRate(newSampleRate);
        tremoloBlock.setSampleRate(newSampleRate);
        delayBlock.setSampleRate(newSampleRate);
        reverbBlock.setSampleRate(newSampleRate);
    }

    // Reports the preset names to the host, so they show up in its
    // program/preset dropdown.
    void initProgramName(uint32_t index, String& programName) override
    {
        if (index < kProgramCount)
            programName = kPresets[index].name;
    }

    // Called when the user picks a preset from the host's program list.
    // We simply replay every parameter value from the snapshot through
    // setParameterValue(), the same path used for manual knob turns -
    // that keeps our own member variables and the EffectChain in sync
    // automatically.
    void loadProgram(uint32_t index) override
    {
        if (index >= kProgramCount)
            return;

        const PresetDefinition& preset = kPresets[index];
        for (uint32_t i = 0; i < kParamCount; ++i)
            setParameterValue(i, preset.values[i]);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* in  = inputs[0];
        float*       out = outputs[0];

        for (uint32_t i = 0; i < frames; ++i)
            out[i] = chain.processSample(in[i]);
    }

private:
    ampforge::NoiseGateBlock gateBlock;
    ampforge::CompressorBlock compBlock;
    ampforge::WahBlock wahBlock;
    ampforge::ScreamerBlock screamerBlock;
    ampforge::DistortionBlock distortionBlock;
    ampforge::AmpBlock ampBlock;
    ampforge::ChorusBlock chorusBlock;
    ampforge::PhaserBlock phaserBlock;
    ampforge::TremoloBlock tremoloBlock;
    ampforge::DelayBlock delayBlock;
    ampforge::ReverbBlock reverbBlock;
    ampforge::EffectChain chain;

    bool gateOn = false;
    float gatePosition = 0.0f, gateThreshold = -50.0f, gateAttack = 5.0f, gateRelease = 150.0f;

    bool compOn = false;
    float compPosition = 1.0f, compThreshold = -18.0f, compRatio = 4.0f, compAttack = 10.0f, compRelease = 100.0f, compMakeup = 0.0f;

    bool wahOn = false;
    float wahPosition = 2.0f, wahPedal = 0.5f, wahQ = 3.0f;

    bool screamerOn = true;
    float screamerPosition = 3.0f, screamerDrive = 1.0f, screamerTone = 0.5f, screamerLevel = 0.0f;

    bool distortionOn = false;
    float distortionPosition = 4.0f, distortionDrive = 4.0f, distortionTone = 0.5f, distortionLevel = 0.0f;

    float ampPosition = 5.0f, ampDrive = 0.0f, ampBass = 0.0f, ampMid = 0.0f, ampTreble = 0.0f, ampVolume = 0.0f;

    bool chorusOn = false;
    float chorusPosition = 6.0f, chorusRate = 1.0f, chorusDepth = 5.0f, chorusMix = 0.5f;

    bool phaserOn = false;
    float phaserPosition = 7.0f, phaserRate = 0.5f, phaserDepth = 0.7f, phaserMix = 0.5f;

    bool tremoloOn = false;
    float tremoloPosition = 8.0f, tremoloRate = 5.0f, tremoloDepth = 0.5f;

    bool delayOn = false;
    float delayPosition = 9.0f, delayTime = 300.0f, delayFeedback = 0.3f, delayMix = 0.3f;

    bool reverbOn = false;
    float reverbPosition = 10.0f, reverbRoomSize = 0.5f, reverbDamping = 0.5f, reverbMix = 0.3f;

    // Bypass flags - independent from the *On* flags above. See the
    // comment on the Bypass parameters in ChainParameters.hpp.
    bool gateBypass = false;
    bool compBypass = false;
    bool wahBypass = false;
    bool screamerBypass = false;
    bool chorusBypass = false;
    bool phaserBypass = false;
    bool tremoloBypass = false;
    bool delayBypass = false;
    bool reverbBypass = false;
    bool distortionBypass = false;

    DISTRHO_DECLARE_NON_COPYABLE(ChainPlugin)
};

Plugin* createPlugin()
{
    return new ChainPlugin();
}

END_NAMESPACE_DISTRHO
