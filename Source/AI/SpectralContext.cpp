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
    float lfValidFraction) const
{
    SpectralContext out;
    out.framesObserved = std::max(framesObserved, 0);
    out.lfValidFraction = clamp01(lfValidFraction);

    if (!finiteAscending(bandCentersHz, meanBandDbFused)
        || (!temporalStdDevDb.empty() && temporalStdDevDb.size() != bandCentersHz.size())
        || !(options.maxAnalysisHz > options.minAnalysisHz)
        || options.fullWarmupFrames <= 0
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

    const float coverageScore = clamp01(static_cast<float>(usableBands)
                                      / static_cast<float>(consideredBands));
    const float levelScore = smoothStep(options.minUsableDb,
                                        options.fullLevelConfidenceDb,
                                        globalRefDb);
    const float warmupScore = clamp01(static_cast<float>(out.framesObserved)
                                    / static_cast<float>(options.fullWarmupFrames));

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

    // Mildly discount extremely unstable snapshots; dynamic music should still
    // reach high confidence after sufficient observation.
    const float stabilityScore = stdCount > 0
        ? std::clamp(std::exp(-meanStdDev / 18.0f), 0.35f, 1.0f)
        : 1.0f;
    const float lfScore = 0.92f + 0.08f * out.lfValidFraction;

    out.confidence = clamp01(levelScore * coverageScore * warmupScore
                           * stabilityScore * lfScore);
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
    lfValidFrames = 0;
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

    const double n = static_cast<double>(frames);
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

    std::vector<float> means(mean.size());
    std::vector<float> stddev(mean.size(), 0.0f);
    for (std::size_t i = 0; i < mean.size(); ++i)
    {
        means[i] = static_cast<float>(mean[i]);
        if (frames > 1)
            stddev[i] = static_cast<float>(std::sqrt(std::max(0.0, m2[i] / static_cast<double>(frames - 1))));
    }

    return builder.build(centers, means, stddev, frames,
        static_cast<float>(lfValidFrames) / static_cast<float>(frames));
}

} // namespace AIEQPerceptual
