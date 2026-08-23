#include <juce_core/juce_core.h>
#include "DSP/TptSvf.h"
#include <algorithm>
#include <cmath>
#include <limits>

class TptSvfTest final : public juce::UnitTest
{
public:
    TptSvfTest() : juce::UnitTest("Surgical TPT-SVF", "AIEQ-DSP") {}

    void runTest() override
    {
        testDomainAndFaultPolicy();
        testZeroGainIdentity();
        testCenterAndAsymptoticGains();
        testShelfMonotonicityAndReciprocity();
        testTimeDomainMatchesTransferFunction();
        testHighRateFiniteProcessing();
    }

private:
    static double db(double magnitude)
    {
        return 20.0 * std::log10(std::max(magnitude, 1.0e-300));
    }

    void testDomainAndFaultPolicy()
    {
        beginTest("Domain validation and fail-closed runtime policy");
        using namespace AIEQDSP;
        expect(!TptSvfCoefficients::design(
            static_cast<TptSvfType>(99), 48000.0, 1000.0, 0.0, 1.0).isValid());
        expect(!TptSvfCoefficients::design(
            TptSvfType::Bell, 31999.0, 1000.0, 0.0, 1.0).isValid());
        expect(!TptSvfCoefficients::design(
            TptSvfType::Bell, 768001.0, 1000.0, 0.0, 1.0).isValid());
        expect(!TptSvfCoefficients::design(
            TptSvfType::Bell, 48000.0, 19.0, 0.0, 1.0).isValid());
        expect(!TptSvfCoefficients::design(
            TptSvfType::Bell, 48000.0, 1000.0, 36.01, 1.0).isValid());
        expect(!TptSvfCoefficients::design(
            TptSvfType::Bell, 48000.0, 1000.0, 0.0, 10.01).isValid());

        const auto valid = TptSvfCoefficients::design(
            TptSvfType::Bell, 48000.0, 1000.0, 6.0, 1.0);
        expect(valid.isValid());

        TptSvfState state;
        bool fault = false;
        expectEquals(state.processSample(
            std::numeric_limits<double>::infinity(), valid, &fault), 0.0);
        expect(fault);
        expectEquals(state.ic1, 0.0);
        expectEquals(state.ic2, 0.0);

        state.ic1 = std::numeric_limits<double>::infinity();
        fault = false;
        expectEquals(state.processSample(0.25, valid, &fault), 0.25);
        expect(fault);
        expectEquals(state.ic1, 0.0);
        expectEquals(state.ic2, 0.0);
    }

    void testZeroGainIdentity()
    {
        beginTest("All Surgical types are unity at zero gain");
        using namespace AIEQDSP;
        for (const auto type : { TptSvfType::Bell,
                                 TptSvfType::LowShelf,
                                 TptSvfType::HighShelf })
        {
            const auto coefficients = TptSvfCoefficients::design(
                type, 48000.0, 1000.0, 0.0, 3.0);
            expect(coefficients.isValid());
            TptSvfState state;
            for (int i = 0; i < 4096; ++i)
            {
                const double input = 0.25 * std::sin(0.017 * static_cast<double>(i))
                                   + 0.1 * std::cos(0.071 * static_cast<double>(i));
                const double output = state.processSample(input, coefficients);
                expectWithinAbsoluteError(output, input, 2.0e-13);
            }
        }
    }

    void testCenterAndAsymptoticGains()
    {
        beginTest("Bell center, shelf corner and asymptotes match the authority");
        using namespace AIEQDSP;
        constexpr double sampleRate = 48000.0;
        constexpr double center = 1000.0;
        for (const double gainDb : { -36.0, -12.0, 6.0, 36.0 })
        {
            const auto bell = TptSvfCoefficients::design(
                TptSvfType::Bell, sampleRate, center, gainDb, 2.0);
            const auto low = TptSvfCoefficients::design(
                TptSvfType::LowShelf, sampleRate, center, gainDb, 9.0);
            const auto high = TptSvfCoefficients::design(
                TptSvfType::HighShelf, sampleRate, center, gainDb, 0.2);
            expect(bell.isValid() && low.isValid() && high.isValid());

            expectWithinAbsoluteError(db(bell.getMagnitudeForFrequency(center, sampleRate)),
                                      gainDb, 2.0e-10);
            expectWithinAbsoluteError(db(low.getMagnitudeForFrequency(center, sampleRate)),
                                      gainDb * 0.5, 2.0e-10);
            expectWithinAbsoluteError(db(high.getMagnitudeForFrequency(center, sampleRate)),
                                      gainDb * 0.5, 2.0e-10);
            expectWithinAbsoluteError(db(low.getMagnitudeForFrequency(0.0, sampleRate)),
                                      gainDb, 2.0e-12);
            expectWithinAbsoluteError(db(low.getMagnitudeForFrequency(sampleRate * 0.5, sampleRate)),
                                      0.0, 2.0e-12);
            expectWithinAbsoluteError(db(high.getMagnitudeForFrequency(0.0, sampleRate)),
                                      0.0, 2.0e-12);
            expectWithinAbsoluteError(db(high.getMagnitudeForFrequency(sampleRate * 0.5, sampleRate)),
                                      gainDb, 2.0e-12);
        }
    }

