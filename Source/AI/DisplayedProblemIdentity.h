#pragma once

#include "AIEngine.h"
#include <cmath>

namespace EmberAI
{

static_assert (AIEngine::kLivePersistenceFreqMatchOctaves == 0.25f,
               "persist ring and problem-list identity share a quarter-octave match");

/** Same-problem identity for the live persist ring and the AI DETECT list.

    Same type, valid positive finite frequencies, within
    AIEngine::kLivePersistenceFreqMatchOctaves. Gain is not part of identity.
*/
[[nodiscard]] inline bool isSameDisplayedProblem (AIEngine::ProblemType typeA, float freqA,
                                                  AIEngine::ProblemType typeB, float freqB) noexcept
{
    if (typeA != typeB)
        return false;
    if (! std::isfinite (freqA) || ! std::isfinite (freqB))
        return false;
    if (! (freqA > 0.0f) || ! (freqB > 0.0f))
        return false;
    return std::abs (std::log2 (freqA / freqB)) <= AIEngine::kLivePersistenceFreqMatchOctaves;
}

[[nodiscard]] inline bool isSameDisplayedProblem (const AIEngine::Correction& a,
                                                  const AIEngine::Correction& b) noexcept
{
    return isSameDisplayedProblem (a.type, a.frequency, b.type, b.frequency);
}

/** Visual-hold / ghost action policy for one problem row.

    Ghosts stay selectable and inspectable. APPLY paths are closed and must
    not delete the hold.
*/
struct ProblemRowGhostPolicy
{
    bool isGhost = false;

    [[nodiscard]] bool canListen() const noexcept { return true; }
    [[nodiscard]] bool canShowDetails() const noexcept { return true; }
    [[nodiscard]] bool canDismiss() const noexcept { return true; }
    [[nodiscard]] bool canApply() const noexcept { return ! isGhost; }
    [[nodiscard]] bool doubleClickApplies() const noexcept { return ! isGhost; }
    [[nodiscard]] bool contextApplyEnabled() const noexcept { return ! isGhost; }

    /** A refused ghost APPLY must never call removeTransientVisualHold. */
    [[nodiscard]] bool refusedApplyRemovesHold() const noexcept { return false; }
};

} // namespace EmberAI
