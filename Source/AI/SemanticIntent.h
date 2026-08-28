#pragma once

#include <cstdint>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace AIEQPerceptual
{

enum class SemanticDimension : int
{
    Brightness = 0,
    Warmth,
    Clarity,
    Presence,
    Smoothness,
    Weight,
    Punch,
    Tightness,
    Count
};

inline constexpr const char* semanticDimensionStableId(SemanticDimension dimension) noexcept
{
    switch (dimension)
    {
        case SemanticDimension::Brightness: return "brightness";
        case SemanticDimension::Warmth:     return "warmth";
        case SemanticDimension::Clarity:    return "clarity";
        case SemanticDimension::Presence:   return "presence";
        case SemanticDimension::Smoothness: return "smoothness";
        case SemanticDimension::Weight:     return "weight";
        case SemanticDimension::Punch:      return "punch";
        case SemanticDimension::Tightness:  return "tightness";
        case SemanticDimension::Count:      break;
    }
    return "unknown";
}

enum class SemanticConstraintKind : int
{
    // Prevent movement toward one semantic direction while still allowing
    // movement in the opposite direction. Example: "without harshness".
    AvoidDirection = 0,

    // Preserve the current tonal region around neutral. Example: "keep my sub".
    Preserve,

    // User-authored frequency fence. Never produced by the text compiler.
    // Lives on SemanticProtectedRange, not on SemanticConstraint, so the
    // dimension/direction fields of Preserve/Avoid are not overloaded.
    ProtectRange
};

/**
 * Which part of a dimension the phrase actually named.
 *
 * "brighter" and "more air" are both Brightness requests, but they are not the
 * same request: a source can be loud at 3-7 kHz and genuinely short of 10 kHz+,
 * and treating the second as the first damps exactly the boost the user asked
 * for. A facet keeps that distinction without multiplying the artistic
 * dimensions, which would destabilise the compiler/builder contracts that are
 * already tested.
 */
enum class SemanticSpectralFocus : std::uint8_t
{
    General = 0,
    Air,
    Brilliance,
    Presence,
    LowMid,
    Bass,
    Sub
};

struct SemanticGoal
{
    SemanticDimension dimension = SemanticDimension::Brightness;
    float amount = 0.0f;      // signed [-1, 1]
    float confidence = 1.0f;  // [0, 1]
    std::string sourcePhrase;

    // Last on purpose: existing aggregate initialisers stay valid and keep
    // defaulting to General, so adding the facet cannot silently reorder a
    // call site.
    SemanticSpectralFocus focus = SemanticSpectralFocus::General;
};

struct SemanticConstraint
{
    SemanticDimension dimension = SemanticDimension::Brightness;
    SemanticConstraintKind kind = SemanticConstraintKind::AvoidDirection;

    // Only meaningful for AvoidDirection. +1 means avoid the positive semantic
    // direction (e.g. avoid more brightness); -1 means avoid the negative
    // direction (e.g. avoid harshness on the Smoothness axis).
    int direction = 1;
    float confidence = 1.0f;
    std::string sourcePhrase;
};

/** User-authored Hz fence. Symmetric |Δ| cap, not a semantic-axis Preserve.

    Enters planning on SemanticPlanningRequest, is copied onto SemanticIntent,
    then becomes PerceptualTarget::protectedRegions. Not a SemanticConstraint:
    Preserve/Avoid have no frequency fields and must keep failing closed on
    same-axis text fights.
*/
struct SemanticProtectedRange
{
    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;
    float maxAbsDeltaDb = 0.25f;
    float confidence = 1.0f;
    std::string sourcePhrase;
    std::string sourceId;

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite(minFrequencyHz) && std::isfinite(maxFrequencyHz)
            && std::isfinite(maxAbsDeltaDb) && std::isfinite(confidence)
            && minFrequencyHz > 0.0f && maxFrequencyHz > minFrequencyHz
            && maxAbsDeltaDb >= 0.0f
            && confidence >= 0.0f && confidence <= 1.0f;
    }
};

inline constexpr const char* kUserProtectSourceIdPrefix = "constraint:user-protect:";

[[nodiscard]] inline std::string makeUserProtectSourceId(std::size_t index)
{
    return std::string(kUserProtectSourceIdPrefix) + std::to_string(index);
}

struct SemanticIntent
{
    std::vector<SemanticGoal> goals;
    std::vector<SemanticConstraint> constraints;
    std::vector<SemanticProtectedRange> protectedRanges;

    float confidence = 0.0f;
    bool contradictory = false;

    // More precise diagnostic than contradictory alone: a valid goal is
    // directly forbidden by a Preserve/AvoidDirection constraint on the same
    // semantic axis (e.g. "more weight, don't touch the low end").
    bool goalConstraintConflict = false;
    bool hasRecognizedContent = false;

    [[nodiscard]] bool isValid() const noexcept
    {
        for (const auto& goal : goals)
        {
            const auto dimension = static_cast<int>(goal.dimension);
            if (dimension < 0 || dimension >= static_cast<int>(SemanticDimension::Count)
                || !std::isfinite(goal.amount) || !std::isfinite(goal.confidence)
                || goal.amount < -1.0f || goal.amount > 1.0f
                || goal.confidence < 0.0f || goal.confidence > 1.0f)
                return false;
        }

        for (const auto& constraint : constraints)
        {
            const auto dimension = static_cast<int>(constraint.dimension);
            if (dimension < 0 || dimension >= static_cast<int>(SemanticDimension::Count)
                || !std::isfinite(constraint.confidence)
                || constraint.confidence < 0.0f || constraint.confidence > 1.0f
                || (constraint.kind == SemanticConstraintKind::AvoidDirection
                    && constraint.direction != -1 && constraint.direction != 1))
                return false;
        }

        for (const auto& range : protectedRanges)
            if (!range.isValid())
                return false;

        return std::isfinite(confidence) && confidence >= 0.0f && confidence <= 1.0f;
    }
};

} // namespace AIEQPerceptual
