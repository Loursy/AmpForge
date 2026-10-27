#include "test_harness.hpp"

#include "Biquad.hpp"

#include <cmath>

/*
 * Biquad is the RBJ-cookbook filter behind the EQ module and the Wah's
 * band-pass, so its frequency response is the one place where "sounds
 * right" can actually be measured. magnitudeDB() reads the response
 * straight off the coefficients, which makes these checks cheap and
 * exact instead of having to measure an FFT of the impulse response.
 */

namespace {

constexpr double kSampleRate = 48000.0;

// Gain of the filter's actual time-domain output at a steady sine, in dB -
// measured independently of magnitudeDB(), so a bug in the coefficient
// math can't make both the filter and the measurement agree on a wrong
// answer.
//
// RMS, not peak: when the probe frequency divides the sample rate exactly
// (8 kHz at 48 kHz, say) the sampled points never land on the sine's crest,
// so a peak-of-samples reading comes out up to 1.25 dB low depending on the
// filter's phase shift. RMS over a long window doesn't care about phase.
double measureSineGainDB(ampforge::Biquad& filter, double frequencyHz, int samples = 48000)
{
    const double amplitude = 0.1;
    const double omega = 2.0 * M_PI * frequencyHz / kSampleRate;

    // Let the filter settle before measuring, so the start-up transient
    // doesn't skew the result.
    for (int n = 0; n < 4800; ++n)
        filter.process(static_cast<float>(amplitude * std::sin(omega * n)));

    double sumIn = 0.0;
    double sumOut = 0.0;
    for (int n = 4800; n < samples; ++n)
    {
        const double input = amplitude * std::sin(omega * n);
        const double output = filter.process(static_cast<float>(input));
        sumIn += input * input;
        sumOut += output * output;
    }

    if (sumIn <= 0.0 || sumOut <= 0.0)
        return -200.0;
    return 10.0 * std::log10(sumOut / sumIn);
}

} // namespace

AMPFORGE_TEST(biquad_peak_boosts_exactly_at_its_center_frequency)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::Peak, 1000.0f, 6.0f, 0.707f);

    CHECK_NEAR(filter.magnitudeDB(1000.0f), 6.0, 0.2);
    // A peak is narrow: far below the center it should be near unity.
    CHECK_NEAR(filter.magnitudeDB(20.0f), 0.0, 0.5);
}

AMPFORGE_TEST(biquad_peak_with_zero_gain_is_transparent)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::Peak, 1000.0f, 0.0f, 0.707f);

    CHECK_NEAR(filter.magnitudeDB(1000.0f), 0.0, 0.01);
    CHECK_NEAR(filter.magnitudeDB(100.0f), 0.0, 0.01);
    CHECK_NEAR(filter.magnitudeDB(10000.0f), 0.0, 0.01);
}

AMPFORGE_TEST(biquad_low_shelf_boosts_lows_and_leaves_highs_alone)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::LowShelf, 200.0f, 6.0f, 0.707f);

    CHECK_NEAR(filter.magnitudeDB(50.0f), 6.0, 0.5);
    CHECK_NEAR(filter.magnitudeDB(10000.0f), 0.0, 0.5);
}

AMPFORGE_TEST(biquad_high_shelf_cuts_highs_and_leaves_lows_alone)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::HighShelf, 3000.0f, -6.0f, 0.707f);

    CHECK_NEAR(filter.magnitudeDB(15000.0f), -6.0, 0.5);
    CHECK_NEAR(filter.magnitudeDB(50.0f), 0.0, 0.5);
}

AMPFORGE_TEST(biquad_highpass_attenuates_below_cutoff)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::HighPass, 1000.0f, 0.0f, 0.707f);

    CHECK(filter.magnitudeDB(100.0f) < -30.0f);
    CHECK_NEAR(filter.magnitudeDB(10000.0f), 0.0, 0.5);
}

AMPFORGE_TEST(biquad_magnitude_matches_measured_time_domain_gain)
{
    // The coefficient math and the actual process() output must agree.
    // This is what would catch an a0-normalization or sign slip that
    // leaves magnitudeDB() (pure math) looking fine while the audio path
    // filters with the wrong coefficients.
    struct Case { ampforge::Biquad::Type type; float freq; float gainDB; float probe; };
    const Case cases[] = {
        { ampforge::Biquad::Type::Peak,      1000.0f,  6.0f, 1000.0f },
        { ampforge::Biquad::Type::Peak,      1000.0f, -6.0f, 1000.0f },
        { ampforge::Biquad::Type::LowShelf,   200.0f,  6.0f,   80.0f },
        { ampforge::Biquad::Type::HighShelf, 3000.0f, -6.0f, 8000.0f },
        { ampforge::Biquad::Type::HighPass,  1000.0f,  0.0f, 8000.0f },
    };

    for (const Case& c : cases)
    {
        ampforge::Biquad filter;
        filter.setSampleRate(kSampleRate);
        filter.setParams(c.type, c.freq, c.gainDB, 0.707f);

        const double predicted = filter.magnitudeDB(c.probe);
        const double measured = measureSineGainDB(filter, c.probe);

        // Peak-of-sine measurement vs. the exact response: 0.5 dB is
        // plenty of room for the settle window and sample quantization.
        CHECK_NEAR(measured, predicted, 0.5);
    }
}

AMPFORGE_TEST(biquad_stays_finite_and_decays_to_silence)
{
    ampforge::Biquad filter;
    filter.setSampleRate(kSampleRate);
    filter.setParams(ampforge::Biquad::Type::Peak, 1000.0f, 12.0f, 4.0f);

    // Impulse, then silence: a stable filter's ring-down must die out
    // rather than stay stuck at a denormal or oscillate forever.
    float output = filter.process(1.0f);
    CHECK_FINITE(output);

    for (int n = 0; n < 48000; ++n)
    {
        output = filter.process(0.0f);
        CHECK_FINITE(output);
    }

    CHECK_NEAR(output, 0.0f, 1e-6f);
}
