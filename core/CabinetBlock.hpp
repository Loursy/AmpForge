#pragma once
#include "AudioBlock.hpp"
#include "Denormal.hpp"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

/*
 * CabinetBlock - convolves the signal with a loaded speaker-cabinet
 * impulse response (a short WAV recording of how a real cab + mic
 * responds to an impulse). This is what actually gives a driven amp
 * model its "coming out of a real speaker" character - without it, even
 * a good amp model on its own tends to sound thin, harsh, or obviously
 * synthetic, since a real guitar amp is never heard without its cab.
 *
 * DSP approach: direct (non-FFT) time-domain convolution via a circular
 * history buffer. This is the simple, obviously-correct way to do it, at
 * the cost of being O(IR length) per sample rather than the O(log N) an
 * FFT-partitioned convolver would give you - a real product would likely
 * want that for long IRs, but for a cabinet IR (as opposed to a reverb
 * tail) the useful length is short: kMaxIRSamples caps it generously
 * while keeping the CPU cost per instance reasonable.
 *
 * Loading a new IR happens on whatever non-audio thread the host calls
 * setState() from (see ChainPlugin.cpp), while audio keeps running on a
 * different thread. That's handled with a lock-free double buffer: the
 * *inactive* IR/length slot is filled in completely off the audio
 * thread, then a single atomic index publishes it - the audio thread
 * only ever reads a fully-formed buffer, never a half-written one, and
 * loadImpulseResponse() never touches anything the audio thread reads
 * from until that final atomic store.
 */

namespace ampforge {

class CabinetBlock : public AudioBlock
{
public:
    static constexpr size_t kMaxIRSamples = 4096;
    static constexpr size_t kHistoryMask  = kMaxIRSamples - 1; // kMaxIRSamples is a power of 2

    CabinetBlock()
    {
        history.fill(0.0f);
        for (auto& buf : irBuffers)
            buf.fill(0.0f);
    }

    void setSampleRate(double sr) override { sampleRate = sr; }
    const char* getName() const override { return "Cabinet"; }

    void setMix(float m)    { mix = std::clamp(m, 0.0f, 1.0f); }
    void setLevel(float db) { levelLinear = std::pow(10.0f, db / 20.0f); }

    float processSample(float input) override
    {
        const int active = activeBuffer.load(std::memory_order_acquire);
        const size_t len = irLength[static_cast<size_t>(active)];

        history[historyPos] = input;

        if (len == 0)
        {
            // Nothing loaded yet (or load failed) - stay a clean
            // pass-through instead of going silent, regardless of Mix.
            historyPos = (historyPos + 1) & kHistoryMask;
            return input;
        }

        const std::array<float, kMaxIRSamples>& ir = irBuffers[static_cast<size_t>(active)];
        size_t readPos = historyPos;
        float acc = 0.0f;
        for (size_t i = 0; i < len; ++i)
        {
            acc += ir[i] * history[readPos];
            readPos = (readPos - 1) & kHistoryMask;
        }
        acc = flushDenormal(acc);

        historyPos = (historyPos + 1) & kHistoryMask;

        const float wet = acc * levelLinear;
        return input * (1.0f - mix) + wet * mix;
    }

    // Loads a mono-downmixed impulse response from a 8/16/24/32-bit PCM or
    // 32-bit float WAV file, resamples it to the current sample rate if
    // needed, and normalizes its peak to 1.0 so switching between cabs
    // doesn't cause wild level jumps. Safe to call from any non-audio
    // thread while audio keeps running (see the class comment above) -
    // never allocates or touches the audio thread's active buffer until
    // the very last step. Returns false (leaving whatever was loaded
    // before untouched) if the file can't be read or parsed.
    bool loadImpulseResponse(const std::string& path)
    {
        std::vector<float> raw;
        uint32_t fileSampleRate = 0;
        if (!readMonoWav(path, raw, fileSampleRate) || raw.empty())
            return false;

        if (fileSampleRate > 0 && sampleRate > 0.0 &&
            static_cast<double>(fileSampleRate) != sampleRate)
        {
            raw = resampleLinear(raw, static_cast<double>(fileSampleRate), sampleRate);
        }

        if (raw.size() > kMaxIRSamples)
            raw.resize(kMaxIRSamples);

        float peak = 0.0f;
        for (const float v : raw)
            peak = std::max(peak, std::fabs(v));
        if (peak > 1.0e-9f)
        {
            const float norm = 1.0f / peak;
            for (float& v : raw)
                v *= norm;
        }

        const int active = activeBuffer.load(std::memory_order_acquire);
        const size_t next = 1 - static_cast<size_t>(active);

        std::array<float, kMaxIRSamples>& target = irBuffers[next];
        target.fill(0.0f);
        std::copy(raw.begin(), raw.end(), target.begin());
        irLength[next] = raw.size();

        activeBuffer.store(static_cast<int>(next), std::memory_order_release);
        return true;
    }

    // True once a WAV has actually been loaded (as opposed to the empty,
    // pass-through starting state).
    bool hasImpulseResponse() const
    {
        return irLength[static_cast<size_t>(activeBuffer.load(std::memory_order_relaxed))] > 0;
    }

private:
    static uint16_t readU16(const uint8_t* p)
    {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }

