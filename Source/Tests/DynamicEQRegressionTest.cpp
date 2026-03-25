#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DynamicEQProcessor.h"
#include <cmath>

/**
 * Regression tests for DynamicEQProcessor.
 *
 * Bug F: detector used eqFiltersL[1] as a second cascade stage when
 *        sidechain was disabled, distorting the gain-reduction curve.
 *        Fix: use raw detect signal when sidechain is disabled.
 *
 * Bug G: dryBuffer not grown when host sends larger blocks, causing
 *        partial silence in the dry/wet tail.
 *        Fix: grow dryBuffer on-demand before computing safeSamples.
 */
class DynamicEQRegressionTest : public juce::UnitTest
{
public:
    DynamicEQRegressionTest()
        : juce::UnitTest("DynamicEQRegression", "DSP") {}

    void runTest() override
    {
        testDynamicDetectionNoSidechain();
        testDryWetOversizedBlock();
        testStaticProcessingClean();
    }

private:
    static constexpr double kSampleRate  = 48000.0;
    static constexpr int    kBlockSize   = 512;
    static constexpr int    kChannels    = 2;

    juce::AudioBuffer<float> makeSine(float freq, int numSamples)
    {
        juce::AudioBuffer<float> buf(kChannels, numSamples);
        const float w = juce::MathConstants<float>::twoPi * freq / static_cast<float>(kSampleRate);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::sin(w * static_cast<float>(i));
        }
        return buf;
    }

    bool isFinite(const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (!std::isfinite(data[i]))
                    return false;
        }
        return true;
    }

    float rms(const juce::AudioBuffer<float>& buf, int ch = 0)
    {
        return buf.getRMSLevel(ch, 0, buf.getNumSamples());
    }

    // Bug F regression: dynamic compression without sidechain must apply
    // gain reduction proportional to input level, not to a cascaded-filtered signal.
    void testDynamicDetectionNoSidechain()
    {
        beginTest("Dynamic compression (no sidechain) reduces gain on loud signal");

        DynamicEQProcessor proc;
        proc.prepare(kSampleRate, kBlockSize, kChannels);

        DynamicEQProcessor::DynamicBandParams params;
        params.frequency   = 1000.0f;
        params.gain        = 0.0f;
        params.q           = 1.0f;
        params.filterType  = 2; // Peak
        params.enabled     = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        params.threshold   = -20.0f;
        params.ratio       = 10.0f;
        params.attackMs    = 1.0f;
        params.releaseMs   = 50.0f;
        params.range       = 24.0f;
        params.knee        = 0.0f;
        params.sidechainEnabled = false;
        proc.setBandParams(0, params);

        // Warm up: 8 blocks of loud signal to let envelope settle
        for (int i = 0; i < 8; ++i)
        {
            auto buf = makeSine(1000.0f, kBlockSize);
            proc.process(buf);
        }

        // Measure gain reduction: output should be quieter than input when above threshold
        auto inputBuf  = makeSine(1000.0f, kBlockSize);
        const float inputRMS = rms(inputBuf);
        proc.process(inputBuf);
        const float outputRMS = rms(inputBuf);

        expect(isFinite(inputBuf), "Non-finite samples in dynamic compress output");
        // Signal is above threshold → compressor must have reduced gain
        expect(outputRMS < inputRMS * 0.99f,
               "Dynamic compressor (no sidechain) did not reduce gain above threshold");

        const float grDB = proc.getBandMeter(0).gainReduction;
        logMessage("GR meter: " + juce::String(grDB, 2) + " dB");
        expect(grDB < 0.0f, "Gain reduction meter should be negative when compressing");
    }

    // Bug G regression: dry/wet mix must work correctly when the block is
    // larger than the size used in prepare().
    void testDryWetOversizedBlock()
    {
        beginTest("Dry/wet mix survives oversized block (Bug G regression)");

        const int preparedBlock  = 256;
        const int oversizedBlock = preparedBlock * 8; // deliberately larger

        DynamicEQProcessor proc;
        proc.prepare(kSampleRate, preparedBlock, kChannels);

        // Set global mix to 50%
        proc.setGlobalMix(0.5f);

        // Build a constant-value buffer (DC) so we can predict dry output
        juce::AudioBuffer<float> buf(kChannels, oversizedBlock);
        for (int ch = 0; ch < kChannels; ++ch)
            buf.clear(ch, 0, oversizedBlock);

        // Fill with a known amplitude so dry/wet blend is detectable
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < oversizedBlock; ++i)
                data[i] = 0.5f;
        }

        proc.process(buf);

        expect(isFinite(buf), "Non-finite samples in dry/wet oversized block");

        // With 50% mix and static bands at 0 dB gain, output ≈ 0.5 throughout.
        // The tail (beyond original dryBuffer capacity) must NOT be zeroed.
        const int tailStart = preparedBlock * 4;
        float tailMax = 0.0f;
        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = tailStart; i < oversizedBlock; ++i)
                tailMax = std::max(tailMax, std::abs(data[i]));
        }

        expect(tailMax > 1e-4f,
               "Tail of oversized block is silent - dry/wet blend was not applied (Bug G)");
        logMessage("Tail max amplitude: " + juce::String(tailMax, 4));
    }

    // Sanity: static processing (no dynamic mode) must not distort signal
    void testStaticProcessingClean()
    {
        beginTest("Static EQ processing produces finite output");

        DynamicEQProcessor proc;
        proc.prepare(kSampleRate, kBlockSize, kChannels);

        DynamicEQProcessor::DynamicBandParams params;
        params.frequency   = 1000.0f;
        params.gain        = 6.0f;
        params.q           = 1.0f;
        params.filterType  = 2; // Peak
        params.enabled     = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        proc.setBandParams(0, params);

        auto buf = makeSine(1000.0f, kBlockSize);
        proc.process(buf);

        expect(isFinite(buf), "Non-finite samples in static EQ output");
        expect(rms(buf) > 0.0f, "Static EQ output is silent");
    }
};

static DynamicEQRegressionTest dynamicEQRegressionTest;

#endif // JUCE_UNIT_TESTS
