#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"
#include <cmath>

/*
 * AmpBlock - the DSP counterpart of the "amp" part of the design.
 *
 * Signal flow:
 *   1) Drive: boost the signal, then soften it with a tanh()-based
 *      soft-clip. This is the simplest way to imitate how a real
 *      tube/transistor "softly" clips the signal when overdriven. tanh(x)
 *      behaves close to x for small values (clean sound) and approaches
 *      -1/+1 for large values (distortion/overdrive feel).
 *   2) Tone stack: three biquad filters in order, Bass -> Mid -> Treble.
 *   3) Volume: final output level.
 *
 * Amp Type (see kAmpVoicings below) picks between a handful of voicings -
 * different Bass/Mid/Treble center frequencies and Q, a per-type headroom
 * trim (how hot the same Drive knob setting hits the clipper), and a
 * touch of clip asymmetry (a cheap, standard trick for tube-like even
 * harmonics: softening only the negative half of the waveform). These are
 * deliberately named for their *character* (Modern/Vintage/Crunch/
 * Hi-Gain), not for a specific real amp brand/circuit - nothing here is a
 * modeled reproduction of any actual amplifier, just a distinct
 * EQ-and-saturation personality. Kept short (name is shown in a
 * fixed-width UI readout - see ChainUI.cpp's isAmpType) rather than more
 * descriptive names like "Vintage Clean"/"British Crunch".
 */

namespace ampforge {

struct AmpVoicing
{
    const char* name;
    float bassFreq, bassQ;
    float midFreq, midQ;
    float trebleFreq, trebleQ;
    float driveTrimDB; // extra headroom (+) or extra heat (-) before the user's own Drive knob
    float asymmetry;   // 0 = symmetric tanh; >0 = softer negative half (even-harmonic "tube" warmth)
};

// Index 0 ("Modern") reproduces AmpBlock's original fixed behavior
// exactly (same centers/Q, zero trim, zero asymmetry) - it's the default,
// so every preset/session saved before Amp Type existed still sounds
// identical.
static constexpr int kAmpVoicingCount = 4;
static constexpr AmpVoicing kAmpVoicings[kAmpVoicingCount] =
{
    { "Modern",  120.0f, 0.707f, 800.0f, 1.0f, 3000.0f, 0.707f,  0.0f, 0.00f },
    { "Vintage", 100.0f, 0.707f, 500.0f, 0.9f, 4500.0f, 0.707f,  6.0f, 0.18f },
    { "Crunch",  150.0f, 0.707f, 900.0f, 1.3f, 2500.0f, 0.707f, -4.0f, 0.25f },
    { "Hi-Gain",  90.0f, 0.707f, 600.0f, 0.8f, 3500.0f, 0.707f, -8.0f, 0.08f },
};

class AmpBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        bass.setSampleRate(sr);
        mid.setSampleRate(sr);
        treble.setSampleRate(sr);
        updateToneStack();
    }

    const char* getName() const override { return "Amp"; }

    // --- Parameter setters: called by the DPF plugin wrapper ---
    void setDriveDB(float db)   { driveDB = db; updateDriveGain(); }
    void setBassDB(float db)    { bassDB = db;   updateToneStack(); }
    void setMidDB(float db)     { midDB = db;    updateToneStack(); }
    void setTrebleDB(float db)  { trebleDB = db; updateToneStack(); }
    void setVolumeDB(float db)  { volumeLinear = std::pow(10.0f, db / 20.0f); }

    void setAmpType(int type)
    {
        ampTypeIndex = (type >= 0 && type < kAmpVoicingCount) ? type : 0;
        updateDriveGain();
        updateToneStack();
    }

    float processSample(float input) override
    {
        // 1) Drive + saturation
        float x = input * driveGainLinear;
        x = saturate(x, kAmpVoicings[ampTypeIndex].asymmetry);

        // 2) Tone stack
        x = bass.process(x);
        x = mid.process(x);
        x = treble.process(x);

        // 3) Output level
        return x * volumeLinear;
    }

private:
    static float saturate(float x, float asymmetry)
    {
        // Only the negative half gets softened - a real tube's asymmetric
        // transfer curve does the same, which is what generates even
        // (not just odd) harmonics and reads as "warmer" than a purely
        // symmetric clipper at the same drive level.
        if (asymmetry <= 0.0f || x >= 0.0f)
            return std::tanh(x);
        return std::tanh(x * (1.0f - asymmetry));
    }

    void updateDriveGain()
    {
        driveGainLinear = std::pow(10.0f, (driveDB + kAmpVoicings[ampTypeIndex].driveTrimDB) / 20.0f);
    }

    void updateToneStack()
    {
        // Typical frequency centers for guitar:
        // Bass: shelf the low frequencies up/down
        // Mid: peak/bell around the voicing's "body" frequency
        // Treble: shelf the high frequencies up/down
        const AmpVoicing& v = kAmpVoicings[ampTypeIndex];
        bass.setParams(Biquad::Type::LowShelf,    v.bassFreq,   bassDB,   v.bassQ);
        mid.setParams(Biquad::Type::Peak,         v.midFreq,    midDB,    v.midQ);
        treble.setParams(Biquad::Type::HighShelf, v.trebleFreq, trebleDB, v.trebleQ);
    }

    Biquad bass, mid, treble;
    int ampTypeIndex = 0;
    float driveGainLinear = 1.0f;
    float volumeLinear    = 1.0f;
    float driveDB = 0.0f, bassDB = 0.0f, midDB = 0.0f, trebleDB = 0.0f;
};

} // namespace ampforge
