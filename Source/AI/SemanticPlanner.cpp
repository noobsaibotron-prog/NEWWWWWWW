#include "SemanticPlanner.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace AIEQPerceptual
{
namespace
{
std::string signedGoalName(const SemanticGoal& goal)
{
    const bool positive = goal.amount >= 0.0f;
    switch (goal.dimension)
    {
        case SemanticDimension::Brightness: return positive ? "brighter" : "darker";
        case SemanticDimension::Warmth:     return positive ? "warmer" : "thinner";
        case SemanticDimension::Clarity:    return positive ? "clearer" : "muddier";
        case SemanticDimension::Presence:   return positive ? "more presence" : "less presence";
        case SemanticDimension::Smoothness: return positive ? "smoother" : "harsher";
        case SemanticDimension::Weight:     return positive ? "more weight" : "less weight";
        case SemanticDimension::Punch:      return positive ? "more punch" : "less punch";
        case SemanticDimension::Tightness:  return positive ? "tighter" : "boomier/looser";
        case SemanticDimension::Count:      break;
    }
    return "unknown goal";
}

std::string constraintName(const SemanticConstraint& constraint)
{
    if (constraint.kind == SemanticConstraintKind::Preserve)
        return std::string("preserve ") + semanticDimensionName(constraint.dimension);

    if (constraint.direction < 0)
    {
        switch (constraint.dimension)
        {
            case SemanticDimension::Brightness: return "avoid darkness";
            case SemanticDimension::Warmth:     return "avoid thinness";
            case SemanticDimension::Clarity:    return "avoid muddiness";
            case SemanticDimension::Presence:   return "avoid recession";
            case SemanticDimension::Smoothness: return "avoid harshness";
            case SemanticDimension::Weight:     return "avoid losing weight";
            case SemanticDimension::Punch:      return "avoid losing punch";
            case SemanticDimension::Tightness:  return "avoid boominess";
            case SemanticDimension::Count:      break;
        }
    }

    return std::string("avoid more ") + semanticDimensionName(constraint.dimension);
}

std::string makeInterpretation(const SemanticIntent& intent)
{
    std::ostringstream out;
    bool first = true;

    for (const auto& goal : intent.goals)
    {
        if (!first)
            out << ", ";
        out << signedGoalName(goal);
        first = false;
    }

    for (const auto& constraint : intent.constraints)
    {
        if (!first)
            out << "; ";
        out << constraintName(constraint);
        first = false;
    }

    if (intent.contradictory)
    {
        if (!first)
            out << "; ";
        out << (intent.goalConstraintConflict
            ? "goal conflicts with requested protection"
            : "ambiguous/contradictory input");
    }

    return out.str();
}

bool startsWith(const std::string& text, const char* prefix)
{
    const std::string p(prefix);
    return text.size() >= p.size() && text.compare(0, p.size(), p) == 0;
}

std::vector<SemanticGoalOutcome> evaluateGoalOutcomes(const SemanticIntent& intent,
                                                      const PerceptualTarget& target,
                                                      const FitResult& fit,
                                                      double sampleRate)
{
    std::vector<SemanticGoalOutcome> outcomes;
    outcomes.reserve(intent.goals.size());

    for (const auto& goal : intent.goals)
    {
        SemanticGoalOutcome outcome;
        outcome.dimension = goal.dimension;
        outcome.requestedAmount = goal.amount;

        const std::string goalSource = SemanticTargetBuilder::sourceIdForDimension(goal.dimension);
        double numerator = 0.0;
        double denominator = 0.0;
        double constraintImpact = 0.0;
        std::map<std::string, std::pair<std::string, double>> limiting;

        for (const auto& point : target.points)
        {
            float desiredGoalDb = 0.0f;
            for (const auto& contribution : point.contributions)
                if (contribution.sourceId == goalSource)
                    desiredGoalDb += contribution.deltaDb;

            if (std::abs(desiredGoalDb) < 1.0e-5f)
                continue;

            const float actualDb = SparseParametricFitter::evaluatePlanDb(
                fit.bands, point.frequencyHz, sampleRate);
            const double weight = std::max(0.0f, point.confidence);
            numerator += weight * desiredGoalDb * actualDb;
            denominator += weight * desiredGoalDb * desiredGoalDb;

            for (const auto& contribution : point.contributions)
            {
                if (!startsWith(contribution.sourceId, "constraint:")
                    || std::abs(contribution.deltaDb) < 1.0e-5f)
                    continue;

                const double impact = weight * std::abs(desiredGoalDb)
                                    * std::abs(contribution.deltaDb);
                constraintImpact += impact;
                auto& entry = limiting[contribution.sourceId];
                if (entry.first.empty())
                    entry.first = contribution.sourcePhrase;
                entry.second += impact;
            }
        }

        const float projection = denominator > 1.0e-10
            ? static_cast<float>(numerator / denominator)
            : 0.0f;
        outcome.achievedFraction = std::clamp(projection, 0.0f, 1.0f);

        if (!limiting.empty())
        {
            auto best = limiting.begin();
            for (auto it = limiting.begin(); it != limiting.end(); ++it)
                if (it->second.second > best->second.second)
                    best = it;
            outcome.limitingSourceId = best->first;
            outcome.limitingPhrase = best->second.first;
        }

        if (outcome.achievedFraction >= 0.85f)
            outcome.status = GoalOutcomeStatus::Achieved;
        else if (constraintImpact > 1.0e-4)
            outcome.status = GoalOutcomeStatus::ConstraintLimited;
        else if (outcome.achievedFraction >= 0.35f)
            outcome.status = GoalOutcomeStatus::Partial;
        else
            outcome.status = GoalOutcomeStatus::Unmet;

        outcomes.push_back(std::move(outcome));
    }

    return outcomes;
}

std::string makeOutcomeSummary(const std::vector<SemanticGoalOutcome>& outcomes)
{
    std::ostringstream out;
    bool first = true;
    for (const auto& outcome : outcomes)
    {
        if (outcome.status == GoalOutcomeStatus::Achieved)
            continue;

        if (!first)
            out << ", ";

        out << semanticDimensionName(outcome.dimension) << " ";
        switch (outcome.status)
        {
            case GoalOutcomeStatus::Partial: out << "partial"; break;
            case GoalOutcomeStatus::ConstraintLimited: out << "constraint-limited"; break;
            case GoalOutcomeStatus::Unmet: out << "unmet"; break;
            case GoalOutcomeStatus::Achieved: break;
        }
        out << " (" << static_cast<int>(std::lround(outcome.achievedFraction * 100.0f)) << "%)";

        if (!outcome.limitingPhrase.empty())
            out << " by '" << outcome.limitingPhrase << "'";
        first = false;
    }
    return out.str();
}

} // namespace

const char* semanticDimensionName(SemanticDimension dimension) noexcept
{
    switch (dimension)
    {
        case SemanticDimension::Brightness: return "Brightness";
        case SemanticDimension::Warmth:     return "Warmth";
        case SemanticDimension::Clarity:    return "Clarity";
        case SemanticDimension::Presence:   return "Presence";
        case SemanticDimension::Smoothness: return "Smoothness";
        case SemanticDimension::Weight:     return "Weight";
        case SemanticDimension::Punch:      return "Punch";
        case SemanticDimension::Tightness:  return "Tightness";
        case SemanticDimension::Count:      break;
    }
    return "Unknown";
}

SemanticPlan SemanticPlanner::plan(std::string_view text,
                                   double sampleRate,
                                   float intensity) const
{
    // No context: an invalid one contextualises to identity, so the two paths
    // share a single implementation rather than drifting apart.
    return plan(text, sampleRate, intensity, SpectralContext {});
}

SemanticPlan SemanticPlanner::plan(std::string_view text,
                                   double sampleRate,
                                   float intensity,
                                   const SpectralContext& context) const
{
    return plan(text, sampleRate, intensity, context, {});
}

SemanticPlan SemanticPlanner::plan(std::string_view text,
                                   double sampleRate,
                                   float intensity,
                                   const SpectralContext& context,
                                   const std::vector<SemanticProtectedRange>& protectedRanges) const
{
    SemanticPlan result;
    result.intent = SemanticIntentCompiler().compile(text);

    const float safeIntensity = std::clamp(intensity, 0.0f, 2.0f);
    for (auto& goal : result.intent.goals)
        goal.amount = std::clamp(goal.amount * safeIntensity, -1.0f, 1.0f);

    for (const auto& range : protectedRanges)
    {
        if (! range.isValid())
            continue;
        auto copy = range;
        if (copy.sourceId.empty())
            copy.sourceId = makeUserProtectSourceId(result.intent.protectedRanges.size());
        result.intent.protectedRanges.push_back(std::move(copy));
    }

    result.interpretation = makeInterpretation(result.intent);

    if (!result.intent.isValid() || !result.intent.hasRecognizedContent
        || result.intent.goals.empty())
        return result;

    if (result.intent.contradictory)
        return result;

    // Source awareness sits here on purpose: after the intent is understood and
    // the user's intensity applied, before any geometry is chosen. It may only
    // attenuate a requested amount - it cannot invent a goal, flip one, or
    // touch a constraint - so the contradiction check above is still the last
    // word on what the user asked for.
    {
        const auto contextualized = SemanticContextualizer().contextualize(result.intent, context);
        result.intent = contextualized.intent;
        result.contextConfidence = contextualized.contextConfidence;
        result.contextApplied = contextualized.contextApplied;
        result.contextAdjustments = contextualized.adjustments;
    }

    result.target = SemanticTargetBuilder().build(result.intent, sampleRate);
    if (!result.target.isValid(sampleRate))
        return result;

    result.fit = SparseParametricFitter().fit(result.target, sampleRate);
    if (!result.fit.valid)
        return result;

    result.goalOutcomes = evaluateGoalOutcomes(
        result.intent, result.target, result.fit, sampleRate);
    result.outcomeSummary = makeOutcomeSummary(result.goalOutcomes);
    if (result.fit.bands.empty() && ! result.target.protectedRegions.empty())
        result.outcomeSummary = SemanticPlan::kProtectedRegionNoSafeMoveSummary;
    result.valid = true;
    return result;
}

} // namespace AIEQPerceptual
