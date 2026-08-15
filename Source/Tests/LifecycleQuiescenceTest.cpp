#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>
#include <functional>

class LifecycleQuiescenceTest : public juce::UnitTest
{
public:
    LifecycleQuiescenceTest()
        // The runner only executes tests whose name matches one of the project
        // prefixes in TestMain.cpp::isAieqProjectTestName(). "Lifecycle
        // Quiescence" matched none, so this test compiled, linked and was
        // silently skipped — a green ThreadSafety run said nothing about EC-005.
        : juce::UnitTest("AIEqualizer Lifecycle Quiescence", "ThreadSafety") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        beginTest("releaseResources quiesces the AI worker and discards stale work");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSampleRate, kBlockSize);

            double phase = 0.0;
            proc.resetAIAnalysisConcurrencyCountersForTests();
            for (int i = 0; i < 120; ++i)
                pumpOneBlock(proc, phase, kSampleRate, kBlockSize);

            expect(pollUntil([&] { return proc.getAIAnalysisCallAttemptsForTests() > 0; }, 2000),
                   "Could not prime the live AI worker before release");

            proc.releaseResources();
            expect(!proc.isProcessorReady(), "Processor must remain closed after releaseResources");

            proc.resetAIAnalysisConcurrencyCountersForTests();
            setFloat(proc.getAPVTS(), "aiSensitivity", 0.83f);

            expect(noEventFor([&] { return proc.getAIAnalysisCallAttemptsForTests() > 0; }, 100),
                   "AI analysis ran after releaseResources; stale worker/queue work survived lifecycle teardown");
        }

        beginTest("prepareToPlay restarts quiesced workers without leaking the previous lifetime");
        {
            AIEqualizerAudioProcessor proc;
            double phase = 0.0;

            proc.prepareToPlay(44100.0, 256);
            for (int i = 0; i < 100; ++i)
                pumpOneBlock(proc, phase, 44100.0, 256);
            expect(pollUntil([&] { return proc.getAIAnalysisCallAttemptsForTests() > 0; }, 2000),
                   "AI worker did not run in first prepare lifetime");

            proc.releaseResources();
            proc.resetAIAnalysisConcurrencyCountersForTests();

            proc.prepareToPlay(96000.0, 1024);
            phase = 0.0;
            for (int i = 0; i < 100; ++i)
                pumpOneBlock(proc, phase, 96000.0, 1024);

            expect(pollUntil([&] { return proc.getAIAnalysisCallAttemptsForTests() > 0; }, 2000),
                   "AI worker did not restart after re-prepare");
            proc.releaseResources();
        }

        beginTest("Repeated lifecycle churn remains bounded with Linear Phase requests active");
        {
            AIEqualizerAudioProcessor proc;
            double phase = 0.0;
            constexpr double rates[] { 44100.0, 48000.0, 96000.0 };
            constexpr int blocks[] { 64, 256, 1024 };

            for (int cycle = 0; cycle < 18; ++cycle)
            {
                const double sr = rates[cycle % 3];
                const int block = blocks[(cycle / 3) % 3];
                proc.prepareToPlay(sr, block);

                // Keep the background IR path live while lifecycle churns.
                setChoice(proc.getAPVTS(), "phaseMode", 2);
                setFloat(proc.getAPVTS(), "band0Gain", (cycle & 1) ? -6.0f : 4.0f);

                for (int i = 0; i < 12; ++i)
                    pumpOneBlock(proc, phase, sr, block);

                proc.releaseResources();
                expect(!proc.isProcessorReady(), "Processor unexpectedly ready after lifecycle release");
            }
        }
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id,
                         float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id,
                          int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void pumpOneBlock(AIEqualizerAudioProcessor& proc,
                             double& phase,
                             double sampleRate,
                             int blockSize)
    {
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 220.0 / sampleRate;

        for (int i = 0; i < blockSize; ++i)
        {
            const float sample = 0.2f * static_cast<float>(std::sin(phase));
            phase += inc;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }

        proc.processBlock(buffer, midi);
        juce::Thread::yield();
    }

    static bool pollUntil(const std::function<bool()>& predicate, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                            + static_cast<juce::uint32>(timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (predicate())
                return true;
            juce::Thread::yield();
        }
        return predicate();
    }

    static bool noEventFor(const std::function<bool()>& predicate, int durationMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                            + static_cast<juce::uint32>(durationMs);
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (predicate())
                return false;
            juce::Thread::yield();
        }
        return !predicate();
    }
};

static LifecycleQuiescenceTest lifecycleQuiescenceTest;
