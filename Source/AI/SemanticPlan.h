#pragma once

#include "PerceptualTarget.h"
#include "SemanticIntent.h"
#include "SemanticContextualizer.h"

#include <string>
#include <vector>

namespace AIEQPerceptual
{

enum class GoalOutcomeStatus : int
{
    Achieved = 0,
    Partial,
    ConstraintLimited,
    Unmet
};

struct SemanticGoalOutcome
{
    SemanticDimension dimension = SemanticDimension::Brightness;
    float requestedAmount = 0.0f;
    float achievedFraction = 0.0f; // [0, 1]
    GoalOutcomeStatus status = GoalOutcomeStatus::Unmet;
    std::string limitingSourceId;
    std::string limitingPhrase;
};

struct SemanticPlan
{
    SemanticIntent intent;
    PerceptualTarget target;
    FitResult fit;

    std::vector<SemanticGoalOutcome> goalOutcomes;
    std::string interpretation;
    std::string outcomeSummary;

    /** Set when a valid fit is empty *and* user Hz fences are on the target.
        Distinct from a blank summary, which the panel reports as
        "No meaningful EQ move required" (intensity ~0 / nothing to do). */
    static constexpr const char* kProtectedRegionNoSafeMoveSummary =
        "No safe correction available within the protected region constraints.";

    // T5.3 - what the source context did to this plan. Recorded rather than
    // inferred, so a caller can explain the decision and a test can assert on
    // it without re-deriving the policy.
    float contextConfidence = 0.0f;
    bool  contextApplied = false;
    std::vector<SemanticContextAdjustment> contextAdjustments;

    bool valid = false;
};

} // namespace AIEQPerceptual
