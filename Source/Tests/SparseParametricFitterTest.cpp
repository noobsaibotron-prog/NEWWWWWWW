#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SparseParametricFitter.h"
#include "../DSP/BiquadCoefficients.h"

#include <cmath>
#include <functional>
#include <vector>

namespace
{
using namespace AIEQPerceptual;
constexpr double kSampleRate = 48000.0;

float toDb(double magnitude)
{
    return magnitude > 1.0e-12 ? static_cast<float>(20.0 * std::log10(magnitude)) : -120.0f;
}

std::vector<float> makeLogGrid(float low = 30.0f, float high = 18000.0f, int pointsPerOctave = 24)
{
    std::vector<float> frequencies;
    const int count = static_cast<int>(std::ceil(std::log2(high / low) * pointsPerOctave));
    frequencies.reserve(static_cast<size_t>(count + 1));

    for (int i = 0; i <= count; ++i)
    {
        const float frequency = low * std::pow(2.0f, static_cast<float>(i) / pointsPerOctave);
        if (frequency <= high)
            frequencies.push_back(frequency);
    }

    return frequencies;
}

float bandResponseDb(const PlannedBand& band, float frequency)
{
    const float gainLinear = std::pow(10.0f, band.gainDb / 20.0f);
    BiquadCoeffs coeffs;

    switch (band.type)
    {
        case FilterType::LowShelf:
            coeffs = BiquadCoeffs::makeLowShelf(kSampleRate, band.frequencyHz, band.q, gainLinear);
            break;
        case FilterType::Peak:
            coeffs = BiquadCoeffs::makePeakFilter(kSampleRate, band.frequencyHz, band.q, gainLinear);
            break;
        case FilterType::HighShelf:
            coeffs = BiquadCoeffs::makeHighShelf(kSampleRate, band.frequencyHz, band.q, gainLinear);
            break;
    }

    return toDb(coeffs.getMagnitudeForFrequency(frequency, kSampleRate));
}

PerceptualTarget targetFromKnownPlan(std::initializer_list<PlannedBand> bands)
{
    PerceptualTarget target;
    target.maxFilters = 6;
    target.maxBoostDb = 6.0f;
    target.maxCutDb = 6.0f;

    for (float frequency : makeLogGrid())
    {
        float delta = 0.0f;
        for (const auto& band : bands)
            delta += bandResponseDb(band, frequency);
        target.points.push_back({ frequency, delta, 1.0f });
    }

    return target;
}

PerceptualTarget targetFromFunction(const std::function<float(float)>& curve,
                                    int pointsPerOctave = 24)
{
    PerceptualTarget target;
    target.maxFilters = 6;
    target.maxBoostDb = 6.0f;
    target.maxCutDb = 6.0f;

    for (float frequency : makeLogGrid(30.0f, 18000.0f, pointsPerOctave))
    {
        TargetPoint point;
        point.frequencyHz = frequency;
        point.deltaDb = std::clamp(curve(frequency), -5.5f, 5.5f);
        point.confidence = 1.0f;
        target.points.push_back(std::move(point));
    }
    return target;
}

float denseMaxBoundViolation(const PerceptualTarget& target,
                             const std::vector<PlannedBand>& bands,
                             int pointsPerOctave = 192)
{
    float maxViolation = 0.0f;
    for (float frequency : makeLogGrid(30.0f, 18000.0f, pointsPerOctave))
    {
        const float response = SparseParametricFitter::evaluatePlanDb(
            bands, frequency, kSampleRate);
        float minBound = -24.0f;
        float maxBound = 24.0f;
        if (getResponseBoundsAtFrequency(frequency, target.responseBounds,
                                         minBound, maxBound))
        {
            if (response < minBound)
                maxViolation = std::max(maxViolation, minBound - response);
            else if (response > maxBound)
                maxViolation = std::max(maxViolation, response - maxBound);
        }

        float allowed = 0.0f;
        if (isFrequencyProtected(frequency, target.protectedRegions, allowed))
            maxViolation = std::max(maxViolation, std::max(0.0f, std::abs(response) - allowed));
    }
    return maxViolation;
}
} // namespace

class SparseParametricFitterTest final : public juce::UnitTest
{
public:
    SparseParametricFitterTest()
        : juce::UnitTest("Sparse Parametric Fitter", "AI-Diag") {}

