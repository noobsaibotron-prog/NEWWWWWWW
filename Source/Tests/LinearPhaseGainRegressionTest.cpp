#if JUCE_UNIT_TESTS

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/LinearPhaseProcessor.h"
#include <cmath>

/**
 * Regression tests for linear-phase IR gain correctness.
 *
 * Bug #3 (hannGainComp = 2.0):
 *   A flat-magnitude IR was producing +6 dB output gain because the Hann
 *   window compensation factor was incorrectly set to 2.0 (phase-vocoder style).
 *   Fix: hannGainComp = 1.0 in both LinearPhaseProcessor and PluginProcessor.
 *
 * These tests ensure:
 *   1. Flat EQ → steady-state gain is within ±1 dB of 0 dB
 *   2. +6 dB shelf → steady-state gain is within ±1.5 dB of +6 dB
 *   3. -6 dB shelf → steady-state gain is within ±1.5 dB of -6 dB
 */
class LinearPhaseGainRegressionTest : public juce::UnitTest
{
public:
    LinearPhaseGainRegressionTest()
        : juce::UnitTest("LinearPhaseGainRegression", "DSP") {}

    void runTest() override
    {
        testFlatEQUnityGain();
        testBoostGain();
        testCutGain();
    }

private:
    static constexpr double kSampleRate  = 44100.0;
    static constexpr int    kBlockSize   = 512;
    static constexpr int    kWarmupHops  = 12;   // let OLA reach steady state
    static constexpr float  kTolerance   = 1.0f; // dB

    // Process kWarmupHops * hopSize samples of a 1 kHz sine through the LP processor
    // and return the RMS of the last block.
    float measureSteadyStateRMS(LinearPhaseProcessor& lp,
                                 float freqHz = 1000.0f,
                                 int numWarmup = kWarmupHops)
    {
        const int hopSize  = static_cast<int>(LinearPhaseProcessor::hopSize);
        const int totalLen = hopSize * (numWarmup + 2);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        float outSumSq = 0.0f;
        int   outCount = 0;

        int samplePos = 0;
        int blocksAfterWarmup = 0;

        while (samplePos < totalLen)
        {
            buf.clear();
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                for (int i = 0; i < kBlockSize; ++i)
                {
                    float t = static_cast<float>(samplePos + i) / static_cast<float>(kSampleRate);
                    data[i] = std::sin(juce::MathConstants<float>::twoPi * freqHz * t);
                }
            }

            juce::dsp::AudioBlock<float> block(buf);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp.process(ctx);

            if (samplePos >= hopSize * numWarmup)
            {
                // Measure only channel 0, skip first and last sample
                auto* out = buf.getReadPointer(0);
                for (int i = 1; i < kBlockSize - 1; ++i)
                    outSumSq += out[i] * out[i];
                outCount += kBlockSize - 2;
                ++blocksAfterWarmup;
                if (blocksAfterWarmup >= 4)
                    break;
            }

            samplePos += kBlockSize;
        }

        return (outCount > 0) ? std::sqrt(outSumSq / static_cast<float>(outCount)) : 0.0f;
    }

    std::unique_ptr<LinearPhaseProcessor> makePrepared()
    {
        auto lp = std::make_unique<LinearPhaseProcessor>();
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = kSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(kBlockSize);
        spec.numChannels      = 2;
        lp->prepare(spec);
        return lp;
    }

    void testFlatEQUnityGain()
    {
        beginTest("Flat EQ (0 dB) produces unity gain in linear phase");

        auto lpPtr = makePrepared();
        auto& lp = *lpPtr;

        // 0 dB magnitude for all bins
        std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, 0.0f);
        lp.updateImpulseResponse(mag, kSampleRate);

        const float inputRMS = std::sqrt(0.5f); // RMS of a full-amplitude sine = 1/sqrt(2)
        const float outputRMS = measureSteadyStateRMS(lp);

        const float gainDB = 20.0f * std::log10(std::max(outputRMS / inputRMS, 1e-9f));

        logMessage("Flat EQ gain: " + juce::String(gainDB, 2) + " dB (expected 0.0 dB, tolerance ±"
                   + juce::String(kTolerance, 1) + " dB)");

        expectWithinAbsoluteError(gainDB, 0.0f, kTolerance,
            "Flat EQ produced non-unity gain (hannGainComp regression?)");
    }

    void testBoostGain()
    {
        beginTest("+6 dB flat boost produces ~+6 dB gain in linear phase");

        auto lpPtr = makePrepared();
        auto& lp = *lpPtr;

        std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, 6.0f); // +6 dB everywhere
        lp.updateImpulseResponse(mag, kSampleRate);

        const float inputRMS  = std::sqrt(0.5f);
        const float outputRMS = measureSteadyStateRMS(lp);
        const float gainDB    = 20.0f * std::log10(std::max(outputRMS / inputRMS, 1e-9f));

        logMessage("+6 dB boost gain: " + juce::String(gainDB, 2) + " dB (expected ~6.0 dB)");
        expectWithinAbsoluteError(gainDB, 6.0f, kTolerance + 0.5f,
            "+6 dB boost produced wrong gain");
    }

    void testCutGain()
    {
        beginTest("-6 dB flat cut produces ~-6 dB gain in linear phase");

        auto lpPtr = makePrepared();
        auto& lp = *lpPtr;

        std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, -6.0f); // -6 dB everywhere
        lp.updateImpulseResponse(mag, kSampleRate);

        const float inputRMS  = std::sqrt(0.5f);
        const float outputRMS = measureSteadyStateRMS(lp);
        const float gainDB    = 20.0f * std::log10(std::max(outputRMS / inputRMS, 1e-9f));

        logMessage("-6 dB cut gain: " + juce::String(gainDB, 2) + " dB (expected ~-6.0 dB)");
        expectWithinAbsoluteError(gainDB, -6.0f, kTolerance + 0.5f,
            "-6 dB cut produced wrong gain");
    }
};

static LinearPhaseGainRegressionTest linearPhaseGainRegressionTest;

#endif // JUCE_UNIT_TESTS
