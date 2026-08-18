#pragma once

#include "SemanticIntentCompiler.h"
#include "SemanticPlan.h"
#include "SemanticTargetBuilder.h"
#include "SparseParametricFitter.h"

#include <string_view>

namespace AIEQPerceptual
{

/** Side-effect-free text -> intent -> target -> sparse-plan pipeline. */
class SemanticPlanner
{
public:
    [[nodiscard]] SemanticPlan plan(std::string_view text,
                                    double sampleRate,
                                    float intensity = 1.0f) const;
};

[[nodiscard]] const char* semanticDimensionName(SemanticDimension dimension) noexcept;

} // namespace AIEQPerceptual
