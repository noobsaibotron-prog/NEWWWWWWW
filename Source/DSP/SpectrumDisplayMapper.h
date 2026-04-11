#pragma once
/**
 * Strato 2 — dB conversion + adaptive IIR ballistics.
 *
 * Converts linear PSD to calibrated dB, applies asymmetric IIR smoothing
 * (fast attack, slow release) computed from UI frame rate and time constants.
 * SpinLock-protected for thread-safe push (FFT thread) / read (UI thread).
 *
 * Calibration: a full-scale sine (0 dBFS) reads 0.0 dB after the offset.
 * The Hann power-normalization offset is ~4.26 dB.
 */

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>

class SpectrumDisplayMapper
{
public:
    SpectrumDisplayMapper (size_t numBins, double frameRateUI, double sampleRate = 48000.0)
        : bins (numBins),
          smoothedDB (numBins, -100.0f)
    {
        for (auto& buf : rawPowerBuffers)
            buf.resize (bins, 0.0f);

        // PSD→magnitude display offset: compensate the fs divisor in PSD formula
        // so that visual levels match the legacy 20*log10(|X|/N) pipeline.
        // Offset = 10*log10(fs) — e.g. 46.8 dB at 48 kHz, 44.1 dB at 44.1 kHz.
        psdDisplayOffsetDB = static_cast<float> (10.0 * std::log10 (std::max (sampleRate, 1.0)));

        // IIR coefficients from time constants and frame rate.
        // Wave 5 Premium Spectrum: defaults tuned for the "liquid flow" reference
        // feel. Attack 15 ms keeps transients readable without twitching; release
        // 500 ms gives the slow decaying tail typical of premium spectrum UIs
        // (instead of the harsh 85 ms legacy default that caused the post-EQ
        // curve to jitter and fight the luminous cyan fill). If a user prefers
        // a faster analyzer they can still call setTimeConstants() at runtime.
        const auto fr = static_cast<float> (frameRateUI);
        alphaAttack  = 1.0f - std::exp (-1.0f / (fr * 0.015f));
        alphaRelease = 1.0f - std::exp (-1.0f / (fr * 0.500f));
    }

    /** Push a raw PSD frame (from SpectrumAnalyzerCore). Thread-safe via SpinLock. */
    void pushRawSpectrum (const std::vector<float>& newPSD)
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        const int currentWrite = (writeSlot + 1) % 2;
        const size_t toCopy = std::min (newPSD.size(), bins);
        std::copy (newPSD.begin(), newPSD.begin() + static_cast<std::ptrdiff_t>(toCopy),
                   rawPowerBuffers[currentWrite].begin());
        writeSlot = currentWrite;
    }

    /** Generate a smoothed dB frame for the UI. Call once per paint frame.
     *  Thread-safe via SpinLock (never called concurrently with pushRawSpectrum
     *  from the audio thread — both are GUI/pipeline thread calls). */
    const std::vector<float>& generateSmoothedUIFrame()
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        const auto& target = rawPowerBuffers[writeSlot];

        // Empirical calibration offset for power-normalized Hann window
        // 10*log10(sum(w^2)/N) for Hann ~ -4.26 dB
        static constexpr float CALIBRATION_OFFSET_DB = 4.260f;

        for (size_t i = 0; i < bins; ++i)
        {
            const float instantDB = 10.0f * std::log10 (std::max (target[i], 1e-15f))
                                  - CALIBRATION_OFFSET_DB + psdDisplayOffsetDB;
            const float alpha = (instantDB > smoothedDB[i]) ? alphaAttack : alphaRelease;
            smoothedDB[i] = alpha * instantDB + (1.0f - alpha) * smoothedDB[i];
        }

        return smoothedDB;
    }

    /** Update time constants (e.g., when user changes analyzer speed). */
    void setTimeConstants (float attackMs, float releaseMs, double frameRateUI)
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        const auto fr = static_cast<float> (frameRateUI);
        alphaAttack  = 1.0f - std::exp (-1.0f / (fr * attackMs * 0.001f));
        alphaRelease = 1.0f - std::exp (-1.0f / (fr * releaseMs * 0.001f));
    }

    /** Reset smoothed state to -100 dB. */
    void reset()
    {
        const juce::SpinLock::ScopedLockType lock (dataLock);
        std::fill (smoothedDB.begin(), smoothedDB.end(), -100.0f);
    }

    size_t getNumBins() const noexcept { return bins; }

private:
    size_t bins;
    std::vector<float> rawPowerBuffers[2];
    std::vector<float> smoothedDB;
    int writeSlot = 0;                     // plain int, protected by SpinLock
    juce::SpinLock dataLock;               // protects rawPowerBuffers + writeSlot + smoothedDB + alphas
    float alphaAttack  = 0.0f;
    float alphaRelease = 0.0f;
    float psdDisplayOffsetDB = 46.8f;  // default for 48 kHz
};
