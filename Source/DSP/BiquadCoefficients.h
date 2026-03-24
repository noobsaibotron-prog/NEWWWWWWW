#pragma once

#include <cmath>
#include <complex>
#include <array>

/**
 * BiquadCoeffs — Zero-allocation biquad coefficient computation
 *
 * Replaces juce::dsp::IIR::Coefficients<float>::Ptr (which heap-allocates
 * a RefCounted object on every call) with a plain POD struct computed on
 * the stack. Replicates JUCE's internal formulas and precision exactly:
 *   - LP, HP, BP, Notch, AllPass: bilinear transform (computed in float to match JUCE)
 *   - Peak, LowShelf, HighShelf: RBJ Audio EQ Cookbook (computed in float to match JUCE)
 *
 * All coefficients are normalized (a0 = 1.0).
 * Transfer function: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
 */
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    bool valid = false; // false = bypass (equivalent to old nullptr check)

    // For GUI: compute magnitude response at a given frequency
    // Uses same complex exponential approach as JUCE's IIR::Coefficients
    [[nodiscard]] double getMagnitudeForFrequency(double freq, double sampleRate) const noexcept
    {
        if (!valid) return 1.0;

        constexpr std::complex<double> j(0.0, 1.0);
        const std::complex<double> zInv = std::exp(-6.28318530717958647692 * freq * j / sampleRate);
        const std::complex<double> zInv2 = zInv * zInv;

        const std::complex<double> numerator = (double)b0 + (double)b1 * zInv + (double)b2 * zInv2;
        const std::complex<double> denominator = 1.0 + (double)a1 * zInv + (double)a2 * zInv2;

        return std::abs(numerator / denominator);
    }

    //==========================================================================
    // Factory methods — exact replicas of JUCE's IIR::ArrayCoefficients<float>
    // Computed in float precision to match JUCE's template instantiation exactly
    //==========================================================================

    // JUCE bilinear transform: n = tan(π*f/sr)
    static BiquadCoeffs makeHighPass(double sampleRate, float frequency, float Q) noexcept
    {
        const float n = std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);

        BiquadCoeffs c;
        c.b0 = c1;
        c.b1 = c1 * -2.0f;
        c.b2 = c1;
        c.a1 = c1 * 2.0f * (nSq - 1.0f);
        c.a2 = c1 * (1.0f - invQ * n + nSq);
        c.valid = true;
        return c;
    }

    // JUCE bilinear transform: n = 1/tan(π*f/sr)
    static BiquadCoeffs makeLowPass(double sampleRate, float frequency, float Q) noexcept
    {
        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);

        BiquadCoeffs c;
        c.b0 = c1;
        c.b1 = c1 * 2.0f;
        c.b2 = c1;
        c.a1 = c1 * 2.0f * (1.0f - nSq);
        c.a2 = c1 * (1.0f - invQ * n + nSq);
        c.valid = true;
        return c;
    }

    // RBJ-style Peak (JUCE's ArrayCoefficients<float>::makePeakFilter)
    static BiquadCoeffs makePeakFilter(double sampleRate, float frequency, float Q, float gainLinear) noexcept
    {
        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float omega = 2.0f * pif * std::max(frequency, 2.0f) / static_cast<float>(sampleRate);
        const float alpha = std::sin(omega) / (Q * 2.0f);
        const float c2 = -2.0f * std::cos(omega);
        const float alphaTimesA = alpha * A;
        const float alphaOverA = alpha / A;

        return normalizeF(1.0f + alphaTimesA, c2, 1.0f - alphaTimesA,
                          1.0f + alphaOverA,  c2, 1.0f - alphaOverA);
    }

    // RBJ-style LowShelf (JUCE's ArrayCoefficients<float>::makeLowShelf)
    static BiquadCoeffs makeLowShelf(double sampleRate, float frequency, float Q, float gainLinear) noexcept
    {
        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float aminus1 = A - 1.0f;
        const float aplus1 = A + 1.0f;
        const float omega = 2.0f * pif * std::max(frequency, 2.0f) / static_cast<float>(sampleRate);
        const float coso = std::cos(omega);
        const float beta = std::sin(omega) * std::sqrt(A) / Q;
        const float aminus1TimesCoso = aminus1 * coso;

        return normalizeF(A * (aplus1 - aminus1TimesCoso + beta),
                          A * 2.0f * (aminus1 - aplus1 * coso),
                          A * (aplus1 - aminus1TimesCoso - beta),
                          aplus1 + aminus1TimesCoso + beta,
                          -2.0f * (aminus1 + aplus1 * coso),
                          aplus1 + aminus1TimesCoso - beta);
    }

    // RBJ-style HighShelf (JUCE's ArrayCoefficients<float>::makeHighShelf)
    static BiquadCoeffs makeHighShelf(double sampleRate, float frequency, float Q, float gainLinear) noexcept
    {
        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float aminus1 = A - 1.0f;
        const float aplus1 = A + 1.0f;
        const float omega = 2.0f * pif * std::max(frequency, 2.0f) / static_cast<float>(sampleRate);
        const float coso = std::cos(omega);
        const float beta = std::sin(omega) * std::sqrt(A) / Q;
        const float aminus1TimesCoso = aminus1 * coso;

        return normalizeF(A * (aplus1 + aminus1TimesCoso + beta),
                          A * -2.0f * (aminus1 + aplus1 * coso),
                          A * (aplus1 + aminus1TimesCoso - beta),
                          aplus1 - aminus1TimesCoso + beta,
                          2.0f * (aminus1 - aplus1 * coso),
                          aplus1 - aminus1TimesCoso - beta);
    }

    // JUCE bilinear transform: n = 1/tan(π*f/sr)
    static BiquadCoeffs makeNotch(double sampleRate, float frequency, float Q) noexcept
    {
        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + n * invQ + nSq);
        const float cb0 = c1 * (1.0f + nSq);
        const float cb1 = 2.0f * c1 * (1.0f - nSq);

        BiquadCoeffs c;
        c.b0 = cb0;
        c.b1 = cb1;
        c.b2 = cb0;
        c.a1 = cb1;
        c.a2 = c1 * (1.0f - n * invQ + nSq);
        c.valid = true;
        return c;
    }

    // JUCE bilinear transform: n = 1/tan(π*f/sr)
    static BiquadCoeffs makeBandPass(double sampleRate, float frequency, float Q) noexcept
    {
        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);

        BiquadCoeffs c;
        c.b0 = c1 * n * invQ;
        c.b1 = 0.0f;
        c.b2 = -c1 * n * invQ;
        c.a1 = c1 * 2.0f * (1.0f - nSq);
        c.a2 = c1 * (1.0f - invQ * n + nSq);
        c.valid = true;
        return c;
    }

    // JUCE bilinear transform: n = 1/tan(π*f/sr)
    static BiquadCoeffs makeAllPass(double sampleRate, float frequency, float Q) noexcept
    {
        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);
        const float cb0 = c1 * (1.0f - n * invQ + nSq);
        const float cb1 = c1 * 2.0f * (1.0f - nSq);

        BiquadCoeffs c;
        c.b0 = cb0;
        c.b1 = cb1;
        c.b2 = 1.0f;
        c.a1 = cb1;
        c.a2 = cb0;
        c.valid = true;
        return c;
    }

    //==========================================================================
    // Bypass (unity gain, no processing needed)
    //==========================================================================
    static BiquadCoeffs makeBypass() noexcept
    {
        return {}; // valid = false
    }

