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
constexpr int    kNumBins      = kFFTSize / 2 + 1;  // 2049
constexpr float  kFloorDB      = -60.0f;   // baseline noise floor
constexpr int    kVariations   = 10;        // variations per problem type

// Minimum acceptable metrics for PASS
constexpr float kMinPrecision = 0.60f;  // 60% — if you say it, be right > half the time
constexpr float kMinRecall    = 0.50f;  // 50% — catch at least half the real problems
constexpr float kMinF1        = 0.55f;  // 55% — balanced metric
constexpr float kMaxFPRate    = 0.30f;  // 30% — max false positive rate on clean signals

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum generation utilities
// ─────────────────────────────────────────────────────────────────────────────

/// Convert frequency in Hz to FFT bin index
int hzToBin(float hz)
{
    return static_cast<int>(std::round(hz / (static_cast<float>(kSampleRate) / kFFTSize)));
}

/// Create a flat spectrum at a given dB level
std::vector<float> makeFlat(float levelDB = kFloorDB)
{
    return std::vector<float>(kNumBins, levelDB);
}

/// Add a narrow peak (resonance) at a given frequency
void addPeak(std::vector<float>& spec, float freqHz, float peakDB, float qFactor = 10.0f)
{
    const int centerBin = hzToBin(freqHz);
    const float bwBins = static_cast<float>(centerBin) / qFactor;

    for (int i = 0; i < kNumBins; ++i)
    {
        const float dist = static_cast<float>(i - centerBin);
        const float falloff = (bwBins > 0.0f) ? (dist * dist) / (bwBins * bwBins) : 1000.0f;
        const float addDB = peakDB * std::exp(-0.5f * falloff);
        // Add in linear domain
        const float existing = std::pow(10.0f, spec[i] / 20.0f);
        const float added    = std::pow(10.0f, addDB / 20.0f);
        spec[i] = 20.0f * std::log10(existing + added);
    }
}

/// Add broadband energy in a frequency range (shelf/band boost)
void addBand(std::vector<float>& spec, float lowHz, float highHz, float boostDB)
{
    const int lo = juce::jmax(0, hzToBin(lowHz));
    const int hi = juce::jmin(kNumBins - 1, hzToBin(highHz));

    for (int i = lo; i <= hi; ++i)
        spec[i] += boostDB;
}

/// Cut broadband energy in a frequency range (for "thin" or "dull" problems)
void cutBand(std::vector<float>& spec, float lowHz, float highHz, float cutDB)
{
    addBand(spec, lowHz, highHz, -std::abs(cutDB));
}

