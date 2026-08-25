#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"

#include <cmath>
#include <vector>

/**
 * Plugin-level external-sidechain routing contract.
 *
 * DynamicEQSidechainBehaviorTest covers DynamicEQProcessor directly.  It cannot
 * prove that the host's auxiliary input bus is isolated from the main bus, or
 * that the correct detector block reaches the DynamicEQProcessor instance that
 * is active in each PluginProcessor phase/channel path.  This test deliberately
 * exercises the flattened processBlock buffer used by JUCE hosts.
 *
 * The sidechain bus is input-only.  Writing to its channel is forbidden by the
 * AudioProcessor contract, so preservation is asserted sample-for-sample in
 * addition to detector availability and gain-reduction behaviour.
 */
class PluginExternalSidechainIntegrationTest final : public juce::UnitTest
{
public:
    PluginExternalSidechainIntegrationTest()
        : juce::UnitTest("Plugin External Sidechain Integration", "Integration")
    {
    }

    void runTest() override
    {
        auto* messageManager = juce::MessageManager::getInstance();
        juce::ignoreUnused(messageManager);

        runStereoCase("Zero Latency / stereo main / mono sidechain",
                      AIEqualizerAudioProcessor::PhaseMode::ZeroLatency,
                      0,
                      AIEqualizerAudioProcessor::MSMode::Stereo,
                      false);
        runStereoCase("Natural 2x / stereo main / mono sidechain",
                      AIEqualizerAudioProcessor::PhaseMode::NaturalPhase,
                      1,
                      AIEqualizerAudioProcessor::MSMode::Stereo,
                      false);
        runStereoCase("Natural 4x / stereo main / mono sidechain",
                      AIEqualizerAudioProcessor::PhaseMode::NaturalPhase,
                      2,
                      AIEqualizerAudioProcessor::MSMode::Stereo,
                      false);
        runStereoCase("Linear Phase / stereo main / mono sidechain",
                      AIEqualizerAudioProcessor::PhaseMode::LinearPhase,
                      0,
                      AIEqualizerAudioProcessor::MSMode::Stereo,
                      true);
        runStereoCase("M/S Linked / stereo main / mono sidechain",
                      AIEqualizerAudioProcessor::PhaseMode::ZeroLatency,
                      0,
                      AIEqualizerAudioProcessor::MSMode::MSLinked,
                      false);

        runMonoMainCase();
        runAbsentSidechainCase();
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 128;
    static constexpr float detectorFrequency = 1000.0f;
    static constexpr int warmupBlocks = 96;

    static bool setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id,
                          int index)
    {
        auto* parameter = apvts.getParameter(id);
        if (parameter == nullptr)
            return false;

        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(static_cast<float>(index)));
        return true;
    }

    static bool setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id,
                         float value)
    {
        auto* parameter = apvts.getParameter(id);
        if (parameter == nullptr)
            return false;

        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        return true;
    }

    static bool setBool(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& id,
                        bool value)
    {
        auto* parameter = apvts.getParameter(id);
        if (parameter == nullptr)
            return false;

        parameter->setValueNotifyingHost(value ? 1.0f : 0.0f);
        return true;
    }

    static bool installLayout(AIEqualizerAudioProcessor& processor,
                              const juce::AudioChannelSet& mainLayout)
    {
        auto layout = processor.getBusesLayout();
        if (layout.inputBuses.size() < 2 || layout.outputBuses.isEmpty())
            return false;

        layout.getChannelSet(true, 0) = mainLayout;
        layout.getChannelSet(true, 1) = juce::AudioChannelSet::mono();
        layout.getChannelSet(false, 0) = mainLayout;
        return processor.setBusesLayout(layout);
    }

    void configure(AIEqualizerAudioProcessor& processor,
                   AIEqualizerAudioProcessor::PhaseMode phase,
                   int oversampling,
                   AIEqualizerAudioProcessor::MSMode msMode)
    {
        auto& apvts = processor.getAPVTS();

        expect(setFloat(apvts, "numActiveBands", 1.0f),
               "numActiveBands parameter is required");
        expect(setChoice(apvts, "phaseMode", static_cast<int>(phase)),
               "phaseMode parameter is required");
        expect(setChoice(apvts, "oversamplingFactor", oversampling),
               "oversamplingFactor parameter is required");
        expect(setChoice(apvts, "msMode", static_cast<int>(msMode)),
               "msMode parameter is required");
        expect(setChoice(apvts, "qualityMode", 0),
               "qualityMode parameter is required");

        expect(setBool(apvts, "dynEqEnabled", true),
               "dynEqEnabled parameter is required");
        expect(setFloat(apvts, "dynEqMix", 100.0f),
               "dynEqMix parameter is required");
        expect(setBool(apvts, "dynAutoMakeup", false),
               "dynAutoMakeup parameter is required");
        expect(setFloat(apvts, "dryWet", 100.0f),
               "dryWet parameter is required");
        expect(setFloat(apvts, "outputGain", 0.0f),
               "outputGain parameter is required");
        expect(setBool(apvts, "autoGain", false),
               "autoGain parameter is required");
        expect(setBool(apvts, "bypass", false),
               "bypass parameter is required");
        expect(setBool(apvts, "aiEnabled", false),
               "aiEnabled parameter is required");

        constexpr auto prefix = "band0";
        expect(setFloat(apvts, juce::String(prefix) + "Freq", detectorFrequency),
               "band frequency parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Gain", 12.0f),
               "band gain parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Q", 3.0f),
               "band Q parameter is required");
        expect(setChoice(apvts, juce::String(prefix) + "Type",
                         static_cast<int>(ParametricEQProcessor::Peak)),
               "band type parameter is required");
        expect(setBool(apvts, juce::String(prefix) + "Enabled", true),
               "band enabled parameter is required");
        expect(setChoice(apvts, juce::String(prefix) + "DynMode",
                         DynamicEQProcessor::DynamicMode_Compress),
               "dynamic mode parameter is required");
        expect(setChoice(apvts, juce::String(prefix) + "DynTrigger",
                         DynamicEQProcessor::TriggerSide_Above),
               "dynamic trigger parameter is required");
        expect(setChoice(apvts, juce::String(prefix) + "DetectionMode",
                         DynamicEQProcessor::DetectionMode_RMS),
               "detection mode must be exposed at plugin level");
        expect(setChoice(apvts, juce::String(prefix) + "DetectorSource",
                         DynamicEQProcessor::DetectorSource_ExternalWideband),
               "external detector source must be exposed at plugin level");
        expect(setFloat(apvts, juce::String(prefix) + "SidechainFreq",
                        detectorFrequency),
               "sidechain frequency must be exposed at plugin level");
        expect(setFloat(apvts, juce::String(prefix) + "SidechainQ", 3.0f),
               "sidechain Q must be exposed at plugin level");
        expect(setFloat(apvts, juce::String(prefix) + "Threshold", -30.0f),
               "threshold parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Ratio", 8.0f),
               "ratio parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Attack", 1.0f),
               "attack parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Release", 80.0f),
               "release parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Range", 24.0f),
               "range parameter is required");
        expect(setFloat(apvts, juce::String(prefix) + "Knee", 0.0f),
               "knee parameter is required");
    }

    struct RenderEvidence
    {
        float maximumMainOutput = 0.0f;
        float maximumSidechainMutation = 0.0f;
        float gainReduction = 0.0f;
        DynamicEQProcessor::DetectorAvailability availability =
            DynamicEQProcessor::DetectorAvailability::ExternalUnavailable;
    };

    static RenderEvidence renderSilentMainWithExternalTone(
        AIEqualizerAudioProcessor& processor,
        int mainChannels)
    {
        const int totalInputChannels = processor.getTotalNumInputChannels();
        juce::AudioBuffer<float> processBuffer(totalInputChannels, blockSize);
        juce::MidiBuffer midi;
        std::vector<float> detectorReference(static_cast<size_t>(blockSize));
        RenderEvidence evidence;
        int64_t absoluteSample = 0;

        const int sidechainChannel =
            processor.getChannelIndexInProcessBlockBuffer(true, 1, 0);

        for (int block = 0; block < warmupBlocks; ++block)
        {
            processBuffer.clear();
            auto* sidechain = processBuffer.getWritePointer(sidechainChannel);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float phase = juce::MathConstants<float>::twoPi
                    * detectorFrequency
                    * static_cast<float>(absoluteSample + sample)
                    / static_cast<float>(sampleRate);
                const float value = 0.8f * std::sin(phase);
                sidechain[sample] = value;
                detectorReference[static_cast<size_t>(sample)] = value;
            }

            processor.processBlock(processBuffer, midi);

            for (int channel = 0; channel < mainChannels; ++channel)
            {
                const auto* output = processBuffer.getReadPointer(channel);
                for (int sample = 0; sample < blockSize; ++sample)
                    evidence.maximumMainOutput = std::max(
                        evidence.maximumMainOutput, std::abs(output[sample]));
            }

            const auto* detectorAfter =
                processBuffer.getReadPointer(sidechainChannel);
            for (int sample = 0; sample < blockSize; ++sample)
                evidence.maximumSidechainMutation = std::max(
                    evidence.maximumSidechainMutation,
                    std::abs(detectorAfter[sample]
                             - detectorReference[static_cast<size_t>(sample)]));

            absoluteSample += blockSize;
        }

        evidence.gainReduction =
            processor.getDynamicBandMeter(0).gainReduction;
        evidence.availability =
            processor.getActiveDynamicEQProcessorForDisplay()
                .getDetectorAvailability(0);
        return evidence;
    }

    void assertEvidence(const juce::String& label,
                        const RenderEvidence& evidence)
    {
        logMessage(label
                   + ": GR=" + juce::String(evidence.gainReduction, 3)
                   + " dB, mainPeak=" + juce::String(evidence.maximumMainOutput, 8)
                   + ", sidechainMutation="
                   + juce::String(evidence.maximumSidechainMutation, 8));

        expect(evidence.availability
                   == DynamicEQProcessor::DetectorAvailability::ExternalAvailable,
               label + ": selected external detector must report available");
        expect(evidence.gainReduction < -1.0f,
               label + ": a hot present external detector must drive compression");
        expect(evidence.maximumMainOutput <= 1.0e-7f,
               label + ": external detector audio must never leak into a silent main bus");
        expect(evidence.maximumSidechainMutation == 0.0f,
               label + ": processBlock must not write to the input-only sidechain bus");
    }

    void runStereoCase(const juce::String& label,
                       AIEqualizerAudioProcessor::PhaseMode phase,
                       int oversampling,
                       AIEqualizerAudioProcessor::MSMode msMode,
                       bool forceLinearReady)
    {
        beginTest(label);

        AIEqualizerAudioProcessor processor;
        const bool layoutInstalled =
            installLayout(processor, juce::AudioChannelSet::stereo());
        expect(layoutInstalled,
               label + ": stereo main + mono sidechain layout must be supported");
        if (!layoutInstalled)
            return;

        configure(processor, phase, oversampling, msMode);
        processor.prepareToPlay(sampleRate, blockSize);
        if (forceLinearReady)
            processor.forceLinearIRReady();

        const auto evidence = renderSilentMainWithExternalTone(processor, 2);
        assertEvidence(label, evidence);
        processor.releaseResources();
    }

    void runMonoMainCase()
    {
        const juce::String label =
            "Zero Latency / mono main / mono sidechain bus isolation";
        beginTest(label);

        AIEqualizerAudioProcessor processor;
        const bool layoutInstalled =
            installLayout(processor, juce::AudioChannelSet::mono());
        expect(layoutInstalled,
               label + ": mono main + mono sidechain layout must be supported");
        if (!layoutInstalled)
            return;

        configure(processor,
                  AIEqualizerAudioProcessor::PhaseMode::ZeroLatency,
                  0,
                  AIEqualizerAudioProcessor::MSMode::Stereo);
        processor.prepareToPlay(sampleRate, blockSize);

        const auto evidence = renderSilentMainWithExternalTone(processor, 1);
        assertEvidence(label, evidence);
        processor.releaseResources();
    }

    void runAbsentSidechainCase()
    {
        const juce::String label =
            "External source selected / sidechain bus absent fails closed";
        beginTest(label);

        AIEqualizerAudioProcessor processor; // optional sidechain stays disabled
        configure(processor,
                  AIEqualizerAudioProcessor::PhaseMode::ZeroLatency,
                  0,
                  AIEqualizerAudioProcessor::MSMode::Stereo);
        processor.prepareToPlay(sampleRate, blockSize);

        juce::AudioBuffer<float> processBuffer(
            processor.getTotalNumInputChannels(), blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < warmupBlocks; ++block)
        {
            processBuffer.clear();
            processor.processBlock(processBuffer, midi);
        }

        expect(processor.getDynamicDetectorAvailability(0)
                   == DynamicEQProcessor::DetectorAvailability::ExternalUnavailable,
               label + ": runtime telemetry must distinguish an absent bus");
        expectWithinAbsoluteError(
            processor.getDynamicBandMeter(0).gainReduction, 0.0f, 0.05f,
            label + ": absent detector must release toward neutral");
        processor.releaseResources();
    }
};

static PluginExternalSidechainIntegrationTest
    pluginExternalSidechainIntegrationTest;
