#pragma once

#include "PerceptualTarget.h"

namespace AIEQPerceptual
{

/**
 * Deterministic, offline sparse fitter used by both Semantic and Match.
 *
 * This class is intentionally pure/non-RT. It never touches the audio callback,
 * APVTS, GUI, ML backend, or PerceptualFrontEnd ownership. It converts an
 * already-defined tonal target into a small editable set of parametric filters.
 */
class SparseParametricFitter
{
public:
    struct Options
    {
        float minAbsoluteTargetDb = 0.10f;
        float minAcceptedImprovementDb = 0.025f;
        float minGainDb = 0.10f;
        float qPenalty = 0.003f;
        float filterPenalty = 0.002f;
        int candidateStride = 1;

        // Hard protection/bound validation is performed at this denser
        // log-frequency resolution after a promising candidate is found and on
        // the final plan. The target itself can remain at 24 points/octave.
        int constraintValidationPointsPerOctave = 96;

        // Exact, deterministic post-greedy gain re-optimization. Frequencies,
        // Qs and filter types stay fixed; only gains are revisited.
        int gainPolishPasses = 2;
        int candidateGainRefinementSteps = 2;
    };

    SparseParametricFitter();
    explicit SparseParametricFitter(Options optionsIn);

    [[nodiscard]] FitResult fit(const PerceptualTarget& target,
                                double sampleRate) const;

    [[nodiscard]] static float evaluatePlanDb(const std::vector<PlannedBand>& bands,
                                              float frequencyHz,
                                              double sampleRate) noexcept;

private:
    Options options;
};

} // namespace AIEQPerceptual
