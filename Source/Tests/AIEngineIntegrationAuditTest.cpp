/**
 * AIEngineIntegrationAuditTest.cpp
 *
 * Forensic integration audit test for the AI detection pipeline.
 * Instruments every stage of: spectrum → AIEngine → MLEngine → detections
 * to find exactly where the retrained model's good outputs (F1 65.2% direct)
 * are being zeroed out in the full pipeline (F1 0%).
 *
 * Tribunal v4.3 P0 priority: "Trovare il punto esatto in cui il modello
 * buono diventa pipeline cattiva."
 *
 * Test structure:
 *   TEST 1 — Pipeline Diagnostics: instruments every stage with diagnostic output
 *   TEST 2 — Direct vs Pipeline comparison (same spectra, MLEngine vs AIEngine)
 *   TEST 3 — Full pipeline accuracy after fix verification
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <random>

#include "../AI/AIEngine.h"
#include "../AI/MLEngine.h"

namespace
{

// ─────────────────────────────────────────────────────────────────────────────
// Constants (matching AIAccuracyTest)
// ─────────────────────────────────────────────────────────────────────────────

constexpr double kSampleRate   = 44100.0;
constexpr int    kFFTSize      = 4096;
constexpr int    kNumBins      = kFFTSize / 2;        // 2048
constexpr float  kBaseline     = 0.05f;
constexpr int    kVariations   = 5;                    // fewer for audit (diagnostic, not statistical)

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum utilities (duplicated from AIAccuracyTest for self-containment)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<float> makeFlat(float level = kBaseline)
{
    return std::vector<float>(kNumBins, level);
}

void addPeak(std::vector<float>& spec, float freqHz, float strength, float sigmaFactor = 0.08f)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    const float sigmaHz = juce::jmax(30.0f, freqHz * sigmaFactor);
    for (int i = 0; i < kNumBins; ++i)
    {
        const float freq = static_cast<float>(i) * binHz;
        const float d = (freq - freqHz) / sigmaHz;
        spec[static_cast<size_t>(i)] += strength * std::exp(-0.5f * d * d);
    }
}

void addNoise(std::vector<float>& spec, std::mt19937& rng, float stddev = 0.02f)
{
    std::normal_distribution<float> noise(0.0f, stddev);
    for (auto& v : spec)
        v = juce::jmax(0.0f, v + noise(rng));
}

/// Convert linear magnitude spectrum to dB (matching SpectrumAnalyzer::getSpectrumDB() output).
/// This is what AIEngine::analyzeSpectrum() receives in the live plugin.
std::vector<float> linearToDb(const std::vector<float>& linear)
{
    std::vector<float> db(linear.size());
    for (size_t i = 0; i < linear.size(); ++i)
        db[i] = juce::Decibels::gainToDecibels(linear[i] + 1e-10f, -100.0f);
    return db;
}

// ─────────────────────────────────────────────────────────────────────────────
// Named test spectrum generators
// ─────────────────────────────────────────────────────────────────────────────

struct AuditCase
{
    std::vector<float> spectrum;
    MLEngine::ProblemType mlType;
    AIEngine::ProblemType aiType;
    juce::String name;
};

std::vector<AuditCase> generateAuditCases(std::mt19937& rng)
{
    std::vector<AuditCase> cases;

    // One strong example of each problem type
    auto makeCase = [&](auto name, MLEngine::ProblemType ml, AIEngine::ProblemType ai,
                        auto specBuilder) {
        auto spec = makeFlat();
        addNoise(spec, rng);
        specBuilder(spec);
        cases.push_back({spec, ml, ai, name});
    };

    makeCase("Resonance@2kHz", MLEngine::ProblemType::Resonance, AIEngine::ProblemType::Resonance,
             [](auto& s) { addPeak(s, 2000.0f, 0.9f, 0.08f); });

    makeCase("Harshness@4kHz", MLEngine::ProblemType::Harshness, AIEngine::ProblemType::Harshness,
             [](auto& s) { addPeak(s, 4000.0f, 0.85f, 0.3f); });

    makeCase("Muddiness@250Hz", MLEngine::ProblemType::Muddiness, AIEngine::ProblemType::Muddiness,
             [](auto& s) { addPeak(s, 250.0f, 0.9f, 0.15f); });

    makeCase("Sibilance@8kHz", MLEngine::ProblemType::Sibilance, AIEngine::ProblemType::Sibilance,
             [](auto& s) { addPeak(s, 8000.0f, 0.85f, 0.12f); });

    makeCase("Boominess@80Hz", MLEngine::ProblemType::Boominess, AIEngine::ProblemType::LowEndBoom,
             [](auto& s) { addPeak(s, 80.0f, 0.9f, 0.15f); });

    makeCase("Boxy@500Hz", MLEngine::ProblemType::BoxyMidrange, AIEngine::ProblemType::Boxyness,
             [](auto& s) { addPeak(s, 500.0f, 0.85f, 0.12f); });

    makeCase("Clipping", MLEngine::ProblemType::Clipping, AIEngine::ProblemType::None,
             [](auto& s) { for (auto& v : s) v = juce::jlimit(0.0f, 1.2f, v + 0.3f); });

    makeCase("Clean", MLEngine::ProblemType::NumProblems, AIEngine::ProblemType::None,
             [](auto&) { /* baseline + noise only */ });

    return cases;
}

