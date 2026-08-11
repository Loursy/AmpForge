#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"
#include "Denormal.hpp"
#include <cmath>
#include <algorithm>

/*
 * DeEsserBlock - tames harsh "s"/"sh" sibilance in a vocal, the second
 * genuinely vocal-only block after Autotune (see AutotuneBlock.hpp -
 * neither makes sense on a guitar signal).
 *
 * How it works (the standard "split-band" de-esser design):
 *   1) A high-pass filter (core/Biquad.hpp's new HighPass type) at
 *      `frequency` pulls out just the sibilant band - everything from
 *      there up, where harsh "s" energy actually lives (typically
 *      4-9kHz depending on the voice).
 *   2) The rest of the signal (the "low band") is gotten by simple
 *      subtraction: low = input - high. This is exact and needs no
 *      second matched filter - low+high always reconstructs the
 *      original input exactly whenever no gain reduction is applied.
 *   3) An envelope follower - the same attack/release shape
 *      CompressorBlock.hpp uses - tracks the high band's level. Once it
 *      crosses `threshold`, the excess above threshold is reduced 1:1
 *      (unlike a musical compressor's ratio knob, a de-esser's job is to
 *      just clamp sibilance down hard whenever it pokes above the
 *      threshold), capped at `reductionDB` so it can never over-attenuate.
 *   4) Only the high band gets that gain reduction applied - the low
 *      band passes straight through untouched. Re-summing them is what
 *      makes this different from (and much less "duller-sounding" than)
 *      just running a plain compressor on the whole signal: the rest of
 *      the voice's tone doesn't duck every time an "s" hits.
 */

namespace ampforge {

class DeEsserBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        highpass.setSampleRate(sr);
        updateFilter();
        updateEnvelopeCoeffs();
    }

    const char* getName() const override { return "De-esser"; }

    void setFrequency(float hz)   { frequency = std::clamp(hz, 1000.0f, 16000.0f); updateFilter(); }
    void setThresholdDB(float db) { thresholdDB = db; }
    void setReductionDB(float db) { reductionDB = std::max(0.0f, db); }

    float processSample(float input) override
    {
        const float highBand = highpass.process(input);
        const float lowBand = input - highBand;

        const float level = std::fabs(highBand);
        if (level > envelope)
            envelope = flushDenormal(attackCoeff * envelope + (1.0f - attackCoeff) * level);
        else
            envelope = flushDenormal(releaseCoeff * envelope + (1.0f - releaseCoeff) * level);

        const float envelopeDB = 20.0f * std::log10(std::max(envelope, 1e-6f));

        float gainReductionDB = 0.0f;
        if (envelopeDB > thresholdDB)
            gainReductionDB = std::min(reductionDB, envelopeDB - thresholdDB);

        const float gainLinear = std::pow(10.0f, -gainReductionDB / 20.0f);

        return lowBand + highBand * gainLinear;
    }

private:
    void updateFilter()
    {
        // Fixed Q=0.707 (a standard Butterworth-flat rolloff) - the
        // Frequency knob is the only thing that needs to move for this
        // to work across different voices, so there's no separate Q knob.
        highpass.setParams(Biquad::Type::HighPass, frequency, 0.0f, 0.707f);
    }

    void updateEnvelopeCoeffs()
    {
        // Fast attack (sibilance is a short, sharp burst - has to be
        // caught almost immediately) and a moderate release (long enough
        // not to visibly "pump" gain on every consonant, short enough to
        // fully recover before the next word) - fixed rather than
        // exposed as knobs, same reasoning as Compressor's own internal
        // constants elsewhere in this project: keeping this pedal to a
        // small, focused knob set.
        attackCoeff  = std::exp(-1.0f / (0.001f * kAttackMs * static_cast<float>(sampleRate)));
        releaseCoeff = std::exp(-1.0f / (0.001f * kReleaseMs * static_cast<float>(sampleRate)));
    }

    static constexpr float kAttackMs = 2.0f;
    static constexpr float kReleaseMs = 60.0f;

    double sampleRate = 44100.0;
    Biquad highpass;

    float frequency = 6000.0f;
    float thresholdDB = -30.0f;
    float reductionDB = 12.0f;

    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelope = 0.0f;
};

} // namespace ampforge