    static uint32_t readU32(const uint8_t* p)
    {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    static float decodeSample(const uint8_t* s, size_t bytesPerSample, uint16_t formatCode)
    {
        if (formatCode == 3) // IEEE float
            return (bytesPerSample == 4) ? bitsToFloat(readU32(s)) : 0.0f;

        // PCM. 8-bit WAV samples are unsigned; everything wider is signed.
        switch (bytesPerSample)
        {
        case 1:
            return (static_cast<int32_t>(s[0]) - 128) / 128.0f;
        case 2:
            return static_cast<int16_t>(readU16(s)) / 32768.0f;
        case 3:
        {
            int32_t v = static_cast<int32_t>(s[0]) | (static_cast<int32_t>(s[1]) << 8) | (static_cast<int32_t>(s[2]) << 16);
            if (v & 0x00800000)
                v |= static_cast<int32_t>(0xFF000000);
            return v / 8388608.0f;
        }
        case 4:
            return static_cast<int32_t>(readU32(s)) / 2147483648.0f;
        default:
            return 0.0f;
        }
    }

    static float bitsToFloat(uint32_t bits)
    {
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    // Reads a RIFF/WAVE file's "fmt " and "data" chunks (skipping any
    // others, e.g. "LIST"/"fact"/metadata) and downmixes every channel to
    // mono by averaging. Only plain PCM and IEEE-float sample formats are
    // supported (including the WAVE_FORMAT_EXTENSIBLE wrapper some DAWs
    // use) - compressed WAV variants are not.
    static bool readMonoWav(const std::string& path, std::vector<float>& outSamples, uint32_t& outSampleRate)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open())
            return false;

        char tag[4];
        f.read(tag, 4);
        if (!f || std::memcmp(tag, "RIFF", 4) != 0)
            return false;
        f.seekg(4, std::ios::cur); // overall RIFF chunk size, unused
        f.read(tag, 4);
        if (!f || std::memcmp(tag, "WAVE", 4) != 0)
            return false;

        uint16_t formatCode = 0, numChannels = 0, bitsPerSample = 0;
        uint32_t sampleRateHz = 0;
        std::vector<uint8_t> dataBytes;
        bool haveFmt = false, haveData = false;

        while (f && !(haveFmt && haveData))
        {
            char chunkId[4];
            char sizeBytes[4];
            f.read(chunkId, 4);
            f.read(sizeBytes, 4);
            if (!f)
                break;
            const uint32_t chunkSize = readU32(reinterpret_cast<const uint8_t*>(sizeBytes));

            if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16)
            {
                std::vector<uint8_t> fmtBytes(chunkSize);
                f.read(reinterpret_cast<char*>(fmtBytes.data()), static_cast<std::streamsize>(chunkSize));
                if (!f)
                    break;

                formatCode    = readU16(fmtBytes.data() + 0);
                numChannels   = readU16(fmtBytes.data() + 2);
                sampleRateHz  = readU32(fmtBytes.data() + 4);
                bitsPerSample = readU16(fmtBytes.data() + 14);

                if (formatCode == 0xFFFE && chunkSize >= 40) // WAVE_FORMAT_EXTENSIBLE
                    formatCode = readU16(fmtBytes.data() + 24);

                haveFmt = true;
            }
            else if (std::memcmp(chunkId, "data", 4) == 0)
            {
                dataBytes.resize(chunkSize);
                f.read(reinterpret_cast<char*>(dataBytes.data()), static_cast<std::streamsize>(chunkSize));
                haveData = true;
            }
            else
            {
                f.seekg(chunkSize, std::ios::cur);
            }

            if (chunkSize % 2 != 0) // chunks are word-aligned
                f.seekg(1, std::ios::cur);
        }

        if (!haveFmt || !haveData || numChannels == 0)
            return false;
        if (formatCode != 1 && formatCode != 3) // only plain PCM or IEEE float
            return false;

        const size_t bytesPerSample = bitsPerSample / 8;
        const size_t frameBytes = bytesPerSample * numChannels;
        if (bytesPerSample == 0 || frameBytes == 0)
            return false;

        const size_t numFrames = dataBytes.size() / frameBytes;
        outSamples.resize(numFrames);
        for (size_t i = 0; i < numFrames; ++i)
        {
            const uint8_t* frame = dataBytes.data() + i * frameBytes;
            float sum = 0.0f;
            for (uint32_t ch = 0; ch < numChannels; ++ch)
                sum += decodeSample(frame + ch * bytesPerSample, bytesPerSample, formatCode);
            outSamples[i] = sum / static_cast<float>(numChannels);
        }

        outSampleRate = sampleRateHz;
        return true;
    }

    static std::vector<float> resampleLinear(const std::vector<float>& input, double fromRate, double toRate)
    {
        if (input.empty() || fromRate <= 0.0 || toRate <= 0.0 || fromRate == toRate)
            return input;

        const double ratio = fromRate / toRate;
        const size_t outLength = static_cast<size_t>(static_cast<double>(input.size()) / ratio);
        std::vector<float> out(outLength);

        for (size_t i = 0; i < outLength; ++i)
        {
            const double srcPos = static_cast<double>(i) * ratio;
            const size_t i0 = static_cast<size_t>(srcPos);
            const size_t i1 = std::min(i0 + 1, input.size() - 1);
            const float frac = static_cast<float>(srcPos - static_cast<double>(i0));
            out[i] = input[i0] * (1.0f - frac) + input[i1] * frac;
        }
        return out;
    }

    double sampleRate = 44100.0;
    float mix = 1.0f;
    float levelLinear = 1.0f;

    std::array<std::array<float, kMaxIRSamples>, 2> irBuffers{};
    size_t irLength[2] = { 0, 0 };
    std::atomic<int> activeBuffer{0};

    std::array<float, kMaxIRSamples> history{};
    size_t historyPos = 0;
};

} // namespace ampforge
