#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DynamicEQProcessor.h"
#include <cmath>

class DynamicEQSidechainBehaviorTest : public juce::UnitTest
{
public:
    DynamicEQSidechainBehaviorTest()
        : juce::UnitTest("DynamicEQSidechainBehavior", "DSP") {}

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
    }
};

static DynamicEQSidechainBehaviorTest dynamicEQSidechainBehaviorTest;

#endif
