#pragma once
#include "Denormal.hpp"
#include <cmath>

/*
 * Biquad - the base filter block used for EQ modules (low-shelf, peak,
 * high-shelf), i.e. our Bass/Mid/Treble controls.
 *
 * The coefficient formulas come from the "RBJ Audio EQ Cookbook", the
 * standard reference everyone in audio DSP uses. You don't need to
 * memorize the math - what matters is the concept: each filter type
 * (bass/mid/treble) solves the same equation shape with different
 * coefficients.
 */

namespace ampforge {

class Biquad
{
public:
    enum class Type { LowShelf, Peak, HighShelf, HighPass };

    void setSampleRate(double sr) { sampleRate = sr; }

    // freq: center/cutoff frequency (Hz)
    // gainDB: how much this band is boosted/cut (dB)
    // q: how "narrow/wide" the band is (matters for Peak, kept fixed for shelves)
    void setParams(Type type, float freq, float gainDB, float q)
    {
        const double A     = std::pow(10.0, gainDB / 40.0);
        const double w0    = 2.0 * M_PI * freq / sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double cosw0 = std::cos(w0);

        double b0, b1, b2, a0, a1, a2;

        switch (type)
        {
        case Type::LowShelf:
        {
            const double sqrtA = std::sqrt(A);
            b0 =    A*((A+1) - (A-1)*cosw0 + 2*sqrtA*alpha);
            b1 =  2*A*((A-1) - (A+1)*cosw0);
            b2 =    A*((A+1) - (A-1)*cosw0 - 2*sqrtA*alpha);
            a0 =        (A+1) + (A-1)*cosw0 + 2*sqrtA*alpha;
            a1 =   -2*((A-1) + (A+1)*cosw0);
            a2 =        (A+1) + (A-1)*cosw0 - 2*sqrtA*alpha;
            break;
        }
        case Type::HighShelf:
        {
            const double sqrtA = std::sqrt(A);
            b0 =    A*((A+1) + (A-1)*cosw0 + 2*sqrtA*alpha);
            b1 = -2*A*((A-1) + (A+1)*cosw0);
            b2 =    A*((A+1) + (A-1)*cosw0 - 2*sqrtA*alpha);
            a0 =        (A+1) - (A-1)*cosw0 + 2*sqrtA*alpha;
            a1 =    2*((A-1) - (A+1)*cosw0);
            a2 =        (A+1) - (A-1)*cosw0 - 2*sqrtA*alpha;
            break;
        }
        case Type::HighPass:
        {
            // Standard RBJ 2nd-order high-pass - unlike the shelf/peak
            // types above, this one doesn't boost or cut, only rolls off
            // everything below freq, so it ignores gainDB/A entirely.
            b0 =  (1 + cosw0) / 2;
            b1 = -(1 + cosw0);
            b2 =  (1 + cosw0) / 2;
            a0 =   1 + alpha;
            a1 =  -2 * cosw0;
            a2 =   1 - alpha;
            break;
        }
        case Type::Peak:
        default:
        {
            b0 = 1 + alpha*A;
            b1 = -2*cosw0;
            b2 = 1 - alpha*A;
            a0 = 1 + alpha/A;
            a1 = -2*cosw0;
            a2 = 1 - alpha/A;
            break;
        }
        }

        // Normalize coefficients by a0 so processing only needs
        // multiply/add operations.
        c_b0 = (float)(b0/a0); c_b1 = (float)(b1/a0); c_b2 = (float)(b2/a0);
        c_a1 = (float)(a1/a0); c_a2 = (float)(a2/a0);
    }

    // Direct Form I - the classic biquad equation:
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    float process(float x)
    {
        const float y = c_b0*x + c_b1*x1 + c_b2*x2 - c_a1*y1 - c_a2*y2;
        x2 = x1; x1 = x;
        // Flushed so the filter's ring-down after the input goes silent
        // can't leave y1/y2 stuck as denormals (see Denormal.hpp).
        y2 = y1; y1 = flushDenormal(y);
        return y;
    }

private:
    double sampleRate = 44100.0;
    float c_b0 = 1.0f, c_b1 = 0.0f, c_b2 = 0.0f, c_a1 = 0.0f, c_a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

} // namespace ampforge
