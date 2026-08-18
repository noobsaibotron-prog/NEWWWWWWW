#include "SemanticTargetBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace AIEQPerceptual
{
namespace
{
struct Lobe
{
    float minFrequencyHz;
    float maxFrequencyHz;
    int positiveDirectionResponseSign; // response sign for + semantic direction
};

float gaussianOctaves(float frequencyHz, float centerHz, float sigmaOctaves) noexcept
{
    const float x = std::log2(std::max(1.0f, frequencyHz) / centerHz);
    return std::exp(-0.5f * (x * x) / (sigmaOctaves * sigmaOctaves));
}

float highShelfShape(float frequencyHz, float pivotHz, float widthOctaves) noexcept
{
    const float x = std::log2(std::max(1.0f, frequencyHz) / pivotHz) / widthOctaves;
    return 0.5f * (1.0f + std::tanh(x));
}

float lowShelfShape(float frequencyHz, float pivotHz, float widthOctaves) noexcept
{
    return 1.0f - highShelfShape(frequencyHz, pivotHz, widthOctaves);
}

/**
 * Where a brightness request should put its energy.
 *
 * Until T4.2 every brightness goal produced the same 6 kHz shelf, so "brighter",
 * "more brilliance" and "more air" differed only in HOW MUCH was applied, never
 * in WHERE. The contextualizer had already learned to tell them apart; the
 * curve had not, which meant an air request could still be answered with a
 * boost starting at 4-6 kHz.
 *
 * General keeps the original shelf exactly, so existing behaviour is unchanged
 * for whole-dimension requests. The named facets are placed inside the
 * perceptual regions they refer to: Presence 2-5 kHz, Brilliance 5-10 kHz, Air
 * 10 kHz and up. Air stays a shelf because air genuinely extends to the top;
 * Brilliance is a lobe because it has air above it.
 */
float brightnessShapeDb(SemanticSpectralFocus focus, float frequencyHz) noexcept
{
    switch (focus)
    {
        case SemanticSpectralFocus::Presence:
            return 2.35f * gaussianOctaves(frequencyHz, 3200.0f, 0.82f);
        case SemanticSpectralFocus::Brilliance:
            return 2.50f * gaussianOctaves(frequencyHz, 7000.0f, 0.75f);
        case SemanticSpectralFocus::Air:
            return 2.50f * highShelfShape(frequencyHz, 11000.0f, 0.55f);
        case SemanticSpectralFocus::General:
        case SemanticSpectralFocus::LowMid:
        case SemanticSpectralFocus::Bass:
        case SemanticSpectralFocus::Sub:
            break;
    }
    return 2.50f * highShelfShape(frequencyHz, 6000.0f, 0.70f);
}

float dimensionShapeImpl(SemanticDimension dimension, float frequencyHz,
                         SemanticSpectralFocus focus) noexcept
{
    switch (dimension)
    {
        case SemanticDimension::Brightness:
            // The only dimension with facets today. The others ignore focus
            // rather than guess at a geometry nothing has asked for.
            return brightnessShapeDb(focus, frequencyHz);

        case SemanticDimension::Warmth:
            return 2.50f * gaussianOctaves(frequencyHz, 190.0f, 0.85f);

        case SemanticDimension::Clarity:
            return -2.10f * gaussianOctaves(frequencyHz, 320.0f, 0.75f)
                   + 0.95f * gaussianOctaves(frequencyHz, 3000.0f, 0.95f);

        case SemanticDimension::Presence:
            return 2.35f * gaussianOctaves(frequencyHz, 3200.0f, 0.82f);

        case SemanticDimension::Smoothness:
            return -2.45f * gaussianOctaves(frequencyHz, 3300.0f, 0.78f)
                   - 0.35f * highShelfShape(frequencyHz, 8500.0f, 0.90f);

        case SemanticDimension::Weight:
            return 2.60f * lowShelfShape(frequencyHz, 95.0f, 0.72f);

        case SemanticDimension::Punch:
            return 2.10f * gaussianOctaves(frequencyHz, 2900.0f, 0.72f);

        case SemanticDimension::Tightness:
            return -2.30f * gaussianOctaves(frequencyHz, 145.0f, 0.62f);

        case SemanticDimension::Count:
            break;
    }
    return 0.0f;
}

std::vector<Lobe> lobesFor(SemanticDimension dimension)
{
    switch (dimension)
    {
        case SemanticDimension::Brightness: return { { 5000.0f, 18000.0f, +1 } };
        case SemanticDimension::Warmth:     return { { 100.0f,   450.0f,   +1 } };
        case SemanticDimension::Clarity:    return { { 250.0f,   500.0f,   -1 },
                                                        { 1800.0f,  5000.0f,  +1 } };
        case SemanticDimension::Presence:   return { { 1800.0f,  6000.0f,  +1 } };
        case SemanticDimension::Smoothness: return { { 1800.0f,  6500.0f,  -1 } };
        case SemanticDimension::Weight:     return { { 30.0f,    135.0f,   +1 } };
        case SemanticDimension::Punch:      return { { 1800.0f,  5000.0f,  +1 } };
        case SemanticDimension::Tightness:  return { { 80.0f,    240.0f,   -1 } };
        case SemanticDimension::Count:      break;
    }
    return {};
}

std::string goalSourceId(SemanticDimension dimension)
{
    return std::string("goal:") + semanticDimensionStableId(dimension);
}

std::string constraintSourceId(const SemanticConstraint& constraint)
{
    std::string id = "constraint:";
    id += semanticDimensionStableId(constraint.dimension);
    if (constraint.kind == SemanticConstraintKind::Preserve)
        id += ":preserve";
    else
        id += constraint.direction > 0 ? ":avoid-positive" : ":avoid-negative";
    return id;
}

void addConstraintBounds(PerceptualTarget& target,
                         const SemanticConstraint& constraint,
                         float highFrequencyHz,
                         float preserveToleranceDb,
                         float avoidToleranceDb)
{
    for (const auto& lobe : lobesFor(constraint.dimension))
    {
        const float low = std::max(20.0f, lobe.minFrequencyHz);
        const float high = std::min(highFrequencyHz, lobe.maxFrequencyHz);
        if (high <= low)
            continue;

        ResponseBoundRegion bound;
        bound.minFrequencyHz = low;
        bound.maxFrequencyHz = high;
        bound.sourceId = constraintSourceId(constraint);
        bound.sourcePhrase = constraint.sourcePhrase;
        bound.confidence = constraint.confidence;

        if (constraint.kind == SemanticConstraintKind::Preserve)
        {
            bound.minDeltaDb = -preserveToleranceDb;
            bound.maxDeltaDb = preserveToleranceDb;
        }
        else
        {
            const int forbiddenResponseSign = constraint.direction * lobe.positiveDirectionResponseSign;
            if (forbiddenResponseSign > 0)
            {
                // Do not move positively in this lobe, but cuts remain available.
                bound.minDeltaDb = -24.0f;
                bound.maxDeltaDb = avoidToleranceDb;
            }
            else
            {
                // Do not move negatively in this lobe, but boosts remain available.
                bound.minDeltaDb = -avoidToleranceDb;
                bound.maxDeltaDb = 24.0f;
            }
        }

        target.responseBounds.push_back(std::move(bound));
    }
}

const ResponseBoundRegion* limitingBoundAtFrequency(
    float frequencyHz,
    const std::vector<ResponseBoundRegion>& bounds,
    float before,
    float after) noexcept
{
    if (std::abs(after - before) < 1.0e-6f)
        return nullptr;

    const bool upperClamp = after < before;
    const ResponseBoundRegion* best = nullptr;

    for (const auto& bound : bounds)
    {
        if (frequencyHz < bound.minFrequencyHz || frequencyHz > bound.maxFrequencyHz)
            continue;

        if (upperClamp)
        {
            if (before <= bound.maxDeltaDb + 1.0e-6f)
                continue;
            if (best == nullptr || bound.maxDeltaDb < best->maxDeltaDb)
                best = &bound;
        }
        else
        {
            if (before >= bound.minDeltaDb - 1.0e-6f)
                continue;
            if (best == nullptr || bound.minDeltaDb > best->minDeltaDb)
                best = &bound;
        }
    }

    return best;
}

} // namespace

float SemanticTargetBuilder::evaluateDimensionShapeDb(
    SemanticDimension dimension, SemanticSpectralFocus focus, float frequencyHz) noexcept
{
    return dimensionShapeImpl(dimension, frequencyHz, focus);
}

float SemanticTargetBuilder::evaluateDimensionShapeDb(
    SemanticDimension dimension, float frequencyHz) noexcept
{
    return dimensionShapeImpl(dimension, frequencyHz, SemanticSpectralFocus::General);
}

const char* SemanticTargetBuilder::sourceIdForDimension(
    SemanticDimension dimension) noexcept
{
    switch (dimension)
    {
        case SemanticDimension::Brightness: return "goal:brightness";
        case SemanticDimension::Warmth:     return "goal:warmth";
        case SemanticDimension::Clarity:    return "goal:clarity";
        case SemanticDimension::Presence:   return "goal:presence";
        case SemanticDimension::Smoothness: return "goal:smoothness";
        case SemanticDimension::Weight:     return "goal:weight";
        case SemanticDimension::Punch:      return "goal:punch";
        case SemanticDimension::Tightness:  return "goal:tightness";
        case SemanticDimension::Count:      break;
    }
    return "goal:unknown";
}

SemanticTargetBuilder::SemanticTargetBuilder()
    : SemanticTargetBuilder(Options{})
{
}

SemanticTargetBuilder::SemanticTargetBuilder(Options optionsIn)
    : options(optionsIn)
{
    options.pointsPerOctave = std::max(6, options.pointsPerOctave);
    options.maxFilters = std::max(0, options.maxFilters);
    options.maxBoostDb = std::max(0.0f, options.maxBoostDb);
    options.maxCutDb = std::max(0.0f, options.maxCutDb);
    options.preserveToleranceDb = std::max(0.0f, options.preserveToleranceDb);
    options.avoidToleranceDb = std::max(0.0f, options.avoidToleranceDb);
}

PerceptualTarget SemanticTargetBuilder::build(const SemanticIntent& intent,
                                              double sampleRate) const
{
    PerceptualTarget target;
    if (!intent.isValid() || intent.contradictory
        || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return target;

    const float nyquistSafe = static_cast<float>(sampleRate * 0.45);
    const float low = std::max(20.0f, options.lowFrequencyHz);
    const float high = std::min(options.highFrequencyHz, nyquistSafe);
    if (high <= low)
        return target;

    target.maxFilters = options.maxFilters;
    target.maxBoostDb = options.maxBoostDb;
    target.maxCutDb = options.maxCutDb;
    target.minConfidence = 0.05f;

    for (const auto& constraint : intent.constraints)
        addConstraintBounds(target, constraint, high,
                            options.preserveToleranceDb, options.avoidToleranceDb);

    const int count = static_cast<int>(std::ceil(std::log2(high / low)
                                                 * options.pointsPerOctave));
    target.points.reserve(static_cast<std::size_t>(count + 1));

    for (int i = 0; i <= count; ++i)
    {
        const float frequency = low * std::pow(2.0f,
            static_cast<float>(i) / static_cast<float>(options.pointsPerOctave));
        if (frequency > high)
            break;

        TargetPoint point;
        point.frequencyHz = frequency;
        point.confidence = intent.goals.empty() ? 0.70f : 0.90f;

        float rawDeltaDb = 0.0f;
        for (const auto& goal : intent.goals)
        {
            const float contributionDb = goal.amount
                * evaluateDimensionShapeDb(goal.dimension, goal.focus, frequency);
            rawDeltaDb += contributionDb;
            point.confidence = std::min(point.confidence, goal.confidence);

            TargetContribution contribution;
            contribution.sourceId = goalSourceId(goal.dimension);
            contribution.sourcePhrase = goal.sourcePhrase;
            contribution.deltaDb = contributionDb;
            contribution.confidence = goal.confidence;
            point.contributions.push_back(std::move(contribution));
        }

        float deltaDb = std::clamp(rawDeltaDb, -target.maxCutDb, target.maxBoostDb);
        if (std::abs(deltaDb - rawDeltaDb) > 1.0e-6f)
        {
            point.contributions.push_back({ "safety:global-gain-clamp", "global gain safety",
                                            deltaDb - rawDeltaDb, 1.0f });
        }

        float minBound = -24.0f;
        float maxBound = 24.0f;
        if (getResponseBoundsAtFrequency(frequency, target.responseBounds,
                                         minBound, maxBound))
        {
            const float beforeConstraint = deltaDb;
            deltaDb = std::clamp(deltaDb, minBound, maxBound);
            if (const auto* limiting = limitingBoundAtFrequency(
                    frequency, target.responseBounds, beforeConstraint, deltaDb))
            {
                point.contributions.push_back({ limiting->sourceId,
                                                limiting->sourcePhrase,
                                                deltaDb - beforeConstraint,
                                                limiting->confidence });
            }
        }

        point.deltaDb = deltaDb;
        target.points.push_back(std::move(point));
    }

    return target;
}

} // namespace AIEQPerceptual
