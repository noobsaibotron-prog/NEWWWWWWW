#include "SparseParametricFitter.h"
#include "../DSP/BiquadCoefficients.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace AIEQPerceptual
{
namespace
{
constexpr std::array<float, 6> kPeakQs {{ 0.45f, 0.60f, 0.80f, 1.05f, 1.40f, 2.0f }};
constexpr float kShelfQ = 0.70710678f;
constexpr float kEpsilon = 1.0e-12f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kConstraintEpsilonDb = 1.0e-4f;

float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}

float magnitudeToDb(double magnitude) noexcept
{
    if (!std::isfinite(magnitude) || magnitude <= 1.0e-12)
        return -120.0f;
    return static_cast<float>(20.0 * std::log10(magnitude));
}

BiquadCoeffs makeCoefficients(const PlannedBand& band, double sampleRate) noexcept
{
    const float gainLinear = dbToGain(band.gainDb);

    switch (band.type)
    {
        case FilterType::LowShelf:
            return BiquadCoeffs::makeLowShelf(sampleRate, band.frequencyHz, band.q, gainLinear);
        case FilterType::Peak:
            return BiquadCoeffs::makePeakFilter(sampleRate, band.frequencyHz, band.q, gainLinear);
        case FilterType::HighShelf:
            return BiquadCoeffs::makeHighShelf(sampleRate, band.frequencyHz, band.q, gainLinear);
    }

    return BiquadCoeffs::makeBypass();
}

struct FrequencyGrid
{
    std::vector<float> frequencyHz;
    std::vector<float> cos1;
    std::vector<float> sin1;
    std::vector<float> cos2;
    std::vector<float> sin2;
};

FrequencyGrid makeFrequencyGrid(std::vector<float> frequencies, double sampleRate)
{
    std::sort(frequencies.begin(), frequencies.end());
    frequencies.erase(std::unique(frequencies.begin(), frequencies.end(), [](float a, float b)
    {
        return std::abs(a - b) <= 1.0e-4f;
    }), frequencies.end());

    FrequencyGrid grid;
    grid.frequencyHz = std::move(frequencies);
    const auto n = grid.frequencyHz.size();
    grid.cos1.resize(n);
    grid.sin1.resize(n);
    grid.cos2.resize(n);
    grid.sin2.resize(n);

    for (std::size_t i = 0; i < n; ++i)
    {
        const float omega = 2.0f * kPi * grid.frequencyHz[i] / static_cast<float>(sampleRate);
        grid.cos1[i] = std::cos(omega);
        grid.sin1[i] = std::sin(omega);
        grid.cos2[i] = std::cos(2.0f * omega);
        grid.sin2[i] = std::sin(2.0f * omega);
    }

    return grid;
}

FrequencyGrid makeTargetGrid(const PerceptualTarget& target, double sampleRate)
{
    std::vector<float> frequencies;
    frequencies.reserve(target.points.size());
    for (const auto& point : target.points)
        frequencies.push_back(point.frequencyHz);
    return makeFrequencyGrid(std::move(frequencies), sampleRate);
}

FrequencyGrid makeDenseConstraintGrid(const PerceptualTarget& target,
                                      double sampleRate,
                                      int pointsPerOctave)
{
    const float nyquistSafe = static_cast<float>(sampleRate * 0.499);
    float low = target.points.empty() ? 20.0f : target.points.front().frequencyHz;
    float high = target.points.empty() ? std::min(20000.0f, nyquistSafe)
                                       : target.points.back().frequencyHz;

    for (const auto& region : target.protectedRegions)
    {
        low = std::min(low, region.minFrequencyHz);
        high = std::max(high, std::min(region.maxFrequencyHz, nyquistSafe));
    }
    for (const auto& region : target.responseBounds)
    {
        low = std::min(low, region.minFrequencyHz);
        high = std::max(high, std::min(region.maxFrequencyHz, nyquistSafe));
    }

    low = std::clamp(low, 10.0f, nyquistSafe * 0.99f);
    high = std::clamp(high, low * 1.001f, nyquistSafe);

    std::vector<float> frequencies;
    const int ppo = std::max(24, pointsPerOctave);
    const int count = static_cast<int>(std::ceil(std::log2(high / low) * ppo));
    frequencies.reserve(static_cast<std::size_t>(count + 8
        + target.protectedRegions.size() * 2 + target.responseBounds.size() * 2));

    for (int i = 0; i <= count; ++i)
    {
        const float frequency = low * std::pow(2.0f, static_cast<float>(i) / ppo);
        if (frequency <= high)
            frequencies.push_back(frequency);
    }

    auto addBoundary = [&](float frequency)
    {
        if (frequency > 0.0f && frequency < nyquistSafe)
            frequencies.push_back(frequency);
    };

    for (const auto& region : target.protectedRegions)
    {
        addBoundary(region.minFrequencyHz);
        addBoundary(region.maxFrequencyHz);
    }
    for (const auto& region : target.responseBounds)
    {
        addBoundary(region.minFrequencyHz);
        addBoundary(region.maxFrequencyHz);
    }

    return makeFrequencyGrid(std::move(frequencies), sampleRate);
}

float coefficientsResponseDb(const BiquadCoeffs& coeffs,
                             const FrequencyGrid& grid,
                             std::size_t index) noexcept
{
    if (!coeffs.valid)
        return 0.0f;

    const double c1 = grid.cos1[index];
    const double s1 = grid.sin1[index];
    const double c2 = grid.cos2[index];
    const double s2 = grid.sin2[index];

    const double nr = static_cast<double>(coeffs.b0)
                    + static_cast<double>(coeffs.b1) * c1
                    + static_cast<double>(coeffs.b2) * c2;
    const double ni = -static_cast<double>(coeffs.b1) * s1
                    - static_cast<double>(coeffs.b2) * s2;
    const double dr = 1.0
                    + static_cast<double>(coeffs.a1) * c1
                    + static_cast<double>(coeffs.a2) * c2;
    const double di = -static_cast<double>(coeffs.a1) * s1
                    - static_cast<double>(coeffs.a2) * s2;

    const double num2 = nr * nr + ni * ni;
    const double den2 = dr * dr + di * di;
    if (!std::isfinite(num2) || !std::isfinite(den2)
        || num2 <= 1.0e-24 || den2 <= 1.0e-24)
        return -120.0f;

    return static_cast<float>(10.0 * std::log10(num2 / den2));
}

std::vector<float> bandResponseOnGrid(const PlannedBand& band,
                                      const FrequencyGrid& grid,
                                      double sampleRate)
{
    const auto coeffs = makeCoefficients(band, sampleRate);
    std::vector<float> response(grid.frequencyHz.size(), 0.0f);
    for (std::size_t i = 0; i < response.size(); ++i)
        response[i] = coefficientsResponseDb(coeffs, grid, i);
    return response;
}

std::vector<float> planResponseOnGrid(const std::vector<PlannedBand>& bands,
                                      const FrequencyGrid& grid,
                                      double sampleRate)
{
    std::vector<float> response(grid.frequencyHz.size(), 0.0f);
    for (const auto& band : bands)
    {
        const auto bandResponse = bandResponseOnGrid(band, grid, sampleRate);
        for (std::size_t i = 0; i < response.size(); ++i)
            response[i] += bandResponse[i];
    }
    return response;
}

struct Score
{
    float weightedMse = std::numeric_limits<float>::infinity();
    float weightedRms = std::numeric_limits<float>::infinity();
    float maxProtectedDeviation = 0.0f;
    float maxResponseBoundViolation = 0.0f;
    bool respectsProtection = true;
    bool respectsResponseBounds = true;
};

Score scoreResponseOnTarget(const PerceptualTarget& target,
                            const std::vector<float>& response) noexcept
{
    double weightedError = 0.0;
    double weightSum = 0.0;
    Score score;

    if (response.size() != target.points.size())
    {
        score.respectsProtection = false;
        score.respectsResponseBounds = false;
        return score;
    }

    for (std::size_t i = 0; i < target.points.size(); ++i)
    {
        const auto& point = target.points[i];
        if (point.confidence < target.minConfidence)
            continue;

        const float value = response[i];
        const float residual = point.deltaDb - value;
        const float weight = std::max(0.0f, point.confidence);

        weightedError += static_cast<double>(weight) * residual * residual;
        weightSum += static_cast<double>(weight);

        float allowed = 0.0f;
        if (isFrequencyProtected(point.frequencyHz, target.protectedRegions, allowed))
        {
            const float magnitude = std::abs(value);
            score.maxProtectedDeviation = std::max(score.maxProtectedDeviation, magnitude);
            if (magnitude > allowed + kConstraintEpsilonDb)
                score.respectsProtection = false;
        }

        float minBound = -24.0f;
        float maxBound = 24.0f;
        if (getResponseBoundsAtFrequency(point.frequencyHz, target.responseBounds,
                                         minBound, maxBound))
        {
            float violation = 0.0f;
            if (value < minBound)
                violation = minBound - value;
            else if (value > maxBound)
                violation = value - maxBound;

            score.maxResponseBoundViolation = std::max(
                score.maxResponseBoundViolation, violation);
            if (violation > kConstraintEpsilonDb)
                score.respectsResponseBounds = false;
        }
    }

    if (weightSum <= 0.0)
    {
        score.weightedMse = 0.0f;
        score.weightedRms = 0.0f;
        return score;
    }

    score.weightedMse = static_cast<float>(weightedError / weightSum);
    score.weightedRms = std::sqrt(std::max(0.0f, score.weightedMse));
    return score;
}

Score denseConstraintScore(const PerceptualTarget& target,
                           const std::vector<PlannedBand>& bands,
                           const FrequencyGrid& denseGrid,
                           double sampleRate)
{
    Score score;
    score.weightedMse = 0.0f;
    score.weightedRms = 0.0f;

    if (target.protectedRegions.empty() && target.responseBounds.empty())
        return score;

    const auto response = planResponseOnGrid(bands, denseGrid, sampleRate);
    for (std::size_t i = 0; i < denseGrid.frequencyHz.size(); ++i)
    {
        const float frequency = denseGrid.frequencyHz[i];
        const float value = response[i];

        float allowed = 0.0f;
        if (isFrequencyProtected(frequency, target.protectedRegions, allowed))
        {
            const float magnitude = std::abs(value);
            score.maxProtectedDeviation = std::max(score.maxProtectedDeviation, magnitude);
            if (magnitude > allowed + kConstraintEpsilonDb)
                score.respectsProtection = false;
        }

        float minBound = -24.0f;
        float maxBound = 24.0f;
        if (getResponseBoundsAtFrequency(frequency, target.responseBounds,
                                         minBound, maxBound))
        {
            float violation = 0.0f;
            if (value < minBound)
                violation = minBound - value;
            else if (value > maxBound)
                violation = value - maxBound;

            score.maxResponseBoundViolation = std::max(
                score.maxResponseBoundViolation, violation);
            if (violation > kConstraintEpsilonDb)
                score.respectsResponseBounds = false;
        }
    }

    return score;
}

float complexityPenalty(const PlannedBand& band,
                        const SparseParametricFitter::Options& options) noexcept
{
    const float qExcess = std::max(0.0f, band.q - 1.0f);
    return options.filterPenalty + options.qPenalty * qExcess * qExcess;
}

float estimateLinearizedGain(const PerceptualTarget& target,
                             const std::vector<float>& currentResponse,
                             const std::vector<float>& oneDbBasis) noexcept
{
    double numerator = 0.0;
    double denominator = 0.0;

    for (std::size_t i = 0; i < target.points.size(); ++i)
    {
        const auto& point = target.points[i];
        if (point.confidence < target.minConfidence)
            continue;

        const float residual = point.deltaDb - currentResponse[i];
        const float basis = oneDbBasis[i];
        const float weight = std::max(0.0f, point.confidence);

        numerator += static_cast<double>(weight) * basis * residual;
        denominator += static_cast<double>(weight) * basis * basis;
    }

    if (denominator <= kEpsilon)
        return 0.0f;

    return static_cast<float>(numerator / denominator);
}

float confidenceNear(const PerceptualTarget& target, float frequencyHz) noexcept
{
    float bestDistance = std::numeric_limits<float>::infinity();
    float confidence = 0.0f;

    const float logF = std::log2(std::max(1.0f, frequencyHz));
    for (const auto& point : target.points)
    {
        const float distance = std::abs(std::log2(std::max(1.0f, point.frequencyHz)) - logF);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            confidence = point.confidence;
        }
    }

    return confidence;
}

