#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "../AI/SemanticContextualizer.h"
#include "../AI/SemanticPlanner.h"
#include "../AI/SemanticPlanningService.h"
#include "../AI/SemanticTargetBuilder.h"
#include "../GUI/SemanticIntentMap.h"

#include <string>
#include <vector>

namespace
{
using namespace AIEQPerceptual;
using namespace EmberUI;
constexpr double kSr = 48000.0;

SemanticProtectedRange makeRange (float minHz, float maxHz, float maxAbsDeltaDb,
                                  const char* phrase = "user lock")
{
    SemanticProtectedRange range;
    range.minFrequencyHz = minHz;
    range.maxFrequencyHz = maxHz;
    range.maxAbsDeltaDb = maxAbsDeltaDb;
    range.confidence = 1.0f;
    range.sourcePhrase = phrase;
    return range;
}

SemanticIntent airGoal()
{
    SemanticIntent intent;
    intent.hasRecognizedContent = true;
    intent.confidence = 1.0f;
    intent.goals.push_back ({ SemanticDimension::Brightness, 0.8f, 1.0f, "more air",
                              SemanticSpectralFocus::Air });
    return intent;
}
}

class SemanticProtectedRangeTest final : public juce::UnitTest
{
public:
    SemanticProtectedRangeTest()
        : juce::UnitTest ("Semantic protected range", "AI-Diag")
    {}

    void runTest() override
    {
        beginTest ("builder copies a user Hz range into protectedRegions");
        {
            auto intent = airGoal();
            intent.protectedRanges.push_back (makeRange (8000.0f, 18000.0f, 0.25f));
            intent.protectedRanges.back().sourceId = makeUserProtectSourceId (0);
            const auto target = SemanticTargetBuilder().build (intent, kSr);
            expect (target.isValid (kSr));
            expectEquals ((int) target.protectedRegions.size(), 1);
            expect (target.responseBounds.empty());
            if (! target.protectedRegions.empty())
            {
                expectWithinAbsoluteError (target.protectedRegions.front().minFrequencyHz, 8000.0f, 1.0e-3f);
                expectWithinAbsoluteError (target.protectedRegions.front().maxFrequencyHz, 18000.0f, 1.0e-3f);
                expectWithinAbsoluteError (target.protectedRegions.front().maxAbsDeltaDb, 0.25f, 1.0e-6f);
                expect (target.protectedRegions.front().sourceId
                            == makeUserProtectSourceId (0));
            }
        }

        beginTest ("more air plus 8-18 kHz fence stays inside the fitter cap");
        {
            const auto plan = SemanticPlanner().plan (
                "more air", kSr, 1.0f, {}, { makeRange (8000.0f, 18000.0f, 0.25f) });
            expect (plan.valid);
            expect (plan.fit.valid);
            expectEquals ((int) plan.target.protectedRegions.size(), 1);
            expect (plan.fit.maxProtectedDeviationDb <= 0.251f,
                    "fitter must fail closed rather than violate a protected zone");
        }

        beginTest ("same-axis text Preserve is still ContradictoryIntent, not NoSafeMove");
        {
            const auto plan = SemanticPlanner().plan (
                "more weight, don't touch the low end", kSr);
            expect (plan.intent.goalConstraintConflict);
            expect (plan.intent.contradictory);
            expect (! plan.valid);
            expect (plan.target.protectedRegions.empty());
        }

        beginTest ("warmer without mud still projects the clarity response bound");
        {
            const auto plan = SemanticPlanner().plan ("warmer without mud", kSr);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expect (! state.protectRegions.empty());
            if (! state.protectRegions.empty())
            {
                expect (state.protectRegions.front().sourceId.find ("constraint:clarity") == 0);
                expect (state.protectRegions.front().kind == SemanticConstraintKind::AvoidDirection);
            }
        }

        beginTest ("contextualizer leaves user ranges byte-identical");
        {
            auto intent = airGoal();
            intent.protectedRanges.push_back (makeRange (200.0f, 500.0f, 0.25f, "mud lock"));
            intent.protectedRanges.back().sourceId = makeUserProtectSourceId (0);
            const auto before = intent.protectedRanges;
            const auto out = SemanticContextualizer().contextualize (intent, {});
            expectEquals ((int) out.intent.protectedRanges.size(), (int) before.size());
            if (! out.intent.protectedRanges.empty() && ! before.empty())
            {
                expectWithinAbsoluteError (out.intent.protectedRanges.front().minFrequencyHz,
                                           before.front().minFrequencyHz, 1.0e-6f);
                expectWithinAbsoluteError (out.intent.protectedRanges.front().maxFrequencyHz,
                                           before.front().maxFrequencyHz, 1.0e-6f);
                expect (out.intent.protectedRanges.front().sourceId == before.front().sourceId);
                expect (out.intent.protectedRanges.front().sourcePhrase == before.front().sourcePhrase);
            }
        }

        beginTest ("mailbox submit carries ranges; empty submit matches old plan");
        {
            juce::MessageManager::getInstance();
            const auto fence = makeRange (8000.0f, 18000.0f, 0.25f, "air lock");
            SemanticPlanningService svc;
            svc.start();
            const auto gen = svc.submit ("more air", 1.0f, kSr, {}, { fence });
            expect (svc.waitUntilQuiescent (8000), "planner did not settle");
            auto result = svc.takeCurrentResult();
            expect (result.has_value() && result->generation == gen);
            if (result.has_value())
            {
                expectEquals ((int) result->plan.target.protectedRegions.size(), 1);
                expect (result->plan.fit.maxProtectedDeviationDb <= 0.251f);
            }

            const auto genBare = svc.submit ("more air", 1.0f, kSr);
            expect (svc.waitUntilQuiescent (8000));
            auto bare = svc.takeCurrentResult();
            svc.stop();
            expect (bare.has_value() && bare->generation == genBare);
            if (bare.has_value())
            {
                expect (bare->plan.target.protectedRegions.empty());
                const auto direct = SemanticPlanner().plan ("more air", kSr);
                expectEquals ((int) bare->plan.fit.bands.size(), (int) direct.fit.bands.size());
            }
        }

        beginTest ("map projects user ProtectedRegion without mutating the plan");
        {
            const auto plan = SemanticPlanner().plan (
                "more air", kSr, 1.0f, {}, { makeRange (8000.0f, 18000.0f, 0.25f, "air lock") });
            expect (plan.valid);
            const auto beforeCount = plan.target.protectedRegions.size();
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) plan.target.protectedRegions.size(), (int) beforeCount);
            bool sawUser = false;
            for (const auto& region : state.protectRegions)
            {
                if (region.sourceId.find (kUserProtectSourceIdPrefix) == 0)
                {
                    sawUser = true;
                    expect (region.kind == SemanticConstraintKind::ProtectRange);
                    expectWithinAbsoluteError (region.minFrequencyHz, 8000.0f, 1.0f);
                    expectWithinAbsoluteError (region.maxFrequencyHz, 18000.0f, 1.0f);
                    expect (region.sourcePhrase == "air lock");
                }
            }
            expect (sawUser, "Ready map must render the user Hz fence");

