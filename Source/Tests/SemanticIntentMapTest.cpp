#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticPlanner.h"
#include "../GUI/SemanticIntentMap.h"

#include <algorithm>
#include <initializer_list>

namespace
{
using namespace AIEQPerceptual;
using namespace EmberUI;
constexpr double kSampleRate = 48000.0;

bool overlaps (const SemanticFocusRegion& a, float lo, float hi)
{
    return a.maxFrequencyHz >= lo && a.minFrequencyHz <= hi;
}
}

class SemanticIntentMapTest final : public juce::UnitTest
{
public:
    SemanticIntentMapTest() : juce::UnitTest ("Semantic Intent Map truth projection", "AI-Diag") {}

    void runTest() override
    {
        beginTest ("more air derives focus from target provenance and highlights AIR only");
        {
            const auto plan = SemanticPlanner().plan ("more air", kSampleRate);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expect (state.visible());
            expect (!state.focusRegions.empty());
            if (!state.focusRegions.empty())
            {
                expect (state.focusRegions.front().dimension == SemanticDimension::Brightness);
                expect (state.focusRegions.front().focus == SemanticSpectralFocus::Air);
                expect (overlaps (state.focusRegions.front(), 8000.0f, 18000.0f));
            }
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Air)]
                    == SemanticAxisRole::Primary);
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Brilliance)]
                    == SemanticAxisRole::Neutral,
                    "Air must not implicitly light Brilliance");
        }

        beginTest ("generic brighter does not invent an Air or Brilliance axis mapping");
        {
            const auto plan = SemanticPlanner().plan ("brighter", kSampleRate);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Air)]
                    == SemanticAxisRole::Neutral);
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Brilliance)]
                    == SemanticAxisRole::Neutral);
        }

        beginTest ("warmer without mud chooses a real response bound relevant to warmth");
        {
            const auto plan = SemanticPlanner().plan ("warmer without mud", kSampleRate);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expect (!state.focusRegions.empty());
            expect (!state.protectRegions.empty());
            if (!state.focusRegions.empty())
                expect (state.focusRegions.front().dimension == SemanticDimension::Warmth);
            if (!state.protectRegions.empty())
            {
                const auto& protect = state.protectRegions.front();
                expect (protect.sourceId.find ("constraint:clarity") == 0);
                expect (protect.minFrequencyHz < 550.0f && protect.maxFrequencyHz > 200.0f,
                        "Balanced v1 should pick the low-mid mud bound, not an unrelated lobe");
            }
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Warmth)]
                    == SemanticAxisRole::Primary);
            expect (state.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Body)]
                    == SemanticAxisRole::Neutral,
                    "Warmth must not invent a Body-axis highlight");
        }

        beginTest ("NoSafeMove phase emits no visual focus or protection");
        {
            const auto plan = SemanticPlanner().plan ("more weight, don't touch the low end", kSampleRate);
            const auto state = buildSemanticIntentMapState (plan, SemanticIntentMapPhase::NoSafeMove);
            expect (!state.visible());
            expect (state.focusRegions.empty());
            expect (state.protectRegions.empty());
            expect (state.bandLinks.empty());
        }

        beginTest ("band links preserve real fitter provenance");
        {
            const auto plan = SemanticPlanner().plan ("warmer", kSampleRate);
            expect (plan.valid && !plan.fit.bands.empty());
            const auto state = buildSemanticIntentMapState (plan);
            expect (!state.bandLinks.empty());
            for (const auto& link : state.bandLinks)
            {
                expect (link.frequencyHz > 20.0f && link.frequencyHz < 20000.0f);
                expect (link.contributionWeight >= 0.0f && link.contributionWeight <= 1.0f);
                expect (link.sourceId.find ("goal:") == 0);
            }
        }

        beginTest ("envelope matches legacy focus provenance and target-point grid");
        {
            const auto plan = SemanticPlanner().plan ("more air", kSampleRate);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.focusEnvelopes.size(), (int) state.focusRegions.size());
            expect (!state.focusEnvelopes.empty());
            for (std::size_t i = 0; i < state.focusEnvelopes.size(); ++i)
            {
                const auto& envelope = state.focusEnvelopes[i];
                const auto& region = state.focusRegions[i];
                expect (envelope.sourceId == region.sourceId);
                expect (envelope.sourcePhrase == region.sourcePhrase);
                expect (envelope.dimension == region.dimension);
                expect (envelope.focus == region.focus);
                expect (envelope.primary == region.primary);
                expectWithinAbsoluteError (envelope.strength, region.strength, 1.0e-6f);
                expectEquals ((int) envelope.samples.size(), (int) plan.target.points.size());
                float peak = 0.0f;
                for (const auto& sample : envelope.samples)
                    peak = std::max (peak, sample.normalizedContribution);
                expectWithinAbsoluteError (peak, 1.0f, 1.0e-5f);
                for (std::size_t p = 0; p < plan.target.points.size(); ++p)
                {
                    expectWithinAbsoluteError (
                        envelope.samples[p].frequencyHz,
                        plan.target.points[p].frequencyHz,
                        1.0e-4f);
                }
            }
        }

        beginTest ("envelope preserves zero-contribution samples and local peak = 1");
        {
            const auto plan = makeMagnitudePlan (
                SemanticDimension::Brightness, SemanticSpectralFocus::Air, 0.9f,
                { 500.0f, 1000.0f, 2000.0f, 4000.0f },
                { 0.8f, 0.0f, 0.0f, 0.4f });
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.focusEnvelopes.size(), 1);
            expectEquals ((int) state.focusRegions.size(), 1);
            const auto& envelope = state.focusEnvelopes.front();
            expectEquals ((int) envelope.samples.size(), 4);
            expectWithinAbsoluteError (envelope.samples[0].normalizedContribution, 1.0f, 1.0e-5f);
            expectWithinAbsoluteError (envelope.samples[1].normalizedContribution, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (envelope.samples[2].normalizedContribution, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (envelope.samples[3].normalizedContribution, 0.5f, 1.0e-5f);
            expect (envelope.samples[1].withinDisplaySupport == false);
            expect (envelope.samples[2].withinDisplaySupport == false);
            expect (state.focusRegions.front().minFrequencyHz <= 500.0f);
            expect (state.focusRegions.front().maxFrequencyHz >= 4000.0f);
        }

        beginTest ("envelope support threshold is max(0.05, peak*0.25)");
        {
            const auto relative = makeMagnitudePlan (
                SemanticDimension::Warmth, SemanticSpectralFocus::LowMid, 0.8f,
                { 200.0f, 400.0f, 800.0f, 1600.0f },
                { 1.0f, 0.24f, 0.25f, 0.26f });
            const auto relativeState = buildSemanticIntentMapState (relative);
            expectEquals ((int) relativeState.focusEnvelopes.size(), 1);
            const auto& relativeSamples = relativeState.focusEnvelopes.front().samples;
            expect (!relativeSamples[1].withinDisplaySupport);
            expect (relativeSamples[2].withinDisplaySupport);
            expect (relativeSamples[3].withinDisplaySupport);

            const auto floorPlan = makeMagnitudePlan (
                SemanticDimension::Clarity, SemanticSpectralFocus::General, 0.8f,
                { 200.0f, 400.0f, 800.0f },
                { 0.10f, 0.049f, 0.05f });
            const auto floorState = buildSemanticIntentMapState (floorPlan);
            expectEquals ((int) floorState.focusEnvelopes.size(), 1);
            const auto& floorSamples = floorState.focusEnvelopes.front().samples;
            expect (floorSamples[0].withinDisplaySupport);
            expect (!floorSamples[1].withinDisplaySupport);
            expect (floorSamples[2].withinDisplaySupport);
        }

        beginTest ("disjoint lobes keep a zero valley; top-2 matches focusRegions");
        {
            const auto plan = makeTwoGoalPlan();
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.focusRegions.size(), 2);
            expectEquals ((int) state.focusEnvelopes.size(), 2);
            expect (state.focusEnvelopes[0].primary);
            expect (!state.focusEnvelopes[1].primary);
            expect (state.focusEnvelopes[0].sourceId == state.focusRegions[0].sourceId);
            expect (state.focusEnvelopes[1].sourceId == state.focusRegions[1].sourceId);

            const auto* brightness = envelopeForSource (state, "goal:brightness");
            expect (brightness != nullptr);
            if (brightness != nullptr)
            {
                expectEquals ((int) brightness->samples.size(), 5);
                expectWithinAbsoluteError (brightness->samples[1].normalizedContribution, 0.0f, 1.0e-6f);
                expect (!brightness->samples[1].withinDisplaySupport);
                expectWithinAbsoluteError (brightness->samples[0].normalizedContribution, 1.0f, 1.0e-5f);
                expectWithinAbsoluteError (brightness->samples[4].normalizedContribution, 1.0f, 1.0e-5f);
            }
        }

        beginTest ("Hidden Planning invalid and NoSafeMove emit no envelope");
        {
            const auto air = SemanticPlanner().plan ("more air", kSampleRate);
            expect (air.valid);
            const auto hidden = buildSemanticIntentMapState (air, SemanticIntentMapPhase::Hidden);
            expect (!hidden.visible());
            expect (hidden.focusEnvelopes.empty());
            expect (hidden.focusRegions.empty());
            expect (hidden.protectRegions.empty());
            expect (hidden.bandLinks.empty());

            const auto planning = buildSemanticIntentMapState (air, SemanticIntentMapPhase::Planning);
            expect (planning.focusEnvelopes.empty());
            expect (planning.focusRegions.empty());

            SemanticPlan invalid;
            invalid.valid = false;
            const auto invalidState = buildSemanticIntentMapState (invalid);
            expect (invalidState.focusEnvelopes.empty());
            expect (invalidState.focusRegions.empty());

            const auto blocked = SemanticPlanner().plan ("more weight, don't touch the low end", kSampleRate);
            const auto noSafe = buildSemanticIntentMapState (blocked, SemanticIntentMapPhase::NoSafeMove);
            expect (noSafe.focusEnvelopes.empty());
            expect (noSafe.focusRegions.empty());
            expect (noSafe.protectRegions.empty());
            expect (noSafe.bandLinks.empty());
        }

        beginTest ("repeated projection is deterministic; legacy fields stay intact");
        {
            const auto plan = SemanticPlanner().plan ("warmer without mud", kSampleRate);
            expect (plan.valid);
            const auto a = buildSemanticIntentMapState (plan);
            const auto b = buildSemanticIntentMapState (plan);
            expectEquals ((int) a.focusRegions.size(), (int) b.focusRegions.size());
            expectEquals ((int) a.protectRegions.size(), (int) b.protectRegions.size());
            expectEquals ((int) a.bandLinks.size(), (int) b.bandLinks.size());
            expectEquals ((int) a.focusEnvelopes.size(), (int) b.focusEnvelopes.size());
            expectEquals ((int) a.appliedBandSlots.size(), (int) b.appliedBandSlots.size());
            for (std::size_t i = 0; i < a.axisRoles.size(); ++i)
                expect (a.axisRoles[i] == b.axisRoles[i]);
            for (std::size_t i = 0; i < a.focusRegions.size(); ++i)
            {
                expectWithinAbsoluteError (a.focusRegions[i].minFrequencyHz, b.focusRegions[i].minFrequencyHz, 1.0e-4f);
                expectWithinAbsoluteError (a.focusRegions[i].maxFrequencyHz, b.focusRegions[i].maxFrequencyHz, 1.0e-4f);
                expect (a.focusRegions[i].sourceId == b.focusRegions[i].sourceId);
            }
            for (std::size_t i = 0; i < a.focusEnvelopes.size(); ++i)
            {
                expectEquals ((int) a.focusEnvelopes[i].samples.size(),
                              (int) b.focusEnvelopes[i].samples.size());
                for (std::size_t s = 0; s < a.focusEnvelopes[i].samples.size(); ++s)
                {
                    expectWithinAbsoluteError (
                        a.focusEnvelopes[i].samples[s].normalizedContribution,
                        b.focusEnvelopes[i].samples[s].normalizedContribution,
                        1.0e-6f);
                    expect (a.focusEnvelopes[i].samples[s].withinDisplaySupport
                            == b.focusEnvelopes[i].samples[s].withinDisplaySupport);
                }
            }
            expect (!a.protectRegions.empty());
            expect (!a.focusRegions.empty());
            expect (a.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Warmth)]
                    == SemanticAxisRole::Primary);
        }
    }

