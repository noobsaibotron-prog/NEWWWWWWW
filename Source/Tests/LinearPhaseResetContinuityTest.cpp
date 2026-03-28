/**
 * LinearPhaseResetContinuityTest.cpp
 *
 * Characterization tests for LinearPhaseProcessor::reset() discontinuity.
 *
 * Known bug: reset() clears overlap buffers and aborts in-progress crossfade
 * in PartitionedConvolver, which can cause audible clicks when called during
 * active convolution or mid-IR-swap. This is suspected to be a contributing
 * factor to the phase transition click (maxDelta ~0.7030).
 *
 * Tests 1-2 are CHARACTERIZATION TESTS — they are expected to FAIL until
 * the reset discontinuity is fixed. They document the bug, not a regression.
 * Test 3-4 verify basic safety (no crash, no NaN).
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <random>

#include "../DSP/LinearPhaseProcessor.h"

namespace
{

// ── Click detection (self-contained copy — HostSessionClickTest.cpp is off-limits) ──

struct ClickMetrics
{
    float maxDelta         = 0.0f;
    float peakAbs          = 0.0f;
    int   maxDropoutRun    = 0;
    bool  hasNaN           = false;
    bool  hasInf           = false;
    int   clickCount       = 0;
    float avgDelta         = 0.0f;
};

// TODO: Extract analyzeForClicks to a shared TestUtils.h when HostSessionClickTest.cpp
// is available for modification.

constexpr float kClickDeltaThreshold   = 0.35f;
constexpr float kMaxDeltaPass          = 0.50f;
constexpr float kPeakAbsMax            = 3.0f;
constexpr int   kMaxDropoutSamples     = 8;

static ClickMetrics analyzeForClicks(const juce::AudioBuffer<float>& output,
                                      const juce::AudioBuffer<float>& input,
                                      int regionStart, int regionLength,
                                      float clickThreshold = kClickDeltaThreshold)
{
    ClickMetrics m;
    if (output.getNumChannels() == 0 || regionLength <= 0) return m;

    const int end = juce::jmin(output.getNumSamples(), regionStart + regionLength);
    long totalSamples = 0;
    double sumDelta = 0.0;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        const auto* out = output.getReadPointer(ch);
        const auto* in  = (ch < input.getNumChannels()) ? input.getReadPointer(ch) : nullptr;

        float prev = out[juce::jmax(0, regionStart)];
        int dropoutRun = 0;

        for (int i = regionStart; i < end; ++i)
        {
            const float s = out[i];

            if (std::isnan(s)) m.hasNaN = true;
            if (std::isinf(s)) m.hasInf = true;

            m.peakAbs = std::max(m.peakAbs, std::abs(s));

            if (i > regionStart)
            {
                const float delta = std::abs(s - prev);
                m.maxDelta = std::max(m.maxDelta, delta);
                sumDelta += delta;
                ++totalSamples;

                if (delta > clickThreshold)
                    ++m.clickCount;
            }
            prev = s;

            if (in != nullptr && std::abs(in[i]) > 0.01f && std::abs(s) < 1.0e-5f)
                ++dropoutRun;
            else
                dropoutRun = 0;

            m.maxDropoutRun = std::max(m.maxDropoutRun, dropoutRun);
        }
    }

    if (totalSamples > 0)
        m.avgDelta = static_cast<float>(sumDelta / totalSamples);

    return m;
}

// ── Helpers ──

static constexpr double kSampleRate = 48000.0;
static constexpr int    kBlockSize  = 128;
static constexpr int    kNumCh      = 2;

/** Dirac delta at centre of IR → flat magnitude, zero phase. */
static std::vector<float> makeUnityIR()
{
    std::vector<float> ir(LinearPhaseProcessor::irSize, 0.0f);
    ir[LinearPhaseProcessor::irSize / 2] = 1.0f;
    return ir;
}

/** Windowed sinc low-pass IR, fc ~ 4kHz. Materially different from unity. */
static std::vector<float> makeLowPassIR()
{
    const size_t N = LinearPhaseProcessor::irSize;
    std::vector<float> ir(N, 0.0f);
    const double fc = 4000.0 / kSampleRate;
    const int M = static_cast<int>(N);
    const int centre = M / 2;
    for (int n = 0; n < M; ++n)
    {
        double x = n - centre;
        double h = (x == 0.0) ? 2.0 * fc
                               : std::sin(2.0 * juce::MathConstants<double>::pi * fc * x)
                                 / (juce::MathConstants<double>::pi * x);
        double w = 0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * n / (M - 1)));
        ir[static_cast<size_t>(n)] = static_cast<float>(h * w);
    }
    return ir;
}