/// Add realistic pink-noise-like spectral shape
void addPinkSlope(std::vector<float>& spec, float levelDB = -30.0f)
{
    for (int i = 1; i < kNumBins; ++i)
    {
        const float freqHz = static_cast<float>(i) * static_cast<float>(kSampleRate) / kFFTSize;
        // Pink noise: -3dB/octave from 1kHz
        const float rolloff = -3.0f * std::log2(std::max(freqHz, 20.0f) / 1000.0f);
        spec[i] = levelDB + rolloff;
    }
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

/// Generate variations of a specific problem type
std::vector<TestCase> generateResonanceTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> freqDist(200.0f, 8000.0f);
    std::uniform_real_distribution<float> gainDist(8.0f, 18.0f);
    std::uniform_real_distribution<float> qDist(8.0f, 30.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float freq = freqDist(rng);
        const float gain = gainDist(rng);
        addPeak(spec, freq, gain, qDist(rng));
        cases.push_back({spec, MLEngine::ProblemType::Resonance,
                         AIEngine::ProblemType::Resonance,
                         "Resonance @" + juce::String(static_cast<int>(freq)) + "Hz +" + juce::String(gain, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateHarshnessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> boostDist(6.0f, 15.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float boost = boostDist(rng);
        // Harshness: broad excess in 1-8 kHz
        const float lo = 1500.0f + static_cast<float>(i) * 200.0f;
        const float hi = 6000.0f + static_cast<float>(i) * 300.0f;
        addBand(spec, lo, hi, boost);
        cases.push_back({spec, MLEngine::ProblemType::Harshness,
                         AIEngine::ProblemType::Harshness,
                         "Harshness " + juce::String(static_cast<int>(lo)) + "-" + juce::String(static_cast<int>(hi)) + "Hz +" + juce::String(boost, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateMuddinessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> boostDist(6.0f, 14.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float boost = boostDist(rng);
        addBand(spec, 150.0f + static_cast<float>(i) * 20.0f,
                400.0f + static_cast<float>(i) * 15.0f, boost);
        cases.push_back({spec, MLEngine::ProblemType::Muddiness,
                         AIEngine::ProblemType::Muddiness,
                         "Muddiness +" + juce::String(boost, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateSibilanceTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> boostDist(6.0f, 14.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float boost = boostDist(rng);
        addBand(spec, 5000.0f + static_cast<float>(i) * 200.0f,
                10000.0f + static_cast<float>(i) * 200.0f, boost);
        cases.push_back({spec, MLEngine::ProblemType::Sibilance,
                         AIEngine::ProblemType::Sibilance,
                         "Sibilance +" + juce::String(boost, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateBoominessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> boostDist(8.0f, 18.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float boost = boostDist(rng);
        addBand(spec, 30.0f, 120.0f + static_cast<float>(i) * 10.0f, boost);
        cases.push_back({spec, MLEngine::ProblemType::Boominess,
                         AIEngine::ProblemType::LowEndBoom,
                         "Boominess +" + juce::String(boost, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateThinnessTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> cutDist(8.0f, 18.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-30.0f);
        addPinkSlope(spec, -25.0f);
        const float cut = cutDist(rng);
        // Thin = lack of low-mids
        cutBand(spec, 100.0f, 500.0f, cut);
        cases.push_back({spec, MLEngine::ProblemType::Thinness,
                         AIEngine::ProblemType::ThinSound,
                         "Thinness -" + juce::String(cut, 1) + "dB in low-mids"});
    }
    return cases;
}

std::vector<TestCase> generateBoxyTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    std::uniform_real_distribution<float> boostDist(6.0f, 14.0f);

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-40.0f);
        addPinkSlope(spec, -30.0f);
        const float boost = boostDist(rng);
        addBand(spec, 350.0f + static_cast<float>(i) * 30.0f,
                800.0f + static_cast<float>(i) * 20.0f, boost);
        cases.push_back({spec, MLEngine::ProblemType::BoxyMidrange,
                         AIEngine::ProblemType::Boxyness,
                         "Boxy +" + juce::String(boost, 1) + "dB"});
    }
    return cases;
}

std::vector<TestCase> generateClippingTests(std::mt19937& rng)
{
    std::vector<TestCase> cases;
    (void) rng;

    for (int i = 0; i < kVariations; ++i)
    {
        auto spec = makeFlat(-10.0f);  // Very hot signal
        addPinkSlope(spec, -5.0f);
        // Clipping: broad energy with harmonics extending unnaturally high
        addBand(spec, 20.0f, 20000.0f, static_cast<float>(i) + 3.0f);
        // Add odd-harmonic emphasis typical of hard clipping
        for (float h = 1000.0f; h < 15000.0f; h *= 3.0f)
            addPeak(spec, h, 6.0f, 5.0f);
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
    std::uniform_real_distribution<float> levelDist(-45.0f, -25.0f);

    // Variation 1-3: flat noise at different levels
    for (int i = 0; i < 3; ++i)
    {
        auto spec = makeFlat(levelDist(rng));
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,  // sentinel: no problem expected
                         AIEngine::ProblemType::None,
                         "Clean flat " + juce::String(i)});
    }

    // Variation 4-6: pink noise (natural spectrum)
    for (int i = 0; i < 3; ++i)
    {
        auto spec = makeFlat(-60.0f);
        addPinkSlope(spec, levelDist(rng));
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,
                         AIEngine::ProblemType::None,
                         "Clean pink " + juce::String(i)});
    }

    // Variation 7-10: gentle musical-like shapes (slight bass emphasis, slight treble roll-off)
    for (int i = 0; i < 4; ++i)
    {
        auto spec = makeFlat(-60.0f);
        addPinkSlope(spec, -30.0f);
        // Gentle bass warmth (+2-3 dB below 200Hz)
        addBand(spec, 40.0f, 200.0f, 2.0f + static_cast<float>(i) * 0.5f);
        // Gentle treble air (+1-2 dB above 10kHz)
        addBand(spec, 10000.0f, 20000.0f, 1.0f + static_cast<float>(i) * 0.3f);
        cases.push_back({spec, MLEngine::ProblemType::NumProblems,
                         AIEngine::ProblemType::None,
                         "Clean musical " + juce::String(i)});
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


static AIAccuracyTest_MLEngine  sAIAccuracyTestML;
static AIAccuracyTest_AIEngine  sAIAccuracyTestAI;