private:
    static SemanticPlan makeMagnitudePlan (
        SemanticDimension dimension,
        SemanticSpectralFocus focus,
        float amount,
        std::initializer_list<float> frequencies,
        std::initializer_list<float> magnitudes)
    {
        SemanticPlan plan;
        plan.valid = true;
        SemanticGoal goal;
        goal.dimension = dimension;
        goal.focus = focus;
        goal.amount = amount;
        goal.confidence = 1.0f;
        goal.sourcePhrase = "fixture";
        plan.intent.goals.push_back (goal);

        const std::string sourceId = std::string ("goal:") + semanticDimensionStableId (dimension);
        auto freq = frequencies.begin();
        auto mag = magnitudes.begin();
        for (; freq != frequencies.end() && mag != magnitudes.end(); ++freq, ++mag)
        {
            TargetPoint point;
            point.frequencyHz = *freq;
            point.deltaDb = *mag;
            TargetContribution contribution;
            contribution.sourceId = sourceId;
            contribution.sourcePhrase = "fixture";
            contribution.deltaDb = *mag;
            contribution.confidence = 1.0f;
            point.contributions.push_back (std::move (contribution));
            plan.target.points.push_back (std::move (point));
        }
        return plan;
    }

    static SemanticPlan makeTwoGoalPlan()
    {
        SemanticPlan plan;
        plan.valid = true;

        SemanticGoal bright;
        bright.dimension = SemanticDimension::Brightness;
        bright.focus = SemanticSpectralFocus::Air;
        bright.amount = 1.0f;
        bright.confidence = 1.0f;
        bright.sourcePhrase = "air fixture";
        plan.intent.goals.push_back (bright);

        SemanticGoal warmth;
        warmth.dimension = SemanticDimension::Warmth;
        warmth.focus = SemanticSpectralFocus::LowMid;
        warmth.amount = 0.4f;
        warmth.confidence = 1.0f;
        warmth.sourcePhrase = "warm fixture";
        plan.intent.goals.push_back (warmth);

        SemanticGoal extra;
        extra.dimension = SemanticDimension::Punch;
        extra.focus = SemanticSpectralFocus::General;
        extra.amount = 0.15f;
        extra.confidence = 1.0f;
        extra.sourcePhrase = "weak fixture";
        plan.intent.goals.push_back (extra);

        const float freqs[] = { 500.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f };
        const float brightMags[] = { 0.9f, 0.0f, 0.0f, 0.0f, 0.9f };
        const float warmMags[] = { 0.2f, 0.8f, 0.8f, 0.1f, 0.0f };
        const float punchMags[] = { 0.05f, 0.05f, 0.4f, 0.05f, 0.05f };

        for (int i = 0; i < 5; ++i)
        {
            TargetPoint point;
            point.frequencyHz = freqs[i];
            TargetContribution b;
            b.sourceId = "goal:brightness";
            b.deltaDb = brightMags[i];
            TargetContribution w;
            w.sourceId = "goal:warmth";
            w.deltaDb = warmMags[i];
            TargetContribution p;
            p.sourceId = "goal:punch";
            p.deltaDb = punchMags[i];
            point.contributions.push_back (std::move (b));
            point.contributions.push_back (std::move (w));
            point.contributions.push_back (std::move (p));
            plan.target.points.push_back (std::move (point));
        }
        return plan;
    }

    static const SemanticFocusEnvelope* envelopeForSource (
        const SemanticIntentMapState& state, const char* sourceId)
    {
        for (const auto& envelope : state.focusEnvelopes)
            if (envelope.sourceId == sourceId)
                return &envelope;
        return nullptr;
    }
};

static SemanticIntentMapTest semanticIntentMapTest;

#endif
