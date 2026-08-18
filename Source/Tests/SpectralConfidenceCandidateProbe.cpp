#if JUCE_UNIT_TESTS

/*
 * T5.4.2 step 0 - measure the candidate statistics before choosing any of them.
 *
 * The brief for this tranche is explicit that sourceLevel = median(all bands),
 * the activity threshold and the stability replacement must be chosen from
 * measurements on real material, not picked and then justified. This probe
 * changes NO production code. It runs the real PerceptualFrontEnd over each
 * file and prints the candidates side by side so the choice has evidence
 * under it.
 *
 * Usage:
 *   AIEQ_CANDIDATE_DIR=/path/to/wavs \
 *     build-verify/Release/bin/AIEqualizerPro_IntegrationTests \
 *       --category=ManualProbe -v
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../AI/PerceptualFrontEnd.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{


constexpr float kAnalysisLoHz = 30.0f;
constexpr float kAnalysisHiHz = 18000.0f;

bool loadAudio(const juce::File& f, juce::AudioBuffer<float>& out, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr) return false;
    out.setSize(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(&out, 0, static_cast<int>(r->lengthInSamples), 0, true, true);
    sr = r->sampleRate;
    return true;
}

std::vector<float> monoMix(const juce::AudioBuffer<float>& a)
{
    std::vector<float> m(static_cast<size_t>(std::max(0, a.getNumSamples())), 0.0f);
    for (int c = 0; c < a.getNumChannels(); ++c)
    {
        const float* s = a.getReadPointer(c);
        for (int i = 0; i < a.getNumSamples(); ++i) m[static_cast<size_t>(i)] += s[i];
    }
    if (a.getNumChannels() > 0)
        for (auto& v : m) v /= static_cast<float>(a.getNumChannels());
    return m;
}

/** Percentile over a copy, p in [0,1]. */
float percentile(std::vector<float> v, float p)
{
    if (v.empty()) return -120.0f;
    const auto idx = static_cast<std::size_t>(
        std::floor(std::clamp(p, 0.0f, 1.0f) * static_cast<double>(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(idx), v.end());
    return v[idx];
}

struct Stats
{
    // Frame-activity duty cycle at several thresholds below the peak frame level.
    float duty30 = 0.0f, duty40 = 0.0f, duty50 = 0.0f;
    int   framesTotal = 0, framesActive40 = 0;

    // sourceLevel candidates.
    float medianAllBands_allFrames = -120.0f;    // what production does today
    float medianAllBands_activeFrames = -120.0f; // same statistic, pauses excluded
    float p50ActiveBands_activeFrames = -120.0f;
    float p70ActiveBands_activeFrames = -120.0f;
    float p90ActiveBands_activeFrames = -120.0f;
    float bandPeak_activeFrames = -120.0f;

    // Band occupancy.
    float activeBandFractionAllFrames = 0.0f;
    float activeBandFractionActiveFrames = 0.0f;

    // Stability candidates.
    float meanStdDevAllFrames = 0.0f;
    float meanStdDevActiveFrames = 0.0f;
    float standardErrorActiveFrames = 0.0f;  // meanStdDev / sqrt(N_active)
};

Stats measure(const juce::AudioBuffer<float>& audio, double sr, float bandSpanDb)
{
    Stats st;
    const auto mono = monoMix(audio);

    PerceptualFrontEnd fe;
    fe.prepare(sr);
    const auto frames = fe.analyzeAll(mono.data(), static_cast<int>(mono.size()));
    if (frames.empty()) return st;

    const std::size_t nb = static_cast<std::size_t>(fe.numBands());
    std::vector<float> centers(nb);
    for (std::size_t b = 0; b < nb; ++b) centers[b] = fe.bandCenterHz(static_cast<int>(b));

    std::vector<std::size_t> inRange;
    for (std::size_t b = 0; b < nb; ++b)
        if (centers[b] >= kAnalysisLoHz && centers[b] <= kAnalysisHiHz)
            inRange.push_back(b);

    // Per-frame level = strongest band in the analysis range. Cheap, and it
    // tracks whichever region the source actually occupies, so it works for a
    // bass and a hi-hat without either being special-cased.
    std::vector<float> frameLevel;
    frameLevel.reserve(frames.size());
    for (const auto& f : frames)
    {
        if (f.bandDbFused.size() != nb) { frameLevel.push_back(-120.0f); continue; }
        float mx = -200.0f;
        for (auto b : inRange) mx = std::max(mx, f.bandDbFused[b]);
        frameLevel.push_back(mx);
    }
    st.framesTotal = static_cast<int>(frames.size());

    // Reference against a high percentile rather than the outright maximum, so a
    // single transient cannot define "loud" for the whole file.
    const float peakRef = percentile(frameLevel, 0.95f);
    auto dutyAt = [&](float belowDb)
    {
        int n = 0;
        for (float l : frameLevel) if (l >= peakRef - belowDb) ++n;
        return static_cast<float>(n) / static_cast<float>(frameLevel.size());
    };
    st.duty30 = dutyAt(30.0f); st.duty40 = dutyAt(40.0f); st.duty50 = dutyAt(50.0f);

    auto accumulate = [&](bool activeOnly, std::vector<float>& means, std::vector<float>& sds, int& count)
    {
        std::vector<double> mean(nb, 0.0), m2(nb, 0.0);
        std::size_t n = 0;
        for (std::size_t fi = 0; fi < frames.size(); ++fi)
        {
            if (frames[fi].bandDbFused.size() != nb) continue;
            if (activeOnly && frameLevel[fi] < peakRef - 40.0f) continue;
            ++n;
            for (std::size_t i = 0; i < nb; ++i)
            {
                const double x = frames[fi].bandDbFused[i];
                const double d = x - mean[i];
                mean[i] += d / static_cast<double>(n);
                m2[i] += d * (x - mean[i]);
            }
        }
        means.assign(nb, -120.0f); sds.assign(nb, 0.0f);
        for (std::size_t i = 0; i < nb; ++i)
        {
            means[i] = static_cast<float>(mean[i]);
            if (n > 1) sds[i] = static_cast<float>(std::sqrt(std::max(0.0, m2[i] / static_cast<double>(n - 1))));
        }
        count = static_cast<int>(n);
    };

    std::vector<float> meanAll, sdAll, meanAct, sdAct;
    int nAll = 0, nAct = 0;
    accumulate(false, meanAll, sdAll, nAll);
    accumulate(true,  meanAct, sdAct, nAct);
    st.framesActive40 = nAct;

    // Today's statistic: median over 50-12k bands above the -108 dB absolute floor.
    auto productionMedian = [&](const std::vector<float>& means)
    {
        std::vector<float> v;
        for (std::size_t b = 0; b < nb; ++b)
            if (centers[b] >= 50.0f && centers[b] <= 12000.0f && means[b] >= -108.0f)
                v.push_back(means[b]);
        return percentile(v, 0.5f);
    };
    st.medianAllBands_allFrames = productionMedian(meanAll);
    st.medianAllBands_activeFrames = productionMedian(meanAct);

    // Candidate: percentiles over bands that are active RELATIVE TO THE SOURCE'S
    // own spectral peak, rather than against an absolute floor. This is the
    // change that should stop a hi-hat from reading as quiet merely because most
    // of the spectrum is empty.
    auto activeBands = [&](const std::vector<float>& means)
    {
        float peak = -200.0f;
        for (auto b : inRange) peak = std::max(peak, means[b]);
        std::vector<float> v;
        for (auto b : inRange) if (means[b] >= peak - bandSpanDb) v.push_back(means[b]);
        return std::pair<std::vector<float>, float>{ v, peak };
    };
    {
        auto [vAct, peakAct] = activeBands(meanAct);
        st.bandPeak_activeFrames = peakAct;
        st.p50ActiveBands_activeFrames = percentile(vAct, 0.50f);
        st.p70ActiveBands_activeFrames = percentile(vAct, 0.70f);
        st.p90ActiveBands_activeFrames = percentile(vAct, 0.90f);
        st.activeBandFractionActiveFrames =
            static_cast<float>(vAct.size()) / static_cast<float>(inRange.size());
        auto [vAll, peakAll] = activeBands(meanAll);
        juce::ignoreUnused(peakAll);
        st.activeBandFractionAllFrames =
            static_cast<float>(vAll.size()) / static_cast<float>(inRange.size());
    }

    auto meanSd = [&](const std::vector<float>& sds)
    {
        double s = 0.0; int c = 0;
        for (auto b : inRange) { s += sds[b]; ++c; }
        return c > 0 ? static_cast<float>(s / c) : 0.0f;
    };
    st.meanStdDevAllFrames = meanSd(sdAll);
    st.meanStdDevActiveFrames = meanSd(sdAct);
    st.standardErrorActiveFrames = nAct > 0
        ? st.meanStdDevActiveFrames / std::sqrt(static_cast<float>(nAct)) : 0.0f;
    return st;
}

juce::String c1(float v, int dp = 1) { return juce::String(v, dp); }
} // namespace

class SpectralConfidenceCandidateProbe final : public juce::UnitTest
{
public:
    SpectralConfidenceCandidateProbe()
        : juce::UnitTest("Confidence candidate statistics probe (T5.4.2)", "ManualProbe") {}

    void runTest() override
    {
        beginTest("Measure sourceLevel / activity / stability candidates on real files");

        const auto dirEnv = juce::SystemStats::getEnvironmentVariable("AIEQ_CANDIDATE_DIR", {});
        if (dirEnv.isEmpty())
        {
            logMessage("  SKIP: set AIEQ_CANDIDATE_DIR=/path/to/wavs to run this report-only probe.");
            expect(true, "skipped");
            return;
        }
        const juce::File dir(dirEnv);
        juce::Array<juce::File> wavs;
        dir.findChildFiles(wavs, juce::File::findFiles, false, "*.wav;*.WAV");
        wavs.sort();
        if (wavs.isEmpty()) { expect(false, "no WAVs in " + dir.getFullPathName()); return; }

        const float bandSpanDb =
            juce::SystemStats::getEnvironmentVariable("AIEQ_BAND_SPAN_DB", "40").getFloatValue();
        logMessage("  band-active span = peak - " + c1(bandSpanDb) + " dB;  frame-active = peakP95 - 40 dB");
        logMessage("");
        logMessage("  FRAME ACTIVITY (duty cycle at 30/40/50 dB below the P95 frame level)");
        logMessage("    file                 frames  duty30  duty40  duty50   activeN");
        std::vector<std::pair<juce::String, Stats>> all;
        for (const auto& w : wavs)
        {
            juce::AudioBuffer<float> a; double sr = 0.0;
            if (!loadAudio(w, a, sr)) { logMessage("    " + w.getFileName() + "  UNREADABLE"); continue; }
            const auto st = measure(a, sr, bandSpanDb);
            all.emplace_back(w.getFileNameWithoutExtension(), st);
            logMessage("    " + w.getFileNameWithoutExtension().paddedRight(' ', 20)
                       + juce::String(st.framesTotal).paddedLeft(' ', 6)
                       + c1(st.duty30, 2).paddedLeft(' ', 8)
                       + c1(st.duty40, 2).paddedLeft(' ', 8)
                       + c1(st.duty50, 2).paddedLeft(' ', 8)
                       + juce::String(st.framesActive40).paddedLeft(' ', 10));
        }

        logMessage("");
        logMessage("  SOURCE LEVEL CANDIDATES (dB). 'prod' is today's median over all 50-12k bands.");
        logMessage("    file                  prod   prod@act  P50act  P70act  P90act  bandPeak");
        for (const auto& [name, st] : all)
            logMessage("    " + name.paddedRight(' ', 20)
                       + c1(st.medianAllBands_allFrames).paddedLeft(' ', 7)
                       + c1(st.medianAllBands_activeFrames).paddedLeft(' ', 10)
                       + c1(st.p50ActiveBands_activeFrames).paddedLeft(' ', 8)
                       + c1(st.p70ActiveBands_activeFrames).paddedLeft(' ', 8)
                       + c1(st.p90ActiveBands_activeFrames).paddedLeft(' ', 8)
                       + c1(st.bandPeak_activeFrames).paddedLeft(' ', 10));

        logMessage("");
        logMessage("  BAND OCCUPANCY and STABILITY candidates");
        logMessage("    file                 actBand(all) actBand(act)  sd(all) sd(act)  stdErr(act)");
        for (const auto& [name, st] : all)
            logMessage("    " + name.paddedRight(' ', 20)
                       + c1(st.activeBandFractionAllFrames, 3).paddedLeft(' ', 12)
                       + c1(st.activeBandFractionActiveFrames, 3).paddedLeft(' ', 13)
                       + c1(st.meanStdDevAllFrames).paddedLeft(' ', 9)
                       + c1(st.meanStdDevActiveFrames).paddedLeft(' ', 8)
                       + c1(st.standardErrorActiveFrames, 3).paddedLeft(' ', 13));

        logMessage("");
        logMessage("  READ: report-only, no production behaviour depends on this. The choice this");
        logMessage("  informs is which sourceLevel statistic is CONSISTENT across source types at");
        logMessage("  comparable programme level - spread across the column matters more than any");
        logMessage("  single value - and whether stdErr separates well-observed from under-observed.");
        expect(true, "probe completed");
    }
};

static SpectralConfidenceCandidateProbe sSpectralConfidenceCandidateProbe;

#endif
