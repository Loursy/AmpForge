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

    // Input Gain - a fixed pre-chain trim, always applied first and not
    // part of the reorderable pedalboard (see ChainUI.cpp's dedicated
    // Input panel). Guitars/interfaces feed wildly different levels into
    // the plugin; without a way to normalize that first, one Noise Gate
    // threshold default can't work for everyone - trimming here, ahead
    // of the gate, is what lets it. Appended at the end like every block
    // above, so existing parameter indices/presets stay unchanged.
    kParamInputGain,

    // Tempo sync - lets Delay/Tremolo/Chorus lock their rate to the host's
    // BPM (via DPF's TimePosition API) instead of only the free-running
    // ms/Hz knob. Each is a single index into kSyncDivisions below: 0 means
    // "Free" (sync off, the block's own Time/Rate parameter is used
    // unchanged), any other value picks a musical note division that
    // ChainPlugin::run() recomputes from the host tempo every block.
    // Appended at the end like every block above, so existing parameter
    // indices/presets stay unchanged.
    kParamDelaySync,
    kParamTremoloSync,
    kParamChorusSync,

    // Tuner - a standalone pitch-detection overlay (core/PitchDetector.hpp)
    // that taps a copy of the post-Input-Gain, pre-chain signal, entirely
    // outside the reorderable pedalboard, so it always reads the guitar's
    // actual pitch regardless of what Distortion/Wah/etc. are doing to the
    // signal that reaches the amp. kParamTunerOn is the UI's toggle button
    // (ChainPlugin::run() only bothers feeding the detector while it's on);
    // its output-only frequency reading lives at the very end of this enum,
    // with the other output-only parameters - see the comment down there
    // for why.
    kParamTunerOn,

    // Amp Type - selects between the handful of EQ-and-saturation
    // voicings defined in core/AmpBlock.hpp's kAmpVoicings (index 0,
    // "Modern", reproduces AmpBlock's original fixed behavior exactly, so
    // every preset/session saved before this parameter existed is
    // unaffected). Lives in AmpBlock.hpp rather than here since it's a
    // property of the Amp DSP block itself, reused as-is by the standalone
    // 02-amp plugin too - not something specific to this chain plugin the
    // way kSyncDivisions above is.
    kParamAmpType,

    // NAM - a neural network capture of one specific real amp/pedal/cab,
    // loaded from a .nam file (core/NamBlock.hpp), reusing NeuralAmpModelerCore
    // (github.com/sdatkinson/NeuralAmpModelerCore, includes A1 and the newer
    // A2 architecture). A separate, optional block from the parametric
    // AmpBlock above, not a replacement for it - the two are independent
    // tone sources the user can mix and match (see kParamAmpOn/Bypass
    // below for why Amp not being bypassable used to be a problem for
    // this pairing specifically). The .nam file
    // path itself isn't a plain automatable value, so like Cabinet's IR
    // above it isn't a parameter at all - it travels as DPF plugin State
    // under kNamModelStateKey below, which is why this block only has
    // On/Position/InputTrim/OutputLevel/Mix/Bypass here. Appended at the
    // end like every block above, so existing parameter indices/presets
    // stay unchanged.
    kParamNamOn,
    kParamNamPosition,
    kParamNamInputTrim,
    kParamNamOutputLevel,
    kParamNamMix,
    kParamNamBypass,

    // Amp On/Bypass - added well after the original Amp parameter block
    // above, which had no way to disable it at all (every comment calling
    // it "the always-on parametric AmpBlock" meant it literally: no other
    // block in this whole enum lacks an On/Bypass pair). That turned out
    // to matter once NAM existed: AmpBlock's Drive/saturation stage is a
    // tanh() soft-clip that runs unconditionally, even with Drive/Bass/
    // Mid/Treble/Volume all left at their neutral 0dB defaults - measured
    // at ~2% third-harmonic THD for a realistic guitar peak level (~0.5),
    // growing well past that for a hotter signal. NAM's own model expects
    // a clean, unprocessed input (it's already a capture of one specific
    // amp/pedal/cab), so with NAM sitting after Amp at their respective
    // default positions, that unavoidable pre-saturation quietly feeds
    // NAM something it was never trained on. On/Bypass here let Amp
    // actually be taken out of the signal path - same as every other
    // block already could - instead of just approximating "off" via
    // Drive=0dB. Both default to on/not-bypassed (see initParameter() in
    // ChainPlugin.cpp and kBlankPresetValues/kPresets) so every preset or
    // session saved before these existed still sounds identical.
    kParamAmpOn,
    kParamAmpBypass,

    // Autotune - classic "hard-tune" vocal pitch correction
    // (core/AutotuneBlock.hpp), the first block in the chain that isn't
    // guitar-oriented (Chorus/Phaser/Tremolo/Delay/Reverb are generic
    // enough to double as vocal effects too, but this one only makes
    // sense for a voice). Sits in the same reorderable pedalboard as
    // everything else - a "vocal chain" is just Amp/Cabinet/NAM turned
    // off and this (plus Delay/Reverb) turned on, same mechanism as
    // switching between guitar tones. Key is 0-11 (C..B), Scale indexes
    // ampforge::kScales (Chromatic/Major/Minor), Speed is the classic
    // autotune "Retune Speed" knob (0 = slow glide into the correction,
    // 1 = the near-instant robotic snap) - see AutotuneBlock.hpp's
    // comment for what it actually controls. Appended at the end like
    // every block above, so existing parameter indices/presets stay
    // unchanged.
    kParamAutotuneOn,
    kParamAutotunePosition,
    kParamAutotuneKey,
    kParamAutotuneScale,
    kParamAutotuneSpeed,
    kParamAutotuneBypass,

    // De-esser - the second vocal-only block (core/DeEsserBlock.hpp),
    // tames harsh "s"/"sh" sibilance. Frequency is where the sibilant
    // band starts (typically 4-9kHz depending on the voice), Threshold
    // is the level (in that band) above which it kicks in, Reduction is
    // the maximum amount of gain reduction it's allowed to apply -
    // see DeEsserBlock.hpp's comment for the split-band technique this
    // uses. Appended at the end like every block above, so existing
    // parameter indices/presets stay unchanged.
    kParamDeEsserOn,
    kParamDeEsserPosition,
    kParamDeEsserFrequency,
    kParamDeEsserThreshold,
    kParamDeEsserReduction,
    kParamDeEsserBypass,

    // Output-only meters (kParameterIsOutput) - deliberately grouped here,
    // all together at the very end, rather than appended next to the
    // input/feature they each report on (which is where they originally
    // lived, and read more naturally). DPF's LV2 presets.ttl exporter
    // writes the non-output parameters as one comma-separated lv2:port
    // list and only closes it with a "." once it hits either the last
    // parameter or an output one - so *any* output parameter that isn't
    // at the very end splits that list into multiple statements with no
    // subject on the second one, which is invalid Turtle syntax (verified
    // with lv2_validate: Carla's own lilv-based loader shrugs it off and
    // loads the plugin fine, but it leaves the host's *own* separate LV2
    // preset browser broken/empty for that bundle). Clustering all three
    // here avoids that entirely.
    //
    // Input Level - the post-trim peak level, reported back to the UI so
    // players can see how hot their signal is while dialing Input Gain
    // and the Noise Gate in together. ChainPlugin::run() writes it every
    // block, the host polls it and forwards the value to the UI's
    // parameterChanged() - the same mechanism DPF's own Meters example
    // uses.
    kParamInputLevel,
    // CPU Load - how much of the available per-block time run() actually
    // used (1.0 = took the whole block interval, i.e. right at the edge of
    // an audio dropout), reported back to the UI the same way Input Level
    // is above. ChainPlugin::run() times itself with std::chrono and
    // writes this every block.
    kParamCpuLoad,
    // Tuner Frequency - the detected pitch in Hz for kParamTunerOn above
    // (0 = no confident pitch right now), which the UI turns into a note
    // name and cents offset for display.
    kParamTunerFrequency,
    // Buffer Size - the host's last-seen block size in samples (the
    // `frames` argument to ChainPlugin::run()), reported back to the UI's
    // status sidebar the same way as the meters above. Purely informational
    // - hosts are free to vary this from call to call, so it reflects
    // "what just happened", not a fixed setting AmpForge controls.
    kParamBufferSize,

    kParamCount
};

