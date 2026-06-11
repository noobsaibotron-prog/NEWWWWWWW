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
        beginTest("P2C3 headline: 45 Hz and 60 Hz resonances separable in the FUSED bands");
        // At 11.7 Hz/bin (4096) the two sines land on near-adjacent bins and smear;
        // the 8192 LF path (5.86 Hz/bin) must resolve them as two distinct band
        // maxima with a dip between. This was the impossible-today witness.
        {
            const int n2 = static_cast<int>(kSr * 4.0);
            juce::Random rng2(555);
            std::vector<float> lfSig(static_cast<size_t>(n2));
            for (int i = 0; i < n2; ++i)
            {
                const double t = i / kSr;
                lfSig[static_cast<size_t>(i)] =
                      0.30f * std::sin(2.0 * juce::MathConstants<double>::pi * 45.0 * t)
                    + 0.30f * std::sin(2.0 * juce::MathConstants<double>::pi * 60.0 * t)
                    + 0.01f * (rng2.nextFloat() * 2.0f - 1.0f);
            }
            PerceptualFrontEnd feLf;
            feLf.prepare(kSr);
            const auto fr = feLf.analyzeAll(lfSig.data(), n2);
            expect(!fr.empty() && fr.back().lfValid, "No LF-fused frame produced.");
            const auto& f = fr.back();

            // Inspect bands around 35..80 Hz.
            int b45 = -1, b60 = -1;
            for (int b = 0; b < feLf.numBands(); ++b)
            {
                if (std::abs(std::log2(feLf.bandCenterHz(b) / 45.0f)) < 1.0f / 24.0f) b45 = b;
                if (std::abs(std::log2(feLf.bandCenterHz(b) / 60.0f)) < 1.0f / 24.0f) b60 = b;
            }
            expect(b45 >= 0 && b60 >= 0 && b60 > b45 + 1, "Band grid misses 45/60 Hz centers.");
            juce::String prof4096, profFused;
            for (int b = juce::jmax(0, b45 - 3); b <= b60 + 3 && b < feLf.numBands(); ++b)
            {
                prof4096  += juce::String(f.bandDb[static_cast<size_t>(b)], 1) + " ";
                profFused += juce::String(f.bandDbFused[static_cast<size_t>(b)], 1) + " ";
            }
            logMessage("  bands " + juce::String(b45 - 3) + ".." + juce::String(b60 + 3)
                       + "  4096: " + prof4096);
            logMessage("  bands " + juce::String(b45 - 3) + ".." + juce::String(b60 + 3)
                       + "  fused: " + profFused);

            // Witness: both peaks present and a genuine dip between them (fused).
            float dipMin = 1.0e9f;
            for (int b = b45 + 1; b < b60; ++b)
                dipMin = std::min(dipMin, f.bandDbFused[static_cast<size_t>(b)]);
            const float peak45 = f.bandDbFused[static_cast<size_t>(b45)];
            const float peak60 = f.bandDbFused[static_cast<size_t>(b60)];
            const float sep = std::min(peak45, peak60) - dipMin;
            logMessage("  fused separation (min peak - dip) = " + juce::String(sep, 2) + " dB");
            expect(sep >= 3.0f,
                   "45/60 Hz not separable in the fused LF representation (sep="
                   + juce::String(sep, 2) + " dB).");
        }

        // ----------------------------------------------------------------
        beginTest("P2C3: equal-loudness salience weighting sanity");
        {
            PerceptualFrontEnd feW;
            feW.prepare(kSr);
            int b1k = 0, b50 = 0, b3k = 0;
            for (int b = 0; b < feW.numBands(); ++b)
            {
                if (std::abs(std::log2(feW.bandCenterHz(b) / 1000.0f)) <
                    std::abs(std::log2(feW.bandCenterHz(b1k) / 1000.0f))) b1k = b;
                if (std::abs(std::log2(feW.bandCenterHz(b) / 50.0f)) <
                    std::abs(std::log2(feW.bandCenterHz(b50) / 50.0f)))  b50 = b;
                if (std::abs(std::log2(feW.bandCenterHz(b) / 3000.0f)) <
                    std::abs(std::log2(feW.bandCenterHz(b3k) / 3000.0f))) b3k = b;
            }
            logMessage("  weight(50 Hz)=" + juce::String(feW.loudnessWeightDb(b50), 1)
                       + "  weight(1 kHz)=" + juce::String(feW.loudnessWeightDb(b1k), 1)
                       + "  weight(3 kHz)=" + juce::String(feW.loudnessWeightDb(b3k), 1));
            expect(std::abs(feW.loudnessWeightDb(b1k)) <= 0.5f, "1 kHz weight should be ~0 dB.");
            expect(feW.loudnessWeightDb(b50) < feW.loudnessWeightDb(b1k) - 5.0f,
                   "50 Hz should be strongly de-weighted vs 1 kHz.");
            expect(feW.loudnessWeightDb(b3k) > feW.loudnessWeightDb(b1k),
                   "3 kHz should be slightly up-weighted vs 1 kHz (ear sensitivity).");
        }

        // ----------------------------------------------------------------
        beginTest("P2C3: spectral flux spikes at a transient and stays low on steady tone");
        {
            const int n3 = static_cast<int>(kSr * 3.0);
            juce::Random rng3(666);
            std::vector<float> sig3(static_cast<size_t>(n3));
            const int burstStart = static_cast<int>(kSr * 2.0); // t = 2 s
            const int burstLen = 2048;
            for (int i = 0; i < n3; ++i)
            {
                const double t = i / kSr;
                float s = 0.25f * static_cast<float>(
                    std::sin(2.0 * juce::MathConstants<double>::pi * 1000.0 * t));
                if (i >= burstStart && i < burstStart + burstLen)
                    s += 0.5f * (rng3.nextFloat() * 2.0f - 1.0f);
                sig3[static_cast<size_t>(i)] = s;
            }
            PerceptualFrontEnd feF;
            feF.prepare(kSr);
            const auto fr3 = feF.analyzeAll(sig3.data(), n3);
            expect(static_cast<int>(fr3.size()) > 50, "Too few frames for the flux witness.");

            int argmax = 1; // skip frame 0 (no flux reference)
            for (int f2 = 2; f2 < static_cast<int>(fr3.size()); ++f2)
                if (fr3[static_cast<size_t>(f2)].fluxDb > fr3[static_cast<size_t>(argmax)].fluxDb)
                    argmax = f2;
            // Expected frame index containing the burst start.
            const int expectedFrame = (burstStart - 4096) / 2048 + 1;
            // Median flux on clearly-steady frames (before the burst region).
            std::vector<float> steady;
            for (int f2 = 2; f2 < expectedFrame - 2; ++f2)
                steady.push_back(fr3[static_cast<size_t>(f2)].fluxDb);
            std::sort(steady.begin(), steady.end());
            const float steadyMedian = steady[steady.size() / 2];
            logMessage("  flux argmax frame=" + juce::String(argmax)
                       + " (expected ~" + juce::String(expectedFrame) + ")"
                       + "  peak=" + juce::String(fr3[static_cast<size_t>(argmax)].fluxDb, 2)
                       + " dB  steady median=" + juce::String(steadyMedian, 3) + " dB");
            expect(std::abs(argmax - expectedFrame) <= 1,
                   "Flux peak not at the transient frame.");
            expect(fr3[static_cast<size_t>(argmax)].fluxDb > steadyMedian * 5.0f + 1.0f,
                   "Flux peak not clearly above the steady floor.");
        }

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
