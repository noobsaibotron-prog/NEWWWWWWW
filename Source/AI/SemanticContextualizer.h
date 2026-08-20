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
        // Schema 4 replaces the single binary gate. Below `lowConfidence` the
        // context has no influence at all; above `highConfidence` it has its
        // full influence; between, it fades. The old gate sat at 0.45, which is
        // the midpoint of this ramp, so the decision point is unchanged - only
        // the cliff either side of it is gone. 0.449 and 0.451 no longer mean
        // text-only versus fully source-aware.
        float lowConfidence = 0.25f;
        float highConfidence = 0.65f;
        float axisFullScaleDb = 6.0f;

        // Full scale for the tilt-corrected Brightness residual. It is a
        // different quantity from axisFullScaleDb, which measures level against
        // the median band, so it does not inherit that number. Measured
        // positive residuals across nine real sources reach +6.8 dB, and the
        // positive side is the one that has to stay resolved because it is the
        // holdback side: a source that reads "already bright" must be RANKED,
        // while a source that reads "dark" only needs to pass the request
        // through unreduced. 9 dB keeps every measured holdback value inside
        // the range instead of on the clamp.
        float brightnessResidualFullScaleDb = 9.0f;

        // Below this, a region's residual is not trusted enough to contribute.
        // The residual extrapolates a fitted slope, and where a region has no
        // content the extrapolation is meaningless rather than merely noisy.
        float minRegionEvidenceForResidual = 0.35f;
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

    /** How much reliable evidence exists for the region this goal depends on.
        A bass has nothing measurable above 10 kHz, so "more air" on a bass must
        find that out - without the bass being declared unanalysable overall,
        which is what a single global confidence forced it to do. */
    [[nodiscard]] float evidenceConfidence(SemanticDimension dimension,
                                           SemanticSpectralFocus focus,
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