    void testShelfMonotonicityAndReciprocity()
    {
        beginTest("Surgical shelves are monotone and reciprocal across the grid");
        using namespace AIEQDSP;
        constexpr double sampleRate = 48000.0;
        constexpr double center = 1000.0;
        for (const auto type : { TptSvfType::LowShelf, TptSvfType::HighShelf })
        {
            const auto boost = TptSvfCoefficients::design(
                type, sampleRate, center, 18.0, 0.1);
            const auto cut = TptSvfCoefficients::design(
                type, sampleRate, center, -18.0, 10.0);
            expect(boost.isValid() && cut.isValid());

            double previous = boost.getMagnitudeForFrequency(20.0, sampleRate);
            for (int i = 1; i <= 512; ++i)
            {
                const double ratio = static_cast<double>(i) / 512.0;
                const double frequency = 20.0 * std::pow(1000.0, ratio);
                const double magnitude = boost.getMagnitudeForFrequency(frequency, sampleRate);
                expect(std::isfinite(magnitude));
                if (type == TptSvfType::LowShelf)
                    expect(magnitude <= previous + 2.0e-14);
                else
                    expect(magnitude + 2.0e-14 >= previous);

                const double reciprocal = magnitude
                    * cut.getMagnitudeForFrequency(frequency, sampleRate);
                expectWithinAbsoluteError(reciprocal, 1.0, 2.0e-12);
                previous = magnitude;
            }
        }
    }

    void testTimeDomainMatchesTransferFunction()
    {
        beginTest("Time-domain state update matches the analytical transfer function");
        using namespace AIEQDSP;
        constexpr double sampleRate = 48000.0;
        constexpr int warmupSamples = 48000;
        constexpr int measurementSamples = 48000;
        constexpr double inputAmplitude = 0.125;
        constexpr double pi = 3.141592653589793238462643383279502884;

        for (const auto type : { TptSvfType::Bell,
                                 TptSvfType::LowShelf,
                                 TptSvfType::HighShelf })
        {
            const auto coefficients = TptSvfCoefficients::design(
                type, sampleRate, 1000.0, 18.0, 2.5);
            expect(coefficients.isValid());

            for (const double frequency : { 80.0, 1000.0, 12000.0 })
            {
                TptSvfState state;
                const double omega = 2.0 * pi * frequency / sampleRate;
                for (int n = 0; n < warmupSamples; ++n)
                {
                    const double input = inputAmplitude
                        * std::sin(omega * static_cast<double>(n));
                    (void) state.processSample(input, coefficients);
                }

                double sineProjection = 0.0;
                double cosineProjection = 0.0;
                for (int n = 0; n < measurementSamples; ++n)
                {
                    const int absoluteSample = warmupSamples + n;
                    const double phase = omega * static_cast<double>(absoluteSample);
                    const double input = inputAmplitude * std::sin(phase);
                    const double output = state.processSample(input, coefficients);
                    sineProjection += output * std::sin(phase);
                    cosineProjection += output * std::cos(phase);
                }

                const double measuredAmplitude = 2.0
                    * std::hypot(sineProjection, cosineProjection)
                    / static_cast<double>(measurementSamples);
                const double measuredMagnitude = measuredAmplitude / inputAmplitude;
                const double analyticalMagnitude = coefficients.getMagnitudeForFrequency(
                    frequency, sampleRate);
                expectWithinAbsoluteError(measuredMagnitude, analyticalMagnitude, 2.0e-9);
            }
        }
    }

    void testHighRateFiniteProcessing()
    {
        beginTest("Official 384/768 kHz extremes remain active, finite and bounded");
        using namespace AIEQDSP;
        for (const double sampleRate : { 384000.0, 768000.0 })
        {
            for (const auto type : { TptSvfType::Bell,
                                     TptSvfType::LowShelf,
                                     TptSvfType::HighShelf })
            {
                for (const double frequency : { 20.0, 20000.0 })
                {
                    const auto coefficients = TptSvfCoefficients::design(
                        type, sampleRate, frequency, 36.0, 10.0);
                    expect(coefficients.isValid());
                    TptSvfState state;
                    double maxMagnitude = 0.0;
                    for (int i = 0; i < 131072; ++i)
                    {
                        const double input = i == 0 ? 1.0 : 0.0;
                        const double output = state.processSample(input, coefficients);
                        expect(std::isfinite(output));
                        maxMagnitude = std::max(maxMagnitude, std::abs(output));
                    }
                    expect(maxMagnitude < 1000.0);
                }
            }
        }
    }
};

static TptSvfTest tptSvfTest;
