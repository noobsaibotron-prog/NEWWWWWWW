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
     *  Uses Catmull-Rom cubic interpolation for smooth low-frequency rendering. */
    void mapToPixels (const std::vector<float>& smoothedDB,
                      std::vector<float>& outPixelDB) const
    {
        const size_t width = pixelToBinLUT.size();
        if (width == 0 || smoothedDB.empty())
            return;

        if (outPixelDB.size() != width)
            outPixelDB.resize (width);

        const int maxBin = static_cast<int> (smoothedDB.size()) - 1;

        for (size_t x = 0; x < width; ++x)
        {
            const float exactBin = pixelToBinLUT[x];
            const int b1 = static_cast<int> (exactBin);
            const float t = exactBin - static_cast<float> (b1);

            // Catmull-Rom needs 4 points: p0, p1, p2, p3
            const int b0 = std::max (b1 - 1, 0);
            const int b2 = std::min (b1 + 1, maxBin);
            const int b3 = std::min (b1 + 2, maxBin);

            const float p0 = smoothedDB[static_cast<size_t> (b0)];
            const float p1 = smoothedDB[static_cast<size_t> (b1)];
            const float p2 = smoothedDB[static_cast<size_t> (b2)];
            const float p3 = smoothedDB[static_cast<size_t> (b3)];

            // Catmull-Rom spline: 0.5 * ((2*p1) + (-p0+p2)*t + (2*p0-5*p1+4*p2-p3)*t^2 + (-p0+3*p1-3*p2+p3)*t^3)
            const float t2 = t * t;
            const float t3 = t2 * t;
            outPixelDB[x] = 0.5f * ((2.0f * p1)
                            + (-p0 + p2) * t
                            + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                            + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
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
