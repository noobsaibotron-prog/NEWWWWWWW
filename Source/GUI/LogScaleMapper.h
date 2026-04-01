#pragma once
/**
 * Strato 3 — Logarithmic pixel-to-bin Look-Up Table.
 *
 * Pre-computes a mapping from screen pixel columns to fractional FFT bins,
 * ensuring geometrically invariant octave spacing. The LUT is rebuilt only
 * on resize (not per-frame), so per-frame cost is a single linear scan.
 *
 * Uses linear interpolation between adjacent bins for smooth visual output.
 */

#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

class LogScaleMapper
{
public:
    LogScaleMapper (double sampleRate, size_t fftSize,
                    double fMin = 20.0, double fMax = 20000.0)
        : fs (sampleRate), N (fftSize), minFreq (fMin), maxFreq (fMax)
    {
        // Fix 7: validate frequency range at construction
        jassert (fMin > 0.0 && fMin < fMax);
    }

    /** Rebuild the LUT when the display width changes.
     *  Call from resized() or when FFT size / sample rate changes. */
    void updateScreenWidth (size_t widthInPixels)
    {
        if (widthInPixels == pixelToBinLUT.size() && !dirty)
            return;

        pixelToBinLUT.resize (widthInPixels);

        const double logMin = std::log (minFreq);
        const double logMax = std::log (maxFreq);
        const double logScale = (widthInPixels > 1)
                              ? (logMax - logMin) / static_cast<double> (widthInPixels - 1)
                              : 0.0;

        const double halfN = static_cast<double> (N) / 2.0;

        for (size_t x = 0; x < widthInPixels; ++x)
        {
            const double targetFreq = std::exp (logMin + static_cast<double> (x) * logScale);
            pixelToBinLUT[x] = static_cast<float> (
                std::clamp (targetFreq * static_cast<double> (N) / fs, 0.0, halfN));
        }

        dirty = false;
    }

    /** Map a smoothed dB spectrum (per-bin) to a per-pixel dB array.
     *  Uses linear interpolation between adjacent bins. */
    void mapToPixels (const std::vector<float>& smoothedDB,
                      std::vector<float>& outPixelDB) const
    {
        const size_t width = pixelToBinLUT.size();
        if (width == 0 || smoothedDB.empty())
            return;

        if (outPixelDB.size() != width)
            outPixelDB.resize (width);

        const size_t maxBin = smoothedDB.size() - 1;

        for (size_t x = 0; x < width; ++x)
        {
            const float exactBin = pixelToBinLUT[x];
            const size_t binLow  = static_cast<size_t> (exactBin);
            const size_t binHigh = std::min (binLow + 1, maxBin);
            const float frac = exactBin - static_cast<float> (binLow);
            outPixelDB[x] = smoothedDB[binLow] + frac * (smoothedDB[binHigh] - smoothedDB[binLow]);
        }
    }

    /** Mark dirty so next updateScreenWidth() forces a rebuild. */
    void markDirty() { dirty = true; }

    /** Update sample rate / FFT size (forces LUT rebuild). */
    void setParameters (double sampleRate, size_t fftSize)
    {
        fs = sampleRate;
        N  = fftSize;
        dirty = true;
    }

    size_t getWidth() const noexcept { return pixelToBinLUT.size(); }

private:
    double fs;
    size_t N;
    double minFreq, maxFreq;
    bool dirty = true;
    std::vector<float> pixelToBinLUT;
};
