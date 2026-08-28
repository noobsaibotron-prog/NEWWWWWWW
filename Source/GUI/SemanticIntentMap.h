#pragma once

#include "../AI/SemanticPlan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace EmberUI
{

enum class SemanticIntentMapPhase : unsigned char
{
    Hidden = 0,
    Planning,
    Ready,
    Applied,
    NoSafeMove
};

enum class SemanticUiAxis : unsigned char
{
    Air = 0,
    Warmth,
    Punch,
    Clarity,
    Body,
    Brilliance,
    Smooth,
    Weight,
    Count
};

enum class SemanticAxisRole : unsigned char
{
    Neutral = 0,
    Involved,
    Primary
};

struct SemanticFocusRegion
{
    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;
    float strength = 0.0f;
    AIEQPerceptual::SemanticDimension dimension = AIEQPerceptual::SemanticDimension::Brightness;
    AIEQPerceptual::SemanticSpectralFocus focus = AIEQPerceptual::SemanticSpectralFocus::General;
    std::string sourceId;
    std::string sourcePhrase;
    bool primary = false;
};

struct SemanticProtectRegion
{
    float minFrequencyHz = 20.0f;
    float maxFrequencyHz = 20000.0f;
    float confidence = 0.0f;
    AIEQPerceptual::SemanticConstraintKind kind = AIEQPerceptual::SemanticConstraintKind::AvoidDirection;
    std::string sourceId;
    std::string sourcePhrase;
};

struct SemanticBandLink
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    float contributionWeight = 0.0f;
    AIEQPerceptual::SemanticDimension dimension = AIEQPerceptual::SemanticDimension::Brightness;
    AIEQPerceptual::SemanticSpectralFocus focus = AIEQPerceptual::SemanticSpectralFocus::General;
    std::string sourceId;
};

/** One attested target-grid sample of a goal envelope.

    normalizedContribution is local to this envelope (peak = 1). Zero-magnitude
    samples are retained so disjoint lobes stay disjoint under linear interpolation.
*/
struct SemanticEnvelopeSample
{
    float frequencyHz = 1000.0f;
    float normalizedContribution = 0.0f;
    bool withinDisplaySupport = false;
};

/** Projection-only envelope for one selected GoalVisual.

    Ranking/top-2 is owned by the same GoalVisual pass as focusRegions.
    This is derived UI state and is not serialized.
*/
struct SemanticFocusEnvelope
{
    std::vector<SemanticEnvelopeSample> samples;
    float strength = 0.0f;
    AIEQPerceptual::SemanticDimension dimension = AIEQPerceptual::SemanticDimension::Brightness;
    AIEQPerceptual::SemanticSpectralFocus focus = AIEQPerceptual::SemanticSpectralFocus::General;
    std::string sourceId;
    std::string sourcePhrase;
    bool primary = false;
};

struct SemanticIntentMapState
{
    SemanticIntentMapPhase phase = SemanticIntentMapPhase::Hidden;
    std::vector<SemanticFocusRegion> focusRegions;
    std::vector<SemanticProtectRegion> protectRegions;
    std::vector<SemanticBandLink> bandLinks;
    std::array<SemanticAxisRole, static_cast<std::size_t>(SemanticUiAxis::Count)> axisRoles {};
    std::vector<int> appliedBandSlots;
    std::vector<SemanticFocusEnvelope> focusEnvelopes;
    std::string interpretation;
    std::string outcomeSummary;

    [[nodiscard]] bool visible() const noexcept
    {
        return phase == SemanticIntentMapPhase::Ready
            || phase == SemanticIntentMapPhase::Applied;
    }

    [[nodiscard]] bool hasFocus() const noexcept { return !focusRegions.empty(); }
    [[nodiscard]] bool hasProtection() const noexcept { return !protectRegions.empty(); }
};