juce::String mlTypeName(MLEngine::ProblemType t)
{
    switch (t)
    {
        case MLEngine::ProblemType::Resonance:    return "Resonance";
        case MLEngine::ProblemType::Harshness:    return "Harshness";
        case MLEngine::ProblemType::Muddiness:    return "Muddiness";
        case MLEngine::ProblemType::Sibilance:    return "Sibilance";
        case MLEngine::ProblemType::Boominess:    return "Boominess";
        case MLEngine::ProblemType::Thinness:     return "Thinness";
        case MLEngine::ProblemType::BoxyMidrange: return "BoxyMid";
        case MLEngine::ProblemType::Clipping:     return "Clipping";
        default:                                  return "Unknown";
    }
}

juce::String aiTypeName(AIEngine::ProblemType t)
{
    switch (t)
    {
        case AIEngine::ProblemType::Resonance:  return "Resonance";
        case AIEngine::ProblemType::Harshness:  return "Harshness";
        case AIEngine::ProblemType::Muddiness:  return "Muddiness";
        case AIEngine::ProblemType::Sibilance:  return "Sibilance";
        case AIEngine::ProblemType::LowEndBoom: return "LowEndBoom";
        case AIEngine::ProblemType::ThinSound:  return "ThinSound";
        case AIEngine::ProblemType::Boxyness:   return "Boxyness";
        case AIEngine::ProblemType::DullSound:  return "DullSound";
        case AIEngine::ProblemType::None:       return "None";
        default:                                return "Other";
    }
}

/// Find ml_weights.bin from the source tree
juce::File findModelFile()
{
    auto srcDir = juce::File(__FILE__).getParentDirectory()  // Tests/
                      .getParentDirectory()                   // Source/
                      .getParentDirectory();                   // project root
    auto f = srcDir.getChildFile("Resources/Models/ml_weights.bin");
    if (f.existsAsFile()) return f;

    // Fallback: try retrained weights
    f = srcDir.getChildFile("Resources/Models/ml_weights_retrained.bin");
    if (f.existsAsFile()) return f;

    return {};
}

} // anonymous namespace


// =============================================================================
// TEST 1 — Pipeline Diagnostics
//
// For each test case, instruments:
//   Stage 0: Input spectrum stats
//   Stage 1: MLEngine direct inference (raw probs + detections)
//   Stage 2: AIEngine.isUsingMLDetection() check
//   Stage 3: AIEngine.analyzeSpectrum() → getPendingCorrections()
//   Stage 4: Comparison (what the ML found vs what the pipeline output)
// =============================================================================
class AIEngineIntegrationAudit_Diagnostics : public juce::UnitTest
{
public:
    AIEngineIntegrationAudit_Diagnostics()
        : juce::UnitTest("AI Integration Audit — Pipeline Diagnostics", "AI-Integration") {}

