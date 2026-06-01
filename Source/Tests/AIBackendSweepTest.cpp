/**
 * AIBackendSweepTest.cpp
 *
 * EMPIRICAL VERIFICATION of the AI detection subsystem (Marco's request).
 *
 * Goal: instead of *deducing* detection behaviour from the source, MEASURE it.
 * We inject three synthetic spectra with a single, known problem each:
 *
 *     - Resonance  @  800 Hz   (narrow Gaussian peak)
 *     - Sibilance  @ 7000 Hz   (HF excess)
 *     - Muddiness  @  250 Hz   (low-mid buildup)
 *     - Clean                  (flat + noise, no problem — false-positive control)
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
        beginTest("Detection matrix: 4 stimuli x 3 backends x 3 sensitivities");

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
    }
};

static AIBackendSweepTest sAIBackendSweepTest;
