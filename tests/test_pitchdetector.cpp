#include "test_harness.hpp"

#include "PitchDetector.hpp"

#include <cmath>

/*
 * The tuner is the one place where "is this correct?" has a numeric
 * answer: feed a known sine and the reported pitch either is that
 * frequency or it isn't. Its autocorrelation is also the most expensive
 * per-sample work in the chain, so getting it wrong is expensive twice
 * over - wrong reading and wasted CPU.
 */

namespace {

constexpr double kSampleRate = 48000.0;

// Returns the detector's reading after feeding `seconds` of a sine.
float detect(double frequencyHz, double seconds = 1.0)
{
    ampforge::PitchDetector detector;
    detector.setSampleRate(kSampleRate);

    const double omega = 2.0 * M_PI * frequencyHz / kSampleRate;
    const int samples = static_cast<int>(kSampleRate * seconds);

    for (int n = 0; n < samples; ++n)
        detector.pushSample(static_cast<float>(0.5 * std::sin(omega * n)));

    return detector.getFrequencyHz();
}

} // namespace

AMPFORGE_TEST(pitch_detector_reads_a_concert_a)
{
    // 1 Hz of tolerance: the detector reports the frequency it locked
    // onto, and a semitone at 440 Hz is about 26 Hz, so anything within a
    // couple of Hz is unambiguously the right note.
    CHECK_NEAR(detect(440.0), 440.0, 2.0);
}

AMPFORGE_TEST(pitch_detector_reads_a_low_guitar_note)
{
    // Low E on a guitar in standard tuning is 82.41 Hz.
    CHECK_NEAR(detect(82.41), 82.41, 2.0);
}

AMPFORGE_TEST(pitch_detector_reads_a_high_note)
{
    // High E at the 24th fret is about 659 Hz.
    CHECK_NEAR(detect(659.26), 659.26, 4.0);
}

AMPFORGE_TEST(pitch_detector_reports_nothing_for_silence)
{
    ampforge::PitchDetector detector;
    detector.setSampleRate(kSampleRate);

    for (int n = 0; n < static_cast<int>(kSampleRate); ++n)
        detector.pushSample(0.0f);

    CHECK_NEAR(detector.getFrequencyHz(), 0.0f, 1e-9f);
    CHECK(!detector.isConfident());
}

AMPFORGE_TEST(pitch_detector_does_nothing_before_the_sample_rate_is_known)
{
    // processSample()/pushSample() run on the audio thread and the host is
    // free to call them before sampleRateChanged() lands; that must be a
    // safe no-op rather than an out-of-bounds write on an empty buffer.
    ampforge::PitchDetector detector;

    for (int n = 0; n < 100; ++n)
        detector.pushSample(0.5f);

    CHECK_NEAR(detector.getFrequencyHz(), 0.0f, 1e-9f);
    CHECK(!detector.isConfident());
}
