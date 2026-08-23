#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/BiquadCoefficients.h"
#include "DSP/CutFilterDesigner.h"
#include "DSP/ParametricEQProcessor.h"
#include <array>
#include <cmath>
#include <complex>
#include <limits>

class HighRateBiquadPrecisionTest final : public juce::UnitTest
{
public:
    HighRateBiquadPrecisionTest()
        : juce::UnitTest("High-rate biquad precision", "AIEQ-DSP") {}

    void runTest() override
    {
        testOfficialCutProfilesRemainActive();
        testAllBiquadFamiliesAtHighRate();
        testJuryGuardAgainstRootOracle();
        testMutationsAreObservable();
        testLongResponsesRemainBounded();
        testLegacyFloatCoefficientIdentity();
    }

private:
    static double magnitude(const CutFilterDesigner::Design& design,
                            double frequency, double sampleRate)
    {
        double result = 1.0;
        for (int stage = 0; stage < design.numStages; ++stage)
            result *= design.coefficients[static_cast<size_t>(stage)]
                .getMagnitudeForFrequency(frequency, sampleRate);
        return result;
    }

    static double largestPoleMagnitude(const BiquadCoeffs& c)
    {
        const std::complex<double> discriminant(c.a1 * c.a1 - 4.0 * c.a2, 0.0);
        const auto root = std::sqrt(discriminant);
        const auto p1 = (-c.a1 + root) * 0.5;
        const auto p2 = (-c.a1 - root) * 0.5;
        return std::max(std::abs(p1), std::abs(p2));
    }

    void expectValid(const BiquadCoeffs& c, const juce::String& context)
    {
        expect(c.valid, context + " was rejected with reason "
                        + juce::String(static_cast<int>(c.failure)));
        expect(c.coefficientsAreFinite(), context + " contains non-finite coefficients");
        expect(c.passesJuryGuard(), context + " failed the runtime Jury guard");
    }

    void testOfficialCutProfilesRemainActive()
    {
        beginTest("Official 96x4 and 192x4 cut profiles are active");
        for (const double sampleRate : { 384000.0, 768000.0 })
        {
            const auto hp = CutFilterDesigner::design(
                true, 2, sampleRate, 30.0f, 1.0f, BiquadPrecision::HighPrecision);
            const auto lp = CutFilterDesigner::design(
                false, 2, sampleRate, 18000.0f, 1.0f, BiquadPrecision::HighPrecision);
            expectEquals(hp.numStages, 4, "HP cascade was silently bypassed");
            expectEquals(lp.numStages, 4, "LP cascade was silently bypassed");
            for (int stage = 0; stage < 4; ++stage)
            {
                expectValid(hp.coefficients[static_cast<size_t>(stage)], "HP stage");
                expectValid(lp.coefficients[static_cast<size_t>(stage)], "LP stage");
            }

            const double hpCorner = juce::Decibels::gainToDecibels(
                magnitude(hp, 30.0, sampleRate), -300.0);
            const double lpCorner = juce::Decibels::gainToDecibels(
                magnitude(lp, 18000.0, sampleRate), -300.0);
            expectWithinAbsoluteError(hpCorner, -3.0102999566, 0.02);
            expectWithinAbsoluteError(lpCorner, -3.0102999566, 0.02);

            ParametricEQProcessor processor;
            processor.setHighPrecisionMode(true);
            processor.prepare(sampleRate, 512, 2);
            const int hpBand = processor.addBand(
                30.0f, 0.0f, 1.0f, ParametricEQProcessor::LowCut);
            processor.setBandSlope(hpBand, 2);
            const double published = processor.getMagnitudeForFrequency(30.0f, sampleRate);
            expectWithinAbsoluteError(
                juce::Decibels::gainToDecibels(published, -300.0),
                -3.0102999566, 0.02,
                "Parametric processor published unity because the HP was bypassed");
            expectEquals(
                static_cast<int>(processor.getBandValidationFailure(hpBand)),
                static_cast<int>(BiquadValidationFailure::None),
                "Official high-rate band did not publish a clean validation reason");
        }
    }

