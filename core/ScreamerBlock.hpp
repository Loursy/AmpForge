#pragma once
#include "AudioBlock.hpp"
#include <cmath>
#include <algorithm>

/*
 * ScreamerBlock - imitates a Tube Screamer style overdrive pedal.
 *
 * Signal flow:
 *   1) Drive: boost the signal.
 *   2) Diode clipping: a harder, more asymmetric-feeling clip than the
 *      Amp's smooth tanh() - this is what gives overdrive pedals their
 *      "gritty" character, different from a clean amp saturation.
 *   3) Tone: a simple one-pole low-pass filter that rolls off highs.
 *   4) Level: final output volume.
 */

namespace ampforge {

class ScreamerBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
    }

    const char* getName() const override { return "Screamer"; }

    void setDrive(float drive)   { driveAmount = drive; }   // roughly 1.0 - 20.0
    void setTone(float tone)     { toneAmount = tone; }       // 0.0 - 1.0
    void setLevel(float levelDB) { levelLinear = std::pow(10.0f, levelDB / 20.0f); }

    float processSample(float input) override
    {
        // 1) Drive: boost the signal
        float x = input * driveAmount;

        // 2) Diode clipping simulation. A simple hard clamp for now -
        //    later this can be replaced with a more realistic diode
        //    equation, but this already gives an audible "grit".
        x = std::clamp(x, -0.7f, 0.7f);

        // 3) Tone: simple one-pole low-pass filter.
        //    Each new sample is blended with the previous one - the
        //    lower toneAmount is, the darker (more highs removed) it sounds;
        //    closer to 1.0 means brighter/more open.
        x = lastSample + toneAmount * (x - lastSample);
        lastSample = x;

        // 4) Output level
        return x * levelLinear;
    }

private:
    double sampleRate = 44100.0;
    float driveAmount = 1.0f;
    float toneAmount = 0.5f;
    float levelLinear = 1.0f;
    float lastSample = 0.0f; // filter's "memory"
};

} // namespace ampforge
