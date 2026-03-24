#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include "../DSP/BiquadCoefficients.h"

/**
 * BiquadRegressionTest — Verifies that BiquadCoeffs produces identical
 * magnitude responses to JUCE's IIR::Coefficients for all filter types.
 *
 * This is the safety net for the Phase B refactor. If this test passes,
 * the EQ sounds exactly the same after replacing JUCE Coefficients::Ptr.
 */
class BiquadRegressionTest : public juce::UnitTest
{
public:
    BiquadRegressionTest() : juce::UnitTest("BiquadCoeffs vs JUCE IIR Regression", "DSP") {}

    void runTest() override
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        const float frequencies[]  = { 30.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f, 18000.0f };
        const float qValues[]     = { 0.1f, 0.5f, 0.707f, 1.0f, 2.0f, 10.0f };
        const float gains[]       = { -12.0f, -6.0f, -1.0f, 0.5f, 3.0f, 6.0f, 12.0f };

        // Test frequencies for magnitude comparison
        const double testFreqs[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0,
                                      2000.0, 5000.0, 10000.0, 15000.0, 20000.0 };
        const int numTestFreqs = 11;
        const double maxErrorDB = 0.05; // Allow 0.05 dB tolerance (float precision)

        for (double sr : sampleRates)
        {
            for (float freq : frequencies)
            {
                if (freq >= sr * 0.499f) continue;

                for (float q : qValues)
                {
                    // HighPass (LowCut)
                    beginTest("HighPass sr=" + juce::String(sr) + " f=" + juce::String(freq) + " Q=" + juce::String(q));
                    {
                        auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, freq, q);
                        auto ours = BiquadCoeffs::makeHighPass(sr, freq, q);
                        compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                    }

                    // LowPass (HighCut)
                    beginTest("LowPass sr=" + juce::String(sr) + " f=" + juce::String(freq) + " Q=" + juce::String(q));
                    {
                        auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, freq, q);
                        auto ours = BiquadCoeffs::makeLowPass(sr, freq, q);
                        compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                    }

                    // Notch
                    beginTest("Notch sr=" + juce::String(sr) + " f=" + juce::String(freq) + " Q=" + juce::String(q));
                    {
                        auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeNotch(sr, freq, q);
                        auto ours = BiquadCoeffs::makeNotch(sr, freq, q);
                        compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                    }

                    // BandPass
                    beginTest("BandPass sr=" + juce::String(sr) + " f=" + juce::String(freq) + " Q=" + juce::String(q));
                    {
                        auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freq, q);
                        auto ours = BiquadCoeffs::makeBandPass(sr, freq, q);
                        compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                    }

                    // Peak, LowShelf, HighShelf need gain
                    for (float gainDB : gains)
                    {
                        float gainLin = juce::Decibels::decibelsToGain(gainDB);

                        // Peak
                        beginTest("Peak sr=" + juce::String(sr) + " f=" + juce::String(freq) +
                                  " Q=" + juce::String(q) + " g=" + juce::String(gainDB));
                        {
                            auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, freq, q, gainLin);
                            auto ours = BiquadCoeffs::makePeakFilter(sr, freq, q, gainLin);
                            compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                        }

                        // LowShelf
                        beginTest("LowShelf sr=" + juce::String(sr) + " f=" + juce::String(freq) +
                                  " Q=" + juce::String(q) + " g=" + juce::String(gainDB));
                        {
                            auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, freq, q, gainLin);
                            auto ours = BiquadCoeffs::makeLowShelf(sr, freq, q, gainLin);
                            compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                        }

                        // HighShelf
                        beginTest("HighShelf sr=" + juce::String(sr) + " f=" + juce::String(freq) +
                                  " Q=" + juce::String(q) + " g=" + juce::String(gainDB));
                        {
                            auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, freq, q, gainLin);
                            auto ours = BiquadCoeffs::makeHighShelf(sr, freq, q, gainLin);
                            compareMagnitudes(juceCoeffs, ours, sr, testFreqs, numTestFreqs, maxErrorDB);
                        }
                    }
                }
            }
        }

        // Process-level test: run same signal through JUCE filter and our BiquadState
        beginTest("ProcessSample equivalence — Peak 1kHz +6dB Q=1.0 @ 48kHz");
        {
            const double sr = 48000.0;
            float gainLin = juce::Decibels::decibelsToGain(6.0f);

            auto juceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 1000.0f, 1.0f, gainLin);
            auto ours = BiquadCoeffs::makePeakFilter(sr, 1000.0f, 1.0f, gainLin);

            juce::dsp::IIR::Filter<float> juceFilter;
            juce::dsp::ProcessSpec spec { sr, 512, 1 };
            juceFilter.prepare(spec);
            *juceFilter.coefficients = *juceCoeffs;

            BiquadState ourState;

            // Generate test signal: impulse + sine sweep mix
            const int numSamples = 4096;
            float maxError = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                float input = (i == 0) ? 1.0f : 0.0f; // Impulse
                input += 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * (float)i / (float)sr); // 1kHz sine

                float juceOut = juceFilter.processSample(input);
                float ourOut = ourState.processSample(input, ours);

                float error = std::abs(juceOut - ourOut);
                if (error > maxError) maxError = error;
            }

            // Expect sample-exact match within float epsilon
            expect(maxError < 1e-5f, "Max sample error: " + juce::String(maxError));
        }
    }

private:
    void compareMagnitudes(const juce::dsp::IIR::Coefficients<float>::Ptr& juceCoeffs,
                           const BiquadCoeffs& ours,
                           double sr,
                           const double* testFreqs, int numFreqs,
                           double maxErrorDB)
    {
        for (int i = 0; i < numFreqs; ++i)
        {
            if (testFreqs[i] >= sr * 0.499) continue;

            double juceMag = juceCoeffs->getMagnitudeForFrequency(testFreqs[i], sr);
            double ourMag  = ours.getMagnitudeForFrequency(testFreqs[i], sr);

            // Compare in dB to catch subtle differences
            double juceDB = 20.0 * std::log10(std::max(1e-10, juceMag));
            double ourDB  = 20.0 * std::log10(std::max(1e-10, ourMag));
            double errorDB = std::abs(juceDB - ourDB);

            expect(errorDB < maxErrorDB,
                   "@ " + juce::String(testFreqs[i]) + "Hz: JUCE=" + juce::String(juceDB, 4) +
                   " ours=" + juce::String(ourDB, 4) + " err=" + juce::String(errorDB, 6) + "dB");
        }
    }
};

static BiquadRegressionTest biquadRegressionTest;
