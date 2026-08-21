/**
 * AIAccuracyTest.cpp
 *
 * Forensic accuracy test for the AI detection subsystem.
 * Generates synthetic spectra with known problems, runs them through
 * both MLEngine (raw inference) and AIEngine (full pipeline), and
 * measures precision, recall, and F1 for each problem type.
 *
 * This test answers the question: "When the model says 'resonance detected',
 * how often is it right? And when there IS a resonance, how often does
 * the model find it?"
 *
 * Test structure:
 *   TEST A — MLEngine direct inference (8 problem types × N variations + clean)
 *   TEST B — AIEngine full pipeline (same stimuli through analyzeSpectrum)
 *   TEST C — False positive rate on clean signals
 *   TEST D — Confusion matrix (which problems get misidentified as which)
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <map>

#include "../AI/AIEngine.h"
#include "../AI/MLEngine.h"
#include "Support/AIHeldoutFixtures.h"

namespace
{

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

constexpr double kSampleRate   = 44100.0;
constexpr int    kFFTSize      = 4096;
constexpr int    kNumBins      = kFFTSize / 2;        // 2048 (matches buildSyntheticSpectrum)
constexpr float  kBaseline     = 0.05f;               // linear magnitude baseline (matches training)
constexpr int    kVariations   = 10;                   // variations per problem type

// Minimum acceptable metrics for PASS
constexpr float kMinPrecision = 0.60f;  // 60% — if you say it, be right > half the time
constexpr float kMinRecall    = 0.50f;  // 50% — catch at least half the real problems
constexpr float kMinF1        = 0.55f;  // 55% — balanced metric
constexpr float kMaxFPRate    = 0.30f;  // 30% — max false positive rate on clean signals

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum generation utilities
//
// IMPORTANT: All spectra are in LINEAR MAGNITUDE format (0.0 to ~2.0),
// matching MLEngine::buildSyntheticSpectrum() and what extractMelBands() expects.
// extractMelBands() does its own dB conversion internally.
// ─────────────────────────────────────────────────────────────────────────────

/// Convert frequency in Hz to FFT bin index
int hzToBin(float hz)
{
    return static_cast<int>(std::round(hz / (static_cast<float>(kSampleRate) / kFFTSize)));
}

/// Create a flat spectrum at a given linear magnitude level
std::vector<float> makeFlat(float level = kBaseline)
{
    return std::vector<float>(kNumBins, level);
}

/// Add a Gaussian peak at a given frequency (linear magnitude domain)
/// strength: 0.0–1.0, typical problem strength
/// sigmaFactor: controls width (smaller = narrower, resonance-like)
void addPeak(std::vector<float>& spec, float freqHz, float strength, float sigmaFactor = 0.08f)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    const float sigmaHz = juce::jmax(30.0f, freqHz * sigmaFactor);

    for (int i = 0; i < kNumBins; ++i)
    {
        const float freq = static_cast<float>(i) * binHz;
        const float d = (freq - freqHz) / sigmaHz;
        const float peak = strength * std::exp(-0.5f * d * d);
        spec[static_cast<size_t>(i)] += peak;
    }
}

/// Add broadband energy boost in a frequency range (linear magnitude)
void addBand(std::vector<float>& spec, float lowHz, float highHz, float boostLinear)
{
    const int lo = juce::jmax(0, hzToBin(lowHz));
    const int hi = juce::jmin(kNumBins - 1, hzToBin(highHz));

    for (int i = lo; i <= hi; ++i)
        spec[static_cast<size_t>(i)] += boostLinear;
}

/// Convert a LINEAR-magnitude fixture spectrum to the dB domain that
/// AIEngine::analyzeSpectrum() expects (the live analyzer publishes dB; the
/// engine internally inverts via convertDbSpectrumToLinearMagnitude: 10^(dB/20)).
/// Exact inverse so the round-trip is identity. Fixes the TEST B domain bug:
/// before this, the pipeline received linear values, treated 0.05 as "0.05 dB",
/// re-flattened the spectrum, and detected nothing (0% recall, false alarm).
std::vector<float> linearToDb(const std::vector<float>& linear)
{
    std::vector<float> db(linear.size());
    for (size_t i = 0; i < linear.size(); ++i)
        db[i] = linear[i] > 1.0e-10f
              ? juce::jlimit(-120.0f, 12.0f, 20.0f * std::log10(linear[i]))
              : -120.0f;
    return db;
}

/// Reduce energy in a frequency range (for "thin" or "dull" problems)
void cutBand(std::vector<float>& spec, float lowHz, float highHz, float cutAmount)
{
    const int lo = juce::jmax(0, hzToBin(lowHz));
    const int hi = juce::jmin(kNumBins - 1, hzToBin(highHz));

    for (int i = lo; i <= hi; ++i)
        spec[static_cast<size_t>(i)] = juce::jmax(0.0f, spec[static_cast<size_t>(i)] - cutAmount);
}

/// Add Gaussian noise to a spectrum
void addNoise(std::vector<float>& spec, std::mt19937& rng, float stddev = 0.02f)
{
    std::normal_distribution<float> noise(0.0f, stddev);
    for (auto& v : spec)
        v = juce::jmax(0.0f, v + noise(rng));
}

// ─────────────────────────────────────────────────────────────────────────────
// Problem generators — each returns a spectrum with ONE known problem
// ─────────────────────────────────────────────────────────────────────────────

struct TestCase
{
    std::vector<float> spectrum;
    MLEngine::ProblemType expectedMLType;
    AIEngine::ProblemType expectedAIType;
    juce::String description;
};

// ─────────────────────────────────────────────────────────────────────────────
// Problem generators — LINEAR MAGNITUDE format
//
// These mirror MLEngine::buildSyntheticSpectrum() which uses:
//   baseline ~0.05, Gaussian peaks with strength 0.6–1.0, noise σ=0.02
// The model was trained on this format. extractMelBands() does dB conversion.
// ─────────────────────────────────────────────────────────────────────────────

/// Generate variations of resonance (sharp narrow peak)
std::vector<TestCase> generateResonanceTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> freqDist(200.0f, 5000.0f);  // matches problemFreqRanges
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float freq = freqDist(rng);
        const float str = strengthDist(rng);
        addPeak(spec, freq, str, 0.08f);  // narrow peak like training
        cases.push_back({spec, MLEngine::ProblemType::Resonance,
                         AIEngine::ProblemType::Resonance,
                         "Resonance @" + juce::String(static_cast<int>(freq)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate harshness (broad 2–8 kHz excess)
std::vector<TestCase> generateHarshnessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        // Broad boost centered around 4kHz with wide sigma
        const float center = 3000.0f + static_cast<float>(i) * 500.0f;
        addPeak(spec, center, str, 0.3f);  // wider sigma for broadband
        cases.push_back({spec, MLEngine::ProblemType::Harshness,
                         AIEngine::ProblemType::Harshness,
                         "Harshness @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate muddiness (100–400 Hz buildup)
std::vector<TestCase> generateMuddinessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        const float center = 150.0f + static_cast<float>(i) * 25.0f;
        addPeak(spec, center, str, 0.15f);  // moderate width
        cases.push_back({spec, MLEngine::ProblemType::Muddiness,
                         AIEngine::ProblemType::Muddiness,
                         "Muddiness @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate sibilance (5–12 kHz excess)
std::vector<TestCase> generateSibilanceTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        const float center = 6000.0f + static_cast<float>(i) * 600.0f;
        addPeak(spec, center, str, 0.12f);
        cases.push_back({spec, MLEngine::ProblemType::Sibilance,
                         AIEngine::ProblemType::Sibilance,
                         "Sibilance @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate boominess (40–150 Hz excess)
std::vector<TestCase> generateBoominessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        const float center = 50.0f + static_cast<float>(i) * 10.0f;
        addPeak(spec, center, str, 0.15f);
        cases.push_back({spec, MLEngine::ProblemType::Boominess,
                         AIEngine::ProblemType::LowEndBoom,
                         "Boominess @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate thinness (lack of low-mids — negative peak)
std::vector<TestCase> generateThinnessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(0.15f);  // higher baseline so cut is visible
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        const float center = 120.0f + static_cast<float>(i) * 20.0f;
        // Thinness: NEGATIVE peak (cut) — matches buildSyntheticSpectrum sign=-1
        cutBand(spec, center * 0.5f, center * 2.0f, str * 0.12f);
        cases.push_back({spec, MLEngine::ProblemType::Thinness,
                         AIEngine::ProblemType::ThinSound,
                         "Thinness @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate boxy midrange (300–800 Hz resonance)
std::vector<TestCase> generateBoxyTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> strengthDist(0.6f, 1.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat();
        addNoise(spec, rng);
        const float str = strengthDist(rng);
        const float center = 350.0f + static_cast<float>(i) * 50.0f;
        addPeak(spec, center, str, 0.12f);
        cases.push_back({spec, MLEngine::ProblemType::BoxyMidrange,
                         AIEngine::ProblemType::Boxyness,
                         "Boxy @" + juce::String(static_cast<int>(center)) + "Hz str=" + juce::String(str, 2)});
    }
    return cases;
}

/// Generate clipping (broadband high-level with harmonics)
std::vector<TestCase> generateClippingTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    (void) rng;

    for (int i = 0; i < kVariations; ++i)
    {
        // Clipping: entire spectrum elevated + clamped (matches training)
        auto spec = makeFlat(0.2f + static_cast<float>(i) * 0.05f);
        for (auto& v : spec)
            v = juce::jlimit(0.0f, 1.2f, v + 0.2f);
        cases.push_back({spec, MLEngine::ProblemType::Clipping,
                         AIEngine::ProblemType::None,  // AIEngine doesn't have Clipping
                         "Clipping level " + juce::String(i)});
    }
    return cases;
}

/// Generate clean spectra (no problems) for false-positive testing
std::vector<TestCase> generateCleanTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> levelDist(0.03f, 0.08f);

    // Variation 1-4: flat noise at different levels (matching training baseline range)
    for (int i = 0; i < 4; ++i)
    {
        auto spec = makeFlat(levelDist(rng));
        addNoise(spec, rng);
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,
                         AIEngine::ProblemType::None,
                         "Clean flat " + juce::String(i)});
    }

    // Variation 5-7: gentle spectral tilt (natural, not a problem)
    for (int i = 0; i < 3; ++i)
    {
        auto spec = makeFlat(0.05f);
        addNoise(spec, rng);
        // Very gentle low-end warmth (well below problem threshold)
        addBand(spec, 40.0f, 200.0f, 0.01f + static_cast<float>(i) * 0.005f);
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,
                         AIEngine::ProblemType::None,
                         "Clean warm " + juce::String(i)});
    }

    // Variation 8-10: slightly brighter (natural, not harsh)
    for (int i = 0; i < 3; ++i)
    {
        auto spec = makeFlat(0.05f);
        addNoise(spec, rng);
        addBand(spec, 8000.0f, 18000.0f, 0.01f + static_cast<float>(i) * 0.005f);
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,
                         AIEngine::ProblemType::None,
                         "Clean bright " + juce::String(i)});
    }

    return cases;
}

// ─────────────────────────────────────────────────────────────────────────────
// Metrics
// ─────────────────────────────────────────────────────────────────────────────

struct ClassMetrics
{
    int truePositive  = 0;
    int falsePositive = 0;
    int falseNegative = 0;
    int trueNegative  = 0;

    float precision() const
    {
        const int denom = truePositive + falsePositive;
        return denom > 0 ? static_cast<float>(truePositive) / static_cast<float>(denom) : 0.0f;
    }

    float recall() const
    {
        const int denom = truePositive + falseNegative;
        return denom > 0 ? static_cast<float>(truePositive) / static_cast<float>(denom) : 0.0f;
    }

    float f1() const
    {
        const float p = precision();
        const float r = recall();
        return (p + r > 0.0f) ? (2.0f * p * r / (p + r)) : 0.0f;
    }

    float falsePositiveRate() const
    {
        const int denom = falsePositive + trueNegative;
        return denom > 0 ? static_cast<float>(falsePositive) / static_cast<float>(denom) : 0.0f;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// P4-D2 — HELD-OUT realistic fixture generator (distinct from the training set).
//
// The generate*Tests() helpers above are deliberately TRAINING-LIKE: flat linear
// baseline + linear Gaussian peaks, mirroring MLEngine::buildSyntheticSpectrum().
// Measuring recall on them flatters the model (it has seen this shape). P4-D2
// instead builds fixtures the training generator NEVER produced:
//   • a STEEP spectral tilt background (P4-M1 frozen extrapolation band -6.5..-5.0
//     dB/decade, referenced @100 Hz) — natural dark/bass-heavy program material,
//     ONE notch steeper than the band reserved for P4-M2 training (see the FROZEN
//     DISJOINT EVAL contract in namespace heldout below);
//   • dB-domain Gaussian humps (or a subtractive low-shelf for Thinness) at
//     randomized centre/height/width within each class's range;
//   • mild dB noise.
// These are exactly the SHAPES validated by AIBackendSweepTest (applyPinkTilt /
// addPeakDb), so the realism is not invented here — only the per-class wiring is.
//
// Level regime is matched ON PURPOSE: baseline −26 dB == linear 0.05, the same
// absolute energy the model trained on (extractMelBands' dB mapping is ABSOLUTE,
// gainToDecibels/−100, so an out-of-regime level would make the probe degenerate
// rather than a fair generalization test). We hold out the SHAPE — the actual
// generalization variable — not the loudness.
//
// Built in dB, returned as LINEAR magnitude so the ML path consumes it directly
// and the AIEngine path re-derives the exact dB via linearToDb() (round-trip id).
// ─────────────────────────────────────────────────────────────────────────────
namespace heldout = aieq_test::heldout;

} // anonymous namespace


// =============================================================================
// TEST A — MLEngine Direct Inference Accuracy
// =============================================================================
class AIAccuracyTest_MLEngine : public juce::UnitTest
{
public:
    AIAccuracyTest_MLEngine()
        // KnownDebt (non-blocking): real ML recall debt — Resonance ~40%,
        // Thinness ~30% on synthetic fixtures (model trained on 64 Gaussians).
        // Scorecard KnownDebt-ML; target P4. Clean FP 0/10 here.
        : juce::UnitTest("AI Accuracy — MLEngine Direct", "KnownDebt") {}

    void runTest() override
    {
        beginTest("MLEngine inference accuracy — 8 problem types");

        MLEngine ml;
        ml.initialize();

        // Load weights from the bundled model
        juce::File modelFile;
        {
            // Try common locations
            auto appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                              .getParentDirectory();
            modelFile = appDir.getChildFile("Resources/Models/ml_weights.bin");
            if (!modelFile.existsAsFile())
                modelFile = appDir.getChildFile("../Resources/Models/ml_weights.bin");
            if (!modelFile.existsAsFile())
                modelFile = appDir.getChildFile("../../Resources/Models/ml_weights.bin");
            if (!modelFile.existsAsFile())
            {
                // Try relative to source tree
                auto srcDir = juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory();
                modelFile = srcDir.getChildFile("Resources/Models/ml_weights.bin");
            }
        }

        if (modelFile.existsAsFile())
        {
            const bool loaded = ml.loadWeights(modelFile);
            logMessage("  Model loaded: " + juce::String(loaded ? "YES" : "NO")
                       + " from " + modelFile.getFullPathName());
        }
        else
        {
            logMessage("  WARNING: ml_weights.bin not found, using random weights");
        }

        ml.setSensitivity(0.5f);  // default sensitivity

        std::mt19937 rng(42);  // fixed seed for reproducibility

        // Generate all test cases
        using GenFunc = std::vector<TestCase>(*)(std::mt19937&);
        struct ProblemGroup
        {
            GenFunc generator;
            MLEngine::ProblemType type;
            juce::String name;
        };

        ProblemGroup groups[] = {
            { generateResonanceTests,  MLEngine::ProblemType::Resonance,    "Resonance" },
            { generateHarshnessTests,  MLEngine::ProblemType::Harshness,    "Harshness" },
            { generateMuddinessTests,  MLEngine::ProblemType::Muddiness,    "Muddiness" },
            { generateSibilanceTests,  MLEngine::ProblemType::Sibilance,    "Sibilance" },
            { generateBoominessTests,  MLEngine::ProblemType::Boominess,    "Boominess" },
            { generateThinnessTests,   MLEngine::ProblemType::Thinness,     "Thinness" },
            { generateBoxyTests,       MLEngine::ProblemType::BoxyMidrange, "BoxyMidrange" },
            { generateClippingTests,   MLEngine::ProblemType::Clipping,     "Clipping" },
        };

        constexpr int numTypes = 8;
        std::array<ClassMetrics, numTypes> metrics{};

        // Confusion matrix: [expected][detected] → count
        std::array<std::array<int, numTypes + 1>, numTypes + 1> confusion{};  // +1 for "None"

        // ── Run problem cases ──
        for (int g = 0; g < numTypes; ++g)
        {
            auto cases = groups[g].generator(rng);
            for (const auto& tc : cases)
            {
                auto detections = ml.detectProblems(tc.spectrum, kSampleRate);

                bool foundExpected = false;
                for (const auto& det : detections)
                {
                    if (det.type == tc.expectedMLType)
                        foundExpected = true;
                }

                if (foundExpected)
                {
                    metrics[g].truePositive++;
                    confusion[g][g]++;
                }
                else
                {
                    metrics[g].falseNegative++;
                    // Record what it detected instead (if anything)
                    if (detections.empty())
                        confusion[g][numTypes]++;  // "None" column
                    else
                        confusion[g][static_cast<int>(detections[0].type)]++;
                }

                // Check for false positives on other types
                for (const auto& det : detections)
                {
                    if (det.type != tc.expectedMLType)
                    {
                        const int detIdx = static_cast<int>(det.type);
                        if (detIdx >= 0 && detIdx < numTypes)
                            metrics[detIdx].falsePositive++;
                    }
                }
            }
        }

        // ── Run clean cases (false positive test) ──
        auto cleanCases = generateCleanTests(rng);
        int cleanTotal = static_cast<int>(cleanCases.size());
        int cleanFalsePositives = 0;

        for (const auto& tc : cleanCases)
        {
            auto detections = ml.detectProblems(tc.spectrum, kSampleRate);

            if (detections.empty())
            {
                for (auto& m : metrics)
                    m.trueNegative++;
                confusion[numTypes][numTypes]++;  // clean detected as clean
            }
            else
            {
                cleanFalsePositives++;
                for (const auto& det : detections)
                {
                    const int detIdx = static_cast<int>(det.type);
                    if (detIdx >= 0 && detIdx < numTypes)
                    {
                        metrics[detIdx].falsePositive++;
                        confusion[numTypes][detIdx]++;
                    }
                }
            }
        }

        // ── Print results ──
        logMessage("");
        logMessage("  ┌─────────────────┬───────────┬────────┬────────┬────────┬────┬────┬────┐");
        logMessage("  │ Problem Type    │ Precision │ Recall │   F1   │ FP Rate│ TP │ FP │ FN │");
        logMessage("  ├─────────────────┼───────────┼────────┼────────┼────────┼────┼────┼────┤");

        float totalF1 = 0.0f;
        int passCount = 0;

        for (int g = 0; g < numTypes; ++g)
        {
            const auto& m = metrics[g];
            const auto name = groups[g].name.paddedRight(' ', 15);
            const auto pStr = juce::String(m.precision() * 100.0f, 1).paddedLeft(' ', 7) + "%";
            const auto rStr = juce::String(m.recall() * 100.0f, 1).paddedLeft(' ', 6) + "%";
            const auto fStr = juce::String(m.f1() * 100.0f, 1).paddedLeft(' ', 6) + "%";
            const auto fpStr = juce::String(m.falsePositiveRate() * 100.0f, 1).paddedLeft(' ', 6) + "%";

            logMessage("  │ " + name + " │ " + pStr + " │ " + rStr + " │ " + fStr
                       + " │ " + fpStr + " │ " + juce::String(m.truePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falsePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falseNegative).paddedLeft(' ', 2) + " │");

            totalF1 += m.f1();
            if (m.f1() >= kMinF1)
                passCount++;
        }

        logMessage("  └─────────────────┴───────────┴────────┴────────┴────────┴────┴────┴────┘");

        const float avgF1 = totalF1 / numTypes;
        const float fpRate = static_cast<float>(cleanFalsePositives) / static_cast<float>(cleanTotal);

        logMessage("");
        logMessage("  Average F1: " + juce::String(avgF1 * 100.0f, 1) + "%");
        logMessage("  Clean FP rate: " + juce::String(cleanFalsePositives) + "/" + juce::String(cleanTotal)
                   + " = " + juce::String(fpRate * 100.0f, 1) + "%");
        logMessage("  Types passing F1 >= " + juce::String(kMinF1 * 100.0f, 0) + "%: "
                   + juce::String(passCount) + "/" + juce::String(numTypes));

        // ── Print confusion matrix ──
        logMessage("");
        logMessage("  Confusion Matrix (rows=expected, cols=detected):");
        juce::String header = "  Expected\\Det  ";
        const char* shortNames[] = {"Res", "Har", "Mud", "Sib", "Bom", "Thn", "Bxy", "Clp", "None"};
        for (int c = 0; c <= numTypes; ++c)
            header += juce::String(shortNames[c]).paddedLeft(' ', 5);
        logMessage(header);

        for (int r = 0; r <= numTypes; ++r)
        {
            juce::String row = "  " + juce::String(r < numTypes ? shortNames[r] : "Clean").paddedRight(' ', 14);
            for (int c = 0; c <= numTypes; ++c)
                row += juce::String(confusion[r][c]).paddedLeft(' ', 5);
            logMessage(row);
        }

        // ── Assertions ──
        logMessage("");

        // Per-type assertions
        for (int g = 0; g < numTypes; ++g)
        {
            const auto& m = metrics[g];
            expect(m.recall() >= kMinRecall,
                   groups[g].name + " recall too low: " + juce::String(m.recall() * 100.0f, 1)
                   + "% (need >= " + juce::String(kMinRecall * 100.0f, 0) + "%)");
        }

        // Overall assertions
        expect(avgF1 >= kMinF1,
               "Average F1 too low: " + juce::String(avgF1 * 100.0f, 1)
               + "% (need >= " + juce::String(kMinF1 * 100.0f, 0) + "%)");

        expect(fpRate <= kMaxFPRate,
               "Clean false positive rate too high: " + juce::String(fpRate * 100.0f, 1)
               + "% (need <= " + juce::String(kMaxFPRate * 100.0f, 0) + "%)");
    }
};


// =============================================================================
// TEST B — AIEngine Full Pipeline Accuracy
// =============================================================================
class AIAccuracyTest_AIEngine : public juce::UnitTest
{
public:
    AIAccuracyTest_AIEngine()
        // KnownDebt (non-blocking): fixture-realism debt. After the 1a domain
        // fix this measures the full pipeline on DEAD-FLAT fixtures (makeFlat →
        // flat -26 dB), unrealistic vs real audio → 100% clean FP / 621 res FP.
        // Authoritative gates are AI-Sweep (tilt, 0/18) + AI-Corpus (real audio).
        // Scorecard KnownDebt-FixtureRealism; do NOT chase green by retuning the
        // engine. Separate ticket: rebuild this test on pink-tilted fixtures.
        : juce::UnitTest("AI Accuracy — AIEngine Pipeline", "KnownDebt") {}

    void runTest() override
    {
        beginTest("AIEngine full pipeline accuracy — 7 problem types");

        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSensitivity(0.6f);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);

        std::mt19937 rng(42);

        // AIEngine problem types (excludes Clipping which MLEngine has but AIEngine doesn't)
        struct ProblemGroup
        {
            std::vector<TestCase>(*generator)(std::mt19937&);
            AIEngine::ProblemType type;
            juce::String name;
        };

        ProblemGroup groups[] = {
            { generateResonanceTests,  AIEngine::ProblemType::Resonance,   "Resonance" },
            { generateHarshnessTests,  AIEngine::ProblemType::Harshness,   "Harshness" },
            { generateMuddinessTests,  AIEngine::ProblemType::Muddiness,   "Muddiness" },
            { generateSibilanceTests,  AIEngine::ProblemType::Sibilance,   "Sibilance" },
            { generateBoominessTests,  AIEngine::ProblemType::LowEndBoom,  "LowEndBoom" },
            { generateThinnessTests,   AIEngine::ProblemType::ThinSound,   "ThinSound" },
            { generateBoxyTests,       AIEngine::ProblemType::Boxyness,    "Boxyness" },
        };

        constexpr int numTypes = 7;
        std::array<ClassMetrics, numTypes> metrics{};

        for (int g = 0; g < numTypes; ++g)
        {
            auto cases = groups[g].generator(rng);
            for (const auto& tc : cases)
            {
                // Feed spectrum to AIEngine in the dB domain it expects (live
                // analyzer publishes dB; engine inverts to linear internally).
                ai.analyzeSpectrum(linearToDb(tc.spectrum), true);

                auto pending = ai.getPendingCorrections();

                bool foundExpected = false;
                for (const auto& corr : pending)
                {
                    if (corr.type == tc.expectedAIType)
                        foundExpected = true;
                    else
                    {
                        // Count as FP for the detected type
                        for (int g2 = 0; g2 < numTypes; ++g2)
                        {
                            if (corr.type == groups[g2].type)
                                metrics[g2].falsePositive++;
                        }
                    }
                }

                if (foundExpected)
                    metrics[g].truePositive++;
                else
                    metrics[g].falseNegative++;
            }
        }

        // Clean signals
        auto cleanCases = generateCleanTests(rng);
        int cleanFP = 0;
        for (const auto& tc : cleanCases)
        {
            ai.analyzeSpectrum(linearToDb(tc.spectrum), true);
            auto pending = ai.getPendingCorrections();
            if (!pending.empty())
                cleanFP++;
            for (auto& m : metrics)
                m.trueNegative++;
        }

        // Print results
        logMessage("");
        logMessage("  ┌─────────────────┬───────────┬────────┬────────┬────┬────┬────┐");
        logMessage("  │ Problem Type    │ Precision │ Recall │   F1   │ TP │ FP │ FN │");
        logMessage("  ├─────────────────┼───────────┼────────┼────────┼────┼────┼────┤");

        float totalF1 = 0.0f;
        for (int g = 0; g < numTypes; ++g)
        {
            const auto& m = metrics[g];
            const auto name = groups[g].name.paddedRight(' ', 15);
            logMessage("  │ " + name
                       + " │ " + (juce::String(m.precision() * 100.0f, 1) + "%").paddedLeft(' ', 9)
                       + " │ " + (juce::String(m.recall() * 100.0f, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + (juce::String(m.f1() * 100.0f, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + juce::String(m.truePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falsePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falseNegative).paddedLeft(' ', 2) + " │");
            totalF1 += m.f1();
        }

        logMessage("  └─────────────────┴───────────┴────────┴────────┴────┴────┴────┘");

        const float avgF1 = totalF1 / numTypes;
        const float fpRate = static_cast<float>(cleanFP) / static_cast<float>(cleanCases.size());

        logMessage("  Average F1: " + juce::String(avgF1 * 100.0f, 1) + "%");
        logMessage("  Clean FP rate: " + juce::String(fpRate * 100.0f, 1) + "%");

        // Assertions (softer for full pipeline due to temporal smoothing, thresholding)
        expect(avgF1 >= 0.40f,
               "AIEngine avg F1 too low: " + juce::String(avgF1 * 100.0f, 1) + "%");

        // At least 4 of 7 types should have recall >= 50%
        int goodRecall = 0;
        for (int g = 0; g < numTypes; ++g)
            if (metrics[g].recall() >= 0.50f)
                goodRecall++;

        expect(goodRecall >= 4,
               "Only " + juce::String(goodRecall) + "/7 types have recall >= 50%");
    }
};


// =============================================================================
// TEST C — Retrain MLEngine + Re-evaluate
//
// Retrains the model from scratch with the improved dataset (clean samples,
// hard negatives, precision-weighted loss), then re-runs accuracy evaluation.
// If the retrained model passes gates, saves new weights.
// =============================================================================
class AIAccuracyTest_Retrain : public juce::UnitTest
{
public:
    AIAccuracyTest_Retrain()
        // KnownDebt (non-blocking): same synthetic-fixture ML recall debt as
        // MLEngine Direct (Thinness ~20% after retrain). Scorecard KnownDebt-ML.
        : juce::UnitTest("AI Accuracy — Retrain + Re-evaluate", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Retrain MLEngine with improved dataset");

        MLEngine ml;
        ml.initialize();
        ml.initializeRandomWeights();  // Start fresh
        ml.setSensitivity(0.5f);

        // Train with much more data and epochs than default
        constexpr int samplesPerProblem = 300;  // 300 per class
        constexpr int epochs = 300;             // long convergence
        constexpr float lr = 0.005f;            // BCE-safe learning rate

        logMessage("  Training: " + juce::String(samplesPerProblem) + " samples/class, "
                   + juce::String(epochs) + " epochs, lr=" + juce::String(lr, 4));

        auto t0 = juce::Time::getMillisecondCounterHiRes();

        // Use the improved generateSyntheticDataset (with clean + hard negatives)
        auto dataset = ml.generateSyntheticDataset(samplesPerProblem, kSampleRate, kFFTSize);
        ml.trainOnDataset(dataset, epochs, lr);

        auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
        logMessage("  Training completed in " + juce::String(elapsed, 0) + " ms");
        logMessage("  Dataset size: " + juce::String(static_cast<int>(dataset.size()))
                   + " (" + juce::String(samplesPerProblem) + " per problem + "
                   + juce::String(samplesPerProblem * 2) + " clean + "
                   + juce::String(samplesPerProblem) + " hard neg)");

        // ── Re-evaluate accuracy ──
        beginTest("Retrained model accuracy");

        std::mt19937 rng(42);

        struct ProblemGroup
        {
            std::vector<TestCase>(*generator)(std::mt19937&);
            MLEngine::ProblemType type;
            juce::String name;
        };

        ProblemGroup groups[] = {
            { generateResonanceTests,  MLEngine::ProblemType::Resonance,    "Resonance" },
            { generateHarshnessTests,  MLEngine::ProblemType::Harshness,    "Harshness" },
            { generateMuddinessTests,  MLEngine::ProblemType::Muddiness,    "Muddiness" },
            { generateSibilanceTests,  MLEngine::ProblemType::Sibilance,    "Sibilance" },
            { generateBoominessTests,  MLEngine::ProblemType::Boominess,    "Boominess" },
            { generateThinnessTests,   MLEngine::ProblemType::Thinness,     "Thinness" },
            { generateBoxyTests,       MLEngine::ProblemType::BoxyMidrange, "BoxyMidrange" },
            { generateClippingTests,   MLEngine::ProblemType::Clipping,     "Clipping" },
        };

        constexpr int numTypes = 8;
        std::array<ClassMetrics, numTypes> metrics{};

        for (int g = 0; g < numTypes; ++g)
        {
            auto cases = groups[g].generator(rng);
            for (const auto& tc : cases)
            {
                auto detections = ml.detectProblems(tc.spectrum, kSampleRate);

                bool foundExpected = false;
                for (const auto& det : detections)
                {
                    if (det.type == tc.expectedMLType)
                        foundExpected = true;
                }

                if (foundExpected)
                    metrics[static_cast<size_t>(g)].truePositive++;
                else
                    metrics[static_cast<size_t>(g)].falseNegative++;

                for (const auto& det : detections)
                {
                    if (det.type != tc.expectedMLType)
                    {
                        const int detIdx = static_cast<int>(det.type);
                        if (detIdx >= 0 && detIdx < numTypes)
                            metrics[static_cast<size_t>(detIdx)].falsePositive++;
                    }
                }
            }
        }

        // Clean test
        auto cleanCases = generateCleanTests(rng);
        int cleanFP = 0;
        for (const auto& tc : cleanCases)
        {
            auto detections = ml.detectProblems(tc.spectrum, kSampleRate);
            if (!detections.empty())
                cleanFP++;
            for (auto& m : metrics)
                m.trueNegative++;
        }

        // Print results
        logMessage("");
        logMessage("  ┌─────────────────┬───────────┬────────┬────────┬────────┬────┬────┬────┐");
        logMessage("  │ Problem Type    │ Precision │ Recall │   F1   │ FP Rate│ TP │ FP │ FN │");
        logMessage("  ├─────────────────┼───────────┼────────┼────────┼────────┼────┼────┼────┤");

        float totalF1 = 0.0f;
        int passCount = 0;
        for (int g = 0; g < numTypes; ++g)
        {
            const auto& m = metrics[static_cast<size_t>(g)];
            const auto name = groups[g].name.paddedRight(' ', 15);
            logMessage("  │ " + name
                       + " │ " + (juce::String(m.precision() * 100.0f, 1) + "%").paddedLeft(' ', 9)
                       + " │ " + (juce::String(m.recall() * 100.0f, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + (juce::String(m.f1() * 100.0f, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + (juce::String(m.falsePositiveRate() * 100.0f, 1) + "%").paddedLeft(' ', 6)
                       + " │ " + juce::String(m.truePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falsePositive).paddedLeft(' ', 2)
                       + " │ " + juce::String(m.falseNegative).paddedLeft(' ', 2) + " │");
            totalF1 += m.f1();
            if (m.f1() >= kMinF1) passCount++;
        }

        logMessage("  └─────────────────┴───────────┴────────┴────────┴────────┴────┴────┴────┘");

        const float avgF1 = totalF1 / numTypes;
        const float fpRate = static_cast<float>(cleanFP) / static_cast<float>(cleanCases.size());

        logMessage("");
        logMessage("  RETRAINED: Average F1: " + juce::String(avgF1 * 100.0f, 1) + "%");
        logMessage("  RETRAINED: Clean FP rate: " + juce::String(fpRate * 100.0f, 1) + "%");
        logMessage("  RETRAINED: Types passing F1 >= " + juce::String(kMinF1 * 100.0f, 0) + "%: "
                   + juce::String(passCount) + "/" + juce::String(numTypes));

        // Save retrained weights if improvement is significant.
        // HERMETICITY (H4): write to the TEMP dir, never into the source tree.
        // The previous target (Resources/Models/ml_weights_retrained.bin) made
        // this unit test mutate a tracked, ship-adjacent binary on every run
        // (polluting git status, non-deterministic). The artifact is for manual
        // inspection only — never tracked.
        if (avgF1 > 0.20f)  // better than baseline 15.6%
        {
            auto modelFile = juce::File("/tmp").getChildFile("aieq_ml_weights_retrained.bin");
            bool saved = ml.saveWeights(modelFile);
            logMessage("  Retrained weights saved: " + juce::String(saved ? "YES" : "NO")
                       + " → " + modelFile.getFullPathName());
        }

        // ── P4-M2a diagnostic: shipped-vs-candidate on the frozen held-out ruler ──
        // This is measurement-only. The candidate stays in memory (and optionally /tmp);
        // Resources/Models/ml_weights.bin remains the shipped baseline until P4-M5.
        beginTest("P4-M2a diagnostic — shipped vs candidate on frozen held-out L2");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");
        MLEngine shipped;
        shipped.initialize();
        expect(shipped.loadWeights(shippedModelFile), "loadWeights failed on shipped model");
        shipped.setSensitivity(0.5f);
        ml.setSensitivity(0.5f);

        struct HeldoutGroup
        {
            heldout::HClass hc;
            MLEngine::ProblemType type;
            const char* name;
        };
        const HeldoutGroup heldoutGroups[] = {
            { heldout::HClass::Resonance, MLEngine::ProblemType::Resonance,    "Resonance" },
            { heldout::HClass::Harshness, MLEngine::ProblemType::Harshness,    "Harshness" },
            { heldout::HClass::Muddiness, MLEngine::ProblemType::Muddiness,    "Muddiness" },
            { heldout::HClass::Sibilance, MLEngine::ProblemType::Sibilance,    "Sibilance" },
            { heldout::HClass::Boominess, MLEngine::ProblemType::Boominess,    "Boominess" },
            { heldout::HClass::Boxyness,  MLEngine::ProblemType::BoxyMidrange, "Boxyness" },
            { heldout::HClass::Thinness,  MLEngine::ProblemType::Thinness,     "Thinness" },
        };

        auto countRuleHits = [](MLEngine& model,
                                const std::vector<std::vector<float>>& fixtures,
                                MLEngine::ProblemType type)
        {
            int hits = 0;
            for (const auto& lin : fixtures)
            {
                for (const auto& det : model.detectProblems(lin, kSampleRate))
                {
                    if (det.type == type)
                    {
                        ++hits;
                        break;
                    }
                }
            }
            return hits;
        };

        logMessage("");
        logMessage("  P4-M2a held-out L2 rule — shipped vs candidate (candidate UNSHIPPED)");
        logMessage("  class      | shipped | candidate | delta");
        logMessage("  -----------+---------+-----------+------");

        std::mt19937 heldRng(20260617); // same frozen heldout_v2 problem sequence
        for (const auto& g : heldoutGroups)
        {
            std::vector<std::vector<float>> fixtures;
            fixtures.reserve(static_cast<size_t>(heldout::kVariations));
            for (int v = 0; v < heldout::kVariations; ++v)
                fixtures.push_back(heldout::make(g.hc, heldRng));

            const int shippedHits = countRuleHits(shipped, fixtures, g.type);
            const int candidateHits = countRuleHits(ml, fixtures, g.type);
            const int delta = candidateHits - shippedHits;
            logMessage("  " + juce::String(g.name).paddedRight(' ', 11)
                       + "|  " + juce::String(shippedHits).paddedLeft(' ', 2)
                       + "/" + juce::String(heldout::kVariations)
                       + "   |    " + juce::String(candidateHits).paddedLeft(' ', 2)
                       + "/" + juce::String(heldout::kVariations)
                       + "    | " + (delta >= 0 ? "+" : "") + juce::String(delta));
        }

        auto countCleanRuleFp = [](MLEngine& model, bool trainBand)
        {
            std::mt19937 cleanRng(trainBand ? 20260619u : 20260618u);
            int fp = 0;
            for (int v = 0; v < heldout::kVariations; ++v)
            {
                const auto lin = trainBand ? heldout::makeCleanTrainBand(cleanRng)
                                           : heldout::makeClean(cleanRng);
                if (!model.detectProblems(lin, kSampleRate).empty())
                    ++fp;
            }
            return fp;
        };

        const int shippedCleanTrain = countCleanRuleFp(shipped, true);
        const int candidateCleanTrain = countCleanRuleFp(ml, true);
        const int shippedCleanHeldout = countCleanRuleFp(shipped, false);
        const int candidateCleanHeldout = countCleanRuleFp(ml, false);
        logMessage("  -----------+---------+-----------+------");
        logMessage("  clean train-band L2   shipped " + juce::String(shippedCleanTrain) + "/"
                   + juce::String(heldout::kVariations) + " | candidate "
                   + juce::String(candidateCleanTrain) + "/" + juce::String(heldout::kVariations)
                   + "  (training tilt band -4.5..-1.5 dB/dec)");
        logMessage("  clean heldout-band L2 shipped " + juce::String(shippedCleanHeldout) + "/"
                   + juce::String(heldout::kVariations) + " | candidate "
                   + juce::String(candidateCleanHeldout) + "/" + juce::String(heldout::kVariations)
                   + "  (held-out tilt band -6.5..-5.0 dB/dec)");

        // Assertions
        expect(avgF1 >= kMinF1,
               "Retrained avg F1 too low: " + juce::String(avgF1 * 100.0f, 1)
               + "% (need >= " + juce::String(kMinF1 * 100.0f, 0) + "%)");

        expect(fpRate <= kMaxFPRate,
               "Retrained clean FP rate: " + juce::String(fpRate * 100.0f, 1)
               + "% (need <= " + juce::String(kMaxFPRate * 100.0f, 0) + "%)");

        for (int g = 0; g < numTypes; ++g)
        {
            expect(metrics[static_cast<size_t>(g)].recall() >= kMinRecall,
                   groups[g].name + " recall: " + juce::String(metrics[static_cast<size_t>(g)].recall() * 100.0f, 1)
                   + "% (need >= " + juce::String(kMinRecall * 100.0f, 0) + "%)");
        }
    }
};


// =============================================================================
// P4-D1 — ML recall separation: ORACLE vs RULE (DIAGNOSTIC, measurement-only).
//
// Disentangles WHERE the ML loses per-class recall, using the SHIPPED model,
// unchanged. Two levels (both pure MLEngine, so they run cleanly here):
//   L1 ML raw ORACLE   — model assigns raw prob > base threshold?
//                        (threshold only; NO margin, NO top-K rank cap)
//   L2 ML runtime RULE — MLEngine::detectProblems returns it?
//                        (threshold + per-class margin + top-K)
// Delta L1->L2 = the cost of the DECISION RULE (margin/top-K). This answers the
// core P4 question "does the model not know it, or does the rule reject it?".
//
// The third level (L3 AIEngine pipeline + vetoes) is the EXISTING "AI Accuracy —
// AIEngine Pipeline" test (KnownDebt). It cannot be measured from THIS binary
// because instantiating AIEngine + its test hooks here trips P2-HAZARD-001 (the
// JUCE_UNIT_TESTS object-layout divergence — same throw as AI Integration Audit).
// Once P2-HAZARD-001 is fixed, fold L3 in here. (Independently reconfirmed: an
// AIEngine path in this binary throws.)
//
// Touches no model/weights/thresholds. Hard assertion only on the structural
// invariant L1 >= L2 (the rule is a SUBSET of the oracle: it can remove, never add).
// =============================================================================
class AIAccuracyTest_ThreeLevel : public juce::UnitTest
{
public:
    AIAccuracyTest_ThreeLevel()
        : juce::UnitTest("AI Accuracy — Oracle vs Rule recall (P4-D1 diagnostic)", "AI-Diag") {}

    void runTest() override
    {
        beginTest("Per-class ML recall: raw oracle vs runtime rule (shipped model)");

        const juce::File modelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");
        expect(modelFile.existsAsFile(), "shipped ml_weights.bin not found");

        constexpr float kSens = 0.5f;                 // sensitivityScale = 1.0
        MLEngine ml;
        ml.initialize();
        const bool loaded = ml.loadWeights(modelFile);
        expect(loaded, "loadWeights failed on shipped model");
        ml.setSensitivity(kSens);
        const auto baseThr = ml.getBaseThresholds();

        struct Group {
            std::vector<TestCase>(*gen)(std::mt19937&);
            MLEngine::ProblemType ml;
            const char* name;
        };
        const Group groups[] = {
            { generateResonanceTests, MLEngine::ProblemType::Resonance,    "Resonance" },
            { generateHarshnessTests, MLEngine::ProblemType::Harshness,    "Harshness" },
            { generateMuddinessTests, MLEngine::ProblemType::Muddiness,    "Muddiness" },
            { generateSibilanceTests, MLEngine::ProblemType::Sibilance,    "Sibilance" },
            { generateBoominessTests, MLEngine::ProblemType::Boominess,    "Boominess" },
            { generateThinnessTests,  MLEngine::ProblemType::Thinness,     "Thinness" },
            { generateBoxyTests,      MLEngine::ProblemType::BoxyMidrange, "Boxyness" },
        };

        logMessage("  class      | meanRaw | thr  | L1 oracle | L2 rule | rule-cost");
        logMessage("  -----------+---------+------+-----------+---------+----------");

        std::mt19937 rng(42);
        for (const auto& g : groups)
        {
            const int mlIdx = static_cast<int>(g.ml);
            const float thr = baseThr[static_cast<size_t>(mlIdx)];
            auto cases = g.gen(rng);
            const int n = static_cast<int>(cases.size());
            int oracle = 0, rule = 0;
            double rawSum = 0.0;

            for (const auto& tc : cases)
            {
                const auto raws = ml.forwardRawProbabilities(tc.spectrum, kSampleRate);
                const float r = raws[static_cast<size_t>(mlIdx)];
                rawSum += r;
                if (r > thr) ++oracle;                                  // L1

                const auto dets = ml.detectProblems(tc.spectrum, kSampleRate);
                for (const auto& d : dets) if (d.type == g.ml) { ++rule; break; } // L2
            }

            auto pct = [n](int x){ return n > 0 ? 100.0f * static_cast<float>(x) / static_cast<float>(n) : 0.0f; };
            logMessage("  " + juce::String(g.name).paddedRight(' ', 11)
                       + "|  " + juce::String(rawSum / juce::jmax(1, n), 3)
                       + "  | " + juce::String(thr, 2)
                       + " |   " + juce::String(pct(oracle), 0) + "%"
                       + "    |  " + juce::String(pct(rule), 0) + "%"
                       + "   |  " + juce::String(pct(oracle) - pct(rule), 0) + "%");

            // Structural invariant: the rule is a strict subset of the oracle.
            expect(rule <= oracle,
                   juce::String(g.name) + ": rule recall (" + juce::String(rule)
                   + ") exceeds oracle recall (" + juce::String(oracle)
                   + ") — impossible unless the harness is wrong.");
        }
    }
};

// =============================================================================
// P4-D2 — HELD-OUT realistic recall across all THREE levels (DIAGNOSTIC).
//
// P4-D1 disentangled oracle-vs-rule on TRAINING-LIKE fixtures. P4-D2 answers the
// honest question: how does the SHIPPED model generalize to realistic, pink-tilted
// material it never saw in training, measured end-to-end through the production
// pipeline? Three levels, per class, on held-out fixtures (see namespace heldout):
//   L1 oracle   — pure MLEngine raw prob > base threshold (no margin, no top-K)
//   L2 rule     — pure MLEngine::detectProblems (margin + top-K)
//   L3 pipeline — full AIEngine::analyzeSpectrum → getPendingCorrections, forced
//                 onto the ML path with the SAME shipped weights, INCLUDING the
//                 production reality-check vetoes AND AIEngine's internal spectrum
//                 normalization.
//
// Hard assertion ONLY on the one structurally-guaranteed invariant: L2 ⊆ L1 (the
// runtime rule is a strict subset of the raw oracle — same model, same input, the
// rule can only remove). L3 is REPORTED, never asserted against L2: it runs the
// model on AIEngine's INTERNALLY-NORMALIZED spectrum (not the raw fixture L2 sees),
// so it is NOT a mathematical subset of L2 — the delta L2→L3 mixes the cost of
// normalization AND the reality-check vetoes, and that mix is itself the finding.
//
// Measurement-only: model, weights and thresholds are untouched. Now unblocked by
// the P2-HAZARD-001 resolution (AIEngine + its test hooks are safe in this binary).
// =============================================================================
class AIAccuracyTest_HeldOut : public juce::UnitTest
{
public:
    AIAccuracyTest_HeldOut()
        : juce::UnitTest("AI Accuracy — Held-out realistic recall L1/L2/L3 (P4-D2 diagnostic)", "AI-Diag") {}

    void runTest() override
    {
        beginTest("Held-out pink-tilt fixtures: oracle vs rule vs pipeline (shipped model)");

        const juce::File modelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");
        expect(modelFile.existsAsFile(), "shipped ml_weights.bin not found");

        constexpr float kSens = 0.5f;                 // sensitivityScale = 1.0 on both paths

        // L1/L2 — standalone MLEngine on the RAW linear fixture.
        MLEngine ml;
        ml.initialize();
        expect(ml.loadWeights(modelFile), "loadWeights failed on shipped model (ML)");
        ml.setSensitivity(kSens);
        const auto baseThr = ml.getBaseThresholds();

        // L3 — full AIEngine pipeline FORCED onto the ML path with the SAME weights.
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSensitivity(kSens);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);
        ai.forceMLDetectionEnabledForTests(true);
        expect(ai.setCustomMLWeightsPathForTests(modelFile), "loadWeights failed on shipped model (AIEngine)");

        struct Group {
            heldout::HClass        hc;
            MLEngine::ProblemType  mlt;
            AIEngine::ProblemType  ait;
            const char*            name;
        };
        const Group groups[] = {
            { heldout::HClass::Resonance, MLEngine::ProblemType::Resonance,    AIEngine::ProblemType::Resonance,  "Resonance" },
            { heldout::HClass::Harshness, MLEngine::ProblemType::Harshness,    AIEngine::ProblemType::Harshness,  "Harshness" },
            { heldout::HClass::Muddiness, MLEngine::ProblemType::Muddiness,    AIEngine::ProblemType::Muddiness,  "Muddiness" },
            { heldout::HClass::Sibilance, MLEngine::ProblemType::Sibilance,    AIEngine::ProblemType::Sibilance,  "Sibilance" },
            { heldout::HClass::Boominess, MLEngine::ProblemType::Boominess,    AIEngine::ProblemType::LowEndBoom, "Boominess" },
            { heldout::HClass::Boxyness,  MLEngine::ProblemType::BoxyMidrange, AIEngine::ProblemType::Boxyness,   "Boxyness" },
            { heldout::HClass::Thinness,  MLEngine::ProblemType::Thinness,     AIEngine::ProblemType::ThinSound,  "Thinness" },
        };

        logMessage("");
        logMessage("  HELD-OUT realistic eval — P4-M1 FROZEN DISJOINT (steep tilt -6.5..-5.0 dB/dec,");
        logMessage("    extrapolation band; P4-M2 training reserved to -4.5..-1.5; baseline -26 dB)");
        logMessage("  (raw counts out of " + juce::String(heldout::kVariations)
                   + "; o-r = oracle-rule (rule cost); r-p = rule-pipe, mixes AIEngine"
                   + " normalization + vetoes and may be < 0 since L3 is NOT a subset of L2)");
        logMessage("  class      | L1 oracle | L2 rule | L3 pipe |  o-r  |  r-p");
        logMessage("  -----------+-----------+---------+---------+-------+------");

        std::mt19937 rng(20260617);   // heldout_v2 seed (P4-M1 freeze; training uses 42 elsewhere)
        for (const auto& g : groups)
        {
            const int   mlIdx = static_cast<int>(g.mlt);
            const float thr   = baseThr[static_cast<size_t>(mlIdx)];
            int oracle = 0, rule = 0, pipe = 0;

            for (int v = 0; v < heldout::kVariations; ++v)
            {
                const auto lin = heldout::make(g.hc, rng);

                const auto raws = ml.forwardRawProbabilities(lin, kSampleRate);
                if (raws[static_cast<size_t>(mlIdx)] > thr) ++oracle;                      // L1

                const auto dets = ml.detectProblems(lin, kSampleRate);
                for (const auto& d : dets) if (d.type == g.mlt) { ++rule; break; }         // L2

                ai.analyzeSpectrum(linearToDb(lin), true);                                  // L3 (force: single-window)
                const auto pend = ai.getPendingCorrections();
                for (const auto& c : pend) if (c.type == g.ait) { ++pipe; break; }         // L3
            }

            const int n = heldout::kVariations;
            auto frac = [n](int x){ return (juce::String(x) + "/" + juce::String(n)); };
            auto sgn  = [](int d){ return (d > 0 ? "+" : "") + juce::String(d); };
            logMessage("  " + juce::String(g.name).paddedRight(' ', 11)
                       + "|   " + frac(oracle).paddedLeft(' ', 6)
                       + "  | " + frac(rule).paddedLeft(' ', 6)
                       + "  | " + frac(pipe).paddedLeft(' ', 6)
                       + "  | " + sgn(oracle - rule).paddedLeft(' ', 4)
                       + "  | " + sgn(rule - pipe).paddedLeft(' ', 4));

            // ONLY structural invariant: the runtime rule is a strict subset of the
            // raw oracle (same model, same input). L3 is reported, not asserted.
            expect(rule <= oracle,
                   juce::String(g.name) + ": rule recall (" + juce::String(rule)
                   + ") exceeds oracle recall (" + juce::String(oracle)
                   + ") — impossible unless the harness is wrong.");
        }

        // Clean control — FALSE POSITIVES on held-out clean tilt across all 3 levels.
        // Reported for completeness; NOT asserted (AI-Sweep's 0/18 floor is the
        // authoritative anti-hallucination gate). Counts any class firing on clean.
        {
            std::mt19937 cleanRng(20260618);   // heldout_v2 clean seed (P4-M1 freeze)
            int l1fp = 0, l2fp = 0, l3fp = 0;
            const int cleanN = heldout::kVariations;
            for (int v = 0; v < cleanN; ++v)
            {
                const auto lin = heldout::makeClean(cleanRng);

                const auto raws = ml.forwardRawProbabilities(lin, kSampleRate);
                for (int k = 0; k < MLEngine::numProblemTypes; ++k)
                    if (raws[static_cast<size_t>(k)] > baseThr[static_cast<size_t>(k)]) { ++l1fp; break; }

                if (!ml.detectProblems(lin, kSampleRate).empty()) ++l2fp;

                ai.analyzeSpectrum(linearToDb(lin), true);
                if (!ai.getPendingCorrections().empty()) ++l3fp;
            }
            auto cf = [cleanN](int x){ return (juce::String(x) + "/" + juce::String(cleanN)); };
            logMessage("  -----------+-----------+---------+---------+-------+------");
            logMessage("  clean FP   |   " + cf(l1fp).paddedLeft(' ', 6)
                       + "  | " + cf(l2fp).paddedLeft(' ', 6)
                       + "  | " + cf(l3fp).paddedLeft(' ', 6)
                       + "   (held-out clean tilt; reported, AI-Sweep 0/18 is authoritative)");
        }
    }
};

// =============================================================================
// P4-M2a-refine — A/B/C candidates vs shipped on the frozen held-out (DIAGNOSTIC).
//
// M2a (tilt-invariance, 2d10bfab) fixed the clean-tilt FP but OVER-SUPPRESSED real
// recall (Res 11→0, Mud 12→1, Boxy 12→2). This refine tests whether recall can be
// recovered WITHOUT reopening the FP, isolating two variables (Codex-approved 2×2):
//   prominence : RELATIVE-to-local-tilted-baseline (A,B) vs ABSOLUTE M2a (C)
//   weak-bumps : absent (A) vs present (B,C) — restored INTO the FIXED-size hard-neg
//                slot (50/50 clean-tilt + weak-bump), so the clean/positive ratio is
//                held fixed (Codex Q2).
// Candidates (MLEngine::DatasetOptions {relativeProminence, weakBumpNegatives}):
//   A={true,false}  B={true,true}  C={false,true}. Shipped + committed M2a {false,
//   false} are the references (the 2×2 corners).
//
// Measurement-only; candidate weights stay in /tmp (UNSHIPPED). Trains 300/300/0.005
// each (Codex Q4) from deterministic random init. Recall is L2 (detectProblems) at
// sens 0.5 on the FROZEN heldout_v2 fixtures, on BOTH the heldout extrapolation band
// AND the training band (Codex Q4). KnownDebt (non-blocking; slow — three retrains).
// =============================================================================
class AIAccuracyTest_M2aRefine : public juce::UnitTest
{
public:
    AIAccuracyTest_M2aRefine()
        : juce::UnitTest("AI Accuracy — P4-M2a-refine A/B/C (KnownDebt diagnostic)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("P4-M2a-refine: candidates A/B/C vs shipped on frozen held-out L2");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");

        struct Group { heldout::HClass hc; MLEngine::ProblemType type; const char* name; const char* target; };
        const Group groups[] = {
            { heldout::HClass::Resonance, MLEngine::ProblemType::Resonance,    "Resonance", ">=8" },
            { heldout::HClass::Harshness, MLEngine::ProblemType::Harshness,    "Harshness", "rep" },
            { heldout::HClass::Muddiness, MLEngine::ProblemType::Muddiness,    "Muddiness", ">=8" },
            { heldout::HClass::Sibilance, MLEngine::ProblemType::Sibilance,    "Sibilance", "rep" },
            { heldout::HClass::Boominess, MLEngine::ProblemType::Boominess,    "Boominess", ">=10" },
            { heldout::HClass::Boxyness,  MLEngine::ProblemType::BoxyMidrange, "Boxyness",  ">=7" },
            { heldout::HClass::Thinness,  MLEngine::ProblemType::Thinness,     "Thinness",  "rep" },
        };
        constexpr int kNumGroups = 7;

        // Frozen held-out fixtures, SAME sequence as P4-D2 (seed 20260617), built once
        // and reused for shipped + every candidate.
        std::array<std::vector<std::vector<float>>, kNumGroups> heldFixtures;
        {
            std::mt19937 rng(20260617);
            for (int g = 0; g < kNumGroups; ++g)
                for (int v = 0; v < heldout::kVariations; ++v)
                    heldFixtures[static_cast<size_t>(g)].push_back(heldout::make(groups[g].hc, rng));
        }
        std::vector<std::vector<float>> cleanTrain, cleanHeld;
        {
            std::mt19937 r1(20260619), r2(20260618);
            for (int v = 0; v < heldout::kVariations; ++v)
            {
                cleanTrain.push_back(heldout::makeCleanTrainBand(r1));
                cleanHeld.push_back(heldout::makeClean(r2));
            }
        }

        auto hits = [](MLEngine& m, const std::vector<std::vector<float>>& fx, MLEngine::ProblemType t)
        {
            int h = 0;
            for (const auto& lin : fx)
                for (const auto& d : m.detectProblems(lin, kSampleRate)) if (d.type == t) { ++h; break; }
            return h;
        };
        auto fp = [](MLEngine& m, const std::vector<std::vector<float>>& fx)
        {
            int f = 0;
            for (const auto& lin : fx) if (! m.detectProblems(lin, kSampleRate).empty()) ++f;
            return f;
        };
        auto frac = [](int x){ return juce::String(x).paddedLeft(' ', 2) + "/" + juce::String(heldout::kVariations); };

        MLEngine shipped;
        shipped.initialize();
        expect(shipped.loadWeights(shippedModelFile), "loadWeights failed on shipped model");
        shipped.setSensitivity(0.5f);

        struct Cand { const char* name; MLEngine::DatasetOptions opt; const char* file; };
        const Cand cands[] = {
            { "A rel/noWB", { true,  false }, "/tmp/aieq_m2a_refine_A.bin" },
            { "B rel/+WB",  { true,  true  }, "/tmp/aieq_m2a_refine_B.bin" },
            { "C abs/+WB",  { false, true  }, "/tmp/aieq_m2a_refine_C.bin" },
        };

        std::array<std::unique_ptr<MLEngine>, 3> models;
        for (int c = 0; c < 3; ++c)
        {
            auto m = std::make_unique<MLEngine>();
            m->initialize();
            m->initializeRandomWeights();
            auto ds = m->generateSyntheticDataset(300, kSampleRate, kFFTSize, cands[static_cast<size_t>(c)].opt);
            m->trainOnDataset(ds, 300, 0.005f);
            m->setSensitivity(0.5f);
            m->saveWeights(juce::File(cands[static_cast<size_t>(c)].file));   // /tmp ONLY — UNSHIPPED
            models[static_cast<size_t>(c)] = std::move(m);
        }

        logMessage("");
        logMessage("  P4-M2a-refine — held-out L2 recall (EXTRAPOLATION band -6.5..-5.0), n=" + juce::String(heldout::kVariations));
        logMessage("  A=rel/noWB  B=rel/+WB  C=abs/+WB   (refs: shipped; committed M2a was abs/noWB)");
        logMessage("  class      | shipped |   A   |   B   |   C   | target");
        logMessage("  -----------+---------+-------+-------+-------+-------");
        for (int g = 0; g < kNumGroups; ++g)
        {
            const auto t = groups[g].type;
            logMessage("  " + juce::String(groups[g].name).paddedRight(' ', 11)
                       + "|  " + frac(hits(shipped, heldFixtures[static_cast<size_t>(g)], t))
                       + "  | " + frac(hits(*models[0], heldFixtures[static_cast<size_t>(g)], t))
                       + " | " + frac(hits(*models[1], heldFixtures[static_cast<size_t>(g)], t))
                       + " | " + frac(hits(*models[2], heldFixtures[static_cast<size_t>(g)], t))
                       + " | " + juce::String(groups[g].target));
        }
        logMessage("  -----------+---------+-------+-------+-------+-------");
        logMessage("  clean train|  " + frac(fp(shipped, cleanTrain))
                   + "  | " + frac(fp(*models[0], cleanTrain))
                   + " | " + frac(fp(*models[1], cleanTrain))
                   + " | " + frac(fp(*models[2], cleanTrain)) + " | <=1");
        logMessage("  clean held |  " + frac(fp(shipped, cleanHeld))
                   + "  | " + frac(fp(*models[0], cleanHeld))
                   + " | " + frac(fp(*models[1], cleanHeld))
                   + " | " + frac(fp(*models[2], cleanHeld)) + " | <=1");
        logMessage("  (candidates saved to /tmp/aieq_m2a_refine_{A,B,C}.bin — UNSHIPPED)");
    }
};

// =============================================================================
// P4-M3-diagnostic — WHERE does narrow Resonance die? (measurement-only)
//
// Codex authorized this BEFORE choosing any M3 variant: prove the cause, do NOT
// assume "mel-resolution". Decisive observation from the A/B/C table: on the SAME
// frozen held-out Resonance fixtures, FLAT-trained shipped = 11/12 but every
// TILT-trained candidate (M2a 0, A 3, B 3, C 0) loses it. shipped and the
// candidates receive IDENTICAL mel input (same fixture → same extractMelBands), so
// mel-smearing affects both equally and CANNOT explain the gap — it is a TRAINING
// effect in the weights. The open question this answers: does the narrow peak
// survive the linear→mel transform well enough that a tilt-trained model COULD keep
// it (→ data/training fix), or does the energy/count normalization dilute it so much
// that better representation is needed (→ V0.5/V1)?
//
// Per the 12 held-out Resonance fixtures it reports:
//   • linear peak prominence (dB) in 250–4000 Hz
//   • mel-current  peak-band prominence — replicating extractMelBands' energy/COUNT
//   • mel-weightSum peak-band prominence — diagnostic energy/Σweight (NOT a runtime
//     change: computed locally here; the shipped extractMelBands is untouched)
//   • shipped L1 raw prob (Resonance) + L2 hit   vs   candidate-B L1 raw + L2 hit
// L1 is the crux (representation-vs-training): identical mel input → any L1 gap is
// purely weights. KnownDebt (non-blocking). Candidate B loaded from /tmp (else retrained).
// =============================================================================
class AIAccuracyTest_M3Diag : public juce::UnitTest
{
public:
    AIAccuracyTest_M3Diag()
        : juce::UnitTest("AI Accuracy — P4-M3 diagnostic: where Resonance dies (KnownDebt)", "KnownDebt") {}

    // Replicates extractMelBands' band energies (pre dB-inversion). byWeightSum=false
    // == the SHIPPED normalization (÷ bin count); true == diagnostic ÷ Σweight.
    static std::vector<float> melEnergies(const std::vector<float>& spectrum, double sr,
                                          int numBands, bool byWeightSum)
    {
        std::vector<float> out(static_cast<size_t>(numBands), 0.0f);
        if (spectrum.empty()) return out;
        const int   fftSize = static_cast<int>(spectrum.size()) * 2;
        const float binHz   = static_cast<float>(sr) / static_cast<float>(fftSize);
        auto hzToMel = [](float hz){ return 2595.0f * std::log10(1.0f + hz / 700.0f); };
        auto melToHz = [](float mel){ return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); };
        const float minMel = hzToMel(20.0f);
        const float maxMel = hzToMel(std::min(20000.0f, static_cast<float>(sr) * 0.5f));
        const float melStep = (maxMel - minMel) / static_cast<float>(numBands + 1);
        const int   hi = static_cast<int>(spectrum.size()) - 1;
        for (int band = 0; band < numBands; ++band)
        {
            const int binLow    = juce::jlimit(0, hi, static_cast<int>(melToHz(minMel + band * melStep) / binHz));
            const int binCenter = juce::jlimit(0, hi, static_cast<int>(melToHz(minMel + (band + 1) * melStep) / binHz));
            const int binHigh   = juce::jlimit(0, hi, static_cast<int>(melToHz(minMel + (band + 2) * melStep) / binHz));
            float energy = 0.0f, wsum = 0.0f; int count = 0;
            for (int bin = binLow; bin <= binHigh; ++bin)
            {
                float w = 0.0f;
                if (bin < binCenter && binCenter > binLow)        w = static_cast<float>(bin - binLow) / static_cast<float>(binCenter - binLow);
                else if (bin >= binCenter && binHigh > binCenter) w = static_cast<float>(binHigh - bin) / static_cast<float>(binHigh - binCenter);
                energy += spectrum[static_cast<size_t>(bin)] * w; ++count; wsum += w;
            }
            const float denom = byWeightSum ? juce::jmax(1.0e-6f, wsum) : static_cast<float>(juce::jmax(1, count));
            out[static_cast<size_t>(band)] = energy / denom;
        }
        return out;
    }

    // Prominence (dB) of the strongest value in [loHz,hiHz] vs the median there.
    static float bandPromDb(const std::vector<float>& vals, double sr, int fftSizeForBinHz,
                            float loHz, float hiHz)
    {
        std::vector<float> inRange;
        const float binHz = static_cast<float>(sr) / static_cast<float>(fftSizeForBinHz);
        float peak = 0.0f;
        for (int i = 0; i < static_cast<int>(vals.size()); ++i)
        {
            const float f = static_cast<float>(i) * binHz;
            if (f >= loHz && f <= hiHz) { inRange.push_back(vals[static_cast<size_t>(i)]); peak = juce::jmax(peak, vals[static_cast<size_t>(i)]); }
        }
        if (inRange.size() < 3) return 0.0f;
        std::sort(inRange.begin(), inRange.end());
        const float med = juce::jmax(1.0e-9f, inRange[inRange.size() / 2]);
        return 20.0f * std::log10(juce::jmax(1.0e-9f, peak) / med);
    }
    // Same but the "bin spacing" for a mel-band vector is per-band, so map band->Hz via mel.
    static float melBandPromDb(const std::vector<float>& bands, double sr, float loHz, float hiHz)
    {
        auto melToHz = [](float mel){ return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); };
        auto hzToMel = [](float hz){ return 2595.0f * std::log10(1.0f + hz / 700.0f); };
        const int n = static_cast<int>(bands.size());
        const float minMel = hzToMel(20.0f), maxMel = hzToMel(std::min(20000.0f, static_cast<float>(sr) * 0.5f));
        const float melStep = (maxMel - minMel) / static_cast<float>(n + 1);
        std::vector<float> inRange; float peak = 0.0f;
        for (int b = 0; b < n; ++b)
        {
            const float f = melToHz(minMel + (b + 1) * melStep);
            if (f >= loHz && f <= hiHz) { inRange.push_back(bands[static_cast<size_t>(b)]); peak = juce::jmax(peak, bands[static_cast<size_t>(b)]); }
        }
        if (inRange.size() < 3) return 0.0f;
        std::sort(inRange.begin(), inRange.end());
        const float med = juce::jmax(1.0e-9f, inRange[inRange.size() / 2]);
        return 20.0f * std::log10(juce::jmax(1.0e-9f, peak) / med);
    }

    void runTest() override
    {
        beginTest("P4-M3-diag: shipped vs candidate-B on held-out Resonance + mel transform");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");
        MLEngine shipped; shipped.initialize();
        expect(shipped.loadWeights(shippedModelFile), "loadWeights failed on shipped model");
        shipped.setSensitivity(0.5f);

        // Candidate B (relative prominence + weak-bumps): load /tmp, else retrain deterministically.
        MLEngine candB; candB.initialize();
        const juce::File bFile("/tmp/aieq_m2a_refine_B.bin");
        if (! (bFile.existsAsFile() && candB.loadWeights(bFile)))
        {
            candB.initializeRandomWeights();
            auto ds = candB.generateSyntheticDataset(300, kSampleRate, kFFTSize,
                                                     MLEngine::DatasetOptions{ true, true });
            candB.trainOnDataset(ds, 300, 0.005f);
        }
        candB.setSensitivity(0.5f);

        const auto  baseThr = shipped.getBaseThresholds();
        const int   resIdx  = static_cast<int>(MLEngine::ProblemType::Resonance);
        const float thr     = baseThr[static_cast<size_t>(resIdx)];

        // SAME 12 Resonance fixtures as the A/B/C table (group 0, seed 20260617).
        std::mt19937 rng(20260617);
        std::vector<std::vector<float>> fx;
        for (int v = 0; v < heldout::kVariations; ++v)
            fx.push_back(heldout::make(heldout::HClass::Resonance, rng));

        logMessage("");
        logMessage("  P4-M3-diag — held-out Resonance (steep tilt). prom in dB; L1 raw vs thr " + juce::String(thr, 2));
        logMessage("  #  | linP, melCnt, melWsum |  shipL1  shipL2 |  B_L1   B_L2");
        logMessage("  ---+-----------------------+----------------+--------------");
        int shipHit = 0, bHit = 0; double shipL1Sum = 0, bL1Sum = 0;
        for (int v = 0; v < heldout::kVariations; ++v)
        {
            const auto& s = fx[static_cast<size_t>(v)];
            const int fftSizeForBinHz = static_cast<int>(s.size()) * 2;
            const float linProm  = bandPromDb(s, kSampleRate, fftSizeForBinHz, 250.0f, 4000.0f);
            const float melCnt   = melBandPromDb(melEnergies(s, kSampleRate, 64, false), kSampleRate, 250.0f, 4000.0f);
            const float melWsum  = melBandPromDb(melEnergies(s, kSampleRate, 64, true ), kSampleRate, 250.0f, 4000.0f);

            const float sL1 = shipped.forwardRawProbabilities(s, kSampleRate)[static_cast<size_t>(resIdx)];
            const float bL1 = candB .forwardRawProbabilities(s, kSampleRate)[static_cast<size_t>(resIdx)];
            bool sL2 = false, bL2 = false;
            for (const auto& d : shipped.detectProblems(s, kSampleRate)) if (d.type == MLEngine::ProblemType::Resonance) { sL2 = true; break; }
            for (const auto& d : candB .detectProblems(s, kSampleRate)) if (d.type == MLEngine::ProblemType::Resonance) { bL2 = true; break; }
            shipHit += sL2 ? 1 : 0; bHit += bL2 ? 1 : 0; shipL1Sum += sL1; bL1Sum += bL1;

            logMessage("  " + juce::String(v).paddedLeft(' ', 2)
                       + " | " + juce::String(linProm, 1).paddedLeft(' ', 5)
                       + ", " + juce::String(melCnt, 1).paddedLeft(' ', 5)
                       + ", " + juce::String(melWsum, 1).paddedLeft(' ', 5)
                       + " |  " + juce::String(sL1, 2) + "   " + (sL2 ? juce::String("Y") : juce::String("."))
                       + "    |  " + juce::String(bL1, 2) + "  " + (bL2 ? juce::String("Y") : juce::String(".")));
        }
        const int n = heldout::kVariations;
        logMessage("  ---+-----------------------+----------------+--------------");
        logMessage("  shipped: L2 " + juce::String(shipHit) + "/" + juce::String(n)
                   + ", mean L1 " + juce::String(shipL1Sum / n, 3)
                   + "  |  candidate B: L2 " + juce::String(bHit) + "/" + juce::String(n)
                   + ", mean L1 " + juce::String(bL1Sum / n, 3));
        logMessage("  READ: identical mel input to both models → any shipL1>>B_L1 gap is TRAINING, not");
        logMessage("  representation. melCnt vs melWsum shows if ÷count dilutes the narrow peak vs ÷Σweight.");
    }
};

// =============================================================================
// P4-M2a-refine-2 / D1 — Resonance recovery by RESAMPLING (DIAGNOSTIC, /tmp, no ship).
//
// Double-counter-examined (two agents, Codex offline). Cause (M3-diag c085bc9a): NOT
// representation — B's tilt-invariance over-suppresses narrow low/mid on the steep eval
// band. D1 = candidate B recipe + N EXTRA Resonance-only positives (resampling, dedicated
// RNG; NOT loss-weighting — trainStep has no per-class weight) to rebalance the one class
// losing the FP↔recall trade. Training tilt UNCHANGED (-4.5..-1.5): D1 does NOT touch the
// disjointness contract. Acceptance: Resonance heldout L2 3→≥8 WITHOUT losing
// Mud≥8/Boxy≥7/Boom≥10 or clean FP (≤1 both bands). Reports Resonance L1 AND L2 on BOTH
// train-band and heldout-band (distinguish "doesn't learn" from "doesn't extrapolate").
// KnownDebt (non-blocking). NOTE: AI-Sweep/AI-Corpus pass trivially here — nothing ships.
// =============================================================================
class AIAccuracyTest_M2aRefine2 : public juce::UnitTest
{
public:
    AIAccuracyTest_M2aRefine2()
        : juce::UnitTest("AI Accuracy — P4-M2a-refine-2 D1 Resonance (KnownDebt diagnostic)", "KnownDebt") {}

    // A Resonance fixture at an arbitrary tilt band, reusing the FROZEN heldout primitives
    // and the exact eval Resonance shape (freq 250-4000, +14..20 dB, sigma-factor 0.04).
    static std::vector<float> makeResAtTilt(std::mt19937& rng, float minSlope, float maxSlope)
    {
        std::vector<float> db(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
        heldout::addPinkTiltDb(db, std::uniform_real_distribution<float>(minSlope, maxSlope)(rng));
        auto U = [&rng](float a, float b){ return std::uniform_real_distribution<float>(a, b)(rng); };
        heldout::addGaussianPeakDb(db, U(250.0f, 4000.0f), U(14.0f, 20.0f), 0.04f);
        heldout::addNoiseDb(db, rng, 1.0f);
        return heldout::toLinear(db);
    }

    void runTest() override
    {
        beginTest("P4-M2a-refine-2 D1: extra Resonance positives vs shipped/B");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");
        MLEngine shipped; shipped.initialize();
        expect(shipped.loadWeights(shippedModelFile), "loadWeights failed on shipped model");
        shipped.setSensitivity(0.5f);

        // Candidate B (load /tmp, else retrain deterministically).
        MLEngine B; B.initialize();
        const juce::File bFile("/tmp/aieq_m2a_refine_B.bin");
        if (! (bFile.existsAsFile() && B.loadWeights(bFile)))
        {
            B.initializeRandomWeights();
            auto ds = B.generateSyntheticDataset(300, kSampleRate, kFFTSize, MLEngine::DatasetOptions{ true, true, 0 });
            B.trainOnDataset(ds, 300, 0.005f);
        }
        B.setSensitivity(0.5f);

        // D1 = B recipe + 300 extra Resonance positives (doubles the Resonance class).
        MLEngine D1; D1.initialize(); D1.initializeRandomWeights();
        {
            auto ds = D1.generateSyntheticDataset(300, kSampleRate, kFFTSize,
                                                  MLEngine::DatasetOptions{ true, true, 300 });
            D1.trainOnDataset(ds, 300, 0.005f);
        }
        D1.setSensitivity(0.5f);
        D1.saveWeights(juce::File("/tmp/aieq_m2a_refine_D1.bin"));   // /tmp ONLY — UNSHIPPED

        const auto  baseThr = shipped.getBaseThresholds();
        const int   resIdx  = static_cast<int>(MLEngine::ProblemType::Resonance);
        const float thr     = baseThr[static_cast<size_t>(resIdx)];

        struct Group { heldout::HClass hc; MLEngine::ProblemType type; const char* name; };
        const Group groups[] = {
            { heldout::HClass::Resonance, MLEngine::ProblemType::Resonance,    "Resonance" },
            { heldout::HClass::Harshness, MLEngine::ProblemType::Harshness,    "Harshness" },
            { heldout::HClass::Muddiness, MLEngine::ProblemType::Muddiness,    "Muddiness" },
            { heldout::HClass::Sibilance, MLEngine::ProblemType::Sibilance,    "Sibilance" },
            { heldout::HClass::Boominess, MLEngine::ProblemType::Boominess,    "Boominess" },
            { heldout::HClass::Boxyness,  MLEngine::ProblemType::BoxyMidrange, "Boxyness" },
            { heldout::HClass::Thinness,  MLEngine::ProblemType::Thinness,     "Thinness" },
        };
        constexpr int kNumGroups = 7;

        std::array<std::vector<std::vector<float>>, kNumGroups> held;
        {
            std::mt19937 rng(20260617);
            for (int g = 0; g < kNumGroups; ++g)
                for (int v = 0; v < heldout::kVariations; ++v)
                    held[static_cast<size_t>(g)].push_back(heldout::make(groups[g].hc, rng));
        }
        std::vector<std::vector<float>> cleanTrain, cleanHeld;
        {
            std::mt19937 r1(20260619), r2(20260618);
            for (int v = 0; v < heldout::kVariations; ++v)
            {
                cleanTrain.push_back(heldout::makeCleanTrainBand(r1));
                cleanHeld.push_back(heldout::makeClean(r2));
            }
        }

        auto l2hits = [](MLEngine& m, const std::vector<std::vector<float>>& fx, MLEngine::ProblemType t)
        { int h = 0; for (const auto& s : fx) for (const auto& d : m.detectProblems(s, kSampleRate)) if (d.type == t) { ++h; break; } return h; };
        auto fp = [](MLEngine& m, const std::vector<std::vector<float>>& fx)
        { int f = 0; for (const auto& s : fx) if (! m.detectProblems(s, kSampleRate).empty()) ++f; return f; };
        auto l1mean = [resIdx](MLEngine& m, const std::vector<std::vector<float>>& fx)
        { double s = 0; for (const auto& v : fx) s += m.forwardRawProbabilities(v, kSampleRate)[static_cast<size_t>(resIdx)]; return fx.empty() ? 0.0 : s / fx.size(); };
        auto frac = [](int x){ return juce::String(x).paddedLeft(' ', 2) + "/" + juce::String(heldout::kVariations); };

        logMessage("");
        logMessage("  P4-M2a-refine-2 D1 — per-class L2 (held-out extrapolation band), n=" + juce::String(heldout::kVariations));
        logMessage("  class      | shipped |   B   |   D1  | target");
        logMessage("  -----------+---------+-------+-------+-------");
        const char* tg[] = { ">=8", "rep", ">=8", "rep", ">=10", ">=7", "rep" };
        for (int g = 0; g < kNumGroups; ++g)
            logMessage("  " + juce::String(groups[g].name).paddedRight(' ', 11)
                       + "|  " + frac(l2hits(shipped, held[static_cast<size_t>(g)], groups[g].type))
                       + "  | " + frac(l2hits(B,       held[static_cast<size_t>(g)], groups[g].type))
                       + " | " + frac(l2hits(D1,      held[static_cast<size_t>(g)], groups[g].type))
                       + " | " + juce::String(tg[g]));
        logMessage("  -----------+---------+-------+-------+-------");
        logMessage("  clean train|  " + frac(fp(shipped, cleanTrain)) + "  | " + frac(fp(B, cleanTrain)) + " | " + frac(fp(D1, cleanTrain)) + " | <=1");
        logMessage("  clean held |  " + frac(fp(shipped, cleanHeld )) + "  | " + frac(fp(B, cleanHeld )) + " | " + frac(fp(D1, cleanHeld )) + " | <=1");

        // Resonance L1 (raw) AND L2 on BOTH bands — distinguish "doesn't learn" from "doesn't extrapolate".
        std::vector<std::vector<float>> resTrain;
        { std::mt19937 r(20260620); for (int v = 0; v < heldout::kVariations; ++v) resTrain.push_back(makeResAtTilt(r, heldout::kTrainTiltMinSlope, heldout::kTrainTiltMaxSlope)); }
        const auto& resHeld = held[0];   // heldout-band Resonance (group 0)
        auto rt = MLEngine::ProblemType::Resonance;
        logMessage("  Resonance L1(mean raw, thr " + juce::String(thr, 2) + ") / L2(hits):");
        logMessage("    band      | shipped         |   B             |   D1");
        auto row = [&](const char* band, const std::vector<std::vector<float>>& fx){
            logMessage(juce::String("    ") + juce::String(band).paddedRight(' ', 10)
                       + "| " + juce::String(l1mean(shipped, fx), 2) + " / " + frac(l2hits(shipped, fx, rt))
                       + "  | " + juce::String(l1mean(B, fx), 2) + " / " + frac(l2hits(B, fx, rt))
                       + "  | " + juce::String(l1mean(D1, fx), 2) + " / " + frac(l2hits(D1, fx, rt))); };
        row("train-band", resTrain);
        row("heldout   ", resHeld);
        logMessage("  (D1 candidate saved /tmp/aieq_m2a_refine_D1.bin — UNSHIPPED)");
    }
};

// =============================================================================
// P4-M3-Q — does the PIPELINE catch Resonance the ML misses? (free measurement)
//
// All prior Resonance numbers were pure-ML L2. The shipped PRODUCT runs the full
// AIEngine pipeline, which has an INDEPENDENT DSP/heuristic resonance detector
// (prominence-based — good at narrow peaks). If the pipeline catches the held-out
// resonances regardless of the ML, then candidate B's ML weakness on Resonance is
// MOOT for the product, and the risky V1 schema change is unnecessary.
// Measures, on the 12 frozen held-out Resonance fixtures (steep tilt) + clean controls,
// AIEngine Resonance recall and clean FP in:
//   HeuristicOnly  — DSP resonance detector ALONE (no ML): the decisive test
//   Hybrid+shipped — current product path
//   Hybrid+B       — product path if candidate B were shipped
//   MLOnly+B       — B's ML through the pipeline (≈ B's L2), for reference
// Measurement-only, no retrain (B loaded from /tmp, else retrained). KnownDebt.
// =============================================================================
class AIAccuracyTest_ResPipeline : public juce::UnitTest
{
public:
    AIAccuracyTest_ResPipeline()
        : juce::UnitTest("AI Accuracy — Resonance via PIPELINE not ML (KnownDebt diagnostic)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Does the AIEngine pipeline catch held-out Resonance the ML misses?");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");

        // Ensure candidate B exists in /tmp (retrain if needed).
        juce::File bFile("/tmp/aieq_m2a_refine_B.bin");
        if (! bFile.existsAsFile())
        {
            MLEngine B; B.initialize(); B.initializeRandomWeights();
            auto ds = B.generateSyntheticDataset(300, kSampleRate, kFFTSize, MLEngine::DatasetOptions{ true, true, 0 });
            B.trainOnDataset(ds, 300, 0.005f);
            B.saveWeights(bFile);
        }

        // Frozen held-out Resonance (group 0, seed 20260617) + clean controls.
        std::vector<std::vector<float>> resFx, cleanTrain, cleanHeld;
        { std::mt19937 r(20260617); for (int v = 0; v < heldout::kVariations; ++v) resFx.push_back(heldout::make(heldout::HClass::Resonance, r)); }
        { std::mt19937 r1(20260619), r2(20260618); for (int v = 0; v < heldout::kVariations; ++v) { cleanTrain.push_back(heldout::makeCleanTrainBand(r1)); cleanHeld.push_back(heldout::makeClean(r2)); } }

        auto aiHits = [](AIEngine& ai, const std::vector<std::vector<float>>& fx)
        {
            int h = 0;
            for (const auto& lin : fx)
            {
                ai.analyzeSpectrum(linearToDb(lin), true);
                for (const auto& c : ai.getPendingCorrections()) if (c.type == AIEngine::ProblemType::Resonance) { ++h; break; }
            }
            return h;
        };
        auto aiFp = [](AIEngine& ai, const std::vector<std::vector<float>>& fx)
        {
            int f = 0;
            for (const auto& lin : fx) { ai.analyzeSpectrum(linearToDb(lin), true); if (! ai.getPendingCorrections().empty()) ++f; }
            return f;
        };

        struct Cfg { const char* name; AIEngine::DetectionBackendMode mode; const juce::File* weights; };
        const Cfg cfgs[] = {
            { "HeuristicOnly (DSP alone)", AIEngine::DetectionBackendMode::HeuristicOnly, nullptr },
            { "Hybrid + shipped ML",       AIEngine::DetectionBackendMode::Hybrid,        &shippedModelFile },
            { "Hybrid + B ML",             AIEngine::DetectionBackendMode::Hybrid,        &bFile },
            { "MLOnly + B ML",             AIEngine::DetectionBackendMode::MLOnly,        &bFile },
        };

        logMessage("");
        logMessage("  PIPELINE Resonance recall + clean FP (held-out, n=" + juce::String(heldout::kVariations) + ", force=single-window)");
        logMessage("  config                      | Res recall | clean-train FP | clean-held FP");
        logMessage("  ----------------------------+------------+----------------+--------------");
        for (const auto& c : cfgs)
        {
            AIEngine ai;
            ai.prepare(kSampleRate, 512);
            ai.setEnabled(true);
            ai.setSensitivity(0.5f);
            ai.setSourceProfile(AIEngine::SourceProfile::Generic);
            ai.setDetectionBackendMode(c.mode);
            if (c.weights != nullptr) { ai.setCustomMLWeightsPathForTests(*c.weights); ai.forceMLDetectionEnabledForTests(true); }
            auto frac = [](int x){ return juce::String(x).paddedLeft(' ', 2) + "/" + juce::String(heldout::kVariations); };
            logMessage("  " + juce::String(c.name).paddedRight(' ', 27)
                       + "|    " + frac(aiHits(ai, resFx)).paddedLeft(' ', 5)
                       + "   |     " + frac(aiFp(ai, cleanTrain)).paddedLeft(' ', 5)
                       + "      |    " + frac(aiFp(ai, cleanHeld)).paddedLeft(' ', 5));
        }
        logMessage("  READ: if HeuristicOnly Res recall is high with clean FP low, the product's DSP path");
        logMessage("  catches narrow resonances regardless of the ML → candidate B's ML Resonance gap is");
        logMessage("  MOOT for the product and the V1 schema change is unnecessary.");
    }
};

// =============================================================================
// P4-M3-SHIP — PRODUCT-LEVEL (Hybrid pipeline) recall, B vs shipped, ALL classes.
//
// All prior per-class tables were pure-ML L2. This measures what the SHIPPED PRODUCT
// actually delivers — the full AIEngine pipeline in Hybrid mode (ML + heuristic + DSP
// vetoes) — for the SHIPPED model vs candidate B, across all 7 classes + clean FP, on
// the frozen held-out. Decides whether B is product-grade: recovers the broad classes
// AND keeps the clean-tilt FP at 0, with Resonance being a pre-existing system-wide weak
// spot (not a B regression). Measurement-only, no retrain (B from /tmp), KnownDebt.
// =============================================================================
class AIAccuracyTest_PipelineAllClasses : public juce::UnitTest
{
public:
    AIAccuracyTest_PipelineAllClasses()
        : juce::UnitTest("AI Accuracy — PRODUCT pipeline B vs shipped, all classes (KnownDebt)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Hybrid pipeline recall + clean FP: shipped vs candidate B, all 7 classes");

        const juce::File shippedModelFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");
        juce::File bFile("/tmp/aieq_m2a_refine_B.bin");
        if (! bFile.existsAsFile())
        {
            MLEngine B; B.initialize(); B.initializeRandomWeights();
            auto ds = B.generateSyntheticDataset(300, kSampleRate, kFFTSize, MLEngine::DatasetOptions{ true, true, 0 });
            B.trainOnDataset(ds, 300, 0.005f);
            B.saveWeights(bFile);
        }

        struct Group { heldout::HClass hc; AIEngine::ProblemType type; const char* name; };
        const Group groups[] = {
            { heldout::HClass::Resonance, AIEngine::ProblemType::Resonance,  "Resonance" },
            { heldout::HClass::Harshness, AIEngine::ProblemType::Harshness,  "Harshness" },
            { heldout::HClass::Muddiness, AIEngine::ProblemType::Muddiness,  "Muddiness" },
            { heldout::HClass::Sibilance, AIEngine::ProblemType::Sibilance,  "Sibilance" },
            { heldout::HClass::Boominess, AIEngine::ProblemType::LowEndBoom, "Boominess" },
            { heldout::HClass::Boxyness,  AIEngine::ProblemType::Boxyness,   "Boxyness" },
            { heldout::HClass::Thinness,  AIEngine::ProblemType::ThinSound,  "Thinness" },
        };
        constexpr int kNumGroups = 7;

        std::array<std::vector<std::vector<float>>, kNumGroups> held;
        { std::mt19937 r(20260617); for (int g = 0; g < kNumGroups; ++g) for (int v = 0; v < heldout::kVariations; ++v) held[static_cast<size_t>(g)].push_back(heldout::make(groups[g].hc, r)); }
        std::vector<std::vector<float>> cleanTrain, cleanHeld;
        { std::mt19937 r1(20260619), r2(20260618); for (int v = 0; v < heldout::kVariations; ++v) { cleanTrain.push_back(heldout::makeCleanTrainBand(r1)); cleanHeld.push_back(heldout::makeClean(r2)); } }

        auto makeHybrid = [&](const juce::File& w){
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(kSampleRate, 512);
            ai->setEnabled(true);
            ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::Hybrid);
            ai->setCustomMLWeightsPathForTests(w);
            ai->forceMLDetectionEnabledForTests(true);
            return ai;
        };
        auto hits = [](AIEngine& ai, const std::vector<std::vector<float>>& fx, AIEngine::ProblemType t){
            int h = 0; for (const auto& lin : fx) { ai.analyzeSpectrum(linearToDb(lin), true); for (const auto& c : ai.getPendingCorrections()) if (c.type == t) { ++h; break; } } return h; };
        auto fp = [](AIEngine& ai, const std::vector<std::vector<float>>& fx){
            int f = 0; for (const auto& lin : fx) { ai.analyzeSpectrum(linearToDb(lin), true); if (! ai.getPendingCorrections().empty()) ++f; } return f; };
        auto frac = [](int x){ return juce::String(x).paddedLeft(' ', 2) + "/" + juce::String(heldout::kVariations); };

        auto shipped = makeHybrid(shippedModelFile);
        auto B       = makeHybrid(bFile);

        logMessage("");
        logMessage("  PRODUCT pipeline (Hybrid) recall — held-out extrapolation band, n=" + juce::String(heldout::kVariations));
        logMessage("  class      | shipped | candidate B");
        logMessage("  -----------+---------+------------");
        for (int g = 0; g < kNumGroups; ++g)
            logMessage("  " + juce::String(groups[g].name).paddedRight(' ', 11)
                       + "|  " + frac(hits(*shipped, held[static_cast<size_t>(g)], groups[g].type))
                       + "  |   " + frac(hits(*B, held[static_cast<size_t>(g)], groups[g].type)));
        logMessage("  -----------+---------+------------");
        logMessage("  clean train|  " + frac(fp(*shipped, cleanTrain)) + "  |   " + frac(fp(*B, cleanTrain)));
        logMessage("  clean held |  " + frac(fp(*shipped, cleanHeld )) + "  |   " + frac(fp(*B, cleanHeld )));
        logMessage("  READ: this is what the PRODUCT delivers (ML+heuristic+vetoes). B is product-grade iff");
        logMessage("  it recovers broad classes and holds clean FP ~0, with Resonance a pre-existing weak spot.");
    }
};

// =============================================================================
// P4-Veto-Sib — per-stage ATTRIBUTION: where does Sibilance die? (Codex-scoped)
//
// Isolates the Sibilance drop across the pipeline stages, on the SAME fixtures, for
// the shipped model AND a DETERMINISTICALLY-REPRODUCED B (no stale /tmp; identity
// logged: path/bytes/checksum; loadWeights expect()ed). Uses the existing AIEngine
// audit hooks — NO veto/production change:
//   rawIn   = ML raw prob on the RAW fixture            (forwardRawProbabilities)
//   normIn  = ML raw prob on AIEngine's NORMALIZED frame (getLastMLRawProbabilitiesForTests)
//   effThr  = effective threshold in the pipeline        (getLastMLThresholdsForTests)
//   detect  = survived rule + veto?                      (getPendingCorrections, MLOnly)
// READ: rawIn>>normIn → NORMALIZATION drops it; normIn>effThr but detect=0 → RULE/VETO
// drops it; normIn low → MODEL. ALL fixtures are SYNTHETIC (synthetic Sibilance Gaussians @
// intensities + clean steep + clean bright (HF shelf) + a broad-HF synthetic negative — NOT
// real cymbal audio). KnownDebt. No product change. Closes ONLY the synthetic attribution.
// =============================================================================
class AIAccuracyTest_VetoSibAttribution : public juce::UnitTest
{
public:
    AIAccuracyTest_VetoSibAttribution()
        : juce::UnitTest("AI Accuracy — P4-Veto-Sib per-stage attribution (KnownDebt)", "KnownDebt") {}

    static void addHfShelfDb(std::vector<float>& db, float cutoffHz, float gainDb)
    {
        const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
        for (int i = 0; i < kNumBins; ++i) { const float f = static_cast<float>(i) * binHz; if (f > cutoffHz) db[static_cast<size_t>(i)] += gainDb * juce::jlimit(0.0f, 1.0f, (f - cutoffHz) / cutoffHz); }
    }
    static std::vector<float> sibFx(std::mt19937& r, float peakDb)
    {
        std::vector<float> db(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
        heldout::addPinkTiltDb(db, std::uniform_real_distribution<float>(heldout::kEvalTiltMinSlope, heldout::kEvalTiltMaxSlope)(r));
        heldout::addGaussianPeakDb(db, std::uniform_real_distribution<float>(5500.0f, 8500.0f)(r), peakDb, 0.18f);
        heldout::addNoiseDb(db, r, 1.0f); return heldout::toLinear(db);
    }
    static std::vector<float> cleanBrightFx(std::mt19937& r)
    {
        std::vector<float> db(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
        heldout::addPinkTiltDb(db, std::uniform_real_distribution<float>(heldout::kEvalTiltMinSlope, heldout::kEvalTiltMaxSlope)(r));
        addHfShelfDb(db, 5000.0f, 6.0f); heldout::addNoiseDb(db, r, 1.0f); return heldout::toLinear(db);
    }
    static std::vector<float> hfNoiseFx(std::mt19937& r)
    {
        std::vector<float> db(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
        heldout::addPinkTiltDb(db, std::uniform_real_distribution<float>(heldout::kEvalTiltMinSlope, heldout::kEvalTiltMaxSlope)(r));
        heldout::addGaussianPeakDb(db, 9000.0f, 8.0f, 0.5f);   // broad HF hump (cymbal-ish, NOT a sibilance peak)
        heldout::addNoiseDb(db, r, 3.0f); return heldout::toLinear(db);
    }

    void runTest() override
    {
        beginTest("Per-stage attribution: where does Sibilance die? (shipped vs reproduced B)");

        const juce::File shippedFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");

        // B reproduced DETERMINISTICALLY (no stale /tmp), identity verified below.
        MLEngine Bml; Bml.initialize(); Bml.initializeRandomWeights();
        { auto ds = Bml.generateSyntheticDataset(300, kSampleRate, kFFTSize, MLEngine::DatasetOptions{ true, true, 0 }); Bml.trainOnDataset(ds, 300, 0.005f); }
        const juce::File bFile("/tmp/aieq_vetosib_B.bin");
        expect(Bml.saveWeights(bFile), "saveWeights(B) failed");

        const int   sib        = static_cast<int>(MLEngine::ProblemType::Sibilance);
        auto makeAi = [&](const juce::File& w, const char* who) -> std::unique_ptr<AIEngine>
        {
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(kSampleRate, 512); ai->setEnabled(true); ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);
            expect(ai->setCustomMLWeightsPathForTests(w), juce::String("AIEngine loadWeights failed: ") + who);
            auto& m = ai->getMLEngineForTest();
            logMessage(juce::String("  ") + who + " identity: " + m.getLoadedWeightsPath()
                       + " (" + juce::String(m.getLoadedWeightsBytes()) + " bytes, fnv " + m.getLoadedWeightsChecksum() + ")");
            return ai;
        };
        auto aiShipped = makeAi(shippedFile, "shipped ");
        auto aiB       = makeAi(bFile,       "B(repro)");

        // Checksum compare: re-load the saved B and verify it matches what the AIEngine loaded.
        {
            MLEngine verify; verify.initialize();
            expect(verify.loadWeights(bFile), "re-load of saved B failed");
            const auto onDisk = verify.getLoadedWeightsChecksum();
            const auto inAi   = aiB->getMLEngineForTest().getLoadedWeightsChecksum();
            expect(onDisk == inAi, "B checksum mismatch saved-vs-loaded: " + onDisk + " != " + inAi);
            logMessage("  B checksum verified (saved==loaded): " + onDisk);
        }

        struct Cat { juce::String name; std::vector<std::vector<float>> fx; };
        std::vector<Cat> cats;
        auto build = [](const juce::String& nm, uint32_t seed, int n, std::function<std::vector<float>(std::mt19937&)> g){
            std::mt19937 r(seed); std::vector<std::vector<float>> v; v.reserve(static_cast<size_t>(n)); for (int i = 0; i < n; ++i) v.push_back(g(r)); return Cat{ nm, std::move(v) }; };
        cats.push_back(build("syn-sib08", 30001, 8, [](std::mt19937& r){ return sibFx(r, 8.0f); }));
        cats.push_back(build("syn-sib12", 30002, 8, [](std::mt19937& r){ return sibFx(r, 12.0f); }));
        cats.push_back(build("syn-sib16", 30003, 8, [](std::mt19937& r){ return sibFx(r, 16.0f); }));
        cats.push_back(build("syn-sib20", 30004, 8, [](std::mt19937& r){ return sibFx(r, 20.0f); }));
        cats.push_back(build("clean-stp", 30010, 8, [](std::mt19937& r){ return heldout::makeClean(r); }));
        cats.push_back(build("clean-brt", 30011, 8, cleanBrightFx));
        cats.push_back(build("broadHFng", 30012, 8, hfNoiseFx));

        // Mirror MLEngine decision rule on the NORMALIZED frame: prob > effThr + margin AND
        // Sibilance in top-K (=2 at sensitivity 0.5) of the 8 normalized probs.
        constexpr std::array<float, MLEngine::numProblemTypes> kMlMargin {{ 0.02f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f }};
        auto sibInTop2 = [](const std::array<float, MLEngine::numProblemTypes>& p, int s){
            int better = 0; for (int i = 0; i < MLEngine::numProblemTypes; ++i) if (i != s && p[static_cast<size_t>(i)] > p[static_cast<size_t>(s)]) ++better; return better < 2; };
        auto bandMeanDb = [](const std::vector<float>& db, float lo, float hi){
            const float binHz = static_cast<float>(kSampleRate) / kFFTSize; double s = 0; int n = 0;
            for (int i = 0; i < static_cast<int>(db.size()); ++i) { const float f = i * binHz; if (f >= lo && f <= hi) { s += db[static_cast<size_t>(i)]; ++n; } } return n ? static_cast<float>(s / n) : -120.0f; };

        auto measure = [&](MLEngine& ml, AIEngine& ai, const std::vector<std::vector<float>>& fx){
            double rawIn = 0, normIn = 0, thr = 0, legacyExc = 0, maxDelta = 0; int ruleNorm = 0, det = 0;
            for (const auto& lin : fx)
            {
                const auto rp = ml.forwardRawProbabilities(lin, kSampleRate);
                rawIn += rp[static_cast<size_t>(sib)];
                const auto db = linearToDb(lin);
                ai.analyzeSpectrum(db, true);
                const auto np = ai.getLastMLRawProbabilitiesForTests();
                const auto nt = ai.getLastMLThresholdsForTests();
                normIn += np[static_cast<size_t>(sib)];
                thr    += nt[static_cast<size_t>(sib)];
                maxDelta = std::max(maxDelta, static_cast<double>(std::abs(np[static_cast<size_t>(sib)] - rp[static_cast<size_t>(sib)])));
                if (np[static_cast<size_t>(sib)] > nt[static_cast<size_t>(sib)] + kMlMargin[static_cast<size_t>(sib)] && sibInTop2(np, sib)) ++ruleNorm;   // RULE on NORMALIZED frame
                legacyExc += bandMeanDb(db, 5000.0f, 10000.0f) - bandMeanDb(db, 2000.0f, 5000.0f);                                                     // veto's band-excess input (>=3dB keeps)
                for (const auto& c : ai.getPendingCorrections()) if (c.type == AIEngine::ProblemType::Sibilance) { ++det; break; }                     // after rule + VETO
            }
            const int n = static_cast<int>(fx.size());
            return std::array<double, 7>{ rawIn / n, normIn / n, maxDelta, thr / n, static_cast<double>(ruleNorm), legacyExc / n, static_cast<double>(det) };
        };

        logMessage("");
        logMessage("  Sibilance per-stage attribution (n=8/cat). ALL fixtures SYNTHETIC. rawIn=ML raw, normIn=ML on");
        logMessage("  AIEngine-normalized frame, maxD=max|norm-raw| per cat, ruleN=rule(thr+margin+topK) on NORMALIZED");
        logMessage("  frame, bandExc=legacy 5-10k minus 2-5k dB (veto keeps if >=3), det=after rule+VETO.");
        logMessage("  category  | model    | rawIn | normIn| maxD | effThr| ruleN| bandExc| det");
        logMessage("  ----------+----------+-------+-------+------+-------+------+--------+----");
        for (const auto& c : cats)
        {
            auto s = measure(shippedMlRef(*aiShipped), *aiShipped, c.fx);
            auto b = measure(Bml, *aiB, c.fx);
            auto pr = [](double x){ return juce::String(x, 2).paddedLeft(' ', 5); };
            auto row = [&](const char* who, const std::array<double,7>& v){
                logMessage("  " + c.name + " | " + juce::String(who) + " | " + pr(v[0]) + " | " + pr(v[1]) + " | " + pr(v[2]) + " | " + pr(v[3])
                           + " |  " + juce::String((int)v[4]) + "/8 | " + juce::String(v[5], 1).paddedLeft(' ', 6) + " |  " + juce::String((int)v[6]) + "/8"); };
            row("shipped ", s);
            row("B(repro)", b);
        }
        logMessage("  READ (HONEST): if ruleN passes on the NORMALIZED frame but det=0 AND bandExc<3 → the VETO drops it;");
        logMessage("  if ruleN<8 → the rule/top-K drops it (NOT the veto). Fixtures are SYNTHETIC shapes — the broad-HF");
        logMessage("  negative is NOT a real cymbal, so 'B confuses sibilance/cymbal' would need REAL audio to claim.");
    }

    // shipped model held inside the AIEngine; expose its MLEngine for the raw-on-raw stage.
    static MLEngine& shippedMlRef(AIEngine& ai) { return ai.getMLEngineForTest(); }
};

// =============================================================================
// P4-BUG-001 — snapshot-coherence WITNESS (Codex finding; characterization, no fix).
//
// The ML-path reality-check vetoes (Resonance prominence: findPeakInRange + 2×
// calculateBandEnergy; Sibilance: 2× calculateBandEnergy) re-read readSpectrumSnapshot,
// which SWAPS the triple-buffer indices each call → the bands in one veto decision can
// come from DIFFERENT frames (incoherent). Hook-free observable witness: if the veto
// outcome on the SAME final frame DIFFERS depending on the PRECEDING (priming) frames,
// the veto read stale buffers. We feed the same Resonance frame either SELF-primed
// (res,res,res) or CROSS-primed (loud-broadband ×2, then res) and compare the final
// detection. Flips>0 ⇒ history/buffer dependence ⇒ P4-BUG-001 confirmed observably.
// NO production change. KnownDebt (non-blocking; reports, does not gate).
// =============================================================================
class AIAccuracyTest_SnapshotCoherence : public juce::UnitTest
{
public:
    AIAccuracyTest_SnapshotCoherence()
        : juce::UnitTest("AI Accuracy — P4-BUG-001 snapshot-coherence witness (KnownDebt)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Is the veto outcome history/buffer-dependent? (same final frame, different priming)");

        const juce::File shippedFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");
        auto makeAi = [&](){
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(kSampleRate, 512); ai->setEnabled(true); ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            // MLOnly (NOT Hybrid) — isolates the ML-path veto from the heuristic detectResonances
            // temporal path (persistence/history), so any history-dependence is the ML reality-check.
            ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);
            expect(ai->setCustomMLWeightsPathForTests(shippedFile), "shipped load failed");
            ai->forceMLDetectionEnabledForTests(true);
            return ai;
        };

        // ── Mechanism witness: consecutive readSpectrumSnapshot() return DIFFERENT versions ──
        // (this is exactly what the ML-path vetoes do via multiple calculateBandEnergy/findPeakInRange).
        {
            auto ai = makeAi();
            std::vector<float> f(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
            for (auto& x : f) x = -20.0f; f = heldout::toLinear(f);
            ai->analyzeSpectrum(linearToDb(f), true);                 // publish + run ML path
            std::vector<long long> vers;
            for (int i = 0; i < 6; ++i) vers.push_back(static_cast<long long>(ai->probeSnapshotVersionForTests()));
            auto sorted = vers; std::sort(sorted.begin(), sorted.end());
            const int distinctCount = static_cast<int>(std::unique(sorted.begin(), sorted.end()) - sorted.begin());
            juce::String seq; for (auto v : vers) seq += juce::String(v) + " ";
            logMessage("");
            logMessage("  MECHANISM: 6 consecutive readSpectrumSnapshot() versions: " + seq.trim());
            logMessage("    distinct versions in a row = " + juce::String(distinctCount)
                       + "  (>1 ⇒ NON-idempotent: a veto's multiple reads see different buffers/frames)");
            expect(distinctCount > 1, "readSpectrumSnapshot() idempotent across calls — bug mechanism NOT reproduced");
        }
        auto detectsRes = [](AIEngine& ai, const std::vector<float>& lin){
            ai.analyzeSpectrum(linearToDb(lin), true);
            for (const auto& c : ai.getPendingCorrections()) if (c.type == AIEngine::ProblemType::Resonance) return true;
            return false;
        };
        // A frame VERY different from a tilted narrow resonance: loud, flat, high everywhere.
        std::vector<float> prime(static_cast<size_t>(kNumBins), heldout::kBaselineDb);
        for (auto& v : prime) v = -8.0f;   // dB, loud broadband
        prime = heldout::toLinear(prime);

        const int resIdx = static_cast<int>(MLEngine::ProblemType::Resonance);
        // Full pre-veto ML ProblemDetection list comparison (closes the localization confound:
        // proves the pre-veto ML DECISION — incl. frequency/severity — is identical across priming).
        auto sameDetList = [](const std::vector<MLEngine::ProblemDetection>& a,
                              const std::vector<MLEngine::ProblemDetection>& b){
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (a[i].type != b[i].type) return false;
                if (std::abs(a[i].frequency  - b[i].frequency)  > 1.0f)   return false;
                if (std::abs(a[i].confidence  - b[i].confidence)  > 0.001f) return false;
                if (std::abs(a[i].severity    - b[i].severity)    > 0.001f) return false;
                if (std::abs(a[i].suggestedGain - b[i].suggestedGain) > 0.001f) return false;
                if (std::abs(a[i].suggestedQ  - b[i].suggestedQ)  > 0.001f) return false;
            }
            return true;
        };
        std::mt19937 rng(40001);
        std::mt19937 cleanRng(40002);
        const int n = heldout::kVariations;
        int loudFlips = 0, cleanFlips = 0, selfPos = 0, loudPos = 0, cleanPos = 0, nondet = 0, mlMatchFull = 0, preVetoMatch = 0;
        double mlSelfSum = 0, mlClnSum = 0, maxMlVecDiff = 0;
        for (int v = 0; v < n; ++v)
        {
            const auto res = heldout::make(heldout::HClass::Resonance, rng);
            // LEVEL-MATCHED primer: a clean tilted frame (~same level as res, no peak) — controls for RMS carryover.
            const auto cleanP = heldout::makeClean(cleanRng);
            auto aiS = makeAi(); detectsRes(*aiS, res); detectsRes(*aiS, res); const bool self = detectsRes(*aiS, res);
            const auto mlSelfV   = aiS->getLastMLRawProbabilitiesForTests();   // FULL 8-vector (rank/top-K depend on all)
            const auto preVetoS  = aiS->getLastPreVetoMLDetectionsForTests();  // FULL pre-veto detection list
            auto aiL = makeAi(); detectsRes(*aiL, prime); detectsRes(*aiL, prime); const bool loud = detectsRes(*aiL, res);
            auto aiK = makeAi(); detectsRes(*aiK, cleanP); detectsRes(*aiK, cleanP); const bool cln = detectsRes(*aiK, res);
            const auto mlClnV    = aiK->getLastMLRawProbabilitiesForTests();
            const auto preVetoK  = aiK->getLastPreVetoMLDetectionsForTests();
            auto aiS2 = makeAi(); detectsRes(*aiS2, res); detectsRes(*aiS2, res); const bool self2 = detectsRes(*aiS2, res);
            float vecDiff = 0.0f;
            for (int k = 0; k < MLEngine::numProblemTypes; ++k)
                vecDiff = std::max(vecDiff, std::abs(mlSelfV[static_cast<size_t>(k)] - mlClnV[static_cast<size_t>(k)]));
            if (self != self2) ++nondet;
            if (self != loud) ++loudFlips;
            if (self != cln)  ++cleanFlips;
            if (vecDiff < 0.01f) ++mlMatchFull;     // ENTIRE ML raw vector identical across priming?
            if (sameDetList(preVetoS, preVetoK)) ++preVetoMatch;   // FULL pre-veto detection list identical?
            selfPos += self ? 1 : 0; loudPos += loud ? 1 : 0; cleanPos += cln ? 1 : 0;
            mlSelfSum += mlSelfV[static_cast<size_t>(resIdx)]; mlClnSum += mlClnV[static_cast<size_t>(resIdx)];
            maxMlVecDiff = std::max(maxMlVecDiff, static_cast<double>(vecDiff));
        }
        logMessage("");
        logMessage("  P4-BUG-001 witness (MLOnly — isolated from heuristic temporal path, shipped, Resonance, n=" + juce::String(n) + "):");
        logMessage("    self-primed  (res,res,res)   Res det : " + juce::String(selfPos) + "/" + juce::String(n));
        logMessage("    loud-primed  (loud,loud,res) Res det : " + juce::String(loudPos) + "/" + juce::String(n) + "  (loud primer: ~18 dB hotter — RMS confound possible)");
        logMessage("    clean-primed (clean,clean,res) Rdet  : " + juce::String(cleanPos) + "/" + juce::String(n) + "  (LEVEL-MATCHED primer: controls for RMS)");
        logMessage("    history-flips loud / clean           : " + juce::String(loudFlips) + "/" + juce::String(n) + "  /  " + juce::String(cleanFlips) + "/" + juce::String(n));
        logMessage("    nondet (self vs self2, should be 0)  : " + juce::String(nondet) + "/" + juce::String(n));
        logMessage("    ML raw Res prob self vs clean-primed : " + juce::String(mlSelfSum / n, 3) + " vs " + juce::String(mlClnSum / n, 3));
        logMessage("    FULL ML 8-vector identical (maxdiff " + juce::String(maxMlVecDiff, 4) + "): " + juce::String(mlMatchFull) + "/" + juce::String(n));
        logMessage("    FULL pre-veto DETECTION list identical (type/freq/conf/sev/Q/gain): " + juce::String(preVetoMatch) + "/" + juce::String(n));
        logMessage("  READ (regression guard): MLOnly removes the heuristic temporal path. The ENTIRE ML raw vector AND the");
        logMessage("  full pre-veto ProblemDetection list (incl. localization frequency) are identical across priming.");
        logMessage("  PRE-FIX the detection FLIPPED here (the veto read bands from DIFFERENT frames); POST-FIX the coherence");
        logMessage("  fix routes every veto band read through the SINGLE scratchTemp frame ⇒ cleanFlips→0 (history-independent).");
        logMessage("  (Stale reads carry PREVIOUS real frames after warm-up, not zeros — 'empty buffer' is cold-start only.)");

        // KnownDebt REGRESSION GUARD (post-fix): the ML-path coherence fix routes the Resonance/
        // Sibilance veto band reads through the SINGLE scratchTemp frame, so the detection must be
        // history-independent (cleanFlips==0). The pre-veto decision is still IDENTICAL across
        // priming (the fix changed ONLY the veto's frame source). See scorecard P4-BUG-001.
        expect(cleanFlips == 0, "P4-BUG-001 REGRESSION: ML-path veto still history/buffer-dependent "
               "(cleanFlips>0) after the coherence fix — a veto band read is not using scratchTemp");
        expect(mlMatchFull == n, "ML raw 8-vector NOT identical across priming (max vec diff "
               + juce::String(maxMlVecDiff, 4) + ") — the flip is not veto-isolated");
        expect(preVetoMatch == n, "pre-veto ML DETECTION LIST (incl. frequency) differs across priming — "
               "flip is not veto-isolated (localization confound NOT closed)");
        expect(nondet == 0, "within-sequence non-determinism — characterization unstable");
    }
};

// =============================================================================
// P4-BUG-001 LIVE BASELINE — does the bug survive the live regime? (Codex closure 2)
//
// The witness above runs force=true (single-window, persistence OFF) — a worst-case that
// EXAGGERATES. This measures the REAL live regime: force=false (rate-limiter analyzes every
// 3rd call, AIEngine.cpp:229) + warm-up + temporal persistence ON (8-frame/60% gate). Codex:
// "live impact must be MEASURED, not deduced", and "a STABLE repeated error passes the gate".
// We compare, per Resonance fixture T (and level-matched clean C), the SURFACED detection
// (getPendingCorrections, persistence-filtered) of two live streams ending on T:
//   CONTROL  = all-T          (T detected when NOT contaminated by a contrasting primer)
//   ALTERNATING = C,T,C,T,…    (each T preceded by C — the regime that exposes the veto bug)
// If CONTROL surfaces Res but ALTERNATING does not (or differs), the bug is LIVE-RELEVANT
// (a stable history error that survives persistence). If they match, persistence absorbs it.
// Measured for MLOnly AND Hybrid (product default). KnownDebt, no production change.
// =============================================================================
class AIAccuracyTest_LiveBaseline : public juce::UnitTest
{
public:
    AIAccuracyTest_LiveBaseline()
        : juce::UnitTest("AI Accuracy — P4-BUG-001 LIVE baseline force=false (KnownDebt)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Live regime (force=false, warm-up, persistence ON): is the bug live-relevant?");

        const juce::File shippedFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");

        auto makeAi = [&](AIEngine::DetectionBackendMode mode){
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(kSampleRate, 512); ai->setEnabled(true); ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            ai->setDetectionBackendMode(mode);
            expect(ai->setCustomMLWeightsPathForTests(shippedFile), "shipped weights load failed");
            ai->forceMLDetectionEnabledForTests(true);
            return ai;
        };
        auto feed = [](AIEngine& ai, const std::vector<float>& lin, int reps){
            for (int i = 0; i < reps; ++i) ai.analyzeSpectrum(linearToDb(lin), false);   // force=false → rate-limited + persistence ON
        };
        auto surfacedRes = [](AIEngine& ai){
            for (const auto& c : ai.getPendingCorrections()) if (c.type == AIEngine::ProblemType::Resonance) return true;
            return false;
        };

        struct Mode { const char* name; AIEngine::DetectionBackendMode mode; };
        const Mode modes[] = { { "MLOnly", AIEngine::DetectionBackendMode::MLOnly },
                               { "Hybrid", AIEngine::DetectionBackendMode::Hybrid } };
        const int n = heldout::kVariations;
        // CODEX FIX: the patterns must keep the TARGET duty-cycle ABOVE the 60% persistence gate
        // (AIEngine.cpp:924), so any suppression is the BUG corrupting C-primed T-frames, NOT a
        // sub-threshold duty-cycle. all-T (100% T), C,T,T (~67% T), C,T,T,T (~75% T) — all >60%,
        // all end on T (kLogical=18: last idx 17 → 17%3=2=T and 17%4=1=T).
        const int kLogical = 18;   // ×3 reps ≈ 54 calls (warm-up + 8-frame persistence window)
        auto runStream = [&](AIEngine& ai, const std::vector<float>& C, const std::vector<float>& T, int cPeriod){
            for (int i = 0; i < kLogical; ++i)
            {
                const bool isC = (cPeriod > 0) && (i % cPeriod == 0);   // cPeriod 0 = all-T (no C)
                feed(ai, isC ? C : T, 3);
            }
        };

        logMessage("");
        logMessage("  LIVE baseline (force=false, ~54 calls/stream, persistence ON), shipped, Resonance, n=" + juce::String(n) + ":");
        logMessage("  target duty-cycle kept >60% (above the persistence gate) → suppression = BUG, not duty-cycle.");
        logMessage("  mode   | all-T (100%) | C,T,T (~67%) | C,T,T,T (~75%)");
        logMessage("  -------+--------------+--------------+---------------");
        for (const auto& m : modes)
        {
            std::mt19937 rng(50001), cleanRng(50002);
            int allT = 0, ctt = 0, cttt = 0;
            for (int v = 0; v < n; ++v)
            {
                const auto T = heldout::make(heldout::HClass::Resonance, rng);
                const auto C = heldout::makeClean(cleanRng);
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 0); allT += surfacedRes(*ai) ? 1 : 0; }
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 3); ctt  += surfacedRes(*ai) ? 1 : 0; }
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 4); cttt += surfacedRes(*ai) ? 1 : 0; }
            }
            logMessage("  " + juce::String(m.name).paddedRight(' ', 6)
                       + " |     " + (juce::String(allT) + "/" + juce::String(n)).paddedRight(' ', 8)
                       + " |     " + (juce::String(ctt) + "/" + juce::String(n)).paddedRight(' ', 8)
                       + " |     " + juce::String(cttt) + "/" + juce::String(n));
        }
        logMessage("  READ (Codex-correct): target stays >60% in C,T,T and C,T,T,T, so persistence ALONE would KEEP it.");
        logMessage("  - all-T detects but C,T,T does NOT  ⇒ the BUG corrupts C-primed T-frames below the gate ⇒ live-relevant.");
        logMessage("  - C,T,T ≈ all-T                      ⇒ the earlier 12→1 (C,T 50%) was DUTY-CYCLE, not the bug.");
        logMessage("  - C,T,T,T detects but C,T,T not      ⇒ borderline (measured RISK, not certainty).");
        logMessage("  MEASURED live baseline; compare the post-fix run on the identical streams.");
    }
};

// =============================================================================
// P4-BUG-001 ACCEPTANCE before/after — ONE consolidated scorecard (Codex)
//
// Run ONCE on the shipped/buggy build to record the BEFORE column, then AGAIN after
// the ML-path coherence fix for the AFTER column; compare each row to its post-fix
// TARGET. Report-only (KnownDebt): pre-fix and post-fix have OPPOSITE expectations,
// so the directional asserts live in the SnapshotCoherence witness (flip>0 pre-fix →
// ==0 post-fix) and in the gate binaries (AI-Sweep floor 0/18, AI-Corpus). All shipped
// weights. Metrics:
//   [A] coherence flip   — clean-primed vs self-primed Res flip (MLOnly)  TARGET post-fix: 0
//   [B] live baseline    — all-T vs C,T,T vs C,T,T,T surfaced Res         TARGET: C,T,T/C,T,T,T → ~all-T
//   [C] clean-input FP   — Res/Sib hallucinated on pure-clean LIVE stream TARGET: NOT higher
//   [D] per-class recall — Hybrid product recall, held-out                TARGET: NOT lower
// =============================================================================
class AIAccuracyTest_BugFixAcceptance : public juce::UnitTest
{
public:
    AIAccuracyTest_BugFixAcceptance()
        : juce::UnitTest("AI Accuracy — P4-BUG-001 fix before/after acceptance (KnownDebt)", "KnownDebt") {}

    void runTest() override
    {
        beginTest("Consolidated before/after acceptance scorecard (shipped weights)");

        const juce::File shippedFile = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");

        auto makeAi = [&](AIEngine::DetectionBackendMode mode){
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(kSampleRate, 512); ai->setEnabled(true); ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            ai->setDetectionBackendMode(mode);
            expect(ai->setCustomMLWeightsPathForTests(shippedFile), "shipped weights load failed");
            ai->forceMLDetectionEnabledForTests(true);
            return ai;
        };
        auto surfaced = [](AIEngine& ai, AIEngine::ProblemType t){
            for (const auto& c : ai.getPendingCorrections()) if (c.type == t) return true;
            return false;
        };
        auto feed = [](AIEngine& ai, const std::vector<float>& lin, int reps){
            for (int i = 0; i < reps; ++i) ai.analyzeSpectrum(linearToDb(lin), false);   // force=false → rate-limited + persistence ON
        };
        const int n = heldout::kVariations;

        logMessage("");
        logMessage("  ===== P4-BUG-001 ACCEPTANCE SCORECARD (shipped, n=" + juce::String(n) + ") =====");

        // ── [A] coherence flip (force=true single-window, MLOnly — isolates the veto) ──
        auto detects = [&](AIEngine& ai, const std::vector<float>& lin, AIEngine::ProblemType t){
            ai.analyzeSpectrum(linearToDb(lin), true); return surfaced(ai, t);
        };
        {
            std::mt19937 rng(40001), cleanRng(40002);
            int cleanFlips = 0;
            for (int v = 0; v < n; ++v)
            {
                const auto res = heldout::make(heldout::HClass::Resonance, rng);
                const auto cln = heldout::makeClean(cleanRng);
                auto aiS = makeAi(AIEngine::DetectionBackendMode::MLOnly);
                detects(*aiS, res, AIEngine::ProblemType::Resonance);
                detects(*aiS, res, AIEngine::ProblemType::Resonance);
                const bool self = detects(*aiS, res, AIEngine::ProblemType::Resonance);
                auto aiK = makeAi(AIEngine::DetectionBackendMode::MLOnly);
                detects(*aiK, cln, AIEngine::ProblemType::Resonance);
                detects(*aiK, cln, AIEngine::ProblemType::Resonance);
                const bool clnPrimed = detects(*aiK, res, AIEngine::ProblemType::Resonance);
                if (self != clnPrimed) ++cleanFlips;
            }
            logMessage("  [A] coherence flip (MLOnly, clean-primed vs self, Res) : "
                       + juce::String(cleanFlips) + "/" + juce::String(n) + "    TARGET post-fix: 0");
        }

        // ── [B] live baseline v2 (force=false, persistence ON, duty-cycle >60%) ──
        struct Mode { const char* name; AIEngine::DetectionBackendMode mode; };
        const Mode modes[] = { { "MLOnly", AIEngine::DetectionBackendMode::MLOnly },
                               { "Hybrid", AIEngine::DetectionBackendMode::Hybrid } };
        const int kLogical = 18;
        auto runStream = [&](AIEngine& ai, const std::vector<float>& C, const std::vector<float>& T, int cPeriod){
            for (int i = 0; i < kLogical; ++i) feed(ai, ((cPeriod > 0) && (i % cPeriod == 0)) ? C : T, 3);
        };
        logMessage("  [B] live baseline Res (force=false)   all-T | C,T,T | C,T,T,T   TARGET: C,T,T & C,T,T,T → ~all-T");
        for (const auto& m : modes)
        {
            std::mt19937 rng(50001), cleanRng(50002);
            int allT = 0, ctt = 0, cttt = 0;
            for (int v = 0; v < n; ++v)
            {
                const auto T = heldout::make(heldout::HClass::Resonance, rng);
                const auto C = heldout::makeClean(cleanRng);
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 0); allT += surfaced(*ai, AIEngine::ProblemType::Resonance) ? 1 : 0; }
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 3); ctt  += surfaced(*ai, AIEngine::ProblemType::Resonance) ? 1 : 0; }
                { auto ai = makeAi(m.mode); runStream(*ai, C, T, 4); cttt += surfaced(*ai, AIEngine::ProblemType::Resonance) ? 1 : 0; }
            }
            logMessage("      " + juce::String(m.name).paddedRight(' ', 6) + "  "
                       + (juce::String(allT) + "/" + juce::String(n)).paddedRight(' ', 7) + "| "
                       + (juce::String(ctt) + "/" + juce::String(n)).paddedRight(' ', 7) + "| "
                       + juce::String(cttt) + "/" + juce::String(n));
        }

        // ── [C] clean-input FP (floor proxy): Res/Sib hallucinated on a pure-clean LIVE stream ──
        // Complements AI-Sweep (which is force=true): this is the force=false persistence regime.
        logMessage("  [C] clean-input FP (force=false, pure-clean stream)   Res-FP | Sib-FP   TARGET: NOT higher post-fix");
        for (const auto& m : modes)
        {
            std::mt19937 cleanRng(60001);
            int resFp = 0, sibFp = 0;
            for (int v = 0; v < n; ++v)
            {
                const auto C = heldout::makeClean(cleanRng);
                auto ai = makeAi(m.mode);
                for (int i = 0; i < kLogical; ++i) feed(*ai, C, 3);
                if (surfaced(*ai, AIEngine::ProblemType::Resonance)) ++resFp;
                if (surfaced(*ai, AIEngine::ProblemType::Sibilance)) ++sibFp;
            }
            logMessage("      " + juce::String(m.name).paddedRight(' ', 6) + "  "
                       + (juce::String(resFp) + "/" + juce::String(n)).paddedRight(' ', 7) + "| "
                       + juce::String(sibFp) + "/" + juce::String(n));
        }

        // ── [D] per-class recall (Hybrid product, force=true single-frame, held-out) ──
        struct Grp { heldout::HClass hc; AIEngine::ProblemType type; const char* name; };
        const Grp groups[] = {
            { heldout::HClass::Resonance, AIEngine::ProblemType::Resonance,  "Resonance" },
            { heldout::HClass::Sibilance, AIEngine::ProblemType::Sibilance,  "Sibilance" },
            { heldout::HClass::Muddiness, AIEngine::ProblemType::Muddiness,  "Muddiness" },
            { heldout::HClass::Boxyness,  AIEngine::ProblemType::Boxyness,   "Boxyness"  },
            { heldout::HClass::Boominess, AIEngine::ProblemType::LowEndBoom, "Boominess" },
            { heldout::HClass::Harshness, AIEngine::ProblemType::Harshness,  "Harshness" },
            { heldout::HClass::Thinness,  AIEngine::ProblemType::ThinSound,  "Thinness"  },
        };
        auto hits = [&](AIEngine& ai, const std::vector<std::vector<float>>& fx, AIEngine::ProblemType t){
            int h = 0; for (const auto& lin : fx) { ai.analyzeSpectrum(linearToDb(lin), true); for (const auto& c : ai.getPendingCorrections()) if (c.type == t) { ++h; break; } } return h;
        };
        auto aiHy = makeAi(AIEngine::DetectionBackendMode::Hybrid);
        logMessage("  [D] per-class recall (Hybrid product, held-out)   TARGET: NOT lower post-fix");
        std::mt19937 r(20260617);
        for (const auto& g : groups)
        {
            std::vector<std::vector<float>> fx; for (int v = 0; v < n; ++v) fx.push_back(heldout::make(g.hc, r));
            logMessage("      " + juce::String(g.name).paddedRight(' ', 11) + ": "
                       + juce::String(hits(*aiHy, fx, g.type)) + "/" + juce::String(n));
        }
        logMessage("  ============================================================");
        logMessage("  Compare BEFORE (this, shipped/buggy) vs AFTER (post-fix) row-by-row against each TARGET.");
        logMessage("  Sibilance recall is model-limited (0/12 PRE-veto) — the veto fix is NOT expected to move it.");
        logMessage("  AI-Sweep floor 0/18 + AI-Corpus are checked by their own binaries (separate gate).");
    }
};

static AIAccuracyTest_MLEngine   sAIAccuracyTestML;
static AIAccuracyTest_AIEngine   sAIAccuracyTestAI;
static AIAccuracyTest_Retrain    sAIAccuracyTestRetrain;
static AIAccuracyTest_ThreeLevel sAIAccuracyTestThreeLevel;
static AIAccuracyTest_HeldOut    sAIAccuracyTestHeldOut;
static AIAccuracyTest_M2aRefine  sAIAccuracyTestM2aRefine;
static AIAccuracyTest_M3Diag     sAIAccuracyTestM3Diag;
static AIAccuracyTest_M2aRefine2 sAIAccuracyTestM2aRefine2;
static AIAccuracyTest_ResPipeline sAIAccuracyTestResPipeline;
static AIAccuracyTest_PipelineAllClasses sAIAccuracyTestPipelineAllClasses;
static AIAccuracyTest_VetoSibAttribution sAIAccuracyTestVetoSibAttribution;
static AIAccuracyTest_SnapshotCoherence  sAIAccuracyTestSnapshotCoherence;
static AIAccuracyTest_LiveBaseline       sAIAccuracyTestLiveBaseline;
static AIAccuracyTest_BugFixAcceptance   sAIAccuracyTestBugFixAcceptance;
