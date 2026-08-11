#pragma once

/*
 * ChainPresets.hpp - the factory preset data table, shared between the
 * DSP side (ChainPlugin.cpp, which reports these to the host's program
 * list) and the UI side (ChainUI.cpp, which shows its own in-window
 * preset bar so presets are reachable without digging through the
 * host's own program dropdown).
 */

#include "ChainParameters.hpp"

START_NAMESPACE_DISTRHO

// --- Factory presets ---
//
// Each preset is just a full snapshot of every parameter's value, in
// the same order as the Parameters enum above. loadProgram() applies
// this snapshot by calling setParameterValue() for every entry, which
// keeps the chain's internal state and the EffectChain enabled/position
// flags perfectly in sync - the same code path a host uses when it
// moves a knob. (The Cabinet block's loaded IR file, if any, is DPF
// State rather than a parameter, so it isn't part of this snapshot -
// factory presets never carry a bundled IR.)
//
// Chain order (the *Position values) is kept at the default pedalboard
// layout for every preset here - only which blocks are on/off and their
// tone-shaping parameters differ between presets. That default layout is
// Gate(0) -> Comp(1) -> Wah(2) -> Screamer(3) -> Distortion(4) -> Amp(5)
// -> Cabinet(6) -> NAM(7) -> Chorus(8) -> Phaser(9) -> Tremolo(10) ->
// Delay(11) -> Reverb(12). NAM sits right after Cabinet - tonally
// alongside Amp/Cabinet, since it's a capture of a real amp/pedal/cab -
// rather than at the very end, even though it's off (and so functionally
// inert) in every preset below.
//
// Gain-staging rule these all follow: AmpBlock's Drive feeds a tanh()
// soft-clip, and Screamer/Distortion are hard clippers ahead of it (see
// their headers in core/). Stacking two hard clippers at high Drive back
// to back (Screamer into Distortion, or either one dimed straight into a
// maxed Amp Drive) mostly just flattens the waveform into a fizzy square
// wave rather than adding useful character - so each preset below leans
// on ONE stage for most of the grit and keeps the others as a modest
// boost/tone-shaper, and none of the output-level knobs (Screamer/
// Distortion Level, Amp Volume) stack positive dB on top of each other,
// since nothing downstream clips an overs signal for us.
//
// Concretely, that "one stage" still needs its own Drive kept in a range
// where it tracks a note's decay instead of just pinning at its clip
// ceiling for the note's entire audible length: verified by feeding a
// synthesized decaying note through each preset's active blocks and
// checking the output actually decays (not stuck at a squared-off max
// forever) and its peak doesn't run away past the input's own headroom.
// Screamer's clamp is +-0.7, so Drive much past ~2 already hard-clips a
// typical picked note almost everywhere except its very tail; Distortion's
// clamp is tighter still (+0.55/-0.85), so its usable Drive range is lower
// again, not higher, despite the larger nominal range in its header comment.
// Amp Drive's tanh is comparatively forgiving on its own, but every dB of
// Bass/Mid/Treble boost downstream eats back into that same headroom, so a
// hot mid-boost preset needs a correspondingly lower Drive than a flat-EQ
// one to land in the same place.
static constexpr uint32_t kProgramCount = 7;

struct PresetDefinition
{
    const char* name;
    float values[kParamCount];
};

