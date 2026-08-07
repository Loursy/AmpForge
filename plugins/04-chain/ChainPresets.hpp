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
// moves a knob.
//
// Chain order (the *Position values) is kept at the default pedalboard
// layout for every preset here - only which blocks are on/off and their
// tone-shaping parameters differ between presets. That default layout is
// Gate(0) -> Comp(1) -> Wah(2) -> Screamer(3) -> Distortion(4) -> Amp(5)
// -> Chorus(6) -> Phaser(7) -> Tremolo(8) -> Delay(9) -> Reverb(10).
static constexpr uint32_t kProgramCount = 6;

struct PresetDefinition
{
    const char* name;
    float values[kParamCount];
};

// clang-format off
static const PresetDefinition kPresets[kProgramCount] =
{
    // "Fender Clean" - light compression for sustain, a scooped-mid EQ,
    // a touch of vibrato (tremolo) and a subtle spring-like reverb -
    // the classic clean American amp sound.
    {
        "Fender Clean",
        {
            /* Gate       on,pos,thr,atk,rel      */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       on,pos,thr,ratio,atk,rel,makeup */ 1.0f, 1.0f, -20.0f, 3.0f, 5.0f, 80.0f, 2.0f,
            /* Wah        on,pos,pedal,q  */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   on,pos,drive,tone,level */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        pos,drive,bass,mid,treble,vol */ 5.0f, 3.0f, 2.0f, -3.0f, 4.0f, 0.0f,
            /* Chorus     on,pos,rate,depth,mix */ 0.0f, 6.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     on,pos,rate,depth,mix */ 0.0f, 7.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    on,pos,rate,depth */ 1.0f, 8.0f, 4.0f, 0.3f,
            /* Delay      on,pos,time,fb,mix */ 0.0f, 9.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     on,pos,room,damp,mix */ 1.0f, 10.0f, 0.3f, 0.6f, 0.2f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion on,pos,drive,tone,level,bypass */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
        }
    },

    // "Marshall Rock" - Screamer pushing a cranked British-voiced amp,
    // classic mid-forward rock rhythm/lead tone with a touch of room reverb.
    {
        "Marshall Rock",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 6.0f, 0.6f, 0.0f,
            /* Amp        */ 5.0f, 18.0f, 3.0f, 4.0f, 2.0f, 0.0f,
            /* Chorus     */ 0.0f, 6.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 7.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 8.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 9.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 1.0f, 10.0f, 0.4f, 0.5f, 0.15f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
        }
    },

    // "Shredder Lead" - high gain solo tone: noise gate to keep it tight,
    // hot Screamer into a saturated amp, boosted output level, and a
    // slapback-ish delay + reverb tail for a solo that sits in the mix.
    {
        "Shredder Lead",
        {
            /* Gate       */ 1.0f, 0.0f, -45.0f, 2.0f, 100.0f,
            /* Comp       */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 12.0f, 0.7f, 3.0f,
            /* Amp        */ 5.0f, 30.0f, 2.0f, 6.0f, 3.0f, 3.0f,
            /* Chorus     */ 0.0f, 6.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 7.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 8.0f, 5.0f, 0.5f,
            /* Delay      */ 1.0f, 9.0f, 350.0f, 0.25f, 0.2f,
            /* Reverb     */ 1.0f, 10.0f, 0.6f, 0.4f, 0.25f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
        }
    },

    // "Metal Rhythm" - tight noise gate, dark/scooped tone, a Distortion
    // stage stacked after Screamer for a tighter, more aggressive clip
    // than either pedal alone, no time-based effects (keeps palm-muted
    // chugs tight).
    {
        "Metal Rhythm",
        {
            /* Gate       */ 1.0f, 0.0f, -40.0f, 1.0f, 80.0f,
            /* Comp       */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 1.0f, 3.0f, 8.0f, 0.4f, 0.0f,
            /* Amp        */ 5.0f, 34.0f, 5.0f, -2.0f, 1.0f, 0.0f,
            /* Chorus     */ 0.0f, 6.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 7.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 8.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 9.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 0.0f, 10.0f, 0.5f, 0.5f, 0.3f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion on,pos,drive,tone,level,bypass */ 1.0f, 4.0f, 10.0f, 0.35f, -2.0f, 0.0f,
        }
    },

    // "Ambient Shoegaze" - clean-ish amp, compressor for even sustain,
    // chorus + phaser stacked for a wide, swirling texture, long delay
    // and a big, dark reverb tail.
    {
        "Ambient Shoegaze",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 1.0f, 1.0f, -24.0f, 3.0f, 15.0f, 200.0f, 2.0f,
            /* Wah        */ 0.0f, 2.0f, 0.5f, 3.0f,
            /* Screamer   */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        */ 5.0f, 5.0f, 1.0f, 0.0f, 2.0f, 0.0f,
            /* Chorus     */ 1.0f, 6.0f, 0.4f, 8.0f, 0.6f,
            /* Phaser     */ 1.0f, 7.0f, 0.2f, 0.5f, 0.3f,
            /* Tremolo    */ 0.0f, 8.0f, 5.0f, 0.5f,
            /* Delay      */ 1.0f, 9.0f, 500.0f, 0.45f, 0.35f,
            /* Reverb     */ 1.0f, 10.0f, 0.85f, 0.3f, 0.5f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
        }
    },

    // "Funk Clean" - snappy compression for percussive clean playing,
    // Wah enabled (rock the "Wah Pedal" parameter while playing), bright
    // clean amp tone, subtle room reverb only.
    {
        "Funk Clean",
        {
            /* Gate       */ 0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
            /* Comp       */ 1.0f, 1.0f, -22.0f, 5.0f, 3.0f, 60.0f, 4.0f,
            /* Wah        */ 1.0f, 2.0f, 0.5f, 4.0f,
            /* Screamer   */ 0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
            /* Amp        */ 5.0f, 2.0f, 0.0f, 1.0f, 3.0f, 0.0f,
            /* Chorus     */ 0.0f, 6.0f, 1.0f, 5.0f, 0.5f,
            /* Phaser     */ 0.0f, 7.0f, 0.5f, 0.7f, 0.5f,
            /* Tremolo    */ 0.0f, 8.0f, 5.0f, 0.5f,
            /* Delay      */ 0.0f, 9.0f, 300.0f, 0.3f, 0.3f,
            /* Reverb     */ 1.0f, 10.0f, 0.25f, 0.6f, 0.15f,
            /* Bypass: gate,comp,wah,screamer,chorus,phaser,tremolo,delay,reverb */ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            /* Distortion */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
        }
    },
};
// clang-format on

END_NAMESPACE_DISTRHO
