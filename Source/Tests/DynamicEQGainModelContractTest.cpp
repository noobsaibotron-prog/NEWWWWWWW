#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/BiquadCoefficients.h"
#include "../DSP/DynamicEQProcessor.h"
#include "../DSP/ParametricEQProcessor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

class DynamicEQGainModelContractTest : public juce::UnitTest
{
public:
    DynamicEQGainModelContractTest()
        : juce::UnitTest("DynamicEQ Gain Model Contract", "DSP") {}

    void runTest() override
    {
        testPeakTracksEffectiveGainAtCenterFrequency();
        testDynamicEQStaticPeakTracksGainAtCenterFrequency();
        testPeakMagnitudeStaysMonotonicAcrossEffectiveGainSweep();
        testPeakPhaseRemainsContinuousThroughZeroCrossing();
        testPeakPhaseRemainsContinuousNearZeroCrossing();
        testPeakMirrorSymmetryAcrossSign();
        testStaticLowShelfMatchesParametricReference();
        testStaticHighShelfMatchesParametricReference();
        testStaticVintageLowShelfMatchesParametricReference();
        testStaticVintageHighShelfMatchesParametricReference();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kChannels = 2;
    static constexpr float kPeakFreq = 1000.0f;

    static std::complex<double> evalH(const BiquadCoeffs& coeffs,
                                      double freq,
                                      double sampleRate) noexcept
    {
        if (!coeffs.valid)
            return { 1.0, 0.0 };

        constexpr std::complex<double> kJ(0.0, 1.0);
        const auto zInv = std::exp(-juce::MathConstants<double>::twoPi * freq * kJ / sampleRate);
        const auto zInv2 = zInv * zInv;
        const std::complex<double> num = static_cast<double>(coeffs.b0)
                                       + static_cast<double>(coeffs.b1) * zInv
                                       + static_cast<double>(coeffs.b2) * zInv2;
        const std::complex<double> den = 1.0
                                       + static_cast<double>(coeffs.a1) * zInv
                                       + static_cast<double>(coeffs.a2) * zInv2;
        return num / den;
    }

    static double dbFromMag(double mag)
    {
        return juce::Decibels::gainToDecibels(static_cast<float>(mag), -120.0f);
    }

    static double unwrapNear(double previous, double current)
    {
        while ((current - previous) > juce::MathConstants<double>::pi)
            current -= juce::MathConstants<double>::twoPi;
        while ((current - previous) < -juce::MathConstants<double>::pi)
            current += juce::MathConstants<double>::twoPi;
        return current;
    }

    static std::vector<float> makeLogGrid(size_t points,
                                          float lowHz = 20.0f,
                                          float highHz = 20000.0f)
    {
        std::vector<float> freqs(points);
        const double lowLog = std::log10(static_cast<double>(lowHz));
        const double highLog = std::log10(static_cast<double>(highHz));
        for (size_t i = 0; i < points; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(points - 1);
            freqs[i] = static_cast<float>(std::pow(10.0, lowLog + (highLog - lowLog) * t));
        }
        return freqs;
    }

    static BiquadCoeffs makePeakContractCoeffs(float effectiveGainDb)
    {
        if (std::abs(effectiveGainDb) < 0.05f)
            return BiquadCoeffs::makeBypass();

        return BiquadCoeffs::makePeakFilter(
            kSampleRate,
            kPeakFreq,
            1.0f,
            juce::Decibels::decibelsToGain(effectiveGainDb));
    }

    static std::unique_ptr<DynamicEQProcessor> makeDynamicStaticProcessor(int filterType,
                                                                          float freq,
                                                                          float gainDb,
                                                                          float q)
    {
        auto proc = std::make_unique<DynamicEQProcessor>();
        proc->prepare(kSampleRate, kBlockSize, kChannels);

        for (int i = 0; i < DynamicEQProcessor::maxBands; ++i)
        {
            DynamicEQProcessor::DynamicBandParams disabled;
            disabled.enabled = false;
            disabled.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
            proc->setBandParams(i, disabled);
        }

        DynamicEQProcessor::DynamicBandParams band;
        band.frequency = freq;
        band.gain = gainDb;
        band.q = q;
        band.filterType = filterType;
        band.enabled = true;
        band.dynamicMode = DynamicEQProcessor::DynamicMode_Off;
        proc->setBandParams(0, band);

        juce::AudioBuffer<float> silence(kChannels, kBlockSize);
        silence.clear();
        proc->process(silence);
        return proc;
    }

    static std::unique_ptr<ParametricEQProcessor> makeParametricReference(int filterType,
                                                                          float freq,
                                                                          float gainDb,
                                                                          float q)
    {
        auto proc = std::make_unique<ParametricEQProcessor>();
        proc->prepare(kSampleRate, kBlockSize, kChannels);
        const int band = proc->addBand(freq, gainDb, q, filterType);
        proc->setBandEnabled(band, true);
        return proc;
    }

    void expectPointwiseMatchAgainstParametric(const juce::String& label,
                                               int filterType,
                                               float bandFreq,
                                               float q,
                                               float passbandProbe)
    {
        const auto freqGrid = makeLogGrid(129);
        const std::array<float, 7> gains { -24.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 24.0f };

        float previousPassbandDb = 0.0f;
        bool hasPrevious = false;

        for (float gainDb : gains)
        {
            auto dynProc = makeDynamicStaticProcessor(filterType, bandFreq, gainDb, q);
            auto refProc = makeParametricReference(filterType, bandFreq, gainDb, q);

            float maxAbsDiffDb = 0.0f;
            for (float freqHz : freqGrid)
            {
                const double dynDb = dbFromMag(dynProc->getMagnitudeForFrequency(freqHz, kSampleRate));
                const double refDb = dbFromMag(refProc->getMagnitudeForFrequency(freqHz, kSampleRate));
                maxAbsDiffDb = std::max(maxAbsDiffDb,
                    static_cast<float>(std::abs(dynDb - refDb)));
            }

            const float passbandDb = static_cast<float>(
                dbFromMag(dynProc->getMagnitudeForFrequency(passbandProbe, kSampleRate)));
            logMessage(label + " gain=" + juce::String(gainDb, 1)
                       + " maxAbsDiffDb=" + juce::String(maxAbsDiffDb, 4)
                       + " passbandDb=" + juce::String(passbandDb, 3));

            expect(maxAbsDiffDb < 0.05f,
                   label + " must match ParametricEQProcessor across the grid");

            if (hasPrevious)
                expect(passbandDb > previousPassbandDb - 0.05f,
                       label + " passband magnitude should increase monotonically with effective gain");

            previousPassbandDb = passbandDb;
            hasPrevious = true;
        }
    }

    void testPeakTracksEffectiveGainAtCenterFrequency()
    {
        beginTest("Peak @ f0 tracks effectiveGainDb");

        const std::array<float, 9> gains { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f,
                                           6.0f, 12.0f, 18.0f, 24.0f };

        for (float gainDb : gains)
        {
            const auto coeffs = makePeakContractCoeffs(gainDb);
            const auto response = evalH(coeffs, kPeakFreq, kSampleRate);
            const double measuredDb = dbFromMag(std::abs(response));

            expectWithinAbsoluteError(static_cast<float>(measuredDb), gainDb, 0.05f,
                                      "Peak magnitude at f0 should match effective gain");
            expect(std::real(response) > 0.0, "Peak response at f0 must stay non-negative");
        }
    }

    void testPeakMagnitudeStaysMonotonicAcrossEffectiveGainSweep()
    {
        beginTest("Peak magnitude at f0 is monotonic across effective gain sweep");

        const std::array<float, 9> gains { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f,
                                           6.0f, 12.0f, 18.0f, 24.0f };

        double previousDb = -200.0;
        for (float gainDb : gains)
        {
            const auto coeffs = makePeakContractCoeffs(gainDb);
            const auto response = evalH(coeffs, kPeakFreq, kSampleRate);
            const double measuredDb = dbFromMag(std::abs(response));

            expect(measuredDb > previousDb - 0.05,
                   "Peak center-band magnitude should grow monotonically with effective gain");
            previousDb = measuredDb;
        }
    }

    void testDynamicEQStaticPeakTracksGainAtCenterFrequency()
    {
        beginTest("DynamicEQProcessor static Peak tracks gain at f0");

        const std::array<float, 5> gains { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };

        for (float gainDb : gains)
        {
            auto proc = makeDynamicStaticProcessor(
                static_cast<int>(ParametricEQProcessor::Peak),
                kPeakFreq,
                gainDb,
                1.0f);

            const float magnitude = proc->getMagnitudeForFrequency(kPeakFreq, kSampleRate);
            const float measuredDb = static_cast<float>(dbFromMag(magnitude));
            expectWithinAbsoluteError(measuredDb, gainDb, 0.05f,
                                      "DynamicEQProcessor static Peak must match the requested gain at f0");
        }
    }

    void testPeakPhaseRemainsContinuousThroughZeroCrossing()
    {
        beginTest("Peak phase remains continuous through 0 dB crossing");

        const float probeFreq = kPeakFreq * 1.25f;
        bool hasPrevious = false;
        double previousPhase = 0.0;

        for (int gainDb = -24; gainDb <= 24; ++gainDb)
        {
            const auto coeffs = makePeakContractCoeffs(static_cast<float>(gainDb));
            const auto response = evalH(coeffs, probeFreq, kSampleRate);
            const double phase = std::arg(response);

            if (hasPrevious)
            {
                const double unwrapped = unwrapNear(previousPhase, phase);
                const double step = std::abs(unwrapped - previousPhase);
                expect(step < 0.2,
                       "Peak phase should evolve smoothly when effective gain crosses 0 dB");
                previousPhase = unwrapped;
            }
            else
            {
                previousPhase = phase;
                hasPrevious = true;
            }
        }
    }

    void testPeakPhaseRemainsContinuousNearZeroCrossing()
    {
        beginTest("Peak phase remains continuous near 0 dB crossing");

        const float probeFreq = kPeakFreq * 1.25f;
        bool hasPrevious = false;
        double previousPhase = 0.0;

        for (int tenthDb = -20; tenthDb <= 20; ++tenthDb)
        {
            const auto coeffs = makePeakContractCoeffs(static_cast<float>(tenthDb) * 0.1f);
            const auto response = evalH(coeffs, probeFreq, kSampleRate);
            const double phase = std::arg(response);

            if (hasPrevious)
            {
                const double unwrapped = unwrapNear(previousPhase, phase);
                const double step = std::abs(unwrapped - previousPhase);
                expect(step < 0.2,
                       "Peak phase should remain smooth in a fine sweep around 0 dB");
                previousPhase = unwrapped;
            }
            else
            {
                previousPhase = phase;
                hasPrevious = true;
            }
        }
    }

    void testPeakMirrorSymmetryAcrossSign()
    {
        beginTest("Peak mirror symmetry across sign");

        const std::array<std::pair<float, float>, 2> targetPairs {{
            { 6.0f, -6.0f },
            { 18.0f, -18.0f }
        }};

        for (const auto& pair : targetPairs)
        {
            auto pos = makeDynamicStaticProcessor(
                static_cast<int>(ParametricEQProcessor::Peak), kPeakFreq, pair.first, 1.0f);
            auto neg = makeDynamicStaticProcessor(
                static_cast<int>(ParametricEQProcessor::Peak), kPeakFreq, pair.second, 1.0f);

            const float posDb = static_cast<float>(
                dbFromMag(pos->getMagnitudeForFrequency(kPeakFreq, kSampleRate)));
            const float negDb = static_cast<float>(
                dbFromMag(neg->getMagnitudeForFrequency(kPeakFreq, kSampleRate)));

            expectWithinAbsoluteError(posDb, -negDb, 0.05f,
                                      "Mirror-sign Peak responses should be symmetric in dB at f0");
        }
    }

    void testStaticLowShelfMatchesParametricReference()
    {
        beginTest("DynamicEQ LowShelf static magnitude matches ParametricEQ reference");
        expectPointwiseMatchAgainstParametric("LowShelf",
                                              static_cast<int>(ParametricEQProcessor::LowShelf),
                                              120.0f,
                                              0.8f,
                                              40.0f);
    }

    void testStaticHighShelfMatchesParametricReference()
    {
        beginTest("DynamicEQ HighShelf static magnitude matches ParametricEQ reference");
        expectPointwiseMatchAgainstParametric("HighShelf",
                                              static_cast<int>(ParametricEQProcessor::HighShelf),
                                              4000.0f,
                                              0.8f,
                                              12000.0f);
    }

    void testStaticVintageLowShelfMatchesParametricReference()
    {
        beginTest("DynamicEQ VintageLowShelf static magnitude matches ParametricEQ reference");
        expectPointwiseMatchAgainstParametric("VintageLowShelf",
                                              static_cast<int>(ParametricEQProcessor::VintageLowShelf),
                                              120.0f,
                                              0.8f,
                                              40.0f);
    }

    void testStaticVintageHighShelfMatchesParametricReference()
    {
        beginTest("DynamicEQ VintageHighShelf static magnitude matches ParametricEQ reference");
        expectPointwiseMatchAgainstParametric("VintageHighShelf",
                                              static_cast<int>(ParametricEQProcessor::VintageHighShelf),
                                              4000.0f,
                                              0.8f,
                                              12000.0f);
    }
};

static DynamicEQGainModelContractTest dynamicEQGainModelContractTest;

#endif
