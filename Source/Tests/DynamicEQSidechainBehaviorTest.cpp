#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DynamicEQProcessor.h"
#include <cmath>

class DynamicEQSidechainBehaviorTest : public juce::UnitTest
{
public:
    DynamicEQSidechainBehaviorTest()
        : juce::UnitTest("DynamicEQSidechainBehavior", "AIEQ-DSP") {}

    void runTest() override
    {
        beginTest("Sidechain tuning changes gain reduction emphasis");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr int channels = 2;
        constexpr float targetFreq = 1000.0f;
        constexpr float distractorFreq = 8000.0f; // far from target for clear separation

        DynamicEQProcessor proc;
        proc.prepare(sampleRate, blockSize, channels);

        DynamicEQProcessor::DynamicBandParams params;
        params.frequency = targetFreq;
        params.gain = 12.0f;
        params.q = 3.0f;
        params.filterType = 2; // peak
        params.enabled = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        params.threshold = -22.0f;
        params.ratio = 8.0f;
        params.attackMs = 1.0f;
        params.releaseMs = 80.0f;
        params.range = 18.0f;
        params.knee = 0.0f;
        params.sidechainEnabled = true;
        params.sidechainQ = 6.0f;
        proc.setBandParams(0, params);

        // Input signal: ONLY at targetFreq.  No energy at distractorFreq.
        // This means a sidechain tuned to distractorFreq should see very little
        // energy and therefore apply less gain reduction than one tuned to targetFreq.
        auto makePureTone = [&](float freq, float amplitude)
        {
            juce::AudioBuffer<float> buf(channels, blockSize);
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
                    data[i] = amplitude * std::sin(juce::MathConstants<float>::twoPi * freq * t);
                }
            }
            return buf;
        };

        auto warmupForSidechain = [&](float sidechainFreq)
        {
            auto p = proc.getBandParams(0);
            p.sidechainFreq = sidechainFreq;
            proc.setBandParams(0, p);
            proc.reset();
            for (int i = 0; i < 10; ++i)
            {
                auto warm = makePureTone(targetFreq, 0.7f);
                proc.process(warm);
            }
            auto probe = makePureTone(targetFreq, 0.7f);
            proc.process(probe);
            return proc.getBandMeter(0).gainReduction;
        };

        const float grMatched = warmupForSidechain(targetFreq);
        const float grMismatched = warmupForSidechain(distractorFreq);

        logMessage("Matched sidechain GR: " + juce::String(grMatched, 2) + " dB");
        logMessage("Mismatched sidechain GR: " + juce::String(grMismatched, 2) + " dB");

        // Basic sanity: matched sidechain should produce meaningful GR
        expect(grMatched < -0.5f,
               "Matched sidechain should produce real gain reduction");

        // Core contract: matched sidechain sees more energy -> more compression.
        // With a pure tone only at targetFreq, the mismatched sidechain bandpass
        // (centred far away) should pass much less energy to the detector.
        expect(grMatched < grMismatched - 0.5f,
               "Sidechain tuned to the signal frequency should compress more than a far-off detector"
               " (matched=" + juce::String(grMatched, 2)
               + " mismatched=" + juce::String(grMismatched, 2) + ")");

        beginTest("External detector is independent from the main program");
        {
            DynamicEQProcessor externalProc;
            DynamicEQProcessor internalProc;
            externalProc.prepare(sampleRate, blockSize, channels);
            internalProc.prepare(sampleRate, blockSize, channels);

            auto p = params;
            p.sidechainEnabled = false;
            p.detectorSource = DynamicEQProcessor::DetectorSource_ExternalWideband;
            externalProc.setBandParams(0, p);
            p.detectorSource = DynamicEQProcessor::DetectorSource_InternalWideband;
            internalProc.setBandParams(0, p);

            juce::AudioBuffer<float> detector(channels, blockSize);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    detector.setSample(ch, i, 0.8f * std::sin(
                        juce::MathConstants<float>::twoPi * targetFreq
                        * static_cast<float>(i) / static_cast<float>(sampleRate)));

            for (int block = 0; block < 20; ++block)
            {
                juce::AudioBuffer<float> silentExternal(channels, blockSize);
                juce::AudioBuffer<float> silentInternal(channels, blockSize);
                silentExternal.clear();
                silentInternal.clear();
                externalProc.process(silentExternal, &detector);
                internalProc.process(silentInternal, &detector);
            }

            const float externalGR = externalProc.getBandMeter(0).gainReduction;
            const float internalGR = internalProc.getBandMeter(0).gainReduction;
            expect(externalGR < -0.5f,
                   "A present external detector must drive compression over silent main audio");
            expect(std::abs(internalGR) < 0.05f,
                   "Internal detector must ignore an external block that was not selected");
            expect(externalProc.getDetectorAvailability(0)
                       == DynamicEQProcessor::DetectorAvailability::ExternalAvailable);
            expect(internalProc.getDetectorAvailability(0)
                       == DynamicEQProcessor::DetectorAvailability::Internal);
        }

        beginTest("Absent external bus is unavailable, not a silent Below trigger");
        {
            DynamicEQProcessor externalProc;
            externalProc.prepare(sampleRate, blockSize, channels);

            auto p = params;
            p.dynamicMode = DynamicEQProcessor::DynamicMode_Expand;
            p.triggerSide = DynamicEQProcessor::TriggerSide_Below;
            p.detectorSource = DynamicEQProcessor::DetectorSource_ExternalWideband;
            p.sidechainEnabled = false;
            p.releaseMs = 10.0f;
            externalProc.setBandParams(0, p);

            juce::AudioBuffer<float> silentDetector(1, blockSize);
            silentDetector.clear();
            for (int block = 0; block < 20; ++block)
            {
                auto main = makePureTone(targetFreq, 0.2f);
                externalProc.process(main, &silentDetector);
            }

            const float activeBelowGR = externalProc.getBandMeter(0).gainReduction;
            expect(activeBelowGR < -0.5f,
                   "A present silent bus must remain a valid Below detector");

            for (int block = 0; block < 20; ++block)
            {
                auto main = makePureTone(targetFreq, 0.2f);
                externalProc.process(main, nullptr);
            }

            const float unavailableGR = externalProc.getBandMeter(0).gainReduction;
            expect(std::abs(unavailableGR) < std::abs(activeBelowGR),
                   "Removing the bus must release control toward neutral");
            expect(std::abs(unavailableGR) < 0.05f,
                   "Absent bus must not keep a Below expander engaged");
            expect(externalProc.getDetectorAvailability(0)
                       == DynamicEQProcessor::DetectorAvailability::ExternalUnavailable);

            juce::AudioBuffer<float> undersizedDetector(1, blockSize / 2);
            undersizedDetector.clear();
            auto main = makePureTone(targetFreq, 0.2f);
            externalProc.process(main, &undersizedDetector);
            expect(externalProc.getDetectorAvailability(0)
                       == DynamicEQProcessor::DetectorAvailability::ExternalUnavailable,
                   "An undersized detector block must fail unavailable without an out-of-bounds read");
            for (int ch = 0; ch < main.getNumChannels(); ++ch)
                for (int i = 0; i < main.getNumSamples(); ++i)
                    expect(std::isfinite(main.getSample(ch, i)),
                           "Unavailable detector path produced non-finite audio");
        }
    }
};

static DynamicEQSidechainBehaviorTest dynamicEQSidechainBehaviorTest;

#endif
