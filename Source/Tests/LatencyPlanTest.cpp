#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

class LatencyPlanTest final : public juce::UnitTest
{
public:
    LatencyPlanTest() : juce::UnitTest("Latency Plan", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        testFreshZeroLatencyPath();
        testLatentModesAdvertiseLatency();
        testRuntimePlanIsMonotonic();
        testDynamicLookaheadRequiresLatency();
    }

private:
    static void setParameter(AIEqualizerAudioProcessor& processor,
                             const juce::String& id,
                             float plainValue)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(
            processor.getAPVTS().getParameter(id));
        jassert(parameter != nullptr);
        if (parameter != nullptr)
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    }

    void testFreshZeroLatencyPath()
    {
        beginTest("Fresh Zero Latency plan reports and renders at sample zero");

        AIEqualizerAudioProcessor processor;
        setParameter(processor, "phaseMode", 0.0f);
        setParameter(processor, "qualityMode", 0.0f);
        setParameter(processor, "dryWet", 50.0f);
        processor.prepareToPlay(48000.0, 256);

        expectEquals(processor.getLatencySamples(), 0);

        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 0, 1.0f);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        expect(std::abs(buffer.getSample(0, 0)) > 0.5f,
               "Zero-latency impulse was delayed away from sample zero");
        expect(std::isfinite(buffer.getSample(0, 0)));
        processor.releaseResources();
    }

    void testLatentModesAdvertiseLatency()
    {
        beginTest("Natural and Linear modes never claim zero latency");

        for (const float mode : { 1.0f, 2.0f })
        {
            AIEqualizerAudioProcessor processor;
            setParameter(processor, "phaseMode", mode);
            processor.prepareToPlay(48000.0, 256);
            expect(processor.getLatencySamples() > 0);
            processor.releaseResources();
        }
    }

    void testRuntimePlanIsMonotonic()
    {
        beginTest("Runtime latency upgrades immediately and defers contraction");

        AIEqualizerAudioProcessor processor;
        setParameter(processor, "phaseMode", 0.0f);
        processor.prepareToPlay(48000.0, 256);
        expectEquals(processor.getLatencySamples(), 0);

        setParameter(processor, "phaseMode", 1.0f);
        const int paddedLatency = processor.getLatencySamples();
        expect(paddedLatency > 0, "Natural mode did not upgrade the latency plan");

        setParameter(processor, "phaseMode", 0.0f);
        expectEquals(processor.getLatencySamples(), paddedLatency,
                     "Live latency contraction must be deferred to prepareToPlay");

        processor.releaseResources();
        processor.prepareToPlay(48000.0, 256);
        expectEquals(processor.getLatencySamples(), 0,
                     "Deferred zero-latency plan was not applied on prepareToPlay");
        processor.releaseResources();
    }

    void testDynamicLookaheadRequiresLatency()
    {
        beginTest("HQ dynamic lookahead cannot advertise zero latency");

        AIEqualizerAudioProcessor processor;
        setParameter(processor, "phaseMode", 0.0f);
        setParameter(processor, "dynEqEnabled", 1.0f);
        setParameter(processor, "qualityMode", 1.0f);
        processor.prepareToPlay(48000.0, 256);
        expect(processor.getLatencySamples() > 0);
        processor.releaseResources();
    }
};

static LatencyPlanTest latencyPlanTest;
