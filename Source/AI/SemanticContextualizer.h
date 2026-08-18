#pragma once

#include "SemanticIntent.h"
#include "SpectralContext.h"

#include <vector>

namespace AIEQPerceptual
{

struct SemanticContextAdjustment
{
    SemanticDimension dimension = SemanticDimension::Brightness;
    float originalAmount = 0.0f;
    float adjustedAmount = 0.0f;
    float scale = 1.0f;
    float axisPosition = 0.0f; // normalized [-1, 1], + = already toward + semantic direction
    bool contextualized = false;
};

struct ContextualizedSemanticIntent
{
    SemanticIntent intent;
    std::vector<SemanticContextAdjustment> adjustments;
    float contextConfidence = 0.0f;
    bool contextApplied = false;
};

/**
 * Conservative deterministic source-awareness layer.
 *
 * It NEVER flips a goal, NEVER edits constraints, and NEVER amplifies a goal
 * beyond the user's requested amount. It may only attenuate goal magnitude when
 * reliable context says the source is already strongly in that direction.
 */
class SemanticContextualizer
{
public:
    struct Options
    {
        float minimumContextConfidence = 0.45f;
        float axisFullScaleDb = 6.0f;
        float deadband = 0.12f;
        float maximumReduction = 0.75f; // at most reduce to 25% of requested amount
    };

    SemanticContextualizer() = default;
    explicit SemanticContextualizer(Options optionsIn) : options(optionsIn) {}

    [[nodiscard]] ContextualizedSemanticIntent contextualize(
        const SemanticIntent& input,
        const SpectralContext& context) const;

    /** Whole-dimension position. Kept as the General-focus case so existing
        callers and tests keep their meaning. */
    [[nodiscard]] float axisPosition(SemanticDimension dimension,
                                     const SpectralContext& context) const noexcept;

    /** Facet-aware position. "more air" and "brighter" are both Brightness but
        must not read the same descriptors: a source can be loud at 3-7 kHz and
        genuinely short above 10 kHz, and answering the first question with the
        second damps exactly the move the user asked for. */
    [[nodiscard]] float axisPosition(SemanticDimension dimension,
                                     SemanticSpectralFocus focus,
                                     const SpectralContext& context) const noexcept;

    /** Diagnostic: how much low-mid congestion argues against adding warmth. */
    [[nodiscard]] float mudRisk(const SpectralContext& context) const noexcept;

private:
    Options options;
};

} // namespace AIEQPerceptual
