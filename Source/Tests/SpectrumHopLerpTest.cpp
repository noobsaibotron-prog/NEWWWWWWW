#include <juce_core/juce_core.h>

#include "../GUI/SpectrumHopLerp.h"

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * Behaviour of live-spectrum hop interpolation (GUI, no DSP / FFT).
 *
 * Same t for pre and post; delta = lerpedPost − lerpedPre.
 * New hop: previous = current, t = 0 after the first hop.
 * t advances with dt / hopInterval and clamps at 1.
 * Freeze/capture reads the current hop, not a mid-lerp mix.
 */
class SpectrumHopLerpTest final : public juce::UnitTest
{
public:
    SpectrumHopLerpTest()
        : juce::UnitTest ("Spectrum hop interpolation", "Integration") {}

    void runTest() override
    {
        testLerpEndpointsAndMidpoint();
        testSharedTAndDeltaFromLerpedColumns();
        testHopResetPreviousEqualsCurrent();
        testTAdvancesWithDtAndClamps();
        testFreezeCaptureUsesStableHopFrame();
        testPresentationPolicies();
        testConfiguredBuildPolicy();
    }

private:
    static constexpr float kEps = 1.0e-5f;

    static std::vector<float> cols (std::initializer_list<float> v)
    {
        return std::vector<float> (v);
    }

    void expectCols (const std::vector<float>& got, const std::vector<float>& want,
                     const juce::String& label)
    {
        expectEquals ((int) got.size(), (int) want.size(), label + " size");
        const size_t n = std::min (got.size(), want.size());
        for (size_t i = 0; i < n; ++i)
            expectWithinAbsoluteError (got[i], want[i], kEps,
                                       label + " [" + juce::String ((int) i) + "]");
    }

    void testLerpEndpointsAndMidpoint()
    {
        beginTest ("lerpSpectrumColumns: t=0 is prev, t=1 is curr, t=0.5 midpoint");

        const auto a = cols ({ -24.0f, 0.0f, 6.0f });
        const auto b = cols ({ -12.0f, 8.0f, 10.0f });
        std::vector<float> out;

        SpectrumHopLerp::lerpSpectrumColumns (a, b, 0.0f, out);
        expectCols (out, a, "t=0");

        SpectrumHopLerp::lerpSpectrumColumns (a, b, 1.0f, out);
        expectCols (out, b, "t=1");

        SpectrumHopLerp::lerpSpectrumColumns (a, b, 0.5f, out);
        expectCols (out, cols ({ -18.0f, 4.0f, 8.0f }), "t=0.5");
    }

    void testSharedTAndDeltaFromLerpedColumns()
    {
        beginTest ("Same t for pre and post; delta = lerpedPost - lerpedPre");

        SpectrumHopLerp hop;
        hop.ingestHop (cols ({ 0.0f, 2.0f }), cols ({ 10.0f, 4.0f }), 0.0, false);
        hop.ingestHop (cols ({ 10.0f, 6.0f }), cols ({ 10.0f, 12.0f }), 21.0, true);
        expectWithinAbsoluteError (hop.t, 0.0f, kEps, "second hop resets t");

        hop.t = 0.5f;
        std::vector<float> lerpedPre, lerpedPost, delta;
        hop.lerpInto (lerpedPre, lerpedPost);
        hop.deltaInto (delta);

        expectCols (lerpedPre, cols ({ 5.0f, 4.0f }), "shared-t pre");
        expectCols (lerpedPost, cols ({ 10.0f, 8.0f }), "shared-t post");
        expectEquals ((int) delta.size(), (int) lerpedPre.size(), "delta width");
        for (size_t i = 0; i < delta.size(); ++i)
            expectWithinAbsoluteError (delta[i], lerpedPost[i] - lerpedPre[i], kEps,
                                       "delta is post-pre at the one t");

        // An independent delta clock at t=1 would be {0, 6}, not {5, 4}.
        expectWithinAbsoluteError (delta[0], 5.0f, kEps, "delta[0] at t=0.5");
        expectWithinAbsoluteError (delta[1], 4.0f, kEps, "delta[1] at t=0.5");
        expect (std::abs (delta[0] - 0.0f) > 1.0f, "not an independent delta lerp at t=1");
    }

    void testHopResetPreviousEqualsCurrent()
    {
        beginTest ("New hop: previous=current, t=0 after first hop");

        SpectrumHopLerp hop;
        const auto hop0 = cols ({ 1.0f, 2.0f, 3.0f });
        const auto hop1 = cols ({ 4.0f, 5.0f, 6.0f });

        hop.ingestHop (hop0, hop0, 0.0, false);
        expectWithinAbsoluteError (hop.t, 1.0f, kEps, "first hop snaps t=1");
        expectCols (hop.prevPre, hop0, "first-hop prev snapped to curr");
        expectCols (hop.currPre, hop0, "first-hop curr");

        hop.ingestHop (hop1, hop1, 21.0, true);
        expectWithinAbsoluteError (hop.t, 0.0f, kEps, "later hop resets t=0");
        expectCols (hop.prevPre, hop0, "prev is previous current");
        expectCols (hop.currPre, hop1, "curr is new hop");
        expectWithinAbsoluteError ((float) hop.hopIntervalMs, 21.0f, kEps, "interval from gap");

        hop.ingestHop (hop0, hop0, 150.0, true);
        expectWithinAbsoluteError (hop.t, 1.0f, kEps, "gap > 100 ms snaps");
        expectWithinAbsoluteError ((float) hop.hopIntervalMs,
                                   (float) SpectrumHopLerp::kHopIntervalMaxMs, kEps,
                                   "interval clamped to max");
    }

