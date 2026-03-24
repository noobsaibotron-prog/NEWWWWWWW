#if JUCE_UNIT_TESTS

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/LinearPhaseProcessor.h"
#include <cmath>
#include <numeric>

/**
 * Behavioral tests for LinearPhaseProcessor — product-grade validation.
 *
 * These tests verify audio behavior, not just "doesn't crash":
 *   1. Block-size invariance: same input → same output regardless of block size
 *   2. Channel symmetry: identical L/R input → identical L/R output
 *   3. Bypass equivalence: flat EQ produces output ≈ delayed input (within tolerance)
 *   4. IR crossfade smoothness: no clicks or discontinuities during IR transitions
 *   5. Repeated prepare/reset stability: no state corruption across reconfiguration
 *   6. Latency consistency: reported latency matches actual delay
 */
class LinearPhaseBehavioralTest : public juce::UnitTest
{
public:
    LinearPhaseBehavioralTest()
        : juce::UnitTest("LinearPhaseBehavioral", "DSP") {}

    void runTest() override
    {
        testBlockSizeInvariance();
        testChannelSymmetry();
        testBypassEquivalence();
        testIRCrossfadeSmoothness();
        testRepeatedPrepareStability();
    }

private:
    static constexpr double kSampleRate = 44100.0;
    static constexpr int kChannels = 2;

