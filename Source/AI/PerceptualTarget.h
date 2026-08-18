#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace AIEQPerceptual
{

/** Versioned contract shared by Semantic and Match target builders. */
inline constexpr std::uint32_t kPerceptualTargetSchemaVersion = 1;

enum class TargetDeltaDomain : std::uint8_t
{
    // Every target point is a RELATIVE EQ response in dB after any builder-side
    // level/shape normalization. It is never an absolute source/reference level.
    RelativeEqDeltaDb = 1
};

enum class FilterType : int
{
    LowShelf = 1,
    Peak = 2,
    HighShelf = 3
};

/** Generic provenance contribution attached to one target point.
    Semantic uses stable goal/constraint IDs; Match can later use reference IDs
    without coupling the fitter to SemanticDimension. */
struct TargetContribution
{
    std::string sourceId;
    std::string sourcePhrase;
    float deltaDb = 0.0f;
    float confidence = 1.0f;
};

struct TargetPoint
{
    TargetPoint() = default;
    TargetPoint(float frequency, float delta, float confidenceIn) noexcept
        : frequencyHz(frequency), deltaDb(delta), confidence(confidenceIn) {}

    float frequencyHz = 1000.0f;
    float deltaDb = 0.0f;
    float confidence = 1.0f;
    std::vector<TargetContribution> contributions;
};

struct ProtectedRegion
{
    ProtectedRegion() = default;
    ProtectedRegion(float minFrequency, float maxFrequency, float maxAbsDelta) noexcept
        : minFrequencyHz(minFrequency), maxFrequencyHz(maxFrequency),
          maxAbsDeltaDb(maxAbsDelta) {}

    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;

    // Hard safety contract: the FINAL accepted plan is validated on a denser
    // log-frequency grid, not only at TargetPoint frequencies.
    float maxAbsDeltaDb = 0.25f;

    std::string sourceId;
    std::string sourcePhrase;
};

// Directional hard constraint. Unlike ProtectedRegion this can allow movement
// in one direction while forbidding the other. Example: "without mud" can
// permit a low-mid cut while preventing a low-mid boost.
struct ResponseBoundRegion
{
    ResponseBoundRegion() = default;
    ResponseBoundRegion(float minFrequency, float maxFrequency,
                        float minDelta, float maxDelta) noexcept
        : minFrequencyHz(minFrequency), maxFrequencyHz(maxFrequency),
          minDeltaDb(minDelta), maxDeltaDb(maxDelta) {}

    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;
    float minDeltaDb = -24.0f;
    float maxDeltaDb = 24.0f;

    std::string sourceId;
    std::string sourcePhrase;
    float confidence = 1.0f;
};

struct PerceptualTarget
{
    std::uint32_t schemaVersion = kPerceptualTargetSchemaVersion;
    TargetDeltaDomain deltaDomain = TargetDeltaDomain::RelativeEqDeltaDb;

    std::vector<TargetPoint> points;
    std::vector<ProtectedRegion> protectedRegions;
    std::vector<ResponseBoundRegion> responseBounds;

    // Global safety/budget constraints shared by Semantic and Match builders.
    // Once a builder emits PerceptualTarget these fields are the single
    // authority. The fitter must not consult competing external gain limits.
    int maxFilters = 6;
    float maxBoostDb = 6.0f;
    float maxCutDb = 6.0f;
    float minConfidence = 0.05f;

    [[nodiscard]] bool isValid(double sampleRate) const noexcept
    {
        if (schemaVersion != kPerceptualTargetSchemaVersion
            || deltaDomain != TargetDeltaDomain::RelativeEqDeltaDb
            || !std::isfinite(sampleRate) || sampleRate <= 0.0 || maxFilters < 0
            || !std::isfinite(maxBoostDb) || !std::isfinite(maxCutDb)
            || !std::isfinite(minConfidence)
            || maxBoostDb < 0.0f || maxCutDb < 0.0f
            || minConfidence < 0.0f || minConfidence > 1.0f)
            return false;

        const float nyquist = static_cast<float>(sampleRate * 0.5);
        float previousFrequency = 0.0f;

        for (const auto& p : points)
        {
            if (!std::isfinite(p.frequencyHz) || !std::isfinite(p.deltaDb)
                || !std::isfinite(p.confidence)
                || p.frequencyHz <= 0.0f || p.frequencyHz >= nyquist
                || p.frequencyHz <= previousFrequency
                || p.confidence < 0.0f || p.confidence > 1.0f)
                return false;

            for (const auto& contribution : p.contributions)
                if (!std::isfinite(contribution.deltaDb)
                    || !std::isfinite(contribution.confidence)
                    || contribution.confidence < 0.0f || contribution.confidence > 1.0f)
                    return false;

            previousFrequency = p.frequencyHz;
        }

        for (const auto& r : protectedRegions)
        {
            if (!std::isfinite(r.minFrequencyHz) || !std::isfinite(r.maxFrequencyHz)
                || !std::isfinite(r.maxAbsDeltaDb)
                || r.minFrequencyHz <= 0.0f
                || r.maxFrequencyHz <= r.minFrequencyHz
                || r.minFrequencyHz >= nyquist
                || r.maxAbsDeltaDb < 0.0f)
                return false;
        }

        for (const auto& r : responseBounds)
        {
            if (!std::isfinite(r.minFrequencyHz) || !std::isfinite(r.maxFrequencyHz)
                || !std::isfinite(r.minDeltaDb) || !std::isfinite(r.maxDeltaDb)
                || !std::isfinite(r.confidence)
                || r.minFrequencyHz <= 0.0f
                || r.maxFrequencyHz <= r.minFrequencyHz
                || r.minFrequencyHz >= nyquist
                || r.maxDeltaDb < r.minDeltaDb
                || r.confidence < 0.0f || r.confidence > 1.0f)
                return false;
        }

        return true;
    }
};

struct BandContribution
{
    std::string sourceId;
    std::string sourcePhrase;
    float weight = 0.0f; // normalized [0, 1], descending in PlannedBand
};

struct PlannedBand
{
    PlannedBand() = default;
    PlannedBand(FilterType typeIn, float frequency, float gain, float qIn,
                float confidenceIn, std::vector<BandContribution> contributionsIn = {})
        : type(typeIn), frequencyHz(frequency), gainDb(gain), q(qIn),
          confidence(confidenceIn), contributions(std::move(contributionsIn)) {}

    FilterType type = FilterType::Peak;
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    float confidence = 1.0f;

    // Generic provenance. The fitter does not know what "Warmth" or a Match
    // reference means; it only preserves stable target source IDs.
    std::vector<BandContribution> contributions;
    std::string reason;
};

struct FitResult
{
    std::vector<PlannedBand> bands;
    float weightedRmsBeforeDb = 0.0f;
    float weightedRmsAfterDb = 0.0f;
    float maxProtectedDeviationDb = 0.0f;
    float maxResponseBoundViolationDb = 0.0f;
    bool reachedBudget = false;
    bool valid = false;

    // Deterministic diagnostics useful for performance/regression gates.
    std::uint64_t candidateEvaluations = 0;
    int gainPolishPasses = 0;
};

inline bool isFrequencyProtected(float frequencyHz,
                                 const std::vector<ProtectedRegion>& regions,
                                 float& maxAbsDeltaDb) noexcept
{
    bool protectedHere = false;
    maxAbsDeltaDb = 0.0f;

    for (const auto& r : regions)
    {
        if (frequencyHz >= r.minFrequencyHz && frequencyHz <= r.maxFrequencyHz)
        {
            if (!protectedHere)
                maxAbsDeltaDb = r.maxAbsDeltaDb;
            else
                maxAbsDeltaDb = std::min(maxAbsDeltaDb, r.maxAbsDeltaDb);
            protectedHere = true;
        }
    }

    return protectedHere;
}

inline bool getResponseBoundsAtFrequency(float frequencyHz,
                                         const std::vector<ResponseBoundRegion>& regions,
                                         float& minDeltaDb,
                                         float& maxDeltaDb) noexcept
{
    bool bounded = false;
    minDeltaDb = -24.0f;
    maxDeltaDb = 24.0f;

    for (const auto& r : regions)
    {
        if (frequencyHz >= r.minFrequencyHz && frequencyHz <= r.maxFrequencyHz)
        {
            minDeltaDb = std::max(minDeltaDb, r.minDeltaDb);
            maxDeltaDb = std::min(maxDeltaDb, r.maxDeltaDb);
            bounded = true;
        }
    }

    return bounded;
}

} // namespace AIEQPerceptual
