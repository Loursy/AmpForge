#pragma once

/*
 * AudioBlock - base interface every module in AmpForge (Amp, Screamer,
 * Delay, Reverb, ...) will derive from.
 *
 * Why do we need this?
 * The design goal is for the user to be able to chain and reorder
 * pedals/amp however they want. The way to support that in code is:
 * make every module fit the same "shape" (interface), then represent
 * the chain as a std::vector<AudioBlock*>. The chain will work like this:
 *
 *   float sample = input;
 *   for (auto* block : chain)
 *       sample = block->processSample(sample);
 *
 * So reordering the chain becomes as simple as reordering the elements
 * of the vector.
 */

namespace ampforge {

class AudioBlock
{
public:
    virtual ~AudioBlock() {}

    // Called when the host reports the sample rate (e.g. 44100, 48000).
    // Sample-rate-dependent calculations (like filter coefficients) happen here.
    virtual void setSampleRate(double sampleRate) = 0;

    // Processes a single audio sample and returns the processed result.
    // NOTE: this function runs on the real-time audio thread -
    // no new/malloc, file I/O, or mutex locking is allowed inside it.
    virtual float processSample(float input) = 0;

    // The block's name - will be used in the GUI and preset files later.
    virtual const char* getName() const = 0;
};

} // namespace ampforge
