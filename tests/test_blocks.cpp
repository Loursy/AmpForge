#include "test_harness.hpp"

#include "EffectChain.hpp"

#include "AmpBlock.hpp"
#include "AutotuneBlock.hpp"
#include "CabinetBlock.hpp"
#include "ChorusBlock.hpp"
#include "CompressorBlock.hpp"
#include "DeEsserBlock.hpp"
#include "DelayBlock.hpp"
#include "DistortionBlock.hpp"
#include "DoublerBlock.hpp"
#include "EqBlock.hpp"
#include "ExciterBlock.hpp"
#include "HarmonizerBlock.hpp"
#include "NoiseGateBlock.hpp"
#include "PhaserBlock.hpp"
#include "ReverbBlock.hpp"
#include "ScreamerBlock.hpp"
#include "TremoloBlock.hpp"
#include "WahBlock.hpp"

#include <cmath>
#include <memory>
#include <vector>

/*
 * Behavioural tests for the individual DSP blocks, plus a sweep that runs
 * every block (and then all of them chained together) through real audio
 * and asserts nothing produces NaN/Inf or blows up.
 *
 * That sweep is the cheap safety net this project has been missing: it
 * can't tell you a Wah sounds like a Wah, but it does catch the failure
 * mode that actually breaks a session - a block emitting NaN, which then
 * spreads through every block after it in the chain and silences the
 * output entirely.
 */

namespace {

constexpr double kSampleRate = 48000.0;

struct PeakInfo
{
    float peak = 0.0f;
    bool allFinite = true;
};

// Feeds `samples` of a sine through `block`, returning the peak of the
// second half only, so the filter/compressor's start-up transient doesn't
// dominate the measurement.
PeakInfo runSine(ampforge::AudioBlock& block, double frequencyHz, float amplitude, int samples)
{
    const double omega = 2.0 * M_PI * frequencyHz / kSampleRate;
    PeakInfo info;

    for (int n = 0; n < samples; ++n)
    {
        const float input = static_cast<float>(amplitude * std::sin(omega * n));
        const float output = block.processSample(input);

        if (!std::isfinite(output))
            info.allFinite = false;

        if (n >= samples / 2)
            info.peak = std::max(info.peak, std::fabs(output));
    }

    return info;
}

} // namespace

AMPFORGE_TEST(delay_puts_the_echo_at_the_requested_time)
{
    ampforge::DelayBlock delay;
    delay.setSampleRate(kSampleRate);
    delay.setDelayTimeMs(10.0f);   // 480 samples at 48 kHz
    delay.setFeedback(0.0f);
    delay.setMix(1.0f);            // fully wet, so the dry signal is gone

    // Impulse, then silence.
    const int delaySamples = 480;
    float peakAtDelay = 0.0f;
    for (int n = 0; n < 2000; ++n)
    {
        const float output = delay.processSample(n == 0 ? 1.0f : 0.0f);
        CHECK_FINITE(output);

        if (n == delaySamples)
            peakAtDelay = output;

        // Nothing should come out before the delay time.
        if (n > 0 && n < delaySamples)
            CHECK_NEAR(output, 0.0f, 1e-6f);
    }

    CHECK_NEAR(peakAtDelay, 1.0f, 0.01f);
}

AMPFORGE_TEST(delay_with_zero_mix_is_a_passthrough)
{
    ampforge::DelayBlock delay;
    delay.setSampleRate(kSampleRate);
    delay.setDelayTimeMs(100.0f);
    delay.setFeedback(0.5f);
    delay.setMix(0.0f);

    for (int n = 0; n < 1000; ++n)
    {
        const float input = std::sin(0.05f * n) * 0.5f;
        CHECK_NEAR(delay.processSample(input), input, 1e-6f);
    }
}

AMPFORGE_TEST(delay_reset_clears_a_pending_echo)
{
    ampforge::DelayBlock delay;
    delay.setSampleRate(kSampleRate);
    delay.setDelayTimeMs(10.0f);
    delay.setFeedback(0.0f);
    delay.setMix(1.0f);

    delay.processSample(1.0f); // schedule an echo 480 samples from now

    delay.reset();

    // The echo must be gone: nothing but silence for the next 2000 samples.
    for (int n = 0; n < 2000; ++n)
        CHECK_NEAR(delay.processSample(0.0f), 0.0f, 1e-6f);
}

AMPFORGE_TEST(noise_gate_opens_for_loud_signal_and_closes_for_quiet_signal)
{
    // Loud: -6 dBFS, well above the default -50 dB threshold.
    ampforge::NoiseGateBlock loudGate;
    loudGate.setSampleRate(kSampleRate);
    const PeakInfo loud = runSine(loudGate, 220.0, 0.5f, 48000);
    CHECK(loud.allFinite);
    CHECK_NEAR(loud.peak, 0.5f, 0.05f);

    // Quiet: -60 dBFS, below the threshold, so it should be pulled down
    // toward the gate's floor rather than passed through.
    ampforge::NoiseGateBlock quietGate;
    quietGate.setSampleRate(kSampleRate);
    const PeakInfo quiet = runSine(quietGate, 220.0, 0.001f, 48000);
    CHECK(quiet.allFinite);
    CHECK(quiet.peak < 0.001f * 0.1f);
}

