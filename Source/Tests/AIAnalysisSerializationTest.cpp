#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <thread>

class AIAnalysisSerializationTest : public juce::UnitTest
{
public:
    AIAnalysisSerializationTest()
        : juce::UnitTest("AI Analysis Serialization", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        beginTest("Capture and live AI analysis cannot overlap");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        auto& ai = proc.getAIEngine();

        double phase = 0.0;
        ai.clearNewAnalysisFlag();
        for (int i = 0; i < 80; ++i)
            pumpOneBlock(proc, phase);

        expect(pollUntil([&] { return ai.isNewAnalysisAvailable(); }, 2000),
               "Live AI thread did not publish the priming analysis");

        proc.captureAudioSnapshotMs(500);
        expect(proc.isCaptureBufferSafeToRead(), "Capture buffer was not marked ready");
        expect(!proc.getCapturedAudioMono().empty(), "Capture buffer unexpectedly empty");

        ai.clearNewAnalysisFlag();
        proc.resetAIAnalysisConcurrencyCountersForTests();
        proc.setAIAnalysisBlockForTests(true);
        AnalysisBlockScope unblock(proc);

        setFloat(apvts, "aiSensitivity", 0.82f);
        expect(pollUntil([&] { return proc.getAIAnalysisEnteredForTests() >= 1; }, 2000),
               "Forced live re-analysis did not enter the serialized AI section");

        std::atomic<bool> captureReturned { false };
        std::atomic<bool> captureOk { false };
        std::thread captureThread([&]
        {
            captureOk.store(proc.analyzeCapturedAudioSnapshot(), std::memory_order_release);
            captureReturned.store(true, std::memory_order_release);
        });

        expect(pollUntil([&] { return proc.getAIAnalysisCallAttemptsForTests() >= 2; }, 2000),
               "Capture analysis did not attempt to enter the serialized AI section");
        expectEquals(proc.getMaxConcurrentAIAnalysesForTests(), 1,
                     "AIEngine::analyzeSpectrum must never run concurrently");
        expect(!captureReturned.load(std::memory_order_acquire),
               "Capture should be waiting behind the live analysis while the test barrier is held");

        proc.setAIAnalysisBlockForTests(false);
        if (captureThread.joinable())
            captureThread.join();

        expect(captureReturned.load(std::memory_order_acquire), "Capture analysis thread did not finish");
        expect(captureOk.load(std::memory_order_acquire), "Capture analysis failed");

        ai.clearNewAnalysisFlag();
        for (int i = 0; i < 40; ++i)
            pumpOneBlock(proc, phase);

        expect(pollUntil([&] { return ai.isNewAnalysisAvailable(); }, 2000),
               "Live analysis did not resume after capture completed");

        proc.releaseResources();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;

    struct AnalysisBlockScope
    {
        explicit AnalysisBlockScope(AIEqualizerAudioProcessor& p) : proc(p) {}
        ~AnalysisBlockScope() { proc.setAIAnalysisBlockForTests(false); }
        AIEqualizerAudioProcessor& proc;
    };

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id,
                         float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static void pumpOneBlock(AIEqualizerAudioProcessor& proc, double& phase)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 220.0 / kSampleRate;

        for (int i = 0; i < kBlockSize; ++i)
        {
            const float sample = 0.25f * static_cast<float>(std::sin(phase));
            phase += inc;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;

            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
        }

        proc.processBlock(buffer, midi);
    }

    static bool pollUntil(std::function<bool()> predicate, int timeoutMs)
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
};

static AIAnalysisSerializationTest aiAnalysisSerializationTest;
