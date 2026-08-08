#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

/*
 * PitchDetector - a monophonic guitar/bass tuner's pitch estimator.
 *
 * Unlike every other class in core/, this doesn't derive from AudioBlock -
 * it doesn't transform the signal at all, it just listens to a copy of it
 * (see ChainPlugin::run()'s tuner tap) and periodically re-estimates the
 * fundamental frequency. There's nothing to reorder or bypass, so it
 * doesn't need to fit the pedalboard's AudioBlock shape.
 *
 * How it works (autocorrelation, the standard approach for monophonic
 * pitch tracking):
 *   - Samples accumulate into a ring buffer. Every `hopSize` samples, we
 *     run one analysis pass over the last `windowSize` samples.
 *   - For a truly periodic signal, shifting it by exactly one period and
 *     multiplying against itself (unshifted) gives a strong positive
 *     correlation - stronger than almost any other shift. So we compute
 *     that correlation for every candidate shift ("lag") in the guitar's
 *     frequency range and look for the peak.
 *   - We take the *first* strong peak rather than the single strongest
 *     one: a string's second harmonic often correlates even more
 *     strongly than the fundamental at double the lag, which would
 *     otherwise report a note an octave too high.
 *   - Parabolic interpolation across the three points around that peak
 *     gives a sub-sample-accurate lag, which is what makes the cents
 *     readout precise instead of jumping in whole-Hz steps.
 */

namespace ampforge {

class PitchDetector
{
public:
    void setSampleRate(double sr)
    {
        sampleRate = sr;

        // Sized in time rather than a fixed sample count, so the tuner
        // behaves the same (same responsiveness, same low-note range)
        // regardless of the host's sample rate.
        windowSize = static_cast<size_t>(sr * kWindowSeconds);
        hopSize = windowSize / 2;

        // Clamped to at least 1: below kMaxFrequencyHz (1200Hz) of sample
        // rate, sr / kMaxFrequencyHz truncates to 0, which would make the
        // corr[]-fill loop below start at (size_t)(0 - 1) - an unsigned
        // wraparound to SIZE_MAX instead of running backwards as intended.
        minLag = std::max<size_t>(1, static_cast<size_t>(sr / kMaxFrequencyHz));
        maxLag = static_cast<size_t>(sr / kMinFrequencyHz);
        maxLag = std::min(maxLag, windowSize / 2); // stay well inside the window

        buffer.assign(windowSize, 0.0f);
        // Pre-sized here (not left for beginAnalysis()'s first resize()
        // call) so that first call doesn't allocate on the audio thread.
        linear.assign(windowSize, 0.0f);
        corr.assign(maxLag + 2, 0.0f);
        writeIndex = 0;
        samplesSinceAnalysis = 0;
        analysisInProgress = false;
        frequencyHz = 0.0f;
        confident = false;
        candidateStreak = 0;
    }

    // Feed one (mono) sample. Cheap - just a ring-buffer write, plus at
    // most one autocorrelation lag's worth of work (see stepAnalysis()'s
    // comment on why it's spread out like this instead of running the
    // whole pass in one go every hopSize samples).
    void pushSample(float sample)
    {
        if (buffer.empty())
            return; // setSampleRate() hasn't been called yet

        buffer[writeIndex] = sample;
        writeIndex = (writeIndex + 1) % windowSize;

        if (++samplesSinceAnalysis >= hopSize)
        {
            samplesSinceAnalysis = 0;
            beginAnalysis();
        }

        stepAnalysis();
    }

