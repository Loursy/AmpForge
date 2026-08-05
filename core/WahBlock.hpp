#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"
#include <cmath>
#include <algorithm>

/*
 * WahBlock - a classic wah-wah pedal, modeled as a swept bandpass
 * filter. A real wah pedal has a rocking treadle that sweeps the
 * filter frequency as you play; since we don't have an expression
 * pedal input yet, "PedalPosition" (0..1) stands in for how far the
 * pedal is rocked - heel down (0) to toe down (1). This can be
 * automated from the host for now, and later driven by a MIDI
 * expression pedal or an envelope follower for an "auto-wah" mode.
 */

namespace ampforge {

class WahBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        filter.setSampleRate(sr);
        updateFilter();
    }

    const char* getName() const override { return "Wah"; }

    void setPedalPosition(float pos) { pedalPosition = std::clamp(pos, 0.0f, 1.0f); updateFilter(); }
    void setQ(float q)               { resonanceQ = q; updateFilter(); }

    float processSample(float input) override
    {
        return filter.process(input);
    }

private:
    void updateFilter()
    {
        // Sweep the bandpass center frequency between ~400Hz (heel down)
        // and ~2000Hz (toe down) - the classic wah range.
        const float minFreq = 400.0f;
        const float maxFreq = 2000.0f;
        const float freq = minFreq + pedalPosition * (maxFreq - minFreq);

        // We reuse our Biquad as a Peak filter with a high gain boost and
        // narrow Q, which behaves like a resonant bandpass sweep - the
        // same trick most simple wah implementations use.
        filter.setParams(Biquad::Type::Peak, freq, 15.0f, resonanceQ);
    }

    double sampleRate = 44100.0;
    Biquad filter;
    float pedalPosition = 0.5f;
    float resonanceQ = 3.0f;
};

} // namespace ampforge
