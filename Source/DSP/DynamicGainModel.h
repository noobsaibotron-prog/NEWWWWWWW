#pragma once

#include <algorithm>
#include <cmath>

namespace AIEQDSP::DynamicGainModel
{
enum class Action : int
{
    Off = 0,
    Compress = 1,
    Expand = 2
};

enum class TriggerSide : int
{
    Above = 0,
    Below = 1
};

enum class DetectionMode : int
{
    Peak = 0,
    RMS = 1
};

struct GainResult
{
    double dynamicGainDb = 0.0;
    double effectiveBandGainDb = 0.0;
};

[[nodiscard]] inline double phiAbove(double u, double kneeDb) noexcept
{
    if (!std::isfinite(u))
        return 0.0;

    const double width = std::clamp(
        std::isfinite(kneeDb) ? kneeDb : 0.0, 0.0, 24.0);
    if (width == 0.0)
        return std::max(u, 0.0);

    const double halfWidth = 0.5 * width;
    if (u <= -halfWidth)
        return 0.0;
    if (u >= halfWidth)
        return u;

    const double x = u + halfWidth;
    return (x * x) / (2.0 * width);
}

[[nodiscard]] inline double phiBelow(double u, double kneeDb) noexcept
{
    if (!std::isfinite(u))
        return 0.0;

    const double width = std::clamp(
        std::isfinite(kneeDb) ? kneeDb : 0.0, 0.0, 24.0);
    if (width == 0.0)
        return std::min(u, 0.0);

    const double halfWidth = 0.5 * width;
    if (u <= -halfWidth)
        return u;
    if (u >= halfWidth)
        return 0.0;

    const double x = u - halfWidth;
    return -(x * x) / (2.0 * width);
}

[[nodiscard]] inline GainResult evaluate(double inputLevelDb,
                                         double staticGainDb,
                                         Action action,
                                         TriggerSide trigger,
                                         double thresholdDb,
                                         double ratio,
                                         double kneeDb,
                                         double rangeDb) noexcept
{
    const double safeInput = std::isfinite(inputLevelDb) ? inputLevelDb : -160.0;
    const double threshold = std::clamp(
        std::isfinite(thresholdDb) ? thresholdDb : -60.0, -60.0, 0.0);
    const double safeRatio = std::clamp(
        std::isfinite(ratio) ? ratio : 1.0, 1.0, 20.0);
    const double safeRange = std::clamp(
        std::isfinite(rangeDb) ? rangeDb : 0.0, 0.0, 48.0);
    const double safeStatic = std::clamp(
        std::isfinite(staticGainDb) ? staticGainDb : 0.0, -36.0, 36.0);

    const double u = safeInput - threshold;
    const double phi = trigger == TriggerSide::Above
        ? phiAbove(u, kneeDb)
        : phiBelow(u, kneeDb);

    double rawDelta = 0.0;
    switch (action)
    {
        case Action::Compress:
            rawDelta = -(1.0 - 1.0 / safeRatio) * phi;
            break;
        case Action::Expand:
            rawDelta = (safeRatio - 1.0) * phi;
            break;
        case Action::Off:
            break;
    }

    const double requestedDelta = std::clamp(rawDelta, -safeRange, safeRange);
    const double effectiveGain = std::clamp(safeStatic + requestedDelta, -36.0, 36.0);
    return { effectiveGain - safeStatic, effectiveGain };
}

[[nodiscard]] inline double detectorInput(double left,
                                          double right,
                                          int channels,
                                          DetectionMode mode) noexcept
{
    const double safeLeft = std::isfinite(left) ? left : 0.0;
    const double safeRight = channels > 1 && std::isfinite(right) ? right : safeLeft;

    if (mode == DetectionMode::Peak)
        return std::max(std::abs(safeLeft), std::abs(safeRight));

    if (channels > 1)
        return 0.5 * (safeLeft * safeLeft + safeRight * safeRight);
    return safeLeft * safeLeft;
}

[[nodiscard]] inline double smoothingCoefficient(double timeMs,
                                                 double sampleRate) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        return 0.0;
    if (!std::isfinite(timeMs) || timeMs <= 0.0)
        return 0.0;

    return std::exp(-1.0 / (timeMs * 0.001 * sampleRate));
}

[[nodiscard]] inline double advanceEnvelope(double previous,
                                            double detectorValue,
                                            double attackCoefficient,
                                            double releaseCoefficient) noexcept
{
    const double safePrevious = std::isfinite(previous) && previous >= 0.0
        ? previous : 0.0;
    const double safeDetector = std::isfinite(detectorValue) && detectorValue >= 0.0
        ? detectorValue : 0.0;
    const double coefficient = std::clamp(
        safeDetector > safePrevious ? attackCoefficient : releaseCoefficient,
        0.0, 1.0);
    return coefficient * safePrevious + (1.0 - coefficient) * safeDetector;
}

[[nodiscard]] inline double envelopeToDb(double envelope,
                                         DetectionMode mode) noexcept
{
    const double safeEnvelope = std::isfinite(envelope) && envelope >= 0.0
        ? envelope : 0.0;
    const double levelDb = mode == DetectionMode::Peak
        ? 20.0 * std::log10(std::max(safeEnvelope, 1.0e-8))
        : 10.0 * std::log10(std::max(safeEnvelope, 1.0e-16));
    return std::max(levelDb, -160.0);
}
}
