#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

namespace AIEQDSP
{
enum class TptSvfType : uint8_t
{
    Bell = 0,
    LowShelf,
    HighShelf
};

enum class TptSvfValidationFailure : uint8_t
{
    None = 0,
    InvalidType,
    InvalidSampleRate,
    InvalidFrequency,
    InvalidQ,
    InvalidGain,
    NonFiniteCoefficient
};

/**
 * Immutable Cytomic-style TPT-SVF mix coefficients.
 *
 * The user-facing shelf Q is intentionally inactive in Surgical mode.  Shelf
 * designs use Q = 1/sqrt(2), as frozen by the premium execution authority.
 */
struct TptSvfCoefficients
{
    static constexpr double minSampleRate = 32000.0;
    static constexpr double maxSampleRate = 768000.0;
    static constexpr double minFrequency = 20.0;
    static constexpr double maxUserFrequency = 20000.0;
    static constexpr double minQ = 0.1;
    static constexpr double maxQ = 10.0;
    static constexpr double maxAbsGainDb = 36.0;
    static constexpr double shelfQ = 0.707106781186547524400844362104849039;

    double g = 0.0;
    double k = 1.0;
    double m0 = 1.0;
    double m1 = 1.0;
    double m2 = 1.0;
    double d = 1.0;
    TptSvfValidationFailure failure = TptSvfValidationFailure::InvalidSampleRate;

    [[nodiscard]] bool isValid() const noexcept
    {
        return failure == TptSvfValidationFailure::None;
    }

    [[nodiscard]] bool valuesAreFinite() const noexcept
    {
        return std::isfinite(g) && std::isfinite(k)
            && std::isfinite(m0) && std::isfinite(m1) && std::isfinite(m2)
            && std::isfinite(d) && d > 0.0;
    }

    [[nodiscard]] static TptSvfCoefficients design(
        TptSvfType type, double sampleRate, double frequency,
        double gainDb, double q) noexcept
    {
        TptSvfCoefficients result;
        if (type != TptSvfType::Bell
            && type != TptSvfType::LowShelf
            && type != TptSvfType::HighShelf)
        {
            result.failure = TptSvfValidationFailure::InvalidType;
            return result;
        }

        if (!std::isfinite(sampleRate)
            || sampleRate < minSampleRate || sampleRate > maxSampleRate)
        {
            result.failure = TptSvfValidationFailure::InvalidSampleRate;
            return result;
        }

        const double frequencyUpperBound = std::min(maxUserFrequency, 0.49 * sampleRate);
        if (!std::isfinite(frequency)
            || frequency < minFrequency || frequency > frequencyUpperBound)
        {
            result.failure = TptSvfValidationFailure::InvalidFrequency;
            return result;
        }

        if (!std::isfinite(gainDb) || std::abs(gainDb) > maxAbsGainDb)
        {
            result.failure = TptSvfValidationFailure::InvalidGain;
            return result;
        }

        const bool shelf = type == TptSvfType::LowShelf
                        || type == TptSvfType::HighShelf;
        if (!shelf && (!std::isfinite(q) || q < minQ || q > maxQ))
        {
            result.failure = TptSvfValidationFailure::InvalidQ;
            return result;
        }

        constexpr double pi = 3.141592653589793238462643383279502884;
        const double effectiveQ = shelf ? shelfQ : q;
        const double g0 = std::tan(pi * frequency / sampleRate);
        const double k0 = 1.0 / effectiveQ;
        const double A = std::pow(10.0, gainDb / 40.0);
        const double sqrtA = std::sqrt(A);

        switch (type)
        {
            case TptSvfType::Bell:
                result.g = g0;
                result.k = k0 / A;
                result.m0 = 1.0;
                result.m1 = k0 * A;
                result.m2 = 1.0;
                break;

            case TptSvfType::LowShelf:
                result.g = g0 / sqrtA;
                result.k = k0;
                result.m0 = 1.0;
                result.m1 = k0 * A;
                result.m2 = A * A;
                break;

            case TptSvfType::HighShelf:
                result.g = g0 * sqrtA;
                result.k = k0;
                result.m0 = A * A;
                result.m1 = k0 * A;
                result.m2 = 1.0;
                break;
        }

        result.d = 1.0 + result.g * (result.g + result.k);
        result.failure = result.valuesAreFinite()
            ? TptSvfValidationFailure::None
            : TptSvfValidationFailure::NonFiniteCoefficient;
        return result;
    }

    /** Exact transfer magnitude of the frozen TPT mix at a digital frequency. */
    [[nodiscard]] double getMagnitudeForFrequency(
        double frequency, double sampleRate) const noexcept
    {
        if (!isValid() || !std::isfinite(frequency) || !std::isfinite(sampleRate)
            || sampleRate <= 0.0 || frequency < 0.0 || frequency > sampleRate * 0.5)
            return std::numeric_limits<double>::quiet_NaN();

        if (frequency == 0.0)
            return std::abs(m2);
        if (frequency == sampleRate * 0.5)
            return std::abs(m0);

        constexpr double pi = 3.141592653589793238462643383279502884;
        const std::complex<double> zInverse = std::polar(
            1.0, -2.0 * pi * frequency / sampleRate);
        const std::complex<double> one(1.0, 0.0);
        const std::complex<double> s = (one - zInverse) / (g * (one + zInverse));
        const std::complex<double> sSquared = s * s;
        const std::complex<double> denominator = sSquared + k * s + one;
        if (!std::isfinite(std::abs(denominator))
            || std::abs(denominator) <= std::numeric_limits<double>::min())
            return std::numeric_limits<double>::quiet_NaN();

        const std::complex<double> numerator = m0 * sSquared + m1 * s + m2;
        const double magnitude = std::abs(numerator / denominator);
        return std::isfinite(magnitude)
            ? magnitude : std::numeric_limits<double>::quiet_NaN();
    }
};

/** Two-integrator TPT state.  Audio-thread owned; allocation-free. */
struct TptSvfState
{
    double ic1 = 0.0;
    double ic2 = 0.0;

    void reset() noexcept
    {
        ic1 = 0.0;
        ic2 = 0.0;
    }

    [[nodiscard]] double processSample(
        double input, const TptSvfCoefficients& coefficients,
        bool* fault = nullptr) noexcept
    {
        if (fault != nullptr)
            *fault = false;

        if (!std::isfinite(input))
        {
            reset();
            if (fault != nullptr)
                *fault = true;
            return 0.0;
        }

        if (!coefficients.isValid() || !coefficients.valuesAreFinite())
        {
            reset();
            if (fault != nullptr)
                *fault = true;
            return input;
        }

        const double t0 = input - ic2;
        const double v0 = t0 / coefficients.d
            - (coefficients.g + coefficients.k) * ic1 / coefficients.d;
        const double t1 = coefficients.g * v0;
        const double v1 = ic1 + t1;
        const double t2 = coefficients.g * v1;
        const double v2 = ic2 + t2;

        const double nextIc1 = ic1 + 2.0 * t1;
        const double nextIc2 = ic2 + 2.0 * t2;
        const double output = coefficients.m0 * v0
                            + coefficients.m1 * v1
                            + coefficients.m2 * v2;

        if (!std::isfinite(nextIc1) || !std::isfinite(nextIc2)
            || !std::isfinite(output))
        {
            reset();
            if (fault != nullptr)
                *fault = true;
            return input;
        }

        ic1 = nextIc1;
        ic2 = nextIc2;
        return output;
    }
};
}