    void runTest() override
    {
        beginTest("Zero target returns a valid zero-band plan");
        {
            PerceptualTarget target;
            for (float frequency : makeLogGrid())
                target.points.push_back({ frequency, 0.0f, 1.0f });

            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expectEquals(static_cast<int>(result.bands.size()), 0);
            expectWithinAbsoluteError(result.weightedRmsAfterDb, 0.0f, 1.0e-6f);
        }

        beginTest("Known broad peak is reconstructed sparsely");
        {
            PlannedBand truth;
            truth.type = FilterType::Peak;
            truth.frequencyHz = 800.0f;
            truth.gainDb = 2.5f;
            truth.q = 0.8f;

            const auto result = SparseParametricFitter().fit(
                targetFromKnownPlan({ truth }), kSampleRate);

            expect(result.valid);
            expect(result.bands.size() <= 3);
            expect(result.weightedRmsAfterDb < 0.30f,
                   "Broad peak fit should remain under 0.30 dB weighted RMS");
        }

        beginTest("Three broad tonal moves remain a small editable plan");
        {
            PlannedBand low;
            low.type = FilterType::LowShelf;
            low.frequencyHz = 100.0f;
            low.gainDb = 1.8f;
            low.q = 0.70710678f;

            PlannedBand mid;
            mid.type = FilterType::Peak;
            mid.frequencyHz = 650.0f;
            mid.gainDb = -1.6f;
            mid.q = 1.05f;

            PlannedBand high;
            high.type = FilterType::HighShelf;
            high.frequencyHz = 7000.0f;
            high.gainDb = 1.3f;
            high.q = 0.70710678f;

            const auto result = SparseParametricFitter().fit(
                targetFromKnownPlan({ low, mid, high }), kSampleRate);

            expect(result.valid);
            expect(result.bands.size() <= 6);
            expect(result.weightedRmsAfterDb < 0.40f,
                   "Known three-band target should fit below 0.40 dB weighted RMS");
        }

        beginTest("Protected region is a hard constraint");
        {
            PlannedBand high;
            high.type = FilterType::HighShelf;
            high.frequencyHz = 6000.0f;
            high.gainDb = 3.0f;
            high.q = 0.70710678f;

            auto target = targetFromKnownPlan({ high });
            target.protectedRegions.push_back({ 8000.0f, 18000.0f, 0.25f });

            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expect(result.maxProtectedDeviationDb <= 0.251f,
                   "Fitter must fail closed rather than violate a protected zone");
        }

        beginTest("Directional response bound permits cuts but forbids boosts");
        {
            PlannedBand mid;
            mid.type = FilterType::Peak;
            mid.frequencyHz = 330.0f;
            mid.gainDb = 2.5f;
            mid.q = 0.8f;

            auto target = targetFromKnownPlan({ mid });
            target.responseBounds.push_back({ 220.0f, 500.0f, -24.0f, 0.10f });

            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expect(result.maxResponseBoundViolationDb <= 1.1e-4f);

            // The constraint is one-sided: a cut in the same range remains legal.
            mid.gainDb = -2.5f;
            auto cutTarget = targetFromKnownPlan({ mid });
            cutTarget.responseBounds.push_back({ 220.0f, 500.0f, -24.0f, 0.10f });
            const auto cutResult = SparseParametricFitter().fit(cutTarget, kSampleRate);
            expect(cutResult.valid);
            expect(cutResult.maxResponseBoundViolationDb <= 1.1e-4f);
            expect(cutResult.weightedRmsAfterDb < result.weightedRmsAfterDb,
                   "one-sided bound should allow the legal cut to fit more closely");
        }


        beginTest("Arbitrary smooth tilt + asymmetric lobes fit without biquad-shaped truth");
        {
            auto curve = [](float f)
            {
                const float x = std::log2(f / 1000.0f);
                const float tilt = 0.38f * x;
                const float bump = 1.9f * std::exp(-0.5f * std::pow(std::log2(f / 760.0f) / 0.62f, 2.0f));
                const float dip = -1.4f * std::exp(-0.5f * std::pow(std::log2(f / 3300.0f) / 0.48f, 2.0f));
                const float air = 1.2f * 0.5f * (1.0f + std::tanh(std::log2(f / 9000.0f) / 0.70f));
                return tilt + bump + dip + air;
            };

            const auto target = targetFromFunction(curve);
            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expect(result.bands.size() <= 6);
            expect(result.weightedRmsAfterDb < 0.30f,
                   "arbitrary smooth target should fit below 0.30 dB weighted RMS");
            expect(result.weightedRmsAfterDb < result.weightedRmsBeforeDb * 0.25f);
        }

        beginTest("Exact gain polish improves an overlapping arbitrary target");
        {
            // Frozen non-biquad target found by deterministic search. The lobes
            // overlap enough that greedy one-band-at-a-time gains are measurably
            // sub-optimal unless already-placed gains are revisited.
            auto curve = [](float f)
            {
                auto lobe = [f](float center, float amount, float sigma)
                {
                    return amount * std::exp(-0.5f * std::pow(
                        std::log2(f / center) / sigma, 2.0f));
                };
                return lobe(645.802f,  -2.86163f, 0.568422f)
                     + lobe(1041.09f,  +0.671117f, 0.649896f)
                     + lobe(143.463f,  -2.72001f, 0.569108f)
                     + lobe(10484.5f,  -0.801829f, 0.524579f);
            };
            const auto target = targetFromFunction(curve);

            SparseParametricFitter::Options noPolishOptions;
            noPolishOptions.gainPolishPasses = 0;
            const auto noPolish = SparseParametricFitter(noPolishOptions).fit(target, kSampleRate);
            const auto polished = SparseParametricFitter().fit(target, kSampleRate);

            expect(noPolish.valid && polished.valid);
            expect(polished.weightedRmsAfterDb <= noPolish.weightedRmsAfterDb - 0.01f,
                   "gain polish should recover at least 0.01 dB RMS on the frozen overlap case");
        }

        beginTest("Hard response bounds are validated on a denser grid");
        {
            auto target = targetFromFunction([](float f)
            {
                return 3.0f * std::exp(-0.5f * std::pow(std::log2(f / 3150.0f) / 0.30f, 2.0f));
            });
            target.responseBounds.push_back({ 3400.0f, 5200.0f, -24.0f, 0.12f });

            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expect(result.maxResponseBoundViolationDb <= 1.1e-4f);
            expect(denseMaxBoundViolation(target, result.bands) <= 0.03f,
                   "192 pt/oct independent validation must not reveal material inter-grid overshoot");
        }

        beginTest("Band provenance survives fitting");
        {
            PerceptualTarget target;
            for (float frequency : makeLogGrid())
            {
                TargetPoint point;
                point.frequencyHz = frequency;
                point.deltaDb = 1.6f * std::exp(-0.5f * std::pow(std::log2(frequency / 1200.0f) / 0.75f, 2.0f));
                point.confidence = 1.0f;
                point.contributions.push_back({ "goal:test", "test provenance", point.deltaDb, 1.0f });
                target.points.push_back(std::move(point));
            }

            const auto result = SparseParametricFitter().fit(target, kSampleRate);
            expect(result.valid);
            expect(!result.bands.empty());
            if (!result.bands.empty())
            {
                expect(!result.bands.front().contributions.empty());
                expect(result.bands.front().reason == "test provenance");
            }
        }

        beginTest("Identical target produces deterministic plan");
        {
            PlannedBand truth;
            truth.type = FilterType::Peak;
            truth.frequencyHz = 1200.0f;
            truth.gainDb = -2.2f;
            truth.q = 1.05f;

            const auto target = targetFromKnownPlan({ truth });
            const auto a = SparseParametricFitter().fit(target, kSampleRate);
            const auto b = SparseParametricFitter().fit(target, kSampleRate);

            expectEquals(static_cast<int>(a.bands.size()), static_cast<int>(b.bands.size()));
            expectWithinAbsoluteError(a.weightedRmsAfterDb, b.weightedRmsAfterDb, 1.0e-7f);

            for (size_t i = 0; i < a.bands.size() && i < b.bands.size(); ++i)
            {
                expectEquals(static_cast<int>(a.bands[i].type), static_cast<int>(b.bands[i].type));
                expectWithinAbsoluteError(a.bands[i].frequencyHz, b.bands[i].frequencyHz, 0.0f);
                expectWithinAbsoluteError(a.bands[i].gainDb, b.bands[i].gainDb, 0.0f);
                expectWithinAbsoluteError(a.bands[i].q, b.bands[i].q, 0.0f);
            }
        }
    }
};

static SparseParametricFitterTest gSparseParametricFitterTest;

#endif
