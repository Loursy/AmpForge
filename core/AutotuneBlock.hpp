#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

/*
 * AutotuneBlock - classic "hard-tune" pitch correction (the robotic,
 * T-Pain-style snap-to-note effect, not a transparent/natural correction).
 *
 * Three things happen per sample:
 *   1. Pitch tracking - continuously estimate the input's fundamental
 *      frequency (see PitchTracker below).
 *   2. Target selection - snap that frequency to the nearest note allowed
 *      by the chosen Key/Scale, and glide toward the correction ratio
 *      this implies at a speed set by Retune Speed.
 *   3. Pitch shifting - actually move the audio's pitch by that ratio,
 *      via a granular delay-line technique (see the bottom of
 *      processSample()).
 *
 * Why not reuse core/PitchDetector.hpp (the Tuner's pitch estimator)?
 * That class is deliberately tuned for a stable *display*: a 90ms
 * analysis window and a "lock-and-hold" that only updates its reported
 * note after 3 consecutive agreeing hops, so the tuner needle doesn't
 * flicker. Autotune needs the opposite - fast *reaction*, so the
 * correction can actually follow a moving voice - so this class has its
 * own small, private tracker (PitchTracker below) instead: the same
 * underlying technique (windowed autocorrelation with octave-doubling
 * correction and parabolic interpolation), just with a much shorter
 * window/hop and no lock-streak gating. The frequency search range
 * (38-1200Hz) is kept identical to PitchDetector's - it already
 * comfortably covers the vocal fundamental range (~70-1100Hz).
 */

namespace ampforge {

// A scale is just "which of the 12 semitones relative to the chosen Key
// are valid correction targets" - Chromatic allows all of them (the safe
// default before a user picks a real key), Major/Minor are the familiar
// diatonic sets. Lives here rather than in ChainParameters.hpp for the
// same reason kAmpVoicings lives in AmpBlock.hpp instead of there: it's a
// property of this DSP block, not something the shared parameter/preset
// plumbing needs to know the internals of.
struct ScaleDef
{
    const char* name;
    bool allowed[12]; // allowed[i] = true if semitone i (relative to Key) is in this scale
};

static constexpr int kScaleCount = 3;

// Expands a scale's interval list into the 12-entry membership table
// AutotuneBlock's nearest-note search actually uses, so kScales below can
// stay written as the intervals a musician would recognize.
static constexpr std::array<bool, 12> expandScale(std::initializer_list<int> intervals)
{
    std::array<bool, 12> table{};
    for (int i : intervals)
        table[static_cast<size_t>(i)] = true;
    return table;
}

struct ScaleTable
{
    const char* name;
    std::array<bool, 12> allowed;
};

static const ScaleTable kScales[kScaleCount] =
{
    { "Chromatic", expandScale({0,1,2,3,4,5,6,7,8,9,10,11}) },
    { "Major",     expandScale({0,2,4,5,7,9,11}) },
    { "Minor",     expandScale({0,2,3,5,7,8,10}) },
};

class AutotuneBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        tracker.setSampleRate(sr);

        // The granular pitch shifter's delay line: only needs to
        // comfortably outlive one grain (~20ms) on both sides of the
        // write head, plus headroom for a read pointer briefly lagging
        // further behind at the most extreme correction ratios this
        // block allows (well under 2x in practice for vocal-range
        // corrections) - 250ms is generous, and doesn't itself add any
        // latency (only the actual read/write gap kept during normal
        // operation does - see processSample()'s retrigger comment).
        // Same double-buffered swap-on-resize pattern as
        // DelayBlock/ChorusBlock: setSampleRate() isn't guaranteed to run
        // on the audio thread, so the new buffer is built fully in the
        // *inactive* slot and published with one atomic store, never
        // observed mid-resize by processSample() running concurrently.
        const size_t maxSamples = static_cast<size_t>(sr * 0.25) + 1;
        const int active = activeSlot.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        buffers[next].assign(maxSamples, 0.0f);
        writeIndices[next] = 0;
        activeSlot.store(static_cast<int>(next), std::memory_order_release);

