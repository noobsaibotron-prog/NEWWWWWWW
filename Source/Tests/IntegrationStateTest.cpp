#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

/**
 * Integration-style checks on plugin state, preset round-trip, bypass and oversampling/latency reporting.
 * These tests run in Debug and link against the plugin shared code.
 */
class IntegrationStateTest : public juce::UnitTest
{
public:
    IntegrationStateTest() : juce::UnitTest("AIEqualizer Integration", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm); // ensure MessageManager exists; current thread becomes message thread

        testStateRoundTrip();
        testBypassPassThrough();
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

    void primeBands(AIEqualizerAudioProcessor& proc)
    {
        proc.setNumActiveBands(3);
        AIEqualizerAudioProcessor::BandState b0{ 500.0f, -3.0f, 0.9f, static_cast<int>(ParametricEQProcessor::Peak), true, false };
        AIEqualizerAudioProcessor::BandState b1{ 2000.0f, 4.0f, 2.0f, static_cast<int>(ParametricEQProcessor::HighShelf), true, false };
        AIEqualizerAudioProcessor::BandState b2{ 8000.0f, -2.0f, 1.4f, static_cast<int>(ParametricEQProcessor::LowShelf), true, false };
        proc.setBandState(0, b0);
        proc.setBandState(1, b1);
        proc.setBandState(2, b2);
    }

    void testStateRoundTrip()
    {
        beginTest("State round-trip preserves bands and globals");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        primeBands(proc);
        setChoice(apvts, "oversamplingFactor", 2); // 4x
        setChoice(apvts, "qualityMode", 1);        // HQ
        setBool(apvts, "bypass", true);
        setFloat(apvts, "outputGain", 3.0f);

        const auto orig0 = proc.getBandState(0);
        const auto orig1 = proc.getBandState(1);
        const auto orig2 = proc.getBandState(2);
        const int origLatency = proc.getLatencySamples();

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        // Mutate
        proc.setNumActiveBands(1);
        setChoice(apvts, "oversamplingFactor", 0);
        setBool(apvts, "bypass", false);
        setFloat(apvts, "outputGain", -6.0f);

        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        auto restored0 = proc.getBandState(0);
        auto restored1 = proc.getBandState(1);
        auto restored2 = proc.getBandState(2);

        expectWithinAbsoluteError(restored0.frequency, orig0.frequency, 1.0f);
        expectWithinAbsoluteError(restored1.frequency, orig1.frequency, 1.0f);
        expectWithinAbsoluteError(restored2.frequency, orig2.frequency, 1.0f);
        expect(restored0.enabled && restored1.enabled && restored2.enabled);

        auto* osParam = apvts.getRawParameterValue("oversamplingFactor");
        auto* bypassParam = apvts.getRawParameterValue("bypass");
        expect(osParam != nullptr);
        expect(bypassParam != nullptr);
        if (osParam) expectWithinAbsoluteError(osParam->load(), 2.0f, 0.01f);
        if (bypassParam) expect(bypassParam->load() > 0.5f);

        expect(proc.getLatencySamples() >= 0);
        expect(proc.getLatencySamples() == origLatency); // worst-case latency should be stable across state load
    }

    void testBypassPassThrough()
    {
        beginTest("Bypass leaves buffer untouched");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 128);
        auto& apvts = proc.getAPVTS();
        setBool(apvts, "bypass", true);

        juce::AudioBuffer<float> buffer(2, 128);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 1, 0.5f);
        juce::AudioBuffer<float> original(buffer);
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);

        bool equal = true;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            auto* ref = original.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(data[i] - ref[i]) > 1.0e-6f)
                {
                    equal = false;
                    break;
                }
            }
            if (!equal) break;
        }
        expect(equal);
    }
};

static IntegrationStateTest integrationStateTest;

