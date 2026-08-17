/**
 * SpectrumCalibrationTest — N5 regression.
 *
 * Pins the metrological spectrum calibration: a full-scale (0 dBFS) bin-centered
 * sine must read ~0 dB after SpectrumAnalyzerCore (PSD) → SpectrumDisplayMapper
 * (dB + ballistics). Before the N5 fix the offset was subtracted with the wrong
 * magnitude (-4.26) and a 0 dBFS sine read ~-9 dB. The correct offset (+4.773 dB)
 * is invariant across FFT size and sample rate.
 */

#include <juce_core/juce_core.h>
#include "../DSP/SpectrumAnalyzerCore.h"
#include "../DSP/SpectrumDisplayMapper.h"
#include <vector>
#include <cmath>

class SpectrumCalibrationTest : public juce::UnitTest
{
public:
    SpectrumCalibrationTest() : juce::UnitTest("Spectrum Calibration", "AIEQ-DSP") {}

    // Drives a bin-centered sine of the given amplitude through the full
    // PSD→dB chain and returns the settled peak-bin dB.
    static float peakDb(size_t fftOrder, double fs, float amplitude)
    {
        SpectrumAnalyzerCore core(fftOrder, fs);
        const size_t N = core.getFFTSize();
        const int k0 = static_cast<int>(N / 16);        // a clean, bin-centered tone
        std::vector<float> sig(N);
        for (size_t n = 0; n < N; ++n)
            sig[n] = amplitude * std::sin(2.0 * juce::MathConstants<double>::pi
                                          * k0 * static_cast<double>(n) / static_cast<double>(N));

        const auto& psd = core.processBlock(sig.data());

        SpectrumDisplayMapper mapper(N / 2 + 1, 60.0, fs);
        mapper.pushRawSpectrum(psd);
        const std::vector<float>* frame = nullptr;
        for (int i = 0; i < 600; ++i)                   // settle the asymmetric IIR
            frame = &mapper.generateSmoothedUIFrame();

        float peak = -1.0e9f;
        for (float v : *frame)
            peak = std::max(peak, v);
        return peak;
    }

    void runTest() override
    {
        beginTest("0 dBFS sine reads ~0 dB (invariant over N and fs)");
        struct Case { size_t order; double fs; };
        for (const auto& c : { Case{11, 48000.0}, Case{12, 48000.0},
                               Case{13, 48000.0}, Case{12, 44100.0} })
        {
            const float db = peakDb(c.order, c.fs, 1.0f);
            logMessage("  N=" + juce::String(1 << c.order) + " fs=" + juce::String(c.fs, 0)
                       + "  peak = " + juce::String(db, 3) + " dB");
            expect(std::abs(db) <= 0.5f,
                   "0 dBFS sine read " + juce::String(db, 3) + " dB (expected ~0, ±0.5)");
        }

        beginTest("-6 dBFS sine reads ~-6 dB");
        const float db6 = peakDb(12, 48000.0, 0.5f);
        logMessage("  -6 dBFS peak = " + juce::String(db6, 3) + " dB");
        expect(std::abs(db6 + 6.0f) <= 0.6f,
               "-6 dBFS sine read " + juce::String(db6, 3) + " dB (expected ~-6)");
    }
};

static SpectrumCalibrationTest spectrumCalibrationTest;