        grainSamples = std::max(4.0f, static_cast<float>(sr) * kGrainSeconds);
        // Start both heads reading from half a grain behind the write
        // head - see processSample()'s retrigger comment for why it has
        // to be behind, not at, the write head.
        readPosA = readPosB = wrapIndex(0.0f - grainSamples * 0.5f, maxSamples);
        grainPhase = 0.0f;
        headActive = true;
        smoothedRatio = 1.0f;
    }

    const char* getName() const override { return "Autotune"; }

    void setKey(int semitone)      { key = ((semitone % 12) + 12) % 12; }
    void setScale(int scaleIndex)  { scale = std::clamp(scaleIndex, 0, kScaleCount - 1); }
    // 0..1 - see the class comment on the ~180ms..~4ms glide range this maps to.
    void setRetuneSpeed(float speed01) { retuneSpeed = std::clamp(speed01, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        const int active = activeSlot.load(std::memory_order_acquire);
        if (active < 0)
            return input; // setSampleRate hasn't been called yet - pass through safely

        // --- 1. Track pitch, decide the correction ratio ---
        tracker.pushSample(input);

        float targetRatio = 1.0f;
        if (tracker.isConfident())
        {
            const float detectedHz = tracker.getFrequencyHz();
            const float targetHz = nearestScaleHz(detectedHz);
            targetRatio = targetHz / detectedHz;
        }
        // else: no confident pitch right now (silence, breath, an
        // unvoiced consonant) - targetRatio stays 1.0, gliding the
        // correction back off instead of warping noise.

        // One-pole glide toward targetRatio. tau ranges from ~180ms at
        // Retune Speed=0 (still a full hard snap - just an audible slide
        // into it, a "ballad" feel) down to ~4ms at Speed=1 (the
        // near-instant T-Pain-style snap) - this one time constant is
        // what actually gives the Speed knob its classic-autotune
        // character.
        const float tauMs = 180.0f - retuneSpeed * (180.0f - 4.0f);
        const float coeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * (tauMs / 1000.0f)));
        smoothedRatio += (targetRatio - smoothedRatio) * coeff;

        // --- 2. Shift pitch by smoothedRatio: dual-read-head granular shifter ---
        // Write the input into the circular delay line, then read it back
        // through two pointers that each advance at smoothedRatio samples
        // per sample (instead of 1.0, like a plain delay) - that's what
        // actually changes the pitch. A single such pointer would
        // eventually run too far ahead or behind the write head to keep
        // reading valid history, so a second pointer, offset by half a
        // grain and crossfaded in as the first approaches its limit,
        // keeps one of the two always mid-grain. This doesn't need exact
        // pitch-period tracking (unlike PSOLA) - simpler to get right,
        // and its artifacts at larger correction amounts suit the
        // "classic hard-tune" character this block is going for anyway.
        std::vector<float>& buffer = buffers[static_cast<size_t>(active)];
        size_t& writeIndex = writeIndices[static_cast<size_t>(active)];
        const size_t bufferSize = buffer.size();

        buffer[writeIndex] = input;

        grainPhase += 1.0f;
        if (grainPhase >= grainSamples * 0.5f)
        {
            // Half a grain has passed - retrigger whichever head is
            // currently faded out, restarting it half a grain *behind*
            // the write head (not at it - reading forward from exactly
            // the write position in a circular buffer means reading data
            // from a full lap ago, i.e. the whole buffer's ~250ms length
            // as latency, which is audible as "hearing yourself late").
            // Starting half a grain behind instead keeps the constant,
            // perceived monitoring latency to about kGrainSeconds/2 -
            // small enough not to be distracting - while still giving the
            // head room to drift with smoothedRatio for its whole
            // lifetime before the next retrigger.
            grainPhase = 0.0f;
            headActive = !headActive;
            (headActive ? readPosA : readPosB) = wrapIndex(static_cast<float>(writeIndex) - grainSamples * 0.5f, bufferSize);
        }

        readPosA = wrapIndex(readPosA + smoothedRatio, bufferSize);
        readPosB = wrapIndex(readPosB + smoothedRatio, bufferSize);

        const float sampleA = readInterpolated(buffer, readPosA);
        const float sampleB = readInterpolated(buffer, readPosB);

        // Equal-power-ish crossfade between the two heads across the
        // grain, so neither pops in/out at a retrigger boundary.
        const float t = grainPhase / (grainSamples * 0.5f);
        const float fadeActive = 0.5f - 0.5f * std::cos(t * static_cast<float>(M_PI));
        const float gainA = headActive ? fadeActive : (1.0f - fadeActive);
        const float gainB = 1.0f - gainA;

        const float output = flushDenormal(sampleA * gainA + sampleB * gainB);

        writeIndex = (writeIndex + 1) % bufferSize;

        return output;
    }

