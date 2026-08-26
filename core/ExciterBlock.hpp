#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"
#include "Denormal.hpp"
#include <cmath>
#include <algorithm>

/*
 * ExciterBlock - a "vocal exciter"/air-band enhancer: adds brightness
 * and presence above a chosen frequency without just boosting a shelf
 * EQ (which only makes existing high content louder - if there isn't
 * much energy up there to begin with, a shelf boost doesn't add the
 * "shimmer" a real exciter gives).
 *
 * The classic technique (same idea as a hardware Aphex Aural Exciter):
 * split off everything above Frequency with a high-pass filter, run
 * *just that band* through a gentle soft-clip saturator to generate new
 * harmonics that weren't in the original signal at all, then add that
 * harmonic-enhanced band back on top of the full, untouched dry signal.
 * Additive, not a dry/wet crossfade - Amount controls how much "air" gets
 * layered on, not how much of the original gets replaced.
 */

namespace ampforge {

class ExciterBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        highpass.setSampleRate(sr);
        updateFilter();
    }

    const char* getName() const override { return "Exciter"; }

    void setFrequency(float hz)
    {
        frequency = std::clamp(hz, 1500.0f, 14000.0f);
        updateFilter();
    }
    void setAmount(float amount01) { amount = std::clamp(amount01, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        const float highBand = highpass.process(input);

        // Fixed, generous drive into the tanh - Amount below controls how
        // much of this harmonic-rich signal gets layered back in, not how
        // hard it's driven; that keeps the *character* of the added
        // harmonics consistent regardless of how subtle the effect is set.
        const float excited = std::tanh(highBand * kExciterDrive);

        return flushDenormal(input + excited * amount * kMaxAddGain);
    }

private:
    void updateFilter()
    {
        // Q left at a plain 0.707 (Butterworth-flat) - this is a
        // brightness enhancer, not a surgical filter, so there's no
        // reason to expose Q as another knob (same reasoning DeEsserBlock
        // gives for its own fixed Q).
        highpass.setParams(Biquad::Type::HighPass, frequency, 0.0f, 0.707f);
    }

    static constexpr float kExciterDrive = 6.0f;
    // Caps how much of the harmonic band can be added back at Amount=1 -
    // tanh output is already bounded to +-1, this just keeps a fully
    // dimed Amount knob from being able to double the signal's peak level.
    static constexpr float kMaxAddGain = 0.35f;

    double sampleRate = 44100.0;
    Biquad highpass;
    float frequency = 6000.0f;
    float amount = 0.3f;
};

} // namespace ampforge