/** Fill buffer with broadband test signal. */
static void fillBroadband(juce::AudioBuffer<float>& buf, int sampleOffset, std::mt19937& rng)
{
    const double freqs[] = { 100.0, 440.0, 1000.0, 3000.0, 8000.0 };
    const float  amps[]  = { 0.08f, 0.06f, 0.05f,  0.04f,  0.03f  };
    std::uniform_real_distribution<float> noiseDist(-0.05f, 0.05f);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float sample = noiseDist(rng);
            for (int t = 0; t < 5; ++t)
            {
                const double w = juce::MathConstants<double>::twoPi * freqs[t] / kSampleRate;
                sample += amps[t] * static_cast<float>(std::sin(w * static_cast<double>(sampleOffset + i)));
            }
            data[i] = sample;
        }
    }
}

using BlockCallback = std::function<void(LinearPhaseProcessor&, int)>;

struct ProcessResult
{
    juce::AudioBuffer<float> output;
    juce::AudioBuffer<float> input;
};

/** Feed blocks through LinearPhaseProcessor with per-block callback. */
static ProcessResult runSession(int numBlocks, const std::vector<float>& initialIR, BlockCallback onBlock)
{
    LinearPhaseProcessor lpp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = kSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(kBlockSize);
    spec.numChannels      = kNumCh;
    lpp.prepare(spec);
    lpp.loadImpulseResponse(initialIR, kSampleRate);

    // Warm up — let the convolver fill its internal buffers
    {
        juce::AudioBuffer<float> warmup(kNumCh, kBlockSize);
        std::mt19937 warmRng(99);
        for (int b = 0; b < 40; ++b)
        {
            fillBroadband(warmup, b * kBlockSize, warmRng);
            juce::dsp::AudioBlock<float> ab(warmup);
            juce::dsp::ProcessContextReplacing<float> ctx(ab);
            lpp.process(ctx);
        }
    }

    ProcessResult result;
    const int totalSamples = numBlocks * kBlockSize;
    result.output = juce::AudioBuffer<float>(kNumCh, totalSamples);
    result.input  = juce::AudioBuffer<float>(kNumCh, totalSamples);

    std::mt19937 rng(42);

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioBuffer<float> chunk(kNumCh, kBlockSize);
        fillBroadband(chunk, block * kBlockSize, rng);

        for (int ch = 0; ch < kNumCh; ++ch)
            result.input.copyFrom(ch, block * kBlockSize, chunk, ch, 0, kBlockSize);

        if (onBlock) onBlock(lpp, block);

        juce::dsp::AudioBlock<float> ab(chunk);
        juce::dsp::ProcessContextReplacing<float> ctx(ab);
        lpp.process(ctx);

        for (int ch = 0; ch < kNumCh; ++ch)
            result.output.copyFrom(ch, block * kBlockSize, chunk, ch, 0, kBlockSize);
    }

    return result;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
class LinearPhaseResetContinuityTest : public juce::UnitTest
{
public:
    LinearPhaseResetContinuityTest()
        : juce::UnitTest("LinearPhase Reset Continuity", "DSP") {}

