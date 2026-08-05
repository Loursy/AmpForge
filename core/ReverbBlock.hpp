#pragma once
#include "AudioBlock.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

/*
 * ReverbBlock - a simple Schroeder-style reverb.
 *
 * Real reverb is the sum of thousands of tiny reflections bouncing
 * around a room. Schroeder's classic trick (1960s) approximates that
 * with just a handful of filters:
 *
 *   - Several parallel "comb filters" (delay + feedback + damping),
 *     each with a different, prime-ish delay length, so their echoes
 *     don't line up and create an unnatural periodic "flutter".
 *   - A couple of "allpass filters" in series afterward, which don't
 *     change the tone but smear/diffuse the sound in time, so the
 *     combs sound like a continuous wash instead of distinct echoes.
 */

namespace ampforge {

// One comb filter: delay + feedback + a simple damping (lowpass) in the
// feedback path, so high frequencies decay faster than low ones - like
// they do in a real room.
class CombFilter
{
public:
    void setDelaySamples(int samples)
    {
        buffer.assign(static_cast<size_t>(std::max(1, samples)), 0.0f);
        index = 0;
        filterStore = 0.0f;
    }

    void setFeedback(float fb) { feedback = fb; }
    void setDamping(float d)   { damping = d; }

    float process(float input)
    {
        // Safety check: if setDelaySamples() hasn't been called yet (buffer
        // still empty), just pass the signal through instead of indexing
        // into an empty vector - this can happen if the host processes
        // audio before it reports the sample rate.
        if (buffer.empty())
            return input;

        const float output = buffer[index];

        // One-pole lowpass in the feedback path - this is what makes
        // high frequencies die out faster than low ones.
        filterStore = output * (1.0f - damping) + filterStore * damping;

        buffer[index] = input + filterStore * feedback;
        index = (index + 1) % buffer.size();

        return output;
    }

private:
    std::vector<float> buffer;
    size_t index = 0;
    float feedback = 0.5f;
    float damping = 0.5f;
    float filterStore = 0.0f;
};

// One allpass filter: passes all frequencies equally (doesn't change
// tone) but smears the signal in time, adding diffusion.
class AllpassFilter
{
public:
    void setDelaySamples(int samples)
    {
        buffer.assign(static_cast<size_t>(std::max(1, samples)), 0.0f);
        index = 0;
    }

    float process(float input)
    {
        // Same safety check as CombFilter - avoid indexing an empty buffer.
        if (buffer.empty())
            return input;

        const float bufOut = buffer[index];
        const float output = -input + bufOut;
        buffer[index] = input + bufOut * feedback;
        index = (index + 1) % buffer.size();
        return output;
    }

private:
    std::vector<float> buffer;
    size_t index = 0;
    float feedback = 0.5f;
};

class ReverbBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;

        // Comb filter delay times, tuned as classic prime-ish millisecond
        // values (Schroeder/Freeverb-style) so their echoes don't line up
        // periodically and create an unnatural "flutter".
        static const float combMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
        for (int i = 0; i < 4; ++i)
            combs[i].setDelaySamples(msToSamples(combMs[i]));

        static const float allpassMs[2] = { 5.0f, 1.7f };
        for (int i = 0; i < 2; ++i)
            allpasses[i].setDelaySamples(msToSamples(allpassMs[i]));

        updateParams();
    }

    const char* getName() const override { return "Reverb"; }

    void setRoomSize(float size) { roomSize = std::clamp(size, 0.0f, 1.0f); updateParams(); }
    void setDamping(float d)     { dampingAmount = std::clamp(d, 0.0f, 1.0f); updateParams(); }
    void setMix(float m)         { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        float combSum = 0.0f;
        for (auto& comb : combs)
            combSum += comb.process(input);
        combSum *= 0.25f; // average the 4 parallel combs

        float diffused = combSum;
        for (auto& ap : allpasses)
            diffused = ap.process(diffused);

        return input * (1.0f - mix) + diffused * mix;
    }

private:
    int msToSamples(float ms) const
    {
        return static_cast<int>((ms / 1000.0f) * static_cast<float>(sampleRate));
    }

    void updateParams()
    {
        // Room size maps to comb feedback: a bigger room means a longer decay.
        // Kept below 1.0 to stay stable (no runaway buildup).
        const float fb = 0.7f + roomSize * 0.28f;
        for (auto& comb : combs)
        {
            comb.setFeedback(fb);
            comb.setDamping(dampingAmount);
        }
    }

    double sampleRate = 44100.0;
    std::array<CombFilter, 4> combs;
    std::array<AllpassFilter, 2> allpasses;

    float roomSize = 0.5f;
    float dampingAmount = 0.5f;
    float mix = 0.3f;
};

} // namespace ampforge
