#include "SpectralContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace AIEQPerceptual
{
namespace
{
struct RegionRange { float lo; float hi; };

constexpr std::array<RegionRange, kSpectralRegionCount> kRegions {{
    { 20.0f, 60.0f },
    { 60.0f, 160.0f },
    { 160.0f, 500.0f },
    { 500.0f, 2000.0f },
    { 2000.0f, 5000.0f },
    { 5000.0f, 10000.0f },
    { 10000.0f, 24000.0f }
}};

float clamp01(float x) noexcept
{
    return std::clamp(x, 0.0f, 1.0f);
}

float smoothStep(float edge0, float edge1, float x) noexcept
{
    if (!(edge1 > edge0)) return x >= edge1 ? 1.0f : 0.0f;
    const float t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

float median(std::vector<float>& values)
{
    if (values.empty()) return -120.0f;
    const auto mid = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), mid, values.end());
    float m = *mid;
    if ((values.size() & 1U) == 0U)
    {
        const auto lower = std::max_element(values.begin(), mid);
        m = 0.5f * (m + *lower);
    }
    return m;
}

bool finiteAscending(std::span<const float> centers,
                     std::span<const float> values) noexcept
{
    if (centers.size() != values.size() || centers.size() < 8)
        return false;

    float previous = 0.0f;
    for (std::size_t i = 0; i < centers.size(); ++i)
    {
        if (!std::isfinite(centers[i]) || !std::isfinite(values[i])
            || centers[i] <= previous)
            return false;
        previous = centers[i];
    }
    return true;
}

float regionMean(std::span<const float> centers,
                 std::span<const float> values,
                 float lo, float hi,
                 float fallback) noexcept
{
    double sum = 0.0;
    int count = 0;
    for (std::size_t i = 0; i < centers.size(); ++i)
    {
        if (centers[i] >= lo && centers[i] < hi)
        {
            sum += values[i];
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : fallback;
}

} // namespace

SpectralContext SpectralContextBuilder::build(
    std::span<const float> bandCentersHz,
    std::span<const float> meanBandDbFused,
    std::span<const float> temporalStdDevDb,
    int framesObserved,
    float lfValidFraction,
    int activeFrames) const
{
    SpectralContext out;
    out.framesObserved = std::max(framesObserved, 0);
    out.activeFrames = activeFrames < 0 ? out.framesObserved
                                        : std::clamp(activeFrames, 0, out.framesObserved);
    out.activeFrameFraction = out.framesObserved > 0
        ? static_cast<float>(out.activeFrames) / static_cast<float>(out.framesObserved)
        : 0.0f;
    out.lfValidFraction = clamp01(lfValidFraction);

    if (!finiteAscending(bandCentersHz, meanBandDbFused)
        || (!temporalStdDevDb.empty() && temporalStdDevDb.size() != bandCentersHz.size())
        || !(options.maxAnalysisHz > options.minAnalysisHz)
        || options.fullObservationFrames <= 0
        || !(options.rolloffFraction > 0.0f && options.rolloffFraction < 1.0f))
        return out;

    std::vector<float> referenceCandidates;
    referenceCandidates.reserve(bandCentersHz.size());
    int usableBands = 0;
    int consideredBands = 0;

    for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
    {
        const float f = bandCentersHz[i];
        if (f < options.minAnalysisHz || f > options.maxAnalysisHz)
            continue;
        ++consideredBands;
        if (meanBandDbFused[i] >= options.minUsableDb)
            ++usableBands;
        if (f >= 50.0f && f <= 12000.0f && meanBandDbFused[i] >= options.minUsableDb)
            referenceCandidates.push_back(meanBandDbFused[i]);
    }

    if (consideredBands == 0)
        return out;

    // Near-silence is still a structurally valid spectral shape; it must fail
    // closed through confidence, not through malformed-state semantics. If no
    // band clears the usable floor, derive the normalization reference from all
    // finite mid-band values and let levelScore/coverage drive confidence to 0.
    if (referenceCandidates.empty())
    {
        for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
            if (bandCentersHz[i] >= 50.0f && bandCentersHz[i] <= 12000.0f)
                referenceCandidates.push_back(meanBandDbFused[i]);
    }

    if (referenceCandidates.empty())
        return out;

    float globalRefDb = median(referenceCandidates);
    out.sourceLevelDb = globalRefDb;

    std::vector<float> normalized(meanBandDbFused.size(), 0.0f);
    for (std::size_t i = 0; i < meanBandDbFused.size(); ++i)
        normalized[i] = std::clamp(meanBandDbFused[i] - globalRefDb, -36.0f, 18.0f);

    for (std::size_t r = 0; r < kSpectralRegionCount; ++r)
    {
        const auto range = kRegions[r];
        out.regionRelativeDb[r] = regionMean(bandCentersHz, normalized,
                                              range.lo, range.hi, 0.0f);
    }

    // Shape-only linear regression in dB vs octaves around 1 kHz.
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    int regressionCount = 0;
    for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
    {
        const float f = bandCentersHz[i];
        if (f < 80.0f || f > 12000.0f || meanBandDbFused[i] < options.minUsableDb)
            continue;
        const double x = std::log2(static_cast<double>(f) / 1000.0);
        const double y = normalized[i];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
        ++regressionCount;
    }
    if (regressionCount >= 4)
    {
        const double n = static_cast<double>(regressionCount);
        const double denom = n * sxx - sx * sx;
        if (std::abs(denom) > 1.0e-12)
            out.spectralTiltDbPerOctave = static_cast<float>((n * sxy - sx * sy) / denom);
    }

    // Perceptual/log-band pseudo-energy: independent of global gain because it
    // is derived from normalized dB. Use a geometric-frequency centroid so the
    // 12-bands/octave representation does not artificially favour HF bandwidth.
    double weightSum = 0.0;
    double weightedLog2Hz = 0.0;
    struct EnergyPoint { float frequency; double weight; };
    std::vector<EnergyPoint> energy;
    energy.reserve(bandCentersHz.size());

    for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
    {
        const float f = bandCentersHz[i];
        if (f < options.minAnalysisHz || f > options.maxAnalysisHz)
            continue;
        const double w = std::pow(10.0, static_cast<double>(std::clamp(normalized[i], -30.0f, 12.0f)) / 10.0);
        energy.push_back({ f, w });
        weightSum += w;
        weightedLog2Hz += w * std::log2(static_cast<double>(f));
    }

    if (weightSum > 0.0)
    {
        out.perceptualCentroidHz = static_cast<float>(std::exp2(weightedLog2Hz / weightSum));
        const double threshold = weightSum * static_cast<double>(options.rolloffFraction);
        double cumulative = 0.0;
        out.perceptualRolloffHz = energy.back().frequency;
        for (const auto& p : energy)
        {
            cumulative += p.weight;
            if (cumulative >= threshold)
            {
                out.perceptualRolloffHz = p.frequency;
                break;
            }
        }
    }

    // --- schema 2 descriptors -------------------------------------------
    // Warmth and mud both live inside the single 160-500 Hz region, so the
    // policy cannot separate body from mud from the public regions alone.
    out.warmthZoneDb = regionMean(bandCentersHz, normalized, 120.0f, 250.0f, 0.0f);
    out.mudZoneDb    = regionMean(bandCentersHz, normalized, 250.0f, 500.0f, 0.0f);

    // Narrow-HF prominence: how far the strongest HF band stands above the
    // robust HF baseline. A regional mean cannot see a narrow spike at all;
    // percentile-minus-median can, without becoming a sibilance detector.
    {
        std::vector<float> hf;
        hf.reserve(bandCentersHz.size());
        for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
            if (bandCentersHz[i] >= 5000.0f && bandCentersHz[i] <= 18000.0f)
                hf.push_back(normalized[i]);

        if (hf.size() >= 4)
        {
            std::vector<float> forMedian = hf;
            const float hfMedian = median(forMedian);
            const std::size_t idx = static_cast<std::size_t>(
                std::floor(0.95 * static_cast<double>(hf.size() - 1)));
            std::nth_element(hf.begin(),
                             hf.begin() + static_cast<std::ptrdiff_t>(idx),
                             hf.end());
            out.hfPeakProminenceDb = std::max(0.0f, hf[idx] - hfMedian);
        }
    }

    // Activity values are normalized-shape prominence descriptors, not meters.
    out.lowMidActivity = smoothStep(-6.0f, 6.0f,
        0.70f * out.region(SpectralRegion::LowMid)
      + 0.30f * out.region(SpectralRegion::Bass));
    out.broadHighFrequencyProminence = smoothStep(-6.0f, 6.0f,
        0.55f * out.region(SpectralRegion::Brilliance)
      + 0.45f * out.region(SpectralRegion::Air));

    // ---- descriptive occupancy (no longer a confidence factor) -----------
    const float coverageScore = clamp01(static_cast<float>(usableBands)
                                      / static_cast<float>(consideredBands));

    // ---- layer 1: hard validity -----------------------------------------
    // Structural sufficiency is a yes/no question and is kept out of the graded
    // score, so that silence and a two-frame buffer fail for the reason they
    // actually fail rather than by scoring low on a curve.
    float bandPeakDb = -200.0f;
    for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
        if (bandCentersHz[i] >= options.minAnalysisHz
            && bandCentersHz[i] <= options.maxAnalysisHz)
            bandPeakDb = std::max(bandPeakDb, meanBandDbFused[i]);
    out.bandPeakDb = bandPeakDb;

    const bool aboveSilence = bandPeakDb > options.nearSilenceFloorDb;
    const bool enoughFrames = out.activeFrames >= options.minObservationFrames;
    out.hasUsableEvidence = aboveSilence && enoughFrames;

    // ---- layer 2: graded evidence quality --------------------------------
    const float levelScore = smoothStep(options.nearSilenceFloorDb,
                                        options.fullEvidenceLevelDb,
                                        bandPeakDb);

    const float observationScore = smoothStep(
        static_cast<float>(options.minObservationFrames),
        static_cast<float>(options.fullObservationFrames),
        static_cast<float>(out.activeFrames));

    float meanStdDev = 0.0f;
    int stdCount = 0;
    if (!temporalStdDevDb.empty())
    {
        for (std::size_t i = 0; i < temporalStdDevDb.size(); ++i)
        {
            if (bandCentersHz[i] >= options.minAnalysisHz
                && bandCentersHz[i] <= options.maxAnalysisHz
                && std::isfinite(temporalStdDevDb[i]))
            {
                meanStdDev += std::max(0.0f, temporalStdDevDb[i]);
                ++stdCount;
            }
        }
    }
    if (stdCount > 0)
        meanStdDev /= static_cast<float>(stdCount);

    // How well the AGGREGATE is pinned down, not how alike the frames were.
    // A singer changing note makes every frame differ and leaves the average
    // spectrum just as knowable, provided enough frames were averaged.
    out.aggregateStandardErrorDb = out.activeFrames > 0
        ? meanStdDev / std::sqrt(static_cast<float>(out.activeFrames))
        : meanStdDev;
    // Descending ramp: LESS standard error means MORE confidence. smoothStep()
    // requires edge0 < edge1 and silently degrades to a step otherwise, so the
    // inversion is explicit here rather than smuggled into the argument order.
    const float stabilityScore = stdCount > 0
        ? 1.0f - smoothStep(options.fullStabilityStdErrDb,
                            options.noStabilityStdErrDb,
                            out.aggregateStandardErrorDb)
        : observationScore;

    const float lfScore = 0.92f + 0.08f * out.lfValidFraction;

    out.levelScore = levelScore;
    out.coverageScore = coverageScore;
    out.warmupScore = observationScore;
    out.stabilityScore = stabilityScore;
    out.lfScore = lfScore;

    // Normalized (geometric-mean) combination rather than a raw product. Three
    // terms at 0.7 multiply to 0.34 and fail a 0.45 gate even though every one
    // of them says "good enough"; their geometric mean is 0.7, which is what
    // three partial evidences actually amount to. Weights are equal because
    // nothing measured so far justifies ranking these three against each other,
    // and an unjustified weighting is just a tuned constant in disguise.
    float softConfidence = 0.0f;
    if (levelScore > 0.0f && stabilityScore > 0.0f && observationScore > 0.0f)
    {
        constexpr float kThird = 1.0f / 3.0f;
        softConfidence = std::exp(kThird * (std::log(levelScore)
                                          + std::log(stabilityScore)
                                          + std::log(observationScore)));
    }
    out.confidence = out.hasUsableEvidence ? clamp01(softConfidence * lfScore) : 0.0f;

    // ---- layer 3: regional evidence --------------------------------------
    // Occupancy measured against the ABSOLUTE analysis floor, per region. A
    // relative-to-peak criterion was measured and rejected: a full mix falls
    // ~40 dB from its bass peak to 15 kHz purely because music is pink, which
    // would have scored a perfectly ordinary mix as having no HF evidence.
    for (std::size_t r = 0; r < kSpectralRegionCount; ++r)
    {
        const auto range = kRegions[r];
        int inRegion = 0, usableInRegion = 0;
        for (std::size_t i = 0; i < bandCentersHz.size(); ++i)
        {
            const float f = bandCentersHz[i];
            if (f < range.lo || f >= range.hi
                || f < options.minAnalysisHz || f > options.maxAnalysisHz)
                continue;
            ++inRegion;
            if (meanBandDbFused[i] >= options.minUsableDb)
                ++usableInRegion;
        }
        const float occupancy = inRegion > 0
            ? static_cast<float>(usableInRegion) / static_cast<float>(inRegion)
            : 0.0f;
        out.regionConfidence[r] = out.confidence * smoothStep(0.15f, 0.60f, occupancy);
    }

    out.valid = true;
    return out;
}

void SpectralContextAccumulator::prepare(std::span<const float> bandCentersHz)
{
    centers.assign(bandCentersHz.begin(), bandCentersHz.end());
    mean.assign(centers.size(), 0.0);
    m2.assign(centers.size(), 0.0);
    reset();
}

void SpectralContextAccumulator::reset() noexcept
{
    std::fill(mean.begin(), mean.end(), 0.0);
    std::fill(m2.begin(), m2.end(), 0.0);
    frames = 0;
    activeFrames = 0;
    lfValidFrames = 0;
    runningPeakDb = -200.0f;
}

bool SpectralContextAccumulator::pushFrame(std::span<const float> bandDbFused,
                                           bool lfValid) noexcept
{
    if (bandDbFused.size() != centers.size() || centers.empty())
        return false;
    for (float value : bandDbFused)
        if (!std::isfinite(value))
            return false;

    ++frames;
    if (lfValid) ++lfValidFrames;

    // Separate playing from pausing. The gap between two sung phrases is not
    // evidence that the voice has a weak spectrum; it is the absence of
    // evidence for that frame. Folding it into the tonal average was measured
    // to cost a lead vocal 11 dB of apparent level and to nearly double its
    // apparent instability.
    float frameLevelDb = -200.0f;
    for (float value : bandDbFused)
        frameLevelDb = std::max(frameLevelDb, value);
    runningPeakDb = std::max(runningPeakDb, frameLevelDb);

    if (frameLevelDb < runningPeakDb - builderOptions.activeFrameRangeDb)
        return true;   // counted as observed, contributes no tonal statistics

    ++activeFrames;
    const double n = static_cast<double>(activeFrames);
    for (std::size_t i = 0; i < bandDbFused.size(); ++i)
    {
        const double x = bandDbFused[i];
        const double delta = x - mean[i];
        mean[i] += delta / n;
        const double delta2 = x - mean[i];
        m2[i] += delta * delta2;
    }
    return true;
}

SpectralContext SpectralContextAccumulator::snapshot() const
{
    if (frames <= 0 || centers.empty())
        return {};

    if (activeFrames <= 0)
    {
        // Observed, but nothing was ever loud enough to describe. Report it as
        // a well-formed snapshot with no usable evidence rather than as a
        // malformed one.
        SpectralContext quiet;
        quiet.framesObserved = frames;
        quiet.valid = true;
        return quiet;
    }

    std::vector<float> means(mean.size());
    std::vector<float> stddev(mean.size(), 0.0f);
    for (std::size_t i = 0; i < mean.size(); ++i)
    {
        means[i] = static_cast<float>(mean[i]);
        if (activeFrames > 1)
            stddev[i] = static_cast<float>(std::sqrt(std::max(0.0, m2[i] / static_cast<double>(activeFrames - 1))));
    }

    return builder.build(centers, means, stddev, frames,
        static_cast<float>(lfValidFrames) / static_cast<float>(frames),
        activeFrames);
}

} // namespace AIEQPerceptual
