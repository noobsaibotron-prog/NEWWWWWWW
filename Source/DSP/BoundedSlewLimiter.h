#pragma once

#include <algorithm>
#include <cmath>

namespace AIEQDSP
{

/** Sample-count-driven slew limiter for positive quantities expressed in log2.

    The stored state is logarithmic, so the rate is bounded in octaves/second
    independently of the absolute frequency or Q. `advance()` is intended to
    be called once per processing quantum with the exact number of elapsed host
    samples; its endpoint is invariant to how that elapsed interval is split.
*/
class Log2SlewLimiter
{
public:
    void prepare(double newSampleRate, double newMaxOctavesPerSecond) noexcept
    {
        sampleRate = validPositive(newSampleRate) ? newSampleRate : 48000.0;
        setMaximumRate(newMaxOctavesPerSecond);
    }

    void setMaximumRate(double newMaxOctavesPerSecond) noexcept
    {
        maxOctavesPerSecond = validNonNegative(newMaxOctavesPerSecond)
            ? newMaxOctavesPerSecond : 0.0;
    }

    void setCurrentAndTargetValue(double value) noexcept
    {
        if (!validPositive(value))
            return;
        currentLog2 = targetLog2 = std::log2(value);
        moving = false;
    }

    void setTargetValue(double value) noexcept
    {
        if (validPositive(value))
        {
            targetLog2 = std::log2(value);
            moving = true;
        }
    }

    void skip(int numSamples) noexcept
    {
        if (numSamples <= 0 || !moving)
            return;

        const double maxStep = maxOctavesPerSecond
                             * static_cast<double>(numSamples) / sampleRate;
        const double delta = targetLog2 - currentLog2;
        if (std::abs(delta) <= maxStep)
        {
            currentLog2 = targetLog2;
            moving = false;
        }
        else
            currentLog2 += std::copysign(maxStep, delta);
    }

    [[nodiscard]] float getCurrentValue() const noexcept
    {
        return static_cast<float>(std::exp2(currentLog2));
    }

    [[nodiscard]] float getTargetValue() const noexcept
    {
        return static_cast<float>(std::exp2(targetLog2));
    }

    [[nodiscard]] double getCurrentLog2() const noexcept { return currentLog2; }
    [[nodiscard]] double getTargetLog2() const noexcept { return targetLog2; }
    [[nodiscard]] bool isSmoothing() const noexcept { return moving; }

private:
    static bool validPositive(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

    static bool validNonNegative(double value) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }

    double sampleRate = 48000.0;
    double maxOctavesPerSecond = 100.0;
    double currentLog2 = 0.0;
    double targetLog2 = 0.0;
    bool moving = false;
};

/** Sample-count-driven slew limiter for a linear quantity (gain in dB here). */
class LinearSlewLimiter
{
public:
    void prepare(double newSampleRate, double newMaxUnitsPerSecond) noexcept
    {
        sampleRate = validPositive(newSampleRate) ? newSampleRate : 48000.0;
        setMaximumRate(newMaxUnitsPerSecond);
    }

    void setMaximumRate(double newMaxUnitsPerSecond) noexcept
    {
        maxUnitsPerSecond = validNonNegative(newMaxUnitsPerSecond)
            ? newMaxUnitsPerSecond : 0.0;
    }

    void setCurrentAndTargetValue(double value) noexcept
    {
        if (!std::isfinite(value))
            return;
        current = target = value;
        moving = false;
    }

    void setTargetValue(double value) noexcept
    {
        if (std::isfinite(value))
        {
            target = value;
            moving = true;
        }
    }

    void skip(int numSamples) noexcept
    {
        if (numSamples <= 0 || !moving)
            return;

        const double maxStep = maxUnitsPerSecond
                             * static_cast<double>(numSamples) / sampleRate;
        const double delta = target - current;
        if (std::abs(delta) <= maxStep)
        {
            current = target;
            moving = false;
        }
        else
            current += std::copysign(maxStep, delta);
    }

    [[nodiscard]] float getCurrentValue() const noexcept
    {
        return static_cast<float>(current);
    }

    [[nodiscard]] float getTargetValue() const noexcept
    {
        return static_cast<float>(target);
    }

    [[nodiscard]] double getCurrentValueDouble() const noexcept { return current; }
    [[nodiscard]] double getTargetValueDouble() const noexcept { return target; }
    [[nodiscard]] bool isSmoothing() const noexcept { return moving; }

private:
    static bool validPositive(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

    static bool validNonNegative(double value) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }

    double sampleRate = 48000.0;
    double maxUnitsPerSecond = 2400.0;
    double current = 0.0;
    double target = 0.0;
    bool moving = false;
};

} // namespace AIEQDSP
