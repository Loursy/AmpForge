#pragma once

/*
 * ChainParameters.hpp - the parameter index list, shared between the DSP
 * side (ChainPlugin.cpp) and the UI side (ChainUI.cpp).
 *
 * This has to live in its own header instead of inside ChainPlugin.cpp,
 * because the UI needs to know the exact same parameter indices to send
 * setParameterValue() calls that land on the right knob/switch - DPF
 * builds the DSP and UI as separate binaries, so they only agree on
 * "what parameter index N means" if they both include this file.
 */

START_NAMESPACE_DISTRHO

enum Parameters
{
    // Noise Gate
    kParamGateOn = 0,
    kParamGatePosition,
    kParamGateThreshold,
    kParamGateAttack,
    kParamGateRelease,

    // Compressor
    kParamCompOn,
    kParamCompPosition,
    kParamCompThreshold,
    kParamCompRatio,
    kParamCompAttack,
    kParamCompRelease,
    kParamCompMakeup,

    // Wah
    kParamWahOn,
    kParamWahPosition,
    kParamWahPedal,
    kParamWahQ,

    // Screamer
    kParamScreamerOn,
    kParamScreamerPosition,
    kParamScreamerDrive,
    kParamScreamerTone,
    kParamScreamerLevel,

    // Amp (always enabled)
    kParamAmpPosition,
    kParamAmpDrive,
    kParamAmpBass,
    kParamAmpMid,
    kParamAmpTreble,
    kParamAmpVolume,

    // Chorus
    kParamChorusOn,
    kParamChorusPosition,
    kParamChorusRate,
    kParamChorusDepth,
    kParamChorusMix,

    // Phaser
    kParamPhaserOn,
    kParamPhaserPosition,
    kParamPhaserRate,
    kParamPhaserDepth,
    kParamPhaserMix,

    // Tremolo
    kParamTremoloOn,
    kParamTremoloPosition,
    kParamTremoloRate,
    kParamTremoloDepth,

    // Delay
    kParamDelayOn,
    kParamDelayPosition,
    kParamDelayTime,
    kParamDelayFeedback,
    kParamDelayMix,

    // Reverb
    kParamReverbOn,
    kParamReverbPosition,
    kParamReverbRoomSize,
    kParamReverbDamping,
    kParamReverbMix,

    // Bypass switches - separate from *On* (which controls whether a
    // pedal is on the board at all). Bypass lets a pedal stay visible
    // in the rack while its processing is temporarily switched off,
    // like a real pedal's footswitch - added at the end of the enum so
    // every existing parameter index above stays unchanged.
    kParamGateBypass,
    kParamCompBypass,
    kParamWahBypass,
    kParamScreamerBypass,
    kParamChorusBypass,
    kParamPhaserBypass,
    kParamTremoloBypass,
    kParamDelayBypass,
    kParamReverbBypass,

    // Distortion - added after the original parameter set (including the
    // bypass block above) so every existing parameter index, and every
    // already-saved preset/session, keeps working unchanged. A harder,
    // more aggressive second gain stage alongside Screamer's overdrive -
    // see core/DistortionBlock.hpp.
    kParamDistortionOn,
    kParamDistortionPosition,
    kParamDistortionDrive,
    kParamDistortionTone,
    kParamDistortionLevel,
    kParamDistortionBypass,

    // Cabinet - convolves with a loaded speaker-cabinet impulse response
    // (see core/CabinetBlock.hpp). The IR file path itself isn't a plain
    // automatable value, so it isn't a parameter at all - it travels as
    // DPF plugin State instead, under kCabinetIRStateKey below, which is
    // why this block only has Mix/Level/On/Position/Bypass here.
    kParamCabinetOn,
    kParamCabinetPosition,
    kParamCabinetMix,
    kParamCabinetLevel,
    kParamCabinetBypass,

    // Gate Range - how far the gate pulls the signal down when closed,
    // instead of always slamming it to full silence. Added after every
    // other parameter (including Cabinet above) so existing parameter
    // indices, and every already-saved preset/session, keep working
    // unchanged - same reasoning as the Bypass block and Distortion/
    // Cabinet additions above.
    kParamGateRange,

    kParamCount
};

// The DPF State key that carries the Cabinet block's loaded impulse-
// response file path. Shared between ChainPlugin.cpp (the state's actual
// owner) and ChainUI.cpp (which requests/display it), the same way the
// Parameters enum above is shared, so the two sides can't drift apart.
static const char* const kCabinetIRStateKey = "cabinet_ir_path";

END_NAMESPACE_DISTRHO
