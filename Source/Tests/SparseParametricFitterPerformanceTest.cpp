#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SparseParametricFitter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
using namespace AIEQPerceptual;

PerceptualTarget makeAdversarialSmoothTarget(double sampleRate)
{
    PerceptualTarget target;
    target.maxFilters = 6;
    target.maxBoostDb = 6.0f;
    target.maxCutDb = 6.0f;

    const float low = 30.0f;
    const float high = std::min(18000.0f, static_cast<float>(sampleRate * 0.45));
    constexpr int pointsPerOctave = 24;
    const int count = static_cast<int>(std::ceil(std::log2(high / low) * pointsPerOctave));

    for (int i = 0; i <= count; ++i)
    {
        const float f = low * std::pow(2.0f, static_cast<float>(i) / pointsPerOctave);
        if (f > high)
            break;

        const float x = std::log2(f / 1000.0f);
        const float tilt = 0.38f * x;
        const float bump = 1.9f * std::exp(-0.5f * std::pow(std::log2(f / 760.0f) / 0.62f, 2.0f));
        const float dip = -1.4f * std::exp(-0.5f * std::pow(std::log2(f / 3300.0f) / 0.48f, 2.0f));
        const float air = 1.2f * 0.5f * (1.0f + std::tanh(std::log2(f / 9000.0f) / 0.70f));

        TargetPoint point;
        point.frequencyHz = f;
        point.deltaDb = std::clamp(tilt + bump + dip + air, -5.5f, 5.5f);
        point.confidence = 1.0f;
        target.points.push_back(std::move(point));
    }

    return target;
}
} // namespace

class SparseParametricFitterPerformanceTest final : public juce::UnitTest
{
public:
    SparseParametricFitterPerformanceTest()
        : juce::UnitTest("Sparse Parametric Fitter Performance", "Performance") {}

    void runTest() override
    {
        beginTest("Adversarial smooth target planning latency");

        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            const auto target = makeAdversarialSmoothTarget(sampleRate);
            SparseParametricFitter fitter;
            std::vector<double> milliseconds;
            milliseconds.reserve(5);

            // One warm-up prevents first-call/runtime noise from dominating the metric.
            const auto warmup = fitter.fit(target, sampleRate);
            expect(warmup.valid);

            std::uint64_t candidateEvaluations = 0;
            for (int run = 0; run < 5; ++run)
            {
                const auto start = std::chrono::steady_clock::now();
                const auto result = fitter.fit(target, sampleRate);
                const auto stop = std::chrono::steady_clock::now();
                expect(result.valid);
                candidateEvaluations = result.candidateEvaluations;
                milliseconds.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
            }

            std::sort(milliseconds.begin(), milliseconds.end());
            const double median = milliseconds[milliseconds.size() / 2];
            const double maximum = milliseconds.back();

            logMessage("  SR=" + juce::String(sampleRate, 0)
                       + " median=" + juce::String(median, 2) + " ms"
                       + " max=" + juce::String(maximum, 2) + " ms"
                       + " candidateEvals=" + juce::String(static_cast<juce::int64>(candidateEvaluations)));

            // This Performance category is explicit/non-default. The ceiling is
            // intentionally generous across CI/debug hardware; the release
            // product target is documented separately as <100 ms median.
            expect(maximum < 1000.0,
                   "Semantic PLAN fitter exceeded the 1 s regression ceiling");
        }
    }
};

static SparseParametricFitterPerformanceTest gSparseParametricFitterPerformanceTest;

#endif
