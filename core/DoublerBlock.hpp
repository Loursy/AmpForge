#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

/*
 * DoublerBlock - Automatic Double Tracking (ADT): simulates a second
 * vocal take layered under the first, the classic way to thicken a lead
 * vocal without actually recording it twice.
 *
 * Same underlying technique as ChorusBlock.hpp (a delay line whose delay
 * time wobbles over time), but tuned for a different character: a longer
 * base delay (still short enough to fuse into "one thicker voice" rather
 * than a discrete echo) and, instead of Chorus's clean sine LFO, a
 * gently low-pass-filtered noise source modulating the delay - a real
 * second take doesn't drift in a perfect sine wave, it wanders
 * unevenly with natural timing imprecision, and filtered noise reads as
 * that "human" wobble instead of an obviously swirly chorus.
 */

namespace ampforge {

class DoublerBlock : public AudioBlock
{
public:
    // Same double-buffered swap-on-resize pattern as ChorusBlock/
    // DelayBlock - setSampleRate() isn't guaranteed to run on the audio
    // thread, so the new buffer is built fully in the *inactive* slot and
    // published with one atomic store.
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        const size_t maxSamples = static_cast<size_t>(sr * 0.1) + 1; // 100ms max, generous for a doubler's delay range
        const int active = activeSlot.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        buffers[next].assign(maxSamples, 0.0f);
        writeIndices[next] = 0;
        activeSlot.store(static_cast<int>(next), std::memory_order_release);
        noiseState = 0.0f;
        rngState = 0x9E3779B9u;
    }

    const char* getName() const override { return "Doubler"; }

    void setDelayMs(float ms) { delayMs = std::clamp(ms, 5.0f, 50.0f); }
    void setDepthMs(float ms) { depthMs = std::clamp(ms, 0.0f, 15.0f); }
    void setMix(float m)      { mix = std::clamp(m, 0.0f, 1.0f); }

    // See AudioBlock::reset()'s comment - mutates the active buffer in
    // place, safe for the same reason DelayBlock::reset() is.
    void reset() override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return;
        std::fill(buffers[static_cast<size_t>(active)].begin(), buffers[static_cast<size_t>(active)].end(), 0.0f);
        writeIndices[static_cast<size_t>(active)] = 0;
        noiseState = 0.0f;
    }

    float processSample(float input) override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return input;

        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        size_t& writeIndex = writeIndices[static_cast<size_t>(active)];
        const size_t bufferSize = buffer.size();

        // Filtered-noise wobble: a fresh random value every sample,
        // smoothed by a one-pole low-pass so it wanders slowly (a few Hz
        // of effective bandwidth) instead of buzzing - see the class
        // comment for why this reads as more "human" than Chorus's sine.
        const float whiteNoise = nextNoise();
        noiseState += (whiteNoise - noiseState) * kNoiseSmoothing;

        const float delaySamples = ((delayMs + noiseState * depthMs) / 1000.0f) * static_cast<float>(sampleRate);

        float readPos = static_cast<float>(writeIndex) - delaySamples;
        while (readPos < 0.0f)
            readPos += static_cast<float>(bufferSize);

        const size_t readIndex0 = static_cast<size_t>(readPos) % bufferSize;
        const size_t readIndex1 = (readIndex0 + 1) % bufferSize;
        const float frac = readPos - std::floor(readPos);

        const float delayedSample = buffer[readIndex0] * (1.0f - frac) + buffer[readIndex1] * frac;

        buffer[writeIndex] = input;
        writeIndex = (writeIndex + 1) % bufferSize;

        return input * (1.0f - mix) + flushDenormal(delayedSample) * mix;
    }

private:
    // A tiny xorshift PRNG - doesn't need to be cryptographically sound,
    // just cheap and free of audible periodicity at audio rates.
    float nextNoise()
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        // Map to -1..1
        return (static_cast<float>(rngState) / static_cast<float>(0xFFFFFFFFu)) * 2.0f - 1.0f;
    }

    static constexpr float kNoiseSmoothing = 0.002f; // lower = slower/smoother wander

    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> buffers;
    std::array<size_t, 2> writeIndices{{0, 0}};
    std::atomic<int> activeSlot{-1};

    uint32_t rngState = 0x9E3779B9u;
    float noiseState = 0.0f;
    float delayMs = 20.0f;
    float depthMs = 4.0f;
    float mix = 0.4f;
};

} // namespace ampforge
