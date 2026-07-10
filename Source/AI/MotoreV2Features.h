#pragma once

// ============================================================================
// Motore v2 — A2 feature extraction (gated)
// ----------------------------------------------------------------------------
// Log-mel bands from a PerceptualFrontEnd rawDb frame. This is a PORT of the
// legacy MLEngine::extractMelBands (MLEngine.cpp) adapted to a dB-domain input
// (rawDb is already 20*log10(|X|/N), clamped [-120, +12]); the shipped MLEngine
// is deliberately NOT touched. Quirks of the legacy math are preserved ON
// PURPOSE — they are part of the parity contract with ml_v2/feature.py:
//   - fftSize = numBins*2 for the binHz mapping (2049 bins -> 4098, not 4096);
//   - `count` counts EVERY bin in [binLow..binHigh], zero-weight ones included;
//   - output dB is mapped to [0,1] INVERTED (0 dB -> 0.0, -100 dB -> 1.0).
// dB -> linear is an explicit pow(10, dB/20): Decibels::decibelsToGain would
// zero out bins at the -120 dB clamp floor (its cutoff default is -100 dB).
//
// Entirely INERT unless AIEQ_ENABLE_MOTORE_V2 is defined.
// ============================================================================

#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

namespace aieq
{

inline float hzToMelV2(float hz) noexcept
{
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
}

inline float melToHzV2(float mel) noexcept
{
    return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
}

/** rawDb (numBins, dB domain) -> numBands log-mel values in [0,1]. */
inline std::vector<float> melBandsFromDb(const float* rawDb, int numBins,
                                         double sampleRate, int numBands = 64)
{
    std::vector<float> melBands(static_cast<size_t>(numBands), 0.0f);
    if (rawDb == nullptr || numBins <= 0)
        return melBands;

    // dB -> linear magnitude, explicit pow (see header note).
    std::vector<float> spectrum(static_cast<size_t>(numBins));
    for (int i = 0; i < numBins; ++i)
        spectrum[static_cast<size_t>(i)] = std::pow(10.0f, rawDb[i] / 20.0f);

    const int fftSize = numBins * 2;                       // legacy quirk
    const float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    const float minMel = hzToMelV2(20.0f);
    const float maxMel = hzToMelV2(std::min(20000.0f, static_cast<float>(sampleRate) * 0.5f));
    const float melStep = (maxMel - minMel) / static_cast<float>(numBands + 1);

    for (int band = 0; band < numBands; ++band)
    {
        const float melLow = minMel + static_cast<float>(band) * melStep;
        const float melCenter = minMel + static_cast<float>(band + 1) * melStep;
        const float melHigh = minMel + static_cast<float>(band + 2) * melStep;

        int binLow = static_cast<int>(melToHzV2(melLow) / binHz);
        int binCenter = static_cast<int>(melToHzV2(melCenter) / binHz);
        int binHigh = static_cast<int>(melToHzV2(melHigh) / binHz);

        binLow = juce::jlimit(0, numBins - 1, binLow);
        binCenter = juce::jlimit(0, numBins - 1, binCenter);
        binHigh = juce::jlimit(0, numBins - 1, binHigh);

        float energy = 0.0f;
        int count = 0;
        for (int bin = binLow; bin <= binHigh; ++bin)
        {
            float weight = 0.0f;
            if (bin < binCenter && binCenter > binLow)
                weight = static_cast<float>(bin - binLow) / static_cast<float>(binCenter - binLow);
            else if (bin >= binCenter && binHigh > binCenter)
                weight = static_cast<float>(binHigh - bin) / static_cast<float>(binHigh - binCenter);

            energy += spectrum[static_cast<size_t>(bin)] * weight;
            ++count;
        }

        if (count > 0)
            energy /= static_cast<float>(count);

        melBands[static_cast<size_t>(band)] = juce::jlimit(0.0f, 1.0f,
            juce::Decibels::gainToDecibels(energy + 1e-10f, -100.0f) / -100.0f);
    }

    return melBands;
}

} // namespace aieq

#endif // AIEQ_ENABLE_MOTORE_V2
