#pragma once

#include "SemanticIntentCompiler.h"
#include "SemanticPlan.h"
#include "SemanticTargetBuilder.h"
#include "SparseParametricFitter.h"
#include "SemanticContextualizer.h"

#include <string_view>
#include <vector>

namespace AIEQPerceptual
{

/** Side-effect-free text -> intent -> target -> sparse-plan pipeline. */
class SemanticPlanner
{
public:
    [[nodiscard]] SemanticPlan plan(std::string_view text,
                                    double sampleRate,
                                    float intensity = 1.0f) const;

    /** T5.3 - the same pipeline with a source context applied between intent
        and target. The context is passed BY VALUE at the call site as an
        immutable snapshot: the planner never reaches into live analysis, which
        is what makes it safe to run on the planning worker. An invalid or
        low-confidence context leaves the intent untouched, so this overload
        degrades exactly into the one above. */
    [[nodiscard]] SemanticPlan plan(std::string_view text,
                                    double sampleRate,
                                    float intensity,
                                    const SpectralContext& context) const;

    /** Phase 5 slice 1 - user Hz fences travel with the request, not the map.
        Invalid ranges are dropped (fail closed). Empty is today's behaviour. */
    [[nodiscard]] SemanticPlan plan(std::string_view text,
                                    double sampleRate,
                                    float intensity,
                                    const SpectralContext& context,
                                    const std::vector<SemanticProtectedRange>& protectedRanges) const;
};

[[nodiscard]] const char* semanticDimensionName(SemanticDimension dimension) noexcept;

} // namespace AIEQPerceptual