private:
    static constexpr float pif = 3.14159265358979323846f;

    // Normalize by a0 — used for Peak/LowShelf/HighShelf where a0 != 1
    static BiquadCoeffs normalizeF(float b0, float b1, float b2,
                                    float a0, float a1, float a2) noexcept
    {
        BiquadCoeffs c;
        const float invA0 = 1.0f / a0;
        c.b0 = b0 * invA0;
        c.b1 = b1 * invA0;
        c.b2 = b2 * invA0;
        c.a1 = a1 * invA0;
        c.a2 = a2 * invA0;
        c.valid = true;
        return c;
    }
};

//==============================================================================
/**
 * BiquadState — Minimal Direct Form II Transposed filter state
 *
 * Replaces juce::dsp::IIR::Filter<float> which internally holds a
 * Coefficients::Ptr (heap-allocated). This is a pure value type with
 * zero allocations.
 */
struct BiquadState
{
    float v1 = 0.0f, v2 = 0.0f;

    void reset() noexcept { v1 = v2 = 0.0f; }

    [[nodiscard]] float processSample(float input, const BiquadCoeffs& c) noexcept
    {
        // Direct Form II Transposed (matches JUCE's IIR::Filter implementation)
        const float output = c.b0 * input + v1;
        v1 = c.b1 * input - c.a1 * output + v2;
        v2 = c.b2 * input - c.a2 * output;
        return output;
    }
};
