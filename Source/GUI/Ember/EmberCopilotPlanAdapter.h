#pragma once

#include "../../AI/SemanticPlan.h"
#include "EmberPhraseParser.h"

#include <cmath>
#include <vector>

namespace EmberCopilot
{
/** Convert the canonical Semantic fit into the exact amber preview/Apply vector.
    Invalid planner output is rejected as a whole instead of being clamped into a
    different move from the one that was reviewed. */
inline std::vector<EmberGhostBand> ghostsFromPlan(
    const AIEQPerceptual::SemanticPlan& plan)
{
    if (! plan.valid || ! plan.fit.valid || plan.fit.bands.empty())
        return {};

    std::vector<EmberGhostBand> ghosts;
    ghosts.reserve(plan.fit.bands.size());

    for (const auto& band : plan.fit.bands)
    {
        const int type = static_cast<int>(band.type);
        if (! std::isfinite(band.frequencyHz) || ! std::isfinite(band.gainDb)
            || ! std::isfinite(band.q)
            || band.frequencyHz < EmberTokens::minHz
            || band.frequencyHz > EmberTokens::maxHz
            || band.gainDb < -24.0f || band.gainDb > 24.0f
            || band.q < 0.1f || band.q > 10.0f
            || (type != 1 && type != 2 && type != 3))
            return {};

        EmberGhostBand ghost;
        ghost.type = type;
        ghost.hz = band.frequencyHz;
        ghost.db = band.gainDb;
        ghost.q = band.q;
        ghost.chip = juce::String(EmberPhrase::typeChipStem(type)) + " "
                   + EmberPhrase::formatHzChip(ghost.hz) + "/"
                   + EmberPhrase::formatDbChip(ghost.db);
        ghosts.push_back(std::move(ghost));
    }

    return ghosts;
}
} // namespace EmberCopilot
