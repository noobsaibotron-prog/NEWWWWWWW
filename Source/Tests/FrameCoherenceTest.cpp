/**
 * FrameCoherenceTest — strict contract tests for B2 (frame-coherent heuristic pass,
 * cherry-picked from ai/evolution-phase1 23f68cdd).
 *
 * B2 property: detectProblems() captures ONE consuming readSpectrumSnapshot() at the
 * top of the pass and hands that frame to every heuristic detector and helper. Before
 * B2 each detector AND each helper did its own CONSUMING triple-buffer read, so a
 * concurrent consumer swap (the GUI reads the same triple-buffer) could hand two
 * helpers two DIFFERENT frames inside one decision — the heuristic twin of P4-BUG-001.
 *
 *  T1 CONTRACT — on a clean flat frame (zero detections, so the documented
 *     out-of-scope FASE-2 confidence modifiers never run) one heuristic
 *     analyzeSpectrum() performs EXACTLY TWO consuming snapshot reads:
 *       1. detectProblems() B2 frame capture (shared by all detectors/helpers);
 *       2. detectGenre() (unconditional, self-contained single read — out of B2
 *          scope, no mixing possible within its own decision).
 *     Pre-B2 the detectors/helpers ALONE added ~8+ further reads. If anyone
 *     reintroduces a per-detector/per-helper read, the count rises and this goes RED.
 *
 *  T2 TORN-FRAME CANARY — two FLAT frames (-30 dB / -75 dB) are pure-silent for every
 *     detector (no peaks, no relative excess — asserted as baseline first). The main
 *     thread alternates them through analyzeSpectrum while a GUI-like consumer storm
 *     hammers consuming snapshot reads: pre-B2, helpers re-reading mid-pass could mix
 *     the two generations and manufacture phantom relative energies (tens of dB);
 *     frame-coherent reads must stay silent on every pass.
 */
#include <juce_core/juce_core.h>
#include "../AI/AIEngine.h"
#include <atomic>
#include <thread>
#include <vector>

namespace
{
constexpr int kNumBins = 2049; // fftSize/2 + 1 (same convention as AIEngineTest)

std::vector<float> flatSpectrum(float levelDb)
{
    return std::vector<float>(kNumBins, levelDb);
}
} // namespace

class FrameCoherenceTest final : public juce::UnitTest
{
public:
    FrameCoherenceTest()
        : juce::UnitTest("Frame coherence — one snapshot per heuristic pass", "AI-Contract") {}

    void runTest() override
    {
        beginTest("T1: clean heuristic pass consumes exactly TWO snapshots (detectProblems + detectGenre)");
        {
            AIEngine ai;
            ai.prepare(44100.0, 512);
            ai.setEnabled(true);
            ai.setSensitivity(0.5f);
            ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::HeuristicOnly);

            const auto clean = flatSpectrum(-60.0f);
            ai.analyzeSpectrum(clean, true); // warm-up: publishes + first full pass

            ai.resetSnapshotReadCountForTests();
            ai.analyzeSpectrum(clean, true);
            expectEquals(ai.getSnapshotReadCountForTests(), 2,
                         "clean heuristic pass budget: 1 detectProblems frame capture + 1 detectGenre "
                         "(pre-B2 the detectors/helpers alone added ~8+ more reads)");
            expect(ai.getPendingCorrections().empty(), "flat -60 dB frame must be silent");
        }

        beginTest("T2 baseline: both storm frames are pure-silent");
        AIEngine ai;
        ai.prepare(44100.0, 512);
        ai.setEnabled(true);
        ai.setSensitivity(0.5f);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::HeuristicOnly);

        const auto loud  = flatSpectrum(-30.0f);
        const auto quiet = flatSpectrum(-75.0f);
        {
            ai.analyzeSpectrum(loud, true);
            expect(ai.getPendingCorrections().empty(), "pure flat -30 dB frame must be silent");
            ai.analyzeSpectrum(quiet, true);
            expect(ai.getPendingCorrections().empty(), "pure flat -75 dB frame must be silent");
        }

        beginTest("T2: consumer storm cannot manufacture phantom detections (torn frames)");
        {
            std::atomic<bool> stop { false };
            std::thread consumerStorm([&ai, &stop]
            {
                // GUI-twin: consuming snapshot swaps racing the heuristic pass.
                while (! stop.load(std::memory_order_relaxed))
                    (void) ai.probeSnapshotVersionForTests();
            });

            int phantomPasses = 0;
            for (int pass = 0; pass < 400; ++pass)
            {
                ai.analyzeSpectrum((pass & 1) != 0 ? loud : quiet, true);
                if (! ai.getPendingCorrections().empty())
                    ++phantomPasses;
            }

            stop.store(true, std::memory_order_relaxed);
            consumerStorm.join();

            expectEquals(phantomPasses, 0,
                         "mixed-generation reads inside one pass manufactured phantom detections");
        }
    }
};

static FrameCoherenceTest frameCoherenceTest;