    // 0.0f when no clear pitch has been found yet (silence, noise, or
    // setSampleRate() hasn't run) - see isConfident().
    float getFrequencyHz() const { return confident ? frequencyHz : 0.0f; }
    bool isConfident() const { return confident; }

private:
    // Snapshots the window and DC-removes it (both O(windowSize), cheap
    // on their own) and decides whether there's enough signal to bother
    // with the expensive part - the actual per-lag autocorrelation, which
    // stepAnalysis() does incrementally from here.
    void beginAnalysis()
    {
        // Unrolled into linear (oldest-to-newest) order so the
        // correlation loop below can index it directly instead of
        // wrapping through the ring buffer on every access.
        linear.resize(windowSize);
        for (size_t i = 0; i < windowSize; ++i)
            linear[i] = buffer[(writeIndex + i) % windowSize];

        // Remove DC offset - an unremoved bias inflates every
        // correlation value (most of all lag 0), which would otherwise
        // throw off both peak-picking and the confidence ratio below.
        float mean = 0.0f;
        for (float s : linear) mean += s;
        mean /= static_cast<float>(windowSize);
        for (float& s : linear) s -= mean;

        // Not enough signal to bother analyzing (e.g. muted/untouched
        // strings) - leave the last result in place rather than reporting
        // a note found in whatever's left: interface self-noise, hiss,
        // room noise picked up by the pickups, etc. kSilenceRmsSquared is
        // an RMS-level threshold (roughly -50dBFS - well above any normal
        // noise floor, even amplified by a hot Input Gain setting for a
        // passive pickup, but comfortably below an actually plucked
        // string) rather than a raw energy total, so it means the same
        // thing regardless of windowSize - i.e. regardless of the host's
        // sample rate.
        float energy = 0.0f;
        for (float s : linear) energy += s * s;
        if (energy < kSilenceRmsSquared * static_cast<float>(windowSize))
        {
            confident = false;
            analysisInProgress = false;
            candidateStreak = 0;
            return;
        }

        cachedR0 = autocorrelate(0);

        // Filled from minLag-1 through maxLag+1 (not just [minLag,maxLag])
        // so the neighbor lookups in finishAnalysis() (corr[lag-1]/corr[lag+1])
        // are always this pass's real values, never a stale/zero leftover
        // from corr[]'s persistent scratch buffer. Computed one lag per
        // pushSample() call (see stepAnalysis()) rather than all at once
        // here.
        nextLag = minLag - 1;
        lastLag = maxLag + 1;
        analysisInProgress = true;
    }

    // Computes exactly one autocorrelation lag per call. A full pass is
    // O(windowSize * numLags) - measured to peg an entire audio-thread
    // core when done in one blocking call every hopSize samples, since
    // that one call's runtime could exceed the host's per-block real-time
    // budget and cause dropouts. numLags is comfortably smaller than
    // hopSize across the guitar/bass range this class covers (see
    // setSampleRate()), so pacing it at one lag per incoming sample still
    // finishes well before the next hop boundary - same total work as
    // before, just spread evenly across every sample's processing instead
    // of dumped into one call.
    void stepAnalysis()
    {
        if (!analysisInProgress)
            return;

        corr[nextLag] = autocorrelate(nextLag);

        if (nextLag == lastLag)
        {
            analysisInProgress = false;
            finishAnalysis();
        }
        else
        {
            ++nextLag;
        }
    }

