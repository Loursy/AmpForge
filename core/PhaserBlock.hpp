#pragma once
#include "AudioBlock.hpp"
#include <array>
#include <cmath>
#include <algorithm>

/*
 * PhaserBlock - creates a sweeping, "whooshing" sound by passing the
 * signal through a chain of allpass filters whose center frequency is
 * modulated by a slow LFO, then mixing that with the dry signal. The
 * moving notches created by combining the phase-shifted and original
 * signal are what give a phaser its character (different from chorus,
 * which uses a delay line instead of allpass filters).
 */

namespace ampforge {

// A single first-order allpass stage with a controllable "break frequency".
class PhaserAllpassStage
{
public:
    void setSampleRate(double sr) { sampleRate = sr; }

    void setFrequency(float freq)
    {
        // Standard first-order allpass coefficient derived from the
        // desired break frequency.
        const float tan_ = std::tan(static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate));
        coeff = (tan_ - 1.0f) / (tan_ + 1.0f);
    }

    float process(float input)
    {
        const float output = coeff * input + previousInput - coeff * previousOutput;
        previousInput = input;
        previousOutput = output;
        return output;
    }

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float previousInput = 0.0f, previousOutput = 0.0f;
};

class PhaserBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        for (auto& stage : stages)
            stage.setSampleRate(sr);
        lfoPhase = 0.0f;
    }

    const char* getName() const override { return "Phaser"; }

    void setRateHz(float hz) { rateHz = hz; }
    void setDepth(float d)   { depth = std::clamp(d, 0.0f, 1.0f); }
    void setMix(float m)     { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        lfoPhase += rateHz / static_cast<float>(sampleRate);
        if (lfoPhase >= 1.0f)
            lfoPhase -= 1.0f;
        const float lfo = 0.5f + 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * lfoPhase); // 0..1

        // Sweep the allpass break frequency between ~200Hz and up to
        // ~2000Hz, scaled by depth - lower depth means a narrower, subtler sweep.
        const float minFreq = 200.0f;
        const float maxFreq = 200.0f + depth * 1800.0f;
        const float freq = minFreq + lfo * (maxFreq - minFreq);

        float x = input;
        for (auto& stage : stages)
        {
            stage.setFrequency(freq);
            x = stage.process(x);
        }

        return input * (1.0f - mix) + x * mix;
    }

private:
    double sampleRate = 44100.0;
    std::array<PhaserAllpassStage, 4> stages;

    float lfoPhase = 0.0f;
    float rateHz = 0.5f;
    float depth = 0.7f;
    float mix = 0.5f;
};

} // namespace ampforge