std::vector<float> addResponses(const std::vector<float>& a,
                                const std::vector<float>& b)
{
    std::vector<float> out(a.size(), 0.0f);
    for (std::size_t i = 0; i < a.size(); ++i)
        out[i] = a[i] + b[i];
    return out;
}

std::vector<float> subtractResponses(const std::vector<float>& a,
                                     const std::vector<float>& b)
{
    std::vector<float> out(a.size(), 0.0f);
    for (std::size_t i = 0; i < a.size(); ++i)
        out[i] = a[i] - b[i];
    return out;
}

struct CandidateEvaluation
{
    PlannedBand band;
    std::vector<float> response;
    Score score;
    float objective = std::numeric_limits<float>::infinity();
    bool valid = false;
};

CandidateEvaluation evaluateCandidate(const PerceptualTarget& target,
                                      const FrequencyGrid& targetGrid,
                                      const std::vector<float>& currentResponse,
                                      FilterType type,
                                      float frequencyHz,
                                      float q,
                                      float gainDb,
                                      double sampleRate,
                                      const SparseParametricFitter::Options& options,
                                      std::uint64_t& candidateEvaluations)
{
    CandidateEvaluation result;
    if (!std::isfinite(gainDb) || std::abs(gainDb) < options.minGainDb)
        return result;

    result.band.type = type;
    result.band.frequencyHz = frequencyHz;
    result.band.q = q;
    result.band.gainDb = std::clamp(gainDb, -target.maxCutDb, target.maxBoostDb);
    result.band.confidence = confidenceNear(target, frequencyHz);

    result.response = bandResponseOnGrid(result.band, targetGrid, sampleRate);
    const auto trialResponse = addResponses(currentResponse, result.response);
    result.score = scoreResponseOnTarget(target, trialResponse);
    ++candidateEvaluations;

    if (!result.score.respectsProtection || !result.score.respectsResponseBounds)
        return result;

    result.objective = result.score.weightedMse + complexityPenalty(result.band, options);
    result.valid = true;
    return result;
}

