#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"
#include <cmath>

/*
 * AmpBlock - the DSP counterpart of the "amp" part of the design.
 *
 * Signal flow:
 *   1) Drive: boost the signal, then soften it with tanh().
 *      This is the simplest way to imitate how a real tube/transistor
 *      "softly" clips the signal when overdriven. tanh(x) behaves close
 *      to x for small values (clean sound) and approaches -1/+1 for
 *      large values (distortion/overdrive feel).
 *   2) Tone stack: three biquad filters in order, Bass -> Mid -> Treble.
 *   3) Volume: final output level.
 */

namespace ampforge {

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
    void setDriveDB(float db)   { driveGainLinear = std::pow(10.0f, db / 20.0f); }
    void setBassDB(float db)    { bassDB = db;   updateToneStack(); }
    void setMidDB(float db)     { midDB = db;    updateToneStack(); }
    void setTrebleDB(float db)  { trebleDB = db; updateToneStack(); }
    void setVolumeDB(float db)  { volumeLinear = std::pow(10.0f, db / 20.0f); }

    float processSample(float input) override
    {
        // 1) Drive + saturation
        float x = input * driveGainLinear;
        x = std::tanh(x);

        // 2) Tone stack
        x = bass.process(x);
        x = mid.process(x);
        x = treble.process(x);

        // 3) Output level
        return x * volumeLinear;
    }

private:
    void updateToneStack()
    {
        // Typical frequency centers for guitar:
        // Bass: shelf the low frequencies up/down
        // Mid: peak/bell around 800Hz, the "body" of the guitar tone
        // Treble: shelf the high frequencies up/down
        bass.setParams(Biquad::Type::LowShelf,    120.0f,  bassDB,   0.707f);
        mid.setParams(Biquad::Type::Peak,         800.0f,  midDB,    1.0f);
        treble.setParams(Biquad::Type::HighShelf, 3000.0f, trebleDB, 0.707f);
    }

    Biquad bass, mid, treble;
    float driveGainLinear = 1.0f;
    float volumeLinear    = 1.0f;
    float bassDB = 0.0f, midDB = 0.0f, trebleDB = 0.0f;
};

} // namespace ampforge