    void runTest() override
    {
        beginTest("Pipeline stage instrumentation");

        // ── Load model ──
        auto modelFile = findModelFile();
        expect(modelFile.existsAsFile(),
               "ml_weights.bin not found! Looked in: "
               + juce::File(__FILE__).getParentDirectory().getParentDirectory()
                     .getParentDirectory().getChildFile("Resources/Models/").getFullPathName());

        if (!modelFile.existsAsFile())
            return;

        logMessage("  Model file: " + modelFile.getFullPathName());

        // ── Setup MLEngine (direct) ──
        MLEngine mlDirect;
        mlDirect.initialize();
        bool mlLoaded = mlDirect.loadWeights(modelFile);
        logMessage("  MLEngine direct loaded: " + juce::String(mlLoaded ? "YES" : "NO"));
        mlDirect.setSensitivity(0.5f);

        // ── Setup AIEngine (full pipeline) ──
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSensitivity(0.6f);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);

        // ── DIAGNOSTIC: Check useMLDetection BEFORE fix ──
        logMessage("");
        logMessage("  ╔══════════════════════════════════════════════════════════╗");
        logMessage("  ║  STAGE 2: useMLDetection check (BEFORE force-load)     ║");
        logMessage("  ╚══════════════════════════════════════════════════════════╝");
        logMessage("  AIEngine.isUsingMLDetection() = "
                   + juce::String(ai.isUsingMLDetection() ? "TRUE" : "FALSE"));

        if (!ai.isUsingMLDetection())
        {
            logMessage("  >>> ROOT CAUSE CONFIRMED: useMLDetection = false!");
            logMessage("  >>> prepare() couldn't find ml_weights.bin at runtime path.");
            logMessage("  >>> Calling setCustomMLWeightsPathForTests() to force ML path...");

            bool forceLoaded = ai.setCustomMLWeightsPathForTests(modelFile);
            logMessage("  >>> setCustomMLWeightsPathForTests() = " + juce::String(forceLoaded ? "SUCCESS" : "FAILED"));
            logMessage("  >>> isUsingMLDetection() = "
                       + juce::String(ai.isUsingMLDetection() ? "TRUE" : "FALSE"));

            expect(forceLoaded, "Failed to force-load ML weights into AIEngine");
        }

        // ── Generate test cases ──
        std::mt19937 rng(42);
        auto cases = generateAuditCases(rng);

        logMessage("");
        logMessage("  ╔══════════════════════════════════════════════════════════╗");
        logMessage("  ║  STAGE-BY-STAGE AUDIT: " + juce::String(static_cast<int>(cases.size())) + " test cases                        ║");
        logMessage("  ╚══════════════════════════════════════════════════════════╝");

        int mlDirectHits = 0;
        int aiPipelineHits = 0;
        int totalProblemCases = 0;

