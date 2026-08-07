#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <cmath>
#include <algorithm>

/*
 * NoiseGateBlock - attenuates the signal when it drops below a threshold.
 *
 * Useful with high-gain distortion (Screamer + high Amp Drive), where
 * the amp keeps amplifying hiss/hum even when you're not playing. The
 * gate closes (fades toward -rangeDB) when the signal is quiet, and
 * opens (fades back to unity) as soon as you play again.
 *
 * Two things keep this from choking actual playing:
 *  - Hysteresis: the gate opens above thresholdDB but only closes once
 *    the level drops hysteresisDB below that, so a signal hovering
 *    right at the threshold doesn't chatter open/closed.
 *  - Hold: once open, the gate stays open for holdMs before it's even
 *    allowed to start closing, so a decaying note or a vibrato dip
 *    doesn't get choked off mid-sustain.
 * On top of that, Range lets the user choose how far the gate pulls
 * the signal down instead of always slamming it to silence - dialing
 * that in (rather than an all-or-nothing mute) is what actually lets
 * someone balance "kills the hiss" against "doesn't cut the sound".
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

    // How far the gate pulls the signal down when closed, in dB
    // (0 = no reduction at all, 80 = effectively muted).
    void setRangeDB(float db)
    {
        rangeDB = db;
        floorGain = std::pow(10.0f, -rangeDB / 20.0f);
    }

    float processSample(float input) override
    {
        const float inputLevel = std::fabs(input);

        // Both smoothed values are flushed so they can't get stuck decaying
        // through denormals once the gate closes on silence (see Denormal.hpp).
        if (inputLevel > envelope)
            envelope = flushDenormal(attackCoeff * envelope + (1.0f - attackCoeff) * inputLevel);
        else
            envelope = flushDenormal(releaseCoeff * envelope + (1.0f - releaseCoeff) * inputLevel);

        const float envelopeDB = 20.0f * std::log10(std::max(envelope, 1e-6f));

        if (isOpen)
        {
            if (envelopeDB < thresholdDB - hysteresisDB)
            {
                if (holdCounter > 0)
                    --holdCounter;
                else
                    isOpen = false;
            }
            else
            {
                holdCounter = holdSamples;
            }
        }
        else if (envelopeDB > thresholdDB)
        {
            isOpen = true;
            holdCounter = holdSamples;
        }

        const float targetGain = isOpen ? 1.0f : floorGain;

        // Smooth toward the target using the same attack/release feel the
        // user dialed in, so Release actually controls how gently the gate
        // fades the signal down instead of snapping shut on a fixed rate.
        const float gainCoeff = (targetGain > gateGain) ? attackCoeff : releaseCoeff;
        gateGain = flushDenormal(gainCoeff * gateGain + (1.0f - gainCoeff) * targetGain);

        return input * gateGain;
    }

private:
    void updateCoefficients()
    {
        attackCoeff  = std::exp(-1.0f / (0.001f * attackMs * static_cast<float>(sampleRate)));
        releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs * static_cast<float>(sampleRate)));
        holdSamples  = static_cast<int>(0.001 * holdMs * sampleRate);
    }

    double sampleRate = 44100.0;
    float thresholdDB = -50.0f;
    float attackMs = 5.0f;
    float releaseMs = 150.0f;
    float rangeDB = 40.0f;

    static constexpr float hysteresisDB = 6.0f;
    static constexpr float holdMs = 50.0f;

    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelope = 0.0f;

    float floorGain = 0.01f; // -40 dB, matches the rangeDB default above
    // Starts at floorGain (not 1.0) to match isOpen's initial false state -
    // otherwise the gate would ease down from a fully-open gain over a
    // full releaseMs on every fresh instance/session, letting hiss/hum
    // through near full volume for up to a second at high Release settings
    // before the envelope ever gets a chance to open/close it for real.
    float gateGain = floorGain;
    bool isOpen = false;
    int holdSamples = 0;
    int holdCounter = 0;
};

} // namespace ampforge
