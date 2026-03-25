#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

/**
 * Latency contract test — verifies that getLatencySamples() is always non-negative
 * and deterministic for a given configuration.
 *
 * NOTE: The current plugin reports *current* latency (varies with phase mode and
 * oversampling factor), not worst-case latency.  If the architecture changes to
 * worst-case reporting in the future, strengthen the cross-setting assertions.
 */
class LinearPhaseLatencyContractTest : public juce::UnitTest
{
public:
    LinearPhaseLatencyContractTest()
        : juce::UnitTest("LinearPhaseLatencyContract", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testLatencyNonNegativeAllCombinations();
        testLatencyDeterministicForSameSettings();
    }

private:
    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    void testLatencyNonNegativeAllCombinations()
    {
        beginTest("Latency is non-negative for every phase/oversampling combination");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        for (int phaseMode = 0; phaseMode <= 2; ++phaseMode)
        {
            setChoice(apvts, "phaseMode", phaseMode);

            for (int oversampling = 0; oversampling <= 2; ++oversampling)
            {
                setChoice(apvts, "oversamplingFactor", oversampling);

                const int latency = proc.getLatencySamples();
                expect(latency >= 0,
                       "Latency must be non-negative (phase=" + juce::String(phaseMode)
                       + " os=" + juce::String(oversampling)
                       + " latency=" + juce::String(latency) + ")");
            }
        }

        proc.releaseResources();
    }

    void testLatencyDeterministicForSameSettings()
    {
        beginTest("Same settings produce same latency across two independent instances");

        for (int phaseMode = 0; phaseMode <= 2; ++phaseMode)
        {
            for (int oversampling = 0; oversampling <= 2; ++oversampling)
            {
                AIEqualizerAudioProcessor procA;
                procA.prepareToPlay(48000.0, 256);
                setChoice(procA.getAPVTS(), "phaseMode", phaseMode);
                setChoice(procA.getAPVTS(), "oversamplingFactor", oversampling);

                AIEqualizerAudioProcessor procB;
                procB.prepareToPlay(48000.0, 256);
                setChoice(procB.getAPVTS(), "phaseMode", phaseMode);
                setChoice(procB.getAPVTS(), "oversamplingFactor", oversampling);

                const int latA = procA.getLatencySamples();
                const int latB = procB.getLatencySamples();

                expect(latA == latB,
                       "Latency should be deterministic for same settings (phase="
                       + juce::String(phaseMode) + " os=" + juce::String(oversampling)
                       + " A=" + juce::String(latA) + " B=" + juce::String(latB) + ")");

                procA.releaseResources();
                procB.releaseResources();
            }
        }
    }
};

static LinearPhaseLatencyContractTest linearPhaseLatencyContractTest;