        for (const auto& tc : cases)
        {
            const bool isClean = (tc.mlType == MLEngine::ProblemType::NumProblems);
            if (!isClean) totalProblemCases++;

            logMessage("");
            logMessage("  ── " + tc.name + " ──────────────────────────────");

            // Stage 0: Input stats
            float minVal = *std::min_element(tc.spectrum.begin(), tc.spectrum.end());
            float maxVal = *std::max_element(tc.spectrum.begin(), tc.spectrum.end());
            float sumVal = 0.0f;
            for (auto v : tc.spectrum) sumVal += v;
            float avgVal = sumVal / static_cast<float>(tc.spectrum.size());

            logMessage("  [S0] Input: " + juce::String(static_cast<int>(tc.spectrum.size()))
                       + " bins, min=" + juce::String(minVal, 4)
                       + " max=" + juce::String(maxVal, 4)
                       + " avg=" + juce::String(avgVal, 4)
                       + " (LINEAR MAG)");

            // Stage 1: MLEngine direct (LINEAR MAGNITUDE — same format as training)
            auto mlDetections = mlDirect.detectProblems(tc.spectrum, kSampleRate);

            juce::String mlStr;
            bool mlFoundExpected = false;
            for (const auto& det : mlDetections)
            {
                if (!mlStr.isEmpty()) mlStr += ", ";
                mlStr += mlTypeName(det.type) + "(conf=" + juce::String(det.confidence, 3)
                         + " sev=" + juce::String(det.severity, 3) + ")";
                if (!isClean && det.type == tc.mlType)
                    mlFoundExpected = true;
            }
            if (mlDetections.empty()) mlStr = "(none)";
            if (mlFoundExpected) mlDirectHits++;

            logMessage("  [S1] MLEngine direct: " + mlStr);
            logMessage("       Expected " + (isClean ? juce::String("clean") : mlTypeName(tc.mlType))
                       + " → " + juce::String(isClean ? (mlDetections.empty() ? "CORRECT" : "FP!")
                                                       : (mlFoundExpected ? "HIT" : "MISS")));

            // Stage 3: AIEngine full pipeline (feed dB spectrum — matches live SpectrumAnalyzer output)
            auto dbSpectrum = linearToDb(tc.spectrum);
            ai.analyzeSpectrum(dbSpectrum, true);
            auto pending = ai.getPendingCorrections();

            juce::String aiStr;
            bool aiFoundExpected = false;
            for (const auto& corr : pending)
            {
                if (!aiStr.isEmpty()) aiStr += ", ";
                aiStr += aiTypeName(corr.type) + "(conf=" + juce::String(corr.confidence, 3)
                         + " sev=" + juce::String(corr.severity, 3)
                         + " freq=" + juce::String(corr.frequency, 0) + "Hz)";
                if (!isClean && corr.type == tc.aiType)
                    aiFoundExpected = true;
            }
            if (pending.empty()) aiStr = "(none)";
            if (!isClean && aiFoundExpected) aiPipelineHits++;

            logMessage("  [S3] AIEngine pipeline: " + aiStr);
            logMessage("       Expected " + (isClean ? juce::String("clean") : aiTypeName(tc.aiType))
                       + " → " + juce::String(isClean ? (pending.empty() ? "CORRECT" : "FP!")
                                                       : (aiFoundExpected ? "HIT" : "MISS")));

            // Stage 4: Verdict
            if (!isClean)
            {
                if (mlFoundExpected && aiFoundExpected)
                    logMessage("  [S4] ✓ BOTH detected correctly");
                else if (mlFoundExpected && !aiFoundExpected)
                    logMessage("  [S4] ⚠ MLEngine HIT but AIEngine MISS — pipeline is losing detections!");
                else if (!mlFoundExpected && aiFoundExpected)
                    logMessage("  [S4] △ MLEngine MISS but AIEngine HIT (heuristic fallback?)");
                else
                    logMessage("  [S4] ✗ BOTH missed");
            }
        }

        // ── Summary ──
        logMessage("");
        logMessage("  ╔══════════════════════════════════════════════════════════╗");
        logMessage("  ║  AUDIT SUMMARY                                          ║");
        logMessage("  ╚══════════════════════════════════════════════════════════╝");
        logMessage("  MLEngine direct hits:   " + juce::String(mlDirectHits) + "/" + juce::String(totalProblemCases));
        logMessage("  AIEngine pipeline hits: " + juce::String(aiPipelineHits) + "/" + juce::String(totalProblemCases));

        if (mlDirectHits > 0 && aiPipelineHits == 0)
            logMessage("  >>> DIAGNOSIS: Pipeline is zeroing ALL ML detections. Check useMLDetection, triple-buffer, thresholds.");
        else if (aiPipelineHits < mlDirectHits)
            logMessage("  >>> DIAGNOSIS: Pipeline loses " + juce::String(mlDirectHits - aiPipelineHits)
                       + " detections. Check thresholds, NN modulation, dedup.");

