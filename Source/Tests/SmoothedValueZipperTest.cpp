#if JUCE_UNIT_TESTS

#include <juce_dsp/juce_dsp.h>
#include "DSP/BoundedSlewLimiter.h"
#include <array>
#include <cmath>
#include <limits>

/** Simple continuity test for juce::SmoothedValue to spot zippering artifacts.
    It simulates a fast automation ramp and checks that successive samples
    are not jumping by an excessive delta.
*/
class SmoothedValueZipperTest : public juce::UnitTest
{
public:
    SmoothedValueZipperTest() : juce::UnitTest("SmoothedValueZipperTest", "AIEQ-DSP") {}

    void runTest() override
    {
        testLegacyJuceSmoothingContinuity();
        testLog2SlewLimits();
        testLinearGainSlewLimit();
        testPartitionInvariantEndpoints();
        testInvalidTargetsAreRejected();
    }

private:
    void testLegacyJuceSmoothingContinuity()
    {
        beginTest("Legacy JUCE smoothing continuity");

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smooth;
        const double sampleRate = 48000.0;
        smooth.reset(sampleRate, 0.01); // 10 ms ramp
        smooth.setCurrentAndTargetValue(0.0f);
        smooth.setTargetValue(1.0f);

        const int totalSamples = static_cast<int>(sampleRate * 0.02); // 20 ms window
        float prev = smooth.getNextValue(); // first sample
        float maxDelta = 0.0f;

        for (int n = 1; n < totalSamples; ++n)
        {
            float v = smooth.getNextValue();
            maxDelta = juce::jmax(maxDelta, std::abs(v - prev));
            prev = v;
        }

        // In a 10 ms linear ramp from 0 -> 1, per-sample step at 48kHz is ~0.0021
        expect(maxDelta < 0.01f, "Detected large step - potential zippering");
    }

    void testLog2SlewLimits()
    {
        beginTest("Frequency and Q are bounded in log2 units per elapsed sample");

        for (const double sampleRate : { 32000.0, 48000.0, 192000.0 })
        {
            for (const int blockSize : { 1, 7, 64, 511, 2048 })
            {
                AIEQDSP::Log2SlewLimiter frequency;
                frequency.prepare(sampleRate, 100.0);
                frequency.setCurrentAndTargetValue(20.0);
                frequency.setTargetValue(20000.0);

                const double before = frequency.getCurrentLog2();
                frequency.skip(blockSize);
                const double moved = frequency.getCurrentLog2() - before;
                const double allowed = 100.0 * static_cast<double>(blockSize) / sampleRate;
                expect(moved <= allowed + 2.0e-15);
                expect(moved >= 0.0);

                AIEQDSP::Log2SlewLimiter q;
                q.prepare(sampleRate, 100.0);
                q.setCurrentAndTargetValue(10.0);
                q.setTargetValue(0.1);
                const double qBefore = q.getCurrentLog2();
                q.skip(blockSize);
                const double qMoved = qBefore - q.getCurrentLog2();
                expect(qMoved <= allowed + 2.0e-15);
                expect(qMoved >= 0.0);
            }
        }
    }

    void testLinearGainSlewLimit()
    {
        beginTest("Gain is bounded at 2400 dB per second without overshoot");

        AIEQDSP::LinearSlewLimiter gain;
        gain.prepare(48000.0, 2400.0);
        gain.setCurrentAndTargetValue(-24.0);
        gain.setTargetValue(24.0);
        gain.skip(128);
        expectWithinAbsoluteError(gain.getCurrentValueDouble(), -17.6, 2.0e-13);

        gain.skip(48000);
        expectWithinAbsoluteError(gain.getCurrentValueDouble(), 24.0, 0.0);
        expect(!gain.isSmoothing());
    }

    void testPartitionInvariantEndpoints()
    {
        beginTest("Slew endpoint is invariant to block partitioning");

        AIEQDSP::Log2SlewLimiter oneBlock;
        AIEQDSP::Log2SlewLimiter splitBlocks;
        oneBlock.prepare(48000.0, 100.0);
        splitBlocks.prepare(48000.0, 100.0);
        oneBlock.setCurrentAndTargetValue(40.0);
        splitBlocks.setCurrentAndTargetValue(40.0);
        oneBlock.setTargetValue(16000.0);
        splitBlocks.setTargetValue(16000.0);

        oneBlock.skip(1000);
        for (const int block : { 1, 7, 32, 64, 128, 256, 512 })
            splitBlocks.skip(block);
        expectWithinAbsoluteError(oneBlock.getCurrentLog2(),
                                  splitBlocks.getCurrentLog2(), 2.0e-14);

        AIEQDSP::LinearSlewLimiter gainOne;
        AIEQDSP::LinearSlewLimiter gainSplit;
        gainOne.prepare(48000.0, 2400.0);
        gainSplit.prepare(48000.0, 2400.0);
        gainOne.setCurrentAndTargetValue(-24.0);
        gainSplit.setCurrentAndTargetValue(-24.0);
        gainOne.setTargetValue(24.0);
        gainSplit.setTargetValue(24.0);
        gainOne.skip(1000);
        for (const int block : { 1, 7, 32, 64, 128, 256, 512 })
            gainSplit.skip(block);
        expectWithinAbsoluteError(gainOne.getCurrentValueDouble(),
                                  gainSplit.getCurrentValueDouble(), 2.0e-14);
    }

    void testInvalidTargetsAreRejected()
    {
        beginTest("Non-finite and non-positive targets cannot poison slew state");

        AIEQDSP::Log2SlewLimiter frequency;
        frequency.prepare(48000.0, 100.0);
        frequency.setCurrentAndTargetValue(1000.0);
        frequency.setTargetValue(0.0);
        frequency.setTargetValue(std::numeric_limits<double>::infinity());
        frequency.skip(48000);
        expectWithinAbsoluteError(frequency.getCurrentValue(), 1000.0f, 0.0f);

        AIEQDSP::LinearSlewLimiter gain;
        gain.prepare(48000.0, 2400.0);
        gain.setCurrentAndTargetValue(3.0);
        gain.setTargetValue(std::numeric_limits<double>::quiet_NaN());
        gain.skip(48000);
        expectWithinAbsoluteError(gain.getCurrentValue(), 3.0f, 0.0f);
    }
};

static SmoothedValueZipperTest smoothedValueZipperTest;

#endif // JUCE_UNIT_TESTS
