#pragma once

#include "PerceptualTarget.h"
#include "SemanticIntent.h"

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
    bool valid = false;
};

} // namespace AIEQPerceptual
