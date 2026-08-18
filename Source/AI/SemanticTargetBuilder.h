#pragma once

#include "PerceptualTarget.h"
#include "SemanticIntent.h"

namespace AIEQPerceptual
{

/** Converts a compiled semantic intent into an explicit tonal target. */
class SemanticTargetBuilder
{
public:
    struct Options
    {
        float lowFrequencyHz = 30.0f;
        float highFrequencyHz = 18000.0f;
        int pointsPerOctave = 24;
        int maxFilters = 6;
        float maxBoostDb = 4.0f;
        float maxCutDb = 4.0f;
        float preserveToleranceDb = 0.25f;
        float avoidToleranceDb = 0.10f;
    };

    SemanticTargetBuilder();
    explicit SemanticTargetBuilder(Options optionsIn);

    [[nodiscard]] PerceptualTarget build(const SemanticIntent& intent,
                                         double sampleRate) const;

    // Signed dB shape of a +1.0 canonical semantic direction before global
    // safety clamping. Exposed for explanation/provenance, not UI control.
    /** Facet-aware shape. General reproduces the whole-dimension curve. */
    [[nodiscard]] static float evaluateDimensionShapeDb(
        SemanticDimension dimension, SemanticSpectralFocus focus, float frequencyHz) noexcept;

    [[nodiscard]] static float evaluateDimensionShapeDb(
        SemanticDimension dimension, float frequencyHz) noexcept;

    // Stable generic provenance ID written into PerceptualTarget points.
    [[nodiscard]] static const char* sourceIdForDimension(
        SemanticDimension dimension) noexcept;

private:
    Options options;
};

} // namespace AIEQPerceptual