CandidateEvaluation refineCandidateGain(const PerceptualTarget& target,
                                        const FrequencyGrid& targetGrid,
                                        const std::vector<float>& currentResponse,
                                        CandidateEvaluation seed,
                                        double sampleRate,
                                        const SparseParametricFitter::Options& options,
                                        std::uint64_t& candidateEvaluations)
{
    if (!seed.valid)
        return seed;

    CandidateEvaluation best = std::move(seed);
    float step = std::clamp(std::abs(best.band.gainDb) * 0.12f, 0.12f, 0.65f);

    for (int iteration = 0; iteration < options.candidateGainRefinementSteps; ++iteration)
    {
        for (int direction : { -1, +1 })
        {
            const float gain = std::clamp(best.band.gainDb + direction * step,
                                          -target.maxCutDb, target.maxBoostDb);
            if (std::abs(gain - best.band.gainDb) < 1.0e-5f)
                continue;

            auto trial = evaluateCandidate(target, targetGrid, currentResponse,
                                           best.band.type, best.band.frequencyHz,
                                           best.band.q, gain, sampleRate, options,
                                           candidateEvaluations);
            if (trial.valid && trial.objective + 1.0e-9f < best.objective)
                best = std::move(trial);
        }
        step *= 0.45f;
    }

    return best;
}

