#include "SemanticContextualizer.h"

#include <algorithm>
#include <cmath>

namespace AIEQPerceptual
{
namespace
{
float clamp11(float x) noexcept { return std::clamp(x, -1.0f, 1.0f); }
float smooth01(float x) noexcept
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

bool dimensionHasReliableStaticContext(SemanticDimension d) noexcept
{
    switch (d)
    {
        case SemanticDimension::Brightness:
        case SemanticDimension::Warmth:
        case SemanticDimension::Clarity:
        case SemanticDimension::Presence:
        case SemanticDimension::Weight:
            return true;
        case SemanticDimension::Smoothness:
        case SemanticDimension::Punch:
        case SemanticDimension::Tightness:
        case SemanticDimension::Count:
            return false;
    }
    return false;
}
} // namespace

float SemanticContextualizer::mudRisk(const SpectralContext& c) const noexcept
{
    if (!c.valid || !(options.axisFullScaleDb > 0.0f))
        return 0.0f;

    // Mud is 250-500 Hz standing proud of what surrounds it, not simply energy
    // down there. Comparing against the warmth zone below and the mid above is
    // what separates a full, warm source from a congested one.
    const float neighbourhood = 0.5f * (c.warmthZoneDb + c.region(SpectralRegion::Mid));
    return clamp11((c.mudZoneDb - neighbourhood) / options.axisFullScaleDb);
}

float SemanticContextualizer::axisPosition(SemanticDimension dimension,
                                            const SpectralContext& c) const noexcept
{
    return axisPosition(dimension, SemanticSpectralFocus::General, c);
}

float SemanticContextualizer::axisPosition(SemanticDimension dimension,
                                            SemanticSpectralFocus focus,
                                            const SpectralContext& c) const noexcept
{
    if (!c.valid || !(options.axisFullScaleDb > 0.0f))
        return 0.0f;

    float db = 0.0f;
    switch (dimension)
    {
        case SemanticDimension::Brightness:
        {
            if (focus == SemanticSpectralFocus::Air)
            {
                // The air question is about 10 kHz and up. Presence energy is
                // not evidence that the request is unnecessary — measured on a
                // source with Presence +7.1 dB and Air -5.9 dB, the old shared
                // axis damped "more air" by 18% while the region the user named
                // was the emptiest part of the spectrum.
                db = c.region(SpectralRegion::Air);

                // Corroboration, not a co-equal vote: if the perceptual rolloff
                // already sits high, the top really is extended.
                const float rolloffOct = std::log2(std::max(1.0f, c.perceptualRolloffHz) / 9000.0f);
                db += 1.5f * std::clamp(rolloffOct, -2.0f, 2.0f);

                // A narrow HF spike is a reason for restraint even on a source
                // that is broadly dark: pushing air would amplify it. This can
                // only reduce a "+air" request, never license a larger one.
                db += 1.2f * std::max(0.0f, c.hfPeakProminenceDb - 4.0f);
                break;
            }
            if (focus == SemanticSpectralFocus::Brilliance)
            {
                db = c.region(SpectralRegion::Brilliance);
                break;
            }
            // General brightness keeps the whole top as the primary signal and
            // uses tilt/centroid only to corroborate it.
            db = 0.40f * c.region(SpectralRegion::Presence)
               + 0.35f * c.region(SpectralRegion::Brilliance)
               + 0.25f * c.region(SpectralRegion::Air);
            db += 0.8f * std::clamp(c.spectralTiltDbPerOctave, -3.0f, 3.0f);
            db += 0.8f * std::clamp(std::log2(std::max(1.0f, c.perceptualCentroidHz) / 1200.0f), -2.0f, 2.0f);
            break;
        }

        case SemanticDimension::Warmth:
        {
            // Two INDEPENDENT reasons to hold back on "warmer", and they must
            // not be summed. Summing lets weights decide the ordering between a
            // source that is already warm and one that is merely congested, and
            // no choice of weights makes that ordering mean anything.
            //
            //   availability - body is already present (120-250 Hz plus bass)
            //   mud risk     - adding body here would worsen congestion
            //
            // Taking the stronger reason is what the sentence actually says:
            // hold back if EITHER applies. Measured on the two equal-energy
            // sources, availability is 0.78 vs 0.15 and mud risk is -0.29 vs
            // 0.93, so the max correctly ranks the muddy source above the warm
            // one while a weighted sum ranked them backwards.
            //
            // Only relevant for a POSITIVE warmth request: for "thinner" the
            // alignment term flips sign and no attenuation is applied, which is
            // right - removing body from a muddy source is the correct move.
            const float availability =
                (0.55f * c.warmthZoneDb + 0.25f * c.region(SpectralRegion::Bass))
                / options.axisFullScaleDb;
            return clamp11(std::max(availability, mudRisk(c)));
        }

        case SemanticDimension::Clarity:
            // Clarity is the absence of low-mid congestion, which is the mud
            // zone specifically rather than the whole region.
            db = -0.85f * c.mudZoneDb + 0.25f * c.region(SpectralRegion::Presence);
            break;

        case SemanticDimension::Presence:
            db = c.region(SpectralRegion::Presence);
            break;

        case SemanticDimension::Weight:
            db = 0.35f * c.region(SpectralRegion::Sub)
               + 0.65f * c.region(SpectralRegion::Bass);
            break;

        case SemanticDimension::Smoothness:
        case SemanticDimension::Punch:
        case SemanticDimension::Tightness:
        case SemanticDimension::Count:
            // Deliberately neutral: a static spectral snapshot carries no
            // transient or dynamic information, and a plausible-looking
            // heuristic here would be worse than none.
            return 0.0f;
    }
    return clamp11(db / options.axisFullScaleDb);
}

float SemanticContextualizer::evidenceConfidence(SemanticDimension dimension,
                                                 SemanticSpectralFocus focus,
                                                 const SpectralContext& context) const noexcept
{
    auto mean2 = [&context](SpectralRegion a, SpectralRegion b)
    {
        return 0.5f * (context.region_confidence(a) + context.region_confidence(b));
    };

    // An explicit facet names its region directly.
    switch (focus)
    {
        case SemanticSpectralFocus::Air:        return context.region_confidence(SpectralRegion::Air);
        case SemanticSpectralFocus::Brilliance: return context.region_confidence(SpectralRegion::Brilliance);
        case SemanticSpectralFocus::Presence:   return context.region_confidence(SpectralRegion::Presence);
        case SemanticSpectralFocus::LowMid:     return context.region_confidence(SpectralRegion::LowMid);
        case SemanticSpectralFocus::Bass:       return context.region_confidence(SpectralRegion::Bass);
        case SemanticSpectralFocus::Sub:        return context.region_confidence(SpectralRegion::Sub);
        case SemanticSpectralFocus::General:    break;
    }

    // Otherwise the dimension implies a span. Averaged rather than minimised:
    // partial evidence across a span is partial grounds to act, and taking the
    // minimum would let one empty region veto a well-observed neighbour.
    switch (dimension)
    {
        case SemanticDimension::Brightness:
            return (context.region_confidence(SpectralRegion::Presence)
                  + context.region_confidence(SpectralRegion::Brilliance)
                  + context.region_confidence(SpectralRegion::Air)) / 3.0f;
        case SemanticDimension::Warmth:  return mean2(SpectralRegion::Bass, SpectralRegion::LowMid);
        case SemanticDimension::Weight:  return mean2(SpectralRegion::Sub, SpectralRegion::Bass);
        case SemanticDimension::Clarity: return mean2(SpectralRegion::LowMid, SpectralRegion::Mid);
        case SemanticDimension::Presence: return mean2(SpectralRegion::Mid, SpectralRegion::Presence);

        case SemanticDimension::Smoothness:
        case SemanticDimension::Punch:
        case SemanticDimension::Tightness:
        case SemanticDimension::Count:
            return context.confidence;
    }
    return context.confidence;
}

ContextualizedSemanticIntent SemanticContextualizer::contextualize(
    const SemanticIntent& input,
    const SpectralContext& context) const
{
    ContextualizedSemanticIntent out;
    out.intent = input;
    out.contextConfidence = context.confidence;
    out.adjustments.reserve(out.intent.goals.size());

    // Layer 1 is a hard yes/no; the graded part lives in `influence` below.
    const bool contextUsable = context.valid && context.hasUsableEvidence;

    for (auto& goal : out.intent.goals)
    {
        SemanticContextAdjustment adjustment;
        adjustment.dimension = goal.dimension;
        adjustment.originalAmount = goal.amount;
        adjustment.adjustedAmount = goal.amount;
        adjustment.axisPosition = axisPosition(goal.dimension, goal.focus, context);

        if (contextUsable && dimensionHasReliableStaticContext(goal.dimension)
            && std::abs(goal.amount) > 1.0e-6f)
        {
            const float direction = goal.amount > 0.0f ? 1.0f : -1.0f;
            const float alignment = direction * adjustment.axisPosition;
            if (alignment > options.deadband)
            {
                const float normalized = (alignment - options.deadband)
                    / std::max(1.0e-6f, 1.0f - options.deadband);
                const float reduction = options.maximumReduction * smooth01(normalized);
                const float fullScale = std::clamp(1.0f - reduction,
                                                   1.0f - options.maximumReduction, 1.0f);

                // Fade the context in over the confidence ramp rather than
                // switching it on. Evidence is consulted for the REGION this
                // goal depends on, so "more air" on a bass finds nothing to go
                // on and the user's requested amount survives intact - which is
                // the correct failure direction: less evidence, less
                // interference, never a larger move than was asked for.
                const float evidence = evidenceConfidence(goal.dimension, goal.focus, context);
                const float influence = smooth01(
                    std::clamp((evidence - options.lowConfidence)
                             / std::max(1.0e-6f, options.highConfidence - options.lowConfidence),
                               0.0f, 1.0f));

                adjustment.scale = 1.0f + influence * (fullScale - 1.0f);
                goal.amount *= adjustment.scale;
                adjustment.adjustedAmount = goal.amount;
                adjustment.contextualized = adjustment.scale < 0.9999f;
                out.contextApplied = out.contextApplied || adjustment.contextualized;
            }
        }

        out.adjustments.push_back(adjustment);
    }

    // Constraints are copied byte/logically unchanged above. Context only scales
    // requested magnitude; it has no authority to weaken safety/preservation.
    return out;
}

} // namespace AIEQPerceptual