    void runTest() override
    {
        auto unityIR = makeUnityIR();
        auto lpIR    = makeLowPassIR();

        // ── Test 1: CHARACTERIZATION — Reset during active convolution ───
        // Known bug: reset() clears overlap tail → click.
        // This test DOCUMENTS the bug. Expected to FAIL until fixed.
        beginTest("[CHARACTERIZATION] Reset mid-stream with active convolution");
        {
            constexpr int numBlocks = 200;
            constexpr int resetBlock = 100;

            auto result = runSession(numBlocks, unityIR, [](LinearPhaseProcessor& lpp, int block) {
                if (block == resetBlock)
                    lpp.reset();
            });

            const int resetSample = resetBlock * kBlockSize;
            const int analyzeStart = juce::jmax(0, resetSample - kBlockSize);
            const int analyzeLen   = kBlockSize * 6;

            auto m = analyzeForClicks(result.output, result.input, analyzeStart, analyzeLen);
            logMessage("  Reset mid-stream: maxDelta=" + juce::String(m.maxDelta, 4)
                       + " clicks=" + juce::String(m.clickCount)
                       + " peakAbs=" + juce::String(m.peakAbs, 4)
                       + " dropout=" + juce::String(m.maxDropoutRun));

            // Characterization: log the actual values for future comparison
            expect(!m.hasNaN, "[CHAR] Reset mid-stream: NaN in output");
            expect(!m.hasInf, "[CHAR] Reset mid-stream: Inf in output");
            // NOTE: maxDelta and click assertions are characterization —
            // if these pass, the bug may have been fixed!
            expect(m.maxDelta < kMaxDeltaPass,
                   "[CHAR] Reset mid-stream: click detected, maxDelta=" + juce::String(m.maxDelta, 4)
                   + " (known bug — reset clears overlap tail)");
        }

        // ── Test 2: CHARACTERIZATION — Reset during IR crossfade ─────────
        // Known bug: reset() zeroes crossfadeSamplesLeft → aborts fade.
        beginTest("[CHARACTERIZATION] Reset during active IR crossfade");
        {
            constexpr int numBlocks = 200;

            auto result = runSession(numBlocks, unityIR, [&lpIR](LinearPhaseProcessor& lpp, int block) {
                if (block == 80)
                    lpp.loadImpulseResponse(lpIR, kSampleRate);
                if (block == 81)
                    lpp.reset();
            });

            const int resetSample = 81 * kBlockSize;
            const int analyzeStart = juce::jmax(0, resetSample - kBlockSize);
            const int analyzeLen   = kBlockSize * 6;

            auto m = analyzeForClicks(result.output, result.input, analyzeStart, analyzeLen);
            logMessage("  Reset during crossfade: maxDelta=" + juce::String(m.maxDelta, 4)
                       + " clicks=" + juce::String(m.clickCount)
                       + " peakAbs=" + juce::String(m.peakAbs, 4)
                       + " dropout=" + juce::String(m.maxDropoutRun));

            expect(!m.hasNaN, "[CHAR] Reset during crossfade: NaN in output");
            expect(!m.hasInf, "[CHAR] Reset during crossfade: Inf in output");
            expect(m.maxDelta < kMaxDeltaPass,
                   "[CHAR] Reset during crossfade: click detected, maxDelta=" + juce::String(m.maxDelta, 4)
                   + " (known bug — reset aborts crossfade)");
        }

        // ── Test 3: Reset + new IR load — basic continuity ───────────────
        beginTest("Reset then new IR load — output continuity");
        {
            constexpr int numBlocks = 200;

            auto result = runSession(numBlocks, unityIR, [&lpIR](LinearPhaseProcessor& lpp, int block) {
                if (block == 80)
                    lpp.reset();
                if (block == 82)
                    lpp.loadImpulseResponse(lpIR, kSampleRate);
            });

            const int analyzeStart = 79 * kBlockSize;
            const int analyzeLen   = kBlockSize * 10;

            auto m = analyzeForClicks(result.output, result.input, analyzeStart, analyzeLen);
            logMessage("  Reset+newIR: maxDelta=" + juce::String(m.maxDelta, 4)
                       + " clicks=" + juce::String(m.clickCount)
                       + " peakAbs=" + juce::String(m.peakAbs, 4));

            expect(!m.hasNaN, "Reset+newIR: NaN in output");
            expect(!m.hasInf, "Reset+newIR: Inf in output");
            expect(m.peakAbs < kPeakAbsMax, "Reset+newIR: output explosion peakAbs=" + juce::String(m.peakAbs, 4));
        }

        // ── Test 4: Back-to-back resets — no crash or UB ─────────────────
        beginTest("Back-to-back resets — no crash or UB");
        {
            constexpr int numBlocks = 200;

            auto result = runSession(numBlocks, unityIR, [](LinearPhaseProcessor& lpp, int block) {
                if (block >= 90 && block < 100)
                    lpp.reset();
            });

            const int analyzeStart = 89 * kBlockSize;
            const int analyzeLen   = kBlockSize * 15;

            auto m = analyzeForClicks(result.output, result.input, analyzeStart, analyzeLen);
            logMessage("  Back-to-back resets: maxDelta=" + juce::String(m.maxDelta, 4)
                       + " hasNaN=" + juce::String(m.hasNaN ? 1 : 0)
                       + " hasInf=" + juce::String(m.hasInf ? 1 : 0)
                       + " peakAbs=" + juce::String(m.peakAbs, 4));

            expect(!m.hasNaN, "Back-to-back resets: NaN in output");
            expect(!m.hasInf, "Back-to-back resets: Inf in output");
            expect(m.peakAbs < kPeakAbsMax, "Back-to-back resets: output explosion");
            // Clicks are expected here (pathological scenario) — the key is no crash/NaN/Inf
        }
    }
};

static LinearPhaseResetContinuityTest linearPhaseResetContinuityTest;
