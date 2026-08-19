#if JUCE_UNIT_TESTS

/*
 * PFE-LF1 - the response of the front end must not depend on how the caller
 * chops the same audio into blocks.
 *
 * This is a regression test for a defect found by the T5.5.1b evidence probe:
 * every real source measured 0.0 dB of temporal variation below 200 Hz, while
 * the same regions computed from the unfused bandDb varied by 19-69 dB. A kick
 * drum whose sub band never moves is not a measurement, it is a constant.
 *
 * Cause: pushMono() fed the whole incoming block to the 8192-point LF path
 * before producing any main frames. Given a large block, the LF path therefore
 * ran to the END of that block first, leaving lfBandDb holding the final LF
 * spectrum, which every main frame from the same call then fused. With the
 * whole file in one call, all bands below kLfCutoverHz became the file's last
 * LF frame repeated.
 *
 * This is NOT only an offline concern. Any host buffer larger than kHopSize
 * hits the same path, and offline bounce commonly uses 4096 or more. The
 * property worth holding is the general one, so the test asserts it directly:
 * one-shot and every block size must agree.
 *
 * Exact equality is not required for the fused LF bands - the header documents
 * that they may lead or lag by up to one LF hop - but the aggregate statistics
 * must match, and the LF bands must actually MOVE.
 */

#include <juce_core/juce_core.h>
#include "../AI/PerceptualFrontEnd.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
struct Summary
{
    int frames = 0;
    float subSpreadDb = 0.0f;    // p90 - p10 of the 20-60 Hz mean, over frames
    float bassSpreadDb = 0.0f;   // ditto 60-160 Hz
    float subMeanDb = 0.0f;
    float bassMeanDb = 0.0f;
    float wideMeanDb = 0.0f;     // 500-2000 Hz control: never touched by fusion
};

