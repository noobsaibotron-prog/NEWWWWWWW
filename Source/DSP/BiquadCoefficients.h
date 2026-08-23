#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

enum class BiquadPrecision : uint8_t
{
    Automatic = 0,
    LegacyFloat,
    HighPrecision
};

enum class BiquadValidationFailure : uint8_t
{
    None = 0,
    IntentionalBypass,
    InvalidDomain,
    NonFiniteCoefficient,
    JuryJ1,
    JuryJ2,
    JuryJ3
};

/**
 * Zero-allocation normalized biquad coefficients.
 *
 * Coefficients are stored in double so HQ/Natural processing remains
 * representable through 768 kHz.  LegacyFloat deliberately evaluates the
 * historical formulas in float first and promotes the resulting bit-exact
 * values to double; HighPrecision evaluates the same formulas in double.
 */
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    bool valid = false;
    BiquadValidationFailure failure = BiquadValidationFailure::IntentionalBypass;

    [[nodiscard]] static constexpr double maxProcessingSampleRate() noexcept
    {
        return 768000.0;
    }

    [[nodiscard]] static double juryTolerance(double denominatorA1,
                                                double denominatorA2) noexcept
    {
        return 32.0 * std::numeric_limits<double>::epsilon()
             * (1.0 + std::abs(denominatorA1) + std::abs(denominatorA2));
    }

    [[nodiscard]] bool coefficientsAreFinite() const noexcept
    {
        return std::isfinite(b0) && std::isfinite(b1) && std::isfinite(b2)
            && std::isfinite(a1) && std::isfinite(a2);
    }

    [[nodiscard]] bool passesJuryGuard() const noexcept
    {
        if (!coefficientsAreFinite())
            return false;

        const double tau = juryTolerance(a1, a2);
        const double j1 = 1.0 + a1 + a2;
        const double j2 = 1.0 - a1 + a2;
        const double j3 = 1.0 - a2;
        return j1 > tau && j2 > tau && j3 > tau;
    }

    [[nodiscard]] double getMagnitudeForFrequency(double freq,
                                                   double sampleRate) const noexcept
    {
        if (!valid || !std::isfinite(freq) || !std::isfinite(sampleRate)
            || sampleRate <= 0.0)
            return 1.0;

        constexpr std::complex<double> j(0.0, 1.0);
        const std::complex<double> zInv = std::exp(-twoPi * freq * j / sampleRate);
        const std::complex<double> zInv2 = zInv * zInv;
        const std::complex<double> numerator = b0 + b1 * zInv + b2 * zInv2;
        const std::complex<double> denominator = 1.0 + a1 * zInv + a2 * zInv2;
        const double denominatorMagnitude = std::abs(denominator);
        if (!std::isfinite(denominatorMagnitude)
            || denominatorMagnitude <= std::numeric_limits<double>::min())
            return 1.0;

        const double magnitude = std::abs(numerator / denominator);
        return std::isfinite(magnitude) ? magnitude : 1.0;
    }

    static BiquadCoeffs makeHighPass(double sampleRate, float frequency, float Q,
                                     BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q))
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
        {
            const double n = std::tan(pi * static_cast<double>(frequency) / sampleRate);
            const double nSq = n * n;
            const double invQ = 1.0 / static_cast<double>(Q);
            const double c1 = 1.0 / (1.0 + invQ * n + nSq);
            return finalize(c1, -2.0 * c1, c1,
                            2.0 * c1 * (nSq - 1.0),
                            c1 * (1.0 - invQ * n + nSq));
        }

        const float n = std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);
        return finalizeLegacy(c1, c1 * -2.0f, c1,
                              c1 * 2.0f * (nSq - 1.0f),
                              c1 * (1.0f - invQ * n + nSq));
    }

    static BiquadCoeffs makeLowPass(double sampleRate, float frequency, float Q,
                                    BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q))
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
        {
            const double n = 1.0 / std::tan(pi * static_cast<double>(frequency) / sampleRate);
            const double nSq = n * n;
            const double invQ = 1.0 / static_cast<double>(Q);
            const double c1 = 1.0 / (1.0 + invQ * n + nSq);
            return finalize(c1, 2.0 * c1, c1,
                            2.0 * c1 * (1.0 - nSq),
                            c1 * (1.0 - invQ * n + nSq));
        }

        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);
        return finalizeLegacy(c1, c1 * 2.0f, c1,
                              c1 * 2.0f * (1.0f - nSq),
                              c1 * (1.0f - invQ * n + nSq));
    }

    static BiquadCoeffs makePeakFilter(double sampleRate, float frequency, float Q,
                                       float gainLinear,
                                       BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q)
            || !std::isfinite(gainLinear) || gainLinear <= 0.0f)
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
        {
            const double A = std::sqrt(static_cast<double>(gainLinear));
            const double omega = twoPi * static_cast<double>(frequency) / sampleRate;
            const double alpha = std::sin(omega) / (2.0 * static_cast<double>(Q));
            const double c2 = -2.0 * std::cos(omega);
            return normalizeDouble(1.0 + alpha * A, c2, 1.0 - alpha * A,
                                   1.0 + alpha / A, c2, 1.0 - alpha / A);
        }

        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float omega = 2.0f * pif * std::max(frequency, 2.0f)
                            / static_cast<float>(sampleRate);
        const float alpha = std::sin(omega) / (Q * 2.0f);
        const float c2 = -2.0f * std::cos(omega);
        const float alphaTimesA = alpha * A;
        const float alphaOverA = alpha / A;
        return normalizeLegacy(1.0f + alphaTimesA, c2, 1.0f - alphaTimesA,
                               1.0f + alphaOverA, c2, 1.0f - alphaOverA);
    }

    static BiquadCoeffs makeLowShelf(double sampleRate, float frequency, float Q,
                                     float gainLinear,
                                     BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q)
            || !std::isfinite(gainLinear) || gainLinear <= 0.0f)
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
            return makeShelfDouble(false, sampleRate, frequency, Q, gainLinear);

        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float aminus1 = A - 1.0f;
        const float aplus1 = A + 1.0f;
        const float omega = 2.0f * pif * std::max(frequency, 2.0f)
                            / static_cast<float>(sampleRate);
        const float coso = std::cos(omega);
        const float beta = std::sin(omega) * std::sqrt(A) / Q;
        const float aminus1TimesCoso = aminus1 * coso;
        return normalizeLegacy(A * (aplus1 - aminus1TimesCoso + beta),
                               A * 2.0f * (aminus1 - aplus1 * coso),
                               A * (aplus1 - aminus1TimesCoso - beta),
                               aplus1 + aminus1TimesCoso + beta,
                               -2.0f * (aminus1 + aplus1 * coso),
                               aplus1 + aminus1TimesCoso - beta);
    }

    static BiquadCoeffs makeHighShelf(double sampleRate, float frequency, float Q,
                                      float gainLinear,
                                      BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q)
            || !std::isfinite(gainLinear) || gainLinear <= 0.0f)
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
            return makeShelfDouble(true, sampleRate, frequency, Q, gainLinear);

        const float A = std::sqrt(std::max(0.0f, gainLinear));
        const float aminus1 = A - 1.0f;
        const float aplus1 = A + 1.0f;
        const float omega = 2.0f * pif * std::max(frequency, 2.0f)
                            / static_cast<float>(sampleRate);
        const float coso = std::cos(omega);
        const float beta = std::sin(omega) * std::sqrt(A) / Q;
        const float aminus1TimesCoso = aminus1 * coso;
        return normalizeLegacy(A * (aplus1 + aminus1TimesCoso + beta),
                               A * -2.0f * (aminus1 + aplus1 * coso),
                               A * (aplus1 + aminus1TimesCoso - beta),
                               aplus1 - aminus1TimesCoso + beta,
                               2.0f * (aminus1 - aplus1 * coso),
                               aplus1 - aminus1TimesCoso - beta);
    }

    static BiquadCoeffs makeNotch(double sampleRate, float frequency, float Q,
                                  BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        return makeNotchBandOrAllPass(0, sampleRate, frequency, Q, precision);
    }

    static BiquadCoeffs makeBandPass(double sampleRate, float frequency, float Q,
                                     BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        return makeNotchBandOrAllPass(1, sampleRate, frequency, Q, precision);
    }

    static BiquadCoeffs makeAllPass(double sampleRate, float frequency, float Q,
                                    BiquadPrecision precision = BiquadPrecision::Automatic) noexcept
    {
        return makeNotchBandOrAllPass(2, sampleRate, frequency, Q, precision);
    }

    static BiquadCoeffs makeBypass() noexcept
    {
        return {};
    }

    static BiquadCoeffs makeRejected(BiquadValidationFailure reason) noexcept
    {
        return makeInvalid(reason);
    }

