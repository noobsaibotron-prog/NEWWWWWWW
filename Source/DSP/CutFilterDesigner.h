#pragma once

#include "BiquadCoefficients.h"
#include <algorithm>
#include <array>
#include <cmath>

/**
 * Allocation-free Butterworth HP/LP cascade designer.
 *
 * slopeIndex 0/1/2 selects orders 2/4/8 (12/24/48 dB per octave).
 * resonance=1 produces the canonical Butterworth pole Qs. Other values scale
 * every section Q coherently, preserving the user's cut-resonance control
 * without collapsing the higher orders into repeated identical biquads.
 */
struct CutFilterDesigner
{
    static constexpr int maxStages = 4;

    struct Design
    {
        std::array<BiquadCoeffs, maxStages> coefficients {};
        int numStages = 0;
    };

    [[nodiscard]] static Design design(bool highPass,
                                       int slopeIndex,
                                       double sampleRate,
                                       float frequency,
                                       float resonance = 1.0f) noexcept
    {
        Design result;
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0
            || !std::isfinite(frequency) || !std::isfinite(resonance))
            return result;

        const int clampedSlope = slopeIndex < 0 ? 0 : (slopeIndex > 2 ? 2 : slopeIndex);
        const int order = clampedSlope == 0 ? 2 : (clampedSlope == 1 ? 4 : 8);
        result.numStages = order / 2;

        const float safeFrequency = std::max(20.0f,
            std::min(static_cast<float>(sampleRate * 0.499), frequency));
        const float qScale = std::max(0.1f, std::min(8.0f, resonance));

        constexpr double pi = 3.14159265358979323846264338327950288;
        for (int stage = 0; stage < result.numStages; ++stage)
        {
            const double angle = static_cast<double>(2 * stage + 1) * pi
                                 / static_cast<double>(2 * order);
            const float butterworthQ = static_cast<float>(1.0 / (2.0 * std::cos(angle)));
            const float sectionQ = std::max(0.1f,
                std::min(40.0f, butterworthQ * qScale));
            result.coefficients[static_cast<size_t>(stage)] = highPass
                ? BiquadCoeffs::makeHighPass(sampleRate, safeFrequency, sectionQ)
                : BiquadCoeffs::makeLowPass(sampleRate, safeFrequency, sectionQ);
        }

        return result;
    }
};
