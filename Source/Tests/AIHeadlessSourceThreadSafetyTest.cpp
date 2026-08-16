/**
 * AIHeadlessSourceThreadSafetyTest — EC-001/B4 sanitizer witness.
 *
 * Exercises the production headless producer/consumer path while the GUI-owned
 * SpectrumAnalyzer runs independently. The purpose is race detection under
 * TSan, not detector-quality scoring.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <atomic>
#include <cmath>
#include <thread>

class AIHeadlessSourceThreadSafetyTest : public juce::UnitTest
{
public:
    AIHeadlessSourceThreadSafetyTest()
        : juce::UnitTest("AI Headless Source Thread Safety", "ThreadSafety") {}

    void runTest() override
    {
        beginTest("Audio/headless AI and GUI analyzer coexist without shared detector state");

        constexpr double sr = 48000.0;
        constexpr int block = 512;
        constexpr int blocks = 180;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sr, block);

        std::atomic<bool> done { false };
        std::thread audio([&]
        {
            juce::MidiBuffer midi;
            double phase = 0.0;
            const double inc = 2.0 * juce::MathConstants<double>::pi * 440.0 / sr;
            for (int b = 0; b < blocks; ++b)
            {
                juce::AudioBuffer<float> buffer(2, block);
                for (int i = 0; i < block; ++i)
                {
                    const float sample = 0.2f * static_cast<float>(std::sin(phase));
                    phase += inc;
                    if (phase >= 2.0 * juce::MathConstants<double>::pi)
                        phase -= 2.0 * juce::MathConstants<double>::pi;
                    buffer.setSample(0, i, sample);
                    buffer.setSample(1, i, sample);
                }
                proc.processBlock(buffer, midi);
                // Paced. This test asserts that the headless AI source and the
                // GUI analyzer coexist, not that the FIFO survives an overrun.
                // An unpaced producer outruns the AI consumer, and the B4
                // fail-closed policy then discards queued audio and resets
                // overlap on every overflow, so the detector never accumulates
                // the continuous samples one frame needs. Overflow behaviour is
                // the deliberate subject of AIFrontEndOverrunRecoveryTest, which
                // stays unpaced on purpose.
                juce::Thread::sleep(2);
            }
            done.store(true, std::memory_order_release);
        });

        int guiCalls = 0;
        while (!done.load(std::memory_order_acquire))
        {
            proc.getSpectrumAnalyzer().processFFT();
            ++guiCalls;
            juce::Thread::yield();
        }
        audio.join();

        const auto deadline = juce::Time::getMillisecondCounter() + 2000u;
        while (juce::Time::getMillisecondCounter() < deadline
               && proc.getAIHeadlessAnalysisCountForTests() == 0)
            juce::Thread::yield();

        expect(guiCalls > 0, "GUI analyzer witness did not execute.");
        expect(proc.getAIHeadlessAnalysisCountForTests() > 0,
               "Headless AI worker executed no productive analysis.");
    }
};

static AIHeadlessSourceThreadSafetyTest sAIHeadlessSourceThreadSafetyTest;