private:
    static constexpr double pi = 3.14159265358979323846264338327950288;
    static constexpr double twoPi = 2.0 * pi;
    static constexpr float pif = 3.14159265358979323846f;

    [[nodiscard]] static bool validDomain(double sampleRate, float frequency,
                                          float Q) noexcept
    {
        return std::isfinite(sampleRate) && sampleRate > 0.0
            && sampleRate <= maxProcessingSampleRate()
            && std::isfinite(frequency) && frequency > 0.0f
            && static_cast<double>(frequency) < 0.5 * sampleRate
            && std::isfinite(Q) && Q > 0.0f;
    }

    [[nodiscard]] static bool useHighPrecision(double sampleRate,
                                               BiquadPrecision precision) noexcept
    {
        return precision == BiquadPrecision::HighPrecision
            || (precision == BiquadPrecision::Automatic && sampleRate > 192000.0);
    }

    static BiquadCoeffs makeInvalid(BiquadValidationFailure reason) noexcept
    {
        BiquadCoeffs c;
        c.failure = reason;
        return c;
    }

    static BiquadCoeffs finalize(double b0, double b1, double b2,
                                 double a1, double a2) noexcept
    {
        BiquadCoeffs c;
        c.b0 = b0; c.b1 = b1; c.b2 = b2; c.a1 = a1; c.a2 = a2;

        if (!c.coefficientsAreFinite())
        {
            c.failure = BiquadValidationFailure::NonFiniteCoefficient;
            return c;
        }

        const double tau = juryTolerance(a1, a2);
        if (!(1.0 + a1 + a2 > tau))
            c.failure = BiquadValidationFailure::JuryJ1;
        else if (!(1.0 - a1 + a2 > tau))
            c.failure = BiquadValidationFailure::JuryJ2;
        else if (!(1.0 - a2 > tau))
            c.failure = BiquadValidationFailure::JuryJ3;
        else
        {
            c.valid = true;
            c.failure = BiquadValidationFailure::None;
        }
        return c;
    }

    static BiquadCoeffs finalizeLegacy(float b0, float b1, float b2,
                                       float a1, float a2) noexcept
    {
        return finalize(static_cast<double>(b0), static_cast<double>(b1),
                        static_cast<double>(b2), static_cast<double>(a1),
                        static_cast<double>(a2));
    }

    static BiquadCoeffs normalizeDouble(double b0, double b1, double b2,
                                        double a0, double a1, double a2) noexcept
    {
        if (!std::isfinite(a0) || a0 == 0.0)
            return makeInvalid(BiquadValidationFailure::NonFiniteCoefficient);
        const double invA0 = 1.0 / a0;
        return finalize(b0 * invA0, b1 * invA0, b2 * invA0,
                        a1 * invA0, a2 * invA0);
    }

    static BiquadCoeffs normalizeLegacy(float b0, float b1, float b2,
                                        float a0, float a1, float a2) noexcept
    {
        if (!std::isfinite(a0) || a0 == 0.0f)
            return makeInvalid(BiquadValidationFailure::NonFiniteCoefficient);
        const float invA0 = 1.0f / a0;
        return finalizeLegacy(b0 * invA0, b1 * invA0, b2 * invA0,
                              a1 * invA0, a2 * invA0);
    }

    static BiquadCoeffs makeShelfDouble(bool highShelf, double sampleRate,
                                        float frequency, float Q,
                                        float gainLinear) noexcept
    {
        const double A = std::sqrt(static_cast<double>(gainLinear));
        const double aminus1 = A - 1.0;
        const double aplus1 = A + 1.0;
        const double omega = twoPi * static_cast<double>(frequency) / sampleRate;
        const double coso = std::cos(omega);
        const double beta = std::sin(omega) * std::sqrt(A) / static_cast<double>(Q);
        const double ac = aminus1 * coso;

        if (!highShelf)
            return normalizeDouble(A * (aplus1 - ac + beta),
                                   A * 2.0 * (aminus1 - aplus1 * coso),
                                   A * (aplus1 - ac - beta),
                                   aplus1 + ac + beta,
                                   -2.0 * (aminus1 + aplus1 * coso),
                                   aplus1 + ac - beta);

        return normalizeDouble(A * (aplus1 + ac + beta),
                               A * -2.0 * (aminus1 + aplus1 * coso),
                               A * (aplus1 + ac - beta),
                               aplus1 - ac + beta,
                               2.0 * (aminus1 - aplus1 * coso),
                               aplus1 - ac - beta);
    }

    static BiquadCoeffs makeNotchBandOrAllPass(int kind, double sampleRate,
                                                float frequency, float Q,
                                                BiquadPrecision precision) noexcept
    {
        if (!validDomain(sampleRate, frequency, Q))
            return makeInvalid(BiquadValidationFailure::InvalidDomain);

        if (useHighPrecision(sampleRate, precision))
        {
            const double n = 1.0 / std::tan(pi * static_cast<double>(frequency) / sampleRate);
            const double nSq = n * n;
            const double invQ = 1.0 / static_cast<double>(Q);
            const double c1 = 1.0 / (1.0 + n * invQ + nSq);
            const double cb1 = 2.0 * c1 * (1.0 - nSq);
            const double a2 = c1 * (1.0 - n * invQ + nSq);
            if (kind == 0)
            {
                const double cb0 = c1 * (1.0 + nSq);
                return finalize(cb0, cb1, cb0, cb1, a2);
            }
            if (kind == 1)
            {
                const double b0 = c1 * n * invQ;
                return finalize(b0, 0.0, -b0, cb1, a2);
            }
            return finalize(a2, cb1, 1.0, cb1, a2);
        }

        const float n = 1.0f / std::tan(pif * frequency / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / Q;
        const float c1 = 1.0f / (1.0f + n * invQ + nSq);
        const float cb1 = 2.0f * c1 * (1.0f - nSq);
        const float a2 = c1 * (1.0f - n * invQ + nSq);
        if (kind == 0)
        {
            const float cb0 = c1 * (1.0f + nSq);
            return finalizeLegacy(cb0, cb1, cb0, cb1, a2);
        }
        if (kind == 1)
        {
            const float b0 = c1 * n * invQ;
            return finalizeLegacy(b0, 0.0f, -b0, cb1, a2);
        }
        return finalizeLegacy(a2, cb1, 1.0f, cb1, a2);
    }
};

/** Allocation-free Direct Form II Transposed state. */
struct BiquadState
{
    double v1 = 0.0, v2 = 0.0;

    void reset() noexcept { v1 = v2 = 0.0; }

    [[nodiscard]] float processSample(float input, const BiquadCoeffs& c,
                                      bool* numericFault = nullptr) noexcept
    {
        if (numericFault != nullptr)
            *numericFault = false;

        if (!std::isfinite(input))
        {
            reset();
            if (numericFault != nullptr)
                *numericFault = true;
            return 0.0f;
        }

        if (!c.valid)
            return input;

        const double output = c.b0 * static_cast<double>(input) + v1;
        const double nextV1 = c.b1 * static_cast<double>(input) - c.a1 * output + v2;
        const double nextV2 = c.b2 * static_cast<double>(input) - c.a2 * output;
        if (!std::isfinite(output) || !std::isfinite(nextV1) || !std::isfinite(nextV2))
        {
            reset();
            if (numericFault != nullptr)
                *numericFault = true;
            return input;
        }

        v1 = nextV1;
        v2 = nextV2;
        return static_cast<float>(output);
    }
};