bool denseSafe(const PerceptualTarget& target,
               const std::vector<PlannedBand>& bands,
               const FrequencyGrid& denseGrid,
               double sampleRate,
               Score* outScore = nullptr)
{
    const auto dense = denseConstraintScore(target, bands, denseGrid, sampleRate);
    if (outScore != nullptr)
        *outScore = dense;
    return dense.respectsProtection && dense.respectsResponseBounds;
}

void polishAllBandGains(const PerceptualTarget& target,
                        const FrequencyGrid& targetGrid,
                        const FrequencyGrid& denseGrid,
                        std::vector<PlannedBand>& bands,
                        std::vector<float>& currentResponse,
                        double sampleRate,
                        const SparseParametricFitter::Options& options,
                        std::uint64_t& candidateEvaluations,
                        int& passesUsed)
{
    if (bands.empty() || options.gainPolishPasses <= 0)
        return;

    for (int pass = 0; pass < options.gainPolishPasses; ++pass)
    {
        bool changed = false;
        const float baseStep = pass == 0 ? 0.35f : 0.15f;

        for (std::size_t bandIndex = 0; bandIndex < bands.size(); ++bandIndex)
        {
            const auto oldBand = bands[bandIndex];
            const auto oldBandResponse = bandResponseOnGrid(oldBand, targetGrid, sampleRate);
            const auto withoutBand = subtractResponses(currentResponse, oldBandResponse);
            const auto baselineScore = scoreResponseOnTarget(target, currentResponse);

            PlannedBand bestBand = oldBand;
            std::vector<float> bestBandResponse = oldBandResponse;
            Score bestScore = baselineScore;

            const float adaptiveStep = std::max(baseStep,
                std::min(0.60f, std::abs(oldBand.gainDb) * 0.08f));

            for (float multiplier : { -2.0f, -1.0f, +1.0f, +2.0f })
            {
                PlannedBand trialBand = oldBand;
                trialBand.gainDb = std::clamp(oldBand.gainDb + multiplier * adaptiveStep,
                                              -target.maxCutDb, target.maxBoostDb);
                if (std::abs(trialBand.gainDb - oldBand.gainDb) < 1.0e-5f
                    || std::abs(trialBand.gainDb) < options.minGainDb)
                    continue;

                const auto trialBandResponse = bandResponseOnGrid(trialBand, targetGrid, sampleRate);
                const auto trialResponse = addResponses(withoutBand, trialBandResponse);
                const auto trialScore = scoreResponseOnTarget(target, trialResponse);
                ++candidateEvaluations;

                if (!trialScore.respectsProtection || !trialScore.respectsResponseBounds
                    || trialScore.weightedMse + 1.0e-9f >= bestScore.weightedMse)
                    continue;

                auto trialBands = bands;
                trialBands[bandIndex] = trialBand;
                if (!denseSafe(target, trialBands, denseGrid, sampleRate))
                    continue;

                bestBand = trialBand;
                bestBandResponse = trialBandResponse;
                bestScore = trialScore;
            }

            if (std::abs(bestBand.gainDb - oldBand.gainDb) > 1.0e-5f)
            {
                bands[bandIndex] = bestBand;
                currentResponse = addResponses(withoutBand, bestBandResponse);
                changed = true;
            }
        }

        ++passesUsed;
        if (!changed)
            break;
    }
}

