/**
 * AIFrontEndWiringTest — AI-owned PerceptualFrontEnd wiring regression.
 *
 * EC-001/B4 promotes this path from diagnostics-only to the production
 * continuous-detection source. The test remains focused on SPSC isolation and
 * in-vivo frontend CPU; detector/editor parity is covered separately by
 * AIHeadlessDetectionParityTest.
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

namespace
{
/** Resolve the shipped ML weights from the source tree and force them into the
    engine. The test binary has no models/ folder beside it, so prepare() leaves
    the ML path unavailable and AIEngine silently falls back to the heuristic
    detector — a backend the product does not ship. The same __FILE__-relative
    idiom is already used by AIBackendSweepTest, AICorpusTest and the other AI
    tests that need the real network.

    Returns false if the ML path could not be made live, so the caller fails the
    test instead of quietly measuring the wrong detector. */
[[nodiscard]] inline bool forceShippedMLWeights(AIEqualizerAudioProcessor& proc,
                                                juce::String& detail)
{
    const juce::File weights =
        juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");

    if (! weights.existsAsFile())
    {
        detail = "Resources/Models/ml_weights.bin is missing from the source tree ("
               + weights.getFullPathName() + ")";
        return false;
    }

    proc.getAIEngine().setCustomMLWeightsPathForTests(weights);

    const auto status = proc.getAIEngine().getMLBackendStatus();
    if (status != AIEngine::MLBackendStatus::Active)
    {
        detail = "ML backend is " + AIEngine::getMLBackendStatusName(status)
               + " after loading the shipped weights. This test asserts the shipping "
                 "detection path, so it must fail here rather than exercise the "
                 "heuristic fallback.";
        return false;
    }

    detail = "ML backend Active (shipped ml_weights.bin)";
    return true;
}
} // namespace

class AIFrontEndWiringTest : public juce::UnitTest
{
public:
    AIFrontEndWiringTest()
        : juce::UnitTest("AI Front-End Wiring (production headless)", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest("Front-end frames flow on the AI thread with no GUI (editor closed)");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSr, kBlock);
        {
            juce::String mlDetail;
            const bool mlLive = forceShippedMLWeights(proc, mlDetail);
            logMessage("  " + mlDetail);
            expect(mlLive, mlDetail);
            if (! mlLive) return;
        }

        // Deterministic playback: 200 blocks (~2.1 s) of sine+noise.
        // Keep the synthetic producer bounded relative to the async consumer so
        // this test measures wiring rather than deliberate FIFO overrun.
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

            if ((b + 1) % 8 == 0)
            {
                const juce::int64 samplesFed = static_cast<juce::int64>(b + 1) * kBlock;
                const juce::int64 wanted = samplesFed < PerceptualFrontEnd::kFftSize
                    ? 0
                    : 1 + (samplesFed - PerceptualFrontEnd::kFftSize)
                          / PerceptualFrontEnd::kHopSize;
                const auto catchupDeadline = juce::Time::getMillisecondCounter() + 1000u;
                while (juce::Time::getMillisecondCounter() < catchupDeadline
                       && proc.getAIFrontEndDiagnostics().frames < wanted)
                    juce::Thread::yield();
            }
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
            {
                juce::String mlDetail;
                const bool mlLive = forceShippedMLWeights(proc2, mlDetail);
                logMessage("  " + mlDetail);
                expect(mlLive, mlDetail);
                if (! mlLive) return;
            }
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

                if ((b + 1) % 8 == 0)
                {
                    const juce::int64 samplesFed = static_cast<juce::int64>(b + 1) * kBlock;
                    const juce::int64 wanted = samplesFed < PerceptualFrontEnd::kFftSize
                        ? 0
                        : 1 + (samplesFed - PerceptualFrontEnd::kFftSize)
                              / PerceptualFrontEnd::kHopSize;
                    const auto catchupDeadline = juce::Time::getMillisecondCounter() + 1000u;
                    while (juce::Time::getMillisecondCounter() < catchupDeadline
                           && proc2.getAIFrontEndDiagnostics().frames < wanted)
                        juce::Thread::yield();
                }
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

        beginTest("Production headless path clears detections on sustained silence");
        // The AIEngine persistence layer is effectively slower than the old
        // 0.8 s comment because its internal every-third-frame throttle remains
        // in place. Feed >3 s of digital zero so this is a real detect->clear
        // witness rather than a stale-history timing accident.
        const auto silenceBaseFrames = proc.getAIFrontEndDiagnostics().frames;
        for (int b = 0; b < 320; ++b)
        {
            juce::AudioBuffer<float> silent(2, kBlock);
            silent.clear();
            proc.processBlock(silent, midi);
            if ((b + 1) % 8 == 0)
            {
                const auto wanted = silenceBaseFrames
                    + (static_cast<juce::int64>(b + 1) * kBlock) / PerceptualFrontEnd::kHopSize;
                const auto deadline = juce::Time::getMillisecondCounter() + 1000u;
                while (juce::Time::getMillisecondCounter() < deadline
                       && proc.getAIFrontEndDiagnostics().frames < wanted)
                    juce::Thread::yield();
            }
        }
        expectEquals(proc.getAIFrontEndDiagnostics().droppedSamples, static_cast<juce::int64>(0),
                     "Bounded realtime-style witness overflowed the AI frontend FIFO.");
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
