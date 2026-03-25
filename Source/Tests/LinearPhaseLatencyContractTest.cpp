#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

class LinearPhaseLatencyContractTest : public juce::UnitTest
{
public:
    LinearPhaseLatencyContractTest()
        : juce::UnitTest("LinearPhaseLatencyContract", "Integration") {}

    void runTest() override
    {
        beginTest("Reported latency stays stable across phase and oversampling changes");

        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        auto setChoice = [&](const juce::String& id, int index)
        {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
        };

        const int initialLatency = proc.getLatencySamples();
        expect(initialLatency >= 0, "Initial latency must be non-negative");

        for (int phaseMode = 0; phaseMode <= 2; ++phaseMode)
        {
            setChoice("phaseMode", phaseMode);
            const int afterPhaseLatency = proc.getLatencySamples();
            expect(afterPhaseLatency == initialLatency,
                   "Phase mode change should not change reported host latency");

            for (int oversampling = 0; oversampling <= 2; ++oversampling)
            {
                setChoice("oversamplingFactor", oversampling);
                const int latency = proc.getLatencySamples();
                expect(latency == initialLatency,
                       "Oversampling change should keep reported worst-case latency stable");
                expect(latency >= 0, "Latency must remain non-negative");
            }
        }

        proc.releaseResources();
    }
};

static LinearPhaseLatencyContractTest linearPhaseLatencyContractTest;
