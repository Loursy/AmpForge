#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

/*
 * NamBlock - runs the signal through a neural network loaded from a .nam
 * file (NeuralAmpModelerCore, https://github.com/sdatkinson/NeuralAmpModelerCore),
 * a capture of one specific real amp/pedal/cab rather than a hand-tuned
 * emulation. This is a separate, optional block from AmpBlock - the two are
 * independent tone sources the user can mix and match, not alternatives to
 * pick between.
 *
 * Per-sample vs. block processing: nam::DSP::process() is designed to run
 * on a block of frames at once (that's where its internal batching
 * efficiency comes from), but every block in this chain - including the
 * heaviest one, CabinetBlock's convolution - is called once per sample via
 * AudioBlock::processSample(). Rather than restructure the chain, this
 * block just calls process() with num_frames = 1 every sample: correct,
 * zero added latency, and it keeps NamBlock a drop-in AudioBlock like
 * everything else, at the cost of losing some of NAM's batching efficiency.
 * Watch kParamCpuLoad if that ever turns out to matter.
 *
 * Loading a .nam file happens on whatever non-audio thread the host calls
 * setState() from (see ChainPlugin.cpp), while audio keeps running on a
 * different thread - handled with the same lock-free double buffer
 * CabinetBlock uses for its impulse response: the new model is fully
 * built and Reset() (which preallocates nam's internal buffers and
 * prewarms, settling initial WaveNet/LSTM state so there's no audible
 * startup transient) in the *inactive* slot, then a single atomic store
 * publishes it - the audio thread never sees a half-constructed model.
 */

namespace ampforge {

class NamBlock : public AudioBlock
{
public:
    void setSampleRate(double sr) override
    {
        sampleRate = sr;
        const int active = activeModel.load(std::memory_order_acquire);
        if (active >= 0 && models[static_cast<size_t>(active)])
            models[static_cast<size_t>(active)]->Reset(sampleRate, 1);
    }

    const char* getName() const override { return "NAM"; }

    void setInputTrim(float db)  { inputGainLinear = std::pow(10.0f, db / 20.0f); }
    void setOutputTrim(float db) { outputGainLinear = std::pow(10.0f, db / 20.0f); }
    void setMix(float m)         { mix = std::clamp(m, 0.0f, 1.0f); }

    float processSample(float input) override
    {
        const int active = activeModel.load(std::memory_order_acquire);
        if (active < 0 || !models[static_cast<size_t>(active)])
            return input; // no model loaded yet (or load failed) - clean pass-through

        NAM_SAMPLE inSample  = static_cast<NAM_SAMPLE>(input * inputGainLinear);
        NAM_SAMPLE outSample = 0;
        NAM_SAMPLE* inChannels[1]  = { &inSample };
        NAM_SAMPLE* outChannels[1] = { &outSample };
        models[static_cast<size_t>(active)]->process(inChannels, outChannels, 1);

        const float wet = flushDenormal(static_cast<float>(outSample)) * outputGainLinear;
        return input * (1.0f - mix) + wet * mix;
    }

    // Parses a .nam file and, on success, hot-swaps it in for the next
    // processSample() call. Safe to call from any non-audio thread while
    // audio keeps running (see the class comment above). Returns false
    // (leaving whatever was loaded before untouched) if the file can't be
    // read or parsed.
    bool loadModel(const std::string& path)
    {
        std::unique_ptr<nam::DSP> model;
        nam::dspData config;
        try
        {
            // Explicit std::filesystem::path: nam::get_dsp() is overloaded for
            // both a file path and a raw nlohmann::json config, and a plain
            // std::string converts implicitly to either - ambiguous otherwise.
            // The dspData overload also hands back the model's declared
            // architecture ("WaveNet", "LSTM", ...) for display purposes -
            // in particular, this is how the UI can confirm an A2 capture
            // actually loaded as A2 rather than falling back to A1 parsing.
            model = nam::get_dsp(std::filesystem::path(path), config);
        }
        catch (const std::exception& e)
        {
            lastError = e.what();
            return false;
        }
        if (!model)
        {
            lastError = "get_dsp() returned no model (unrecognized/unsupported .nam file)";
            return false;
        }

        // Preallocates nam's internal buffers and prewarms - must happen here,
        // off the audio thread, since process() would otherwise do this
        // lazily (and expensively) on its first call.
        model->Reset(sampleRate, 1);

        architecture = config.architecture;

        const int active = activeModel.load(std::memory_order_acquire);
        const size_t next = (active < 0) ? 0 : (1 - static_cast<size_t>(active));
        models[next] = std::move(model);
        activeModel.store(static_cast<int>(next), std::memory_order_release);
        return true;
    }

    // True once a model has actually been loaded (as opposed to the empty,
    // pass-through starting state).
    bool hasModel() const
    {
        return activeModel.load(std::memory_order_relaxed) >= 0;
    }

    // The loaded model's architecture name ("WaveNet", "LSTM", ...), as
    // reported by the .nam file itself - empty until a model has actually
    // been loaded. Purely informational (for the UI's status sidebar);
    // doesn't affect processing, since nam::DSP already dispatches to the
    // right implementation internally.
    const std::string& getArchitecture() const { return architecture; }

    // Why the most recent loadModel() call failed (exception message or
    // "no model" reason) - empty if the last call succeeded or none has
    // been made yet. Purely for the UI's failure toast (see ChainPlugin::
    // setState()); doesn't affect processing.
    const std::string& getLastError() const { return lastError; }

private:
    double sampleRate = 44100.0;
    float inputGainLinear  = 1.0f;
    float outputGainLinear = 1.0f;
    float mix = 1.0f;
    std::string architecture;
    std::string lastError;

    std::array<std::unique_ptr<nam::DSP>, 2> models;
    std::atomic<int> activeModel{-1};
};

} // namespace ampforge