inline const char* semanticFocusDisplayName (
    AIEQPerceptual::SemanticDimension dimension,
    AIEQPerceptual::SemanticSpectralFocus focus) noexcept
{
    using D = AIEQPerceptual::SemanticDimension;
    using F = AIEQPerceptual::SemanticSpectralFocus;
    if (dimension == D::Brightness)
    {
        if (focus == F::Air) return "Air";
        if (focus == F::Brilliance) return "Brilliance";
        return "Brightness";
    }
    switch (dimension)
    {
        case D::Warmth:     return "Warmth";
        case D::Clarity:    return "Clarity";
        case D::Presence:   return "Presence";
        case D::Smoothness: return "Smoothness";
        case D::Weight:     return "Weight";
        case D::Punch:      return "Punch";
        case D::Tightness:  return "Tightness";
        case D::Brightness:
        case D::Count:      break;
    }
    return "Intent";
}

namespace detail
{
inline std::optional<AIEQPerceptual::SemanticDimension> dimensionFromGoalSourceId (
    std::string_view sourceId) noexcept
{
    using D = AIEQPerceptual::SemanticDimension;
    constexpr std::array<D, static_cast<std::size_t> (D::Count)> dimensions {
        D::Brightness, D::Warmth, D::Clarity, D::Presence,
        D::Smoothness, D::Weight, D::Punch, D::Tightness
    };

    for (const auto dimension : dimensions)
    {
        std::string expected = "goal:";
        expected += AIEQPerceptual::semanticDimensionStableId (dimension);
        if (sourceId == expected)
            return dimension;
    }
    return std::nullopt;
}

inline std::optional<SemanticUiAxis> uiAxisForGoal (
    const AIEQPerceptual::SemanticGoal& goal) noexcept
{
    using D = AIEQPerceptual::SemanticDimension;
    using F = AIEQPerceptual::SemanticSpectralFocus;

    switch (goal.dimension)
    {
        case D::Warmth:     return SemanticUiAxis::Warmth;
        case D::Clarity:    return SemanticUiAxis::Clarity;
        case D::Smoothness: return SemanticUiAxis::Smooth;
        case D::Weight:     return SemanticUiAxis::Weight;
        case D::Punch:      return SemanticUiAxis::Punch;
        case D::Brightness:
            if (goal.focus == F::Air)        return SemanticUiAxis::Air;
            if (goal.focus == F::Brilliance) return SemanticUiAxis::Brilliance;
            return std::nullopt;
        case D::Presence:
        case D::Tightness:
        case D::Count:
            return std::nullopt;
    }
    return std::nullopt;
}

inline AIEQPerceptual::SemanticSpectralFocus focusForDimension (
    const AIEQPerceptual::SemanticIntent& intent,
    AIEQPerceptual::SemanticDimension dimension) noexcept
{
    using F = AIEQPerceptual::SemanticSpectralFocus;
    std::optional<F> focus;
    for (const auto& goal : intent.goals)
    {
        if (goal.dimension != dimension)
            continue;
        if (!focus.has_value())
            focus = goal.focus;
        else if (*focus != goal.focus)
            return F::General;
    }
    return focus.value_or (F::General);
}

inline const AIEQPerceptual::SemanticGoal* goalForDimension (
    const AIEQPerceptual::SemanticIntent& intent,
    AIEQPerceptual::SemanticDimension dimension) noexcept
{
    for (const auto& goal : intent.goals)
        if (goal.dimension == dimension)
            return &goal;
    return nullptr;
}

inline float logSpan (float minHz, float maxHz) noexcept
{
    if (! (minHz > 0.0f) || ! (maxHz > minHz))
        return 0.0f;
    return std::log2 (maxHz / minHz);
}

inline float logOverlapRatio (float aMin, float aMax, float bMin, float bMax) noexcept
{
    const float lo = std::max (aMin, bMin);
    const float hi = std::min (aMax, bMax);
    if (hi <= lo)
        return 0.0f;
    const float overlap = logSpan (lo, hi);
    const float denom = std::max (0.001f, std::min (logSpan (aMin, aMax), logSpan (bMin, bMax)));
    return std::clamp (overlap / denom, 0.0f, 1.0f);
}

inline const AIEQPerceptual::SemanticConstraint* constraintForSourceId (
    const AIEQPerceptual::SemanticIntent& intent,
    std::string_view sourceId) noexcept
{
    for (const auto& c : intent.constraints)
    {
        std::string expected = "constraint:";
        expected += AIEQPerceptual::semanticDimensionStableId (c.dimension);
        if (c.kind == AIEQPerceptual::SemanticConstraintKind::Preserve)
            expected += ":preserve";
        else
            expected += c.direction > 0 ? ":avoid-positive" : ":avoid-negative";
        if (sourceId == expected)
            return &c;
    }
    return nullptr;
}
} // namespace detail

