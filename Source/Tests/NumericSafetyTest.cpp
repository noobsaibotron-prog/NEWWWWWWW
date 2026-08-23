#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/NumericSafety.h"
#include "DSP/ParametricEQProcessor.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

class NumericSafetyTest final : public juce::UnitTest
{
public:
    NumericSafetyTest() : juce::UnitTest("Vintage and limiter numeric safety", "AIEQ-DSP") {}

    void runTest() override
    {
        testVintageProtectedDomainIdentity();
        testVintageClampAndFaults();
        testLimiterShape();
        testLimiterCompatibilityBound();
        testProcessorFaultPublication();
    }

private:
    static uint32_t bits(float value) noexcept
    {
        return std::bit_cast<uint32_t>(value);
    }

    void testVintageProtectedDomainIdentity()
    {
        beginTest("Vintage Padé remains bit-identical inside the protected domain");
        constexpr float drive = 1.2f;
        constexpr float invDrive = 1.0f / drive;
        for (int i = -4096; i <= 4096; ++i)
        {
            const float input = static_cast<float>(i) / 128.0f;
            const float z = input * drive;
            expect(std::abs(z) <= 64.0f);
            const float expected = AIEQDSP::NumericSafety::legacyPade(z) * invDrive;
            bool fault = false;
            const float actual = AIEQDSP::NumericSafety::vintageSaturate(input, fault);
            expect(!fault);
            expect(bits(actual) == bits(expected));
        }
    }

    void testVintageClampAndFaults()
    {
        beginTest("Vintage Padé clamps driven input and rejects non-finite samples");
        constexpr float invDrive = 1.0f / 1.2f;
        const float positiveBound = AIEQDSP::NumericSafety::legacyPade(64.0f) * invDrive;
        bool fault = false;
        const float huge = AIEQDSP::NumericSafety::vintageSaturate(
            std::numeric_limits<float>::max(), fault);
        expect(!fault);
        expect(bits(huge) == bits(positiveBound));

        for (const float invalid : { std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN() })
        {
            fault = false;
            expectEquals(AIEQDSP::NumericSafety::vintageSaturate(invalid, fault), 0.0f);
            expect(fault);
        }
    }

    void testLimiterShape()
    {
        beginTest("Safety limiter is exact below threshold, C1, monotone and bounded");
        bool fault = false;
        for (int i = -8192; i <= 8192; ++i)
        {
            const float input = static_cast<float>(i) / 1024.0f;
            const float output = AIEQDSP::NumericSafety::safetyLimit(input, fault);
            expect(!fault);
            expect(bits(output) == bits(input));
        }

        float previous = -std::numeric_limits<float>::infinity();
        for (int i = -20000; i <= 20000; ++i)
        {
            const float input = static_cast<float>(i) * 0.01f;
            const float output = AIEQDSP::NumericSafety::safetyLimit(input, fault);
            expect(!fault);
            expect(output >= previous);
            expect(std::abs(output) < 40.0f);
            previous = output;
        }

        constexpr float h = 0.001f;
        const float atBoundary = AIEQDSP::NumericSafety::safetyLimit(8.0f, fault);
        const float slopeBelow = (atBoundary
            - AIEQDSP::NumericSafety::safetyLimit(8.0f - h, fault)) / h;
        const float slopeAbove = (AIEQDSP::NumericSafety::safetyLimit(8.0f + h, fault)
            - atBoundary) / h;
        expectWithinAbsoluteError(slopeBelow, 1.0f, 0.002f);
        expectWithinAbsoluteError(slopeAbove, 1.0f, 0.002f);

        for (const float invalid : { std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN() })
        {
            fault = false;
            expectEquals(AIEQDSP::NumericSafety::safetyLimit(invalid, fault), 0.0f);
            expect(fault);
        }
    }

    void testLimiterCompatibilityBound()
    {
        beginTest("Limiter compatibility bound is respected through 0 dBFS");
        double maxError = 0.0;
        bool fault = false;
        for (int i = -100000; i <= 100000; ++i)
        {
            const float input = static_cast<float>(i) / 100000.0f;
            const float legacy = 32.0f * std::tanh(input / 32.0f);
            const float current = AIEQDSP::NumericSafety::safetyLimit(input, fault);
            maxError = std::max(maxError,
                std::abs(static_cast<double>(legacy) - static_cast<double>(current)));
        }
        expect(maxError <= 3.5e-4, "Observed max error " + juce::String(maxError, 9));
    }

    void testProcessorFaultPublication()
    {
        beginTest("Parametric processor flushes and publishes numerical faults");
        ParametricEQProcessor processor;
        processor.prepare(48000.0, 16, 1);
        const int band = processor.addBand(
            200.0f, 6.0f, 0.7f, ParametricEQProcessor::VintageLowShelf);
        processor.setBandVintageMode(band, true);

        juce::AudioBuffer<float> buffer(1, 16);
        buffer.clear();
        buffer.setSample(0, 0, std::numeric_limits<float>::infinity());
        processor.process(buffer);

        expectEquals(buffer.getSample(0, 0), 0.0f);
        expect(processor.getNumericalFaultCount() > 0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect(std::isfinite(buffer.getSample(0, i)));
    }
};

static NumericSafetyTest numericSafetyTest;
