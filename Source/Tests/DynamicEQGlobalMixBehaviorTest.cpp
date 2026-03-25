#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/DynamicEQProcessor.h"
#include <cmath>

class DynamicEQGlobalMixBehaviorTest : public juce::UnitTest
{
public:
    DynamicEQGlobalMixBehaviorTest()
        : juce::UnitTest("DynamicEQGlobalMixBehavior", "DSP") {}

    void runTest() override
    {
        beginTest("Global mix interpolates between dry and wet paths");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr int channels = 2;
        constexpr float toneFreq = 1000.0f;

        auto makeTone = [&]()
        {
            juce::AudioBuffer<float> buf(channels, blockSize);
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
                    data[i] = 0.25f * std::sin(juce::MathConstants<float>::twoPi * toneFreq * t);
                }
            }
            return buf;
        };

        auto processWithMix = [&](float mix)
        {
            DynamicEQProcessor proc;
            proc.prepare(sampleRate, blockSize, channels);
            proc.setGlobalMix(mix);

            DynamicEQProcessor::DynamicBandParams params;
            params.frequency = toneFreq;
            params.gain = 12.0f;
            params.q = 1.0f;
            params.filterType = 2; // peak
            params.enabled = true;
            params.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
            proc.setBandParams(0, params);

            auto buf = makeTone();
            auto dry = buf;
            proc.process(buf);
            return std::pair<juce::AudioBuffer<float>, juce::AudioBuffer<float>>(std::move(dry), std::move(buf));
        };

        const auto [dry0, out0] = processWithMix(0.0f);
        const auto [dry50, out50] = processWithMix(0.5f);
        const auto [dry100, out100] = processWithMix(1.0f);

        auto rms = [](const juce::AudioBuffer<float>& buf)
        {
            return buf.getRMSLevel(0, 0, buf.getNumSamples());
        };

        const float dryRms = rms(dry0);
        const float out0Rms = rms(out0);
        const float out50Rms = rms(out50);
        const float out100Rms = rms(out100);

        logMessage("dry=" + juce::String(dryRms, 4)
                 + " mix0=" + juce::String(out0Rms, 4)
                 + " mix50=" + juce::String(out50Rms, 4)
                 + " mix100=" + juce::String(out100Rms, 4));

        expectWithinAbsoluteError(out0Rms, dryRms, 1.0e-4f,
                                  "Mix 0 should preserve the dry signal");
        expect(out100Rms > dryRms * 1.2f,
               "Mix 1 should reflect the boosted wet path");
        expect(out50Rms > dryRms * 1.05f && out50Rms < out100Rms * 0.98f,
               "Mix 0.5 should sit between dry and fully wet levels");
    }
};

static DynamicEQGlobalMixBehaviorTest dynamicEQGlobalMixBehaviorTest;

#endif