// Tempo-sync note divisions, shared between the DSP side (ChainPlugin.cpp,
// which turns a division into an actual ms/Hz value from the host's BPM)
// and the UI side (ChainUI.cpp, which shows the division's label on the
// Sync knob) - same reasoning as kCabinetIRStateKey below. Index 0 ("Free")
// means tempo sync is off; beatMultiplier is relative to a quarter note
// (e.g. 0.5 = an eighth note), so `beatMs * beatMultiplier` gives the
// synced time in milliseconds for any host BPM.
struct SyncDivision
{
    const char* label;
    float beatMultiplier;
};

static constexpr int kSyncDivisionCount = 8;
static const SyncDivision kSyncDivisions[kSyncDivisionCount] =
{
    { "Free", 0.0f },
    { "1/1",  4.0f },
    { "1/2",  2.0f },
    { "1/4",  1.0f },
    { "1/8",  0.5f },
    { "1/16", 0.25f },
    { "1/8.", 0.75f },
    { "1/8T", 1.0f / 3.0f },
};

// The DPF State key that carries the Cabinet block's loaded impulse-
// response file path. Shared between ChainPlugin.cpp (the state's actual
// owner) and ChainUI.cpp (which requests/display it), the same way the
// Parameters enum above is shared, so the two sides can't drift apart.
static const char* const kCabinetIRStateKey = "cabinet_ir_path";

// The DPF State key that carries the NAM block's loaded .nam model file
// path - same reasoning and pattern as kCabinetIRStateKey above.
static const char* const kNamModelStateKey = "nam_model_path";

// NAM model architecture display and load-failure notification used to be
// two more DPF State keys here (kNamModelInfoStateKey, kNamModelErrorStateKey),
// pushed from ChainPlugin::setState() via Plugin::updateStateValue(). That
// push is a no-op on this project's vendored DPF for the VST3 backend (its
// callback is wired to nullptr) and for CLAP (its updateState() is a stub
// that never notifies the UI) - only LV2/Carla-native/standalone jack ever
// delivered it, so on the two most commonly used formats the status
// sidebar never heard about a successful load, and a failed one never got
// a toast. Both are now derived entirely client-side in ChainUI.cpp
// (see readNamArchitecture()) by reading the .nam file directly off of
// the path in kNamModelStateKey below - a path ChainUI always has
// correctly, on every format, since DPF's file-browser glue echoes it to
// the UI directly rather than through this plugin-initiated push.

// The DPF State key used to open a native "pick a file" dialog for
// importing a preset (see ChainUI.cpp's importPresetsFromFile() and
// handleControlBarClick()'s Import button). kStateIsOnlyForUI (see
// ChainPlugin.cpp's initState()) keeps the picked path from ever being
// persisted as part of the session - unlike the Cabinet IR above, it's a
// one-shot action, not a lasting characteristic of the plugin instance.
static const char* const kPresetImportStateKey = "preset_import_path";

END_NAMESPACE_DISTRHO