private:
    // A small, private pitch tracker - see the class comment for why this
    // isn't core/PitchDetector.hpp. Same underlying technique (windowed
    // autocorrelation, first-strong-peak search with octave-doubling
    // correction, parabolic interpolation for a sub-sample lag), tuned
    // for continuous low-latency tracking instead of a stable display:
    // a ~20ms window/hop (vs. PitchDetector's 90ms) and no lock-streak
    // hold - just a per-hop confidence gate.
    class PitchTracker
    {
    public:
        void setSampleRate(double sr)
        {
            sampleRate = sr;
            windowSize = static_cast<size_t>(sr * kWindowSeconds);
            hopSize = windowSize; // non-overlapping - a fresh estimate every hop

            minLag = std::max<size_t>(1, static_cast<size_t>(sr / kMaxFrequencyHz));
            maxLag = static_cast<size_t>(sr / kMinFrequencyHz);
            maxLag = std::min(maxLag, windowSize / 2);

            buffer.assign(windowSize, 0.0f);
            linear.assign(windowSize, 0.0f);
            corr.assign(maxLag + 2, 0.0f);
            writeIndex = 0;
            samplesSinceAnalysis = 0;
            frequencyHz = 0.0f;
            confident = false;
        }

        void pushSample(float sample)
        {
            if (buffer.empty())
                return;

            buffer[writeIndex] = sample;
            writeIndex = (writeIndex + 1) % windowSize;

            if (++samplesSinceAnalysis >= hopSize)
            {
                samplesSinceAnalysis = 0;
                analyze();
            }
        }

        float getFrequencyHz() const { return frequencyHz; }
        bool isConfident() const { return confident; }

    private:
        // Unlike PitchDetector, this runs the whole analysis pass in one
        // call rather than spreading it across pushSample() calls - the
        // ~20ms window here is small enough (a few hundred samples at
        // most sample rates) that one pass comfortably finishes well
        // inside a hop's real-time budget, so the extra complexity of
        // incremental pacing (needed for PitchDetector's much larger 90ms
        // window) isn't worth it here.
        void analyze()
        {
            for (size_t i = 0; i < windowSize; ++i)
                linear[i] = buffer[(writeIndex + i) % windowSize];

            float mean = 0.0f;
            for (float s : linear) mean += s;
            mean /= static_cast<float>(windowSize);
            for (float& s : linear) s -= mean;

            float energy = 0.0f;
            for (float s : linear) energy += s * s;
            if (energy < kSilenceRmsSquared * static_cast<float>(windowSize))
            {
                confident = false;
                return;
            }

            const float r0 = autocorrelate(0);
            for (size_t lag = minLag - 1; lag <= maxLag + 1; ++lag)
                corr[lag] = autocorrelate(lag);

            size_t bestLag = minLag;
            float bestCorr = -1.0f;
            size_t chosenLag = 0;
            for (size_t lag = minLag; lag <= maxLag; ++lag)
            {
                if (corr[lag] > bestCorr)
                {
                    bestCorr = corr[lag];
                    bestLag = lag;
                }
                const bool isLocalPeak = corr[lag] > corr[lag - 1] && corr[lag] >= corr[lag + 1];
                if (chosenLag == 0 && isLocalPeak && corr[lag] > kPeakThreshold * r0)
                    chosenLag = lag;
            }
            if (chosenLag == 0)
                chosenLag = bestLag;

            while (chosenLag * 2 <= maxLag && corr[chosenLag * 2] >= corr[chosenLag] && corr[chosenLag * 2] > kPeakThreshold * r0)
                chosenLag *= 2;

            const float confidenceRatio = (r0 > 0.0f) ? (corr[chosenLag] / r0) : 0.0f;
            confident = confidenceRatio > kConfidenceThreshold;
            if (!confident)
                return;

            const float yLeft = corr[chosenLag - 1];
            const float yCenter = corr[chosenLag];
            const float yRight = corr[chosenLag + 1];
            const float denom = yLeft - 2.0f * yCenter + yRight;
            const float shift = (std::fabs(denom) > 1e-9f) ? 0.5f * (yLeft - yRight) / denom : 0.0f;
            const float interpolatedLag = static_cast<float>(chosenLag) + std::clamp(shift, -1.0f, 1.0f);

            if (interpolatedLag <= 0.0f)
            {
                confident = false;
                return;
            }

            frequencyHz = static_cast<float>(sampleRate) / interpolatedLag;
        }

        float autocorrelate(size_t lag) const
        {
            float sum = 0.0f;
            for (size_t i = 0; i + lag < windowSize; ++i)
                sum += linear[i] * linear[i + lag];
            return sum;
        }

        static constexpr float kMinFrequencyHz = 38.0f;
        static constexpr float kMaxFrequencyHz = 1200.0f;
        static constexpr float kWindowSeconds = 0.02f; // ~20ms - fast reaction, unlike the tuner's 90ms
        static constexpr float kPeakThreshold = 0.35f;
        static constexpr float kConfidenceThreshold = 0.45f;
        static constexpr float kSilenceRmsSquared = 1e-5f;

        double sampleRate = 44100.0;
        size_t windowSize = 0, hopSize = 0;
        size_t minLag = 0, maxLag = 0;
        std::vector<float> buffer, linear, corr;
        size_t writeIndex = 0, samplesSinceAnalysis = 0;
        float frequencyHz = 0.0f;
        bool confident = false;
    };

    // Finds the nearest frequency to detectedHz whose pitch class (its
    // semitone distance from Key, mod 12) is allowed by the current
    // Scale, and returns it in Hz.
    float nearestScaleHz(float detectedHz) const
    {
        const float midi = 69.0f + 12.0f * std::log2(detectedHz / 440.0f);
        const int rounded = static_cast<int>(std::lround(midi));

        // Search outward from the nearest integer note (0, -1, +1, -2,
        // +2, ...) - bounded to a full octave's worth of steps, since
        // every scale here has at least one allowed note per octave.
        const bool* allowed = kScales[scale].allowed.data();
        for (int offset = 0; offset <= 12; ++offset)
        {
            for (int sign : { -1, 1 })
            {
                if (offset == 0 && sign == 1)
                    continue; // don't test 0 twice
                const int candidate = rounded + sign * offset;
                const int pitchClass = (((candidate - key) % 12) + 12) % 12;
                if (allowed[pitchClass])
                    return 440.0f * std::exp2((static_cast<float>(candidate) - 69.0f) / 12.0f);
            }
        }

        // Unreachable in practice - every ScaleTable in kScales has at
        // least one allowed pitch class, so the loop above always finds
        // one well before offset reaches 12. Falls back to the plain
        // nearest integer note if that invariant is ever broken.
        return 440.0f * std::exp2((static_cast<float>(rounded) - 69.0f) / 12.0f);
    }

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

    // ~20ms grains - kept short deliberately, not just for pitch-shift
    // quality: since a vocalist hears their own corrected voice back
    // through this in real time, the ~grainSamples/2 latency this design
    // adds (see processSample()'s retrigger comment) has to stay small
    // enough not to feel like "hearing yourself late" - roughly 10ms
    // here, well under the point self-monitoring delay gets distracting.
    // Shorter grains do mean slightly more granular artifacting, an
    // acceptable trade for this block's "hard-tune" character anyway.
    static constexpr float kGrainSeconds = 0.02f;

    double sampleRate = 44100.0;
    PitchTracker tracker;

    int key = 0;             // 0 = C .. 11 = B
    int scale = 0;            // index into kScales
    float retuneSpeed = 0.5f; // 0..1

    float smoothedRatio = 1.0f;

    std::array<std::vector<float>, 2> buffers;
    std::array<size_t, 2> writeIndices{{0, 0}};
    std::atomic<int> activeSlot{-1};

    float grainSamples = 1764.0f; // recomputed in setSampleRate()
    float grainPhase = 0.0f;
    float readPosA = 0.0f, readPosB = 0.0f;
    bool headActive = true;
};

} // namespace ampforge
