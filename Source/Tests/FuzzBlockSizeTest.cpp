#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>
#include "../DSP/ParametricEQProcessor.h"
#include "../DSP/DynamicEQProcessor.h"
#include "../DSP/LinearPhaseProcessor.h"

/**
 * Fuzz test: stress-test DSP processors with varied block sizes and sample rates.
 *
 * Tests:
 *   - Block sizes: 1, 32, 64, 128, 256, 512, 1024, 2048, 4096
 *   - Sample rates: 44100, 48000, 88200, 96000
 *   - All three processors individually
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

        // ── ParametricEQ ─────────────────────────────────────────────────────
        beginTest("ParametricEQ: varied block sizes and sample rates");
        for (double sr : sampleRates)
            for (int bs : blockSizes)
                runParametricEQ(sr, bs);

        // ── DynamicEQ ────────────────────────────────────────────────────────
        beginTest("DynamicEQ: varied block sizes and sample rates");
        for (double sr : sampleRates)
            for (int bs : blockSizes)
                runDynamicEQ(sr, bs);

        // ── LinearPhase ──────────────────────────────────────────────────────
        beginTest("LinearPhase: varied block sizes and sample rates");
        // Skip 1-sample for LP (hopSize >> 1 sample, meaningless to test)
        const int lpBlockSizes[] = { 32, 64, 128, 256, 512, 1024 };
        for (double sr : sampleRates)
            for (int bs : lpBlockSizes)
                runLinearPhase(sr, bs);

        // ── Mid-session block size change ─────────────────────────────────────
        beginTest("ParametricEQ: mid-session block size change without re-prepare");
        testMidSessionChange();

        // ── 1-sample edge case ────────────────────────────────────────────────
        beginTest("ParametricEQ: 1-sample block edge case");
        runParametricEQ(48000.0, 1);
        beginTest("DynamicEQ: 1-sample block edge case");
        runDynamicEQ(48000.0, 1);
    }

private:
    // ─── ParametricEQ ─────────────────────────────────────────────────────────
    void runParametricEQ(double sr, int blockSize)
    {
        ParametricEQProcessor eq;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sr;
        spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
        spec.numChannels      = 2;
        eq.prepare(spec);

        // Enable a few bands with different types
        eq.setBandEnabled(0, true);
        eq.setBandFrequency(0, 1000.0f);
        eq.setBandGain(0, -6.0f);
        eq.setBandQ(0, 2.0f);

        juce::AudioBuffer<float> buf(2, blockSize);
        fillNoise(buf, blockSize + static_cast<int>(sr));
        processBuffer(eq, buf);

        juce::String label = "ParametricEQ sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    // ─── DynamicEQ ────────────────────────────────────────────────────────────
    void runDynamicEQ(double sr, int blockSize)
    {
        DynamicEQProcessor deq;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sr;
        spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
        spec.numChannels      = 2;
        deq.prepare(spec);

        deq.setDynamicMode(0, DynamicEQProcessor::DynamicMode::Compress);
        deq.setThreshold(0, -20.0f);
        deq.setRatio(0, 4.0f);

        juce::AudioBuffer<float> buf(2, blockSize);
        fillNoise(buf, blockSize + static_cast<int>(sr) + 1);

        // Process 3 blocks to warm up lookahead and smoothers
        for (int i = 0; i < 3; ++i)
        {
            fillNoise(buf, i * 1000 + blockSize);
            processBuffer(deq, buf);
        }

        juce::String label = "DynamicEQ sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    // ─── LinearPhase ──────────────────────────────────────────────────────────
    void runLinearPhase(double sr, int blockSize)
    {
        LinearPhaseProcessor lp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sr;
        spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
        spec.numChannels      = 2;
        lp.prepare(spec);

        // Build a flat IR (identity-ish response)
        std::vector<double> freqs, mags;
        for (int i = 1; i <= 20; ++i)
        {
            freqs.push_back(1000.0 * i);
            mags.push_back(1.0);
        }
        lp.updateImpulseResponse(freqs, mags);

        juce::AudioBuffer<float> buf(2, blockSize);

        // Warm up (LP needs several blocks to fill the convolution pipeline)
        for (int i = 0; i < 8; ++i)
        {
            fillNoise(buf, i + blockSize);
            processBuffer(lp, buf);
        }

        juce::String label = "LinearPhase sr=" + juce::String(static_cast<int>(sr))
                           + " bs=" + juce::String(blockSize);
        expect(checkFinite(buf), label + " produced NaN/Inf");
    }

    // ─── Mid-session block size change ────────────────────────────────────────
    void testMidSessionChange()
    {
        ParametricEQProcessor eq;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels      = 2;
        eq.prepare(spec);

        eq.setBandEnabled(0, true);
        eq.setBandFrequency(0, 500.0f);
        eq.setBandGain(0, 3.0f);

        juce::MidiBuffer midi;

        // Warm up
        {
            juce::AudioBuffer<float> buf(2, 512);
            fillNoise(buf, 1);
            processBuffer(eq, buf);
        }

        // Deliver 3x the prepared size without re-prepare
        {
            juce::AudioBuffer<float> bigBuf(2, 1536);
            fillNoise(bigBuf, 2);
            processBuffer(eq, bigBuf);
            expect(checkFinite(bigBuf), "NaN/Inf after mid-session block size increase");
        }

        // Tiny block
        {
            juce::AudioBuffer<float> tinyBuf(2, 1);
            fillNoise(tinyBuf, 3);
            processBuffer(eq, tinyBuf);
            expect(checkFinite(tinyBuf), "NaN/Inf on 1-sample block after mid-session change");
        }
    }

    // ─── helpers ──────────────────────────────────────────────────────────────
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

    template<typename Processor>
    static void processBuffer(Processor& proc, juce::AudioBuffer<float>& buf)
    {
        juce::dsp::AudioBlock<float> block(buf);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        proc.process(ctx);
    }
};

static FuzzBlockSizeTest fuzzBlockSizeTest;
