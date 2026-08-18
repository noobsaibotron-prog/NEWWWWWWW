#if JUCE_UNIT_TESTS

/*
 * T4 adversarial review.
 *
 * The candidate's own suite builds sources that separate cleanly along the axis
 * being measured, so it can only confirm that the descriptors are ordered the
 * way they were designed to be. These cases are built to be awkward for the
 * policy instead: sources where one region disagrees with the axis it feeds,
 * where two shapes share an aggregate but differ in distribution, and where the
 * level sits on the confidence boundary.
 *
 * Assertion policy, stated because it matters for how the output should be
 * read: contract invariants are asserted hard (a context may never flip a goal,
 * invent one, weaken a constraint, or ask for more than the user did). The
 * heuristic questions are MEASURED and logged rather than asserted, because
 * inventing a threshold here and then passing against it would prove nothing —
 * the numbers are the deliverable, and what is acceptable is a product call.
 */

#include <juce_core/juce_core.h>
#include "../AI/SemanticContextualizer.h"
#include "../AI/SpectralContext.h"
#include "../AI/SemanticTargetBuilder.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
using namespace AIEQPerceptual;

std::vector<float> centers12PerOctave()
{
    std::vector<float> out;
    const float step = std::pow(2.0f, 1.0f / 12.0f);
    for (float f = 20.0f; f < 24000.0f / step; f *= step)
        out.push_back(f);
    return out;
}

/** Gaussian bump in log-frequency. width is in octaves. */
void addBump(std::vector<float>& db, const std::vector<float>& c,
             float centreHz, float gainDb, float widthOct)
{
    for (std::size_t i = 0; i < c.size(); ++i)
        db[i] += gainDb * std::exp(-0.5f * std::pow(std::log2(c[i] / centreHz) / widthOct, 2.0f));
}

std::vector<float> flatAt(const std::vector<float>& c, float baseDb)
{
    return std::vector<float>(c.size(), baseDb);
}

SpectralContext contextOf(const std::vector<float>& c, const std::vector<float>& db, int frames = 24)
{
    SpectralContextAccumulator a;
    a.prepare(c);
    for (int i = 0; i < frames; ++i)
        a.pushFrame(db, true);
    return a.snapshot();
}

SemanticIntent goal(SemanticDimension d, float amount,
                    SemanticSpectralFocus focus = SemanticSpectralFocus::General)
{
    SemanticIntent i;
    i.hasRecognizedContent = true;
    i.confidence = 1.0f;
    SemanticGoal g;
    g.dimension = d;
    g.amount = amount;
    g.confidence = 1.0f;
    g.sourcePhrase = "adversarial";
    g.focus = focus;
    i.goals.push_back(std::move(g));
    return i;
}

float scaleFor(const SemanticContextualizer& ctx, SemanticDimension d, float amount,
               const SpectralContext& c,
               SemanticSpectralFocus focus = SemanticSpectralFocus::General)
{
    const auto out = ctx.contextualize(goal(d, amount, focus), c);
    return out.adjustments.empty() ? 1.0f : out.adjustments.front().scale;
}

juce::String f2(float v) { return juce::String(v, 2); }
}

class SpectralContextAdversarialTest final : public juce::UnitTest
{
public:
    SpectralContextAdversarialTest()
        : juce::UnitTest("Spectral Context Adversarial (T4 review)", "AI-Diag") {}