    void testTAdvancesWithDtAndClamps()
    {
        beginTest ("t advances with dt / hopInterval and clamps at 1");

        expectWithinAbsoluteError (SpectrumHopLerp::advanceT (0.0f, 10.5, 21.0), 0.5f, kEps,
                                   "half interval → t=0.5");
        expectWithinAbsoluteError (SpectrumHopLerp::advanceT (0.8f, 21.0, 21.0), 1.0f, kEps,
                                   "overshoot clamps at 1");
        expectWithinAbsoluteError (SpectrumHopLerp::advanceT (1.0f, 16.0, 21.0), 1.0f, kEps,
                                   "already 1 stays 1");
        expectWithinAbsoluteError (SpectrumHopLerp::advanceT (0.25f, 0.0, 21.0), 0.25f, kEps,
                                   "dt<=0 does not move t");

        expectWithinAbsoluteError ((float) SpectrumHopLerp::clampHopIntervalMs (10.0),
                                   (float) SpectrumHopLerp::kHopIntervalMinMs, kEps, "min clamp");
        expectWithinAbsoluteError ((float) SpectrumHopLerp::clampHopIntervalMs (40.0),
                                   40.0f, kEps, "in-range interval");

        SpectrumHopLerp hop;
        hop.ingestHop (cols ({ 0.0f }), cols ({ 0.0f }), 0.0, false);
        hop.ingestHop (cols ({ 8.0f }), cols ({ 8.0f }), 21.0, true);
        hop.advance (7.0);
        expectWithinAbsoluteError (hop.t, 7.0f / 21.0f, kEps, "state.advance uses hopInterval");
        hop.advance (100.0);
        expectWithinAbsoluteError (hop.t, 1.0f, kEps, "state.advance clamps");
    }

    void testFreezeCaptureUsesStableHopFrame()
    {
        beginTest ("Freeze/capture uses current hop (t==1), not a mid-lerp mix");

        SpectrumHopLerp hop;
        hop.ingestHop (cols ({ 0.0f, 0.0f }), cols ({ 0.0f, 0.0f }), 0.0, false);
        hop.ingestHop (cols ({ 10.0f, 20.0f }), cols ({ 30.0f, 40.0f }), 21.0, true);
        hop.t = 0.5f;

        std::vector<float> midPre, midPost;
        hop.lerpInto (midPre, midPost);
        expectCols (midPre, cols ({ 5.0f, 10.0f }), "mid-lerp is not the hop");

        const auto fallback = cols ({ -99.0f, -99.0f });
        const auto& frozen = SpectrumHopLerp::stableHopFrame (hop.currPre, fallback);
        const auto& captured = SpectrumHopLerp::stableHopFrame (hop.currPre, fallback);
        expectCols (frozen, hop.currPre, "freeze = current hop");
        expectCols (captured, hop.currPre, "capture = current hop");
        expect (frozen != midPre, "stable frame is not the mid-lerp mix");

        hop.t = 1.0f;
        std::vector<float> atOne;
        SpectrumHopLerp::lerpSpectrumColumns (hop.prevPre, hop.currPre, hop.t, atOne);
        expectCols (atOne, hop.currPre, "t==1 lerp equals current hop");
    }

    void testPresentationPolicies()
    {
        beginTest ("A1 presents latest hop immediately; A2 presents interpolated hop");

        SpectrumHopLerp hop;
        const auto oldPre = cols ({ -60.0f, -50.0f });
        const auto oldPost = cols ({ -58.0f, -48.0f });
        const auto newPre = cols ({ -30.0f, -20.0f });
        const auto newPost = cols ({ -27.0f, -17.0f });

        hop.ingestHop (oldPre, oldPost, 0.0, false);
        hop.ingestHop (newPre, newPost, 21.0, true);
        expectWithinAbsoluteError (hop.t, 0.0f, kEps,
                                   "the interpolation state still records the hop boundary");

        std::vector<float> a1Pre, a1Post, a2Pre, a2Post;
        hop.presentInto (a1Pre, a1Post, SpectrumHopLerp::PresentationPolicy::latestHop);
        hop.presentInto (a2Pre, a2Post, SpectrumHopLerp::PresentationPolicy::interpolate);

        expectCols (a1Pre, newPre, "A1 pre is the newest hop, not prev");
        expectCols (a1Post, newPost, "A1 post is the newest hop, not prev");
        expect (a1Pre != oldPre, "A1 never displays the previous hop at t=0");
        expectCols (a2Pre, oldPre, "A2 pre starts from the previous hop at t=0");
        expectCols (a2Post, oldPost, "A2 post starts from the previous hop at t=0");
    }

    void testConfiguredBuildPolicy()
    {
        beginTest ("Configured A/B build policy selects exactly one presentation path");

        SpectrumHopLerp hop;
        const auto oldHop = cols ({ -60.0f, -50.0f });
        const auto newHop = cols ({ -30.0f, -20.0f });
        hop.ingestHop (oldHop, oldHop, 0.0, false);
        hop.ingestHop (newHop, newHop, 21.0, true);

        std::vector<float> shownPre, shownPost;
        hop.presentInto (shownPre, shownPost);

#if defined(AIEQ_ANALYZER_AB_A1_LATEST_HOP) && AIEQ_ANALYZER_AB_A1_LATEST_HOP
        expect (! SpectrumHopLerp::kInterpolationEnabled, "A1 define disables interpolation");
        expectCols (shownPre, newHop, "configured A1 presents current hop");
#else
        expect (SpectrumHopLerp::kInterpolationEnabled, "default build preserves A2 interpolation");
        expectCols (shownPre, oldHop, "configured A2 presents previous hop at t=0");
#endif
        expectCols (shownPost, shownPre, "configured policy is shared by pre and post");
    }
};

static SpectrumHopLerpTest spectrumHopLerpTest;
