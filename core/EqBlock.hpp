#pragma once
#include "AudioBlock.hpp"
#include "Biquad.hpp"

/*
 * EqBlock - a dedicated 3-band parametric EQ pedal.
 *
 * This is deliberately separate from AmpBlock's own Bass/Mid/Treble tone
 * stack (core/AmpBlock.hpp), even though both are three Biquads in a row -
 * the difference is what's a knob. AmpBlock's tone stack has a *fixed*
 * center frequency per band (set by the chosen Amp Type voicing, see
 * kAmpVoicings) and only exposes Gain; this pedal exposes Frequency too,
 * for every band, so it can be swept anywhere in its range - a real
 * parametric EQ, not a fixed tone stack. It's also a separate, reorderable
 * pedal (like Wah or Distortion), so it can sit anywhere in the chain -
 * ahead of the amp to shape what hits the input, or at the very end as a
 * final correction/mastering stage.
 *
 * Signal flow: Low (shelf) -> Mid (peak) -> High (shelf), same order and
 * same filter types AmpBlock's tone stack uses. Each band's Q is fixed
 * (not a knob) - same reasoning as DeEsserBlock's fixed Q: this keeps the
 * pedal to 6 knobs (Freq+Gain x3) instead of 9, and a fixed, sane Q per
 * band shape (wide for the shelves, moderate for the peak) covers the
 * vast majority of what a guitarist/vocalist actually reaches for an EQ
 * pedal to do.
 */

namespace ampforge {

class EqBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        low.setSampleRate(sr);
        mid.setSampleRate(sr);
        high.setSampleRate(sr);
        updateLow();
        updateMid();
        updateHigh();
    }

    const char* getName() const override { return "EQ"; }

    void setLowFreq(float hz)    { lowFreq = hz;   updateLow(); }
    void setLowGainDB(float db)  { lowGainDB = db;  updateLow(); }
    void setMidFreq(float hz)    { midFreq = hz;   updateMid(); }
    void setMidGainDB(float db)  { midGainDB = db;  updateMid(); }
    void setHighFreq(float hz)   { highFreq = hz;  updateHigh(); }
    void setHighGainDB(float db) { highGainDB = db; updateHigh(); }

    float processSample(float input) override
    {
        float x = low.process(input);
        x = mid.process(x);
        x = high.process(x);
        return x;
    }

private:
    void updateLow()  { low.setParams(Biquad::Type::LowShelf,  lowFreq,  lowGainDB,  kShelfQ); }
    void updateMid()  { mid.setParams(Biquad::Type::Peak,      midFreq,  midGainDB,  kPeakQ); }
    void updateHigh() { high.setParams(Biquad::Type::HighShelf, highFreq, highGainDB, kShelfQ); }

    static constexpr float kShelfQ = 0.707f;
    static constexpr float kPeakQ  = 1.0f;

    double sampleRate = 44100.0;
    Biquad low, mid, high;

    float lowFreq = 100.0f,  lowGainDB = 0.0f;
    float midFreq = 800.0f,  midGainDB = 0.0f;
    float highFreq = 3000.0f, highGainDB = 0.0f;
};

} // namespace ampforge
