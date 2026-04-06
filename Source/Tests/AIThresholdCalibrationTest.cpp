/**
 * AIThresholdCalibrationTest.cpp
 *
 * Per-class threshold calibration for the retrained ML model.
 *
 * Strategy: precision-first.
 *   "Meglio che il plugin parli meno, ma quando parla abbia ragione."
 *
 * For each problem class:
 *   1. Generate N problem spectra + N clean spectra
 *   2. Run forwardRawProbabilities() to get raw sigmoid outputs
 *   3. Sweep thresholds from 0.05 to 0.95
 *   4. At each threshold: compute precision, recall, F1
 *   5. Select optimal threshold: maximize F1 subject to precision >= 0.80
 *   6. Classify class as "strong" (shipping) or "weak" (conservative)
 *
 * Output:
 *   - Per-class: current threshold, optimal threshold, metrics at both
 *   - Recommended baseThresholds array (copy-paste into MLEngine.cpp)
 *   - Full pipeline validation with recommended thresholds
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <algorithm>

#include "../AI/AIEngine.h"
#include "../AI/MLEngine.h"

namespace
{

constexpr double kSampleRate   = 44100.0;
constexpr int    kFFTSize      = 4096;
constexpr int    kNumBins      = kFFTSize / 2;
constexpr float  kBaseline     = 0.05f;
constexpr int    kSamplesPerClass = 30;  // enough for stable statistics

// Precision-first calibration target
constexpr float kMinPrecisionTarget = 0.80f;  // when the plugin speaks, it must be right 80%+
constexpr float kStrongClassF1      = 0.60f;  // F1 >= 60% at optimal threshold = strong
constexpr float kWeakClassMaxThresh = 0.70f;  // weak classes get conservative threshold

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum utilities (same format as training: linear magnitude)
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

void cutBand(std::vector<float>& spec, float lowHz, float highHz, float cutAmount)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    const int lo = juce::jmax(0, static_cast<int>(lowHz / binHz));
    const int hi = juce::jmin(kNumBins - 1, static_cast<int>(highHz / binHz));
    for (int i = lo; i <= hi; ++i)
        spec[static_cast<size_t>(i)] = juce::jmax(0.0f, spec[static_cast<size_t>(i)] - cutAmount);
}

void addNoise(std::vector<float>& spec, std::mt19937& rng, float stddev = 0.02f)
{
    std::normal_distribution<float> noise(0.0f, stddev);
    for (auto& v : spec)
        v = juce::jmax(0.0f, v + noise(rng));
}

std::vector<float> linearToDb(const std::vector<float>& linear)
{
    std::vector<float> db(linear.size());
    for (size_t i = 0; i < linear.size(); ++i)
        db[i] = juce::Decibels::gainToDecibels(linear[i] + 1e-10f, -100.0f);
    return db;
}

// ─────────────────────────────────────────────────────────────────────────────
// Problem spectrum generators — one per class
// ─────────────────────────────────────────────────────────────────────────────

using SpecGen = std::vector<float>(*)(std::mt19937&);

std::vector<float> genResonance(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(200.0f, 5000.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.08f);
    return s;
}

std::vector<float> genHarshness(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(2500.0f, 7000.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.3f);
    return s;
}

std::vector<float> genMuddiness(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(120.0f, 400.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.15f);
    return s;
}

std::vector<float> genSibilance(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(5000.0f, 12000.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.12f);
    return s;
}

std::vector<float> genBoominess(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(40.0f, 150.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.15f);
    return s;
}

std::vector<float> genThinness(std::mt19937& rng)
{
    auto s = makeFlat(0.15f); addNoise(s, rng);
    std::uniform_real_distribution<float> f(100.0f, 300.0f), str(0.6f, 1.0f);
    cutBand(s, f(rng) * 0.5f, f(rng) * 2.0f, str(rng) * 0.12f);
    return s;
}

std::vector<float> genBoxyMidrange(std::mt19937& rng)
{
    auto s = makeFlat(); addNoise(s, rng);
    std::uniform_real_distribution<float> f(300.0f, 800.0f), str(0.6f, 1.0f);
    addPeak(s, f(rng), str(rng), 0.12f);
    return s;
}

std::vector<float> genClipping(std::mt19937& rng)
{
    std::uniform_real_distribution<float> lvl(0.2f, 0.5f);
    auto s = makeFlat(lvl(rng));
    addNoise(s, rng, 0.03f);
    for (auto& v : s) v = juce::jlimit(0.0f, 1.2f, v + 0.2f);
    return s;
}

std::vector<float> genClean(std::mt19937& rng)
{
    std::uniform_real_distribution<float> lvl(0.03f, 0.08f);
    auto s = makeFlat(lvl(rng));
    addNoise(s, rng);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Calibration metrics at a given threshold
// ─────────────────────────────────────────────────────────────────────────────

struct ThresholdMetrics
{
    float threshold;
    int tp = 0, fp = 0, fn = 0, tn = 0;

    float precision() const { return (tp + fp) > 0 ? float(tp) / float(tp + fp) : 1.0f; }
    float recall()    const { return (tp + fn) > 0 ? float(tp) / float(tp + fn) : 0.0f; }
    float f1()        const
    {
        float p = precision(), r = recall();
        return (p + r) > 0 ? 2.0f * p * r / (p + r) : 0.0f;
    }
    float fpRate()    const { return (fp + tn) > 0 ? float(fp) / float(fp + tn) : 0.0f; }
};

juce::File findModelFile()
{
    auto root = juce::File(__FILE__).getParentDirectory()
                    .getParentDirectory().getParentDirectory();
    auto f = root.getChildFile("Resources/Models/ml_weights.bin");
    if (f.existsAsFile()) return f;
    f = root.getChildFile("Resources/Models/ml_weights_retrained.bin");
    if (f.existsAsFile()) return f;
    return {};
}

const char* problemName(int idx)
{
    static const char* names[] = {
        "Resonance", "Harshness", "Muddiness", "Sibilance",
        "Boominess", "Thinness", "BoxyMid", "Clipping"
    };
    return (idx >= 0 && idx < 8) ? names[idx] : "?";
}

} // anonymous namespace


// =============================================================================
// TEST 1 — Per-Class Threshold Sweep
// =============================================================================
class AIThresholdCalibration_Sweep : public juce::UnitTest
{
public:
    AIThresholdCalibration_Sweep()
        : juce::UnitTest("AI Threshold Calibration — Per-Class Sweep", "AI-Calibration") {}

    void runTest() override
    {
        beginTest("Threshold sweep — precision-first optimization");

        auto modelFile = findModelFile();
        expect(modelFile.existsAsFile(), "ml_weights.bin not found");
        if (!modelFile.existsAsFile()) return;

        MLEngine ml;
        ml.initialize();
        ml.loadWeights(modelFile);

        const SpecGen generators[] = {
            genResonance, genHarshness, genMuddiness, genSibilance,
            genBoominess, genThinness, genBoxyMidrange, genClipping
        };

        const auto& currentThresholds = ml.getBaseThresholds();

        std::array<float, 8> optimalThresholds {};
        std::array<bool, 8> isStrong {};

        logMessage("");
        logMessage("  ┌─────────────────────────────────────────────────────────────────────────────────┐");
        logMessage("  │  PER-CLASS THRESHOLD CALIBRATION — precision-first (target >= 80%)              │");
        logMessage("  └─────────────────────────────────────────────────────────────────────────────────┘");

        for (int cls = 0; cls < 8; ++cls)
        {
            std::mt19937 rng(42 + cls);

            // Generate problem + clean spectra, get raw probs for this class
            struct Sample { float prob; bool isPositive; };
            std::vector<Sample> samples;

            for (int i = 0; i < kSamplesPerClass; ++i)
            {
                auto spec = generators[cls](rng);
                auto probs = ml.forwardRawProbabilities(spec, kSampleRate);
                samples.push_back({probs[static_cast<size_t>(cls)], true});
            }
            for (int i = 0; i < kSamplesPerClass; ++i)
            {
                auto spec = genClean(rng);
                auto probs = ml.forwardRawProbabilities(spec, kSampleRate);
                samples.push_back({probs[static_cast<size_t>(cls)], false});
            }

            // Sweep thresholds
            std::vector<ThresholdMetrics> sweepResults;
            for (float t = 0.05f; t <= 0.95f; t += 0.05f)
            {
                ThresholdMetrics m;
                m.threshold = t;
                for (const auto& s : samples)
                {
                    bool predicted = s.prob > t;
                    if (s.isPositive && predicted)  m.tp++;
                    if (s.isPositive && !predicted) m.fn++;
                    if (!s.isPositive && predicted)  m.fp++;
                    if (!s.isPositive && !predicted) m.tn++;
                }
                sweepResults.push_back(m);
            }

            // Find optimal: best F1 with precision >= target
            float bestF1 = -1.0f;
            float bestThreshold = 0.5f;
            for (const auto& m : sweepResults)
            {
                if (m.precision() >= kMinPrecisionTarget && m.f1() > bestF1)
                {
                    bestF1 = m.f1();
                    bestThreshold = m.threshold;
                }
            }

            // If no threshold achieves target precision, pick highest-precision threshold
            if (bestF1 < 0.0f)
            {
                float bestPrec = -1.0f;
                for (const auto& m : sweepResults)
                {
                    if (m.precision() > bestPrec)
                    {
                        bestPrec = m.precision();
                        bestThreshold = m.threshold;
                        bestF1 = m.f1();
                    }
                }
            }

            optimalThresholds[static_cast<size_t>(cls)] = bestThreshold;
            isStrong[static_cast<size_t>(cls)] = (bestF1 >= kStrongClassF1);

            // Find metrics at current and optimal thresholds
            ThresholdMetrics atCurrent {}, atOptimal {};
            for (const auto& m : sweepResults)
            {
                if (std::abs(m.threshold - currentThresholds[static_cast<size_t>(cls)]) < 0.026f)
                    atCurrent = m;
                if (std::abs(m.threshold - bestThreshold) < 0.026f)
                    atOptimal = m;
            }

            // Print class report
            logMessage("");
            logMessage("  ── " + juce::String(problemName(cls)) + " ──────────────────────────");
            logMessage("    Current threshold: " + juce::String(currentThresholds[static_cast<size_t>(cls)], 2)
                       + "  → P=" + juce::String(atCurrent.precision() * 100, 0) + "%"
                       + " R=" + juce::String(atCurrent.recall() * 100, 0) + "%"
                       + " F1=" + juce::String(atCurrent.f1() * 100, 1) + "%"
                       + " FP=" + juce::String(atCurrent.fp));
            logMessage("    Optimal threshold: " + juce::String(bestThreshold, 2)
                       + "  → P=" + juce::String(atOptimal.precision() * 100, 0) + "%"
                       + " R=" + juce::String(atOptimal.recall() * 100, 0) + "%"
                       + " F1=" + juce::String(atOptimal.f1() * 100, 1) + "%"
                       + " FP=" + juce::String(atOptimal.fp));
            logMessage("    Class status: " + juce::String(isStrong[static_cast<size_t>(cls)] ? "STRONG (shipping)" : "WEAK (conservative)"));

            // Print sweep table for this class
            logMessage("    Sweep:");
            logMessage("      Thresh  Prec   Rec    F1     TP  FP  FN");
            for (const auto& m : sweepResults)
            {
                juce::String marker = "";
                if (std::abs(m.threshold - bestThreshold) < 0.026f) marker = " <<<";
                if (std::abs(m.threshold - currentThresholds[static_cast<size_t>(cls)]) < 0.026f) marker += " [current]";

                logMessage("      " + juce::String(m.threshold, 2).paddedLeft(' ', 5)
                           + "  " + (juce::String(m.precision() * 100, 0) + "%").paddedLeft(' ', 5)
                           + "  " + (juce::String(m.recall() * 100, 0) + "%").paddedLeft(' ', 5)
                           + "  " + (juce::String(m.f1() * 100, 1) + "%").paddedLeft(' ', 6)
                           + "  " + juce::String(m.tp).paddedLeft(' ', 3)
                           + " " + juce::String(m.fp).paddedLeft(' ', 3)
                           + " " + juce::String(m.fn).paddedLeft(' ', 3)
                           + marker);
            }
        }

        // ── Summary: recommended thresholds ──
        logMessage("");
        logMessage("  ╔══════════════════════════════════════════════════════════════╗");
        logMessage("  ║  RECOMMENDED baseThresholds (precision-first)               ║");
        logMessage("  ╚══════════════════════════════════════════════════════════════╝");
        logMessage("");
        logMessage("  // Copy-paste into MLEngine::initialize():");
        logMessage("  baseThresholds = {{");

        int strongCount = 0;
        for (int cls = 0; cls < 8; ++cls)
        {
            float t = optimalThresholds[static_cast<size_t>(cls)];
            // For weak classes, enforce conservative minimum
            if (!isStrong[static_cast<size_t>(cls)] && t < kWeakClassMaxThresh)
                t = kWeakClassMaxThresh;

            juce::String line = "      " + juce::String(t, 2) + "f,";
            line = line.paddedRight(' ', 16);
            line += "// " + juce::String(problemName(cls));
            if (isStrong[static_cast<size_t>(cls)])
            {
                line += " [STRONG]";
                strongCount++;
            }
            else
            {
                line += " [WEAK — conservative]";
            }
            logMessage("  " + line);
        }
        logMessage("  }};");
        logMessage("");
        logMessage("  Strong classes: " + juce::String(strongCount) + "/8");
        logMessage("  Weak classes:   " + juce::String(8 - strongCount) + "/8 (threshold clamped to >= "
                   + juce::String(kWeakClassMaxThresh, 2) + ")");

        // ── Print comparison table ──
        logMessage("");
        logMessage("  ┌──────────────┬─────────┬─────────┬────────┬──────────────────────┐");
        logMessage("  │ Class        │ Current │ Optimal │ Status │ Delta                │");
        logMessage("  ├──────────────┼─────────┼─────────┼────────┼──────────────────────┤");
        for (int cls = 0; cls < 8; ++cls)
        {
            float cur = currentThresholds[static_cast<size_t>(cls)];
            float opt = optimalThresholds[static_cast<size_t>(cls)];
            if (!isStrong[static_cast<size_t>(cls)] && opt < kWeakClassMaxThresh)
                opt = kWeakClassMaxThresh;
            float delta = opt - cur;
            juce::String deltaStr = (delta >= 0 ? "+" : "") + juce::String(delta, 2);

            logMessage("  │ " + juce::String(problemName(cls)).paddedRight(' ', 12)
                       + " │ " + juce::String(cur, 2).paddedLeft(' ', 7)
                       + " │ " + juce::String(opt, 2).paddedLeft(' ', 7)
                       + " │ " + juce::String(isStrong[static_cast<size_t>(cls)] ? "STRONG" : "WEAK").paddedRight(' ', 6)
                       + " │ " + deltaStr.paddedRight(' ', 20) + " │");
        }
        logMessage("  └──────────────┴─────────┴─────────┴────────┴──────────────────────┘");

        // Assertions
        expect(strongCount >= 4,
               "Only " + juce::String(strongCount) + "/8 classes are strong — need at least 4 for shipping");
    }
};


// =============================================================================
// TEST 2 — Pipeline Validation with Optimal Thresholds
//
// Applies the recommended thresholds and verifies through the full
// AIEngine pipeline (dB→linear conversion included).
// =============================================================================
class AIThresholdCalibration_PipelineValidation : public juce::UnitTest
{
public:
    AIThresholdCalibration_PipelineValidation()
        : juce::UnitTest("AI Threshold Calibration — Pipeline Validation", "AI-Calibration") {}

    void runTest() override
    {
        beginTest("Full pipeline validation with calibrated thresholds");

        auto modelFile = findModelFile();
        if (!modelFile.existsAsFile()) return;

        // Setup AIEngine with ML path forced
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setCustomMLWeightsPathForTests(modelFile);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);
        ai.setSensitivity(0.5f);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);

        expect(ai.isUsingMLDetection(), "ML detection not active");

        // Also setup direct MLEngine for comparison
        MLEngine& ml = ai.getMLEngineForTest();

        const SpecGen generators[] = {
            genResonance, genHarshness, genMuddiness, genSibilance,
            genBoominess, genThinness, genBoxyMidrange, genClipping
        };

        // AIEngine problem type mapping (Clipping maps to Harshness)
        const AIEngine::ProblemType aiTypes[] = {
            AIEngine::ProblemType::Resonance,
            AIEngine::ProblemType::Harshness,
            AIEngine::ProblemType::Muddiness,
            AIEngine::ProblemType::Sibilance,
            AIEngine::ProblemType::LowEndBoom,
            AIEngine::ProblemType::ThinSound,
            AIEngine::ProblemType::Boxyness,
            AIEngine::ProblemType::Harshness  // Clipping → Harshness
        };

        constexpr int numTestsPerClass = 15;
        logMessage("");

        struct ClassResult
        {
            int tp = 0, fp = 0, fn = 0;
            float precision() const { return (tp + fp) > 0 ? float(tp) / float(tp + fp) : 1.0f; }
            float recall()    const { return (tp + fn) > 0 ? float(tp) / float(tp + fn) : 0.0f; }
            float f1()        const
            {
                float p = precision(), r = recall();
                return (p + r) > 0 ? 2.0f * p * r / (p + r) : 0.0f;
            }
        };
        std::array<ClassResult, 8> results {};

        for (int cls = 0; cls < 8; ++cls)
        {
            std::mt19937 rng(100 + cls);  // different seed than calibration

            for (int i = 0; i < numTestsPerClass; ++i)
            {
                auto spec = generators[cls](rng);
                auto dbSpec = linearToDb(spec);
                ai.analyzeSpectrum(dbSpec, true);
                auto pending = ai.getPendingCorrections();

                bool found = false;
                for (const auto& c : pending)
                {
                    if (c.type == aiTypes[cls])
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                    results[static_cast<size_t>(cls)].tp++;
                else
                    results[static_cast<size_t>(cls)].fn++;

                // Count FP: other classes detected on this input
                for (const auto& c : pending)
                {
                    if (c.type != aiTypes[cls])
                    {
                        for (int other = 0; other < 8; ++other)
                        {
                            if (c.type == aiTypes[other] && other != cls)
                                results[static_cast<size_t>(other)].fp++;
                        }
                    }
                }
            }
        }

        // Clean FP test
        int cleanFP = 0;
        constexpr int cleanCount = 20;
        {
            std::mt19937 rng(999);
            for (int i = 0; i < cleanCount; ++i)
            {
                auto spec = genClean(rng);
                auto dbSpec = linearToDb(spec);
                ai.analyzeSpectrum(dbSpec, true);
                auto pending = ai.getPendingCorrections();
                if (!pending.empty()) cleanFP++;
            }
        }

        // Print results
        logMessage("  ┌──────────────┬───────────┬────────┬────────┬────┬────┬────┐");
        logMessage("  │ Class        │ Precision │ Recall │   F1   │ TP │ FP │ FN │");
        logMessage("  ├──────────────┼───────────┼────────┼────────┼────┼────┼────┤");

        float totalF1 = 0;
        int passing = 0;
        for (int cls = 0; cls < 8; ++cls)
        {
            const auto& r = results[static_cast<size_t>(cls)];
            logMessage("  │ " + juce::String(problemName(cls)).paddedRight(' ', 12)
                       + " │ " + (juce::String(r.precision() * 100, 0) + "%").paddedLeft(' ', 9)
                       + " │ " + (juce::String(r.recall() * 100, 0) + "%").paddedLeft(' ', 6)
                       + " │ " + (juce::String(r.f1() * 100, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + juce::String(r.tp).paddedLeft(' ', 2)
                       + " │ " + juce::String(r.fp).paddedLeft(' ', 2)
                       + " │ " + juce::String(r.fn).paddedLeft(' ', 2) + " │");
            totalF1 += r.f1();
            if (r.f1() >= 0.50f) passing++;
        }

        logMessage("  └──────────────┴───────────┴────────┴────────┴────┴────┴────┘");

        float avgF1 = totalF1 / 8.0f;
        float cleanFPRate = float(cleanFP) / float(cleanCount);

        logMessage("");
        logMessage("  Average F1 (pipeline): " + juce::String(avgF1 * 100, 1) + "%");
        logMessage("  Clean FP rate:         " + juce::String(cleanFP) + "/" + juce::String(cleanCount)
                   + " = " + juce::String(cleanFPRate * 100, 1) + "%");
        logMessage("  Classes with F1 >= 50%: " + juce::String(passing) + "/8");

        // Assertions
        expect(cleanFPRate <= 0.15f,
               "Clean FP rate too high: " + juce::String(cleanFPRate * 100, 1) + "%");
        expect(passing >= 4,
               "Only " + juce::String(passing) + "/8 classes pass F1 >= 50% in pipeline");
    }
};


static AIThresholdCalibration_Sweep              sCalibSweep;
static AIThresholdCalibration_PipelineValidation  sCalibPipeline;
