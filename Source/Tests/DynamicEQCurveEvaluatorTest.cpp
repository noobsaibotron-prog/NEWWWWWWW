#if JUCE_UNIT_TESTS

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "../DSP/DynamicEQProcessor.h"
#include <array>
#include <cmath>

class DynamicEQCurveEvaluatorTest : public juce::UnitTest
{
public:
    DynamicEQCurveEvaluatorTest()
        : juce::UnitTest("DynamicEQCurveEvaluator", "DSP") {}

    void runTest() override
    {
        testNoDynamicBandsYieldZeroDelta();
        testCompressFullMixProducesNegativeDelta();
        testMixZeroCancelsStaticContribution();
        testMixHalfSitsBetweenDryAndWet();
        testGateProducesStrongAttenuation();
        testExpandProducesPositiveDelta();
        testAutoMakeupAddsBroadbandOffset();
        testDeepNotchStaysFinite();
        testRangeSweepRemainsContinuous();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kChannels = 2;
    static constexpr float kToneFreq = 1000.0f;

    static DynamicEQProcessor::DynamicBandParams makeBaseBand()
    {
        DynamicEQProcessor::DynamicBandParams params;
        params.frequency = kToneFreq;
        params.gain = 12.0f;
        params.q = 1.0f;
        params.filterType = 2; // Peak
        params.enabled = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        params.threshold = -30.0f;
        params.ratio = 8.0f;
        params.attackMs = 1.0f;
        params.releaseMs = 50.0f;
        params.range = 24.0f;
        params.knee = 0.0f;
        return params;
    }

    static std::unique_ptr<DynamicEQProcessor> makePrepared(float mix = 1.0f,
                                                            bool autoMakeup = false)
    {
        auto proc = std::make_unique<DynamicEQProcessor>();
        proc->prepare(kSampleRate, kBlockSize, kChannels);
        proc->setGlobalMix(mix);
        proc->setAutoMakeup(autoMakeup);
        return proc;
    }

    static void driveTone(DynamicEQProcessor& proc,
                          float amplitude,
                          int blocks = 20,
                          float freqHz = kToneFreq)
    {
        double phase = 0.0;
        const double delta = juce::MathConstants<double>::twoPi
            * static_cast<double>(freqHz) / kSampleRate;

        for (int block = 0; block < blocks; ++block)
        {
            juce::AudioBuffer<float> buffer(kChannels, kBlockSize);
            for (int ch = 0; ch < kChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                double localPhase = phase;
                for (int sample = 0; sample < kBlockSize; ++sample)
                {
                    data[sample] = amplitude * std::sin(localPhase);
                    localPhase += delta;
                }
            }

            phase += delta * static_cast<double>(kBlockSize);
            proc.process(buffer);
        }
    }

    static float evaluateDeltaAt(const DynamicEQProcessor& proc, float freqHz)
    {
        float deltaDb = 0.0f;
        proc.evaluateDynamicReplacementDeltaDbForFrequencyArray(
            &freqHz, &deltaDb, 1, kSampleRate);
        return deltaDb;
    }

    static float staticMagnitudeDbAt(const DynamicEQProcessor& proc, float freqHz)
    {
        const float magnitude = proc.getMagnitudeForFrequency(freqHz, kSampleRate);
        return juce::Decibels::gainToDecibels(magnitude, -120.0f);
    }

    static float expectedAutoMakeupDb(const DynamicEQProcessor& proc)
    {
        float totalGainLinear = 1.0f;
        for (int band = 0; band < DynamicEQProcessor::maxBands; ++band)
        {
            const auto params = proc.getBandParams(band);
            if (!params.enabled || params.dynamicMode == DynamicEQProcessor::DynamicMode_Off)
                continue;

            totalGainLinear *= juce::Decibels::decibelsToGain(
                proc.getBandMeter(band).gainReduction);
        }

        if (totalGainLinear < 0.999f)
        {
            const float makeupGain = juce::jlimit(0.25f, 4.0f, 1.0f / totalGainLinear);
            return juce::Decibels::gainToDecibels(makeupGain, -120.0f);
        }

        return 0.0f;
    }

    static bool isFinite(float value)
    {
        return std::isfinite(static_cast<double>(value));
    }

    void testNoDynamicBandsYieldZeroDelta()
    {
        beginTest("No dynamic-owned bands yield zero replacement delta");

        auto proc = makePrepared();
        auto params = makeBaseBand();
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        proc->setBandParams(0, params);

        const std::array<float, 3> freqs { 100.0f, kToneFreq, 6000.0f };
        std::array<float, freqs.size()> deltaDb {};
        proc->evaluateDynamicReplacementDeltaDbForFrequencyArray(
            freqs.data(), deltaDb.data(), deltaDb.size(), kSampleRate);

        for (size_t i = 0; i < deltaDb.size(); ++i)
            expectWithinAbsoluteError(deltaDb[i], 0.0f, 1.0e-4f, "Expected zero delta");
    }

    void testCompressFullMixProducesNegativeDelta()
    {
        beginTest("Compress, mix=1, above threshold produces negative delta near band");

        auto proc = makePrepared(1.0f, false);
        proc->setBandParams(0, makeBaseBand());
        driveTone(*proc, 0.5f);

        const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
        logMessage("compress delta @1k = " + juce::String(deltaDb, 3) + " dB");
        expect(deltaDb < -2.0f, "Expected a clearly negative replacement delta");
    }

    void testMixZeroCancelsStaticContribution()
    {
        beginTest("Mix 0 cancels the static contribution of dynamic-owned bands");

        auto proc = makePrepared(0.0f, false);
        proc->setBandParams(0, makeBaseBand());
        driveTone(*proc, 0.5f);

        const float staticDb = staticMagnitudeDbAt(*proc, kToneFreq);
        const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
        logMessage("static @1k = " + juce::String(staticDb, 3)
                   + " dB, delta = " + juce::String(deltaDb, 3) + " dB");
        expectWithinAbsoluteError(deltaDb, -staticDb, 0.75f,
                                  "Mix 0 should cancel the displayed static curve");
    }

    void testMixHalfSitsBetweenDryAndWet()
    {
        beginTest("Mix 0.5 sits between dry and fully wet live curves");

        auto proc0 = makePrepared(0.0f, false);
        auto proc50 = makePrepared(0.5f, false);
        auto proc100 = makePrepared(1.0f, false);

        const auto params = makeBaseBand();
        proc0->setBandParams(0, params);
        proc50->setBandParams(0, params);
        proc100->setBandParams(0, params);

        driveTone(*proc0, 0.5f);
        driveTone(*proc50, 0.5f);
        driveTone(*proc100, 0.5f);

        const float staticDb = staticMagnitudeDbAt(*proc100, kToneFreq);
        const float live0 = staticDb + evaluateDeltaAt(*proc0, kToneFreq);
        const float live50 = staticDb + evaluateDeltaAt(*proc50, kToneFreq);
        const float live100 = staticDb + evaluateDeltaAt(*proc100, kToneFreq);

        logMessage("live0=" + juce::String(live0, 3)
                   + " live50=" + juce::String(live50, 3)
                   + " live100=" + juce::String(live100, 3));

        const float lo = std::min(live0, live100);
        const float hi = std::max(live0, live100);
        expect(live50 > lo + 0.2f && live50 < hi - 0.2f,
               "Mix 0.5 should land between dry and fully wet live responses");
    }

    void testGateProducesStrongAttenuation()
    {
        beginTest("Gate mode produces strong attenuation when well below threshold");

        auto proc = makePrepared(1.0f, false);
        auto params = makeBaseBand();
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Gate;
        params.threshold = -20.0f;
        params.ratio = 4.0f;
        params.range = 24.0f;
        proc->setBandParams(0, params);
        driveTone(*proc, 0.001f);

        const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
        logMessage("gate delta @1k = " + juce::String(deltaDb, 3) + " dB");
        expect(deltaDb < -20.0f, "Expected deep attenuation from a closed gate");
    }

    void testExpandProducesPositiveDelta()
    {
        beginTest("Expand mode produces positive divergence near the band");

        auto proc = makePrepared(1.0f, false);
        auto params = makeBaseBand();
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Expand;
        params.ratio = 4.0f;
        proc->setBandParams(0, params);
        driveTone(*proc, 0.5f);

        const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
        logMessage("expand delta @1k = " + juce::String(deltaDb, 3) + " dB");
        expect(deltaDb > 0.5f, "Expected positive replacement delta in expand mode");
    }

    void testAutoMakeupAddsBroadbandOffset()
    {
        beginTest("Auto makeup adds the same broadband offset as the audio path");

        auto procOff = makePrepared(1.0f, false);
        auto procOn = makePrepared(1.0f, true);
        const auto params = makeBaseBand();
        procOff->setBandParams(0, params);
        procOn->setBandParams(0, params);
        driveTone(*procOff, 0.5f);
        driveTone(*procOn, 0.5f);

        const float expectedDb = expectedAutoMakeupDb(*procOn);
        const float lowDiff = evaluateDeltaAt(*procOn, 100.0f) - evaluateDeltaAt(*procOff, 100.0f);
        const float highDiff = evaluateDeltaAt(*procOn, 6000.0f) - evaluateDeltaAt(*procOff, 6000.0f);

        logMessage("expected makeup=" + juce::String(expectedDb, 3)
                   + " lowDiff=" + juce::String(lowDiff, 3)
                   + " highDiff=" + juce::String(highDiff, 3));

        expectWithinAbsoluteError(lowDiff, expectedDb, 0.6f,
                                  "Low-frequency offset should match auto makeup");
        expectWithinAbsoluteError(highDiff, expectedDb, 0.6f,
                                  "High-frequency offset should match auto makeup");
        expectWithinAbsoluteError(lowDiff, highDiff, 0.25f,
                                  "Auto makeup should behave as a broadband offset");
    }

    void testDeepNotchStaysFinite()
    {
        beginTest("Deep notch edge case stays finite");

        auto proc = makePrepared(0.0f, false);
        auto params = makeBaseBand();
        params.filterType = 5; // Notch
        params.q = 12.0f;
        proc->setBandParams(0, params);
        driveTone(*proc, 0.5f);

        const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
        logMessage("deep notch delta @1k = " + juce::String(deltaDb, 3) + " dB");
        expect(isFinite(deltaDb), "Evaluator must not emit inf/nan on deep notches");
    }

    void testRangeSweepRemainsContinuous()
    {
        beginTest("Range sweep above threshold remains continuous");

        std::array<float, 6> ranges { 6.0f, 9.0f, 12.0f, 15.0f, 18.0f, 24.0f };
        float previousDelta = 0.0f;
        bool hasPrevious = false;

        for (float range : ranges)
        {
            auto proc = makePrepared(1.0f, false);
            auto params = makeBaseBand();
            params.range = range;
            proc->setBandParams(0, params);
            driveTone(*proc, 0.5f);

            const float deltaDb = evaluateDeltaAt(*proc, kToneFreq);
            logMessage("range=" + juce::String(range, 1)
                       + " delta=" + juce::String(deltaDb, 3));

            expect(isFinite(deltaDb), "Sweep must remain finite");
            if (hasPrevious)
                expect(std::abs(deltaDb - previousDelta) < 8.0f,
                       "Adjacent range values should not introduce staircase jumps");

            previousDelta = deltaDb;
            hasPrevious = true;
        }
    }
};

static DynamicEQCurveEvaluatorTest dynamicEQCurveEvaluatorTest;

#endif