    void runTest() override
    {
        const auto c = centers12PerOctave();
        const SemanticContextualizer ctx;

        //==================================================================
        beginTest("1. Bright in the presence band but short of air");
        {
            // 3-7 kHz is strong, above 10 kHz is not. Brightness sums Presence
            // 0.40 + Brilliance 0.35 + Air 0.25, so a source can read "already
            // bright" while the region the user actually asked about is empty.
            auto db = flatAt(c, -50.0f);
            addBump(db, c, 4500.0f, 10.0f, 0.85f);
            addBump(db, c, 14000.0f, -8.0f, 0.90f);
            const auto sc = contextOf(c, db);

            logMessage("  Presence=" + f2(sc.region(SpectralRegion::Presence))
                     + "  Brilliance=" + f2(sc.region(SpectralRegion::Brilliance))
                     + "  Air=" + f2(sc.region(SpectralRegion::Air))
                     + "  hfActivity=" + f2(sc.broadHighFrequencyProminence));
            const float generalScale = scaleFor(ctx, SemanticDimension::Brightness, 1.0f, sc);
            const float airScale = scaleFor(ctx, SemanticDimension::Brightness, 1.0f, sc,
                                            SemanticSpectralFocus::Air);
            logMessage("  general brightness axis=" + f2(ctx.axisPosition(SemanticDimension::Brightness, sc))
                     + "  scale=" + f2(generalScale));
            logMessage("  AIR-focus axis=" + f2(ctx.axisPosition(SemanticDimension::Brightness,
                                                                 SemanticSpectralFocus::Air, sc))
                     + "  scale=" + f2(airScale));

            expect(sc.valid, "context invalid on a well-formed source");
            expect(airScale > generalScale + 0.05f,
                   "an air request on an air-poor source is damped as hard as a generic "
                   "brightness request; the facet is not changing the decision");
            expect(airScale > 0.9f,
                   "the source is 5.9 dB SHORT of air and the user asked for air, yet the "
                   "request is still being reduced");
        }

        //==================================================================
        beginTest("2. Dark source carrying a narrow HF spike");
        {
            // Globally dark, but with a narrow spike at 12 kHz. The question is
            // whether Air (10-24 kHz, one averaged region) can see it at all.
            auto dark = flatAt(c, -50.0f);
            addBump(dark, c, 200.0f, 6.0f, 1.2f);
            addBump(dark, c, 9000.0f, -10.0f, 1.1f);
            auto spiky = dark;
            addBump(spiky, c, 12000.0f, 14.0f, 0.16f); // narrow

            const auto scDark = contextOf(c, dark);
            const auto scSpiky = contextOf(c, spiky);

            logMessage("  dark : Air=" + f2(scDark.region(SpectralRegion::Air))
                     + "  hfAct=" + f2(scDark.broadHighFrequencyProminence)
                     + "  axis=" + f2(ctx.axisPosition(SemanticDimension::Brightness, scDark))
                     + "  scale=" + f2(scaleFor(ctx, SemanticDimension::Brightness, 1.0f, scDark)));
            logMessage("  spike: Air=" + f2(scSpiky.region(SpectralRegion::Air))
                     + "  hfAct=" + f2(scSpiky.broadHighFrequencyProminence)
                     + "  axis=" + f2(ctx.axisPosition(SemanticDimension::Brightness, scSpiky))
                     + "  scale=" + f2(scaleFor(ctx, SemanticDimension::Brightness, 1.0f, scSpiky)));
            logMessage("  delta Air = "
                     + f2(scSpiky.region(SpectralRegion::Air) - scDark.region(SpectralRegion::Air))
                     + " dB  (a 14 dB narrow spike, averaged across 10-24 kHz)");
            logMessage("  hfPeakProminence: dark=" + f2(scDark.hfPeakProminenceDb)
                     + "  spike=" + f2(scSpiky.hfPeakProminenceDb));
            const float airDark = scaleFor(ctx, SemanticDimension::Brightness, 1.0f, scDark,
                                           SemanticSpectralFocus::Air);
            const float airSpike = scaleFor(ctx, SemanticDimension::Brightness, 1.0f, scSpiky,
                                            SemanticSpectralFocus::Air);
            logMessage("  '+air' scale: dark=" + f2(airDark) + "  spike=" + f2(airSpike));

            expect(scSpiky.hfPeakProminenceDb > scDark.hfPeakProminenceDb + 1.0f,
                   "a 14 dB narrow spike at 12 kHz is invisible to every HF descriptor; "
                   "the policy cannot tell a broadly dark top from one with a spike in it");
            expect(airSpike <= airDark + 1.0e-4f,
                   "the source with an isolated HF spike is granted at least as much air "
                   "boost as the clean dark one");
        }

        //==================================================================
        beginTest("3. Warm versus muddy at equal low-mid energy");
        {
            // Same bump amplitude and width, moved from 170 Hz to 370 Hz. The
            // region grid puts 160-500 Hz in a single bucket, so the question is
            // how much of that distinction survives.
            auto warm = flatAt(c, -50.0f);
            addBump(warm, c, 170.0f, 8.0f, 0.55f);
            auto muddy = flatAt(c, -50.0f);
            addBump(muddy, c, 370.0f, 8.0f, 0.55f);

            const auto scWarm = contextOf(c, warm);
            const auto scMuddy = contextOf(c, muddy);

            const float axWarm = ctx.axisPosition(SemanticDimension::Warmth, scWarm);
            const float axMuddy = ctx.axisPosition(SemanticDimension::Warmth, scMuddy);

            logMessage("  A(170Hz): Bass=" + f2(scWarm.region(SpectralRegion::Bass))
                     + " LowMid=" + f2(scWarm.region(SpectralRegion::LowMid))
                     + " warmthAxis=" + f2(axWarm)
                     + " scale=" + f2(scaleFor(ctx, SemanticDimension::Warmth, 1.0f, scWarm)));
            logMessage("  B(370Hz): Bass=" + f2(scMuddy.region(SpectralRegion::Bass))
                     + " LowMid=" + f2(scMuddy.region(SpectralRegion::LowMid))
                     + " warmthAxis=" + f2(axMuddy)
                     + " scale=" + f2(scaleFor(ctx, SemanticDimension::Warmth, 1.0f, scMuddy)));
            logMessage("  clarityAxis A=" + f2(ctx.axisPosition(SemanticDimension::Clarity, scWarm))
                     + "  B=" + f2(ctx.axisPosition(SemanticDimension::Clarity, scMuddy)));

            const float scWarmScale = scaleFor(ctx, SemanticDimension::Warmth, 1.0f, scWarm);
            const float scMuddyScale = scaleFor(ctx, SemanticDimension::Warmth, 1.0f, scMuddy);
            logMessage("  mudRisk A=" + f2(ctx.mudRisk(scWarm))
                     + "  B=" + f2(ctx.mudRisk(scMuddy)));
            logMessage("  warmthZone A=" + f2(scWarm.warmthZoneDb) + " B=" + f2(scMuddy.warmthZoneDb)
                     + "   mudZone A=" + f2(scWarm.mudZoneDb) + " B=" + f2(scMuddy.mudZoneDb));

            expect(std::abs(axWarm - axMuddy) > 1.0e-3f,
                   "two low-mid distributions that a listener would call warm and "
                   "muddy produce an identical warmth axis");
            expect(scMuddyScale <= scWarmScale + 1.0e-4f,
                   "the MUDDIER source is granted more room to add warmth than the "
                   "genuinely warm one - the ordering is inverted");
        }

        //==================================================================
        beginTest("3b. CONTROL: a broadly bright source still damps generic 'brighter'");
        {
            // Guards the fix for case 1: making the Air facet permissive must not
            // make generic brightness permissive too. This behaviour was correct
            // before T4.1 and must stay correct.
            auto db = flatAt(c, -50.0f);
            addBump(db, c, 3000.0f, 7.0f, 1.4f);
            addBump(db, c, 9000.0f, 7.0f, 1.2f);
            addBump(db, c, 14000.0f, 6.0f, 1.0f);
            const auto sc = contextOf(c, db);

            const float scale = scaleFor(ctx, SemanticDimension::Brightness, 1.0f, sc);
            logMessage("  Presence=" + f2(sc.region(SpectralRegion::Presence))
                     + " Brilliance=" + f2(sc.region(SpectralRegion::Brilliance))
                     + " Air=" + f2(sc.region(SpectralRegion::Air)));
            logMessage("  general axis=" + f2(ctx.axisPosition(SemanticDimension::Brightness, sc))
                     + "  scale=" + f2(scale));
            expect(scale < 0.9f,
                   "a source that is genuinely bright across presence, brilliance AND air "
                   "no longer damps a generic 'brighter' request");
        }

        //==================================================================
        beginTest("3c. Focus decides WHERE the target sits, not only how much");
        {
            // The contextualizer learned to tell "more air" from "brighter" in
            // T4.1, but until the builder reads the same facet both requests
            // produce the identical 6 kHz shelf: understanding without geometry.
            // Same amount everywhere, so any difference is the facet alone.
            auto centroidOf = [](SemanticSpectralFocus focus)
            {
                SemanticIntent in;
                in.hasRecognizedContent = true;
                in.confidence = 1.0f;
                SemanticGoal g;
                g.dimension = SemanticDimension::Brightness;
                g.amount = 0.8f;
                g.confidence = 1.0f;
                g.sourcePhrase = "focus geometry";
                g.focus = focus;
                in.goals.push_back(std::move(g));

                const auto target = SemanticTargetBuilder().build(in, 48000.0);
                double w = 0.0, wl = 0.0, below8k = 0.0, total = 0.0;
                for (const auto& pt : target.points)
                {
                    const double weight = std::max(0.0f, pt.deltaDb);
                    w += weight;
                    wl += weight * std::log2(static_cast<double>(pt.frequencyHz));
                    total += weight;
                    if (pt.frequencyHz < 8000.0f) below8k += weight;
                }
                struct R { float centroidHz; float fractionBelow8k; std::size_t points; };
                return R { w > 0.0 ? static_cast<float>(std::exp2(wl / w)) : 0.0f,
                           total > 0.0 ? static_cast<float>(below8k / total) : 0.0f,
                           target.points.size() };
            };

            const auto general    = centroidOf(SemanticSpectralFocus::General);
            const auto presence   = centroidOf(SemanticSpectralFocus::Presence);
            const auto brilliance = centroidOf(SemanticSpectralFocus::Brilliance);
            const auto air        = centroidOf(SemanticSpectralFocus::Air);

            auto line = [&](const char* n, auto r)
            {
                logMessage("  " + juce::String(n) + ": centroid=" + juce::String(r.centroidHz, 0)
                           + " Hz  energy below 8k=" + f2(r.fractionBelow8k * 100.0f) + "%");
            };
            line("General   ", general);
            line("Presence  ", presence);
            line("Brilliance", brilliance);
            line("Air       ", air);

            expect(presence.centroidHz < brilliance.centroidHz,
                   "a Presence-focused request does not sit below a Brilliance-focused one");
            expect(brilliance.centroidHz < air.centroidHz,
                   "a Brilliance-focused request does not sit below an Air-focused one");
            expect(air.centroidHz > general.centroidHz,
                   "'more air' still lands no higher than a generic 'brighter'");
            expect(air.fractionBelow8k < presence.fractionBelow8k,
                   "an air request puts as much energy under 8 kHz as a presence request");
        }

        //==================================================================
        beginTest("4. No cliff across the confidence boundary");
        {
            float previousConfidence = -1.0f, worstJump = 0.0f;
            float worstAtDb = 0.0f;
            juce::String trace;
            for (float base = -100.0f; base <= -40.0f; base += 2.0f)
            {
                auto db = flatAt(c, base);
                addBump(db, c, 300.0f, 6.0f, 0.8f);
                const auto sc = contextOf(c, db);
                if (previousConfidence >= 0.0f)
                {
                    const float jump = std::abs(sc.confidence - previousConfidence);
                    if (jump > worstJump) { worstJump = jump; worstAtDb = base; }
                }
                previousConfidence = sc.confidence;
                if (base >= -70.0f && base <= -52.0f)
                    trace << f2(base) << "dB->" << f2(sc.confidence) << " ";
            }
            logMessage("  confidence near the gate: " + trace);
            logMessage("  worst step over a 2 dB level change = " + f2(worstJump)
                     + " (at " + f2(worstAtDb) + " dB)");
            expect(worstJump < 0.5f,
                   "confidence moves more than 0.5 across a 2 dB level step, which is "
                   "a cliff: two takes of the same material would be contextualized "
                   "very differently");
        }

        //==================================================================
        beginTest("5. Level invariance holds until the usable floor bites");
        {
            // The normalization subtracts a median, so shape is invariant by
            // construction. But the median and the tilt regression are computed
            // over bands filtered by an ABSOLUTE threshold (minUsableDb), so as
            // the source approaches that floor the admitted band set changes and
            // absolute level can leak into a shape descriptor. This measures
            // where that begins.
            auto shape = [&](float base)
            {
                auto db = flatAt(c, base);
                addBump(db, c, 250.0f, 7.0f, 0.9f);
                addBump(db, c, 6000.0f, 5.0f, 0.9f);
                return contextOf(c, db);
            };
            const auto ref = shape(-40.0f);
            juce::String trace;
            float worstDrift = 0.0f;
            for (float base : { -50.0f, -60.0f, -70.0f, -80.0f, -90.0f, -100.0f })
            {
                const auto sc = shape(base);
                float drift = 0.0f;
                for (std::size_t r = 0; r < kSpectralRegionCount; ++r)
                    drift = std::max(drift, std::abs(sc.regionRelativeDb[r] - ref.regionRelativeDb[r]));
                const float tiltDrift = std::abs(sc.spectralTiltDbPerOctave - ref.spectralTiltDbPerOctave);
                worstDrift = std::max(worstDrift, drift);
                trace << "\n    " << f2(base) << " dB: maxRegionDrift=" << f2(drift)
                      << "  tiltDrift=" << f2(tiltDrift)
                      << "  conf=" << f2(sc.confidence);
            }
            logMessage("  vs the -40 dB reference:" + trace);
            logMessage("  worst region drift across the sweep = " + f2(worstDrift) + " dB");
        }

        //==================================================================
        beginTest("6. Contract invariants hold across every case (hard)");
        {
            std::vector<SpectralContext> corpus;
            for (float base : { -35.0f, -55.0f, -75.0f, -95.0f })
                for (float bumpHz : { 90.0f, 300.0f, 3000.0f, 12000.0f })
                {
                    auto db = flatAt(c, base);
                    addBump(db, c, bumpHz, 9.0f, 0.7f);
                    corpus.push_back(contextOf(c, db));
                }

            int checked = 0;
            for (const auto& sc : corpus)
                for (auto d : { SemanticDimension::Brightness, SemanticDimension::Warmth,
                                SemanticDimension::Clarity, SemanticDimension::Presence,
                                SemanticDimension::Weight, SemanticDimension::Punch })
                    for (float amount : { -1.0f, -0.4f, 0.4f, 1.0f })
                    {
                        const auto in = goal(d, amount);
                        const auto out = ctx.contextualize(in, sc);
                        ++checked;

                        expect(out.intent.goals.size() == in.goals.size(),
                               "context changed the number of goals");
                        const float before = in.goals.front().amount;
                        const float after = out.intent.goals.front().amount;

                        expect(std::isfinite(after), "context produced a non-finite amount");
                        expect(before * after >= 0.0f,
                               "context flipped the sign of a goal");
                        expect(std::abs(after) <= std::abs(before) + 1.0e-6f,
                               "context asked for more than the user did");
                        expect(out.intent.constraints.size() == in.constraints.size(),
                               "context altered the constraint set");
                    }
            logMessage("  " + juce::String(checked) + " (context, dimension, amount) combinations checked");
        }

        //==================================================================
        beginTest("7. Below the confidence gate, contextualization is exactly identity");
        {
            auto db = flatAt(c, -103.0f);          // deep enough to gate out
            addBump(db, c, 300.0f, 6.0f, 0.8f);
            const auto sc = contextOf(c, db, 24);
            logMessage("  confidence=" + f2(sc.confidence));

            for (float amount : { -1.0f, -0.25f, 0.25f, 1.0f })
            {
                const auto in = goal(SemanticDimension::Warmth, amount);
                const auto out = ctx.contextualize(in, sc);
                expect(out.intent.goals.front().amount == in.goals.front().amount,
                       "a low-confidence context still changed the requested amount");
                expect(!out.contextApplied, "contextApplied set below the confidence gate");
            }
        }
    }
};

static SpectralContextAdversarialTest sSpectralContextAdversarialTest;

#endif // JUCE_UNIT_TESTS