            const auto hidden = buildSemanticIntentMapState (plan, SemanticIntentMapPhase::Hidden);
            const auto planning = buildSemanticIntentMapState (plan, SemanticIntentMapPhase::Planning);
            const auto noSafe = buildSemanticIntentMapState (plan, SemanticIntentMapPhase::NoSafeMove);
            expect (hidden.protectRegions.empty());
            expect (planning.protectRegions.empty());
            expect (noSafe.protectRegions.empty());
        }

        beginTest ("full-band zero fence is NoSafeMove, not intensity-zero empty summary");
        {
            const auto blocked = SemanticPlanner().plan (
                "more air", kSr, 1.0f, {}, { makeRange (20.0f, 20000.0f, 0.0f) });
            expect (blocked.valid);
            expect (blocked.fit.valid);
            expect (blocked.fit.bands.empty());
            expect (! blocked.target.protectedRegions.empty());
            expect (blocked.outcomeSummary
                        == std::string (SemanticPlan::kProtectedRegionNoSafeMoveSummary));

            const auto zeroIntensity = SemanticPlanner().plan ("more air", kSr, 0.0f);
            expect (zeroIntensity.valid);
            expect (zeroIntensity.fit.bands.empty());
            expect (zeroIntensity.target.protectedRegions.empty());
            expect (zeroIntensity.outcomeSummary
                        != std::string (SemanticPlan::kProtectedRegionNoSafeMoveSummary),
                    "intensity 0 must not reuse the protected-region NoSafeMove copy");
        }
    }
};

static SemanticProtectedRangeTest semanticProtectedRangeTest;

#endif
