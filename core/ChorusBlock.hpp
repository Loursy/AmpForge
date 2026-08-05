#pragma once
#include "AudioBlock.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

/*
 * ChorusBlock - thickens the sound by mixing in a copy of the signal
 * delayed by a small, constantly-wobbling amount (a few milliseconds,
 * modulated by a slow LFO - a low-frequency oscillator). The tiny,
 * shifting pitch difference between the dry and delayed copies is
 * what creates the "chorus" (multiple-instruments-at-once) effect.
 */

namespace ampforge {

class ChorusBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        const size_t maxSamples = static_cast<size_t>(sr * 0.05) + 1; // 50ms max
        buffer.assign(maxSamples, 0.0f);
        writeIndex = 0;
        lfoPhase = 0.0f;
    }

    const char* getName() const override { return "Chorus"; }

    void setRateHz(float hz)  { rateHz = hz; }
    void setDepthMs(float ms) { depthMs = ms; }
    void setMix(float m)      { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        if (buffer.empty())
            return input;

        const size_t bufferSize = buffer.size();

        lfoPhase += rateHz / static_cast<float>(sampleRate);
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;
        const float lfo = std::sin(2.0f * static_cast<float>(M_PI) * lfoPhase); // -1..1

        const float delayMs = (depthMs * 0.5f) + (lfo * depthMs * 0.5f);
        const float delaySamples = (delayMs / 1000.0f) * static_cast<float>(sampleRate);

        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(bufferSize);

        const size_t readIndex0 = static_cast<size_t>(readPos) % bufferSize;
        const size_t readIndex1 = (readIndex0 + 1) % bufferSize;
        const float frac = readPos - std::floor(readPos);

        const float delayedSample = buffer[readIndex0] * (1.0f - frac) + buffer[readIndex1] * frac;

        buffer[writeIndex] = input;
        writeIndex = (writeIndex + 1) % bufferSize;

        return input * (1.0f - mix) + delayedSample * mix;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    size_t writeIndex = 0;

    float lfoPhase = 0.0f;
    float rateHz = 1.0f;
    float depthMs = 5.0f;
    float mix = 0.5f;
};

} // namespace ampforge