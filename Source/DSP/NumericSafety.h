#pragma once

#include <algorithm>
#include <cmath>

namespace AIEQDSP::NumericSafety
{
[[nodiscard]] inline float legacyPade(float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
 * Historical Vintage saturation with a bounded Padé input.
 *
 * The arithmetic order inside the protected domain is intentionally frozen.
 * `fault` reports a non-finite input or result; those cases are flushed to 0.
 */
[[nodiscard]] inline float vintageSaturate(float sample, bool& fault) noexcept
{
    fault = false;
    if (!std::isfinite(sample))
    {
        fault = true;
        return 0.0f;
    }

    constexpr float drive = 1.2f;
    constexpr float invDrive = 1.0f / drive;
    const float z = sample * drive;
    const float output = legacyPade(std::clamp(z, -64.0f, 64.0f)) * invDrive;
    if (!std::isfinite(output))
    {
        fault = true;
        return 0.0f;
    }
    return output;
}

/** Identity through +/-8, then a C1 monotone asymptote to +/-40. */
[[nodiscard]] inline float safetyLimit(float input, bool& fault) noexcept
{
    fault = false;
    if (!std::isfinite(input))
    {
        fault = true;
        return 0.0f;
    }

    constexpr float threshold = 8.0f;
    constexpr float curve = 32.0f;
    const float magnitude = std::abs(input);
    if (magnitude <= threshold)
        return input;

    const float limitedMagnitude = threshold
        + curve * std::tanh((magnitude - threshold) / curve);
    const float output = std::copysign(limitedMagnitude, input);
    if (!std::isfinite(output))
    {
        fault = true;
        return 0.0f;
    }
    return output;
}
}
