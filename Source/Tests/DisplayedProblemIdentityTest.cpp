#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/DisplayedProblemIdentity.h"
#include <cmath>
#include <limits>

class DisplayedProblemIdentityTest final : public juce::UnitTest
{
public:
    DisplayedProblemIdentityTest()
        : juce::UnitTest ("Displayed problem identity and ghost-row policy", "AI-Diag")
    {}

    void runTest() override
    {
        using EmberAI::isSameDisplayedProblem;
        using EmberAI::ProblemRowGhostPolicy;
        using Type = AIEngine::ProblemType;

        beginTest ("exact 0.25-octave boundary is the same problem");
        {
            const float f0 = 1000.0f;
            const float fOn = f0 * std::exp2 (AIEngine::kLivePersistenceFreqMatchOctaves);
            expect (std::abs (std::log2 (fOn / f0))
                        <= AIEngine::kLivePersistenceFreqMatchOctaves + 1.0e-6f);
            expect (isSameDisplayedProblem (Type::Resonance, f0, Type::Resonance, fOn));
            expect (isSameDisplayedProblem (make (Type::Resonance, f0, -4.0f),
                                            make (Type::Resonance, fOn, -4.0f)));
        }

        beginTest ("just beyond 0.25 octave is a different problem");
        {
            const float f0 = 1000.0f;
            const float fBeyond = f0 * std::exp2 (AIEngine::kLivePersistenceFreqMatchOctaves + 0.002f);
            expect (std::abs (std::log2 (fBeyond / f0)) > AIEngine::kLivePersistenceFreqMatchOctaves);
            expect (! isSameDisplayedProblem (Type::Resonance, f0, Type::Resonance, fBeyond));
        }

        beginTest ("different correction type never matches");
        {
            expect (! isSameDisplayedProblem (Type::Resonance, 1000.0f, Type::Sibilance, 1000.0f));
            expect (! isSameDisplayedProblem (make (Type::Harshness, 3000.0f, -3.0f),
                                              make (Type::Muddiness, 3000.0f, -3.0f)));
        }

        beginTest ("gain changes do not split identity");
        {
            const auto a = make (Type::Resonance, 800.0f, -1.2f);
            const auto b = make (Type::Resonance, 800.0f, -6.8f);
            expect (isSameDisplayedProblem (a, b));
            expect (isSameDisplayedProblem (a, make (Type::Resonance, 800.0f, 2.5f)));
        }

        beginTest ("zero, negative, NaN and infinite frequencies fail closed");
        {
            const float nan = std::numeric_limits<float>::quiet_NaN();
            const float inf = std::numeric_limits<float>::infinity();
            expect (! isSameDisplayedProblem (Type::Resonance, 0.0f, Type::Resonance, 1000.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, 1000.0f, Type::Resonance, 0.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, -200.0f, Type::Resonance, 1000.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, 1000.0f, Type::Resonance, -50.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, nan, Type::Resonance, 1000.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, 1000.0f, Type::Resonance, nan));
            expect (! isSameDisplayedProblem (Type::Resonance, inf, Type::Resonance, 1000.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, 1000.0f, Type::Resonance, inf));
            expect (! isSameDisplayedProblem (Type::Resonance, -inf, Type::Resonance, 1000.0f));
            expect (! isSameDisplayedProblem (Type::Resonance, nan, Type::Resonance, nan));
            expect (! isSameDisplayedProblem (Type::Resonance, inf, Type::Resonance, inf));
        }

        beginTest ("ghost keeps PRE, details and dismiss; APPLY paths are closed");
        {
            const ProblemRowGhostPolicy ghost { true };
            expect (ghost.canListen());
            expect (ghost.canShowDetails());
            expect (ghost.canDismiss());
            expect (! ghost.canApply());
            expect (! ghost.doubleClickApplies());
            expect (! ghost.contextApplyEnabled());
            expect (! ghost.refusedApplyRemovesHold(),
                    "ghost APPLY/double-click/context apply must not eat the visual hold");
        }

        beginTest ("live APPLY, double-click and context apply stay available");
        {
            const ProblemRowGhostPolicy live { false };
            expect (live.canListen());
            expect (live.canShowDetails());
            expect (live.canDismiss());
            expect (live.canApply());
            expect (live.doubleClickApplies());
            expect (live.contextApplyEnabled());
        }
    }

private:
    static AIEngine::Correction make (AIEngine::ProblemType type, float freq, float gain)
    {
        AIEngine::Correction c;
        c.type = type;
        c.frequency = freq;
        c.suggestedGain = gain;
        return c;
    }
};

static DisplayedProblemIdentityTest displayedProblemIdentityTest;

#endif