/** Pure UI projection of already-attested SemanticPlan data.

    Focus geometry comes from TargetPoint contributions. Protection comes from
    ResponseBoundRegion. Axis highlights are emitted only for attested mappings.
    No hard-coded "Air = 10 kHz" visual bands.
*/
inline SemanticIntentMapState buildSemanticIntentMapState (
    const AIEQPerceptual::SemanticPlan& plan,
    SemanticIntentMapPhase phase = SemanticIntentMapPhase::Ready)
{
    SemanticIntentMapState out;
    out.phase = phase;
    out.interpretation = plan.interpretation;
    out.outcomeSummary = plan.outcomeSummary;

    if (phase == SemanticIntentMapPhase::Hidden
        || phase == SemanticIntentMapPhase::Planning
        || phase == SemanticIntentMapPhase::NoSafeMove
        || !plan.valid)
        return out;

    struct GoalVisual
    {
        AIEQPerceptual::SemanticDimension dimension;
        AIEQPerceptual::SemanticSpectralFocus focus;
        std::string sourceId;
        std::string sourcePhrase;
        float minHz = 0.0f;
        float maxHz = 0.0f;
        float peakAbsDb = 0.0f;
        float totalAbsDb = 0.0f;
        float score = 0.0f;
        bool valid = false;
    };

    std::vector<GoalVisual> visuals;
    visuals.reserve (plan.intent.goals.size());

    for (const auto& goal : plan.intent.goals)
    {
        const std::string sourceId = std::string ("goal:")
            + AIEQPerceptual::semanticDimensionStableId (goal.dimension);
        if (std::any_of (visuals.begin(), visuals.end(), [&] (const GoalVisual& v)
            { return v.sourceId == sourceId; }))
            continue;

        GoalVisual visual;
        visual.dimension = goal.dimension;
        visual.focus = detail::focusForDimension (plan.intent, goal.dimension);
        visual.sourceId = sourceId;
        if (const auto* representative = detail::goalForDimension (plan.intent, goal.dimension))
        {
            visual.sourcePhrase = representative->sourcePhrase;
            visual.score = std::abs (representative->amount) * representative->confidence;
        }

        std::vector<std::pair<float, float>> samples;
        samples.reserve (plan.target.points.size());
        for (const auto& point : plan.target.points)
        {
            float contributionAbs = 0.0f;
            for (const auto& contribution : point.contributions)
            {
                if (contribution.sourceId == sourceId)
                    contributionAbs += std::abs (contribution.deltaDb);
            }
            if (contributionAbs > 1.0e-5f)
            {
                samples.emplace_back (point.frequencyHz, contributionAbs);
                visual.peakAbsDb = std::max (visual.peakAbsDb, contributionAbs);
                visual.totalAbsDb += contributionAbs;
            }
        }

        if (visual.peakAbsDb <= 1.0e-5f || samples.empty())
            continue;

        const float threshold = std::max (0.05f, visual.peakAbsDb * 0.25f);
        for (const auto& [frequency, magnitude] : samples)
        {
            if (magnitude < threshold)
                continue;
            if (!visual.valid)
            {
                visual.minHz = visual.maxHz = frequency;
                visual.valid = true;
            }
            else
            {
                visual.minHz = std::min (visual.minHz, frequency);
                visual.maxHz = std::max (visual.maxHz, frequency);
            }
        }

        if (visual.valid)
        {
            if (visual.maxHz <= visual.minHz * 1.01f)
            {
                const float center = visual.minHz;
                visual.minHz = std::max (20.0f, center / std::sqrt (2.0f));
                visual.maxHz = std::min (20000.0f, center * std::sqrt (2.0f));
            }
            visual.score *= (0.5f + 0.5f * std::min (1.0f, visual.totalAbsDb / 3.0f));
            visuals.push_back (std::move (visual));
        }
    }

    std::sort (visuals.begin(), visuals.end(), [] (const GoalVisual& a, const GoalVisual& b)
    {
        return a.score > b.score;
    });

    const float strongestScore = visuals.empty() ? 0.0f : std::max (0.001f, visuals.front().score);
    const std::size_t focusCount = std::min<std::size_t> (2, visuals.size());
    for (std::size_t i = 0; i < focusCount; ++i)
    {
        const auto& v = visuals[i];
        SemanticFocusRegion region;
        region.minFrequencyHz = v.minHz;
        region.maxFrequencyHz = v.maxHz;
        region.strength = std::clamp (v.score / strongestScore, 0.0f, 1.0f);
        region.dimension = v.dimension;
        region.focus = v.focus;
        region.sourceId = v.sourceId;
        region.sourcePhrase = v.sourcePhrase;
        region.primary = (i == 0);
        out.focusRegions.push_back (std::move (region));
    }

    float bestAxisScore = -1.0f;
    std::optional<SemanticUiAxis> bestAxis;
    for (const auto& goal : plan.intent.goals)
    {
        const auto axis = detail::uiAxisForGoal (goal);
        if (!axis.has_value())
            continue;
        const auto idx = static_cast<std::size_t> (*axis);
        out.axisRoles[idx] = SemanticAxisRole::Involved;
        const float score = std::abs (goal.amount) * goal.confidence;
        if (score > bestAxisScore)
        {
            bestAxisScore = score;
            bestAxis = axis;
        }
    }
    if (bestAxis.has_value())
        out.axisRoles[static_cast<std::size_t> (*bestAxis)] = SemanticAxisRole::Primary;

    std::string limitingSource;
    for (const auto& outcome : plan.goalOutcomes)
    {
        if (outcome.status == AIEQPerceptual::GoalOutcomeStatus::ConstraintLimited
            && !outcome.limitingSourceId.empty())
        {
            limitingSource = outcome.limitingSourceId;
            break;
        }
    }

    const AIEQPerceptual::ResponseBoundRegion* bestBound = nullptr;
    float bestBoundScore = -1.0f;
    for (const auto& bound : plan.target.responseBounds)
    {
        float score = std::clamp (bound.confidence, 0.0f, 1.0f);
        if (!limitingSource.empty() && bound.sourceId == limitingSource)
            score += 10.0f;
        for (const auto& focus : out.focusRegions)
            score += 4.0f * detail::logOverlapRatio (
                bound.minFrequencyHz, bound.maxFrequencyHz,
                focus.minFrequencyHz, focus.maxFrequencyHz);
        if (score > bestBoundScore)
        {
            bestBoundScore = score;
            bestBound = &bound;
        }
    }

    if (bestBound != nullptr)
    {
        SemanticProtectRegion region;
        region.minFrequencyHz = bestBound->minFrequencyHz;
        region.maxFrequencyHz = bestBound->maxFrequencyHz;
        region.confidence = bestBound->confidence;
        region.sourceId = bestBound->sourceId;
        region.sourcePhrase = bestBound->sourcePhrase;
        if (const auto* c = detail::constraintForSourceId (plan.intent, bestBound->sourceId))
            region.kind = c->kind;
        out.protectRegions.push_back (std::move (region));
    }

    for (const auto& band : plan.fit.bands)
    {
        const AIEQPerceptual::BandContribution* strongest = nullptr;
        for (const auto& contribution : band.contributions)
        {
            if (!detail::dimensionFromGoalSourceId (contribution.sourceId).has_value())
                continue;
            if (strongest == nullptr || contribution.weight > strongest->weight)
                strongest = &contribution;
        }
        if (strongest == nullptr)
            continue;

        const auto dimension = *detail::dimensionFromGoalSourceId (strongest->sourceId);
        SemanticBandLink link;
        link.frequencyHz = band.frequencyHz;
        link.gainDb = band.gainDb;
        link.q = band.q;
        link.contributionWeight = std::clamp (strongest->weight, 0.0f, 1.0f);
        link.dimension = dimension;
        link.focus = detail::focusForDimension (plan.intent, dimension);
        link.sourceId = strongest->sourceId;
        out.bandLinks.push_back (std::move (link));
    }

    return out;
}

} // namespace EmberUI
