#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

#include "../PluginProcessor.h"

class SurgicalAutomationContractTest final : public juce::UnitTest
{
public:
    SurgicalAutomationContractTest()
        : juce::UnitTest("Surgical Automation Contract", "Integration") {}

    void runTest() override
    {
        testPluginUsesLogFrequencySlew();
        testTopologyRequestsAreLastWinsAfterFade();
    }

private:
    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(static_cast<float>(index)));
    }

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }

    static void processSilence(AIEqualizerAudioProcessor& processor, int blockSize)
    {
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
    }

    static void configureSingleBand(AIEqualizerAudioProcessor& processor)
    {
        auto& apvts = processor.getAPVTS();
        setChoice(apvts, "numActiveBands", 0);
        setChoice(apvts, "band0Type", ParametricEQProcessor::Peak);
        setChoice(apvts, "band0CurveMode", 1);
        setFloat(apvts, "band0Freq", 1000.0f);
        setFloat(apvts, "band0Gain", 6.0f);
        setFloat(apvts, "band0Q", 1.0f);
    }

    void testPluginUsesLogFrequencySlew()
    {
        beginTest("Plugin applies the signed 100 octaves/s frequency ceiling");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 48;
        AIEqualizerAudioProcessor processor;
        configureSingleBand(processor);
        processor.prepareToPlay(sampleRate, blockSize);
        processSilence(processor, blockSize);

        const double start = processor.getEQProcessor().getBandFrequency(0);
        expectWithinAbsoluteError(start, 1000.0, 1.0e-3);

        setFloat(processor.getAPVTS(), "band0Freq", 8000.0f);
        processSilence(processor, blockSize);

        const double expected = 1000.0 * std::exp2(100.0 * blockSize / sampleRate);
        expectWithinAbsoluteError(
            static_cast<double>(processor.getEQProcessor().getBandFrequency(0)),
            expected, 2.0e-3);
        processor.releaseResources();
    }

    void testTopologyRequestsAreLastWinsAfterFade()
    {
        beginTest("Type and CurveMode requests during fade are deferred and last-wins");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 32;
        AIEqualizerAudioProcessor processor;
        configureSingleBand(processor);
        processor.prepareToPlay(sampleRate, blockSize);
        processSilence(processor, blockSize);
        auto& apvts = processor.getAPVTS();

        setChoice(apvts, "band0Type", ParametricEQProcessor::Notch);
        processSilence(processor, blockSize);
        expectEquals(processor.getEQProcessor().getBandType(0),
                     static_cast<int>(ParametricEQProcessor::Notch));
        expect(processor.getEQProcessor().getBandCurveMode(0)
               == ParametricEQProcessor::CurveMode::Surgical);

        // This request arrives during the Notch transition and must never be
        // published before the signed 1024-sample transition completes.
        setChoice(apvts, "band0Type", ParametricEQProcessor::HighShelf);
        setChoice(apvts, "band0CurveMode", 0);
        processSilence(processor, blockSize);
        expectEquals(processor.getEQProcessor().getBandType(0),
                     static_cast<int>(ParametricEQProcessor::Notch));
        expect(processor.getEQProcessor().getBandCurveMode(0)
               == ParametricEQProcessor::CurveMode::Surgical);

        // A newer request supersedes HighShelf. It is the only pending request
        // that may be applied when the current fade has finished.
        setChoice(apvts, "band0Type", ParametricEQProcessor::BandPass);
        processSilence(processor, blockSize);
        expectEquals(processor.getEQProcessor().getBandType(0),
                     static_cast<int>(ParametricEQProcessor::Notch));
        expect(processor.getEQProcessor().getBandCurveMode(0)
               == ParametricEQProcessor::CurveMode::Surgical);

        // Two countdown blocks have elapsed (HighShelf, then BandPass). Keep
        // processing until exactly one block remains and verify that the old
        // topology is still the published authority at sample 1023.
        constexpr int topologyFadeSamples = 1024;
        constexpr int fadeBlocks = topologyFadeSamples / blockSize;
        for (int elapsedBlocks = 2; elapsedBlocks < fadeBlocks - 1; ++elapsedBlocks)
            processSilence(processor, blockSize);
        expectEquals(processor.getEQProcessor().getBandType(0),
                     static_cast<int>(ParametricEQProcessor::Notch));

        // The final countdown block publishes only the latest request.
        processSilence(processor, blockSize);
        expectEquals(processor.getEQProcessor().getBandType(0),
                     static_cast<int>(ParametricEQProcessor::BandPass));
        expect(processor.getEQProcessor().getBandCurveMode(0)
               == ParametricEQProcessor::CurveMode::Legacy);

        processor.releaseResources();
    }
};

static SurgicalAutomationContractTest surgicalAutomationContractTest;
