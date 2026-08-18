#if JUCE_UNIT_TESTS

/*
 * T5.3 - the two halves of Semantic actually meeting.
 *
 * Everything up to here was proven separately: the compiler understands text,
 * the contextualizer knows how to scale a goal against a SpectralContext, and
 * the front-end produces a real snapshot from real audio. This is the first
 * point where a single call - SemanticPlanner::plan(text, sr, intensity,
 * context) - carries a snapshot all the way to a fitted EQ plan, which is the
 * thing the product actually needs to be true.
 *
 * Deliberately testing ORDERING and RATIOS rather than absolute numbers
 * throughout: the policy coefficients in SemanticContextualizer/TargetBuilder
 * will keep moving as they get tuned by ear, and a test that hardcodes today's
 * output would fail on every such tuning for no product reason. What must not
 * change is the shape of the decision - dark gets more, bright gets less, mud
 * stays fenced - and that is what is asserted.
 */

#include <juce_core/juce_core.h>
#include "../AI/SemanticPlanner.h"
#include "../AI/SemanticTargetBuilder.h"
#include "../AI/SemanticPlanningService.h"

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

/** Mature, high-confidence context: enough frames that warmup/coverage/level
    all read as fully trusted, so the test isolates the SHAPE question rather
    than accidentally exercising the confidence gate. */
SpectralContext matureContext(const std::vector<float>& c, const std::vector<float>& db)
{
    SpectralContextAccumulator a;
    a.prepare(c);
    for (int i = 0; i < 30; ++i)
        a.pushFrame(db, true);
    return a.snapshot();
}

float maxPositiveDb(const PerceptualTarget& target)
{
    float m = 0.0f;
    for (const auto& p : target.points)
        m = std::max(m, p.deltaDb);
    return m;
}

float goalContextScale(const SemanticPlan& p, SemanticDimension d)
{
    for (const auto& adj : p.contextAdjustments)
        if (adj.dimension == d)
            return adj.scale;
    return 1.0f;
}

/** Worst violation of a hard "do not increase here" constraint: the largest
    positive plan response inside the named frequency band, or 0 if the plan
    never goes positive there. */
float worstPositiveResponseInBand(const FitResult& fit, double sr,
                                 float loHz, float hiHz)
{
    float worst = 0.0f;
    for (float f = loHz; f <= hiHz; f *= 1.05946f) // 1/12 octave steps
        worst = std::max(worst, SparseParametricFitter::evaluatePlanDb(fit.bands, f, sr));
    return worst;
}

juce::String f2(float v) { return juce::String(v, 3); }
}

class SemanticPlanningContextTest final : public juce::UnitTest
{
public:
    SemanticPlanningContextTest()
        : juce::UnitTest("Semantic Planning With Context (T5.3)", "Integration") {}