void assignBandProvenance(const PerceptualTarget& target,
                          PlannedBand& band,
                          const FrequencyGrid& targetGrid,
                          double sampleRate)
{
    struct Aggregate
    {
        std::string phrase;
        double score = 0.0;
    };

    std::map<std::string, Aggregate> bySource;
    const auto bandResponse = bandResponseOnGrid(band, targetGrid, sampleRate);

    for (std::size_t i = 0; i < target.points.size(); ++i)
    {
        const auto& point = target.points[i];
        const double localBandInfluence = std::abs(static_cast<double>(bandResponse[i]));
        if (localBandInfluence < 1.0e-6)
            continue;

        for (const auto& contribution : point.contributions)
        {
            if (contribution.sourceId.empty())
                continue;

            auto& aggregate = bySource[contribution.sourceId];
            if (aggregate.phrase.empty())
                aggregate.phrase = contribution.sourcePhrase;
            aggregate.score += localBandInfluence
                             * std::abs(static_cast<double>(contribution.deltaDb))
                             * std::max(0.0f, contribution.confidence)
                             * std::max(0.0f, point.confidence);
        }
    }

    std::vector<std::pair<std::string, Aggregate>> ranked(bySource.begin(), bySource.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b)
    {
        if (std::abs(a.second.score - b.second.score) > 1.0e-12)
            return a.second.score > b.second.score;
        return a.first < b.first;
    });

    double total = 0.0;
    for (const auto& entry : ranked)
        total += entry.second.score;

    band.contributions.clear();
    const std::size_t limit = std::min<std::size_t>(3, ranked.size());
    for (std::size_t i = 0; i < limit; ++i)
    {
        BandContribution contribution;
        contribution.sourceId = ranked[i].first;
        contribution.sourcePhrase = ranked[i].second.phrase;
        contribution.weight = total > 0.0
            ? static_cast<float>(ranked[i].second.score / total)
            : 0.0f;
        band.contributions.push_back(std::move(contribution));
    }

    if (!band.contributions.empty())
    {
        band.reason = !band.contributions.front().sourcePhrase.empty()
            ? band.contributions.front().sourcePhrase
            : band.contributions.front().sourceId;
    }
}

} // namespace

