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

} // anonymous namespace


// =============================================================================
// TEST A — MLEngine Direct Inference Accuracy
// =============================================================================
class AIAccuracyTest_MLEngine : public juce::UnitTest
{
public:
    AIAccuracyTest_MLEngine()
        : juce::UnitTest("AI Accuracy — MLEngine Direct", "AI-Accuracy") {}

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
        : juce::UnitTest("AI Accuracy — AIEngine Pipeline", "AI-Accuracy") {}

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
                // Feed spectrum to AIEngine (force = true bypasses rate limiter)
                ai.analyzeSpectrum(tc.spectrum, true);

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
            ai.analyzeSpectrum(tc.spectrum, true);
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
        : juce::UnitTest("AI Accuracy — Retrain + Re-evaluate", "AI-Retrain") {}

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

        // Save retrained weights if improvement is significant
        if (avgF1 > 0.20f)  // better than baseline 15.6%
        {
            auto modelFile = juce::File(__FILE__).getParentDirectory()
                                 .getParentDirectory().getParentDirectory()
                                 .getChildFile("Resources/Models/ml_weights_retrained.bin");
            bool saved = ml.saveWeights(modelFile);
            logMessage("  Retrained weights saved: " + juce::String(saved ? "YES" : "NO")
                       + " → " + modelFile.getFullPathName());
        }

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


static AIAccuracyTest_MLEngine  sAIAccuracyTestML;
static AIAccuracyTest_AIEngine  sAIAccuracyTestAI;
static AIAccuracyTest_Retrain   sAIAccuracyTestRetrain;