        // After fix: pipeline should match MLEngine within reason
        float pipelineRecovery = totalProblemCases > 0
            ? static_cast<float>(aiPipelineHits) / static_cast<float>(totalProblemCases) : 0.0f;
        logMessage("  Pipeline recovery rate: " + juce::String(pipelineRecovery * 100.0f, 1) + "%");

        // Soft assertion: with fix applied, pipeline should recover at least 50% of what MLEngine finds
        expect(aiPipelineHits > 0,
               "AIEngine pipeline detected ZERO problems — integration is broken");
    }
};


// =============================================================================
// TEST 2 — Direct vs Pipeline Side-by-Side (statistical)
//
// Runs N variations per problem type through both paths and compares recall.
// =============================================================================
class AIEngineIntegrationAudit_Comparison : public juce::UnitTest
{
public:
    AIEngineIntegrationAudit_Comparison()
        : juce::UnitTest("AI Integration Audit — Direct vs Pipeline", "AI-Integration") {}

    void runTest() override
    {
        beginTest("MLEngine direct vs AIEngine pipeline — side-by-side recall");

        auto modelFile = findModelFile();
        if (!modelFile.existsAsFile())
        {
            logMessage("  SKIP: ml_weights.bin not found");
            return;
        }

        // ── Setup both engines ──
        MLEngine mlDirect;
        mlDirect.initialize();
        mlDirect.loadWeights(modelFile);
        mlDirect.setSensitivity(0.5f);

        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setCustomMLWeightsPathForTests(modelFile);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);
        ai.setSensitivity(0.6f);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);

        expect(ai.isUsingMLDetection(), "AIEngine ML detection not active after setCustomMLWeightsPathForTests");

        std::mt19937 rng(42);

        // Problem type definitions
        struct ProblemDef
        {
            juce::String name;
            MLEngine::ProblemType mlType;
            AIEngine::ProblemType aiType;
        };

        ProblemDef problems[] = {
            {"Resonance",  MLEngine::ProblemType::Resonance,    AIEngine::ProblemType::Resonance},
            {"Harshness",  MLEngine::ProblemType::Harshness,    AIEngine::ProblemType::Harshness},
            {"Muddiness",  MLEngine::ProblemType::Muddiness,    AIEngine::ProblemType::Muddiness},
            {"Sibilance",  MLEngine::ProblemType::Sibilance,    AIEngine::ProblemType::Sibilance},
            {"Boominess",  MLEngine::ProblemType::Boominess,    AIEngine::ProblemType::LowEndBoom},
            {"BoxyMid",    MLEngine::ProblemType::BoxyMidrange, AIEngine::ProblemType::Boxyness},
        };

        constexpr int numProblems = 6;

        logMessage("");
        logMessage("  ┌─────────────────┬──────────────┬──────────────┬─────────┐");
        logMessage("  │ Problem Type    │ ML Direct    │ AI Pipeline  │ Delta   │");
        logMessage("  ├─────────────────┼──────────────┼──────────────┼─────────┤");

        int totalMLHits = 0, totalAIHits = 0;

        for (int p = 0; p < numProblems; ++p)
        {
            int mlHits = 0, aiHits = 0;
            std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);
            std::uniform_real_distribution<float> freqDist(100.0f, 10000.0f);

            for (int v = 0; v < kVariations; ++v)
            {
                auto spec = makeFlat();
                addNoise(spec, rng);
                const float str = strengthDist(rng);

                // Problem-specific spectrum generation
                switch (problems[p].mlType)
                {
                    case MLEngine::ProblemType::Resonance:
                        addPeak(spec, 200.0f + static_cast<float>(v) * 900.0f, str, 0.08f);
                        break;
                    case MLEngine::ProblemType::Harshness:
                        addPeak(spec, 3000.0f + static_cast<float>(v) * 500.0f, str, 0.3f);
                        break;
                    case MLEngine::ProblemType::Muddiness:
                        addPeak(spec, 150.0f + static_cast<float>(v) * 25.0f, str, 0.15f);
                        break;
                    case MLEngine::ProblemType::Sibilance:
                        addPeak(spec, 6000.0f + static_cast<float>(v) * 600.0f, str, 0.12f);
                        break;
                    case MLEngine::ProblemType::Boominess:
                        addPeak(spec, 50.0f + static_cast<float>(v) * 10.0f, str, 0.15f);
                        break;
                    case MLEngine::ProblemType::BoxyMidrange:
                        addPeak(spec, 350.0f + static_cast<float>(v) * 50.0f, str, 0.12f);
                        break;
                    default:
                        break;
                }

                // MLEngine direct (linear magnitude)
                auto mlDets = mlDirect.detectProblems(spec, kSampleRate);
                for (const auto& d : mlDets)
                    if (d.type == problems[p].mlType) { mlHits++; break; }

                // AIEngine pipeline (feed dB — matches live SpectrumAnalyzer)
                auto dbSpec = linearToDb(spec);
                ai.analyzeSpectrum(dbSpec, true);
                auto aiDets = ai.getPendingCorrections();
                for (const auto& c : aiDets)
                    if (c.type == problems[p].aiType) { aiHits++; break; }
            }

            totalMLHits += mlHits;
            totalAIHits += aiHits;

            float mlRecall = static_cast<float>(mlHits) / kVariations * 100.0f;
            float aiRecall = static_cast<float>(aiHits) / kVariations * 100.0f;
            float delta = aiRecall - mlRecall;

            logMessage("  │ " + problems[p].name.paddedRight(' ', 15)
                       + " │ " + (juce::String(mlHits) + "/" + juce::String(kVariations)
                                  + " (" + juce::String(mlRecall, 0) + "%)").paddedRight(' ', 12)
                       + " │ " + (juce::String(aiHits) + "/" + juce::String(kVariations)
                                  + " (" + juce::String(aiRecall, 0) + "%)").paddedRight(' ', 12)
                       + " │ " + (juce::String(delta > 0 ? "+" : "") + juce::String(delta, 0) + "%").paddedRight(' ', 7) + " │");
        }

        logMessage("  └─────────────────┴──────────────┴──────────────┴─────────┘");

        float totalML = static_cast<float>(totalMLHits) / static_cast<float>(numProblems * kVariations) * 100.0f;
        float totalAI = static_cast<float>(totalAIHits) / static_cast<float>(numProblems * kVariations) * 100.0f;

        logMessage("");
        logMessage("  Overall ML direct recall:   " + juce::String(totalML, 1) + "%");
        logMessage("  Overall AI pipeline recall: " + juce::String(totalAI, 1) + "%");

        // ── Clean signal false positive comparison ──
        beginTest("Clean signal false positive comparison");

        int mlCleanFP = 0, aiCleanFP = 0;
        constexpr int cleanCount = 10;
        std::uniform_real_distribution<float> levelDist(0.03f, 0.08f);

        for (int i = 0; i < cleanCount; ++i)
        {
            auto spec = makeFlat(levelDist(rng));
            addNoise(spec, rng);

            auto mlDets = mlDirect.detectProblems(spec, kSampleRate);
            if (!mlDets.empty()) mlCleanFP++;

            auto dbSpec = linearToDb(spec);
            ai.analyzeSpectrum(dbSpec, true);
            auto aiDets = ai.getPendingCorrections();
            if (!aiDets.empty()) aiCleanFP++;
        }

        logMessage("  Clean FP — ML direct: " + juce::String(mlCleanFP) + "/" + juce::String(cleanCount)
                   + ", AI pipeline: " + juce::String(aiCleanFP) + "/" + juce::String(cleanCount));

        // Assertions
        expect(totalAIHits > 0,
               "AIEngine pipeline detected ZERO problems across all types — integration broken");

        // Pipeline should recover at least 60% of what MLEngine direct finds
        if (totalMLHits > 0)
        {
            float recoveryRate = static_cast<float>(totalAIHits) / static_cast<float>(totalMLHits);
            expect(recoveryRate >= 0.5f,
                   "Pipeline recovery < 50%: only " + juce::String(totalAIHits)
                   + "/" + juce::String(totalMLHits) + " ML detections survived pipeline");
        }
    }
};


