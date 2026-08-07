#pragma once
#include <cmath>

/*
 * Denormal.hpp - a tiny helper used anywhere a value recirculates (delay
 * lines with feedback, comb/allpass filters, envelope followers, filter
 * state) and can therefore decay toward silence.
 *
 * When a recirculating value gets extremely close to zero it can become a
 * "denormalized" float - still numerically correct, but on x86 the FPU
 * handles denormals through a much slower microcode path. Without
 * protection, a quiet passage (or the player simply stopping) can make a
 * plugin's CPU usage spike even though nothing audible is happening -
 * a classic, well-known class of bug in feedback-based audio DSP. Since
 * anything this small is inaudible anyway, we just snap it to exact zero.
 */

namespace ampforge {

inline float flushDenormal(float x)
{
    return (x > -1.0e-15f && x < 1.0e-15f) ? 0.0f : x;
}

} // namespace ampforge
