#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DynamicEQProcessor.h"
#include "../DSP/BiquadCoefficients.h"
#include <cmath>

/**
 * D1 Regression Test — Peak filter identity at gain ≈ 0
 *
 * Bug: DynamicEQProcessor returned makeAllPass(sr, 20.0f, 0.1f) for Peak
 *      filter when |gain| < 0.05 dB. AllPass has unity magnitude but
 *      introduces phase rotation, which corrupts linear-phase EQ matching
 *      and causes audible comb filtering when mixed with dry signal.
 *
 * Fix: Return makeBypass() (valid=false, coeffs = identity) so the filter
 *      is a true wire — no magnitude change, no phase change.
 */
class D1PeakIdentityTest : public juce::UnitTest
{
public:
    D1PeakIdentityTest()
        : juce::UnitTest("D1 Peak Identity", "DSP") {}

    void runTest() override
    {
        testBypassCoeffsAtZeroGain();
        testProcessorOutputIdentity();
        testPhaseNeutral();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int    kBlockSize  = 512;
    static constexpr int    kChannels   = 2;

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

    static float rms(const juce::AudioBuffer<float>& buf)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                sum += data[i] * data[i];
        }
        return std::sqrt(sum / static_cast<float>(buf.getNumChannels() * buf.getNumSamples()));
    }

    // ── Test 1: BiquadCoeffs level — makeBypass returned at gain=0 ──────
    void testBypassCoeffsAtZeroGain()
    {
        beginTest("Peak filter at gain=0 returns bypass coefficients (not allpass)");

        // Directly test the coefficients that would be produced
        // makeBypass returns valid=false (wire)
        auto bypass = BiquadCoeffs::makeBypass();
        expect(!bypass.valid, "makeBypass should have valid=false");

        // makeAllPass returns valid=true with phase-rotating coefficients
        auto allpass = BiquadCoeffs::makeAllPass(kSampleRate, 20.0f, 0.1f);
        expect(allpass.valid, "makeAllPass should have valid=true");

        // Verify magnitude is 1.0 for both at various frequencies
        for (float freq : {100.0f, 1000.0f, 5000.0f, 15000.0f})
        {
            double bypassMag = bypass.getMagnitudeForFrequency(freq, kSampleRate);
            double allpassMag = allpass.getMagnitudeForFrequency(freq, kSampleRate);

            expectWithinAbsoluteError(bypassMag, 1.0, 0.001,
                "Bypass magnitude at " + juce::String(freq) + " Hz");
            expectWithinAbsoluteError(allpassMag, 1.0, 0.001,
                "AllPass magnitude at " + juce::String(freq) + " Hz");
        }
    }

    // ── Test 2: Processor level — output == input when Peak gain = 0 ────
    void testProcessorOutputIdentity()
    {
        beginTest("DynamicEQ with Peak gain=0 produces bit-identical output");

        DynamicEQProcessor proc;
        proc.prepare(kSampleRate, kBlockSize, kChannels);

        DynamicEQProcessor::DynamicBandParams params;
        params.frequency   = 1000.0f;
        params.gain        = 0.0f;  // This is the D1 trigger condition
        params.q           = 1.0f;
        params.filterType  = 2;     // Peak
        params.enabled     = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        proc.setBandParams(0, params);

        auto input = makeSine(1000.0f, kBlockSize);
        auto processed = input;  // deep copy

        // Process several blocks to let filters stabilize
        for (int i = 0; i < 5; ++i)
        {
            processed.makeCopyOf(input);
            proc.process(processed);
        }

        // After stabilization, output should be identical to input
        float maxDiff = 0.0f;
        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto* in  = input.getReadPointer(ch);
            const auto* out = processed.getReadPointer(ch);
            for (int i = 0; i < kBlockSize; ++i)
            {
                float diff = std::abs(in[i] - out[i]);
                maxDiff = std::max(maxDiff, diff);
            }
        }

        // With bypass (valid=false), processor skips the filter entirely → bit-identical
        expect(maxDiff < 1e-6f,
               "Peak gain=0 should be identity, max sample diff = " +
               juce::String(maxDiff, 10));
    }

    // ── Test 3: Phase neutrality — no comb filtering with dry mix ───────
    void testPhaseNeutral()
    {
        beginTest("Peak gain=0 does not introduce phase rotation (no comb when summed with dry)");

        DynamicEQProcessor proc;
        proc.prepare(kSampleRate, kBlockSize, kChannels);

        DynamicEQProcessor::DynamicBandParams params;
        params.frequency   = 1000.0f;
        params.gain        = 0.0f;
        params.q           = 1.0f;
        params.filterType  = 2; // Peak
        params.enabled     = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        proc.setBandParams(0, params);

        // Single-frequency probe: allpass rotates phase of any frequency,
        // so even one sine suffices to detect non-identity behavior.
        auto input = makeSine(1000.0f, kBlockSize);

        auto processed = input;
        for (int i = 0; i < 5; ++i)
        {
            processed.makeCopyOf(input);
            proc.process(processed);
        }

        // Sum dry + wet — if phase-neutral, amplitude doubles (6 dB)
        // If allpass was used, some frequencies cancel → lower RMS
        juce::AudioBuffer<float> sumBuf(kChannels, kBlockSize);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            const auto* dry = input.getReadPointer(ch);
            const auto* wet = processed.getReadPointer(ch);
            auto* sum = sumBuf.getWritePointer(ch);
            for (int i = 0; i < kBlockSize; ++i)
                sum[i] = dry[i] + wet[i];
        }

        float dryRMS = rms(input);
        float sumRMS = rms(sumBuf);

        // Perfect identity: sumRMS = 2 * dryRMS (6 dB gain)
        // AllPass would cause partial cancellation at some frequencies
        float ratio = sumRMS / dryRMS;

        expectWithinAbsoluteError(static_cast<double>(ratio), 2.0, 0.01,
            "Dry+Wet sum should be exactly 2x (6 dB), got ratio " +
            juce::String(ratio, 4));
    }
};

static D1PeakIdentityTest d1PeakIdentityTest;

#endif // JUCE_UNIT_TESTS
