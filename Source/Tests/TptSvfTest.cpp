#include <juce_core/juce_core.h>
#include "DSP/ParametricEQProcessor.h"
#include "DSP/TptSvf.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
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
        testParametricLegacyBitIdentity();
        testParametricProcessorSurgicalRouting();
        testParametricProcessorSurgicalLinearity();
        testCurveModeCrossfadeIsFinite();
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

    void testParametricProcessorSurgicalRouting()
    {
        beginTest("Parametric EQ routes Bell/shelves through the Surgical core");
        constexpr double sampleRate = 48000.0;
        ParametricEQProcessor processor;
        processor.prepare(sampleRate, 256, 1);
        const int band = processor.addBand(
            1000.0f, 18.0f, 2.5f, ParametricEQProcessor::Peak);
        processor.setBandCurveMode(band, ParametricEQProcessor::CurveMode::Surgical);
        expect(processor.getBandCurveMode(band)
            == ParametricEQProcessor::CurveMode::Surgical);

        const auto reference = AIEQDSP::TptSvfCoefficients::design(
            AIEQDSP::TptSvfType::Bell, sampleRate, 1000.0, 18.0, 2.5);
        const float publishedMagnitude = processor.getMagnitudeForFrequency(
            1000.0f, sampleRate);
        expectWithinAbsoluteError(
            static_cast<double>(publishedMagnitude),
            reference.getMagnitudeForFrequency(1000.0, sampleRate), 2.0e-6);

        processor.setBandType(band, ParametricEQProcessor::LowShelf);
        processor.setBandQ(band, 0.1f);
        const float shelfAtLowQ = processor.getMagnitudeForFrequency(1000.0f, sampleRate);
        processor.setBandQ(band, 10.0f);
        const float shelfAtHighQ = processor.getMagnitudeForFrequency(1000.0f, sampleRate);
        expectWithinAbsoluteError(shelfAtLowQ, shelfAtHighQ, 1.0e-7f);
        expectWithinAbsoluteError(
            20.0 * std::log10(static_cast<double>(shelfAtLowQ)), 9.0, 2.0e-5);

        // Exercise the audible route independently of the published response.
        // Add before prepare so there is no legacy topology to crossfade from.
        ParametricEQProcessor audible;
        const int audibleBand = audible.addBand(
            1375.0f, -15.0f, 4.0f, ParametricEQProcessor::Peak);
        audible.setBandCurveMode(
            audibleBand, ParametricEQProcessor::CurveMode::Surgical);
        audible.prepare(sampleRate, 512, 1);

        const auto audibleReference = AIEQDSP::TptSvfCoefficients::design(
            AIEQDSP::TptSvfType::Bell, sampleRate, 1375.0, -15.0, 4.0);
        AIEQDSP::TptSvfState referenceState;
        juce::AudioBuffer<float> audibleBuffer(1, 512);
        std::array<float, 512> expected {};
        for (int sample = 0; sample < audibleBuffer.getNumSamples(); ++sample)
        {
            const float input = 0.31f * std::sin(0.071f * static_cast<float>(sample))
                              + 0.09f * std::cos(0.019f * static_cast<float>(sample));
            audibleBuffer.setSample(0, sample, input);
            expected[static_cast<size_t>(sample)] = static_cast<float>(
                referenceState.processSample(static_cast<double>(input), audibleReference));
        }
        audible.process(audibleBuffer);
        for (int sample = 0; sample < audibleBuffer.getNumSamples(); ++sample)
        {
            expect(std::bit_cast<uint32_t>(audibleBuffer.getSample(0, sample))
                == std::bit_cast<uint32_t>(expected[static_cast<size_t>(sample)]));
        }
    }

    void testParametricLegacyBitIdentity()
    {
        beginTest("Legacy Parametric processing remains bit-identical");
        constexpr int blockSize = 1024;
        ParametricEQProcessor processor;
        processor.prepare(48000.0, blockSize, 1);
        (void) processor.addBand(
            1000.0f, 6.0f, 0.7f, ParametricEQProcessor::Peak);
        expect(processor.getBandCurveMode(0)
            == ParametricEQProcessor::CurveMode::Legacy);

        const auto coefficients = BiquadCoeffs::makePeakFilter(
            48000.0, 1000.0f, 0.7f,
            juce::Decibels::decibelsToGain(6.0f), BiquadPrecision::Automatic);
        expect(coefficients.valid);
        BiquadState referenceState;
        juce::AudioBuffer<float> buffer(1, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float input = 0.2f * std::sin(0.031f * static_cast<float>(sample))
                              + 0.07f * std::cos(0.113f * static_cast<float>(sample));
            buffer.setSample(0, sample, input);
        }

        std::array<float, blockSize> expected {};
        for (int sample = 0; sample < blockSize; ++sample)
            expected[static_cast<size_t>(sample)] = referenceState.processSample(
                buffer.getSample(0, sample), coefficients);
        processor.process(buffer);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            expect(std::bit_cast<uint32_t>(buffer.getSample(0, sample))
                == std::bit_cast<uint32_t>(expected[static_cast<size_t>(sample)]));
        }
    }

    void testParametricProcessorSurgicalLinearity()
    {
        beginTest("Surgical Parametric path is linear and applies no band saturation");
        constexpr int blockSize = 256;
        ParametricEQProcessor lower;
        ParametricEQProcessor upper;
        lower.prepare(48000.0, blockSize, 1);
        upper.prepare(48000.0, blockSize, 1);
        const int lowerBand = lower.addBand(
            800.0f, 24.0f, 3.0f, ParametricEQProcessor::Peak);
        const int upperBand = upper.addBand(
            800.0f, 24.0f, 3.0f, ParametricEQProcessor::Peak);
        lower.setBandCurveMode(lowerBand, ParametricEQProcessor::CurveMode::Surgical);
        upper.setBandCurveMode(upperBand, ParametricEQProcessor::CurveMode::Surgical);

        juce::AudioBuffer<float> lowerBuffer(1, blockSize);
        juce::AudioBuffer<float> upperBuffer(1, blockSize);
        for (int block = 0; block < 32; ++block)
        {
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const int absoluteSample = block * blockSize + sample;
                const float value = 0.2f * std::sin(
                    0.013f * static_cast<float>(absoluteSample));
                lowerBuffer.setSample(0, sample, value);
                upperBuffer.setSample(0, sample, value * 2.0f);
            }

            lower.process(lowerBuffer);
            upper.process(upperBuffer);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                expectWithinAbsoluteError(
                    upperBuffer.getSample(0, sample),
                    lowerBuffer.getSample(0, sample) * 2.0f, 3.0e-6f);
            }
        }
    }

    void testCurveModeCrossfadeIsFinite()
    {
        beginTest("Legacy-to-Surgical topology crossfade remains finite");
        constexpr int blockSize = 256;
        ParametricEQProcessor processor;
        processor.prepare(48000.0, blockSize, 1);
        const int band = processor.addBand(
            1200.0f, 18.0f, 8.0f, ParametricEQProcessor::Peak);
        juce::AudioBuffer<float> buffer(1, blockSize);

        for (int block = 0; block < 8; ++block)
        {
            for (int sample = 0; sample < blockSize; ++sample)
                buffer.setSample(0, sample, 0.5f * std::sin(
                    0.17f * static_cast<float>(block * blockSize + sample)));
            processor.process(buffer);
        }

        processor.beginBandCrossfade(band, 128);
        processor.setBandCurveMode(band, ParametricEQProcessor::CurveMode::Surgical);
        for (int block = 0; block < 16; ++block)
        {
            for (int sample = 0; sample < blockSize; ++sample)
                buffer.setSample(0, sample, 0.5f * std::sin(
                    0.17f * static_cast<float>((block + 8) * blockSize + sample)));
            processor.process(buffer);
            for (int sample = 0; sample < blockSize; ++sample)
                expect(std::isfinite(buffer.getSample(0, sample)));
        }
        expectEquals(static_cast<int>(processor.getNumericalFaultCount()), 0);
    }
};

static TptSvfTest tptSvfTest;
