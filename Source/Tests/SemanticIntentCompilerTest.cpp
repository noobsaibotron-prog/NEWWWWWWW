#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticIntentCompiler.h"
#include "../AI/SemanticTargetBuilder.h"
#include "../AI/SemanticPlanner.h"
#include "../AI/SparseParametricFitter.h"

#include <cmath>

namespace
{
using namespace AIEQPerceptual;
constexpr double kSampleRate = 48000.0;

const SemanticGoal* findGoal(const SemanticIntent& intent, SemanticDimension dimension)
{
    for (const auto& goal : intent.goals)
        if (goal.dimension == dimension)
            return &goal;
    return nullptr;
}

const SemanticConstraint* findConstraint(const SemanticIntent& intent,
                                         SemanticDimension dimension)
{
    for (const auto& constraint : intent.constraints)
        if (constraint.dimension == dimension)
            return &constraint;
    return nullptr;
}
} // namespace

class SemanticIntentCompilerTest final : public juce::UnitTest
{
public:
    SemanticIntentCompilerTest()
        : juce::UnitTest("Semantic Intent Compiler", "AI-Diag") {}

    void runTest() override
    {
        SemanticIntentCompiler compiler;

        beginTest("Clause-local polarity: less harsh but more air");
        {
            const auto intent = compiler.compile("less harsh but more air");
            const auto* smooth = findGoal(intent, SemanticDimension::Smoothness);
            const auto* bright = findGoal(intent, SemanticDimension::Brightness);
            expect(intent.isValid());
            expect(smooth != nullptr && smooth->amount > 0.0f,
                   "less harsh must compile toward +Smoothness");
            expect(bright != nullptr && bright->amount > 0.0f,
                   "more air must remain positive despite earlier less");
        }

        beginTest("without creates an avoid-direction constraint, not a second goal");
        {
            const auto intent = compiler.compile("warmer without mud");
            const auto* warmth = findGoal(intent, SemanticDimension::Warmth);
            const auto* mudGuard = findConstraint(intent, SemanticDimension::Clarity);
            expect(warmth != nullptr && warmth->amount > 0.0f);
            expect(mudGuard != nullptr);
            if (mudGuard != nullptr)
            {
                expect(mudGuard->kind == SemanticConstraintKind::AvoidDirection);
                expectEquals(mudGuard->direction, -1);
            }

            // "mud" is a constraint here and must not also become a Clarity goal.
            expect(findGoal(intent, SemanticDimension::Clarity) == nullptr);
        }

        beginTest("keep/don't touch creates a preservation constraint");
        {
            const auto intent = compiler.compile("more air, don't touch the low end");
            const auto* bright = findGoal(intent, SemanticDimension::Brightness);
            const auto* keepWeight = findConstraint(intent, SemanticDimension::Weight);
            expect(bright != nullptr && bright->amount > 0.0f);
            expect(keepWeight != nullptr);
            if (keepWeight != nullptr)
                expect(keepWeight->kind == SemanticConstraintKind::Preserve);
        }

        beginTest("Intensity words remain local to their clause");
        {
            const auto intent = compiler.compile("slightly less muddy but much more presence");
            const auto* clarity = findGoal(intent, SemanticDimension::Clarity);
            const auto* presence = findGoal(intent, SemanticDimension::Presence);
            expect(clarity != nullptr && presence != nullptr);
            if (clarity != nullptr && presence != nullptr)
            {
                expect(clarity->amount > 0.0f && presence->amount > 0.0f);
                expect(std::abs(clarity->amount) < std::abs(presence->amount),
                       "slightly and much must not collapse to one global intensity");
            }
        }

        beginTest("Constraint scope does not swallow a later goal");
        {
            const auto intent = compiler.compile("warmer without mud and more air");
            expect(findGoal(intent, SemanticDimension::Warmth) != nullptr);
            expect(findGoal(intent, SemanticDimension::Brightness) != nullptr);
            expect(findConstraint(intent, SemanticDimension::Clarity) != nullptr);
        }

        beginTest("Less open remains negative brightness");
        {
            const auto intent = compiler.compile("less open");
            const auto* brightness = findGoal(intent, SemanticDimension::Brightness);
            expect(brightness != nullptr && brightness->amount < 0.0f);
        }

        beginTest("Without losing weight means preserve, not avoid more weight");
        {
            const auto intent = compiler.compile("without losing weight");
            const auto* weight = findConstraint(intent, SemanticDimension::Weight);
            expect(weight != nullptr);
            if (weight != nullptr)
                expect(weight->kind == SemanticConstraintKind::Preserve);
        }

        beginTest("Italian deterministic path");
        {
            const auto intent = compiler.compile("piu caldo senza impastato");
            expect(findGoal(intent, SemanticDimension::Warmth) != nullptr);
            expect(findConstraint(intent, SemanticDimension::Clarity) != nullptr);
        }

        beginTest("Semantic target + sparse fitter obey directional mud guard");
        {
            const auto intent = compiler.compile("warmer without mud");
            const auto target = SemanticTargetBuilder().build(intent, kSampleRate);
            expect(target.isValid(kSampleRate));
            expect(!target.responseBounds.empty());

            const auto fit = SparseParametricFitter().fit(target, kSampleRate);
            expect(fit.valid);
            expect(fit.maxResponseBoundViolationDb <= 1.1e-4f,
                   "fitter must reject candidates that violate semantic response bounds");
        }

        beginTest("Preserve low end remains hard-bounded after fitting");
        {
            const auto intent = compiler.compile("more air, don't touch the low end");
            const auto target = SemanticTargetBuilder().build(intent, kSampleRate);
            const auto fit = SparseParametricFitter().fit(target, kSampleRate);
            expect(fit.valid);
            expect(fit.maxResponseBoundViolationDb <= 1.1e-4f);
        }

        beginTest("Full SemanticPlanner returns an explainable sparse plan");
        {
            const auto plan = SemanticPlanner().plan("less harsh but more air", kSampleRate);
            expect(plan.valid);
            expect(plan.fit.valid);
            expect(!plan.fit.bands.empty());
            expect(plan.interpretation.find("smoother") != std::string::npos);
            expect(plan.interpretation.find("brighter") != std::string::npos);
        }

        beginTest("Contradictory same-axis instruction fails closed");
        {
            const auto plan = SemanticPlanner().plan("more bright but less bright", kSampleRate);
            expect(plan.intent.contradictory);
            expect(!plan.valid);
        }

        beginTest("Unknown marketing language is not guessed");
        {
            const auto plan = SemanticPlanner().plan("make it expensive and purple", kSampleRate);
            expect(!plan.intent.hasRecognizedContent);
            expect(!plan.valid);
        }

        beginTest("Global intensity scales target magnitude without changing grammar");
        {
            const auto low = SemanticPlanner().plan("more air", kSampleRate, 0.5f);
            const auto high = SemanticPlanner().plan("more air", kSampleRate, 1.5f);
            expect(low.valid && high.valid);

            float lowMax = 0.0f;
            float highMax = 0.0f;
            for (const auto& p : low.target.points)
                lowMax = std::max(lowMax, std::abs(p.deltaDb));
            for (const auto& p : high.target.points)
                highMax = std::max(highMax, std::abs(p.deltaDb));
            expect(lowMax < highMax);
        }


        beginTest("Modifier tokens require word boundaries");
        {
            const auto timeless = compiler.compile("timeless and bright");
            const auto* bright = findGoal(timeless, SemanticDimension::Brightness);
            expect(bright != nullptr && bright->amount > 0.0f,
                   "less inside timeless must not invert brightness");

            const auto careless = compiler.compile("careless but warm");
            const auto* warm = findGoal(careless, SemanticDimension::Warmth);
            expect(warm != nullptr && warm->amount > 0.0f,
                   "less inside careless must not invert warmth");

            const auto scuttle = compiler.compile("scuttle the mud");
            const auto* clarity = findGoal(scuttle, SemanticDimension::Clarity);
            expect(clarity != nullptr && clarity->amount < 0.0f,
                   "cut inside scuttle must not become a negative modifier");
        }

        beginTest("Goal versus preserve constraint fails closed explicitly");
        {
            const auto intent = compiler.compile("more weight, don't touch the low end");
            expect(intent.goalConstraintConflict);
            expect(intent.contradictory);
            const auto plan = SemanticPlanner().plan(
                "more weight, don't touch the low end", kSampleRate);
            expect(!plan.valid);
            expect(plan.interpretation.find("goal conflicts") != std::string::npos);
        }

        beginTest("Goal versus avoid-same-direction constraint fails closed");
        {
            const auto intent = compiler.compile("more mud without mud");
            expect(intent.goalConstraintConflict);
            expect(intent.contradictory);
        }

        beginTest("PerceptualTarget carries a versioned relative-delta contract");
        {
            const auto plan = SemanticPlanner().plan("more air", kSampleRate);
            expect(plan.valid);
            expectEquals(static_cast<int>(plan.target.schemaVersion),
                         static_cast<int>(kPerceptualTargetSchemaVersion));
            expect(plan.target.deltaDomain == TargetDeltaDomain::RelativeEqDeltaDb);
        }

        beginTest("Fitted bands keep generic semantic provenance");
        {
            const auto plan = SemanticPlanner().plan("less harsh but more air", kSampleRate);
            expect(plan.valid);
            expect(!plan.fit.bands.empty());
            for (const auto& band : plan.fit.bands)
            {
                expect(!band.contributions.empty(), "planned band must keep target provenance");
                expect(!band.reason.empty(), "planned band must expose a reason");
            }
        }

        beginTest("Constraint-limited goals are reported instead of silently sacrificed");
        {
            const auto plan = SemanticPlanner().plan("warmer without mud", kSampleRate);
            expect(plan.valid);
            bool sawWarmth = false;
            for (const auto& outcome : plan.goalOutcomes)
            {
                if (outcome.dimension != SemanticDimension::Warmth)
                    continue;
                sawWarmth = true;
                expect(outcome.status == GoalOutcomeStatus::ConstraintLimited
                       || outcome.status == GoalOutcomeStatus::Partial
                       || outcome.status == GoalOutcomeStatus::Achieved);
                if (outcome.status == GoalOutcomeStatus::ConstraintLimited)
                    expect(!outcome.limitingSourceId.empty());
            }
            expect(sawWarmth);
        }

        beginTest("Identical phrase compiles deterministically");
        {
            const auto a = compiler.compile("brighter without harshness");
            const auto b = compiler.compile("brighter without harshness");
            expectEquals(static_cast<int>(a.goals.size()), static_cast<int>(b.goals.size()));
            expectEquals(static_cast<int>(a.constraints.size()), static_cast<int>(b.constraints.size()));
            if (!a.goals.empty() && !b.goals.empty())
                expectWithinAbsoluteError(a.goals.front().amount, b.goals.front().amount, 0.0f);
        }
    }
};

static SemanticIntentCompilerTest gSemanticIntentCompilerTest;

#endif
