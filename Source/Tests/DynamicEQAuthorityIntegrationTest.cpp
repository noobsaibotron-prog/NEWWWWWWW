#if JUCE_UNIT_TESTS

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "../DSP/DynamicEQProcessor.h"
#include <algorithm>
#include <cmath>

class DynamicEQAuthorityIntegrationTest final : public juce::UnitTest
{
public:
    DynamicEQAuthorityIntegrationTest()
        : juce::UnitTest("Dynamic EQ authority integration", "AIEQ-DSP") {}

    void runTest() override
    {
        testSingleDetectorTimeConstantControlsAudioGain();
        testLegacyGateEqualsExpandBelow();
        testPeakAndRmsUseLinkedAmplitudeAndPower();
        testSwitchingOffRestoresStaticCurve();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 480;

    static DynamicEQProcessor::DynamicBandParams baseBand()
    {
        DynamicEQProcessor::DynamicBandParams params;
        params.frequency = 1000.0f;
        params.gain = 0.0f;
        params.q = 1.0f;
        params.filterType = 2;
        params.enabled = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        params.triggerSide = DynamicEQProcessor::TriggerSide_Above;
        params.threshold = -20.0f;
        params.ratio = 2.0f;
        params.attackMs = 10.0f;
        params.releaseMs = 100.0f;
        params.range = 48.0f;
        params.knee = 0.0f;
        params.detection = DynamicEQProcessor::DetectionMode_Peak;
        return params;
    }

    static juce::AudioBuffer<float> constantBuffer(int samples, float left, float right)
    {
        juce::AudioBuffer<float> buffer(2, samples);
        for (int channel = 0; channel < 2; ++channel)
            std::fill(buffer.getWritePointer(channel),
                      buffer.getWritePointer(channel) + samples,
                      channel == 0 ? left : right);
        return buffer;
    }

    static float curveDeltaAt1k(const DynamicEQProcessor& processor)
    {
        constexpr float frequency = 1000.0f;
        float delta = 0.0f;
        processor.evaluateDynamicReplacementDeltaDbForFrequencyArray(
            &frequency, &delta, 1, sampleRate);
        return delta;
    }

    void testSingleDetectorTimeConstantControlsAudioGain()
    {
        beginTest("One detector time constant is not squared by gain smoothing");

        DynamicEQProcessor processor;
        processor.prepare(sampleRate, blockSize, 2);
        processor.setBandParams(0, baseBand());

        auto buffer = constantBuffer(blockSize, 1.0f, 1.0f);
        processor.process(buffer);

        const double expectedEnvelope = 1.0 - std::exp(-1.0);
        const double expectedInputDb = 20.0 * std::log10(expectedEnvelope);
        const double expectedDynamicDb = -0.5 * (expectedInputDb + 20.0);
        const auto meter = processor.getBandMeter(0);

        expectWithinAbsoluteError(static_cast<double>(meter.inputLevel),
                                  expectedInputDb, 2.0e-3);
        expectWithinAbsoluteError(static_cast<double>(meter.gainReduction),
                                  expectedDynamicDb, 2.0e-3);

        // Coefficients publish on a bounded control slice, so the audible curve
        // may trail the exact final detector sample by less than one 0.5 dB step.
        // A second attack smoother would instead leave it several dB too shallow.
        const float curveDelta = curveDeltaAt1k(processor);
        expect(curveDelta < -7.0f && curveDelta > -9.0f,
               "Audible dynamic curve does not follow the single detector envelope");
    }

    void testLegacyGateEqualsExpandBelow()
    {
        beginTest("Legacy DynMode=3 is exactly Expand+Below");

        DynamicEQProcessor legacy;
        DynamicEQProcessor canonical;
        legacy.prepare(sampleRate, 64, 2);
        canonical.prepare(sampleRate, 64, 2);

        auto legacyParams = baseBand();
        legacyParams.gain = 12.0f;
        legacyParams.dynamicMode = DynamicEQProcessor::DynamicMode_Gate;
        legacyParams.triggerSide = DynamicEQProcessor::TriggerSide_Above;
        legacyParams.attackMs = 0.0f;
        legacyParams.releaseMs = 0.0f;

        auto canonicalParams = legacyParams;
        canonicalParams.dynamicMode = DynamicEQProcessor::DynamicMode_Expand;
        canonicalParams.triggerSide = DynamicEQProcessor::TriggerSide_Below;

        legacy.setBandParams(0, legacyParams);
        canonical.setBandParams(0, canonicalParams);

        for (int block = 0; block < 8; ++block)
        {
            auto legacyBuffer = constantBuffer(64, 0.01f, -0.01f);
            auto canonicalBuffer = legacyBuffer;
            legacy.process(legacyBuffer);
            canonical.process(canonicalBuffer);

            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 64; ++sample)
                    expectEquals(legacyBuffer.getSample(channel, sample),
                                 canonicalBuffer.getSample(channel, sample));
        }

        expectEquals(legacy.getBandMeter(0).inputLevel,
                     canonical.getBandMeter(0).inputLevel);
        expectEquals(legacy.getBandMeter(0).gainReduction,
                     canonical.getBandMeter(0).gainReduction);
        expectEquals(curveDeltaAt1k(legacy), curveDeltaAt1k(canonical));
    }

