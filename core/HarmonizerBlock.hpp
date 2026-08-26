#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

/*
 * HarmonizerBlock - adds a second voice a fixed musical interval above
 * or below the input (e.g. a major third or a perfect fifth), mixed back
 * in with the dry signal - the classic "instant harmony" vocal effect.
 *
 * Unlike AutotuneBlock, this does *not* track the input's pitch first -
 * it shifts by a fixed ratio the player picks (the Interval knob), the
 * same way a simple/"dumb" hardware harmonizer pedal works: no pitch
 * detection to get wrong, and a well-chosen fixed interval (a fifth, a
 * third) still sounds musical against most vocal lines in a given song,
 * without needing this block to know what key the song is in. What it
 * *does* reuse from AutotuneBlock is the actual pitch-shifting mechanism
 * itself - the same dual-read-head granular delay-line technique (see
 * that file's processSample() comment for the full explanation of why
 * two crossfading heads are needed) - just fed a constant ratio instead
 * of a dynamically-tracked correction ratio.
 */

namespace ampforge {

class HarmonizerBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;

        // Same sizing/double-buffer reasoning as AutotuneBlock's own
        // delay line - 250ms comfortably outlives a grain on both sides
        // of the write head, with headroom for the most extreme interval
        // this block allows (+-1 octave, i.e. up to 2x/0.5x read speed).
        const size_t maxSamples = static_cast<size_t>(sr * 0.25) + 1;
        const int active = activeSlot.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        buffers[next].assign(maxSamples, 0.0f);
        writeIndices[next] = 0;
        activeSlot.store(static_cast<int>(next), std::memory_order_release);

        grainSamples = std::max(4.0f, static_cast<float>(sr) * kGrainSeconds);
        // Start a full grain behind the write head, not half a grain -
        // see processSample()'s retrigger comment for why this block
        // needs more head start than AutotuneBlock's own half-grain does.
        readPosA = readPosB = wrapIndex(0.0f - grainSamples, maxSamples);
        grainPhase = 0.0f;
        headActive = true;
    }

    const char* getName() const override { return "Harmonizer"; }

    // Semitones, e.g. +4 = major third up, +7 = perfect fifth up,
    // -12 = an octave down.
    void setIntervalSemitones(float semitones)
    {
        ratio = std::exp2(semitones / 12.0f);
    }
    void setMix(float m) { mix = std::clamp(m, 0.0f, 1.0f); }

    // See AudioBlock::reset()'s comment - mutates the active buffer in
    // place, safe for the same reason DelayBlock::reset() is. Also
    // restarts the grain heads exactly as setSampleRate() does, so a
    // re-enable doesn't resume mid-grain against now-zeroed history.
    void reset() override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return;
        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndices[static_cast<size_t>(active)] = 0;
        readPosA = readPosB = wrapIndex(0.0f - grainSamples, buffer.size());
        grainPhase = 0.0f;
        headActive = true;
    }

    float processSample(float input) override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return input;

        // Dual-read-head granular pitch shifter - see AutotuneBlock's own
        // processSample() comment for the full explanation; identical
        // mechanism, just driven by a fixed `ratio` instead of a
        // dynamically-tracked one.
        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        size_t& writeIndex = writeIndices[static_cast<size_t>(active)];
        const size_t bufferSize = buffer.size();

        buffer[writeIndex] = input;

        grainPhase += 1.0f;
        if (grainPhase >= grainSamples * 0.5f)
        {
            // Retrigger a full grain behind the write head, not half a
            // grain like AutotuneBlock's identical-looking retrigger does.
            // AutotuneBlock's ratio is a *correction* that stays close to
            // 1.0 in practice, so a half-grain head start never runs out;
            // this block's ratio is a deliberate, fixed, up-to-2x/0.5x
            // shift (a full octave either way - see setIntervalSemitones()'s
            // comment), and at ratio=2 a half-grain head start gets
            // fully consumed by the read head catching up to the write
            // head before the next retrigger - at that point the two
            // interpolation taps straddle the write index, blending a
            // just-written sample against one a whole buffer-length
            // (~250ms) stale, which is an audible click, not a subtle
            // artifact. A full-grain head start keeps the read head
            // strictly behind the write head across this block's entire
            // allowed ratio range, at the cost of roughly doubling the
            // ~grainSamples/2 self-monitoring latency AutotuneBlock has.
            grainPhase = 0.0f;
            headActive = !headActive;
            (headActive ? readPosA : readPosB) = wrapIndex(static_cast<float>(writeIndex) - grainSamples, bufferSize);
        }

        readPosA = wrapIndex(readPosA + ratio, bufferSize);
        readPosB = wrapIndex(readPosB + ratio, bufferSize);

        const float sampleA = readInterpolated(buffer, readPosA);
        const float sampleB = readInterpolated(buffer, readPosB);

        const float t = grainPhase / (grainSamples * 0.5f);
        const float fadeActive = 0.5f - 0.5f * std::cos(t * static_cast<float>(M_PI));
        const float gainA = headActive ? fadeActive : (1.0f - fadeActive);
        const float gainB = 1.0f - gainA;

        const float harmony = flushDenormal(sampleA * gainA + sampleB * gainB);

        writeIndex = (writeIndex + 1) % bufferSize;

        return input * (1.0f - mix) + harmony * mix;
    }

private:
    static float wrapIndex(float pos, size_t bufferSize)
    {
        const float size = static_cast<float>(bufferSize);
        while (pos < 0.0f) pos += size;
        while (pos >= size) pos -= size;
        return pos;
    }

    static float readInterpolated(const std::vector<float>& buffer, float pos)
    {
        const size_t bufferSize = buffer.size();
        const size_t i0 = static_cast<size_t>(pos) % bufferSize;
        const size_t i1 = (i0 + 1) % bufferSize;
        const float frac = pos - std::floor(pos);
        return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
    }

    // Same 20ms grain length as AutotuneBlock, though the self-monitoring
    // latency this actually gives is ~grainSamples (not AutotuneBlock's
    // ~grainSamples/2) - see the retrigger comment in processSample() for
    // why this block needs the read heads to start a full grain behind
    // the write head instead of half a grain.
    static constexpr float kGrainSeconds = 0.02f;

    double sampleRate = 44100.0;
    float ratio = 1.0f; // recomputed by setIntervalSemitones()
    float mix = 0.4f;

    std::array<std::vector<float>, 2> buffers;
    std::array<size_t, 2> writeIndices{{0, 0}};
    std::atomic<int> activeSlot{-1};

    float grainSamples = 1764.0f;
    float grainPhase = 0.0f;
    float readPosA = 0.0f, readPosB = 0.0f;
    bool headActive = true;
};

} // namespace ampforge
