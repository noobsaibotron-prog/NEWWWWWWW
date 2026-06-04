#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>
#include <functional>

/**
 * Regression test for ticket #1: the AI Sensitivity / Strength knobs must update
 * the detection even when TRANSPORT IS STOPPED (no audio frames flowing).
 *
 * Bug class: detection is audio-frame-driven. At transport stop no frames reach
 * the AI thread, so moving the knob did nothing (amber bars / AI panel frozen).
 *
 * The fix requires TWO corrections, both verified here independently:
 *   (A) parameterChanged() arms a forced re-analysis of the LAST spectrum on the
 *       AI thread (analyzeSpectrum(..., force=true)) -> isNewAnalysisAvailable()
 *       becomes true with NO processBlock running.
 *   (B) parameterChanged() pushes the new value to the engine directly from the
 *       message thread (setSensitivity/setStrength), because updateEQFromParameters()
 *       only runs inside processBlock -> getSensitivity()/getStrength() == target.
 *
 * Falsification (proven by hand):
 *   - remove (A): no re-analysis fires at transport stop -> poll times out -> FAIL.
 *   - remove (B): engine keeps the stale value -> getSensitivity()!=target -> FAIL.
 * These two assertions are atomic-observable and deterministic (no fixed sleeps).
 */
class AISensitivityTransportStopTest : public juce::UnitTest
{
public:
    AISensitivityTransportStopTest()
        : juce::UnitTest("AI Knob At Transport Stop", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm); // current thread becomes the message thread

        testSensitivityReanalyzesAtTransportStop();
        testStrengthReanalyzesAtTransportStop();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int    kBlockSize  = 512;

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    // Quiet low-mid sine so the spectrum analyzer produces a non-empty spectrum
    // that the AI thread caches. Content is irrelevant to the assertions; we only
    // need the AI thread's `spectrum` cache to be populated by >=1 popped frame.
    static void pumpOneBlock(AIEqualizerAudioProcessor& proc, double& phase)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 200.0 / kSampleRate;
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float s = 0.25f * static_cast<float>(std::sin(phase));
            phase += inc;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        proc.processBlock(buffer, midi);
    }

    // Bounded poll (NOT a fixed sleep): returns as soon as pred() is true, yielding
    // to the AI thread between checks, capped at timeoutMs as a failure ceiling.
    static bool pollUntil(std::function<bool()> pred, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                              + static_cast<juce::uint32>(timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (pred())
                return true;
            juce::Thread::yield();
        }
        return pred();
    }

    // Feed enough "playing" blocks that the ~10 Hz enqueue pushes several frames
    // and the AI thread pops at least one (populating its last-spectrum cache).
    void primeLastSpectrum(AIEqualizerAudioProcessor& proc, double& phase)
    {
        for (int b = 0; b < 60; ++b)
            pumpOneBlock(proc, phase);
        // Give the AI thread time to drain the queue (bounded, yields).
        pollUntil([]{ return false; }, 200);
    }

    void testSensitivityReanalyzesAtTransportStop()
    {
        beginTest("Sensitivity change re-analyzes the last spectrum at transport stop");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        auto& ai = proc.getAIEngine();
        double phase = 0.0;

        primeLastSpectrum(proc, phase);

        // --- TRANSPORT STOP: from here on we never call processBlock again. ---
        ai.clearNewAnalysisFlag();

        const float target = 0.90f; // clearly different from the 0.5 default
        setFloat(apvts, "aiSensitivity", target);

        // (A) A fresh analysis must fire with NO processBlock running.
        const bool reanalyzed = pollUntil([&]{ return ai.isNewAnalysisAvailable(); }, 2000);
        expect(reanalyzed,
               "No re-analysis fired at transport stop (correction A missing): the "
               "AI thread never re-ran detection after the knob moved");

        // (B) The engine must hold the new value, pushed from the message thread.
        expectWithinAbsoluteError(ai.getSensitivity(), target, 1.0e-3f,
               "Engine sensitivity not updated at transport stop (correction B "
               "missing): updateEQFromParameters never ran because transport is stopped");
    }

    void testStrengthReanalyzesAtTransportStop()
    {
        beginTest("Strength change re-analyzes the last spectrum at transport stop");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        auto& ai = proc.getAIEngine();
        double phase = 0.0;

        primeLastSpectrum(proc, phase);

        ai.clearNewAnalysisFlag();

        const float target = 0.20f; // clearly different from the 0.7 default
        setFloat(apvts, "aiStrength", target);

        const bool reanalyzed = pollUntil([&]{ return ai.isNewAnalysisAvailable(); }, 2000);
        expect(reanalyzed,
               "No re-analysis fired at transport stop for Strength (correction A missing)");

        expectWithinAbsoluteError(ai.getStrength(), target, 1.0e-3f,
               "Engine strength not updated at transport stop (correction B missing)");
    }
};

static AISensitivityTransportStopTest aiSensitivityTransportStopTest;
