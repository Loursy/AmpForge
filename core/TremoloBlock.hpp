#pragma once
#include "AudioBlock.hpp"
#include <cmath>
#include <algorithm>

/*
 * TremoloBlock - rhythmically raises and lowers the volume using a
 * slow LFO (low-frequency oscillator). This is the classic "pulsing"
 * surf/indie guitar sound.
 */

namespace ampforge {

class TremoloBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        lfoPhase = 0.0f;
    }

    const char* getName() const override { return "Tremolo"; }

    void setRateHz(float hz) { rateHz = hz; }
    void setDepth(float d)   { depth = std::clamp(d, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        lfoPhase += rateHz / static_cast<float>(sampleRate);
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;

        const float lfo = 0.5f + 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * lfoPhase); // 0..1

        // depth=0 -> gain always 1.0 (no effect). depth=1 -> gain swings all the way down to 0.
        const float gain = 1.0f - depth * (1.0f - lfo);

        return input * gain;
    }

private:
    double sampleRate = 44100.0;
    float lfoPhase = 0.0f;
    float rateHz = 5.0f;
    float depth = 0.5f;
};

} // namespace ampforge