    void testPeakAndRmsUseLinkedAmplitudeAndPower()
    {
        beginTest("Stereo-linked Peak and RMS use authority-domain quantities");

        DynamicEQProcessor peak;
        DynamicEQProcessor rms;
        peak.prepare(sampleRate, 1, 2);
        rms.prepare(sampleRate, 1, 2);

        auto peakParams = baseBand();
        peakParams.attackMs = 0.0f;
        peakParams.releaseMs = 0.0f;
        peakParams.detection = DynamicEQProcessor::DetectionMode_Peak;
        auto rmsParams = peakParams;
        rmsParams.detection = DynamicEQProcessor::DetectionMode_RMS;
        peak.setBandParams(0, peakParams);
        rms.setBandParams(0, rmsParams);

        auto peakBuffer = constantBuffer(1, 1.0f, 0.0f);
        auto rmsBuffer = peakBuffer;
        peak.process(peakBuffer);
        rms.process(rmsBuffer);

        expectWithinAbsoluteError(peak.getBandMeter(0).inputLevel, 0.0f, 1.0e-6f);
        expectWithinAbsoluteError(rms.getBandMeter(0).inputLevel,
                                  static_cast<float>(10.0 * std::log10(0.5)),
                                  1.0e-5f);
    }

    void testSwitchingOffRestoresStaticCurve()
    {
        beginTest("Dynamic-to-Off transition restores the static coefficients");

        constexpr int samples = 256;
        DynamicEQProcessor switched;
        DynamicEQProcessor staticReference;
        switched.prepare(sampleRate, samples, 2);
        staticReference.prepare(sampleRate, samples, 2);

        auto dynamicParams = baseBand();
        dynamicParams.gain = 12.0f;
        dynamicParams.attackMs = 0.0f;
        dynamicParams.releaseMs = 0.0f;
        switched.setBandParams(0, dynamicParams);

        auto staticParams = dynamicParams;
        staticParams.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        staticReference.setBandParams(0, staticParams);

        double phase = 0.0;
        const double phaseStep = juce::MathConstants<double>::twoPi * 1000.0 / sampleRate;
        auto makeTone = [&]()
        {
            juce::AudioBuffer<float> buffer(2, samples);
            for (int sample = 0; sample < samples; ++sample)
            {
                const float value = 0.1f * static_cast<float>(std::sin(phase));
                phase += phaseStep;
                buffer.setSample(0, sample, value);
                buffer.setSample(1, sample, value);
            }
            return buffer;
        };

        for (int block = 0; block < 12; ++block)
        {
            auto input = makeTone();
            switched.process(input);
        }

        switched.setBandParams(0, staticParams);
        float switchedRms = 0.0f;
        float referenceRms = 0.0f;
        for (int block = 0; block < 30; ++block)
        {
            auto input = makeTone();
            auto switchedBuffer = input;
            auto referenceBuffer = input;
            switched.process(switchedBuffer);
            staticReference.process(referenceBuffer);
            if (block == 29)
            {
                switchedRms = switchedBuffer.getRMSLevel(0, 0, samples);
                referenceRms = referenceBuffer.getRMSLevel(0, 0, samples);
            }
        }

        const float deltaDb = juce::Decibels::gainToDecibels(
            switchedRms / std::max(referenceRms, 1.0e-12f));
        expectWithinAbsoluteError(deltaDb, 0.0f, 0.1f,
                                  "Switching DynamicMode Off left stale dynamic coefficients");
        expectWithinAbsoluteError(switched.getBandMeter(0).gainReduction,
                                  0.0f, 1.0e-6f);
    }
};

static DynamicEQAuthorityIntegrationTest dynamicEQAuthorityIntegrationTest;

#endif
