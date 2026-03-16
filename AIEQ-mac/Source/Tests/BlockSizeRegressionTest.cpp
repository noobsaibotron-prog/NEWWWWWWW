#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <random>
#include "../PluginProcessor.h"

/**
 * Regression: ensure dry/wet mix is stable when the host delivers a block
 * larger than the size used in prepareToPlay (simulates block-size change
 * without a re-prepare). Previously, DynamicEQProcessor cleared the tail
 * of the buffer when numSamples > dryBuffer capacity, causing partial silence.
 */
class BlockSizeRegressionTest : public juce::UnitTest
{
public:
    BlockSizeRegressionTest() : juce::UnitTest("BlockSize Regression", "Regression") {}

    void runTest() override
    {
        beginTest("Dry/wet survives oversized block without re-prepare");

        AIEqualizerAudioProcessor proc;
        constexpr double sampleRate = 48000.0;
        constexpr int initialBlockSize = 256;
        constexpr int oversizedBlock = 2048;

        proc.prepareToPlay(sampleRate, initialBlockSize);

        auto& apvts = proc.getAPVTS();
        setChoice(apvts, "phaseMode", 2);            // Linear phase
        setChoice(apvts, "msMode", 0);               // Stereo
        setChoice(apvts, "oversamplingFactor", 1);   // 2x (deterministic)
        setChoice(apvts, "numActiveBands", AIEqualizerAudioProcessor::maxBands - 1); // 24 bands
        setFloat(apvts, "dryWet", 50.0f);            // 50% blend
        setFloat(apvts, "outputGain", 1.0f);         // avoid zero-gain false positives
        setBool(apvts, "bypass", false);

        juce::AudioBuffer<float> buffer(2, oversizedBlock);
        juce::MidiBuffer midi;

        // Deterministic noise block (reused to avoid reallocations)
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-0.25f, 0.25f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = dist(rng);
        }

        proc.processBlock(buffer, midi);

        // Basic sanity: no NaN/Inf
        bool finite = true;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (!std::isfinite(data[i]))
                {
                    finite = false;
                    break;
                }
            }
        }
        expect(finite, "Buffer contains non-finite values after processing.");

        // Tail region (beyond dryBuffer prepared for initialBlockSize*4). It must not be zeroed.
        const int tailStart = initialBlockSize * 4; // matches DynamicEQProcessor dryBuffer preallocation
        expect(tailStart < buffer.getNumSamples(), "Tail start exceeds buffer length.");

        float tailMax = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = tailStart; i < buffer.getNumSamples(); ++i)
                tailMax = std::max(tailMax, std::abs(data[i]));
        }

        expect(tailMax > 1.0e-4f, "Tail of oversized block was cleared (regression for dry/wet).");

        // RMS guard: tail should stay within 20 dB of full-block RMS and above -80 dBFS
        const auto rmsFull = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        const auto rmsTail = buffer.getRMSLevel(0, tailStart, buffer.getNumSamples() - tailStart);
        expect(rmsTail > 1.0e-4f, "Tail RMS is effectively silent.");
        if (rmsFull > 1.0e-6f)
            expect(rmsTail > rmsFull * 0.1f, "Tail RMS dropped more than 20 dB vs full block.");

        expectEquals(proc.getBlockClampEvents(), 0u, "Block clamp events should remain zero.");
        auto* bypassParam = apvts.getRawParameterValue("bypass");
        if (bypassParam != nullptr)
            expect(bypassParam->load() < 0.5f, "Bypass should remain off.");
    }

private:
    static void setChoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void setBool(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
    }

    static void setFloat(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }
};

static BlockSizeRegressionTest blockSizeRegressionTest;
