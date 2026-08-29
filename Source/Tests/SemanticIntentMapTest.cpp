#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticPlanner.h"
#include "../GUI/SemanticIntentMap.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace AIEQPerceptual;
using namespace EmberUI;
constexpr double kSampleRate = 48000.0;

bool overlaps (const SemanticFocusRegion& a, float lo, float hi)
{
    return a.maxFrequencyHz >= lo && a.minFrequencyHz <= hi;
}

bool isPositiveFinite (float x) noexcept
{
    return std::isfinite (x) && x > 0.0f;
}

/** Independent oracle from attested TargetPoint contributions.

    Peak uses the same display-support threshold as Envelope 2.0
    (max(0.05, peakAbs*0.25)), then the max contribution with a
    lower-frequency tie. Centroid and support use every sample with
    finite positive frequency and weight. This must not consult a
    visual band table.
*/
struct AttestedAnchorExpect
{
    bool valid = false;
    float peakHz = 0.0f;
    float centroidHz = 0.0f;
    float lowerHz = 0.0f;
    float upperHz = 0.0f;
};

AttestedAnchorExpect attestedAnchorExpect (const SemanticPlan& plan, const std::string& sourceId)
{
    float peakAbs = 0.0f;
    std::vector<float> frequencies, magnitudes;
    frequencies.reserve (plan.target.points.size());
    magnitudes.reserve (plan.target.points.size());
    for (const auto& point : plan.target.points)
    {
        float mag = 0.0f;
        for (const auto& contribution : point.contributions)
            if (contribution.sourceId == sourceId)
                mag += std::abs (contribution.deltaDb);
        frequencies.push_back (point.frequencyHz);
        magnitudes.push_back (mag);
        if (std::isfinite (mag))
            peakAbs = std::max (peakAbs, mag);
    }

    if (! std::isfinite (peakAbs) || peakAbs <= 1.0e-5f)
        return {};

    const float threshold = std::max (0.05f, peakAbs * 0.25f);
    AttestedAnchorExpect out;
    float peakWeight = -1.0f;
    bool havePeak = false;
    double weightSum = 0.0;
    double weightedLog2 = 0.0;

    for (std::size_t i = 0; i < frequencies.size(); ++i)
    {
        const float f = frequencies[i];
        const float mag = magnitudes[i];
        const float w = mag / peakAbs;
        const bool usableWeight = isPositiveFinite (w);
        const bool usableFreq = isPositiveFinite (f);

        if (usableFreq && usableWeight)
        {
            if (! out.valid)
            {
                out.lowerHz = f;
                out.valid = true;
            }
            out.upperHz = f;
            weightSum += static_cast<double> (w);
            weightedLog2 += static_cast<double> (w) * std::log2 (static_cast<double> (f));
        }

        if (mag >= threshold && usableFreq && usableWeight)
        {
            if (! havePeak || w > peakWeight || (w == peakWeight && f < out.peakHz))
            {
                havePeak = true;
                peakWeight = w;
                out.peakHz = f;
            }
        }
    }

    if (! out.valid || ! havePeak || ! (weightSum > 0.0))
        return {};

    const float centroid = static_cast<float> (std::exp2 (weightedLog2 / weightSum));
    if (! isPositiveFinite (centroid) || ! isPositiveFinite (out.peakHz)
        || ! isPositiveFinite (out.lowerHz) || ! isPositiveFinite (out.upperHz))
        return {};

    out.centroidHz = centroid;
    return out;
}