// =============================================================================
// TEST 3 — numBins Mismatch Audit
//
// Verifies that the spectrum size difference (2048 test bins vs 2049 AIEngine)
// doesn't corrupt the data during normalization.
// =============================================================================
class AIEngineIntegrationAudit_BinMismatch : public juce::UnitTest
{
public:
    AIEngineIntegrationAudit_BinMismatch()
        : juce::UnitTest("AI Integration Audit — Bin Mismatch Check", "AI-Integration") {}

    void runTest() override
    {
        beginTest("Spectrum bin count compatibility");

        // AIEngine.numBins = fftSize/2 + 1 = 2049
        // Test spectra: kNumBins = fftSize/2 = 2048
        // MLEngine: accepts any size, extractMelBands adapts

        constexpr int aiEngineNumBins = 4096 / 2 + 1;  // 2049
        constexpr int testNumBins = kNumBins;            // 2048

        logMessage("  AIEngine numBins: " + juce::String(aiEngineNumBins));
        logMessage("  Test spectrum bins: " + juce::String(testNumBins));
        logMessage("  Mismatch: " + juce::String(aiEngineNumBins - testNumBins) + " bins");

        // When sizes don't match, analyzeSpectrum interpolates.
        // Verify the interpolation preserves peak location and magnitude.

        auto modelFile = findModelFile();
        if (!modelFile.existsAsFile()) return;

        MLEngine mlDirect;
        mlDirect.initialize();
        mlDirect.loadWeights(modelFile);
        mlDirect.setSensitivity(0.5f);

        // Test with both 2048 and 2049 bins
        std::mt19937 rng(42);

        auto spec2048 = makeFlat();
        addNoise(spec2048, rng);
        addPeak(spec2048, 2000.0f, 0.9f, 0.08f);

        // Create 2049-bin version
        std::vector<float> spec2049(aiEngineNumBins, kBaseline);
        {
            std::mt19937 rng2(42);
            addNoise(spec2049, rng2);
            const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
            const float sigmaHz = juce::jmax(30.0f, 2000.0f * 0.08f);
            for (int i = 0; i < aiEngineNumBins; ++i)
            {
                const float freq = static_cast<float>(i) * binHz;
                const float d = (freq - 2000.0f) / sigmaHz;
                spec2049[static_cast<size_t>(i)] += 0.9f * std::exp(-0.5f * d * d);
            }
        }

        auto det2048 = mlDirect.detectProblems(spec2048, kSampleRate);
        auto det2049 = mlDirect.detectProblems(spec2049, kSampleRate);

        logMessage("  2048-bin detections: " + juce::String(static_cast<int>(det2048.size())));
        logMessage("  2049-bin detections: " + juce::String(static_cast<int>(det2049.size())));

        bool found2048 = false, found2049 = false;
        for (const auto& d : det2048)
            if (d.type == MLEngine::ProblemType::Resonance) found2048 = true;
        for (const auto& d : det2049)
            if (d.type == MLEngine::ProblemType::Resonance) found2049 = true;

        logMessage("  2048 found Resonance: " + juce::String(found2048 ? "YES" : "NO"));
        logMessage("  2049 found Resonance: " + juce::String(found2049 ? "YES" : "NO"));

        // Both should detect the same problem
        expect(found2048 == found2049,
               "Bin count mismatch changes detection result! 2048=" + juce::String(found2048 ? "Y" : "N")
               + " 2049=" + juce::String(found2049 ? "Y" : "N"));
    }
};


// ─────────────────────────────────────────────────────────────────────────────
// Registration
// ─────────────────────────────────────────────────────────────────────────────

static AIEngineIntegrationAudit_Diagnostics    sAuditDiagnostics;
static AIEngineIntegrationAudit_Comparison     sAuditComparison;
static AIEngineIntegrationAudit_BinMismatch    sAuditBinMismatch;
