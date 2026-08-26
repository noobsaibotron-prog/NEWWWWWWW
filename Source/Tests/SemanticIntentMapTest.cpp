#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticPlanner.h"
#include "../GUI/SemanticIntentMap.h"

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
    }
};

static SemanticIntentMapTest semanticIntentMapTest;

#endif
