/**
 * AIFrontEndOverrunRecoveryTest — EC-001/B4 stream-integrity regression.
 *
 * Deliberately stalls AIEngine so the dedicated audio->AI FIFO overruns. The
 * production policy must fail closed: whole audio blocks are dropped, a sticky
 * discontinuity is published, queued history is discarded, frontend overlap and
 * live detection persistence are reset, and subsequent bounded input recovers.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>

// Deliberately UNPACED. Every other AI test paces its producer so the consumer
// keeps up; this one exists to outrun the consumer on purpose and prove that the
// fail-closed overrun policy drops whole blocks, resets overlap and recovers.
// If this test is ever paced, it stops testing anything.
class AIFrontEndOverrunRecoveryTest : public juce::UnitTest
{
public:
    AIFrontEndOverrunRecoveryTest()
        : juce::UnitTest("AI Front-End Overrun Recovery", "ThreadSafety") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        constexpr int block = 512;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sr, block);
        proc.resetAIAnalysisConcurrencyCountersForTests();
        proc.setAIAnalysisBlockForTests(true);

        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 440.0 / sr;

        auto feedTone = [&]
        {
            juce::AudioBuffer<float> buffer(2, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.2f * static_cast<float>(std::sin(phase));
                phase += inc;
                if (phase >= 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
            }
            proc.processBlock(buffer, midi);
        };

        beginTest("Deliberate worker stall causes observable whole-block FIFO drop");
        for (int i = 0; i < 24; ++i)
            feedTone();

        const auto enteredDeadline = juce::Time::getMillisecondCounter() + 2000u;
        while (juce::Time::getMillisecondCounter() < enteredDeadline
               && proc.getAIAnalysisEnteredForTests() == 0)
            juce::Thread::yield();
        expect(proc.getAIAnalysisEnteredForTests() > 0,
               "AI analysis did not enter the deliberate stall witness.");

        // Worker is now held outside aiFrontEndDrainBusy, so this burst must fill
        // the 32768-sample FIFO and exercise the fail-closed overrun path.
        for (int i = 0; i < 220; ++i)
            feedTone();

        auto diag = proc.getAIFrontEndDiagnostics();
        expect(diag.droppedSamples > 0,
               "Deliberate FIFO overrun was not observed by dropped-sample diagnostics.");

        proc.setAIAnalysisBlockForTests(false);

        const auto recoveryDeadline = juce::Time::getMillisecondCounter() + 3000u;
        while (juce::Time::getMillisecondCounter() < recoveryDeadline)
        {
            diag = proc.getAIFrontEndDiagnostics();
            if (diag.discontinuities > 0)
                break;
            juce::Thread::yield();
        }
        expect(diag.discontinuities > 0,
               "Worker did not consume the sticky discontinuity after overrun.");

        beginTest("Overrun recovery clears stale live detections and resumes on a clean stream");
        // resetLiveDetectionState() is part of discontinuity handling: user-applied
        // corrections survive, but stale pending detector output/history must not.
        expect(proc.getAIEngine().getPendingCorrections().empty(),
               "Pending detections survived the fail-closed stream reset.");

        // Feed >3 s of silence in bounded 4096-sample groups. The frontend was
        // reset by recovery, so each group should add two frames without drops.
        const auto droppedBeforeRecovery = proc.getAIFrontEndDiagnostics().droppedSamples;
        auto baseFrames = proc.getAIFrontEndDiagnostics().frames;
        for (int b = 0; b < 320; ++b)
        {
            juce::AudioBuffer<float> silent(2, block);
            silent.clear();
            proc.processBlock(silent, midi);

            if ((b + 1) % 8 == 0)
            {
                const auto wanted = baseFrames
                    + (static_cast<juce::int64>(b + 1) * block) / PerceptualFrontEnd::kHopSize;
                const auto deadline = juce::Time::getMillisecondCounter() + 1000u;
                while (juce::Time::getMillisecondCounter() < deadline
                       && proc.getAIFrontEndDiagnostics().frames < wanted)
                    juce::Thread::yield();
            }
        }

        const auto finalDiag = proc.getAIFrontEndDiagnostics();
        expectEquals(finalDiag.droppedSamples, droppedBeforeRecovery,
                     "Bounded recovery stream caused additional AI FIFO drops.");
        expect(proc.getAIEngine().getPendingCorrections().empty(),
               "Detector did not remain clean after overrun recovery + sustained silence.");
    }
};

static AIFrontEndOverrunRecoveryTest sAIFrontEndOverrunRecoveryTest;
