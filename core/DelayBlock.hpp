#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <vector>
#include <array>
#include <atomic>
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
    // Builds the new delay line fully in the *inactive* slot, then a
    // single atomic store publishes it - processSample() on the audio
    // thread never sees a buffer mid-resize racing against a writeIndex
    // that's stale for the new size. setSampleRate() isn't guaranteed to
    // run on the audio thread: DPF calls sampleRateChanged() again any
    // time the host's rate changes mid-session (e.g. switching audio
    // interfaces while the plugin is already running), while
    // processSample() keeps running concurrently on the realtime audio
    // thread - see ReverbBlock.hpp's CombFilter for the same reasoning
    // (this exact race is what crashed there with a std::vector
    // out-of-bounds abort).
    void setSampleRate(double sr) override
    {
        sampleRate = sr;

        // Allocate enough buffer for the longest delay we'll allow (2 seconds)
        const size_t maxSamples = static_cast<size_t>(sr * 2.0) + 1;
        const int active = activeSlot.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        buffers[next].assign(maxSamples, 0.0f);
        writeIndices[next] = 0;
        activeSlot.store(static_cast<int>(next), std::memory_order_release);
    }

    const char* getName() const override { return "Delay"; }

    void setDelayTimeMs(float ms)  { delayTimeMs = ms; }
    void setFeedback(float fb)     { feedback = std::clamp(fb, 0.0f, 0.95f); } // clamped to avoid runaway feedback
    void setMix(float m)           { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return input; // setSampleRate hasn't been called yet - pass through safely

        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        size_t& writeIndex = writeIndices[static_cast<size_t>(active)];
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
    std::array<std::vector<float>, 2> buffers;
    std::array<size_t, 2> writeIndices{{0, 0}};
    std::atomic<int> activeSlot{-1};

    float delayTimeMs = 300.0f;
    float feedback = 0.3f;
    float mix = 0.3f;
};

} // namespace ampforge