    void finishAnalysis()
    {
        const float r0 = cachedR0;

        // First local maximum whose correlation is a strong-enough
        // fraction of r0 wins outright (see the class comment on why
        // "first" beats "strongest" here); otherwise fall back to
        // whichever lag scored highest.
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

        // Octave-too-high correction: a real string's even harmonics can
        // make the *first* strong peak land at half the true period - the
        // fundamental and every harmonic all correlate maximally at
        // lag = trueperiod (and its multiples), but a dominant 2nd
        // harmonic on its own also correlates strongly at half that lag,
        // and since the scan above runs from short lags (high frequency)
        // upward, it hits that false half-period peak first. If doubling
        // chosenLag lands on a correlation that's at least as strong,
        // that's the real giveaway - the true fundamental's period is the
        // doubled one, not this one. Repeated in case more than one
        // octave's worth of even-harmonic energy is fooling it. Bounded to
        // maxLag (not maxLag+1) so chosenLag stays in [minLag,maxLag] -
        // the same range the loop above guarantees - since the parabolic
        // interpolation below reads corr[chosenLag+1], and corr[] only
        // goes up to index maxLag+1.
        while (chosenLag * 2 <= maxLag && corr[chosenLag * 2] >= corr[chosenLag] && corr[chosenLag * 2] > kPeakThreshold * r0)
            chosenLag *= 2;

        const float confidenceRatio = (r0 > 0.0f) ? (corr[chosenLag] / r0) : 0.0f;
        confident = confidenceRatio > kConfidenceThreshold;
        if (!confident)
        {
            candidateStreak = 0;
            return;
        }

        // Parabolic interpolation across (chosenLag-1, chosenLag, chosenLag+1)
        // for a fractional-sample lag estimate - without it, frequency
        // resolution would be limited to whole-sample steps (several Hz
        // at guitar frequencies, easily 10+ cents of error).
        const float yLeft = corr[chosenLag - 1];
        const float yCenter = corr[chosenLag];
        const float yRight = corr[chosenLag + 1];
        const float denom = yLeft - 2.0f * yCenter + yRight;
        const float shift = (std::fabs(denom) > 1e-9f) ? 0.5f * (yLeft - yRight) / denom : 0.0f;
        const float interpolatedLag = static_cast<float>(chosenLag) + std::clamp(shift, -1.0f, 1.0f);

        if (interpolatedLag <= 0.0f)
            return;

        const float rawFrequencyHz = static_cast<float>(sampleRate) / interpolatedLag;

        // Lock-and-hold: a single hop landing on a competing candidate
        // (a harmonic that briefly out-scored the fundamental, a pick
        // transient) shouldn't yank the displayed note around - a median
        // alone doesn't fully fix that, since a reading that keeps
        // alternating between two candidates can still flip the median
        // every other hop. So a new candidate has to repeat within
        // kLockToleranceRatio of itself for kLockStreak consecutive hops
        // before it actually updates frequencyHz; until then the
        // previous locked note (or nothing, right after a gap - see the
        // confident=false paths' reset of candidateStreak) keeps
        // showing instead of flickering through every raw guess. Once
        // locked, it keeps gently tracking the note (pitch bends,
        // vibrato) rather than freezing.
        if (candidateStreak == 0 || std::fabs(rawFrequencyHz - candidateFreq) > candidateFreq * kLockToleranceRatio)
        {
            candidateFreq = rawFrequencyHz;
            candidateStreak = 1;
        }
        else
        {
            candidateFreq += (rawFrequencyHz - candidateFreq) * 0.5f;
            ++candidateStreak;
        }

        if (candidateStreak >= kLockStreak)
            frequencyHz = candidateFreq;
    }

    // Plain (unnormalized) autocorrelation at a given lag: how strongly
    // linear[] resembles a copy of itself shifted by `lag` samples.
    float autocorrelate(size_t lag) const
    {
        float sum = 0.0f;
        for (size_t i = 0; i + lag < windowSize; ++i)
            sum += linear[i] * linear[i + lag];
        return sum;
    }

    // Guitar/bass fundamental range, with headroom on both ends (open
    // low B on a 5-string down to high notes well up the neck).
    static constexpr float kMinFrequencyHz = 38.0f;
    static constexpr float kMaxFrequencyHz = 1200.0f;
    static constexpr float kWindowSeconds = 0.09f; // ~90ms: several periods even at the lowest note
    static constexpr float kPeakThreshold = 0.35f;        // vs r0, for early-exit "first peak" search
    static constexpr float kConfidenceThreshold = 0.45f;  // vs r0, for accepting the final chosen lag
    static constexpr float kSilenceRmsSquared = 1e-5f;    // ~-50dBFS RMS - see beginAnalysis()'s comment
    static constexpr float kLockToleranceRatio = 0.03f;   // vs the candidate - see finishAnalysis()'s comment
    static constexpr int kLockStreak = 3;                  // consecutive agreeing hops needed to lock

    double sampleRate = 44100.0;
    size_t windowSize = 0, hopSize = 0;
    size_t minLag = 0, maxLag = 0;

    std::vector<float> buffer;      // ring buffer, most recent windowSize samples
    size_t writeIndex = 0;
    size_t samplesSinceAnalysis = 0;

    std::vector<float> linear;      // scratch: buffer unwrapped into time order
    std::vector<float> corr;        // scratch: correlation per lag

    // Incremental-analysis state - see stepAnalysis()'s comment on why
    // one pass is spread across many pushSample() calls instead of run
    // in one go.
    bool analysisInProgress = false;
    size_t nextLag = 0, lastLag = 0;
    float cachedR0 = 0.0f;

    float frequencyHz = 0.0f;
    bool confident = false;

    // Lock-and-hold state for the reported frequency - see
    // finishAnalysis()'s comment. Reset (candidateStreak = 0) whenever a
    // hop isn't confident, so a fresh run of confident readings after a
    // gap has to earn its own lock instead of inheriting a stale streak.
    float candidateFreq = 0.0f;
    int candidateStreak = 0;
};

} // namespace ampforge
