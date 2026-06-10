/**
 * AIFrontEndWiringTest — Roadmap v1, P2 Commit 2 (category "Integration").
 *
 * Witnesses for the DIAGNOSTICS-ONLY wiring of the AI-owned PerceptualFrontEnd:
 * the AI thread drains preEqSpectrumFifo and runs the front-end, no detector
 * consumes its output.
 *
 *  1. EDITOR-CLOSED witness: frames flow with ZERO GUI involvement — this test
 *     never creates an editor and never calls SpectrumAnalyzer::processFFT().
 *     (The legacy analysis path is GUI-driven; the front-end must not be.)
 *  2. CPU in-vivo witness: per-frame cost measured on the REAL AI thread,
 *     logged and bounded (scorecard budget: mean < 5 ms/frame).
 *  3. Behavior guard: detection floors are covered by the existing suites; this
 *     test additionally asserts the wiring did not disturb the AI pipeline by
 *     checking the engine still produces no detections on near-silence.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>
#include <functional>

class AIFrontEndWiringTest : public juce::UnitTest
{
public:
    AIFrontEndWiringTest()
        : juce::UnitTest("AI Front-End Wiring (diagnostics-only)", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest("Front-end frames flow on the AI thread with no GUI (editor closed)");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSr, kBlock);

        // Deterministic playback: 200 blocks (~2.1 s) of sine+noise.
        juce::Random rng(9090);
        double phase = 0.0;
        const int numBlocks = 200;
        juce::MidiBuffer midi;
        for (int b = 0; b < numBlocks; ++b)
        {
            juce::AudioBuffer<float> buf(2, kBlock);
            for (int i = 0; i < kBlock; ++i)
            {
                const float s = 0.2f * static_cast<float>(std::sin(phase))
                              + 0.02f * (rng.nextFloat() * 2.0f - 1.0f);
                phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / kSr;
                buf.setSample(0, i, s);
                buf.setSample(1, i, s * 0.7f);
            }
            proc.processBlock(buf, midi);
        }

        // The drain runs on the AI thread (async): poll with a bounded timeout.
        const auto deadline = juce::Time::getMillisecondCounter() + 3000u;
        AIEqualizerAudioProcessor::FrontEndDiagnostics diag;
        const juce::int64 expectedFrames =
            (static_cast<juce::int64>(numBlocks) * kBlock - 4096) / 2048; // ~48
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            diag = proc.getAIFrontEndDiagnostics();
            if (diag.frames >= expectedFrames)
                break;
            juce::Thread::yield();
        }

        logMessage("  frames=" + juce::String(diag.frames)
                   + " (expected ~" + juce::String(expectedFrames) + ")"
                   + "  mean=" + juce::String(diag.meanMs, 3) + " ms/frame"
                   + "  max=" + juce::String(diag.maxMs, 3) + " ms/frame");

        expect(diag.frames >= expectedFrames,
               "Front-end produced too few frames with the editor closed: the AI-thread "
               "drain of preEqSpectrumFifo is not working (got "
               + juce::String(diag.frames) + ", expected >= " + juce::String(expectedFrames) + ").");

        beginTest("CPU in-vivo witness: per-frame cost within budget on the AI thread");
        expect(diag.frames > 0, "No frames - cannot measure CPU.");
        expect(diag.meanMs < 5.0,
               "Front-end mean frame cost on the AI thread exceeds the 5 ms budget ("
               + juce::String(diag.meanMs, 3) + " ms).");

        beginTest("EDITOR-OPEN witness: GUI consumer on preEq FIFO does not starve the front-end");
        // The GUI's NewSpectrumPipeline pulls preEqSpectrumFifo (PluginEditor.cpp).
        // The front-end has its own dedicated FIFO (P2C2.1), so a concurrent GUI
        // consumer must not steal its samples. Simulate the GUI reader with the
        // exact same call it uses (pullAudioBlock on getPreEqFifo) interleaved
        // with playback, and require the front-end still produce ~all frames.
        {
            AIEqualizerAudioProcessor proc2;
            proc2.prepareToPlay(kSr, kBlock);
            const auto base = proc2.getAIFrontEndDiagnostics().frames;
            std::vector<float> guiScratch(4096, 0.0f);
            juce::int64 guiPulled = 0;
            double ph = 0.0;
            for (int b = 0; b < numBlocks; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                for (int i = 0; i < kBlock; ++i)
                {
                    const float s = 0.2f * static_cast<float>(std::sin(ph));
                    ph += 2.0 * juce::MathConstants<double>::pi * 440.0 / kSr;
                    buf.setSample(0, i, s);
                    buf.setSample(1, i, s);
                }
                proc2.processBlock(buf, midi);
                // "GUI" drains preEq concurrently with the AI thread's own drain.
                guiPulled += static_cast<juce::int64>(
                    proc2.getPreEqFifo().pullAudioBlock(guiScratch.data(), guiScratch.size()));
            }
            const auto dl2 = juce::Time::getMillisecondCounter() + 3000u;
            AIEqualizerAudioProcessor::FrontEndDiagnostics d2;
            while (juce::Time::getMillisecondCounter() < dl2)
            {
                d2 = proc2.getAIFrontEndDiagnostics();
                if (d2.frames - base >= expectedFrames)
                    break;
                juce::Thread::yield();
            }
            logMessage("  with GUI consumer: frontend frames=" + juce::String(d2.frames)
                       + " (expected ~" + juce::String(expectedFrames) + ")"
                       + "  gui pulled=" + juce::String(guiPulled) + " samples");
            expect(guiPulled > 0, "Simulated GUI consumer received no samples from preEq FIFO.");
            expect(d2.frames >= expectedFrames,
                   "Front-end starved while a GUI consumer drains preEq: got "
                   + juce::String(d2.frames) + ", expected >= " + juce::String(expectedFrames)
                   + " (dedicated FIFO not isolating the readers?).");
        }

        beginTest("Wiring did not disturb the AI pipeline (no detections on near-silence)");
        // The blocks above are a clean 440 Hz tone + tiny noise: the engine may
        // legitimately detect or not depending on profile, so instead feed pure
        // near-silence and require zero pending corrections afterwards.
        for (int b = 0; b < 50; ++b)
        {
            juce::AudioBuffer<float> silent(2, kBlock);
            silent.clear();
            proc.processBlock(silent, midi);
        }
        // Give the AI thread a moment to settle (bounded).
        const auto settleDeadline = juce::Time::getMillisecondCounter() + 1000u;
        while (juce::Time::getMillisecondCounter() < settleDeadline)
            juce::Thread::yield();
        const auto pending = proc.getAIEngine().getPendingCorrections();
        juce::String det;
        for (const auto& c : pending)
            det += AIEngine::getProblemTypeName(c.type) + "@" + juce::String(c.frequency, 0) + " ";
        logMessage("  pending after silence: " + juce::String(static_cast<int>(pending.size()))
                   + (pending.empty() ? juce::String() : ("  [" + det + "]")));
        expect(pending.empty(),
               "Detections appeared on near-silence after the wiring: " + det);
    }
};

static AIFrontEndWiringTest sAIFrontEndWiringTest;
