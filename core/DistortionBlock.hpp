#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <cmath>
#include <algorithm>

/*
 * DistortionBlock - a harder, more aggressive gain stage than Screamer.
 *
 * Screamer models an overdrive: a matched diode pair clips both halves
 * of the waveform at (roughly) the same threshold, which keeps the
 * result fairly smooth and "warm". A lot of dedicated Distortion pedals
 * (think Boss DS-1 / ProCo Rat style circuits) clip asymmetrically -
 * the positive and negative halves get clamped at different thresholds -
 * which adds strong even-order harmonics on top of the odd ones and is
 * what gives that family of pedals their harsher, buzzier, more
 * aggressive character instead of overdrive's smoother crunch.
 *
 * Signal flow:
 *   1) Drive: boost the signal (higher range than Screamer's - this is
 *      meant to get considerably dirtier).
 *   2) Asymmetric hard clipping.
 *   3) Tone: one-pole low-pass, same idea as Screamer's.
 *   4) Level: final output volume.
 */

namespace ampforge {

class DistortionBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
    }

    const char* getName() const override { return "Distortion"; }

    void setDrive(float drive)   { driveAmount = drive; }   // roughly 1.0 - 30.0
    void setTone(float tone)     { toneAmount = tone; }       // 0.0 - 1.0
    void setLevel(float levelDB) { levelLinear = std::pow(10.0f, levelDB / 20.0f); }

    float processSample(float input) override
    {
        // 1) Drive: boost the signal
        float x = input * driveAmount;

        // 2) Asymmetric hard clipping - different thresholds each side.
        x = (x > 0.0f) ? std::min(x, 0.55f) : std::max(x, -0.85f);

        // 3) Tone: simple one-pole low-pass filter (see Screamer for the
        //    same idea, explained in more detail).
        x = lastSample + toneAmount * (x - lastSample);
        lastSample = flushDenormal(x);

        // 4) Output level
        return x * levelLinear;
    }

private:
    double sampleRate = 44100.0;
    float driveAmount = 4.0f;
    float toneAmount = 0.5f;
    float levelLinear = 1.0f;
    float lastSample = 0.0f; // filter's "memory"
};

} // namespace ampforge
