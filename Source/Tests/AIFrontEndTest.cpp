/**
 * AIFrontEndTest — Roadmap v1, P2 Commit 1 (category "AI-Front").
 *
 * Diagnostics-only witnesses for the AI-owned PerceptualFrontEnd. The module is
 * NOT wired into production yet; these tests pin its math before any wiring:
 *   1. first-frame equivalence with the legacy mirror (rising attack ~= raw);
 *   2. rawness: later frames legitimately DIFFER from the legacy smoothed path
 *      (no release ballistics) — this difference is the point of P2;
 *   3. band mapping sanity: a sine lands in the correct 12/octave band;
 *   4. CPU witness: mean/max ns per frame, logged + bounded (scorecard row).
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../AI/PerceptualFrontEnd.h"
#include "Support/OfflineAnalysisPipeline.h"

namespace
{
constexpr double kSr = 48000.0;

std::vector<float> makeTestSignal(int numSamples, juce::int64 seed)
{
    juce::Random rng(seed);
    std::vector<float> s(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
    {
        const double t = i / kSr;
        s[static_cast<size_t>(i)] =
              0.25f * std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t)
            + 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
    }
    return s;
}
} // namespace

class AIFrontEndTest : public juce::UnitTest
{
public:
    AIFrontEndTest() : juce::UnitTest("AI Front-End — raw frames & band map witnesses", "AI-Front") {}

    void runTest() override
    {
        const int n = static_cast<int>(kSr * 3.0);
        const auto mono = makeTestSignal(n, 777);

        // Mirror (legacy path, WITH ballistics) on the same mono signal.
        juce::AudioBuffer<float> buf(1, n);
        std::copy(mono.begin(), mono.end(), buf.getWritePointer(0));
        aieq_test::OfflineAnalysisPipeline legacy(kSr);
        const auto legacyFrames = legacy.analyze(buf);

        PerceptualFrontEnd fe;
        fe.prepare(kSr);
        const auto rawFrames = fe.analyzeAll(mono.data(), n);

        // ----------------------------------------------------------------
        beginTest("Frame cadence matches the legacy path");
        expectEquals(static_cast<int>(rawFrames.size()), static_cast<int>(legacyFrames.size()),
                     "Front-end hop framing diverges from the legacy path.");
        expect(!rawFrames.empty(), "No frames produced.");
        if (rawFrames.empty() || legacyFrames.empty())
            return;

        // ----------------------------------------------------------------
        beginTest("First-frame equivalence with the legacy mirror (attack ~= raw)");
        // On the FIRST frame every bin rises from -120 with attackCoeff ~= 1,
        // so legacy(smoothed) ~= raw to within ~1e-6 dB. This pins the FFT,
        // window, normalization and clamp as identical.
        float maxDiff0 = 0.0f;
        const size_t bins0 = std::min(rawFrames[0].rawDb.size(), legacyFrames[0].size());
        for (size_t i = 0; i < bins0; ++i)
            maxDiff0 = std::max(maxDiff0,
                                std::abs(rawFrames[0].rawDb[i] - legacyFrames[0][i]));
        logMessage("  first-frame max |raw - legacy| = " + juce::String(maxDiff0, 6) + " dB");
        expect(maxDiff0 <= 0.01f, "First frame deviates from the legacy mirror.");

        // ----------------------------------------------------------------
        beginTest("Rawness: later frames differ from the smoothed legacy path");
        // With release ballistics the legacy path lags on decaying bins; the raw
        // front-end must NOT exhibit that lag. A measurable difference on noisy
        // material is the witness that the ballistics are really gone.
        float maxDiffLater = 0.0f;
        for (size_t f = 4; f < rawFrames.size(); ++f)
        {
            const size_t bins = std::min(rawFrames[f].rawDb.size(), legacyFrames[f].size());
            for (size_t i = 0; i < bins; ++i)
                maxDiffLater = std::max(maxDiffLater,
                                        std::abs(rawFrames[f].rawDb[i] - legacyFrames[f][i]));
        }
        logMessage("  later-frames max |raw - legacy| = " + juce::String(maxDiffLater, 3) + " dB");
        expect(maxDiffLater > 0.5f,
               "Raw frames are identical to the smoothed path - ballistics still present?");

        // ----------------------------------------------------------------
        beginTest("Band map sanity: 1 kHz sine lands in the correct 12/oct band");
        const auto& lastFrame = rawFrames.back();
        int argmax = 0;
        for (int b = 1; b < fe.numBands(); ++b)
            if (lastFrame.bandDb[static_cast<size_t>(b)] > lastFrame.bandDb[static_cast<size_t>(argmax)])
                argmax = b;
        const float centerHz = fe.bandCenterHz(argmax);
        const float octOff = std::abs(std::log2(centerHz / 1000.0f));
        logMessage("  bands=" + juce::String(fe.numBands())
                   + "  argmax band center = " + juce::String(centerHz, 1) + " Hz ("
                   + juce::String(octOff, 3) + " oct from 1 kHz)");
        expect(octOff <= 1.0f / 12.0f + 1.0e-3f,
               "1 kHz sine did not land in the nearest 12/oct band.");

        // ----------------------------------------------------------------
        beginTest("CPU witness: per-frame cost within budget");
        // Fresh run over 10 s so the counters cover a realistic stretch.
        PerceptualFrontEnd fe10;
        fe10.prepare(kSr);
        const int n10 = static_cast<int>(kSr * 10.0);
        const auto mono10 = makeTestSignal(n10, 778);
        fe10.pushMono(mono10.data(), n10, nullptr);
        const double meanMs = fe10.meanFrameNs() / 1.0e6;
        const double maxMs  = static_cast<double>(fe10.maxFrameNs()) / 1.0e6;
        logMessage("  frames=" + juce::String(static_cast<int>(fe10.framesProcessed()))
                   + "  mean=" + juce::String(meanMs, 3) + " ms/frame"
                   + "  max=" + juce::String(maxMs, 3) + " ms/frame");
        expect(fe10.framesProcessed() > 200, "Too few frames for a meaningful CPU witness.");
        expect(meanMs < 5.0, "Front-end mean frame cost exceeds the 5 ms budget.");
    }
};

static AIFrontEndTest sAIFrontEndTest;