SparseParametricFitter::SparseParametricFitter()
    : SparseParametricFitter(Options{})
{
}

SparseParametricFitter::SparseParametricFitter(Options optionsIn)
    : options(optionsIn)
{
    options.candidateStride = std::max(1, options.candidateStride);
    options.minAbsoluteTargetDb = std::max(0.0f, options.minAbsoluteTargetDb);
    options.minAcceptedImprovementDb = std::max(0.0f, options.minAcceptedImprovementDb);
    options.minGainDb = std::max(0.0f, options.minGainDb);
    options.constraintValidationPointsPerOctave = std::max(
        24, options.constraintValidationPointsPerOctave);
    options.gainPolishPasses = std::max(0, options.gainPolishPasses);
    options.candidateGainRefinementSteps = std::max(0, options.candidateGainRefinementSteps);
}

float SparseParametricFitter::evaluatePlanDb(const std::vector<PlannedBand>& bands,
                                             float frequencyHz,
                                             double sampleRate) noexcept
{
    double magnitude = 1.0;
    for (const auto& band : bands)
        magnitude *= makeCoefficients(band, sampleRate)
                         .getMagnitudeForFrequency(frequencyHz, sampleRate);

    return magnitudeToDb(magnitude);
}

FitResult SparseParametricFitter::fit(const PerceptualTarget& target,
                                      double sampleRate) const
{
    FitResult result;
    if (!target.isValid(sampleRate))
        return result;

    const auto targetGrid = makeTargetGrid(target, sampleRate);
    const auto denseGrid = makeDenseConstraintGrid(
        target, sampleRate, options.constraintValidationPointsPerOctave);

    std::vector<float> currentResponse(target.points.size(), 0.0f);
    const auto initialScore = scoreResponseOnTarget(target, currentResponse);
    result.weightedRmsBeforeDb = initialScore.weightedRms;
    result.weightedRmsAfterDb = initialScore.weightedRms;
    result.valid = true;

    if (target.maxFilters == 0 || target.points.empty()
        || initialScore.weightedRms < options.minAbsoluteTargetDb)
        return result;

    std::vector<PlannedBand> current;
    current.reserve(static_cast<std::size_t>(target.maxFilters));
    Score currentScore = initialScore;

    const int pointCount = static_cast<int>(target.points.size());
    const float nyquistSafe = static_cast<float>(sampleRate * 0.48);

    for (int iteration = 0; iteration < target.maxFilters; ++iteration)
    {
        bool found = false;
        CandidateEvaluation bestCandidate;
        float bestObjective = currentScore.weightedMse;

        for (int pointIndex = 0; pointIndex < pointCount; pointIndex += options.candidateStride)
        {
            const auto& point = target.points[static_cast<std::size_t>(pointIndex)];
            if (point.confidence < target.minConfidence)
                continue;

            const float localResidual = point.deltaDb
                - currentResponse[static_cast<std::size_t>(pointIndex)];
            if (std::abs(localResidual) < options.minAbsoluteTargetDb)
                continue;

            const float candidateFrequency = std::clamp(point.frequencyHz, 20.0f, nyquistSafe);

            auto tryCandidate = [&](FilterType type, float q)
            {
                PlannedBand oneDb;
                oneDb.type = type;
                oneDb.frequencyHz = candidateFrequency;
                oneDb.q = q;
                oneDb.gainDb = 1.0f;
                const auto basis = bandResponseOnGrid(oneDb, targetGrid, sampleRate);

                float gain = estimateLinearizedGain(target, currentResponse, basis);
                gain = std::clamp(gain, -target.maxCutDb, target.maxBoostDb);
                if (!std::isfinite(gain) || std::abs(gain) < options.minGainDb)
                    return;

                auto trial = evaluateCandidate(target, targetGrid, currentResponse,
                                               type, candidateFrequency, q, gain,
                                               sampleRate, options,
                                               result.candidateEvaluations);
                if (!trial.valid)
                    return;

                // RBJ magnitude shape is not perfectly linear in dB gain. Refine
                // only candidates that are already competitive under the exact
                // response, keeping the hot path bounded.
                if (trial.objective < bestObjective + 0.02f)
                    trial = refineCandidateGain(target, targetGrid, currentResponse,
                                                std::move(trial), sampleRate,
                                                options, result.candidateEvaluations);

                if (!trial.valid || trial.objective + 1.0e-9f >= bestObjective)
                    return;

                auto trialBands = current;
                trialBands.push_back(trial.band);
                Score dense;
                if (!denseSafe(target, trialBands, denseGrid, sampleRate, &dense))
                    return;

                trial.score.maxProtectedDeviation = dense.maxProtectedDeviation;
                trial.score.maxResponseBoundViolation = dense.maxResponseBoundViolation;
                bestObjective = trial.objective;
                bestCandidate = std::move(trial);
                found = true;
            };

            for (float q : kPeakQs)
                tryCandidate(FilterType::Peak, q);

            if (candidateFrequency <= 500.0f)
                tryCandidate(FilterType::LowShelf, kShelfQ);
            if (candidateFrequency >= 2500.0f)
                tryCandidate(FilterType::HighShelf, kShelfQ);
        }

        if (!found)
            break;

        const float rmsImprovement = currentScore.weightedRms - bestCandidate.score.weightedRms;
        if (rmsImprovement < options.minAcceptedImprovementDb)
            break;

        current.push_back(bestCandidate.band);
        currentResponse = addResponses(currentResponse, bestCandidate.response);
        currentScore = scoreResponseOnTarget(target, currentResponse);

        polishAllBandGains(target, targetGrid, denseGrid, current, currentResponse,
                           sampleRate, options, result.candidateEvaluations,
                           result.gainPolishPasses);
        currentScore = scoreResponseOnTarget(target, currentResponse);
    }

    Score denseFinal;
    if (!denseSafe(target, current, denseGrid, sampleRate, &denseFinal))
    {
        // This should be unreachable because every accepted insertion/polish is
        // dense-validated. Fail closed rather than return a constraint-breaking
        // plan if numerical drift ever makes the invariant false.
        result.valid = false;
        result.bands.clear();
        return result;
    }

    for (auto& band : current)
        assignBandProvenance(target, band, targetGrid, sampleRate);

    result.reachedBudget = static_cast<int>(current.size()) >= target.maxFilters;
    result.bands = std::move(current);
    result.weightedRmsAfterDb = currentScore.weightedRms;
    result.maxProtectedDeviationDb = denseFinal.maxProtectedDeviation;
    result.maxResponseBoundViolationDb = denseFinal.maxResponseBoundViolation;
    return result;
}

} // namespace AIEQPerceptual
