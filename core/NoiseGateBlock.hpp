#pragma once
#include "AudioBlock.hpp"
#include <cmath>
#include <algorithm>

/*
 * NoiseGateBlock - mutes the signal when it drops below a threshold.
 *
 * Useful with high-gain distortion (Screamer + high Amp Drive), where
 * the amp keeps amplifying hiss/hum even when you're not playing. The
 * gate "closes" (fades to silence) when the signal is quiet, and
 * "opens" (fades back in) as soon as you play again.
 */

namespace ampforge {

class NoiseGateBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        updateCoefficients();
    }

    const char* getName() const override { return "Noise Gate"; }

    void setThresholdDB(float db) { thresholdDB = db; }
    void setAttackMs(float ms)    { attackMs = ms; updateCoefficients(); }
    void setReleaseMs(float ms)   { releaseMs = ms; updateCoefficients(); }

    float processSample(float input) override
    {
        const float inputLevel = std::fabs(input);

        if (inputLevel > envelope)
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputLevel;
        else
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * inputLevel;

        const float envelopeDB = 20.0f * std::log10(std::max(envelope, 1e-6f));

        // Target gain: 1.0 above threshold (gate open), 0.0 below (gate closed)
        const float targetGain = (envelopeDB > thresholdDB) ? 1.0f : 0.0f;

        // Smooth the gain itself too, so the gate doesn't click open/closed
        gateGain = gateGain + (targetGain - gateGain) * gateSmoothingCoeff;

        return input * gateGain;
    }

private:
    void updateCoefficients()
    {
        attackCoeff  = std::exp(-1.0f / (0.001f * attackMs * static_cast<float>(sampleRate)));
        releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs * static_cast<float>(sampleRate)));
    }

    double sampleRate = 44100.0;
    float thresholdDB = -50.0f;
    float attackMs = 5.0f;
    float releaseMs = 150.0f;

    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelope = 0.0f;

    float gateGain = 1.0f;
    static constexpr float gateSmoothingCoeff = 0.01f; // fixed smoothing for the gain itself
};

} // namespace ampforge