    void testAllBiquadFamiliesAtHighRate()
    {
        beginTest("All biquad families remain finite at 384/768 kHz");
        constexpr float gain = 1.9952623149688795f; // +6 dB
        for (const double sampleRate : { 384000.0, 768000.0 })
        {
            for (const float frequency : { 20.0f, 20000.0f })
            {
                const std::array<BiquadCoeffs, 7> sections {
                    BiquadCoeffs::makeHighPass(sampleRate, frequency, 0.70710678f,
                                               BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makeLowPass(sampleRate, frequency, 0.70710678f,
                                              BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makePeakFilter(sampleRate, frequency, 1.0f, gain,
                                                 BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makeLowShelf(sampleRate, frequency, 0.70710678f, gain,
                                               BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makeHighShelf(sampleRate, frequency, 0.70710678f, gain,
                                                BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makeNotch(sampleRate, frequency, 1.0f,
                                            BiquadPrecision::HighPrecision),
                    BiquadCoeffs::makeBandPass(sampleRate, frequency, 1.0f,
                                               BiquadPrecision::HighPrecision)
                };
                for (const auto& section : sections)
                    expectValid(section, "high-rate section");
            }
        }
    }

    void testJuryGuardAgainstRootOracle()
    {
        beginTest("Jury classification agrees with direct pole roots");
        for (const double sampleRate : { 384000.0, 768000.0 })
        {
            for (const float frequency : { 20.0f, 30.0f, 1000.0f, 18000.0f, 20000.0f })
            {
                const auto c = BiquadCoeffs::makePeakFilter(
                    sampleRate, frequency, 10.0f, 15.848931924611133f,
                    BiquadPrecision::HighPrecision);
                expectValid(c, "root-oracle section");
                expect(largestPoleMagnitude(c) < 1.0,
                       "Jury accepted a pole on or outside the unit circle");
            }
        }
    }

    void testMutationsAreObservable()
    {
        beginTest("Old absolute Jury margin and float high-rate path are killed");
        const auto precise = BiquadCoeffs::makeHighPass(
            768000.0, 30.0f, 0.5411961f, BiquadPrecision::HighPrecision);
        expectValid(precise, "precise HP");
        const double j1 = 1.0 + precise.a1 + precise.a2;
        expect(j1 > BiquadCoeffs::juryTolerance(precise.a1, precise.a2));
        expect(j1 < 1.0e-6,
               "Fixture no longer kills restoration of the obsolete 1e-6 margin");

        const auto legacyFloat = BiquadCoeffs::makeHighPass(
            768000.0, 30.0f, 0.5411961f, BiquadPrecision::LegacyFloat);
        expect(!legacyFloat.valid,
               "Fixture no longer kills restoration of float coefficients at high rate");
        expectEquals(static_cast<int>(legacyFloat.failure),
                     static_cast<int>(BiquadValidationFailure::JuryJ1));
    }

    void testLongResponsesRemainBounded()
    {
        beginTest("Long DC/Nyquist responses remain finite and bounded");
        for (const double sampleRate : { 384000.0, 768000.0 })
        {
            const auto hp = BiquadCoeffs::makeHighPass(
                sampleRate, 30.0f, 0.70710678f, BiquadPrecision::HighPrecision);
            const auto lp = BiquadCoeffs::makeLowPass(
                sampleRate, 18000.0f, 0.70710678f, BiquadPrecision::HighPrecision);
            BiquadState hpState;
            BiquadState lpState;
            float hpOut = 0.0f;
            float lpOut = 0.0f;
            const int samples = static_cast<int>(sampleRate); // one second
            double peak = 0.0;
            bool allFinite = true;
            for (int n = 0; n < samples; ++n)
            {
                hpOut = hpState.processSample(0.25f, hp);
                lpOut = lpState.processSample((n & 1) == 0 ? 0.25f : -0.25f, lp);
                peak = std::max(peak, std::max(std::abs(static_cast<double>(hpOut)),
                                               std::abs(static_cast<double>(lpOut))));
                allFinite = allFinite && std::isfinite(hpOut) && std::isfinite(lpOut);
            }
            expect(allFinite, "Long high-rate response produced a non-finite sample");
            expect(peak < 1.0, "Stable high-rate response grew without bound");
            expect(std::abs(hpOut) < 1.0e-4f, "HP DC response did not settle");
            expect(std::abs(lpOut) < 0.01f, "LP Nyquist response did not settle");
        }
    }

    void testLegacyFloatCoefficientIdentity()
    {
        beginTest("Legacy Zero Latency high-pass coefficients remain float-identical");
        constexpr double sampleRate = 48000.0;
        constexpr float frequency = 30.0f;
        constexpr float q = 0.70710678f;
        const float n = std::tan(juce::MathConstants<float>::pi * frequency
                                 / static_cast<float>(sampleRate));
        const float nSq = n * n;
        const float invQ = 1.0f / q;
        const float c1 = 1.0f / (1.0f + invQ * n + nSq);
        const float expectedA1 = c1 * 2.0f * (nSq - 1.0f);
        const float expectedA2 = c1 * (1.0f - invQ * n + nSq);

        const auto actual = BiquadCoeffs::makeHighPass(
            sampleRate, frequency, q, BiquadPrecision::LegacyFloat);
        expectValid(actual, "legacy HP");
        expect(actual.b0 == static_cast<double>(c1));
        expect(actual.b1 == static_cast<double>(c1 * -2.0f));
        expect(actual.b2 == static_cast<double>(c1));
        expect(actual.a1 == static_cast<double>(expectedA1));
        expect(actual.a2 == static_cast<double>(expectedA2));
    }
};

static HighRateBiquadPrecisionTest highRateBiquadPrecisionTest;