// clang-format off
static const PresetDefinition kPresets[kProgramCount] =
{
    // "Chimey Clean" - Amp Drive at 0dB (unity gain into the tanh stage,
    // so it stays clean regardless of how hot the input is), gentle
    // compression for sustain, a scooped-mid/chimey-treble EQ, a touch of
    // vibrato (tremolo) and a subtle spring-like reverb - the classic
    // clean American amp sound. (Named for the tone, not a brand - the
    // Amp block is a generic saturator/EQ, not a modeled Fender circuit.)
    {
        "Chimey Clean",
        {
            /* Gate       on,pos,thr,atk,rel      */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       on,pos,thr,ratio,atk,rel,makeup */ 1.0f, 1.0f, -22.0f, 2.5f, 8.0f, 100.0f, 1.0f,
            /* Wah        on,pos,pedal,q  */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   on,pos,drive,tone,level */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        pos,drive,bass,mid,treble,vol */ 5.0f, 0.0f, 1.0f, -2.0f, 3.0f, 0.5f,
            /* Chorus     on,pos,rate,depth,mix */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     on,pos,rate,depth,mix */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    on,pos,rate,depth */ 1.0f, 10.0f, 4.0f, 0.25f,
            /* Delay      on,pos,time,fb,mix */ 0.0f, 11.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     on,pos,room,damp,mix */ 1.0f, 12.0f, 0.3f, 0.6f, 0.18f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion on,pos,drive,tone,level,bypass */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    on,pos,mix,level,bypass */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 40.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "British Crunch" - a modest Screamer boost (just enough to push the
    // amp, barely engaging its own clipper) into a hot, mid-forward Amp
    // Drive that does the actual saturating - classic cranked-British-stack
    // rhythm/lead tone, with a touch of room reverb. (Named for the tone,
    // not a brand - the Amp block is a generic saturator/EQ, not a modeled
    // Marshall circuit.) Amp Drive is tuned so the tanh saturator still
    // tracks pick dynamics instead of pinning flat at max the instant a
    // string is struck - a driveDB much past here starts to just square the
    // waveform off and swallow the note's natural decay (verified by
    // measuring output crest factor and decay time against a synthesized
    // decaying note - see the project's DSP tuning notes).
    {
        "British Crunch",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 1.5f, 0.6f, 1.0f,
            /* Amp        */ 5.0f, 9.0f, 2.0f, 3.0f, 1.5f, 0.0f,
            /* Chorus     */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 11.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 1.0f, 12.0f, 0.4f, 0.5f, 0.15f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 40.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "Shredder Lead" - the Screamer is dialed back to a light boost
    // (its own hard clipper barely engages) so the *Amp's* smooth tanh
    // saturation - not a squared-off pedal waveform - is what actually
    // shapes the tone; that's what makes a lead sing/sustain instead of
    // buzzing apart under fast picking or tapping. A sustain-friendly
    // compressor evens out pick attack, boosted mid keeps it cutting
    // through, and a tamed slapback delay + reverb tail sits it in the
    // mix without smearing fast runs. Amp Drive is tuned the same way as
    // British Crunch above - hot enough to sing, not so hot the tanh
    // stage pins flat and chokes the decay.
    {
        "Shredder Lead",
        {
            /* Gate       */ 1.0f, 0.0f, -45.0f, 2.0f, 100.0f,
            /* Comp       */ 1.0f, 1.0f, -24.0f, 3.0f, 5.0f, 150.0f, 2.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 1.3f, 0.6f, 1.5f,
            /* Amp        */ 5.0f, 9.0f, 2.0f, 4.0f, 1.5f, 0.0f,
            /* Chorus     */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 1.0f, 11.0f, 350.0f, 0.2f, 0.18f,
            /* Reverb     */ 1.0f, 12.0f, 0.5f, 0.45f, 0.2f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 40.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "Metal Rhythm" - tight noise gate, Distortion as the *only* gain
    // stage (Screamer stays off entirely here rather than "barely on" -
    // stacking it with Distortion measured no differently than a single
    // stage doing the same job, just with more places for gain-staging to
    // go wrong), a deeper mid scoop and rolled-off treble to keep it tight
    // instead of fizzy, no time-based effects (keeps palm-muted chugs
    // tight). Distortion's own hard clip thresholds are much tighter than
    // Screamer's, so its Drive lives on a much smaller scale - a value
    // that reads as "light" on Screamer would still slam Distortion into a
    // fully squared-off wall.
    {
        "Metal Rhythm",
        {
            /* Gate       */ 1.0f, 0.0f, -40.0f, 1.0f, 60.0f,
            /* Comp       */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 0.0f, 3.0f, 1.2f, 0.4f, 0.0f,
            /* Amp        */ 5.0f, 6.0f, 2.0f, -3.0f, 0.0f, 1.0f,
            /* Chorus     */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 11.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 0.0f, 12.0f, 0.5f, 0.5f, 0.3f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion on,pos,drive,tone,level,bypass */ 1.0f, 4.0f, 2.5f, 0.3f, -1.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 60.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "Ambient Shoegaze" - clean-ish amp with a light mid scoop for
    // space, compressor for even sustain, chorus + phaser stacked for a
    // wide, swirling texture, long delay and a big, dark reverb tail.
    {
        "Ambient Shoegaze",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 1.0f, 1.0f, -24.0f, 3.0f, 15.0f, 200.0f, 2.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        */ 5.0f, 6.0f, 1.5f, -2.0f, 1.5f, 0.0f,
            /* Chorus     */ 1.0f, 8.0f, 0.4f, 8.0f, 0.6f,
            /* Phaser     */ 1.0f, 9.0f, 0.2f, 0.5f, 0.3f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 1.0f, 11.0f, 500.0f, 0.45f, 0.35f,
            /* Reverb     */ 1.0f, 12.0f, 0.85f, 0.3f, 0.5f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 40.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "Funk Clean" - snappy compression for percussive clean playing,
    // Wah enabled (rock the "Wah Pedal" parameter while playing), Amp
    // Drive kept near unity for a genuinely clean, bright tone, subtle
    // room reverb only.
    {
        "Funk Clean",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 1.0f, 1.0f, -22.0f, 5.0f, 3.0f, 60.0f, 3.0f,
            /* Wah        */ 1.0f, 2.0f, 0.5f, 4.5f,
            /* Screamer   */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        */ 5.0f, 1.0f, 0.0f, 1.0f, 3.0f, 0.0f,
            /* Chorus     */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 11.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 1.0f, 12.0f, 0.25f, 0.6f, 0.15f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 40.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },

    // "Blues Rock Sustain" - the singing, long-sustain blues-rock lead tone
    // (think Gary Moore's "Parisienne Walkways"): a light Screamer boost
    // pushes a hot, heavily mid-forward Amp Drive (mid is what makes a
    // lead "sing" and cut through rather than just distort), a
    // low-threshold/slow-release Comp keeps notes blooming instead of
    // decaying, and a plate-style reverb plus a low, subtle delay give it
    // space without smearing bends and vibrato. No Distortion stage - the
    // Amp's smooth tanh saturation is what carries the sustain, same
    // reasoning as "Shredder Lead" above, just tuned for long, vocal notes
    // instead of fast picking. Amp Drive is intentionally well short of
    // where the tanh stage would pin flat - a note needs *some* remaining
    // headroom to actually decay through as it sustains, or the "sustain"
    // is really just a wall of constant-level buzz that stops dead the
    // instant the gate closes rather than singing and tapering off.
    {
        "Blues Rock Sustain",
        {
            /* Gate       */ 1.0f, 0.0f, -45.0f, 3.0f, 120.0f,
            /* Comp       */ 1.0f, 1.0f, -28.0f, 4.0f, 5.0f, 200.0f, 3.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 1.3f, 0.55f, 2.0f,
            /* Amp        */ 5.0f, 9.0f, 3.0f, 3.5f, 1.0f, 0.0f,
            /* Chorus     */ 0.0f, 8.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 9.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 10.0f, 5.0f, 0.5f,
            /* Delay      */ 1.0f, 11.0f, 380.0f, 0.2f, 0.15f,
            /* Reverb     */ 1.0f, 12.0f, 0.5f, 0.4f, 0.25f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
            /* Cabinet    */ 0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
            /* Gate Range */ 45.0f,
            /* Input Gain */ 0.0f,
            /* Delay/Tremolo/Chorus sync, Tuner on, Amp Type - all left at
               their zero defaults (Free/off/off/"Modern") in every preset */
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* NAM on,pos,inputTrim,outputLevel,mix,bypass */ 0.0f, 7.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            /* Amp on,bypass */                                1.0f, 0.0f,
            /* Autotune on,pos,key,scale,speed,bypass */       0.0f, 13.0f, 0.0f, 0.0f, 0.5f, 0.0f,
            /* De-esser on,pos,freq,threshold,reduction,bypass */ 0.0f, 14.0f, 6000.0f, -30.0f, 12.0f, 0.0f,
        }
    },
};
// clang-format on

END_NAMESPACE_DISTRHO