float pct(std::vector<float> v, float p)
{
    if (v.empty()) return 0.0f;
    const auto i = static_cast<std::size_t>(
        std::floor(std::clamp(p, 0.0f, 1.0f) * static_cast<double>(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(i), v.end());
    return v[i];
}

/** A kick-like signal: a decaying low sine burst every 0.5 s, so the sub band
    genuinely swings tens of dB. Synthetic on purpose - the witness has to be a
    signal whose LF behaviour is known by construction, not by recording. */
std::vector<float> kickLike(double sr, double seconds)
{
    const int n = static_cast<int>(sr * seconds);
    std::vector<float> x(static_cast<std::size_t>(n), 0.0f);
    const int period = static_cast<int>(sr * 0.5);
    for (int i = 0; i < n; ++i)
    {
        const int t = i % period;
        const double env = std::exp(-static_cast<double>(t) / (sr * 0.06));
        const double f = 55.0 * std::exp(-static_cast<double>(t) / (sr * 0.03)) + 42.0;
        x[static_cast<std::size_t>(i)] =
            static_cast<float>(0.7 * env * std::sin(2.0 * M_PI * f * t / sr));
    }
    return x;
}

Summary run(const std::vector<float>& x, double sr, int blockSize)
{
    PerceptualFrontEnd fe;
    fe.prepare(sr);
    std::vector<PerceptualFrontEnd::Frame> frames;
    const int n = static_cast<int>(x.size());
    const int step = blockSize <= 0 ? n : blockSize;
    for (int off = 0; off < n; off += step)
        fe.pushMono(x.data() + off, std::min(step, n - off),
                    [&frames](const PerceptualFrontEnd::Frame& f) { frames.push_back(f); });

    const std::size_t nb = static_cast<std::size_t>(fe.numBands());
    std::vector<float> centers(nb);
    for (std::size_t b = 0; b < nb; ++b) centers[b] = fe.bandCenterHz(static_cast<int>(b));

    auto series = [&](float lo, float hi)
    {
        std::vector<float> out;
        for (const auto& f : frames)
        {
            if (f.bandDbFused.size() != nb) continue;
            double s = 0.0; int c = 0;
            for (std::size_t b = 0; b < nb; ++b)
                if (centers[b] >= lo && centers[b] < hi) { s += f.bandDbFused[b]; ++c; }
            if (c > 0) out.push_back(static_cast<float>(s / c));
        }
        return out;
    };

    Summary sm;
    sm.frames = static_cast<int>(frames.size());
    const auto sub = series(20.0f, 60.0f);
    const auto bass = series(60.0f, 160.0f);
    const auto wide = series(500.0f, 2000.0f);
    sm.subSpreadDb = sub.empty() ? 0.0f : pct(sub, 0.90f) - pct(sub, 0.10f);
    sm.bassSpreadDb = bass.empty() ? 0.0f : pct(bass, 0.90f) - pct(bass, 0.10f);
    auto mean = [](const std::vector<float>& v)
    {
        if (v.empty()) return 0.0f;
        double s = 0.0; for (float q : v) s += q;
        return static_cast<float>(s / static_cast<double>(v.size()));
    };
    sm.subMeanDb = mean(sub); sm.bassMeanDb = mean(bass); sm.wideMeanDb = mean(wide);
    return sm;
}
} // namespace

class PerceptualFrontEndBlockInvarianceTest final : public juce::UnitTest
{
public:
    PerceptualFrontEndBlockInvarianceTest()
        : juce::UnitTest("PFE block-partition invariance (LF fusion)", "Integration") {}

    void runTest() override
    {
        constexpr double kSr = 48000.0;
        const auto signal = kickLike(kSr, 6.0);

        beginTest("LF bands must vary over time, whatever the block size");
        {
            const int sizes[] = { 0, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
            std::vector<Summary> results;
            for (int bs : sizes)
            {
                const auto s = run(signal, kSr, bs);
                results.push_back(s);
                logMessage("    block=" + (bs == 0 ? juce::String("one-shot")
                                                   : juce::String(bs)).paddedLeft(' ', 8)
                           + "  frames=" + juce::String(s.frames).paddedLeft(' ', 4)
                           + "  subSpread=" + juce::String(s.subSpreadDb, 1).paddedLeft(' ', 6) + "dB"
                           + "  bassSpread=" + juce::String(s.bassSpreadDb, 1).paddedLeft(' ', 6) + "dB"
                           + "  subMean=" + juce::String(s.subMeanDb, 1).paddedLeft(' ', 7) + "dB");
            }

            // The witness. A kick's sub band swings by design; if it does not,
            // the fused value is being held rather than measured.
            for (std::size_t i = 0; i < results.size(); ++i)
            {
                expect(results[i].subSpreadDb > 6.0f,
                       "sub band is effectively constant at block size index "
                       + juce::String((int) i) + " (spread "
                       + juce::String(results[i].subSpreadDb, 2) + " dB) - LF fusion is "
                       "holding a stale frame instead of tracking the signal");
                expect(results[i].bassSpreadDb > 6.0f,
                       "bass band is effectively constant at block size index "
                       + juce::String((int) i));
            }
        }

        beginTest("One-shot and block-fed analysis agree");
        {
            const auto reference = run(signal, kSr, 512);
            const int sizes[] = { 0, 64, 128, 256, 1024, 2048, 4096, 8192 };
            for (int bs : sizes)
            {
                const auto s = run(signal, kSr, bs);
                const juce::String tag = bs == 0 ? "one-shot" : juce::String(bs);

                expectEquals(s.frames, reference.frames,
                             "frame count depends on block size (" + tag + ")");

                // The 500-2000 Hz control never goes through LF fusion, so any
                // disagreement there would mean the main path had become
                // block-dependent too - a different and worse defect.
                expect(std::abs(s.wideMeanDb - reference.wideMeanDb) < 0.05f,
                       "main-path (non-fused) mean differs at block " + tag + ": "
                       + juce::String(s.wideMeanDb, 3) + " vs "
                       + juce::String(reference.wideMeanDb, 3));

                // Fused LF may lead or lag by up to one LF hop, so this compares
                // aggregates rather than demanding sample-accurate identity.
                expect(std::abs(s.subMeanDb - reference.subMeanDb) < 1.5f,
                       "fused sub mean differs at block " + tag + ": "
                       + juce::String(s.subMeanDb, 2) + " vs "
                       + juce::String(reference.subMeanDb, 2) + " dB");
                expect(std::abs(s.bassMeanDb - reference.bassMeanDb) < 1.5f,
                       "fused bass mean differs at block " + tag + ": "
                       + juce::String(s.bassMeanDb, 2) + " vs "
                       + juce::String(reference.bassMeanDb, 2) + " dB");
                expect(std::abs(s.subSpreadDb - reference.subSpreadDb) < 3.0f,
                       "fused sub spread differs at block " + tag + ": "
                       + juce::String(s.subSpreadDb, 2) + " vs "
                       + juce::String(reference.subSpreadDb, 2) + " dB");
            }
        }
    }
};

static PerceptualFrontEndBlockInvarianceTest sPerceptualFrontEndBlockInvarianceTest;

#endif
