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
        testDynamicABStateRoundTrip();
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

    void testDynamicABStateRoundTrip()
    {
        beginTest("A/B slots preserve per-band Dynamic EQ state across save/load");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        proc.setNumActiveBands(1);
        AIEqualizerAudioProcessor::BandState band0;
        band0.frequency = 1000.0f;
        band0.gain = 12.0f;
        band0.q = 1.0f;
        band0.type = static_cast<int>(ParametricEQProcessor::Peak);
        band0.enabled = true;
        band0.solo = false;
        band0.dynMode = 0;
        band0.dynThreshold = 0.0f;
        band0.dynRatio = 2.0f;
        band0.dynAttack = 10.0f;
        band0.dynRelease = 100.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 6.0f;
        proc.setBandState(0, band0);
        setBool(apvts, "dynEqEnabled", false);
        setFloat(apvts, "dynEqMix", 0.0f);
        setBool(apvts, "dynAutoMakeup", false);

        // Save A with Dynamic EQ effectively off.
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);

        // Configure B with aggressive dynamic compression.
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 100.0f);
        setBool(apvts, "dynAutoMakeup", true);
        band0.dynMode = 1;
        band0.dynThreshold = -30.0f;
        band0.dynRatio = 8.0f;
        band0.dynAttack = 1.0f;
        band0.dynRelease = 50.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 0.0f;
        proc.setBandState(0, band0);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        AIEqualizerAudioProcessor restored;
        restored.prepareToPlay(48000.0, 512);
        restored.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        auto& restoredAPVTS = restored.getAPVTS();

        restored.setABState(AIEqualizerAudioProcessor::ABState::A);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynMode"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Threshold"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Ratio"))
            expectWithinAbsoluteError(p->load(), 2.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqEnabled"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqMix"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynAutoMakeup"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);

        restored.setABState(AIEqualizerAudioProcessor::ABState::B);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynMode"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Threshold"))
            expectWithinAbsoluteError(p->load(), -30.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Ratio"))
            expectWithinAbsoluteError(p->load(), 8.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Attack"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Release"))
            expectWithinAbsoluteError(p->load(), 50.0f, 0.1f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Range"))
            expectWithinAbsoluteError(p->load(), 24.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Knee"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqEnabled"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqMix"))
            expectWithinAbsoluteError(p->load(), 100.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynAutoMakeup"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
    }

    void testBypassPassThrough()
    {
        beginTest("Bypass leaves buffer untouched");
        AIEqualizerAudioProcessor proc;
        const int blockSize = 128;
        proc.prepareToPlay(48000.0, blockSize);
        auto& apvts = proc.getAPVTS();
        setBool(apvts, "bypass", true);

        // With Maximum Latency Padding, bypass output is delayed by worstCaseLatencySamples.
        // Fill the delay line first by processing enough silent blocks, then verify
        // that the delayed output matches the original input.
        const int latency = proc.getLatencySamples();
        const int warmupBlocks = (latency / blockSize) + 3; // extra blocks for safety

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> warmup(2, blockSize);
        for (int b = 0; b < warmupBlocks; ++b)
        {
            warmup.clear();
            proc.processBlock(warmup, midi);
        }

        // Now send our test signal — it will come out `latency` samples later
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 1, 0.5f);
        juce::AudioBuffer<float> original(buffer);
        proc.processBlock(buffer, midi);

        // If latency == 0, output should match immediately
        if (latency == 0)
        {
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
        else
        {
            // Output is delayed — collect enough output blocks to find our test signal
            std::vector<float> outputL, outputR;
            for (int i = 0; i < blockSize; ++i)
            {
                outputL.push_back(buffer.getSample(0, i));
                outputR.push_back(buffer.getSample(1, i));
            }
            // Process more blocks to flush
            const int flushBlocks = (latency / blockSize) + 2;
            for (int b = 0; b < flushBlocks; ++b)
            {
                juce::AudioBuffer<float> flush(2, blockSize);
                flush.clear();
                proc.processBlock(flush, midi);
                for (int i = 0; i < blockSize; ++i)
                {
                    outputL.push_back(flush.getSample(0, i));
                    outputR.push_back(flush.getSample(1, i));
                }
            }

            // Find our impulse in the delayed output (should appear at offset `latency`)
            bool foundL = false, foundR = false;
            for (int i = 0; i < static_cast<int>(outputL.size()); ++i)
            {
                if (!foundL && std::abs(outputL[static_cast<size_t>(i)] - 1.0f) < 1.0e-4f)
                    foundL = true;
                if (!foundR && std::abs(outputR[static_cast<size_t>(i)] - 0.5f) < 1.0e-4f)
                    foundR = true;
            }
            expect(foundL, "Expected impulse in L channel after delay");
            expect(foundR, "Expected impulse in R channel after delay");
        }
    }
};

static IntegrationStateTest integrationStateTest;

