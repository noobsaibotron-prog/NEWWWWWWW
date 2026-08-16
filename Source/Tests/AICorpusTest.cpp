/**
 * AICorpusTest — Roadmap v1, P1 (category "AI-Corpus").
 *
 * BASELINE-EMITTING harness over real rendered audio — not a final quality gate:
 *   1. proves the test-only LiveAIAnalysisPipeline matches the shipped
 *      PerceptualFrontEnd::rawDb representation exactly (so corpus results
 *      predict in-plugin behavior at the representation level);
 *   2. emits the corpus baseline scorecard (clip × backend × sensitivity, with
 *      LIVE semantics). Clips marked known_fail in the manifest are MEASURED
 *      gaps of the current detector (logged as KNOWN_FAIL, never asserted);
 *      hard assertions fire only for infrastructure failures and for clips the
 *      detector is already expected to handle;
 *   3. proves the whole audio→spectrum→detection chain is deterministic.
 *
 * The human-readable baseline + promotion criteria live in docs/AI_SCORECARD.md.
 * known_fail is recorded technical debt, not a masked pass: each entry carries
 * its reason and a roadmap pillar that is expected to fix it.
 *
 * Fixtures: TestAssets/ai_corpus (rendered by tools/make_fixtures.py, labels in
 * manifest.json). The directory is passed in via the AIEQ_CORPUS_DIR define.
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../AI/AIEngine.h"
#include "../AI/PerceptualFrontEnd.h"
#include "Support/LiveAIAnalysisPipeline.h"

namespace
{

constexpr double kCorpusSampleRate = 48000.0;

juce::File corpusDir()
{
#ifdef AIEQ_CORPUS_DIR
    return juce::File(AIEQ_CORPUS_DIR);
#else
    return juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
        .getChildFile("TestAssets/ai_corpus");
#endif
}

juce::File shippedWeights()
{
    return juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
        .getChildFile("Resources/Models/ml_weights.bin");
}

bool loadWavMono48k(const juce::File& f, juce::AudioBuffer<float>& out)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (reader == nullptr)
        return false;
    out.setSize(static_cast<int>(reader->numChannels),
                static_cast<int>(reader->lengthInSamples));
    reader->read(&out, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    return std::abs(reader->sampleRate - kCorpusSampleRate) < 1.0;
}

// Final detections after feeding every frame with LIVE semantics (force=false):
// the rate limiter and the temporal-persistence hysteresis are ACTIVE, exactly
// as in the running plugin. This is deliberate and differs from the synthetic
// AI-Sweep harness (force=true): a 6 s real clip has plenty of frames for the
// persistence layer, and what the corpus must measure is what the USER sees —
// frame-level flicker that persistence suppresses is not a user-facing FP.
// Determinism is preserved (no RNG in the live path; counters are deterministic).
struct CorpusResult
{
    int count = 0;
    juce::String details;
    std::vector<std::pair<juce::String, float>> found; // {typeName, frequency}

    /** Recall check: a detection of `type` within `tolCents` of `f0`. */
    bool detectionMatches(const juce::String& type, float f0, float tolCents) const
    {
        for (const auto& d : found)
            if (d.first == type && f0 > 0.0f && d.second > 0.0f
                && std::abs(1200.0f * std::log2(d.second / f0)) <= tolCents)
                return true;
        return false;
    }
};

CorpusResult runClipThroughEngine(const std::vector<std::vector<float>>& frames,
                                  AIEngine::DetectionBackendMode mode,
                                  float sensitivity,
                                  const juce::File& mlWeights)
{
    AIEngine ai;
    ai.prepare(kCorpusSampleRate, 512);
    ai.setEnabled(true);
    ai.setSourceProfile(AIEngine::SourceProfile::Generic);
    ai.setDetectionBackendMode(mode);
    ai.setSensitivity(sensitivity);
    if (mode != AIEngine::DetectionBackendMode::HeuristicOnly)
    {
        if (mlWeights.existsAsFile())
            ai.setCustomMLWeightsPathForTests(mlWeights);
        ai.forceMLDetectionEnabledForTests(true);
    }

    for (const auto& frame : frames)
        ai.analyzeSpectrum(frame, /*force=*/false);

    CorpusResult r;
    for (const auto& c : ai.getPendingCorrections())
    {
        ++r.count;
        const auto typeName = AIEngine::getProblemTypeName(c.type);
        r.found.push_back({ typeName, c.frequency });
        r.details += typeName + "@" + juce::String(c.frequency, 0)
                   + " c=" + juce::String(c.confidence, 2) + "  ";
    }
    return r;
}

} // anonymous namespace

class AICorpusTest : public juce::UnitTest
{
public:
    AICorpusTest() : juce::UnitTest("AI Corpus — fixtures, mirror & clean floor", "AI-Corpus") {}

