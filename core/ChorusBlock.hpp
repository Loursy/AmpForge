#pragma once
#include "AudioBlock.hpp"
#include <vector>
#include <array>
#include <atomic>
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
    // Builds the new delay line fully in the *inactive* slot, then a
    // single atomic store publishes it - same cross-thread hot-swap
    // reasoning as ReverbBlock.hpp's CombFilter (setSampleRate() isn't
    // guaranteed to run on the audio thread, and a plain buffer.assign()
    // here used to be visible mid-resize from processSample() running
    // concurrently on the realtime audio thread - see that file's comment
    // for the crash this caused).
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        const size_t maxSamples = static_cast<size_t>(sr * 0.05) + 1; // 50ms max, plenty for chorus
        const int active = activeSlot.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        buffers[next].assign(maxSamples, 0.0f);
        writeIndices[next] = 0;
        activeSlot.store(static_cast<int>(next), std::memory_order_release);
        lfoPhase = 0.0f;
    }

    const char* getName() const override { return "Chorus"; }

    void setRateHz(float hz)  { rateHz = hz; }
    void setDepthMs(float ms) { depthMs = ms; }
    void setMix(float m)      { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return input;

        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        size_t& writeIndex = writeIndices[static_cast<size_t>(active)];
        const size_t bufferSize = buffer.size();

        // LFO: a slow sine wave that sweeps the delay time up and down
        lfoPhase += rateHz / static_cast<float>(sampleRate);
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;
        const float lfo = std::sin(2.0f * static_cast<float>(M_PI) * lfoPhase); // -1..1

        // Center the modulation around half the depth, so delay time stays positive
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
    std::array<std::vector<float>, 2> buffers;
    std::array<size_t, 2> writeIndices{{0, 0}};
    std::atomic<int> activeSlot{-1};

    float lfoPhase = 0.0f;
    float rateHz = 1.0f;
    float depthMs = 5.0f;
    float mix = 0.5f;
};

} // namespace ampforge