AMPFORGE_TEST(compressor_reduces_gain_above_threshold)
{
    ampforge::CompressorBlock compressor;
    compressor.setSampleRate(kSampleRate);
    compressor.setThresholdDB(-18.0f);
    compressor.setRatio(4.0f);
    compressor.setMakeupGainDB(0.0f);

    // 0.5 amplitude is about -6 dBFS, i.e. 12 dB over the threshold -
    // with a 4:1 ratio that should come out roughly 9 dB quieter.
    const PeakInfo compressed = runSine(compressor, 220.0, 0.5f, 48000);
    CHECK(compressed.allFinite);
    CHECK(compressed.peak < 0.5f * 0.5f);  // at least 6 dB of reduction
    CHECK(compressed.peak > 0.05f);        // but not muted either
}

AMPFORGE_TEST(compressor_leaves_quiet_signal_alone)
{
    ampforge::CompressorBlock compressor;
    compressor.setSampleRate(kSampleRate);
    compressor.setThresholdDB(-18.0f);
    compressor.setRatio(4.0f);

    // -40 dBFS is well under the threshold, so gain reduction is 0.
    const PeakInfo quiet = runSine(compressor, 220.0, 0.01f, 48000);
    CHECK(quiet.allFinite);
    CHECK_NEAR(quiet.peak, 0.01f, 0.001f);
}

AMPFORGE_TEST(every_block_stays_finite_on_real_audio)
{
    // Default settings for all of them: no IR loaded for Cabinet - a
    // documented pass-through in that state, and loading one here would
    // mean shipping a binary test fixture. NamBlock is left out entirely:
    // it includes NAM's own headers and links nam_core, which would drag
    // the whole fetch into these otherwise dependency-free tests (and its
    // interesting logic is model loading, which needs a .nam fixture).
    std::vector<std::unique_ptr<ampforge::AudioBlock>> blocks;
    blocks.emplace_back(new ampforge::NoiseGateBlock());
    blocks.emplace_back(new ampforge::CompressorBlock());
    blocks.emplace_back(new ampforge::WahBlock());
    blocks.emplace_back(new ampforge::ScreamerBlock());
    blocks.emplace_back(new ampforge::DistortionBlock());
    blocks.emplace_back(new ampforge::AmpBlock());
    blocks.emplace_back(new ampforge::CabinetBlock());
    blocks.emplace_back(new ampforge::ChorusBlock());
    blocks.emplace_back(new ampforge::PhaserBlock());
    blocks.emplace_back(new ampforge::TremoloBlock());
    blocks.emplace_back(new ampforge::DelayBlock());
    blocks.emplace_back(new ampforge::ReverbBlock());
    blocks.emplace_back(new ampforge::EqBlock());
    blocks.emplace_back(new ampforge::AutotuneBlock());
    blocks.emplace_back(new ampforge::DeEsserBlock());
    blocks.emplace_back(new ampforge::DoublerBlock());
    blocks.emplace_back(new ampforge::HarmonizerBlock());
    blocks.emplace_back(new ampforge::ExciterBlock());

    for (auto& block : blocks)
    {
        block->setSampleRate(kSampleRate);

        const PeakInfo info = runSine(*block, 220.0, 0.2f, 9600);
        if (!info.allFinite)
        {
            char detail[128];
            std::snprintf(detail, sizeof detail, "%s produced a non-finite sample", block->getName());
            ::ampforge_test::reportFailure(__FILE__, __LINE__, "output is finite", detail);
        }
    }
}

AMPFORGE_TEST(a_full_chain_of_every_block_stays_finite_and_bounded)
{
    ampforge::NoiseGateBlock gate;
    ampforge::CompressorBlock compressor;
    ampforge::WahBlock wah;
    ampforge::ScreamerBlock screamer;
    ampforge::DistortionBlock distortion;
    ampforge::AmpBlock amp;
    ampforge::CabinetBlock cabinet;
    ampforge::ChorusBlock chorus;
    ampforge::PhaserBlock phaser;
    ampforge::TremoloBlock tremolo;
    ampforge::DelayBlock delay;
    ampforge::ReverbBlock reverb;
    ampforge::EqBlock eq;
    ampforge::AutotuneBlock autotune;
    ampforge::DeEsserBlock deEsser;
    ampforge::DoublerBlock doubler;
    ampforge::HarmonizerBlock harmonizer;
    ampforge::ExciterBlock exciter;

    // Every block enabled at once, in one chain - the worst case for
    // accumulated gain, and the arrangement a user can actually create by
    // dragging every pedal in.
    ampforge::EffectChain chain;
    ampforge::AudioBlock* all[] = {
        &gate, &compressor, &wah, &screamer, &distortion, &amp, &cabinet,
        &chorus, &phaser, &tremolo, &delay, &reverb, &eq, &autotune, &deEsser,
        &doubler, &harmonizer, &exciter,
    };

    int position = 0;
    for (ampforge::AudioBlock* block : all)
    {
        block->setSampleRate(kSampleRate);
        chain.addBlock(block, position++);
    }
    chain.rebuildOrder();

    const double omega = 2.0 * M_PI * 220.0 / kSampleRate;
    float peak = 0.0f;
    for (int n = 0; n < 48000; ++n)
    {
        const float input = static_cast<float>(0.2 * std::sin(omega * n));
        const float output = chain.processSample(input);

        CHECK_FINITE(output);
        peak = std::max(peak, std::fabs(output));
    }

    // Nothing in the default chain should turn a -14 dBFS input into
    // something that clips the host into oblivion.
    CHECK(peak < 20.0f);
}