    void runTest() override
    {
        // ============================================================
        // GATE 1 — mirror equivalence vs the REAL SpectrumAnalyzer
        // ============================================================
        beginTest("Corpus frontend equivalence vs PerceptualFrontEnd rawDb");

        // Deterministic stereo test signal: three sines + seeded noise,
        // different channel gains so the mono-sum is genuinely exercised.
        const int n = static_cast<int>(kCorpusSampleRate * 3.0);
        juce::AudioBuffer<float> sig(2, n);
        {
            juce::Random rng(4242);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / kCorpusSampleRate;
                const float s = 0.20f * std::sin(2.0 * juce::MathConstants<double>::pi * 100.0 * t)
                              + 0.15f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t)
                              + 0.10f * std::sin(2.0 * juce::MathConstants<double>::pi * 3200.0 * t)
                              + 0.02f * (rng.nextFloat() * 2.0f - 1.0f);
                sig.setSample(0, i, s);
                sig.setSample(1, i, s * 0.5f); // asymmetric channels -> mono-sum matters
            }
        }

        // Production representation, directly through PerceptualFrontEnd.
        std::vector<float> mono(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i)
            mono[static_cast<size_t>(i)] = 0.5f * (sig.getSample(0, i) + sig.getSample(1, i));

        PerceptualFrontEnd directFrontEnd;
        directFrontEnd.prepare(kCorpusSampleRate);
        const auto directFrames = directFrontEnd.analyzeAll(mono.data(), n);

        // Corpus wrapper path over the same stereo signal.
        aieq_test::LiveAIAnalysisPipeline mirror(kCorpusSampleRate);
        const auto mirrorFrames = mirror.analyzeRawFrames(sig);

        expect(!directFrames.empty(), "PerceptualFrontEnd produced no frames.");
        expectEquals(static_cast<int>(mirrorFrames.size()), static_cast<int>(directFrames.size()),
                     "Corpus frontend produced a different number of frames than PerceptualFrontEnd.");

        float maxDiff = 0.0f;
        const size_t framesToCompare = std::min(mirrorFrames.size(), directFrames.size());
        for (size_t f = 0; f < framesToCompare; ++f)
        {
            const size_t bins = std::min(mirrorFrames[f].size(), directFrames[f].rawDb.size());
            for (size_t i = 0; i < bins; ++i)
                maxDiff = std::max(maxDiff,
                                   std::abs(mirrorFrames[f][i] - directFrames[f].rawDb[i]));
        }
        logMessage("  frames=" + juce::String(static_cast<int>(framesToCompare))
                   + "  max |corpus - live frontend| = " + juce::String(maxDiff, 7) + " dB");
        expect(maxDiff <= 1.0e-6f,
               "Corpus frontend diverges from the shipped PerceptualFrontEnd rawDb representation.");

        // ============================================================
        // GATE 2 — corpus BASELINE scorecard (emits status per clip×backend×sens)
        //
        // HARD-FAIL only on infrastructure or on regressions the detector is
        // already expected to handle: missing/unreadable fixtures, or a clip NOT
        // marked known_fail behaving outside its expectation. Clips marked
        // known_fail in the manifest document MEASURED gaps of the CURRENT
        // detector (P1-GAP-001 recall miss, P1-GAP-002 dark-tilt heuristic FP):
        // they are logged as KNOWN_FAIL — never asserted — and fixing them is
        // roadmap work (P2/P4). Do NOT "fix" them by retuning fixtures.
        // ============================================================
        beginTest("Corpus baseline scorecard (hard gate only on non-known_fail clips)");

        const auto dir = corpusDir();
        const auto manifestFile = dir.getChildFile("manifest.json");
        expect(manifestFile.existsAsFile(),
               "Corpus manifest missing: " + manifestFile.getFullPathName()
               + " (run tools/make_fixtures.py)");

        const auto manifest = juce::JSON::parse(manifestFile.loadFileAsString());
        const auto* clips = manifest.getProperty("clips", {}).getArray();
        expect(clips != nullptr && !clips->isEmpty(), "Corpus manifest has no clips.");
        if (clips == nullptr)
            return;

        const auto mlWeights = shippedWeights();
        std::vector<std::vector<float>> labeledClipFrames; // reused by gate 3

        logMessage("  AI-Corpus baseline:");
        logMessage("  clip                 | expected  | backend | sens | detections                  | status");
        logMessage("  ---------------------+-----------+---------+------+-----------------------------+-------");

        for (const auto& clipVar : *clips)
        {
            const auto file = dir.getChildFile(clipVar.getProperty("file", "").toString());
            juce::AudioBuffer<float> audio;
            expect(loadWavMono48k(file, audio), "Cannot load corpus clip: " + file.getFullPathName());
            if (audio.getNumSamples() == 0)
                continue;

            aieq_test::LiveAIAnalysisPipeline pipe(kCorpusSampleRate);
            auto frames = pipe.analyze(audio);
            expect(!frames.empty(), "No analysis frames for clip: " + file.getFileName());

            const auto* problems   = clipVar.getProperty("problems", {}).getArray();
            const bool  isClean    = (problems == nullptr || problems->isEmpty());
            const bool  knownFail  = static_cast<bool>(clipVar.getProperty("known_fail", false));
            const auto  expectedStr = clipVar.getProperty("expected", "None").toString();

            // Expected problem (first label) for recall scoring.
            float expF0 = 0.0f, expTolCents = 0.0f;
            juce::String expType;
            if (!isClean)
            {
                const auto p = (*problems)[0];
                expType     = p.getProperty("type", "").toString();
                expF0       = static_cast<float>(static_cast<double>(p.getProperty("f0", 0.0)));
                expTolCents = static_cast<float>(static_cast<double>(p.getProperty("tolerance_cents", 300.0)));
            }

            for (const float sens : { 0.2f, 0.5f })
                for (const auto mode : { AIEngine::DetectionBackendMode::MLOnly,
                                         AIEngine::DetectionBackendMode::Hybrid })
                {
                    const auto r = runClipThroughEngine(frames, mode, sens, mlWeights);

                    bool pass = false;
                    if (isClean)
                        pass = (r.count == 0);
                    else
                        pass = r.detectionMatches(expType, expF0, expTolCents);

                    const juce::String status = pass ? "PASS"
                                              : (knownFail ? "KNOWN_FAIL" : "FAIL");
                    logMessage("  " + file.getFileName().paddedRight(' ', 21) + "| "
                               + expectedStr.paddedRight(' ', 10) + "| "
                               + (mode == AIEngine::DetectionBackendMode::MLOnly
                                      ? juce::String("ML     ") : juce::String("Hybrid "))
                               + "| " + juce::String(sens, 1) + "  | "
                               + (r.count == 0 ? juce::String("(none)") : r.details).paddedRight(' ', 28)
                               + "| " + status);

                    // HARD gate only on clips the detector is already expected to
                    // handle. known_fail clips are the measured baseline.
                    if (!knownFail)
                        expect(pass,
                               "Non-known_fail clip '" + file.getFileName() + "' failed at sens "
                               + juce::String(sens, 1) + ": " + r.details);
                }

            if (!isClean)
                labeledClipFrames = std::move(frames); // kept for gate 3
        }

        // ── P4-M1: real-audio SANITY roll-up (logging only, NO new hard gate) ──
        // AI-Corpus is the out-of-distribution real-audio SANITY gate, NOT the primary
        // per-class metric (that is the FROZEN synthetic held-out — AIAccuracyTest
        // namespace heldout / P4-D2 heldout_v2). The corpus is tiny today, so this
        // roll-up states coverage explicitly so the numbers are never over-read.
        {
            int nClips = 0, nClean = 0, nLabeled = 0;
            juce::StringArray coveredClasses;
            for (const auto& clipVar : *clips)
            {
                ++nClips;
                const auto* probs = clipVar.getProperty("problems", {}).getArray();
                if (probs == nullptr || probs->isEmpty()) { ++nClean; continue; }
                ++nLabeled;
                const auto t = (*probs)[0].getProperty("type", "").toString();
                if (t.isNotEmpty()) coveredClasses.addIfNotAlreadyThere(t);
            }
            logMessage("");
            logMessage("  AI-Corpus SANITY roll-up (real-audio, out-of-distribution; NOT a per-class metric):");
            logMessage("    clips=" + juce::String(nClips)
                       + "  labeled=" + juce::String(nLabeled)
                       + "  clean=" + juce::String(nClean)
                       + "  classes covered=" + (coveredClasses.isEmpty()
                                                 ? juce::String("(none)")
                                                 : coveredClasses.joinIntoString(",")));
            logMessage("    role: real-audio sanity only — per-class recall lives in the frozen synthetic");
            logMessage("    held-out (P4-D2 heldout_v2). Corpus expansion is out of scope for P4-M1.");
        }

        // ============================================================
        // GATE 3 — determinism across two identical runs
        // ============================================================
        beginTest("Corpus detection is deterministic across two identical runs");

        // NOTE: with a single labeled clip this covers the whole labeled set; when
        // more labeled clips are added, generalize to iterate every labeled clip
        // (cheap: same runClipThroughEngine twice per clip).
        expect(!labeledClipFrames.empty(), "No labeled clip available for the determinism gate.");
        if (!labeledClipFrames.empty())
        {
            const auto a = runClipThroughEngine(labeledClipFrames,
                                                AIEngine::DetectionBackendMode::Hybrid,
                                                0.5f, mlWeights);
            const auto b = runClipThroughEngine(labeledClipFrames,
                                                AIEngine::DetectionBackendMode::Hybrid,
                                                0.5f, mlWeights);
            logMessage("  run A: " + juce::String(a.count) + "  [" + a.details + "]");
            logMessage("  run B: " + juce::String(b.count) + "  [" + b.details + "]");
            expect(a.count == b.count && a.details == b.details,
                   "Two identical corpus runs produced different detections - the chain "
                   "is not deterministic.");
        }
    }
};

static AICorpusTest sAICorpusTest;