const SemanticIntentAnchor* anchorForSource (const SemanticIntentMapState& state, const char* sourceId)
{
    for (const auto& anchor : state.intentAnchors)
        if (anchor.sourceId == sourceId)
            return &anchor;
    return nullptr;
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
            expect (state.intentAnchors.empty());
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
            expectEquals ((int) state.intentAnchors.size(), 2);
            expect (state.intentAnchors[0].primary);
            expect (!state.intentAnchors[1].primary);
            expect (state.intentAnchors[0].sourceId == state.focusEnvelopes[0].sourceId);
            expect (state.intentAnchors[1].sourceId == state.focusEnvelopes[1].sourceId);

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
            expect (hidden.intentAnchors.empty());
            expect (hidden.focusRegions.empty());
            expect (hidden.protectRegions.empty());
            expect (hidden.bandLinks.empty());

            const auto planning = buildSemanticIntentMapState (air, SemanticIntentMapPhase::Planning);
            expect (planning.focusEnvelopes.empty());
            expect (planning.intentAnchors.empty());
            expect (planning.focusRegions.empty());

            SemanticPlan invalid;
            invalid.valid = false;
            const auto invalidState = buildSemanticIntentMapState (invalid);
            expect (invalidState.focusEnvelopes.empty());
            expect (invalidState.intentAnchors.empty());
            expect (invalidState.focusRegions.empty());

            const auto blocked = SemanticPlanner().plan ("more weight, don't touch the low end", kSampleRate);
            const auto noSafe = buildSemanticIntentMapState (blocked, SemanticIntentMapPhase::NoSafeMove);
            expect (noSafe.focusEnvelopes.empty());
            expect (noSafe.intentAnchors.empty());
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
            expectEquals ((int) a.intentAnchors.size(), (int) b.intentAnchors.size());
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
            for (std::size_t i = 0; i < a.intentAnchors.size(); ++i)
            {
                expect (a.intentAnchors[i].sourceId == b.intentAnchors[i].sourceId);
                expect (a.intentAnchors[i].primary == b.intentAnchors[i].primary);
                expectWithinAbsoluteError (a.intentAnchors[i].peakFrequencyHz,
                                          b.intentAnchors[i].peakFrequencyHz, 1.0e-4f);
                expectWithinAbsoluteError (a.intentAnchors[i].centroidFrequencyHz,
                                          b.intentAnchors[i].centroidFrequencyHz, 1.0e-4f);
                expectWithinAbsoluteError (a.intentAnchors[i].lowerSupportHz,
                                          b.intentAnchors[i].lowerSupportHz, 1.0e-4f);
                expectWithinAbsoluteError (a.intentAnchors[i].upperSupportHz,
                                          b.intentAnchors[i].upperSupportHz, 1.0e-4f);
            }
            expect (!a.protectRegions.empty());
            expect (!a.focusRegions.empty());
            expect (a.axisRoles[static_cast<std::size_t> (SemanticUiAxis::Warmth)]
                    == SemanticAxisRole::Primary);
        }

        beginTest ("peak and centroid come only from attested contributions, not a visual table");
        {
            const auto plan = SemanticPlanner().plan ("more air", kSampleRate);
            expect (plan.valid);
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.intentAnchors.size(), (int) state.focusEnvelopes.size());
            expect (!state.intentAnchors.empty());
            expect (!state.focusEnvelopes.empty());
            expect ((int) state.focusEnvelopes.front().samples.size()
                        == (int) plan.target.points.size(),
                    "anchors must reuse Envelope 2.0's attested grid, not a 128-bin table");
            expect ((int) plan.target.points.size() != 128);

            const auto expected = attestedAnchorExpect (plan, state.focusEnvelopes.front().sourceId);
            expect (expected.valid);
            const auto& anchor = state.intentAnchors.front();
            expect (anchor.sourceId == state.focusEnvelopes.front().sourceId);
            expect (anchor.sourcePhrase == state.focusEnvelopes.front().sourcePhrase);
            expect (anchor.dimension == state.focusEnvelopes.front().dimension);
            expect (anchor.focus == state.focusEnvelopes.front().focus);
            expect (anchor.primary == state.focusEnvelopes.front().primary);
            expectWithinAbsoluteError (anchor.strength, state.focusEnvelopes.front().strength, 1.0e-6f);
            expectWithinAbsoluteError (anchor.peakFrequencyHz, expected.peakHz, 1.0e-3f);
            expectWithinAbsoluteError (anchor.centroidFrequencyHz, expected.centroidHz, 1.0e-2f);
            expectWithinAbsoluteError (anchor.lowerSupportHz, expected.lowerHz, 1.0e-3f);
            expectWithinAbsoluteError (anchor.upperSupportHz, expected.upperHz, 1.0e-3f);
            expect (std::abs (anchor.peakFrequencyHz - 10000.0f) > 1.0f
                    || expected.peakHz == 10000.0f,
                    "must not hard-code Air = 10 kHz");
        }

        beginTest ("peak tie prefers the lower frequency");
        {
            const auto plan = makeMagnitudePlan (
                SemanticDimension::Warmth, SemanticSpectralFocus::LowMid, 0.9f,
                { 250.0f, 1000.0f, 4000.0f },
                { 0.8f, 0.4f, 0.8f });
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.intentAnchors.size(), 1);
            expectWithinAbsoluteError (state.intentAnchors.front().peakFrequencyHz, 250.0f, 1.0e-4f);
            expect (state.intentAnchors.front().peakFrequencyHz
                    < state.intentAnchors.front().upperSupportHz);
        }

        beginTest ("support bounds span disjoint lobes; centroid uses both");
        {
            const auto plan = makeMagnitudePlan (
                SemanticDimension::Brightness, SemanticSpectralFocus::Air, 0.9f,
                { 500.0f, 1000.0f, 2000.0f, 4000.0f },
                { 0.8f, 0.0f, 0.0f, 0.4f });
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.focusEnvelopes.size(), 1);
            expectEquals ((int) state.intentAnchors.size(), 1);
            const auto& envelope = state.focusEnvelopes.front();
            expectWithinAbsoluteError (envelope.samples[1].normalizedContribution, 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (envelope.samples[2].normalizedContribution, 0.0f, 1.0e-6f);

            const auto& anchor = state.intentAnchors.front();
            expectWithinAbsoluteError (anchor.lowerSupportHz, 500.0f, 1.0e-4f);
            expectWithinAbsoluteError (anchor.upperSupportHz, 4000.0f, 1.0e-4f);
            expectWithinAbsoluteError (anchor.peakFrequencyHz, 500.0f, 1.0e-4f);

            const double w0 = 1.0;
            const double w3 = 0.5;
            const double expectedCentroid = std::exp2 (
                (w0 * std::log2 (500.0) + w3 * std::log2 (4000.0)) / (w0 + w3));
            expectWithinAbsoluteError (anchor.centroidFrequencyHz,
                                      static_cast<float> (expectedCentroid), 1.0e-3f);
            expect (anchor.centroidFrequencyHz > 500.0f);
            expect (anchor.centroidFrequencyHz < 4000.0f);
        }

        beginTest ("two ranked goals emit matching primary and secondary anchors");
        {
            const auto plan = makeTwoGoalPlan();
            const auto state = buildSemanticIntentMapState (plan);
            expectEquals ((int) state.focusEnvelopes.size(), 2);
            expectEquals ((int) state.intentAnchors.size(), 2);
            for (std::size_t i = 0; i < state.intentAnchors.size(); ++i)
            {
                const auto& envelope = state.focusEnvelopes[i];
                const auto& anchor = state.intentAnchors[i];
                expect (anchor.sourceId == envelope.sourceId);
                expect (anchor.sourcePhrase == envelope.sourcePhrase);
                expect (anchor.dimension == envelope.dimension);
                expect (anchor.focus == envelope.focus);
                expect (anchor.primary == envelope.primary);
                expect (anchor.primary == (i == 0));
                expectWithinAbsoluteError (anchor.strength, envelope.strength, 1.0e-6f);

                const auto expected = attestedAnchorExpect (plan, envelope.sourceId);
                expect (expected.valid);
                expectWithinAbsoluteError (anchor.peakFrequencyHz, expected.peakHz, 1.0e-4f);
                expectWithinAbsoluteError (anchor.centroidFrequencyHz, expected.centroidHz, 1.0e-3f);
                expectWithinAbsoluteError (anchor.lowerSupportHz, expected.lowerHz, 1.0e-4f);
                expectWithinAbsoluteError (anchor.upperSupportHz, expected.upperHz, 1.0e-4f);
            }
            expect (state.intentAnchors[0].sourceId != state.intentAnchors[1].sourceId);

            const auto* brightness = anchorForSource (state, "goal:brightness");
            const auto* warmth = anchorForSource (state, "goal:warmth");
            expect (brightness != nullptr && warmth != nullptr);
            if (brightness != nullptr)
            {
                expectWithinAbsoluteError (brightness->lowerSupportHz, 500.0f, 1.0e-4f);
                expectWithinAbsoluteError (brightness->upperSupportHz, 4000.0f, 1.0e-4f);
            }
            if (warmth != nullptr)
            {
                expectWithinAbsoluteError (warmth->lowerSupportHz, 500.0f, 1.0e-4f);
                expectWithinAbsoluteError (warmth->upperSupportHz, 3000.0f, 1.0e-4f);
            }
        }

        beginTest ("NaN Inf non-positive frequency and zero support omit anchors fail-closed");
        {
            const auto nan = std::numeric_limits<float>::quiet_NaN();
            const auto inf = std::numeric_limits<float>::infinity();

            const auto zeros = makeMagnitudePlan (
                SemanticDimension::Punch, SemanticSpectralFocus::General, 0.9f,
                { 500.0f, 1000.0f },
                { 0.0f, 0.0f });
            const auto zeroState = buildSemanticIntentMapState (zeros);
            expect (zeroState.focusEnvelopes.empty());
            expect (zeroState.intentAnchors.empty());

            const auto allInvalid = makeMagnitudePlan (
                SemanticDimension::Clarity, SemanticSpectralFocus::General, 0.9f,
                { nan, inf, -40.0f, 0.0f },
                { 1.0f, 1.0f, 1.0f, 1.0f });
            const auto invalidState = buildSemanticIntentMapState (allInvalid);
            expect (invalidState.intentAnchors.empty());

            const auto infMag = makeMagnitudePlan (
                SemanticDimension::Weight, SemanticSpectralFocus::General, 0.9f,
                { 500.0f, 1000.0f },
                { inf, 0.5f });
            expect (buildSemanticIntentMapState (infMag).intentAnchors.empty());

            const auto mixed = makeMagnitudePlan (
                SemanticDimension::Smoothness, SemanticSpectralFocus::General, 0.9f,
                { nan, 500.0f, inf, 4000.0f, -10.0f },
                { 1.0f, 0.8f, 1.0f, 0.4f, 1.0f });
            const auto mixedState = buildSemanticIntentMapState (mixed);
            expectEquals ((int) mixedState.intentAnchors.size(), 1);
            const auto& mixedAnchor = mixedState.intentAnchors.front();
            expectWithinAbsoluteError (mixedAnchor.peakFrequencyHz, 500.0f, 1.0e-4f);
            expectWithinAbsoluteError (mixedAnchor.lowerSupportHz, 500.0f, 1.0e-4f);
            expectWithinAbsoluteError (mixedAnchor.upperSupportHz, 4000.0f, 1.0e-4f);
            const double expectedCentroid = std::exp2 (
                (1.0 * std::log2 (500.0) + 0.5 * std::log2 (4000.0)) / 1.5);
            expectWithinAbsoluteError (mixedAnchor.centroidFrequencyHz,
                                      static_cast<float> (expectedCentroid), 1.0e-3f);
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