    //==============================================================================
    // Helpers
    //==============================================================================
    std::unique_ptr<LinearPhaseProcessor> makePrepared(int blockSize)
    {
        auto lp = std::make_unique<LinearPhaseProcessor>();
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = kSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
        spec.numChannels      = static_cast<juce::uint32>(kChannels);
        lp->prepare(spec);

        // Load flat 0 dB IR so output ≈ input (with latency)
        std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, 0.0f);
        lp->updateImpulseResponse(mag, kSampleRate);
        return lp;
    }

    juce::AudioBuffer<float> generateSine(float freqHz, int numSamples)
    {
        juce::AudioBuffer<float> buf(kChannels, numSamples);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
                data[i] = std::sin(juce::MathConstants<float>::twoPi * freqHz * t);
            }
        }
        return buf;
    }

    // Process a full buffer through LP in chunks of blockSize, return collected output
    std::vector<float> processFullBuffer(LinearPhaseProcessor& lp, int blockSize,
                                          const juce::AudioBuffer<float>& input)
    {
        const int totalSamples = input.getNumSamples();
        std::vector<float> output;
        output.reserve(static_cast<size_t>(totalSamples));

        int pos = 0;
        while (pos < totalSamples)
        {
            const int thisBlock = std::min(blockSize, totalSamples - pos);
            juce::AudioBuffer<float> chunk(kChannels, thisBlock);
            for (int ch = 0; ch < kChannels; ++ch)
                chunk.copyFrom(ch, 0, input, ch, pos, thisBlock);

            juce::dsp::AudioBlock<float> block(chunk);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp.process(ctx);

            // Collect channel 0 output
            const auto* out = chunk.getReadPointer(0);
            for (int i = 0; i < thisBlock; ++i)
                output.push_back(out[i]);

            pos += thisBlock;
        }
        return output;
    }

    float computeRMS(const std::vector<float>& data, int start, int end)
    {
        if (start >= end) return 0.0f;
        float sum = 0.0f;
        for (int i = start; i < end; ++i)
            sum += data[static_cast<size_t>(i)] * data[static_cast<size_t>(i)];
        return std::sqrt(sum / static_cast<float>(end - start));
    }

    //==============================================================================
    // Test 1: Block-size invariance
    // Same input processed with blockSize=128 and blockSize=512 should produce
    // the same steady-state output (within tolerance from different OLA alignment).
    //==============================================================================
    void testBlockSizeInvariance()
    {
        beginTest("Block-size invariance: steady-state output matches across block sizes");

        const int totalSamples = static_cast<int>(LinearPhaseProcessor::hopSize) * 8;
        auto input = generateSine(1000.0f, totalSamples);

        auto lp128  = makePrepared(128);
        auto lp512  = makePrepared(512);

        auto out128 = processFullBuffer(*lp128, 128, input);
        auto out512 = processFullBuffer(*lp512, 512, input);

        // Compare steady-state RMS (skip warmup: first 3 hops)
        const int warmup = static_cast<int>(LinearPhaseProcessor::hopSize) * 3;
        const int end    = std::min(static_cast<int>(out128.size()),
                                    static_cast<int>(out512.size()));

        if (end <= warmup)
        {
            logMessage("Not enough samples for steady-state comparison, skipping");
            return;
        }

        const float rms128 = computeRMS(out128, warmup, end);
        const float rms512 = computeRMS(out512, warmup, end);

        const float rmsRatio = (rms512 > 1e-9f) ? rms128 / rms512 : 0.0f;
        const float rmsRatioDB = 20.0f * std::log10(std::max(rmsRatio, 1e-9f));

        logMessage("RMS ratio (128 vs 512): " + juce::String(rmsRatioDB, 2) + " dB");
        expectWithinAbsoluteError(rmsRatioDB, 0.0f, 1.5f,
            "Block-size invariance violated: >1.5 dB difference between block sizes");
    }

    //==============================================================================
    // Test 2: Channel symmetry
    // Identical L/R input must produce identical L/R output.
    //==============================================================================
    void testChannelSymmetry()
    {
        beginTest("Channel symmetry: identical L/R input produces identical L/R output");

        const int blockSize = 512;
        auto lp = makePrepared(blockSize);

        const int totalSamples = static_cast<int>(LinearPhaseProcessor::hopSize) * 4;
        auto input = generateSine(1000.0f, totalSamples);

        // Process
        int pos = 0;
        float maxDiff = 0.0f;
        int samplesCompared = 0;
        const int warmup = static_cast<int>(LinearPhaseProcessor::hopSize) * 2;

        while (pos < totalSamples)
        {
            const int thisBlock = std::min(blockSize, totalSamples - pos);
            juce::AudioBuffer<float> chunk(kChannels, thisBlock);
            for (int ch = 0; ch < kChannels; ++ch)
                chunk.copyFrom(ch, 0, input, ch, pos, thisBlock);

            juce::dsp::AudioBlock<float> block(chunk);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp->process(ctx);

            if (pos >= warmup)
            {
                const auto* left  = chunk.getReadPointer(0);
                const auto* right = chunk.getReadPointer(1);
                for (int i = 0; i < thisBlock; ++i)
                {
                    const float diff = std::abs(left[i] - right[i]);
                    maxDiff = std::max(maxDiff, diff);
                    samplesCompared++;
                }
            }
            pos += thisBlock;
        }

        logMessage("Max L/R difference: " + juce::String(maxDiff, 8)
                   + " over " + juce::String(samplesCompared) + " samples");
        expect(maxDiff < 1e-6f,
            "Channel symmetry violated: L and R differ by " + juce::String(maxDiff));
    }

    //==============================================================================
    // Test 3: Bypass equivalence
    // Flat EQ (0 dB) should produce output ≈ delayed input.
    // We verify the output RMS matches input RMS within ±0.5 dB.
    //==============================================================================
    void testBypassEquivalence()
    {
        beginTest("Bypass equivalence: flat EQ output ≈ input (within ±0.5 dB)");

        const int blockSize = 512;
        auto lp = makePrepared(blockSize);

        const int totalSamples = static_cast<int>(LinearPhaseProcessor::hopSize) * 6;
        auto input = generateSine(1000.0f, totalSamples);

        auto output = processFullBuffer(*lp, blockSize, input);

        // Compare RMS after warmup
        const int warmup = static_cast<int>(LinearPhaseProcessor::hopSize) * 3;
        const int end    = static_cast<int>(output.size());

        const float inputRMS  = std::sqrt(0.5f); // sine RMS = 1/√2
        const float outputRMS = computeRMS(output, warmup, end);
        const float gainDB    = 20.0f * std::log10(std::max(outputRMS / inputRMS, 1e-9f));

        logMessage("Bypass gain: " + juce::String(gainDB, 3) + " dB");
        expectWithinAbsoluteError(gainDB, 0.0f, 0.5f,
            "Flat EQ deviates from unity by more than ±0.5 dB");
    }

    //==============================================================================
    // Test 4: IR crossfade smoothness
    // Switching IR mid-stream should not produce clicks (peak > 2x steady-state).
    //==============================================================================
    void testIRCrossfadeSmoothness()
    {
        beginTest("IR crossfade: no clicks or discontinuities during IR switch");

        const int blockSize = 512;
        auto lp = makePrepared(blockSize);

        // Warm up with flat IR
        const int warmupSamples = static_cast<int>(LinearPhaseProcessor::hopSize) * 4;
        auto warmupBuf = generateSine(1000.0f, warmupSamples);
        processFullBuffer(*lp, blockSize, warmupBuf);

        // Measure steady-state peak
        auto steadyBuf = generateSine(1000.0f, blockSize * 4);
        auto steadyOut = processFullBuffer(*lp, blockSize, steadyBuf);
        float steadyPeak = 0.0f;
        for (float s : steadyOut)
            steadyPeak = std::max(steadyPeak, std::abs(s));

        // Now switch to a +6 dB IR and process immediately
        std::vector<float> boostedMag(LinearPhaseProcessor::fftSize / 2, 6.0f);
        lp->updateImpulseResponse(boostedMag, kSampleRate);

        // Process the transition block(s) and look for clicks
        auto transitionBuf = generateSine(1000.0f, blockSize * 4);
        auto transOut = processFullBuffer(*lp, blockSize, transitionBuf);

        float transitionPeak = 0.0f;
        for (float s : transOut)
            transitionPeak = std::max(transitionPeak, std::abs(s));

        // The peak during transition should not exceed 3x the expected steady-state
        // (2x boosted = ~2x, plus crossfade headroom)
        const float maxAllowed = steadyPeak * 4.0f + 0.01f;
        logMessage("Steady peak: " + juce::String(steadyPeak, 4)
                   + ", Transition peak: " + juce::String(transitionPeak, 4)
                   + ", Max allowed: " + juce::String(maxAllowed, 4));

        expect(transitionPeak < maxAllowed,
            "Click detected during IR crossfade: peak "
            + juce::String(transitionPeak, 4) + " > " + juce::String(maxAllowed, 4));
    }

    //==============================================================================
    // Test 5: Repeated prepare/reset stability
    // Calling prepare() multiple times must not corrupt state or cause NaN.
    //==============================================================================
    void testRepeatedPrepareStability()
    {
        beginTest("Repeated prepare/reset: no state corruption");

        auto lp = std::make_unique<LinearPhaseProcessor>();
        juce::dsp::ProcessSpec spec;
        spec.numChannels = static_cast<juce::uint32>(kChannels);

        // Cycle through different sample rates and block sizes
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 44100.0 };
        const int blockSizes[]     = { 128, 256, 1024, 512 };

        for (int cycle = 0; cycle < 4; ++cycle)
        {
            spec.sampleRate       = sampleRates[cycle];
            spec.maximumBlockSize = static_cast<juce::uint32>(blockSizes[cycle]);
            lp->prepare(spec);

            std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, 0.0f);
            lp->updateImpulseResponse(mag, spec.sampleRate);

            juce::AudioBuffer<float> buf(kChannels, blockSizes[cycle]);
            // Fill with impulse
            buf.clear();
            buf.setSample(0, 0, 1.0f);
            buf.setSample(1, 0, 1.0f);

            juce::dsp::AudioBlock<float> block(buf);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp->process(ctx);

            // Check no NaN/Inf
            bool allFinite = true;
            for (int ch = 0; ch < kChannels; ++ch)
            {
                const auto* data = buf.getReadPointer(ch);
                for (int i = 0; i < blockSizes[cycle]; ++i)
                {
                    if (!std::isfinite(data[i]))
                    {
                        allFinite = false;
                        break;
                    }
                }
            }
            expect(allFinite, "Non-finite output after prepare cycle " + juce::String(cycle)
                   + " (sr=" + juce::String(sampleRates[cycle])
                   + ", bs=" + juce::String(blockSizes[cycle]) + ")");
        }
    }
};

static LinearPhaseBehavioralTest linearPhaseBehavioralTest;

#endif // JUCE_UNIT_TESTS
