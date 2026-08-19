#if JUCE_UNIT_TESTS

/*
 * T5.5.1b - regional evidence / noise-floor probe. Measurement only: no
 * production behaviour changes and nothing here is asserted as ground truth.
 *
 * WHY salienceDb IS NOT USED. The brief asked whether PerceptualFrontEnd's
 * salienceDb could supply signal-versus-background evidence without adding a
 * second detector. It cannot. PerceptualFrontEnd.cpp:402 computes
 *
 *     salienceDb[b] = bandDbFused[b] + bandLoudnessWeightDb[b]
 *
 * and bandLoudnessWeightDb is filled once in prepare() from the band centre
 * frequency alone. It is therefore a FIXED per-band offset - a monotone
 * transform of the level that carries exactly zero extra information about
 * whether a band contains content or room noise. It ranks bands against each
 * other by perceived loudness, which is a different question.
 *
 * WHAT THE DISCRIMINATOR HAS TO SURVIVE. Two failure modes rule out the
 * obvious candidates:
 *
 *   - "region > peak - X dB" is already falsified: a pink-tilted full mix sits
 *     tens of dB below its bass peak at 15 kHz and still has perfectly good HF.
 *
 *   - Temporal spread alone fails on sustained sources. A pad or a drone has
 *     low spread everywhere BECAUSE the content is steady, so low spread cannot
 *     mean "this is a noise carpet".
 *
 * The candidate measured here instead asks whether a band MOVES WITH THE
 * SOURCE. Room rumble under a shaker sits at a constant level while the shaker
 * pulses; the shaker's own brilliance rises and falls with every hit. A pad's
 * low end rises and falls with the pad. That distinction survives both failure
 * modes above, because it is about covariance rather than level or variance.
 *
 * Usage:
 *   AIEQ_CANDIDATE_DIR=/path/to/wavs \
 *     build-verify/Release/bin/AIEqualizerPro_IntegrationTests \
 *       --category=ManualProbe -v
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../AI/PerceptualFrontEnd.h"
#include "../AI/SpectralContext.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
using AIEQPerceptual::SpectralContext;
using AIEQPerceptual::SpectralContextAccumulator;

constexpr float kLoHz = 30.0f, kHiHz = 18000.0f;
struct RRange { float lo, hi; const char* name; };
constexpr RRange kRegions[] = {
    { 20.0f, 60.0f, "Sub" }, { 60.0f, 160.0f, "Bass" }, { 160.0f, 500.0f, "LoMid" },
    { 500.0f, 2000.0f, "Mid" }, { 2000.0f, 5000.0f, "Pres" },
    { 5000.0f, 10000.0f, "Brill" }, { 10000.0f, 24000.0f, "Air" } };

bool loadAudio(const juce::File& f, juce::AudioBuffer<float>& out, double& sr)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr) return false;
    out.setSize(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(&out, 0, static_cast<int>(r->lengthInSamples), 0, true, true);
    sr = r->sampleRate; return true;
}

float pct(std::vector<float> v, float p)
{
    if (v.empty()) return -120.0f;
    const auto i = static_cast<std::size_t>(std::floor(std::clamp(p, 0.0f, 1.0f)
                                          * static_cast<double>(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(i), v.end());
    return v[i];
}

struct RegionStats
{
    float p10 = -120.0f, p50 = -120.0f, p90 = -120.0f;
    float spread = 0.0f;          // p90 - p10
    float spreadUnfused = 0.0f;   // same from bandDb, i.e. without LF fusion
    float corrWithSource = 0.0f;  // covariance with the source's own level
    float floorMargin = 0.0f;     // p50 - p10
    float regionConfidence = 0.0f;
};

struct Measured
{
    juce::String name;
    std::vector<RegionStats> regions;
};

Measured measure(const juce::File& file)
{
    Measured m; m.name = file.getFileNameWithoutExtension();
    juce::AudioBuffer<float> a; double sr = 0.0;
    if (!loadAudio(file, a, sr)) return m;

    std::vector<float> mono(static_cast<size_t>(std::max(0, a.getNumSamples())), 0.0f);
    for (int c = 0; c < a.getNumChannels(); ++c)
    {
        const float* s = a.getReadPointer(c);
        for (int i = 0; i < a.getNumSamples(); ++i) mono[static_cast<size_t>(i)] += s[i];
    }
    if (a.getNumChannels() > 0) for (auto& v : mono) v /= static_cast<float>(a.getNumChannels());

    PerceptualFrontEnd fe; fe.prepare(sr);
    const auto frames = fe.analyzeAll(mono.data(), static_cast<int>(mono.size()));
    if (frames.empty()) return m;

    const std::size_t nb = static_cast<std::size_t>(fe.numBands());
    std::vector<float> centers(nb);
    for (std::size_t b = 0; b < nb; ++b) centers[b] = fe.bandCenterHz(static_cast<int>(b));

    SpectralContextAccumulator acc; acc.prepare(centers);
    for (const auto& f : frames) acc.pushFrame(f.bandDbFused, f.lfValid);
    const auto context = acc.snapshot();

    // Per-frame source level. The first version of this probe used the MAXIMUM
    // band, which is what the schema-4 accumulator uses to detect pauses, and
    // it was degenerate here: on a bass the maximum band sits in a region whose
    // fused LF value barely moves, so the "source level" came out constant and
    // every correlation collapsed to zero. A broadband MEAN tracks whether the
    // source is playing without being pinned by one held band.
    //
    // Both are computed so the difference is visible rather than asserted.
    std::vector<float> frameLevel, frameMax;
    frameLevel.reserve(frames.size()); frameMax.reserve(frames.size());
    for (const auto& f : frames)
    {
        float mx = -200.0f; double sum = 0.0; int cnt = 0;
        if (f.bandDbFused.size() == nb)
            for (std::size_t b = 0; b < nb; ++b)
                if (centers[b] >= kLoHz && centers[b] <= kHiHz)
                { mx = std::max(mx, f.bandDbFused[b]); sum += f.bandDbFused[b]; ++cnt; }
        frameMax.push_back(mx);
        frameLevel.push_back(cnt > 0 ? static_cast<float>(sum / cnt) : -200.0f);
    }
    // Activity still keyed off the maximum, to stay identical to production.
    float peak = -200.0f; std::vector<char> active(frames.size(), 0);
    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        peak = std::max(peak, frameMax[i]);
        active[i] = frameMax[i] >= peak - 40.0f ? 1 : 0;
    }

    std::vector<float> srcActive;
    for (std::size_t i = 0; i < frames.size(); ++i) if (active[i]) srcActive.push_back(frameLevel[i]);
    double srcMean = 0.0; for (float v : srcActive) srcMean += v;
    if (!srcActive.empty()) srcMean /= static_cast<double>(srcActive.size());
    double srcVar = 0.0; for (float v : srcActive) srcVar += (v - srcMean) * (v - srcMean);

    for (std::size_t r = 0; r < 7; ++r)
    {
        RegionStats st;
        st.regionConfidence = context.regionConfidence[r];

        // Region level per frame = mean of that region's bands.
        std::vector<float> series;
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            if (!active[i] || frames[i].bandDbFused.size() != nb) continue;
            double s = 0.0; int n = 0;
            for (std::size_t b = 0; b < nb; ++b)
                if (centers[b] >= kRegions[r].lo && centers[b] < kRegions[r].hi
                    && centers[b] >= kLoHz && centers[b] <= kHiHz)
                { s += frames[i].bandDbFused[b]; ++n; }
            if (n > 0) series.push_back(static_cast<float>(s / n));
        }
        if (series.empty()) { m.regions.push_back(st); continue; }

        {
            std::vector<float> raw;
            for (std::size_t i = 0; i < frames.size(); ++i)
            {
                if (!active[i] || frames[i].bandDb.size() != nb) continue;
                double s2 = 0.0; int n2 = 0;
                for (std::size_t b = 0; b < nb; ++b)
                    if (centers[b] >= kRegions[r].lo && centers[b] < kRegions[r].hi
                        && centers[b] >= kLoHz && centers[b] <= kHiHz)
                    { s2 += frames[i].bandDb[b]; ++n2; }
                if (n2 > 0) raw.push_back(static_cast<float>(s2 / n2));
            }
            st.spreadUnfused = raw.empty() ? 0.0f : pct(raw, 0.90f) - pct(raw, 0.10f);
        }
        st.p10 = pct(series, 0.10f); st.p50 = pct(series, 0.50f); st.p90 = pct(series, 0.90f);
        st.spread = st.p90 - st.p10;
        st.floorMargin = st.p50 - st.p10;

        double rMean = 0.0; for (float v : series) rMean += v;
        rMean /= static_cast<double>(series.size());
        double cov = 0.0, rVar = 0.0;
        const std::size_t n = std::min(series.size(), srcActive.size());
        for (std::size_t i = 0; i < n; ++i)
        {
            cov += (series[i] - rMean) * (srcActive[i] - srcMean);
            rVar += (series[i] - rMean) * (series[i] - rMean);
        }
        const double denom = std::sqrt(std::max(1.0e-12, rVar * srcVar));
        st.corrWithSource = static_cast<float>(cov / denom);
        m.regions.push_back(st);
    }
    return m;
}

juce::String d1(float v) { return juce::String(v, 1); }
juce::String d2(float v) { return juce::String(v, 2); }
juce::String padL(const juce::String& s, int w) { return s.paddedLeft(' ', w); }
} // namespace

class RegionalEvidenceProbe final : public juce::UnitTest
{
public:
    RegionalEvidenceProbe()
        : juce::UnitTest("Regional evidence / noise-floor probe (T5.5.1b)", "ManualProbe") {}

    void runTest() override
    {
        beginTest("Measure per-region temporal evidence candidates");

        const auto dirEnv = juce::SystemStats::getEnvironmentVariable("AIEQ_CANDIDATE_DIR", {});
        if (dirEnv.isEmpty()) { logMessage("  SKIP: set AIEQ_CANDIDATE_DIR."); expect(true, "skipped"); return; }
        juce::Array<juce::File> wavs;
        juce::File(dirEnv).findChildFiles(wavs, juce::File::findFiles, false, "*.wav;*.WAV");
        wavs.sort();
        if (wavs.isEmpty()) { expect(false, "no WAVs"); return; }

        logMessage("  salienceDb was checked first and is NOT usable here: it is");
        logMessage("  bandDbFused plus a fixed per-band equal-loudness offset set in");
        logMessage("  prepare() from the centre frequency, so it adds no information");
        logMessage("  about content versus background. (PerceptualFrontEnd.cpp:402)");

        std::vector<Measured> all;
        for (const auto& w : wavs) { auto m = measure(w); if (!m.regions.empty()) all.push_back(std::move(m)); }

        for (int block = 0; block < 5; ++block)
        {
            const char* title = block == 0
                ? "  CANDIDATE: corrWithSource - does this region move WITH the source? (-1..+1)"
                : (block == 1 ? "  TEMPORAL SPREAD p90-p10 (dB) - falls down on sustained sources"
                : (block == 2 ? "  MEDIAN REGION LEVEL p50 (dB) - to tell a floor clamp from real steadiness"
                : (block == 3 ? "  SPREAD from UNFUSED bandDb - cross-check for an LF-fusion hold"
                              : "  CURRENT regionConfidence (schema 4, absolute -108 dB floor)")));
            logMessage(""); logMessage(title);
            logMessage("    source            Sub  Bass LoMid   Mid  Pres Brill   Air");
            for (const auto& m : all)
            {
                juce::String row;
                for (const auto& st : m.regions)
                    row += padL(block == 0 ? d2(st.corrWithSource)
                              : (block == 1 ? d1(st.spread)
                              : (block == 2 ? d1(st.p50)
                              : (block == 3 ? d1(st.spreadUnfused) : d1(st.regionConfidence)))), 7);
                logMessage("    " + m.name.paddedRight(' ', 16) + row);
            }
        }

        // Direct check of the LF-fusion suspicion, rather than an inference from
        // the tables above: feed the SAME file through pushMono in host-sized
        // blocks and compare Sub/Bass spread with the whole-file analyzeAll call.
        logMessage("");
        logMessage("  LF-FUSION CHECK: Sub/Bass spread, whole-file analyzeAll vs 512-sample blocks");
        logMessage("    source           Sub(all) Sub(blk)  Bass(all) Bass(blk)");
        for (const auto& w : wavs)
        {
            juce::AudioBuffer<float> a; double sr = 0.0;
            if (!loadAudio(w, a, sr)) continue;
            std::vector<float> mono(static_cast<size_t>(std::max(0, a.getNumSamples())), 0.0f);
            for (int c = 0; c < a.getNumChannels(); ++c)
            {
                const float* sp = a.getReadPointer(c);
                for (int i = 0; i < a.getNumSamples(); ++i) mono[static_cast<size_t>(i)] += sp[i];
            }
            if (a.getNumChannels() > 0) for (auto& v : mono) v /= static_cast<float>(a.getNumChannels());

            auto spreadOf = [&](const std::vector<PerceptualFrontEnd::Frame>& fr,
                                const std::vector<float>& ctr, float lo, float hi)
            {
                std::vector<float> series;
                for (const auto& f : fr)
                {
                    if (f.bandDbFused.size() != ctr.size()) continue;
                    double s2 = 0.0; int n2 = 0;
                    for (std::size_t b = 0; b < ctr.size(); ++b)
                        if (ctr[b] >= lo && ctr[b] < hi && ctr[b] >= kLoHz)
                        { s2 += f.bandDbFused[b]; ++n2; }
                    if (n2 > 0) series.push_back(static_cast<float>(s2 / n2));
                }
                return series.empty() ? 0.0f : pct(series, 0.90f) - pct(series, 0.10f);
            };

            PerceptualFrontEnd feA; feA.prepare(sr);
            const auto framesA = feA.analyzeAll(mono.data(), static_cast<int>(mono.size()));
            std::vector<float> ctr;
            for (int b = 0; b < feA.numBands(); ++b) ctr.push_back(feA.bandCenterHz(b));

            PerceptualFrontEnd feB; feB.prepare(sr);
            std::vector<PerceptualFrontEnd::Frame> framesB;
            for (int off = 0; off < static_cast<int>(mono.size()); off += 512)
            {
                const int n2 = std::min(512, static_cast<int>(mono.size()) - off);
                feB.pushMono(mono.data() + off, n2,
                             [&framesB](const PerceptualFrontEnd::Frame& f) { framesB.push_back(f); });
            }
            logMessage("    " + w.getFileNameWithoutExtension().paddedRight(' ', 16)
                + padL(d1(spreadOf(framesA, ctr, 20.0f, 60.0f)), 8)
                + padL(d1(spreadOf(framesB, ctr, 20.0f, 60.0f)), 9)
                + padL(d1(spreadOf(framesA, ctr, 60.0f, 160.0f)), 10)
                + padL(d1(spreadOf(framesB, ctr, 60.0f, 160.0f)), 10));
        }

        logMessage("");
        logMessage("  STRESS CASES from the brief (report only - none of this is a fixture):");
        auto show = [&](const char* src, int rA, const char* nA, int rB, const char* nB)
        {
            for (const auto& m : all)
                if (m.name.containsIgnoreCase(src))
                    logMessage(juce::String("    ") + juce::String(src).paddedRight(' ', 12)
                        + juce::String(nA) + " corr=" + d2(m.regions[(size_t) rA].corrWithSource)
                        + " conf=" + d1(m.regions[(size_t) rA].regionConfidence)
                        + "   vs " + juce::String(nB) + " corr=" + d2(m.regions[(size_t) rB].corrWithSource)
                        + " conf=" + d1(m.regions[(size_t) rB].regionConfidence));
        };
        show("shaker", 0, "Sub", 5, "Brill");
        show("hihat",  1, "Bass", 5, "Brill");
        show("bass",   1, "Bass", 6, "Air");
        show("fullmix",1, "Bass", 6, "Air");
        show("pad",    1, "Bass", 3, "Mid");

        logMessage("");
        logMessage("  READ: report-only. The question is whether corrWithSource separates");
        logMessage("  content from carpet WITHOUT punishing a pink-tilted full mix in the");
        logMessage("  Air region or a sustained pad in its own low end. Nothing is decided");
        logMessage("  here and no instrument is assumed to have a required timbre.");
        expect(true, "probe completed");
    }
};

static RegionalEvidenceProbe sRegionalEvidenceProbe;

#endif