    void runTest() override
    {
        const auto c = centers12PerOctave();
        constexpr double kSr = 48000.0;
        const SemanticPlanner planner;

        //==================================================================
        beginTest("1. 'more air' moves further on a dark source than an airy one");
        {
            // Dark: strong low end, empty above 10 kHz. Airy: already extended
            // on top. Same sentence, opposite ends of the axis it names.
            auto darkDb = flatAt(c, -50.0f);
            addBump(darkDb, c, 200.0f, 6.0f, 1.1f);
            addBump(darkDb, c, 9000.0f, -12.0f, 1.0f);
            auto airyDb = flatAt(c, -50.0f);
            addBump(airyDb, c, 12000.0f, 9.0f, 1.3f);

            const auto dark = matureContext(c, darkDb);
            const auto airy = matureContext(c, airyDb);

            const auto planDark = planner.plan("more air", kSr, 1.0f, dark);
            const auto planAiry = planner.plan("more air", kSr, 1.0f, airy);

            expect(planDark.valid && planAiry.valid, "harness precondition: one plan is invalid");
            expect(!planDark.intent.goals.empty() && !planAiry.intent.goals.empty(),
                   "harness precondition: no goal in one of the plans");

            const auto& goalDark = planDark.intent.goals.front();
            const auto& goalAiry = planAiry.intent.goals.front();
            expect(goalDark.dimension == SemanticDimension::Brightness
                       && goalAiry.dimension == SemanticDimension::Brightness,
                   "'more air' did not compile to Brightness on both sources");
            expect(goalDark.focus == SemanticSpectralFocus::Air
                       && goalAiry.focus == SemanticSpectralFocus::Air,
                   "'more air' lost its Air focus depending on the source");

            const float scaleDark = goalContextScale(planDark, SemanticDimension::Brightness);
            const float scaleAiry = goalContextScale(planAiry, SemanticDimension::Brightness);
            const float peakDark = maxPositiveDb(planDark.target);
            const float peakAiry = maxPositiveDb(planAiry.target);
            const float moveDark = SparseParametricFitter::evaluatePlanDb(planDark.fit.bands, 14000.0f, kSr);
            const float moveAiry = SparseParametricFitter::evaluatePlanDb(planAiry.fit.bands, 14000.0f, kSr);

            logMessage("  DARK: context scale=" + f2(scaleDark) + "  target peak=" + f2(peakDark)
                     + " dB  plan@14k=" + f2(moveDark) + " dB");
            logMessage("  AIRY: context scale=" + f2(scaleAiry) + "  target peak=" + f2(peakAiry)
                     + " dB  plan@14k=" + f2(moveAiry) + " dB");

            expect(scaleDark > scaleAiry + 1.0e-4f,
                   "the dark source is not granted more contextual room than the airy one");
            expect(peakDark > peakAiry,
                   "the fitted TARGET peak is not larger on the dark source");
            expect(moveDark > moveAiry,
                   "the actual FITTED PLAN moves less at 14 kHz on the dark source than the airy one");
        }

        //==================================================================
        beginTest("2. 'warmer without mud' moves further on a lean source, mud stays fenced");
        {
            // Lean: real energy in the warmth zone, little in the mud zone.
            // Muddy: the reverse, same total low-mid energy either way.
            auto leanDb = flatAt(c, -50.0f);
            addBump(leanDb, c, 180.0f, 8.0f, 0.55f);
            auto muddyDb = flatAt(c, -50.0f);
            addBump(muddyDb, c, 380.0f, 8.0f, 0.55f);

            const auto lean = matureContext(c, leanDb);
            const auto muddy = matureContext(c, muddyDb);

            const auto planLean = planner.plan("warmer without mud", kSr, 1.0f, lean);
            const auto planMuddy = planner.plan("warmer without mud", kSr, 1.0f, muddy);

            expect(planLean.valid && planMuddy.valid, "harness precondition: one plan is invalid");

            const float scaleLean = goalContextScale(planLean, SemanticDimension::Warmth);
            const float scaleMuddy = goalContextScale(planMuddy, SemanticDimension::Warmth);
            const float moveLean = SparseParametricFitter::evaluatePlanDb(planLean.fit.bands, 180.0f, kSr);
            const float moveMuddy = SparseParametricFitter::evaluatePlanDb(planMuddy.fit.bands, 180.0f, kSr);

            // "without mud" compiles to a hard AvoidDirection constraint on the
            // mud region: the plan must never go positive there, on EITHER
            // source, regardless of how much warmth was granted.
            const float mudRespLean = worstPositiveResponseInBand(planLean.fit, kSr, 250.0f, 500.0f);
            const float mudRespMuddy = worstPositiveResponseInBand(planMuddy.fit, kSr, 250.0f, 500.0f);

            logMessage("  LEAN : warmth scale=" + f2(scaleLean) + "  plan@180Hz=" + f2(moveLean)
                     + " dB  worst mud response=" + f2(mudRespLean) + " dB");
            logMessage("  MUDDY: warmth scale=" + f2(scaleMuddy) + "  plan@180Hz=" + f2(moveMuddy)
                     + " dB  worst mud response=" + f2(mudRespMuddy) + " dB");

            expect(scaleLean > scaleMuddy + 1.0e-4f,
                   "the lean source is not granted more warmth room than the muddy one");
            expect(moveLean > moveMuddy,
                   "the fitted plan does not add more warmth on the lean source");
            // Anchored to the builder's OWN avoidToleranceDb (0.10 dB) rather
            // than a number picked to make this test pass - a looser bound here
            // would test something weaker than what the product itself defines
            // as "the constraint is respected".
            const float avoidTolerance = SemanticTargetBuilder::Options {}.avoidToleranceDb;
            expect(mudRespLean <= avoidTolerance,
                   "the mud constraint is violated on the LEAN source beyond the "
                   "builder's own avoid tolerance");
            expect(mudRespMuddy <= avoidTolerance,
                   "the mud constraint is violated on the MUDDY source beyond the "
                   "builder's own avoid tolerance");
        }

        //==================================================================
        beginTest("3. Low-confidence context is identical to no context at all");
        {
            // Deep enough that the confidence gate fails closed (T4 measured
            // this floor: confidence collapses well before -103 dB).
            auto db = flatAt(c, -50.0f);
            addBump(db, c, 200.0f, 6.0f, 1.0f);
            addBump(db, c, 9000.0f, -10.0f, 1.0f);

            SpectralContextAccumulator low;
            low.prepare(c);
            auto quiet = flatAt(c, -103.0f);
            addBump(quiet, c, 200.0f, 6.0f, 1.0f);
            addBump(quiet, c, 9000.0f, -10.0f, 1.0f);
            for (int i = 0; i < 30; ++i)
                low.pushFrame(quiet, true);
            const auto lowConfidenceContext = low.snapshot();
            logMessage("  low-confidence context: confidence=" + f2(lowConfidenceContext.confidence));

            const auto withLowConfidence = planner.plan("more air", kSr, 1.0f, lowConfidenceContext);
            const auto withoutContext = planner.plan("more air", kSr, 1.0f);

            expect(!withLowConfidence.contextApplied,
                   "contextApplied is set despite a low-confidence context");
            expect(std::abs(maxPositiveDb(withLowConfidence.target)
                           - maxPositiveDb(withoutContext.target)) < 1.0e-4f,
                   "a low-confidence context produced a different target than no context at all");
        }

        //==================================================================
        beginTest("4. The worker plans against the frozen snapshot, not a moving one");
        {
            // Same guarantee T5.1 proved for getSpectralContextSnapshot() itself,
            // now proved for the request that actually reaches the worker: once
            // submitted, further audio must not change what gets planned.
            auto darkDb = flatAt(c, -50.0f);
            addBump(darkDb, c, 200.0f, 6.0f, 1.1f);
            addBump(darkDb, c, 9000.0f, -12.0f, 1.0f);
            const auto frozen = matureContext(c, darkDb);

            SemanticPlanningService svc;
            svc.start();
            const auto gen = svc.submit("more air", 1.0f, kSr, frozen);
            expect(svc.waitUntilQuiescent(8000), "planner did not settle");
            auto result = svc.takeCurrentResult();
            svc.stop();

            expect(result.has_value() && result->generation == gen,
                   "no current-generation result after submitting with a context");
            if (!result.has_value())
                return;

            // Recompute directly against the SAME frozen snapshot: if the worker
            // had reached into something live instead of the value it was given,
            // this would not match.
            const auto direct = planner.plan("more air", kSr, 1.0f, frozen);
            expect(std::abs(maxPositiveDb(result->plan.target) - maxPositiveDb(direct.target)) < 1.0e-4f,
                   "the worker's result does not match planning directly against "
                   "the same frozen context - it read something other than the "
                   "value it was given");
        }

        //==================================================================
        beginTest("5. A new submission invalidates the OLD text+context together");
        {
            // The epoch must protect the whole frozen picture, not only the
            // text: a stale generation must be unusable even though the text
            // in it is a request that would otherwise be perfectly valid.
            auto darkDb = flatAt(c, -50.0f);
            addBump(darkDb, c, 200.0f, 6.0f, 1.1f);
            addBump(darkDb, c, 9000.0f, -12.0f, 1.0f);
            auto airyDb = flatAt(c, -50.0f);
            addBump(airyDb, c, 12000.0f, 9.0f, 1.3f);

            SemanticPlanningService svc;
            svc.start();
            const auto genOld = svc.submit("more air", 1.0f, kSr, matureContext(c, darkDb));
            const auto genNew = svc.submit("more air", 1.0f, kSr, matureContext(c, airyDb));
            expect(genNew > genOld, "generation did not advance on resubmit");

            expect(svc.waitUntilQuiescent(8000), "planner did not settle");
            const auto result = svc.takeCurrentResult();

            expect(result.has_value(), "no result at all after two submissions");
            if (result.has_value())
            {
                logMessage("  old=" + juce::String((int) genOld) + "  new=" + juce::String((int) genNew)
                         + "  delivered=" + juce::String((int) result->generation));
                expect(result->generation == genNew,
                       "a result belonging to the OLD text+context pair was delivered "
                       "as current after a newer one, with different text AND context, "
                       "was submitted");
            }
            svc.stop();
        }
    }
};

static SemanticPlanningContextTest sSemanticPlanningContextTest;

#endif // JUCE_UNIT_TESTS
