#pragma once
/**
 * Strato 1 — Metrological FFT Engine.
 *
 * Power-normalized Hann window + juce::dsp::FFT → Power Spectral Density.
 * Guarantees Parseval energy conservation.
 * DC and Nyquist bins are halved to maintain spectral symmetry.
 *
 * This class is NOT thread-safe. Call processBlock() from the GUI thread only.
 */

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class SpectrumAnalyzerCore
{
public:
    SpectrumAnalyzerCore (size_t fftOrder, double sampleRate)
        : N (1u << fftOrder),
          fs (sampleRate),
          fft (static_cast<int> (fftOrder)),
          timeDomainBuffer (N * 2, 0.0f),
          window (N, 0.0f),
          spectrum (N / 2 + 1, 0.0f)
    {
        preparePowerNormalizedWindow();
    }

    /** Feed exactly N samples. Returns PSD (linear power, NOT dB). */
    const std::vector<float>& processBlock (const float* inputSamples)
    {
        // 1. Apply power-normalized Hann window
        for (size_t i = 0; i < N; ++i)
            timeDomainBuffer[i] = inputSamples[i] * window[i];

        // Zero-pad the second half (JUCE FFT requires 2*N buffer)
        std::fill (timeDomainBuffer.begin() + static_cast<std::ptrdiff_t>(N),
                   timeDomainBuffer.end(), 0.0f);

        // 2. Real-only forward FFT
        fft.performRealOnlyForwardTransform (timeDomainBuffer.data());

        // 3. Convert to Power Spectral Density (linear)
        //    PSD[k] = 2 * |X[k]|^2 / (fs * N)
        //    Factor 2 accounts for single-sided spectrum.
        //    DC (k=0) and Nyquist (k=N/2) are unique — halve them.
        const float scaleFactor = 2.0f / (static_cast<float>(fs) * static_cast<float>(N));
        const size_t numBins = N / 2 + 1;

        for (size_t k = 0; k < numBins; ++k)
        {
            const float re = timeDomainBuffer[k * 2];
            const float im = timeDomainBuffer[k * 2 + 1];
            float power = (re * re + im * im) * scaleFactor;
            if (k == 0 || k == N / 2)
                power *= 0.5f;
            spectrum[k] = power;
        }

        return spectrum;
    }

    size_t getFFTSize() const noexcept { return N; }
    size_t getNumBins() const noexcept { return N / 2 + 1; }
    double getSampleRate() const noexcept { return fs; }

    /** Reconfigure for a different sample rate (call from message thread). */
    void setSampleRate (double newSampleRate) { fs = newSampleRate; }

private:
    size_t N;
    double fs;
    juce::dsp::FFT fft;
    std::vector<float> timeDomainBuffer;
    std::vector<float> window;
    std::vector<float> spectrum;

    void preparePowerNormalizedWindow()
    {
        double sumSq = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            window[i] = static_cast<float> (0.5 * (1.0 - std::cos (2.0 * M_PI * static_cast<double>(i)
                                                                     / static_cast<double>(N - 1))));
            sumSq += static_cast<double>(window[i]) * static_cast<double>(window[i]);
        }
        const float normFactor = static_cast<float> (1.0 / std::sqrt (sumSq));
        for (size_t i = 0; i < N; ++i)
            window[i] *= normFactor;
    }
};
