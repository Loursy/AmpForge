#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <cmath>
#include <algorithm>

/*
 * CompressorBlock - a classic dynamics compressor.
 *
 * How it works:
 *   1) We track the signal's level over time with an "envelope
 *      follower" (attack/release smoothing) instead of reacting to
 *      every single sample - this avoids harsh, clicky gain changes.
 *   2) If the tracked level goes above `threshold`, we reduce gain
 *      by `ratio` (e.g. ratio 4 means every 4dB over the threshold
 *      becomes only 1dB over it).
 *   3) `makeupGain` boosts the output back up afterward, since
 *      compression quietens the signal overall.
 */

namespace ampforge {

class CompressorBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        updateCoefficients();
    }

    const char* getName() const override { return "Compressor"; }

    void setThresholdDB(float db)  { thresholdDB = db; }
    void setRatio(float r)         { ratio = std::max(1.0f, r); }
    void setAttackMs(float ms)     { attackMs = ms; updateCoefficients(); }
    void setReleaseMs(float ms)    { releaseMs = ms; updateCoefficients(); }
    void setMakeupGainDB(float db) { makeupGainLinear = std::pow(10.0f, db / 20.0f); }

    float processSample(float input) override
    {
        const float inputLevel = std::fabs(input);

        // Envelope follower: rises quickly (attack), falls slowly (release).
        // Flushed so it can't get stuck decaying through denormals during a
        // quiet passage (see Denormal.hpp).
        if (inputLevel > envelope)
            envelope = flushDenormal(attackCoeff * envelope + (1.0f - attackCoeff) * inputLevel);
        else
            envelope = flushDenormal(releaseCoeff * envelope + (1.0f - releaseCoeff) * inputLevel);

        const float envelopeDB = 20.0f * std::log10(std::max(envelope, 1e-6f));

        float gainReductionDB = 0.0f;
        if (envelopeDB > thresholdDB)
            gainReductionDB = (envelopeDB - thresholdDB) * (1.0f - 1.0f / ratio);

        const float gainLinear = std::pow(10.0f, -gainReductionDB / 20.0f);

        return input * gainLinear * makeupGainLinear;
    }

private:
    void updateCoefficients()
    {
        attackCoeff  = std::exp(-1.0f / (0.001f * attackMs * static_cast<float>(sampleRate)));
        releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs * static_cast<float>(sampleRate)));
    }

    double sampleRate = 44100.0;
    float thresholdDB = -18.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float makeupGainLinear = 1.0f;

    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelope = 0.0f;
};

} // namespace ampforge
