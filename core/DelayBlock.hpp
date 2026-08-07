#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

/*
 * DelayBlock - a classic echo/delay effect using a circular buffer.
 *
 * How it works:
 *   - We keep a buffer of past samples (the "delay line").
 *   - The current input is written into the buffer at a "write" position.
 *   - We read from the buffer at a position that is `delayTime` behind
 *     the write position - that's our delayed (echoed) signal.
 *   - Feedback: some of the delayed signal is mixed back into what we
 *     write next time, so echoes repeat and gradually fade out.
 *   - Mix: how much of the delayed (wet) signal is blended with the
 *     original (dry) signal in the final output.
 */

namespace ampforge {

class DelayBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;

        // Allocate enough buffer for the longest delay we'll allow (2 seconds)
        const size_t maxSamples = static_cast<size_t>(sr * 2.0) + 1;
        buffer.assign(maxSamples, 0.0f);
        writeIndex = 0;
    }

    const char* getName() const override { return "Delay"; }

    void setDelayTimeMs(float ms)  { delayTimeMs = ms; }
    void setFeedback(float fb)     { feedback = std::clamp(fb, 0.0f, 0.95f); } // clamped to avoid runaway feedback
    void setMix(float m)           { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        if (buffer.empty())
            return input; // setSampleRate hasn't been called yet - pass through safely

        const size_t bufferSize = buffer.size();

        // Convert delay time (ms) to samples, then read that far back in
        // the circular buffer. Linear interpolation lets us use fractional
        // sample delays instead of being limited to whole numbers.
        const float delaySamples = (delayTimeMs / 1000.0f) * static_cast<float>(sampleRate);

        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(bufferSize);

        const size_t readIndex0 = static_cast<size_t>(readPos) % bufferSize;
        const size_t readIndex1 = (readIndex0 + 1) % bufferSize;
        const float frac = readPos - std::floor(readPos);

        const float delayedSample = buffer[readIndex0] * (1.0f - frac) + buffer[readIndex1] * frac;

        // Write input + feedback portion of the delayed signal, so echoes
        // repeat and decay over time. Flushed to guard against denormals
        // once the echoes decay toward silence (see Denormal.hpp).
        buffer[writeIndex] = flushDenormal(input + delayedSample * feedback);

        writeIndex = (writeIndex + 1) % bufferSize;

        // Blend dry (original) and wet (delayed) signal
        return input * (1.0f - mix) + delayedSample * mix;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    size_t writeIndex = 0;

    float delayTimeMs = 300.0f;
    float feedback = 0.3f;
    float mix = 0.3f;
};

} // namespace ampforge
