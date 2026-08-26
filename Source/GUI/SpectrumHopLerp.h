#pragma once

/**
 * Paint-thread hop interpolation helpers for the live spectrum display.
 * Header-only, no DSP / FFT / hop-size policy — just column lerp and t.
 *
 * Contract:
 *   - One t for pre and post; delta = lerpedPost − lerpedPre (no second clock).
 *   - New hop: previous = current, t = 0 after the first hop (first hop snaps).
 *   - t advances by dtMs / hopIntervalMs and clamps at 1.
 *   - Freeze / capture reads the current hop (t == 1 equivalent), never a mid-lerp mix.
 */

#include <algorithm>
#include <cstddef>
#include <vector>

struct SpectrumHopLerp
{
    enum class PresentationPolicy
    {
        latestHop,
        interpolate
    };

    // A2 remains the product default. A1 is enabled only by the explicit,
    // developer-only CMake option AIEQ_ANALYZER_AB_A1_LATEST_HOP.
#if defined(AIEQ_ANALYZER_AB_A1_LATEST_HOP) && AIEQ_ANALYZER_AB_A1_LATEST_HOP
    static constexpr PresentationPolicy kBuildPresentationPolicy = PresentationPolicy::latestHop;
#else
    static constexpr PresentationPolicy kBuildPresentationPolicy = PresentationPolicy::interpolate;
#endif
    static constexpr bool kInterpolationEnabled =
        kBuildPresentationPolicy == PresentationPolicy::interpolate;

    static constexpr double kHopIntervalMinMs = 16.0;
    static constexpr double kHopIntervalMaxMs = 50.0;
    static constexpr double kSnapGapMs = 100.0;
    static constexpr double kDefaultHopIntervalMs = 21.0;

    std::vector<float> prevPre, currPre;
    std::vector<float> prevPost, currPost;
    float t = 1.0f;
    double hopIntervalMs = kDefaultHopIntervalMs;

    static void lerpSpectrumColumns (const std::vector<float>& prev,
                                     const std::vector<float>& curr,
                                     float t,
                                     std::vector<float>& out)
    {
        const size_t n = curr.size();
        if (n == 0)
        {
            out.clear();
            return;
        }

        out.resize (n);
        if (prev.size() != n || t >= 1.0f)
        {
            std::copy (curr.begin(), curr.end(), out.begin());
            return;
        }
        if (t <= 0.0f)
        {
            std::copy (prev.begin(), prev.end(), out.begin());
            return;
        }

        const float u = 1.0f - t;
        for (size_t i = 0; i < n; ++i)
            out[i] = prev[i] * u + curr[i] * t;
    }

    static float advanceT (float t, double dtMs, double hopIntervalMs) noexcept
    {
        if (dtMs <= 0.0)
            return t;
        const double interval = hopIntervalMs > 0.0 ? hopIntervalMs : kDefaultHopIntervalMs;
        return std::min (1.0f, t + static_cast<float> (dtMs / interval));
    }

    static double clampHopIntervalMs (double measuredMs) noexcept
    {
        return std::clamp (measuredMs, kHopIntervalMinMs, kHopIntervalMaxMs);
    }

    static const std::vector<float>& stableHopFrame (const std::vector<float>& currHop,
                                                     const std::vector<float>& fallback)
    {
        return currHop.empty() ? fallback : currHop;
    }

    void ingestHop (const std::vector<float>& pre,
                    const std::vector<float>& post,
                    double gapMs,
                    bool updateHopInterval)
    {
        const bool firstHop = currPre.empty();
        prevPre.assign (currPre.begin(), currPre.end());
        currPre.assign (pre.begin(), pre.end());
        prevPost.assign (currPost.begin(), currPost.end());
        currPost.assign (post.begin(), post.end());

        const bool sizeMismatch = prevPre.size() != currPre.size();
        if (firstHop || prevPre.empty() || sizeMismatch)
            prevPre.assign (currPre.begin(), currPre.end());
        if (prevPost.empty() || prevPost.size() != currPost.size())
            prevPost.assign (currPost.begin(), currPost.end());

        t = (firstHop || sizeMismatch || gapMs > kSnapGapMs) ? 1.0f : 0.0f;

        if (updateHopInterval)
            hopIntervalMs = clampHopIntervalMs (gapMs);
    }

    void advance (double dtMs) noexcept
    {
        t = advanceT (t, dtMs, hopIntervalMs);
    }

    void lerpInto (std::vector<float>& displayPre, std::vector<float>& displayPost) const
    {
        lerpSpectrumColumns (prevPre, currPre, t, displayPre);
        lerpSpectrumColumns (prevPost, currPost, t, displayPost);
    }

    void presentInto (std::vector<float>& displayPre,
                      std::vector<float>& displayPost,
                      PresentationPolicy policy) const
    {
        if (policy == PresentationPolicy::interpolate)
        {
            lerpInto (displayPre, displayPost);
        }
        else
        {
            displayPre = currPre;
            displayPost = currPost;
        }
    }

    void presentInto (std::vector<float>& displayPre, std::vector<float>& displayPost) const
    {
        presentInto (displayPre, displayPost, kBuildPresentationPolicy);
    }

    void deltaInto (std::vector<float>& out) const
    {
        std::vector<float> lerpedPre, lerpedPost;
        lerpInto (lerpedPre, lerpedPost);
        const size_t n = std::min (lerpedPre.size(), lerpedPost.size());
        out.resize (n);
        for (size_t i = 0; i < n; ++i)
            out[i] = lerpedPost[i] - lerpedPre[i];
    }
};
