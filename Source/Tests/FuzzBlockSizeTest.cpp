#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>
#include "../DSP/ParametricEQProcessor.h"
#include "../DSP/DynamicEQProcessor.h"
#include "../DSP/LinearPhaseProcessor.h"

/**
 * Fuzz test: stress all DSP processors with varied block sizes and sample rates.
 *
 * Covers:
 *   - Block sizes: 1, 32, 64, 128, 256, 512, 1024, 2048, 4096
 *   - Sample rates: 44100, 48000, 88200, 96000
 *   - Mid-session block size change without re-prepare
 *   - 1-sample edge case
 */
class FuzzBlockSizeTest : public juce::UnitTest
{
public:
    FuzzBlockSizeTest() : juce::UnitTest("Fuzz BlockSize/SampleRate", "Regression") {}

    void runTest() override
    {
        const int blockSizes[]     = { 1, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };

        beginTest("ParametricEQ: varied block sizes and sample rates");
        for (double sr : sampleRates)
            for (int bs : blockSizes)
                runParametricEQ(sr, bs);

        beginTest("DynamicEQ: varied block sizes and sample rates");
        for (double sr : sampleRates)
            for (int bs : blockSizes)
                runDynamicEQ(sr, bs);

        beginTest("LinearPhase: varied block sizes and sample rates");
        const int lpBlockSizes[] = { 32, 64, 128, 256, 512, 1024 };
        for (double sr : sampleRates)
            for (int bs : lpBlockSizes)
                runLinearPhase(sr, bs);

        beginTest("ParametricEQ: mid-session block size change without re-prepare");
        testMidSessionChange();
    }

private:
    void runParametricEQ(double sr, int blockSize)
    {
        ParametricEQProcessor eq;
        eq.prepare(sr, blockSize, 2);

        eq.setBandEnabled(0, true);
        eq.setBandFrequency(0, 1000.0f);
        eq.setBandGain(0, -6.0f);

        juce::AudioBuffer<float> buf(2, blockSize);
        for (int i = 0; i < 3; ++i)
        {
            fillNoise(buf, i * 100 + blockSize);
            eq.process(buf);
        }

        juce::String label = "ParametricEQ sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    void runDynamicEQ(double sr, int blockSize)
    {
        DynamicEQProcessor deq;
        deq.prepare(sr, blockSize, 2);

        deq.setDynamicMode(0, DynamicEQProcessor::DynamicMode_Compress);

        juce::AudioBuffer<float> buf(2, blockSize);
        for (int i = 0; i < 3; ++i)
        {
            fillNoise(buf, i * 200 + blockSize);
            deq.process(buf);
        }

        juce::String label = "DynamicEQ sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    void runLinearPhase(double sr, int blockSize)
    {
        LinearPhaseProcessor lp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sr;
        spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
        spec.numChannels      = 2;
        lp.prepare(spec);

        // Flat magnitude response (identity)
        std::vector<float> mags(512, 1.0f);
        lp.updateImpulseResponse(mags, sr);

        juce::AudioBuffer<float> buf(2, blockSize);
        // Warm up: LP needs several blocks to fill convolution pipeline
        for (int i = 0; i < 8; ++i)
        {
            fillNoise(buf, i + blockSize);
            juce::dsp::AudioBlock<float> block(buf);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp.process(ctx);
        }

        juce::String label = "LinearPhase sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    void testMidSessionChange()
    {
        ParametricEQProcessor eq;
        eq.prepare(48000.0, 512, 2);
        eq.setBandEnabled(0, true);
        eq.setBandFrequency(0, 500.0f);
        eq.setBandGain(0, 3.0f);

        // Warm up at prepared size
        {
            juce::AudioBuffer<float> buf(2, 512);
            fillNoise(buf, 1);
            eq.process(buf);
        }

        // 3× the prepared size without re-prepare
        {
            juce::AudioBuffer<float> big(2, 1536);
            fillNoise(big, 2);
            eq.process(big);
            expect(checkFinite(big), "NaN/Inf after block size increase mid-session");
        }

        // 1-sample block
        {
            juce::AudioBuffer<float> tiny(2, 1);
            fillNoise(tiny, 3);
            eq.process(tiny);
            expect(checkFinite(tiny), "NaN/Inf on 1-sample block after mid-session change");
        }
    }

    // ─── helpers ─────────────────────────────────────────────────────────────
    static void fillNoise(juce::AudioBuffer<float>& buf, int seed)
    {
        std::mt19937 rng(static_cast<unsigned>(seed));
        std::uniform_real_distribution<float> dist(-0.25f, 0.25f);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* p = buf.getWritePointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                p[i] = dist(rng);
        }
    }

    static bool checkFinite(const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (!std::isfinite(buf.getReadPointer(ch)[i]))
                    return false;
        return true;
    }
};

static FuzzBlockSizeTest fuzzBlockSizeTest;
