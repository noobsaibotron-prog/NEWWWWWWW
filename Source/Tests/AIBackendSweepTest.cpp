/**
 * AIBackendSweepTest.cpp
 *
 * EMPIRICAL VERIFICATION of the AI detection subsystem (Marco's request).
 *
 * Goal: instead of *deducing* detection behaviour from the source, MEASURE it.
 * We inject synthetic spectra with a single, known problem each:
 *
 *     - Resonance  @  800 Hz   (narrow Gaussian peak)
 *     - Sibilance  @ 7000 Hz   (HF excess)
 *     - Muddiness  @  250 Hz   (low-mid buildup)
 *     - Clean                  (flat + noise, no problem — false-positive control)
 *     - Resonance @ 800/6k/12k on STEEP tilt (HF recall on the same background
 *       that triggers the CleanSteep false-positive)
 *
 * Each stimulus is run through ALL THREE detection backends:
 *
 *     - HeuristicOnly   (pure DSP analytic path: detectProblems)
 *     - MLOnly          (neural net path: detectProblemsWithML)
 *     - Hybrid          (ML primary + heuristic resonance supplement)
 *
 * ...at THREE sensitivity settings: 0.2 / 0.5 / 0.8.
 *
 * For every (stimulus × backend × sensitivity) cell we log:
 *     - whether the EXPECTED problem type was detected (true positive)
 *     - the detected frequency / gain / confidence
 *     - how many OTHER problem types fired (extra detections ≈ false positives)
 *
 * This is a DIAGNOSTIC test: it prints a full matrix. The hard assertions are
 * intentionally lenient (the value is the printed evidence, not a pass/fail gate),
 * but a couple of sanity expectations guard against the pipeline going silent.
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <algorithm>

#include "../AI/AIEngine.h"

namespace
{

constexpr double kSampleRate = 44100.0;
constexpr int    kFFTSize    = 4096;
constexpr int    kNumBins    = kFFTSize / 2 + 1;   // 2049 — matches AIEngine numBins
constexpr float  kBaselineDb = -65.0f;             // dB-domain baseline (real FFT publishes dB)

// IMPORTANT: AIEngine::analyzeSpectrum expects a dB-domain spectrum (the real
// plugin publishes magDB = gainToDecibels(mag), floor ~-80..-120 dB). Feeding
// linear magnitude makes every detector see a ~0 dB flat curve → zero detections.
// All helpers below therefore operate in dB.

int hzToBin(float hz)
{
    return static_cast<int>(std::round(hz / (static_cast<float>(kSampleRate) / kFFTSize)));
}

std::vector<float> makeFlat(float levelDb = kBaselineDb)
{
    return std::vector<float>(kNumBins, levelDb);
}

void addNoise(std::vector<float>& spec, std::mt19937& rng, float stddevDb = 1.5f)
{
    std::normal_distribution<float> noise(0.0f, stddevDb);
    for (auto& v : spec)
        v += noise(rng);
}

/// Gaussian bump in the dB domain (additive dB on top of the baseline).
/// peakDb = height of the bump in dB above local baseline.
void addPeakDb(std::vector<float>& spec, float freqHz, float peakDb, float sigmaFactor)
{
    const float binHz   = static_cast<float>(kSampleRate) / kFFTSize;
    const float sigmaHz = juce::jmax(30.0f, freqHz * sigmaFactor);
    for (int i = 0; i < kNumBins; ++i)
    {
        const float freq = static_cast<float>(i) * binHz;
        const float d    = (freq - freqHz) / sigmaHz;
        spec[static_cast<size_t>(i)] += peakDb * std::exp(-0.5f * d * d);
    }
}

void addPeakDbWithSigmaHz(std::vector<float>& spec, float freqHz, float peakDb, float sigmaHz)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    sigmaHz = juce::jmax(binHz, sigmaHz);
    for (int i = 0; i < kNumBins; ++i)
    {
        const float freq = static_cast<float>(i) * binHz;
        const float d = (freq - freqHz) / sigmaHz;
        spec[static_cast<size_t>(i)] += peakDb * std::exp(-0.5f * d * d);
    }
}

struct Stimulus
{
    juce::String           name;
    AIEngine::ProblemType  expected;
    std::vector<float>     base;   // dB spectrum WITHOUT noise (noise added fresh per frame)
};

// Apply a gentle pink-ish tilt (natural, NOT a problem) to a dB spectrum.
void applyPinkTilt(std::vector<float>& spec)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    for (int i = 0; i < kNumBins; ++i)
    {
        const float f = juce::jmax(20.0f, static_cast<float>(i) * binHz);
        spec[static_cast<size_t>(i)] += -3.0f * std::log10(f / 100.0f); // ~ -3 dB/decade
    }
}

// Steeper pink tilt (~ -6 dB/decade) — still NATURAL, NOT a problem. Common on real
// bass-heavy material. Used to probe whether a Muddiness veto that relies on an
// absolute band-excess threshold (Approach B) hallucinates low-mid "mud" purely from a
// steeper-but-clean spectral slope. A self-normalising / local reference (Approach A)
// should stay clean here regardless of slope.
void applySteepTilt(std::vector<float>& spec)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    for (int i = 0; i < kNumBins; ++i)
    {
        const float f = juce::jmax(20.0f, static_cast<float>(i) * binHz);
        spec[static_cast<size_t>(i)] += -6.0f * std::log10(f / 100.0f); // ~ -6 dB/decade
    }
}

// Monotonic, clean, bass-heavy tilt. This is intentionally NOT a local boom:
// a tilt-robust LowEndBoom veto should model it as slope, not as an excess.
void applyCleanBassTilt(std::vector<float>& spec)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    for (int i = 0; i < kNumBins; ++i)
    {
        const float f = juce::jmax(20.0f, static_cast<float>(i) * binHz);
        spec[static_cast<size_t>(i)] += -9.0f * std::log10(f / 100.0f);
    }
}

std::vector<Stimulus> buildStimuli()
{
    std::vector<Stimulus> s;

    // 1) Resonance @ 800 Hz — narrow +20 dB peak
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        addPeakDb(spec, 800.0f, 20.0f, 0.04f);   // narrow, resonance-like
        s.push_back({ "Resonance@800Hz", AIEngine::ProblemType::Resonance, std::move(spec) });
    }

    // 2) Sibilance @ 7 kHz — broad +14 dB HF excess
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        addPeakDb(spec, 7000.0f, 14.0f, 0.18f);
        s.push_back({ "Sibilance@7kHz", AIEngine::ProblemType::Sibilance, std::move(spec) });
    }

    // 3) Muddiness @ 250 Hz — broad +14 dB low-mid buildup
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        addPeakDb(spec, 250.0f, 14.0f, 0.25f);
        s.push_back({ "Muddiness@250Hz", AIEngine::ProblemType::Muddiness, std::move(spec) });
    }

    // 4) Clean control — gentle tilt only, no injected problem
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        s.push_back({ "Clean(control)", AIEngine::ProblemType::None, std::move(spec) });
    }

    // 5) Clean control with STEEPER tilt (-6 dB/decade) — still no injected problem.
    //    Appended AFTER index 3 so the robustness section's stimuli[3] stays the gentle
    //    clean. This is the A-vs-B decider: a tilt-fragile veto will false-positive here.
    {
        auto spec = makeFlat();
        applySteepTilt(spec);
        s.push_back({ "CleanSteep(control)", AIEngine::ProblemType::None, std::move(spec) });
    }

    // 6) True resonance on the SAME steep tilt background — low/mid anchor.
    {
        auto spec = makeFlat();
        applySteepTilt(spec);
        addPeakDb(spec, 800.0f, 20.0f, 0.04f);
        s.push_back({ "Resonance@800Hz on SteepTilt", AIEngine::ProblemType::Resonance, std::move(spec) });
    }

    // 7) True HF resonance on the SAME steep tilt background — recall probe.
    {
        auto spec = makeFlat();
        applySteepTilt(spec);
        addPeakDb(spec, 6000.0f, 20.0f, 0.02f);
        s.push_back({ "Resonance@6kHz on SteepTilt", AIEngine::ProblemType::Resonance, std::move(spec) });
    }

    // 8) Very-HF resonance on the SAME steep tilt background — hardest case.
    {
        auto spec = makeFlat();
        applySteepTilt(spec);
        addPeakDb(spec, 12000.0f, 20.0f, 0.02f);
        s.push_back({ "Resonance@12kHz on SteepTilt", AIEngine::ProblemType::Resonance, std::move(spec) });
    }

    return s;
}

struct BackendOpt
{
    AIEngine::DetectionBackendMode mode;
    juce::String                   name;
};

const char* shortType(AIEngine::ProblemType t)
{
    switch (t)
    {
        case AIEngine::ProblemType::None:       return "None";
        case AIEngine::ProblemType::Resonance:  return "Res";
        case AIEngine::ProblemType::Harshness:  return "Har";
        case AIEngine::ProblemType::Muddiness:  return "Mud";
        case AIEngine::ProblemType::Boxyness:   return "Bxy";
        case AIEngine::ProblemType::Sibilance:  return "Sib";
        case AIEngine::ProblemType::LowEndBoom: return "Boom";
        case AIEngine::ProblemType::ThinSound:  return "Thin";
        case AIEngine::ProblemType::DullSound:  return "Dull";
    }
    return "?";
}

float medianOfValues(std::vector<float> values)
{
    if (values.empty())
        return 0.0f;

    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if ((values.size() & 1u) != 0)
        return values[mid];

    return 0.5f * (values[mid - 1] + values[mid]);
}

float linearBinAverageDb(const std::vector<float>& spec, float loHz, float hiHz)
{
    const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
    const int loBin = juce::jlimit(0, kNumBins - 1, static_cast<int>(std::floor(loHz / binHz)));
    const int hiBin = juce::jlimit(0, kNumBins - 1, static_cast<int>(std::ceil(hiHz / binHz)));

    float sum = 0.0f;
    int count = 0;
    for (int bin = loBin; bin <= hiBin; ++bin)
    {
        const float value = spec[static_cast<size_t>(bin)];
        if (value > -100.0f)
        {
            sum += value;
            ++count;
        }
    }

    return count > 0 ? sum / static_cast<float>(count) : -100.0f;
}

std::vector<std::pair<float, float>> logBucketLevels(const std::vector<float>& spec,
                                                     float loHz,
                                                     float hiHz,
                                                     int buckets)
{
    std::vector<std::pair<float, float>> levels;
    if (loHz <= 0.0f || hiHz <= loHz || buckets <= 0)
        return levels;

    const float logLo = std::log10(loHz);
    const float logHi = std::log10(hiHz);
    for (int b = 0; b < buckets; ++b)
    {
        const float t0 = static_cast<float>(b) / static_cast<float>(buckets);
        const float t1 = static_cast<float>(b + 1) / static_cast<float>(buckets);
        const float bucketLo = std::pow(10.0f, logLo + (logHi - logLo) * t0);
        const float bucketHi = std::pow(10.0f, logLo + (logHi - logLo) * t1);
        const float center = std::sqrt(bucketLo * bucketHi);

        std::vector<float> values;
        const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
        const int loBin = juce::jlimit(1, kNumBins - 1, static_cast<int>(std::floor(bucketLo / binHz)));
        const int hiBin = juce::jlimit(1, kNumBins - 1, static_cast<int>(std::ceil(bucketHi / binHz)));
        values.reserve(static_cast<size_t>(juce::jmax(0, hiBin - loBin + 1)));

        for (int bin = loBin; bin <= hiBin; ++bin)
        {
            const float f = static_cast<float>(bin) * binHz;
            if (f >= bucketLo && f <= bucketHi)
                values.push_back(spec[static_cast<size_t>(bin)]);
        }

        if (! values.empty())
            levels.push_back({ center, medianOfValues(std::move(values)) });
    }

    return levels;
}

struct BroadBandMetric
{
    float legacyExcessDb = 0.0f;
    float trendExcessDb = 0.0f;
    float problemLevelDb = 0.0f;
    float trendBaselineDb = 0.0f;
};

struct BroadBandAggregate
{
    float legacyExcessDb = 0.0f;
    float trendExcessDb = 0.0f;
    float problemLevelDb = 0.0f;
    float trendBaselineDb = 0.0f;
    float trendStableRatio = 0.0f;
};

BroadBandMetric computeBroadBandMetric(const std::vector<float>& spec,
                                       float problemLo,
                                       float problemHi,
                                       std::initializer_list<std::pair<float, float>> referenceBands,
                                       float legacyRefLo,
                                       float legacyRefHi)
{
    BroadBandMetric result;

    const auto problemBuckets = logBucketLevels(spec, problemLo, problemHi, 10);
    std::vector<float> problemLevels;
    problemLevels.reserve(problemBuckets.size());
    for (const auto& bucket : problemBuckets)
        problemLevels.push_back(bucket.second);

    result.problemLevelDb = medianOfValues(std::move(problemLevels));
    result.legacyExcessDb = linearBinAverageDb(spec, problemLo, problemHi)
                          - linearBinAverageDb(spec, legacyRefLo, legacyRefHi);

    std::vector<std::pair<float, float>> refs;
    for (const auto& ref : referenceBands)
    {
        auto levels = logBucketLevels(spec, ref.first, ref.second, 8);
        refs.insert(refs.end(), levels.begin(), levels.end());
    }

    if (refs.size() < 2)
    {
        result.trendBaselineDb = result.problemLevelDb;
        result.trendExcessDb = 0.0f;
        return result;
    }

    double n = 0.0, sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    for (const auto& ref : refs)
    {
        const double x = std::log10(static_cast<double>(juce::jmax(1.0f, ref.first)));
        const double y = static_cast<double>(ref.second);
        n += 1.0;
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }

    const double denom = n * sumXX - sumX * sumX;
    if (std::abs(denom) < 1e-9)
    {
        std::vector<float> refLevels;
        refLevels.reserve(refs.size());
        for (const auto& ref : refs)
            refLevels.push_back(ref.second);
        result.trendBaselineDb = medianOfValues(std::move(refLevels));
    }
    else
    {
        const double slope = (n * sumXY - sumX * sumY) / denom;
        const double intercept = (sumY - slope * sumX) / n;
        const float problemCenter = std::sqrt(problemLo * problemHi);
        result.trendBaselineDb = static_cast<float>(slope * std::log10(problemCenter) + intercept);
    }

    result.trendExcessDb = result.problemLevelDb - result.trendBaselineDb;
    return result;
}

BroadBandAggregate runBroadBandMetric(const std::vector<float>& base,
                                      float problemLo,
                                      float problemHi,
                                      std::initializer_list<std::pair<float, float>> referenceBands,
                                      float legacyRefLo,
                                      float legacyRefHi,
                                      const juce::String& seedKey)
{
    std::mt19937 rng(static_cast<uint32_t>(seedKey.hashCode()));
    std::vector<float> legacy;
    std::vector<float> trend;
    std::vector<float> problem;
    std::vector<float> baseline;
    int stable = 0;

    for (int frame = 0; frame < 16; ++frame)
    {
        auto noisy = base;
        addNoise(noisy, rng, 2.0f);
        const auto metric = computeBroadBandMetric(noisy, problemLo, problemHi, referenceBands, legacyRefLo, legacyRefHi);
        legacy.push_back(metric.legacyExcessDb);
        trend.push_back(metric.trendExcessDb);
        problem.push_back(metric.problemLevelDb);
        baseline.push_back(metric.trendBaselineDb);
        if (metric.trendExcessDb >= 3.0f)
            ++stable;
    }

    BroadBandAggregate result;
    result.legacyExcessDb = medianOfValues(std::move(legacy));
    result.trendExcessDb = medianOfValues(std::move(trend));
    result.problemLevelDb = medianOfValues(std::move(problem));
    result.trendBaselineDb = medianOfValues(std::move(baseline));
    result.trendStableRatio = static_cast<float>(stable) / 16.0f;
    return result;
}

juce::String formatBroadBand(const BroadBandAggregate& metric)
{
    return juce::String::formatted("legacy=%.2f trend=%.2f stable=%.2f problem=%.2f baseline=%.2f",
                                   metric.legacyExcessDb,
                                   metric.trendExcessDb,
                                   metric.trendStableRatio,
                                   metric.problemLevelDb,
                                   metric.trendBaselineDb);
}

} // anonymous namespace


// =============================================================================
// EMPIRICAL SWEEP — stimulus × backend × sensitivity
// =============================================================================
class AIBackendSweepTest : public juce::UnitTest
{
public:
    AIBackendSweepTest()
        : juce::UnitTest("AI Backend Sweep — empirical detection matrix", "AI-Sweep") {}

    void runTest() override
    {
        beginTest("Detection matrix: base stimuli + steep-tilt resonance recall probes");

        // Resolve the shipped ML weights from the source tree (test binary has no
        // models/ folder of its own). __FILE__ → Source/Tests/ → repo root.
        const juce::File mlWeightsFile =
            juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
                .getChildFile("Resources/Models/ml_weights.bin");
        logMessage(juce::String("  ML weights: ")
                   + (mlWeightsFile.existsAsFile()
                        ? ("FOUND (" + juce::String(mlWeightsFile.getSize()) + " bytes) — ML/Hybrid run the neural path")
                        : "NOT FOUND — ML/Hybrid fall back to heuristic"));

        auto stimuli = buildStimuli();

        const BackendOpt backends[] = {
            { AIEngine::DetectionBackendMode::HeuristicOnly, "Heuristic" },
            { AIEngine::DetectionBackendMode::MLOnly,        "ML       " },
            { AIEngine::DetectionBackendMode::Hybrid,        "Hybrid   " },
        };

        const float sensitivities[] = { 0.2f, 0.5f, 0.8f };

        int totalExpectedFound = 0;
        int totalCells         = 0;
        int cleanFalsePositive = 0;
        int cleanCells         = 0;

        for (const auto& stim : stimuli)
        {
            logMessage("");
            logMessage("================================================================");
            logMessage("  STIMULUS: " + stim.name
                       + "   (expected: " + juce::String(shortType(stim.expected)) + ")");
            logMessage("================================================================");
            logMessage("  Backend   | Sens | Found? | Detections (type@Hz g=dB c=conf)");
            logMessage("  ----------+------+--------+-----------------------------------");

            for (const auto& be : backends)
            {
                for (float sens : sensitivities)
                {
                    // Fresh engine per cell → no temporal-history carryover between cells.
                    AIEngine ai;
                    ai.prepare(kSampleRate, 512);
                    ai.setEnabled(true);
                    ai.setSourceProfile(AIEngine::SourceProfile::Generic);
                    ai.setDetectionBackendMode(be.mode);
                    ai.setSensitivity(sens);

                    // Load the SHIPPED ML weights so MLOnly/Hybrid genuinely exercise the
                    // neural path (the test binary has no models/ folder next to it, so
                    // prepare() leaves useMLDetection=false → without this, all three
                    // backends collapse to the heuristic path and look identical).
                    if (be.mode != AIEngine::DetectionBackendMode::HeuristicOnly)
                    {
                        if (mlWeightsFile.existsAsFile())
                            ai.setCustomMLWeightsPathForTests(mlWeightsFile);
                        ai.forceMLDetectionEnabledForTests(true);
                    }

                    // Feed many frames, each = stable problem base + FRESH random noise.
                    // This is the key methodological point: the injected problem peak is
                    // identical every frame (temporal consensus should KEEP it), while the
                    // noise floor fluctuates frame-to-frame (consensus should REJECT it) —
                    // exactly like real audio. A per-cell seed keeps the run reproducible.
                    // Seed depends ONLY on stimulus+sensitivity (NOT backend) so all three
                    // backends see the identical noise realisation — any difference between
                    // backend columns is then purely algorithmic, not luck of the noise.
                    std::mt19937 frameRng(static_cast<uint32_t>(
                        juce::String(stim.name + juce::String(sens, 1)).hashCode()));
                    for (int frame = 0; frame < 16; ++frame)
                    {
                        auto noisy = stim.base;
                        addNoise(noisy, frameRng, 2.0f);
                        ai.analyzeSpectrum(noisy, true);
                    }

                    auto pending = ai.getPendingCorrections();

                    bool foundExpected = false;
                    int  extras        = 0;
                    juce::String detailStr;

                    for (const auto& c : pending)
                    {
                        if (c.type == stim.expected && stim.expected != AIEngine::ProblemType::None)
                            foundExpected = true;
                        else
                            ++extras;

                        detailStr += juce::String(shortType(c.type))
                                   + "@" + juce::String(static_cast<int>(c.frequency))
                                   + " g=" + juce::String(c.suggestedGain, 1)
                                   + " c=" + juce::String(c.confidence, 2) + "  ";
                    }

                    if (pending.empty())
                        detailStr = "(none)";

                    const bool isClean = (stim.expected == AIEngine::ProblemType::None);

                    juce::String foundCol;
                    if (isClean)
                        foundCol = pending.empty() ? "  CLEAN " : juce::String(" FP x") + juce::String((int) pending.size());
                    else
                        foundCol = foundExpected ? "  YES   " : "  no    ";

                    logMessage("  " + be.name + " | " + juce::String(sens, 1)
                               + "  | " + foundCol + " | " + detailStr);

                    // Tally
                    if (isClean)
                    {
                        ++cleanCells;
                        if (!pending.empty())
                            ++cleanFalsePositive;
                    }
                    else
                    {
                        ++totalCells;
                        if (foundExpected)
                            ++totalExpectedFound;
                    }

                    (void) extras;
                }
            }
        }

        // ── Summary ──
        logMessage("");
        logMessage("================================================================");
        logMessage("  SUMMARY");
        logMessage("================================================================");
        logMessage("  Expected-problem hit rate: " + juce::String(totalExpectedFound)
                   + "/" + juce::String(totalCells)
                   + " (" + juce::String(totalCells > 0
                        ? 100.0f * (float) totalExpectedFound / (float) totalCells : 0.0f, 1) + "%)");
        logMessage("  Clean false-positive cells: " + juce::String(cleanFalsePositive)
                   + "/" + juce::String(cleanCells)
                   + " (" + juce::String(cleanCells > 0
                        ? 100.0f * (float) cleanFalsePositive / (float) cleanCells : 0.0f, 1) + "%)");
        logMessage("");
        logMessage("  Interpretation:");
        logMessage("   - Higher sensitivity should raise hit rate AND clean FP count.");
        logMessage("   - Compare Heuristic vs ML vs Hybrid columns per stimulus to see");
        logMessage("     which backend actually carries the detection.");
        logMessage("================================================================");

        // ── Sanity gates (lenient — the printed matrix is the deliverable) ──
        expect(totalCells > 0, "No non-clean cells were evaluated");
        expect(totalExpectedFound > 0,
               "Pipeline detected NONE of the injected problems across the entire sweep "
               "— the detection path is silent, which is a real regression.");

        // ====================================================================
        // CAP-3 GUARD — test-only proof that the tilt-robust band-excess veto keeps
        // clean tilted material clean even when rank-3 ML candidates are allowed
        // through. Production remains cap 1/2/2 until the separate cap-unlock commit.
        // ====================================================================
        beginTest("Cap-3 test-only CleanSteep/BassTilt floor after tilt-robust veto");

        const Stimulus* cleanSteepStim = nullptr;
        for (const auto& stim : stimuli)
        {
            if (stim.name == "CleanSteep(control)")
            {
                cleanSteepStim = &stim;
                break;
            }
        }
        expect(cleanSteepStim != nullptr, "CleanSteep stimulus not found");

        struct Cap3Result
        {
            std::vector<AIEngine::Correction> corrections;
        };

        auto runCap3Cell = [&](const std::vector<float>& base,
                               AIEngine::DetectionBackendMode mode,
                               const juce::String& seedKey) -> Cap3Result
        {
            Cap3Result result;

            AIEngine ai;
            ai.prepare(kSampleRate, 512);
            ai.setEnabled(true);
            ai.setSourceProfile(AIEngine::SourceProfile::Generic);
            ai.setDetectionBackendMode(mode);
            ai.setSensitivity(0.8f);

            if (mlWeightsFile.existsAsFile())
                ai.setCustomMLWeightsPathForTests(mlWeightsFile);
            ai.forceMLDetectionEnabledForTests(true);
            ai.getMLEngineForTest().setTopKOverrideForTests(3);

            std::mt19937 frameRng(static_cast<uint32_t>(seedKey.hashCode()));
            for (int frame = 0; frame < 16; ++frame)
            {
                auto noisy = base;
                addNoise(noisy, frameRng, 2.0f);
                ai.analyzeSpectrum(noisy, true);
            }

            result.corrections = ai.getPendingCorrections();
            return result;
        };

        auto formatCorrections = [](const std::vector<AIEngine::Correction>& corrections) {
            juce::String s;
            for (const auto& c : corrections)
            {
                if (s.isNotEmpty())
                    s << " ";
                s << shortType(c.type) << "@" << juce::String(static_cast<int>(c.frequency))
                  << " c=" << juce::String(c.confidence, 2);
            }
            return s.isNotEmpty() ? s : "(none)";
        };

        auto cleanBassTilt = makeFlat();
        applyCleanBassTilt(cleanBassTilt);

        const auto cap3Ml = cleanSteepStim != nullptr
            ? runCap3Cell(cleanSteepStim->base, AIEngine::DetectionBackendMode::MLOnly, "Cap3CleanSteepML")
            : Cap3Result {};
        const auto cap3Hybrid = cleanSteepStim != nullptr
            ? runCap3Cell(cleanSteepStim->base, AIEngine::DetectionBackendMode::Hybrid, "Cap3CleanSteepHybrid")
            : Cap3Result {};
        const auto cap3BassMl = runCap3Cell(cleanBassTilt, AIEngine::DetectionBackendMode::MLOnly, "Cap3CleanBassTiltML");
        const auto cap3BassHybrid = runCap3Cell(cleanBassTilt, AIEngine::DetectionBackendMode::Hybrid, "Cap3CleanBassTiltHybrid");

        logMessage("  cap3 MLOnly CleanSteep : " + formatCorrections(cap3Ml.corrections));
        logMessage("  cap3 Hybrid CleanSteep : " + formatCorrections(cap3Hybrid.corrections));
        logMessage("  cap3 MLOnly CleanBass  : " + formatCorrections(cap3BassMl.corrections));
        logMessage("  cap3 Hybrid CleanBass  : " + formatCorrections(cap3BassHybrid.corrections));

        expect(cap3Ml.corrections.empty() && cap3Hybrid.corrections.empty()
               && cap3BassMl.corrections.empty() && cap3BassHybrid.corrections.empty(),
               "Forcing cap=3 reopened a false positive on clean tilted material.");

        // ====================================================================
        // ROBUSTNESS — multi-seed false-positive / recall under noise variation
        // ====================================================================
        // The matrix above reads the FINAL cell output for ONE noise realisation
        // per cell. That hid a frame-level fragility once before (Codex): a veto
        // can pass today's seed yet go noisy the moment the noise realisation
        // changes. This section sweeps MANY independent seeds and measures, on the
        // ML-bearing backends:
        //   - Clean FINAL false-positive rate  (does the floor return under noise?)
        //   - Clean FRAME-LEVEL false-positive rate (transient flicker the UI would show)
        //   - Resonance recall                 (does the prominence veto keep true peaks?)
        beginTest("Robustness: multi-seed FP rate + Resonance recall under noise");

        const Stimulus& resStim   = stimuli[0]; // Resonance@800
        const Stimulus& cleanStim = stimuli[3]; // Clean control
        expect(resStim.expected   == AIEngine::ProblemType::Resonance, "stimulus[0] is not Resonance");
        expect(cleanStim.expected == AIEngine::ProblemType::None,      "stimulus[3] is not Clean");

        const BackendOpt mlBackends[] = {
            { AIEngine::DetectionBackendMode::MLOnly, "ML       " },
            { AIEngine::DetectionBackendMode::Hybrid, "Hybrid   " },
        };
        constexpr int   kSeeds = 64;
        constexpr float kSens  = 0.5f;

        int cleanFinalFP_ML = 0, cleanFinalFP_Hy = 0; // final-output FP, split by backend
        int cleanTrialsPer  = 0;                       // trials per backend (== kSeeds)
        int cleanFrameFP = 0, cleanFrames = 0;         // frame-level FP (both backends)
        int resFound     = 0, resTrials   = 0;         // Resonance recall (both backends)
        juce::String fpSamples;                        // a few example FP rows for the log

        // Run one cell: feed 16 noisy frames, optionally tally per-frame FP, return final output.
        auto runCell = [&](const Stimulus& stim, AIEngine::DetectionBackendMode mode,
                           uint32_t seed, bool trackFrames, int& frameFPaccum, int& frameAccum)
        {
            AIEngine ai;
            ai.prepare(kSampleRate, 512);
            ai.setEnabled(true);
            ai.setSourceProfile(AIEngine::SourceProfile::Generic);
            ai.setDetectionBackendMode(mode);
            ai.setSensitivity(kSens);
            if (mode != AIEngine::DetectionBackendMode::HeuristicOnly)
            {
                if (mlWeightsFile.existsAsFile())
                    ai.setCustomMLWeightsPathForTests(mlWeightsFile);
                ai.forceMLDetectionEnabledForTests(true);
            }
            std::mt19937 rng(seed);
            for (int frame = 0; frame < 16; ++frame)
            {
                auto noisy = stim.base;
                addNoise(noisy, rng, 2.0f);
                ai.analyzeSpectrum(noisy, true);
                if (trackFrames)
                {
                    ++frameAccum;
                    if (! ai.getPendingCorrections().empty())
                        ++frameFPaccum;
                }
            }
            return ai.getPendingCorrections();
        };

        for (const auto& be : mlBackends)
        {
            const bool isHybrid = (be.mode == AIEngine::DetectionBackendMode::Hybrid);
            for (int seed = 0; seed < kSeeds; ++seed)
            {
                // Clean — final + frame-level false positives
                {
                    const uint32_t s = static_cast<uint32_t>(
                        juce::String("cleanRobust" + juce::String(seed)).hashCode());
                    auto pend = runCell(cleanStim, be.mode, s, true, cleanFrameFP, cleanFrames);
                    if (! pend.empty())
                    {
                        if (isHybrid) ++cleanFinalFP_Hy; else ++cleanFinalFP_ML;
                        if (fpSamples.length() < 400)
                        {
                            juce::String d;
                            for (const auto& c : pend)
                                d += juce::String(shortType(c.type)) + "@" + juce::String((int) c.frequency)
                                   + " c=" + juce::String(c.confidence, 2) + " ";
                            fpSamples += "      " + be.name.trim() + " seed=" + juce::String(seed)
                                       + " : " + d + "\n";
                        }
                    }
                }
                // Resonance — recall (true-positive retention)
                {
                    const uint32_t s = static_cast<uint32_t>(
                        juce::String("resRobust" + juce::String(seed)).hashCode());
                    int dummyA = 0, dummyB = 0;
                    auto pend = runCell(resStim, be.mode, s, false, dummyA, dummyB);
                    ++resTrials;
                    bool found = false;
                    for (const auto& c : pend)
                        if (c.type == AIEngine::ProblemType::Resonance)
                            found = true;
                    if (found)
                        ++resFound;
                }
            }
            cleanTrialsPer = kSeeds; // same per backend
        }

        const float cleanRateML  = cleanTrialsPer > 0 ? 100.0f * (float) cleanFinalFP_ML / (float) cleanTrialsPer : 0.0f;
        const float cleanRateHy  = cleanTrialsPer > 0 ? 100.0f * (float) cleanFinalFP_Hy / (float) cleanTrialsPer : 0.0f;
        const float cleanFrameRate = cleanFrames > 0 ? 100.0f * (float) cleanFrameFP / (float) cleanFrames : 0.0f;
        const float resRecall      = resTrials   > 0 ? 100.0f * (float) resFound     / (float) resTrials   : 0.0f;

        logMessage("");
        logMessage("  --- Robustness over " + juce::String(kSeeds) + " seeds per backend (sens 0.5) ---");
        logMessage("  Clean FINAL FP rate  MLOnly : " + juce::String(cleanFinalFP_ML) + "/" + juce::String(cleanTrialsPer)
                   + " (" + juce::String(cleanRateML, 1) + "%)");
        logMessage("  Clean FINAL FP rate  Hybrid : " + juce::String(cleanFinalFP_Hy) + "/" + juce::String(cleanTrialsPer)
                   + " (" + juce::String(cleanRateHy, 1) + "%)   [Hybrid adds the heuristic resonance supplement]");
        logMessage("  Clean FRAME-LEVEL FP rate   : " + juce::String(cleanFrameFP) + "/" + juce::String(cleanFrames)
                   + " (" + juce::String(cleanFrameRate, 1) + "%)  (both backends)");
        logMessage("  Resonance recall            : " + juce::String(resFound) + "/" + juce::String(resTrials)
                   + " (" + juce::String(resRecall, 1) + "%)  (both backends)");
        if (fpSamples.isNotEmpty())
        {
            logMessage("  Residual clean-FP samples (for triage):");
            logMessage(fpSamples.trimEnd());
        }

        // ── Regression gates ──
        // The ML floor used to fire on EVERY clean frame (100%). After 4A/4B/4C the
        // MLOnly path must be essentially clean across noise realisations, and the
        // prominence veto must keep the genuine +20 dB resonance. The Hybrid gate is
        // deliberately looser: it still carries the heuristic resonance supplement
        // (c~=0.45, right at the Commit-3 gate), whose tightening is a separate,
        // already-scoped follow-up (Hybrid supplement-only). These thresholds guard
        // against the floor RETURNING; the logged rates above are the live signal.
        expect(cleanRateML <= 5.0f,
               "MLOnly clean false-positive rate regressed above 5% — the ML floor / band-excess is back.");
        expect(cleanFrameRate <= 25.0f,
               "Clean frame-level false-positive rate regressed above 25% — transient floor flicker.");
        expect(cleanRateHy <= 20.0f,
               "Hybrid clean false-positive rate regressed above 20% — heuristic supplement too loose.");
        expect(resRecall >= 85.0f,
               "Resonance recall fell below 85% — the prominence veto is too aggressive.");
    }
};

static AIBackendSweepTest sAIBackendSweepTest;


// =============================================================================
// KNOB EFFECT PROOF (Marco's request: "certezza assoluta" that tweaking the
// SENSITIVITY and CORRECTION knobs changes something concrete).
//
// This test does NOT deduce from the source — it MEASURES:
//   PART 1  SENSITIVITY → number of detected problems + their gains/confidence.
//   PART 2  CORRECTION (aiStrength) → the gain actually applied to a correction.
// Both knobs are swept; the tables are printed and a hard assertion FAILS the
// build if a knob ever stops affecting the output (a permanent wiring guard).
//
// Honest nuance baked into the assertions: the heuristic path responds to
// sensitivity continuously (thresholds + gain + confidence all scale), so we
// hard-assert it changes. The ML path's accept gates (per-class margin, rank<=2,
// top-K) are INTENTIONALLY sensitivity-independent to keep the clean floor shut,
// so ML is logged as evidence but not hard-gated.
// =============================================================================
class AIKnobEffectTest : public juce::UnitTest
{
public:
    AIKnobEffectTest()
        : juce::UnitTest("AI Knob Effect — sensitivity & correction proof", "AI-Knobs") {}

    // Stimulus carrying several REAL problems so sensitivity has something to
    // modulate: a strong +18 dB resonance (always found) plus weaker mud/harsh
    // humps sitting near the detection threshold.
    static std::vector<float> makeMultiProblemStim()
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        addPeakDb(spec,  800.0f, 18.0f, 0.04f);  // strong resonance — always detected
        addPeakDb(spec,  250.0f,  7.0f, 0.30f);  // moderate muddiness — near threshold
        addPeakDb(spec, 4000.0f,  6.0f, 0.20f);  // moderate harshness — near threshold
        return spec;
    }

    // Strong multi-problem witness for the ML top-K cap. The model emits three
    // validated corrections here (Res, Mud, Bxy); sensitivity should expose them
    // as 1/2/3 via the cap, not by loosening thresholds.
    static std::vector<float> makeMlCapWitnessStim()
    {
        auto spec = makeFlat();
        applyPinkTilt(spec);
        addPeakDb(spec, 800.0f, 18.0f, 0.04f);  // Resonance
        addPeakDb(spec, 250.0f, 12.0f, 0.30f);  // Muddiness
        addPeakDb(spec, 500.0f, 14.0f, 0.35f);  // Boxyness
        return spec;
    }

    struct Response
    {
        int count = 0;
        float sumAbsGain = 0.0f;
        float sumConf = 0.0f;
        juce::String details;
    };

    static Response runCell(const std::vector<float>& base,
                            AIEngine::DetectionBackendMode mode,
                            float sens,
                            const juce::File& mlWeights,
                            int topKOverride = 0)
    {
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);
        ai.setDetectionBackendMode(mode);
        ai.setSensitivity(sens);

        if (mode != AIEngine::DetectionBackendMode::HeuristicOnly)
        {
            if (mlWeights.existsAsFile())
                ai.setCustomMLWeightsPathForTests(mlWeights);
            ai.forceMLDetectionEnabledForTests(true);
            if (topKOverride > 0)
                ai.getMLEngineForTest().setTopKOverrideForTests(topKOverride);
        }

        // force=true → bypasses temporal persistence, fully deterministic.
        std::mt19937 rng(1234u);
        for (int f = 0; f < 16; ++f)
        {
            auto noisy = base;
            addNoise(noisy, rng, 1.0f);
            ai.analyzeSpectrum(noisy, true);
        }

        Response r;
        for (const auto& c : ai.getPendingCorrections())
        {
            ++r.count;
            r.sumAbsGain += std::abs(c.suggestedGain);
            r.sumConf    += c.confidence;
            if (r.details.isNotEmpty())
                r.details << " ";
            r.details << shortType(c.type) << "@" << juce::String(static_cast<int>(c.frequency))
                      << " c=" << juce::String(c.confidence, 2);
        }
        return r;
    }

    void runTest() override
    {
        const juce::File mlWeights =
            juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
                .getChildFile("Resources/Models/ml_weights.bin");

        const auto stim = makeMultiProblemStim();

        // ---- PART 1a: SENSITIVITY on the heuristic path (hard-asserted) ------
        beginTest("SENSITIVITY changes detection output — Heuristic path");
        logMessage("  sens | #problems | sum|gain|dB | sum conf");
        logMessage("  -----+-----------+------------+---------");
        Response loH, hiH;
        bool firstH = true;
        for (float s = 0.0f; s <= 1.0001f; s += 0.25f)
        {
            const auto r = runCell(stim, AIEngine::DetectionBackendMode::HeuristicOnly, s, mlWeights);
            logMessage("  " + juce::String(s, 2) + " |     " + juce::String(r.count)
                       + "     |   " + juce::String(r.sumAbsGain, 2)
                       + "   |  " + juce::String(r.sumConf, 2));
            if (firstH) { loH = r; firstH = false; }
            hiH = r;
        }
        const bool heurChanged = (loH.count != hiH.count)
                              || std::abs(loH.sumAbsGain - hiH.sumAbsGain) > 0.01f
                              || std::abs(loH.sumConf    - hiH.sumConf)    > 0.01f;
        expect(heurChanged,
               "Sensitivity 0.0 vs 1.0 gave an IDENTICAL heuristic result — the knob is inert.");

        // ---- PART 1b: SENSITIVITY on the ML path (logged evidence) -----------
        beginTest("SENSITIVITY effect on ML path (evidence; ML accept-gates are sensitivity-independent by design)");
        logMessage("  ML weights: " + juce::String(mlWeights.existsAsFile() ? "FOUND" : "MISSING"));
        logMessage("  sens | #problems | sum|gain|dB | sum conf");
        logMessage("  -----+-----------+------------+---------");
        for (float s = 0.0f; s <= 1.0001f; s += 0.25f)
        {
            const auto r = runCell(stim, AIEngine::DetectionBackendMode::MLOnly, s, mlWeights);
            logMessage("  " + juce::String(s, 2) + " |     " + juce::String(r.count)
                       + "     |   " + juce::String(r.sumAbsGain, 2)
                       + "   |  " + juce::String(r.sumConf, 2));
        }

        beginTest("ML cap-3 is exercised on a validated multi-problem witness");
        const auto capWitness = makeMlCapWitnessStim();
        const auto capLo          = runCell(capWitness, AIEngine::DetectionBackendMode::MLOnly, 0.0f, mlWeights);
        const auto capDefault     = runCell(capWitness, AIEngine::DetectionBackendMode::MLOnly, 0.5f, mlWeights);
        const auto capHiForcedTwo = runCell(capWitness, AIEngine::DetectionBackendMode::MLOnly, 1.0f, mlWeights, 2);
        const auto capHi          = runCell(capWitness, AIEngine::DetectionBackendMode::MLOnly, 1.0f, mlWeights);

        logMessage("  mode          | #problems | details");
        logMessage("  --------------+-----------+------------------------------");
        logMessage("  sens 0.00     |     " + juce::String(capLo.count)          + "     | " + capLo.details);
        logMessage("  sens 0.50     |     " + juce::String(capDefault.count)     + "     | " + capDefault.details);
        logMessage("  sens 1.00 k=2 |     " + juce::String(capHiForcedTwo.count) + "     | " + capHiForcedTwo.details);
        logMessage("  sens 1.00 k=3 |     " + juce::String(capHi.count)          + "     | " + capHi.details);

        expect(capLo.count == 1 && capHi.count == 3 && capHiForcedTwo.count < capHi.count,
               "ML cap-3 should expose validated corrections that cap-2 suppresses.");

        // ---- PART 2: CORRECTION (aiStrength) scales the applied gain ----------
        beginTest("CORRECTION knob scales applied gain exactly: gain = suggested * strength");
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        AIEngine::Correction c;
        c.type          = AIEngine::ProblemType::Resonance;
        c.frequency     = 800.0f;
        c.suggestedGain = -6.0f;
        logMessage("  strength | applied gain (dB)   [suggested = -6.00 dB]");
        logMessage("  ---------+------------------");
        for (float st : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            ai.setStrength(st);
            const auto scaled = ai.getScaledCorrection(c);
            logMessage("    " + juce::String(st, 2) + "   |   " + juce::String(scaled.suggestedGain, 3));
            expectWithinAbsoluteError(scaled.suggestedGain, c.suggestedGain * st, 1.0e-4f,
                "CORRECTION knob did not scale the gain linearly — aiStrength is not wired to the output.");
        }
    }
};

static AIKnobEffectTest sAIKnobEffectTest;

// =============================================================================
// Ticket #3 diagnostic — same stimulus helpers, same heuristic replay, but with
// test-only resonance debug snapshots captured from the REAL detector.
// =============================================================================
class AITicket3ResonanceInversionDiagnostic : public juce::UnitTest
{
public:
    AITicket3ResonanceInversionDiagnostic()
        : juce::UnitTest("AI Ticket #3 — resonance inversion diagnostic", "AI-Diag") {}

    struct FrameTrace
    {
        AIEngine::ResonanceDebugSnapshot dbg;
        std::vector<AIEngine::Correction> corrections;
    };

    template <typename Item, typename FreqFn>
    static const Item* findNearestByRatio(const std::vector<Item>& items,
                                          float targetHz,
                                          float maxRatioDelta,
                                          FreqFn getFreq)
    {
        const Item* best = nullptr;
        float bestDelta = std::numeric_limits<float>::max();
        for (const auto& item : items)
        {
            const float f = getFreq(item);
            if (f <= 0.0f)
                continue;
            const float ratio = f / targetHz;
            const float delta = std::abs(std::log(ratio));
            if (ratio > (1.0f - maxRatioDelta) && ratio < (1.0f + maxRatioDelta) && delta < bestDelta)
            {
                best = &item;
                bestDelta = delta;
            }
        }
        return best;
    }

    static std::vector<FrameTrace> runHeuristicCleanSteep(float sens)
    {
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::HeuristicOnly);
        ai.setSensitivity(sens);

        auto base = makeFlat();
        applySteepTilt(base);

        std::mt19937 frameRng(static_cast<uint32_t>(
            juce::String("CleanSteep(control)" + juce::String(sens, 1)).hashCode()));
        std::vector<FrameTrace> frames;
        frames.reserve(16);

        for (int f = 0; f < 16; ++f)
        {
            auto noisy = base;
            addNoise(noisy, frameRng, 2.0f);
            ai.analyzeSpectrum(noisy, true);
            frames.push_back({ ai.getLastResonanceDebugSnapshotForTests(), ai.getPendingCorrections() });
        }

        return frames;
    }

    static std::vector<FrameTrace> runHeuristicStimulus(const std::vector<float>& base,
                                                        float sens,
                                                        const juce::String& seedKey,
                                                        std::initializer_list<float> probeFreqs = {})
    {
        AIEngine ai;
        ai.prepare(kSampleRate, 512);
        ai.setEnabled(true);
        ai.setSourceProfile(AIEngine::SourceProfile::Generic);
        ai.setDetectionBackendMode(AIEngine::DetectionBackendMode::HeuristicOnly);
        ai.setSensitivity(sens);
        ai.setResonanceDebugProbeFrequenciesForTests(std::vector<float>(probeFreqs));

        std::mt19937 frameRng(static_cast<uint32_t>(seedKey.hashCode()));
        std::vector<FrameTrace> frames;
        frames.reserve(16);

        for (int f = 0; f < 16; ++f)
        {
            auto noisy = base;
            addNoise(noisy, frameRng, 2.0f);
            ai.analyzeSpectrum(noisy, true);
            frames.push_back({ ai.getLastResonanceDebugSnapshotForTests(), ai.getPendingCorrections() });
        }

        return frames;
    }

    static juce::String formatCorrection(const AIEngine::Correction* c)
    {
        if (c == nullptr)
            return "none";
        return juce::String::formatted("%s@%.0f g=%.1f c=%.2f",
                                       shortType(c->type),
                                       c->frequency,
                                       c->suggestedGain,
                                       c->confidence);
    }

    static juce::String formatRaw(const AIEngine::ResonanceDebugCandidate* c)
    {
        if (c == nullptr)
            return "none";
        return juce::String::formatted("%.0fHz h=%.2f p=%.2f",
                                       c->frequency,
                                       c->peakHeight,
                                       c->prominenceDb);
    }

    static juce::String formatEval(const AIEngine::ResonanceDebugPeakEval* e)
    {
        if (e == nullptr)
            return "none";
        return juce::String::formatted("%.0fHz h=%.2f p=%.2f frames=%d z=%.2f conf=%.2f early=%s confGate=%s",
                                       e->frequency,
                                       e->peakHeight,
                                       e->prominenceDb,
                                       e->frameCount,
                                       e->zScore,
                                       e->confidence,
                                       e->passedEarlyGate ? "yes" : "no",
                                       e->passedConfidenceGate ? "yes" : "no");
    }

    static juce::String formatProbe(const AIEngine::ResonanceDebugProbe* p)
    {
        if (p == nullptr)
            return "none";
        return juce::String::formatted("target=%.0f sampled=%.0f mag=%.2f h=%.2f p=%.2f localMax=%s cand=%s",
                                       p->targetFrequency,
                                       p->sampledFrequency,
                                       p->magnitude,
                                       p->peakHeight,
                                       p->prominenceDb,
                                       p->localMax ? "yes" : "no",
                                       p->passedCandidateGate ? "yes" : "no");
    }

    static int countFramesWithTargetCorrection(const std::vector<FrameTrace>& frames, float targetHz)
    {
        int hits = 0;
        for (const auto& frame : frames)
        {
            if (findNearestByRatio(frame.corrections, targetHz, 0.12f,
                                   [](const AIEngine::Correction& c) { return c.frequency; }) != nullptr)
            {
                ++hits;
            }
        }
        return hits;
    }

    struct OfflineFrameMetric
    {
        float bandExcessDb = 0.0f;
        float peakProminenceDb = 0.0f;
        float widthHz = 0.0f;
    };

    struct OfflineAggregate
    {
        float bandExcessDb = 0.0f;
        float peakProminenceDb = 0.0f;
        float widthHz = 0.0f;
        float stableRatio = 0.0f;
        float peakStableRatio = 0.0f;
        float softPeakStableRatio = 0.0f;
    };

    static float medianOf(std::vector<float> values)
    {
        if (values.empty())
            return 0.0f;
        std::sort(values.begin(), values.end());
        const size_t mid = values.size() / 2;
        if ((values.size() & 1u) != 0)
            return values[mid];
        return 0.5f * (values[mid - 1] + values[mid]);
    }

    static OfflineFrameMetric computeOfflineMetric(const std::vector<float>& spec, float targetHz)
    {
        constexpr float kBandHalfOctaves = 0.04f;
        constexpr float kContextHalfOctaves = 0.25f;
        constexpr float kContextExcludeHalfOctaves = 0.08f;
        const float binHz = static_cast<float>(kSampleRate) / kFFTSize;

        const float bandLo = targetHz * std::pow(2.0f, -kBandHalfOctaves);
        const float bandHi = targetHz * std::pow(2.0f,  kBandHalfOctaves);
        const float contextLo = targetHz * std::pow(2.0f, -kContextHalfOctaves);
        const float contextHi = targetHz * std::pow(2.0f,  kContextHalfOctaves);
        const float excludeLo = targetHz * std::pow(2.0f, -kContextExcludeHalfOctaves);
        const float excludeHi = targetHz * std::pow(2.0f,  kContextExcludeHalfOctaves);

        std::vector<float> band;
        std::vector<float> context;
        band.reserve(64);
        context.reserve(512);

        int peakBin = -1;
        float peak = -1000.0f;

        for (int i = 1; i < static_cast<int>(spec.size()); ++i)
        {
            const float f = static_cast<float>(i) * binHz;
            const float v = spec[static_cast<size_t>(i)];

            if (f >= bandLo && f <= bandHi)
            {
                band.push_back(v);
                if (v > peak)
                {
                    peak = v;
                    peakBin = i;
                }
            }

            if (f >= contextLo && f <= contextHi && (f < excludeLo || f > excludeHi))
                context.push_back(v);
        }

        const float bandMedian = medianOf(std::move(band));
        const float contextMedian = medianOf(std::move(context));

        OfflineFrameMetric result;
        result.bandExcessDb = bandMedian - contextMedian;
        result.peakProminenceDb = peak - contextMedian;

        if (peakBin >= 0 && result.peakProminenceDb > 0.0f)
        {
            const float halfHeight = contextMedian + 0.5f * result.peakProminenceDb;
            int left = peakBin;
            int right = peakBin;
            while (left > 1 && spec[static_cast<size_t>(left - 1)] >= halfHeight)
                --left;
            while (right + 1 < static_cast<int>(spec.size()) && spec[static_cast<size_t>(right + 1)] >= halfHeight)
                ++right;
            result.widthHz = static_cast<float>(right - left + 1) * binHz;
        }

        return result;
    }

    static OfflineAggregate runOfflineMetric(const std::vector<float>& base, float targetHz, const juce::String& seedKey)
    {
        std::mt19937 rng(static_cast<uint32_t>(seedKey.hashCode()));
        std::vector<float> bandExcess;
        std::vector<float> peakProm;
        std::vector<float> widths;
        int stable = 0;
        int peakStable = 0;
        int softPeakStable = 0;

        for (int frame = 0; frame < 16; ++frame)
        {
            auto noisy = base;
            addNoise(noisy, rng, 2.0f);
            const auto metric = computeOfflineMetric(noisy, targetHz);
            bandExcess.push_back(metric.bandExcessDb);
            peakProm.push_back(metric.peakProminenceDb);
            widths.push_back(metric.widthHz);
            if (metric.bandExcessDb >= 3.0f)
                ++stable;
            if (metric.peakProminenceDb >= 8.0f)
                ++peakStable;
            if (metric.peakProminenceDb >= 5.5f)
                ++softPeakStable;
        }

        OfflineAggregate result;
        result.bandExcessDb = medianOf(std::move(bandExcess));
        result.peakProminenceDb = medianOf(std::move(peakProm));
        result.widthHz = medianOf(std::move(widths));
        result.stableRatio = static_cast<float>(stable) / 16.0f;
        result.peakStableRatio = static_cast<float>(peakStable) / 16.0f;
        result.softPeakStableRatio = static_cast<float>(softPeakStable) / 16.0f;
        return result;
    }

    static juce::String formatOffline(const OfflineAggregate& m)
    {
        return juce::String::formatted("band=%.2f peak=%.2f width=%.0fHz stable=%.2f peakStable=%.2f softStable=%.2f",
                                       m.bandExcessDb,
                                       m.peakProminenceDb,
                                       m.widthHz,
                                       m.stableRatio,
                                       m.peakStableRatio,
                                       m.softPeakStableRatio);
    }

    static bool compositeOfflinePass(const OfflineAggregate& m)
    {
        const bool broadBand = m.bandExcessDb >= 3.0f && m.stableRatio >= 0.75f;
        const bool narrowStable = m.peakProminenceDb >= 5.5f
                                  && m.softPeakStableRatio >= 0.75f
                                  && m.widthHz >= 15.0f;
        return broadBand || narrowStable;
    }

    void runTest() override
    {
        beginTest("CleanSteep low-sensitivity resonance FP is resolved");

        const auto low  = runHeuristicCleanSteep(0.2f);
        const auto mid  = runHeuristicCleanSteep(0.5f);
        const auto high = runHeuristicCleanSteep(0.8f);

        expectEquals(static_cast<int>(low.back().corrections.size()), 0,
                     "Sensitivity 0.2 should now remain clean on CleanSteep.");
        expectEquals(static_cast<int>(mid.back().corrections.size()), 0,
                     "Sensitivity 0.5 should remain clean on CleanSteep.");
        expectEquals(static_cast<int>(high.back().corrections.size()), 0,
                     "Sensitivity 0.8 should remain clean on CleanSteep.");

        beginTest("Log resonance candidate/persistence/confidence path across sensitivities");

        for (float sens : { 0.20f, 0.35f, 0.50f, 0.80f })
        {
            const auto frames = runHeuristicCleanSteep(sens);
            const auto& last = frames.back();

            int maxRaw = 0;
            int maxPersistent = 0;
            int capHitFrames = 0;
            for (const auto& frame : frames)
            {
                maxRaw = juce::jmax(maxRaw, frame.dbg.rawCandidateCount);
                maxPersistent = juce::jmax(maxPersistent, frame.dbg.persistentCount);
                if (frame.dbg.persistentCapHit)
                    ++capHitFrames;
            }

            logMessage("------------------------------------------------------------");
            logMessage("sens=" + juce::String(sens, 2)
                       + " outer=" + juce::String(last.dbg.outerThreshold, 3)
                       + " outerFactor=" + juce::String(last.dbg.outerSensitivityFactor, 3)
                       + " adaptSens=" + juce::String(last.dbg.adaptiveSensitivityMultiplier, 3)
                       + " adapted=" + juce::String(last.dbg.adaptedThreshold, 3)
                       + " inner=" + juce::String(last.dbg.innerSensitivityFactor, 3)
                       + " effective=" + juce::String(last.dbg.effectiveThreshold, 3));
            logMessage("  raw max=" + juce::String(maxRaw)
                       + " persistent max=" + juce::String(maxPersistent)
                       + " cap-hit frames=" + juce::String(capHitFrames));
            logMessage("  final corrections:");
            if (last.corrections.empty())
            {
                logMessage("    (none)");
            }
            else
            {
                for (const auto& c : last.corrections)
                    logMessage("    " + formatCorrection(&c));
            }

            logMessage("  frames with final correction near 5890Hz  = "
                       + juce::String(countFramesWithTargetCorrection(frames, 5890.0f)));
            logMessage("  frames with final correction near 14373Hz = "
                       + juce::String(countFramesWithTargetCorrection(frames, 14373.0f)));

            const auto* raw5890 = findNearestByRatio(last.dbg.rawCandidates, 5890.0f, 0.12f,
                                                     [](const AIEngine::ResonanceDebugCandidate& c) { return c.frequency; });
            const auto* raw14373 = findNearestByRatio(last.dbg.rawCandidates, 14373.0f, 0.12f,
                                                      [](const AIEngine::ResonanceDebugCandidate& c) { return c.frequency; });
            const auto* eval5890 = findNearestByRatio(last.dbg.evaluatedPeaks, 5890.0f, 0.12f,
                                                      [](const AIEngine::ResonanceDebugPeakEval& e) { return e.frequency; });
            const auto* eval14373 = findNearestByRatio(last.dbg.evaluatedPeaks, 14373.0f, 0.12f,
                                                       [](const AIEngine::ResonanceDebugPeakEval& e) { return e.frequency; });
            const auto* corr5890 = findNearestByRatio(last.corrections, 5890.0f, 0.12f,
                                                      [](const AIEngine::Correction& c) { return c.frequency; });
            const auto* corr14373 = findNearestByRatio(last.corrections, 14373.0f, 0.12f,
                                                       [](const AIEngine::Correction& c) { return c.frequency; });

            logMessage("  nearest raw  5890   : " + formatRaw(raw5890));
            logMessage("  nearest eval 5890   : " + formatEval(eval5890));
            logMessage("  nearest corr 5890   : " + formatCorrection(corr5890));
            logMessage("  nearest raw  14373  : " + formatRaw(raw14373));
            logMessage("  nearest eval 14373  : " + formatEval(eval14373));
            logMessage("  nearest corr 14373  : " + formatCorrection(corr14373));

            if (!last.dbg.rawCandidates.empty())
            {
                juce::String line = "  top raw candidates: ";
                const int limit = juce::jmin(8, static_cast<int>(last.dbg.rawCandidates.size()));
                for (int i = 0; i < limit; ++i)
                {
                    if (i > 0) line << " | ";
                    line << formatRaw(&last.dbg.rawCandidates[static_cast<size_t>(i)]);
                }
                logMessage(line);
            }

            if (!last.dbg.evaluatedPeaks.empty())
            {
                juce::String line = "  evaluated peaks: ";
                const int limit = juce::jmin(8, static_cast<int>(last.dbg.evaluatedPeaks.size()));
                for (int i = 0; i < limit; ++i)
                {
                    if (i > 0) line << " | ";
                    line << formatEval(&last.dbg.evaluatedPeaks[static_cast<size_t>(i)]);
                }
                logMessage(line);
            }
        }

        beginTest("HF resonance recall at low sensitivity on flat vs pink vs steep backgrounds");

        auto makeTiltedRes = [](float freqHz, int tiltMode) {
            auto spec = makeFlat();
            if (tiltMode == 1)
                applyPinkTilt(spec);
            else if (tiltMode == 2)
                applySteepTilt(spec);
            addPeakDb(spec, freqHz, 20.0f, 0.02f);
            return spec;
        };

        for (float targetHz : { 6000.0f, 12000.0f })
        {
            for (const auto& bg : { std::pair<const char*, int>{ "flat", 0 },
                                    std::pair<const char*, int>{ "pink", 1 },
                                    std::pair<const char*, int>{ "steep", 2 } })
            {
                const auto frames = runHeuristicStimulus(
                    makeTiltedRes(targetHz, bg.second),
                    0.2f,
                    juce::String::formatted("HFRes_%.0f_%s_0.2", targetHz, bg.first),
                    { targetHz });
                const auto& last = frames.back();
                const auto* probe = findNearestByRatio(last.dbg.probes, targetHz, 0.12f,
                                                       [](const AIEngine::ResonanceDebugProbe& p) { return p.targetFrequency; });
                const auto* raw = findNearestByRatio(last.dbg.rawCandidates, targetHz, 0.12f,
                                                     [](const AIEngine::ResonanceDebugCandidate& c) { return c.frequency; });
                const auto* eval = findNearestByRatio(last.dbg.evaluatedPeaks, targetHz, 0.12f,
                                                      [](const AIEngine::ResonanceDebugPeakEval& e) { return e.frequency; });
                const auto* corr = findNearestByRatio(last.corrections, targetHz, 0.12f,
                                                      [](const AIEngine::Correction& c) { return c.frequency; });

                logMessage("HF recall sens=0.2  target=" + juce::String(targetHz, 0)
                           + "Hz bg=" + bg.first);
                logMessage("  probe: " + formatProbe(probe));
                logMessage("  raw  : " + formatRaw(raw));
                logMessage("  eval : " + formatEval(eval));
                logMessage("  corr : " + formatCorrection(corr));
            }
        }

        beginTest("Compare true 12k resonance vs 14.3k CleanSteep ripple at low sensitivity");

        {
            auto steep12k = makeFlat();
            applySteepTilt(steep12k);
            addPeakDb(steep12k, 12000.0f, 20.0f, 0.02f);

            const auto true12k = runHeuristicStimulus(steep12k, 0.2f, "True12kSteep_0.2", { 12000.0f });
            const auto ripple14k = runHeuristicStimulus([&]() {
                auto spec = makeFlat();
                applySteepTilt(spec);
                return spec;
            }(), 0.2f, "CleanSteep(control)0.2", { 14373.0f });

            const auto& tLast = true12k.back();
            const auto& rLast = ripple14k.back();

            const auto* tProbe = findNearestByRatio(tLast.dbg.probes, 12000.0f, 0.12f,
                                                    [](const AIEngine::ResonanceDebugProbe& p) { return p.targetFrequency; });
            const auto* rProbe = findNearestByRatio(rLast.dbg.probes, 14373.0f, 0.12f,
                                                    [](const AIEngine::ResonanceDebugProbe& p) { return p.targetFrequency; });
            const auto* tCorr = findNearestByRatio(tLast.corrections, 12000.0f, 0.12f,
                                                   [](const AIEngine::Correction& c) { return c.frequency; });
            const auto* rCorr = findNearestByRatio(rLast.corrections, 14373.0f, 0.12f,
                                                   [](const AIEngine::Correction& c) { return c.frequency; });

            logMessage("true 12k on steep:");
            logMessage("  probe: " + formatProbe(tProbe));
            logMessage("  corr : " + formatCorrection(tCorr));
            logMessage("clean-steep ripple near 14.3k:");
            logMessage("  probe: " + formatProbe(rProbe));
            logMessage("  corr : " + formatCorrection(rCorr));

            expect(tCorr != nullptr,
                   "True 12k resonance on steep tilt should survive the HF octave-salience gate.");
            expect(rCorr == nullptr,
                   "Clean-steep 14.3k ripple should not pass the HF octave-salience gate.");
        }

        beginTest("Ticket #2 offline log-trend band-excess diagnostics for Boxy/Boom");

        {
            auto cleanSteep = makeFlat();
            applySteepTilt(cleanSteep);

            auto cleanBassTilt = makeFlat();
            applyCleanBassTilt(cleanBassTilt);

            auto boxy500 = makeFlat();
            applySteepTilt(boxy500);
            addPeakDb(boxy500, 500.0f, 12.0f, 0.35f);

            auto boxy500Res800 = boxy500;
            addPeakDb(boxy500Res800, 800.0f, 20.0f, 0.04f);

            auto boom60 = makeFlat();
            applySteepTilt(boom60);
            addPeakDb(boom60, 60.0f, 12.0f, 0.50f);

            const auto boxyClean = runBroadBandMetric(
                cleanSteep, 300.0f, 800.0f,
                { { 150.0f, 280.0f }, { 850.0f, 1600.0f } },
                100.0f, 8000.0f,
                "Ticket2_Boxy_CleanSteep");
            const auto boxyTrue = runBroadBandMetric(
                boxy500, 300.0f, 800.0f,
                { { 150.0f, 280.0f }, { 850.0f, 1600.0f } },
                100.0f, 8000.0f,
                "Ticket2_Boxy_True500");
            const auto boxyUnderRes = runBroadBandMetric(
                boxy500Res800, 300.0f, 800.0f,
                { { 150.0f, 280.0f }, { 850.0f, 1600.0f } },
                100.0f, 8000.0f,
                "Ticket2_Boxy_True500_Res800");

            const auto boomCleanSteep = runBroadBandMetric(
                cleanSteep, 30.0f, 100.0f,
                { { 100.0f, 500.0f } },
                100.0f, 5000.0f,
                "Ticket2_Boom_CleanSteep");
            const auto boomCleanBass = runBroadBandMetric(
                cleanBassTilt, 30.0f, 100.0f,
                { { 100.0f, 500.0f } },
                100.0f, 5000.0f,
                "Ticket2_Boom_CleanBassTilt");
            const auto boomTrue = runBroadBandMetric(
                boom60, 30.0f, 100.0f,
                { { 100.0f, 500.0f } },
                100.0f, 5000.0f,
                "Ticket2_Boom_True60");

            logMessage("  Boxy CleanSteep       : " + formatBroadBand(boxyClean));
            logMessage("  Boxy@500 SteepTilt    : " + formatBroadBand(boxyTrue));
            logMessage("  Boxy@500 + Res@800    : " + formatBroadBand(boxyUnderRes));
            logMessage("  Boom CleanSteep       : " + formatBroadBand(boomCleanSteep));
            logMessage("  Boom CleanBassTilt    : " + formatBroadBand(boomCleanBass));
            logMessage("  Boom@60 SteepTilt     : " + formatBroadBand(boomTrue));

            expect(boxyClean.trendExcessDb < 3.0f,
                   "Boxy log-trend metric still false-positives on CleanSteep.");
            expect(boxyTrue.trendExcessDb >= 3.0f,
                   "Boxy log-trend metric misses Boxy@500 on steep tilt.");
            expect(boxyUnderRes.trendExcessDb >= 3.0f,
                   "Boxy log-trend metric is masked by nearby Res@800.");
            expect(boomCleanSteep.trendExcessDb < 3.0f,
                   "Boom log-trend metric still false-positives on CleanSteep.");
            expect(boomCleanBass.trendExcessDb < 3.0f,
                   "Boom log-trend metric still false-positives on clean bass-heavy tilt.");
            expect(boomTrue.trendExcessDb >= 3.0f,
                   "Boom log-trend metric misses Boom@60 on steep tilt.");
        }

        beginTest("HF steep-tilt path across sensitivities (candidate gate vs later gates)");

        for (float targetHz : { 6000.0f, 12000.0f })
        {
            auto steepRes = makeFlat();
            applySteepTilt(steepRes);
            addPeakDb(steepRes, targetHz, 20.0f, 0.02f);

            for (float sens : { 0.20f, 0.50f, 0.80f })
            {
                const auto frames = runHeuristicStimulus(
                    steepRes,
                    sens,
                    juce::String::formatted("HFSteep_%.0f_%.1f", targetHz, sens),
                    { targetHz });
                const auto& last = frames.back();
                const auto* probe = findNearestByRatio(last.dbg.probes, targetHz, 0.12f,
                                                       [](const AIEngine::ResonanceDebugProbe& p) { return p.targetFrequency; });
                const auto* raw = findNearestByRatio(last.dbg.rawCandidates, targetHz, 0.12f,
                                                     [](const AIEngine::ResonanceDebugCandidate& c) { return c.frequency; });
                const auto* eval = findNearestByRatio(last.dbg.evaluatedPeaks, targetHz, 0.12f,
                                                      [](const AIEngine::ResonanceDebugPeakEval& e) { return e.frequency; });
                const auto* corr = findNearestByRatio(last.corrections, targetHz, 0.12f,
                                                      [](const AIEngine::Correction& c) { return c.frequency; });

                logMessage("HF steep path target=" + juce::String(targetHz, 0)
                           + "Hz sens=" + juce::String(sens, 2)
                           + " effective=" + juce::String(last.dbg.effectiveThreshold, 3));
                logMessage("  probe: " + formatProbe(probe));
                logMessage("  raw  : " + formatRaw(raw));
                logMessage("  eval : " + formatEval(eval));
                logMessage("  corr : " + formatCorrection(corr));
            }
        }

        beginTest("Offline salience candidates: true 12k resonance vs clean 14.3k ripple");

        for (const auto& bg : { std::pair<const char*, int>{ "flat", 0 },
                                std::pair<const char*, int>{ "pink", 1 },
                                std::pair<const char*, int>{ "steep", 2 } })
        {
            auto true12k = makeFlat();
            auto clean14k = makeFlat();
            if (bg.second == 1)
            {
                applyPinkTilt(true12k);
                applyPinkTilt(clean14k);
            }
            else if (bg.second == 2)
            {
                applySteepTilt(true12k);
                applySteepTilt(clean14k);
            }

            addPeakDb(true12k, 12000.0f, 20.0f, 0.02f);

            const juce::String seed = juce::String("OfflineSalience_") + bg.first;
            const auto trueMetric = runOfflineMetric(true12k, 12000.0f, seed);
            const auto rippleMetric = runOfflineMetric(clean14k, 14373.0f, seed);
            const float margin = trueMetric.bandExcessDb - rippleMetric.bandExcessDb;
            const bool alive = margin >= 1.5f;

            logMessage("offline bg=" + juce::String(bg.first)
                       + " true12=[" + formatOffline(trueMetric) + "]"
                       + " ripple14=[" + formatOffline(rippleMetric) + "]"
                       + " bandMargin=" + juce::String(margin, 2)
                       + " alive=" + juce::String(alive ? "yes" : "no"));
        }

        beginTest("Offline salience candidates: narrow-Q HF resonances vs clean 14.3k ripple");

        for (const auto& bg : { std::pair<const char*, int>{ "flat", 0 },
                                std::pair<const char*, int>{ "pink", 1 },
                                std::pair<const char*, int>{ "steep", 2 } })
        {
            auto clean14k = makeFlat();
            if (bg.second == 1)
                applyPinkTilt(clean14k);
            else if (bg.second == 2)
                applySteepTilt(clean14k);

            const juce::String seed = juce::String("OfflineNarrow_") + bg.first;
            const auto rippleMetric = runOfflineMetric(clean14k, 14373.0f, seed);

            for (float targetHz : { 6000.0f, 12000.0f })
            {
                for (float fwhmHz : { 40.0f, 80.0f })
                {
                    for (float peakDb : { 20.0f, 6.0f })
                    {
                        auto narrow = makeFlat();
                        if (bg.second == 1)
                            applyPinkTilt(narrow);
                        else if (bg.second == 2)
                            applySteepTilt(narrow);

                        const float sigmaHz = fwhmHz / 2.355f;
                        addPeakDbWithSigmaHz(narrow, targetHz, peakDb, sigmaHz);

                        const auto metric = runOfflineMetric(
                            narrow,
                            targetHz,
                            juce::String::formatted("OfflineNarrow_%.0f_%.0f_%.0f_%s", targetHz, fwhmHz, peakDb, bg.first));
                        const float bandMargin = metric.bandExcessDb - rippleMetric.bandExcessDb;
                        const float peakMargin = metric.peakProminenceDb - rippleMetric.peakProminenceDb;
                        const bool bandAlive = bandMargin >= 1.5f;
                        const bool compositeAlive = compositeOfflinePass(metric) && !compositeOfflinePass(rippleMetric);

                        logMessage("narrow bg=" + juce::String(bg.first)
                                   + " target=" + juce::String(targetHz, 0)
                                   + "Hz fwhm=" + juce::String(fwhmHz, 0)
                                   + " peakDb=" + juce::String(peakDb, 0)
                                   + " true=[" + formatOffline(metric) + "]"
                                   + " ripple14=[" + formatOffline(rippleMetric) + "]"
                                   + " bandMargin=" + juce::String(bandMargin, 2)
                                   + " peakMargin=" + juce::String(peakMargin, 2)
                                   + " bandAlive=" + juce::String(bandAlive ? "yes" : "no")
                                   + " compositeAlive=" + juce::String(compositeAlive ? "yes" : "no"));
                    }
                }
            }
        }
    }
};

static AITicket3ResonanceInversionDiagnostic sAITicket3ResonanceInversionDiagnostic;
