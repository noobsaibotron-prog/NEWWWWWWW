#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticIntentCompiler.h"
#include "../AI/SemanticTargetBuilder.h"
#include "../AI/SemanticPlanner.h"
#include "../AI/SparseParametricFitter.h"

#include <cmath>

namespace
{
using namespace AIEQPerceptual;
constexpr double kSampleRate = 48000.0;

const SemanticGoal* findGoal(const SemanticIntent& intent, SemanticDimension dimension)
{
    for (const auto& goal : intent.goals)
        if (goal.dimension == dimension)
            return &goal;
    return nullptr;
}

const SemanticConstraint* findConstraint(const SemanticIntent& intent,
                                         SemanticDimension dimension)
{
    for (const auto& constraint : intent.constraints)
        if (constraint.dimension == dimension)
            return &constraint;
    return nullptr;
}

bool hasMudFence(const SemanticPlan& plan)
{
    for (const auto& bound : plan.target.responseBounds)
        if (bound.minFrequencyHz >= 240.0f && bound.minFrequencyHz <= 260.0f
            && bound.maxFrequencyHz >= 490.0f && bound.maxFrequencyHz <= 510.0f)
            return true;
    return false;
}

const PlannedBand* findBandNear(const FitResult& fit, float hz, float tolHz)
{
    const PlannedBand* best = nullptr;
    float bestErr = tolHz + 1.0f;
    for (const auto& band : fit.bands)
    {
        const float err = std::abs(band.frequencyHz - hz);
        if (err <= tolHz && err < bestErr)
        {
            best = &band;
            bestErr = err;
        }
    }
    return best;
}

bool interpretationHas(const SemanticPlan& plan, const char* needle)
{
    return plan.interpretation.find(needle) != std::string::npos;
}
} // namespace

class SemanticIntentCompilerTest final : public juce::UnitTest
{
public:
    SemanticIntentCompilerTest()
        : juce::UnitTest("Semantic Intent Compiler", "AI-Diag") {}

    void runTest() override
    {
        SemanticIntentCompiler compiler;

        beginTest("Clause-local polarity: less harsh but more air");
        {
            const auto intent = compiler.compile("less harsh but more air");
            const auto* smooth = findGoal(intent, SemanticDimension::Smoothness);
            const auto* bright = findGoal(intent, SemanticDimension::Brightness);
            expect(intent.isValid());
            expect(smooth != nullptr && smooth->amount > 0.0f,
                   "less harsh must compile toward +Smoothness");
            expect(bright != nullptr && bright->amount > 0.0f,
                   "more air must remain positive despite earlier less");
        }

        beginTest("without creates an avoid-direction constraint, not a second goal");
        {
            const auto intent = compiler.compile("warmer without mud");
            const auto* warmth = findGoal(intent, SemanticDimension::Warmth);
            const auto* mudGuard = findConstraint(intent, SemanticDimension::Clarity);
            expect(warmth != nullptr && warmth->amount > 0.0f);
            expect(mudGuard != nullptr);
            if (mudGuard != nullptr)
            {
                expect(mudGuard->kind == SemanticConstraintKind::AvoidDirection);
                expectEquals(mudGuard->direction, -1);
            }

            // "mud" is a constraint here and must not also become a Clarity goal.
            expect(findGoal(intent, SemanticDimension::Clarity) == nullptr);
        }

        beginTest("keep/don't touch creates a preservation constraint");
        {
            const auto intent = compiler.compile("more air, don't touch the low end");
            const auto* bright = findGoal(intent, SemanticDimension::Brightness);
            const auto* keepWeight = findConstraint(intent, SemanticDimension::Weight);
            expect(bright != nullptr && bright->amount > 0.0f);
            expect(keepWeight != nullptr);
            if (keepWeight != nullptr)
                expect(keepWeight->kind == SemanticConstraintKind::Preserve);
        }

        beginTest("Intensity words remain local to their clause");
        {
            const auto intent = compiler.compile("slightly less muddy but much more presence");
            const auto* clarity = findGoal(intent, SemanticDimension::Clarity);
            const auto* presence = findGoal(intent, SemanticDimension::Presence);
            expect(clarity != nullptr && presence != nullptr);
            if (clarity != nullptr && presence != nullptr)
            {
                expect(clarity->amount > 0.0f && presence->amount > 0.0f);
                expect(std::abs(clarity->amount) < std::abs(presence->amount),
                       "slightly and much must not collapse to one global intensity");
            }
        }

        beginTest("Constraint scope does not swallow a later goal");
        {
            const auto intent = compiler.compile("warmer without mud and more air");
            expect(findGoal(intent, SemanticDimension::Warmth) != nullptr);
            expect(findGoal(intent, SemanticDimension::Brightness) != nullptr);
            expect(findConstraint(intent, SemanticDimension::Clarity) != nullptr);
        }

        beginTest("Less open remains negative brightness");
        {
            const auto intent = compiler.compile("less open");
            const auto* brightness = findGoal(intent, SemanticDimension::Brightness);
            expect(brightness != nullptr && brightness->amount < 0.0f);
        }

        beginTest("Without losing weight means preserve, not avoid more weight");
        {
            const auto intent = compiler.compile("without losing weight");
            const auto* weight = findConstraint(intent, SemanticDimension::Weight);
            expect(weight != nullptr);
            if (weight != nullptr)
                expect(weight->kind == SemanticConstraintKind::Preserve);
        }

        beginTest("P0-C studio phrases: add bass / add mids / more air");
        {
            const auto bass = compiler.compile("add some bass");
            const auto mids = compiler.compile("add some mids");
            const auto air  = compiler.compile("more air");
            expect(bass.isValid() && findGoal(bass, SemanticDimension::Weight) != nullptr,
                   "add some bass must compile to +Weight");
            expect(mids.isValid() && findGoal(mids, SemanticDimension::Presence) != nullptr,
                   "add some mids must compile to +Presence");
            expect(air.isValid() && findGoal(air, SemanticDimension::Brightness) != nullptr,
                   "more air must compile to +Brightness");

            const auto bassPlan = SemanticPlanner().plan("add some bass", kSampleRate);
            const auto midsPlan = SemanticPlanner().plan("add some mids", kSampleRate);
            const auto airPlan  = SemanticPlanner().plan("more air", kSampleRate);
            expect(bassPlan.valid && !bassPlan.fit.bands.empty());
            expect(midsPlan.valid && !midsPlan.fit.bands.empty());
            expect(airPlan.valid && !airPlan.fit.bands.empty());
        }

        beginTest("P1 studio vocabulary: treble/mids/boxy/scoop");
        {
            struct Case
            {
                const char* phrase;
                SemanticDimension dimension;
                bool positive;
            };

            const Case cases[] = {
                { "add some treble", SemanticDimension::Brightness, true },
                { "more highs", SemanticDimension::Brightness, true },
                { "add some low mids", SemanticDimension::Warmth, true },
                { "more upper mids", SemanticDimension::Presence, true },
                { "more bite", SemanticDimension::Presence, true },
                { "less boxy", SemanticDimension::Clarity, true },
                { "scoop the mids", SemanticDimension::Presence, false },
                { "less nasal", SemanticDimension::Presence, true },
                { "less sibilance", SemanticDimension::Smoothness, true },
                { "add some punch", SemanticDimension::Punch, true },
                { "più medi", SemanticDimension::Presence, true },
                { "più acuti", SemanticDimension::Brightness, true },
                { "più gravi", SemanticDimension::Weight, true },
            };

            for (const auto& c : cases)
            {
                const auto intent = compiler.compile(c.phrase);
                const auto* goal = findGoal(intent, c.dimension);
                expect(intent.isValid() && intent.hasRecognizedContent && !intent.contradictory,
                       juce::String(c.phrase) + " must compile valid");
                expect(goal != nullptr,
                       juce::String(c.phrase) + " must hit the expected dimension");
                if (goal != nullptr)
                {
                    expect(c.positive ? goal->amount > 0.0f : goal->amount < 0.0f,
                           juce::String(c.phrase) + " has the wrong polarity");
                }
            }

            const auto highEnd = compiler.compile("more high end");
            const auto* highEndGoal = findGoal(highEnd, SemanticDimension::Brightness);
            expect(highEnd.isValid() && highEndGoal != nullptr);
            if (highEndGoal != nullptr)
                expect(highEndGoal->sourcePhrase == "high end",
                       "highs must not steal the longer high end alias");

            const auto scoop = compiler.compile("scoop the mids");
            expect(findGoal(scoop, SemanticDimension::Presence) != nullptr);
            expect(!scoop.contradictory, "scoop the mids must not fight mids");

            const auto expensive = compiler.compile("make it expensive and purple");
            expect(!expensive.hasRecognizedContent);
            expect(expensive.goals.empty());

            const auto cinematic = SemanticPlanner().plan("make it cinematic", kSampleRate);
            expect(!cinematic.intent.hasRecognizedContent);
            expect(!cinematic.valid, "cinematic must stay unknown, no guess");

            for (const auto* phrase : { "add some treble", "scoop the mids", "less boxy" })
            {
                const auto plan = SemanticPlanner().plan(phrase, kSampleRate);
                expect(plan.valid && !plan.fit.bands.empty(),
                       juce::String(phrase) + " must PLAN with a non-empty fit");
            }
        }

        beginTest("Italian deterministic path");
        {
            const auto intent = compiler.compile("piu caldo senza impastato");
            expect(findGoal(intent, SemanticDimension::Warmth) != nullptr);
            expect(findConstraint(intent, SemanticDimension::Clarity) != nullptr);
        }

        beginTest("Semantic target + sparse fitter obey directional mud guard");
        {
            const auto intent = compiler.compile("warmer without mud");
            const auto target = SemanticTargetBuilder().build(intent, kSampleRate);
            expect(target.isValid(kSampleRate));
            expect(!target.responseBounds.empty());

            const auto fit = SparseParametricFitter().fit(target, kSampleRate);
            expect(fit.valid);
            expect(fit.maxResponseBoundViolationDb <= 1.1e-4f,
                   "fitter must reject candidates that violate semantic response bounds");
        }

        beginTest("Preserve low end remains hard-bounded after fitting");
        {
            const auto intent = compiler.compile("more air, don't touch the low end");
            const auto target = SemanticTargetBuilder().build(intent, kSampleRate);
            const auto fit = SparseParametricFitter().fit(target, kSampleRate);
            expect(fit.valid);
            expect(fit.maxResponseBoundViolationDb <= 1.1e-4f);
        }

        beginTest("Full SemanticPlanner returns an explainable sparse plan");
        {
            const auto plan = SemanticPlanner().plan("less harsh but more air", kSampleRate);
            expect(plan.valid);
            expect(plan.fit.valid);
            expect(!plan.fit.bands.empty());
            expect(plan.interpretation.find("smoother") != std::string::npos);
            expect(plan.interpretation.find("brighter") != std::string::npos);
        }

        beginTest("Contradictory same-axis instruction fails closed");
        {
            const auto plan = SemanticPlanner().plan("more bright but less bright", kSampleRate);
            expect(plan.intent.contradictory);
            expect(!plan.valid);
        }

        beginTest("Unknown marketing language is not guessed");
        {
            const auto plan = SemanticPlanner().plan("make it expensive and purple", kSampleRate);
            expect(!plan.intent.hasRecognizedContent);
            expect(!plan.valid);
        }

        beginTest("Global intensity scales target magnitude without changing grammar");
        {
            const auto low = SemanticPlanner().plan("more air", kSampleRate, 0.5f);
            const auto high = SemanticPlanner().plan("more air", kSampleRate, 1.5f);
            expect(low.valid && high.valid);

            float lowMax = 0.0f;
            float highMax = 0.0f;
            for (const auto& p : low.target.points)
                lowMax = std::max(lowMax, std::abs(p.deltaDb));
            for (const auto& p : high.target.points)
                highMax = std::max(highMax, std::abs(p.deltaDb));
            expect(lowMax < highMax);
        }


        beginTest("Modifier tokens require word boundaries");
        {
            const auto timeless = compiler.compile("timeless and bright");
            const auto* bright = findGoal(timeless, SemanticDimension::Brightness);
            expect(bright != nullptr && bright->amount > 0.0f,
                   "less inside timeless must not invert brightness");

            const auto careless = compiler.compile("careless but warm");
            const auto* warm = findGoal(careless, SemanticDimension::Warmth);
            expect(warm != nullptr && warm->amount > 0.0f,
                   "less inside careless must not invert warmth");

            const auto scuttle = compiler.compile("scuttle the mud");
            const auto* clarity = findGoal(scuttle, SemanticDimension::Clarity);
            expect(clarity != nullptr && clarity->amount < 0.0f,
                   "cut inside scuttle must not become a negative modifier");
        }

        beginTest("Goal versus preserve constraint fails closed explicitly");
        {
            const auto intent = compiler.compile("more weight, don't touch the low end");
            expect(intent.goalConstraintConflict);
            expect(intent.contradictory);
            const auto plan = SemanticPlanner().plan(
                "more weight, don't touch the low end", kSampleRate);
            expect(!plan.valid);
            expect(plan.interpretation.find("goal conflicts") != std::string::npos);
        }

        beginTest("Goal versus avoid-same-direction constraint fails closed");
        {
            const auto intent = compiler.compile("more mud without mud");
            expect(intent.goalConstraintConflict);
            expect(intent.contradictory);
        }

        beginTest("PerceptualTarget carries a versioned relative-delta contract");
        {
            const auto plan = SemanticPlanner().plan("more air", kSampleRate);
            expect(plan.valid);
            expectEquals(static_cast<int>(plan.target.schemaVersion),
                         static_cast<int>(kPerceptualTargetSchemaVersion));
            expect(plan.target.deltaDomain == TargetDeltaDomain::RelativeEqDeltaDb);
        }

        beginTest("Fitted bands keep generic semantic provenance");
        {
            const auto plan = SemanticPlanner().plan("less harsh but more air", kSampleRate);
            expect(plan.valid);
            expect(!plan.fit.bands.empty());
            for (const auto& band : plan.fit.bands)
            {
                expect(!band.contributions.empty(), "planned band must keep target provenance");
                expect(!band.reason.empty(), "planned band must expose a reason");
            }
        }

        beginTest("Constraint-limited goals are reported instead of silently sacrificed");
        {
            const auto plan = SemanticPlanner().plan("warmer without mud", kSampleRate);
            expect(plan.valid);
            bool sawWarmth = false;
            for (const auto& outcome : plan.goalOutcomes)
            {
                if (outcome.dimension != SemanticDimension::Warmth)
                    continue;
                sawWarmth = true;
                expect(outcome.status == GoalOutcomeStatus::ConstraintLimited
                       || outcome.status == GoalOutcomeStatus::Partial
                       || outcome.status == GoalOutcomeStatus::Achieved);
                if (outcome.status == GoalOutcomeStatus::ConstraintLimited)
                    expect(!outcome.limitingSourceId.empty());
            }
            expect(sawWarmth);
        }

        beginTest("Identical phrase compiles deterministically");
        {
            const auto a = compiler.compile("brighter without harshness");
            const auto b = compiler.compile("brighter without harshness");
            expectEquals(static_cast<int>(a.goals.size()), static_cast<int>(b.goals.size()));
            expectEquals(static_cast<int>(a.constraints.size()), static_cast<int>(b.constraints.size()));
            if (!a.goals.empty() && !b.goals.empty())
                expectWithinAbsoluteError(a.goals.front().amount, b.goals.front().amount, 0.0f);
        }

        beginTest("Golden geometry: more air is an Air shelf near 11 kHz");
        {
            const auto plan = SemanticPlanner().plan("more air", kSampleRate, 1.0f);
            expect(plan.valid && !plan.fit.bands.empty());
            const auto* air = findGoal(plan.intent, SemanticDimension::Brightness);
            expect(air != nullptr && air->focus == SemanticSpectralFocus::Air,
                   "more air must keep Air focus, not generic brightness");
            const PlannedBand* hs11k = nullptr;
            bool sawGeneric6kShelf = false;
            for (const auto& band : plan.fit.bands)
            {
                if (band.type == FilterType::HighShelf && band.gainDb > 0.0f
                    && band.frequencyHz >= 10000.0f && band.frequencyHz <= 12500.0f)
                    hs11k = &band;
                if (band.type == FilterType::HighShelf && band.gainDb > 0.0f
                    && band.frequencyHz >= 5000.0f && band.frequencyHz <= 7000.0f)
                    sawGeneric6kShelf = true;
            }
            expect(hs11k != nullptr, "more air must PLAN an Air high-shelf near 11 kHz");
            expect(!sawGeneric6kShelf, "more air must not collapse to a generic 6 kHz brightness shelf");
        }

        beginTest("Golden geometry: less harsh is Smoothness near 3.4 kHz");
        {
            const auto plan = SemanticPlanner().plan("less harsh", kSampleRate, 1.0f);
            expect(plan.valid);
            const auto* smooth = findGoal(plan.intent, SemanticDimension::Smoothness);
            expect(smooth != nullptr && smooth->amount > 0.0f);
            expect(!interpretationHas(plan, "harsher"));
            const auto* cut = findBandNear(plan.fit, 3400.0f, 500.0f);
            expect(cut != nullptr && cut->gainDb < 0.0f,
                   "less harsh must cut around 3.4 kHz, not boost");
        }

        beginTest("Golden geometry: more body unconstrained peaks near 190 Hz");
        {
            const auto plan = SemanticPlanner().plan("more body", kSampleRate, 1.0f);
            expect(plan.valid);
            const auto* warmth = findGoal(plan.intent, SemanticDimension::Warmth);
            expect(warmth != nullptr && warmth->amount > 0.0f);
            const auto* body = findBandNear(plan.fit, 190.0f, 40.0f);
            expect(body != nullptr && body->gainDb > 0.0f,
                   "more body without a mud fence must stay near 190 Hz");
        }

        beginTest("Golden geometry: warmer without mud keeps 250-500 Hz fence and ~104 Hz peak");
        {
            const auto plan = SemanticPlanner().plan("warmer without mud", kSampleRate, 1.0f);
            expect(plan.valid);
            expect(hasMudFence(plan), "warmer without mud must keep the 250-500 Hz mud fence");
            const auto* peak = findBandNear(plan.fit, 104.0f, 20.0f);
            expect(peak != nullptr && peak->gainDb > 0.0f && peak->type == FilterType::Peak,
                   "constrained warmth must remain a ~104 Hz peak, not an unconstrained 190 Hz body");
        }

        beginTest("Mix-language V1: not/no/avoid/don't never invert to harsher or muddier");
        {
            struct Case { const char* phrase; SemanticDimension dimension; };
            const Case cases[] = {
                { "avoid harshness", SemanticDimension::Smoothness },
                { "not harsh", SemanticDimension::Smoothness },
                { "no mud", SemanticDimension::Clarity },
                { "don't make it harsh", SemanticDimension::Smoothness },
                { "dont make it harsh", SemanticDimension::Smoothness },
            };

            for (const auto& c : cases)
            {
                const auto intent = compiler.compile(c.phrase);
                const auto plan = SemanticPlanner().plan(c.phrase, kSampleRate, 1.0f);
                const auto* goal = findGoal(intent, c.dimension);
                expect(intent.hasRecognizedContent && intent.isValid() && !intent.contradictory,
                       juce::String(c.phrase) + " must compile");
                expect(goal != nullptr && goal->amount > 0.0f,
                       juce::String(c.phrase) + " must not invert polarity");
                expect(plan.valid && !plan.fit.bands.empty(),
                       juce::String(c.phrase) + " must PLAN");
                expect(!interpretationHas(plan, "harsher") && !interpretationHas(plan, "muddier"),
                       juce::String(c.phrase) + " must not interpret as harsher/muddier");
            }
        }

        beginTest("Mix-language V1: bright but not harsh is brighter + smoother");
        {
            const auto intent = compiler.compile("bright but not harsh");
            const auto plan = SemanticPlanner().plan("bright but not harsh", kSampleRate, 1.0f);
            const auto* bright = findGoal(intent, SemanticDimension::Brightness);
            const auto* smooth = findGoal(intent, SemanticDimension::Smoothness);
            const auto* avoidHarsh = findConstraint(intent, SemanticDimension::Smoothness);
            expect(bright != nullptr && bright->amount > 0.0f);
            expect((smooth != nullptr && smooth->amount > 0.0f) || avoidHarsh != nullptr,
                   "not harsh must add smoothness or avoid-harsh, never +harsh");
            expect(smooth == nullptr || smooth->amount > 0.0f);
            expect(plan.valid);
            expect(interpretationHas(plan, "brighter"));
            expect(!interpretationHas(plan, "harsher"));
            bool sawSmoothCut = false;
            for (const auto& band : plan.fit.bands)
                if (band.gainDb < 0.0f && band.frequencyHz >= 2500.0f && band.frequencyHz <= 4500.0f)
                    sawSmoothCut = true;
            expect(sawSmoothCut || avoidHarsh != nullptr,
                   "bright but not harsh must cut 3.4 kHz or fence harshness");
        }

        beginTest("Mix-language V1: Italian aliases fango/sibilanza/asprezza/morso");
        {
            const auto fango = compiler.compile("più caldo senza fango");
            const auto fangoPlan = SemanticPlanner().plan("più caldo senza fango", kSampleRate, 1.0f);
            expect(findGoal(fango, SemanticDimension::Warmth) != nullptr);
            expect(findConstraint(fango, SemanticDimension::Clarity) != nullptr,
                   "fango must alias mud so senza fango applies the mud fence");
            expect(fangoPlan.valid && hasMudFence(fangoPlan));

            const auto sib = compiler.compile("più aria senza sibilanza");
            const auto sibPlan = SemanticPlanner().plan("più aria senza sibilanza", kSampleRate, 1.0f);
            expect(findGoal(sib, SemanticDimension::Brightness) != nullptr);
            expect(findConstraint(sib, SemanticDimension::Smoothness) != nullptr,
                   "sibilanza must alias sibilance/sibilo");
            expect(sibPlan.valid);
            const auto* air = findGoal(sib, SemanticDimension::Brightness);
            expect(air != nullptr && air->focus == SemanticSpectralFocus::Air);

            const auto asp = compiler.compile("più presenza senza asprezza");
            expect(findGoal(asp, SemanticDimension::Presence) != nullptr);
            expect(findConstraint(asp, SemanticDimension::Smoothness) != nullptr,
                   "asprezza must alias harsh/aspro");

            const auto bite = compiler.compile("più morso senza aspro");
            const auto bitePlan = SemanticPlanner().plan("più morso senza aspro", kSampleRate, 1.0f);
            expect(findGoal(bite, SemanticDimension::Presence) != nullptr,
                   "morso must alias bite");
            expect(findConstraint(bite, SemanticDimension::Smoothness) != nullptr);
            expect(bitePlan.valid);
        }

        beginTest("Mix-language V1: less boxy in the mids stays boxy/clarity");
        {
            const auto intent = compiler.compile("less boxy in the mids");
            const auto plan = SemanticPlanner().plan("less boxy in the mids", kSampleRate, 1.0f);
            const auto* clarity = findGoal(intent, SemanticDimension::Clarity);
            const auto* presence = findGoal(intent, SemanticDimension::Presence);
            expect(clarity != nullptr && clarity->amount > 0.0f,
                   "less boxy is the primary move");
            expect(presence == nullptr || presence->amount >= 0.0f,
                   "in the mids must not apply less to mids as a presence scoop");
            expect(plan.valid);
            expect(!interpretationHas(plan, "less presence"));
        }

        beginTest("Mix-language V1: fatter without boom is warmth plus boom fence");
        {
            const auto intent = compiler.compile("fatter without boom");
            const auto plan = SemanticPlanner().plan("fatter without boom", kSampleRate, 1.0f);
            expect(findGoal(intent, SemanticDimension::Warmth) != nullptr,
                   "fatter is the comparative of fat/body, not an unknown");
            expect(findConstraint(intent, SemanticDimension::Tightness) != nullptr);
            expect(plan.valid);
        }

        beginTest("Mix-language V1: keep/Hz/cinematic fail closed, not guessed");
        {
            const auto keep = SemanticPlanner().plan("keep it warm", kSampleRate, 1.0f);
            expect(!keep.valid, "keep it warm with no other goal is a no-op, fail closed");
            expect(keep.intent.hasRecognizedContent);
            expect(findConstraint(keep.intent, SemanticDimension::Warmth) != nullptr);

            const auto hz = SemanticPlanner().plan("cut 300 without losing body", kSampleRate, 1.0f);
            expect(!hz.valid, "Hz numerals must not be parsed into a cut");
            bool invented300 = false;
            for (const auto& band : hz.fit.bands)
                if (std::abs(band.frequencyHz - 300.0f) <= 20.0f)
                    invented300 = true;
            expect(!invented300);

            const auto boost12k = SemanticPlanner().plan("boost 12k without sibilance", kSampleRate, 1.0f);
            expect(!boost12k.valid, "12k must not invent a brightness goal");

            const auto cinematic = SemanticPlanner().plan("make it cinematic", kSampleRate, 1.0f);
            expect(!cinematic.intent.hasRecognizedContent);
            expect(!cinematic.valid);

            const auto glue = SemanticPlanner().plan("glue the mix", kSampleRate, 1.0f);
            expect(!glue.intent.hasRecognizedContent);
            expect(!glue.valid);
        }

        beginTest("PlanTruth E5: packProposalPlanSummary is label-only and <=128");
        {
            const auto air = SemanticPlanner().plan("more air", kSampleRate, 1.0f);
            const auto airSummary = packProposalPlanSummary(air);
            expect(air.valid);
            expect(airSummary.find("brighter") != std::string::npos);
            expect(airSummary.find("air") != std::string::npos);
            expect(airSummary.find("1b") != std::string::npos);
            expect(airSummary.find("10861") == std::string::npos);
            expect(airSummary.find("+1.45") == std::string::npos);
            expect(airSummary.find("Hz") == std::string::npos);
            expect(airSummary.find("dB") == std::string::npos);
            expect(airSummary.size() <= 128);

            const auto mud = SemanticPlanner().plan("warmer without mud", kSampleRate, 1.0f);
            const auto mudSummary = packProposalPlanSummary(mud);
            expect(mudSummary.find("avoid") != std::string::npos || mudSummary.find("mud") != std::string::npos);
            expect(mudSummary.find("104") == std::string::npos);
            expect(mudSummary.find("Hz") == std::string::npos);
            bool sawLim = mudSummary.find("lim") != std::string::npos;
            bool sawConstraint = false;
            for (const auto& outcome : mud.goalOutcomes)
                if (outcome.status == GoalOutcomeStatus::ConstraintLimited)
                    sawConstraint = true;
            expect(!sawConstraint || sawLim, "constraint-limited warmth must mark lim");

            const auto harsh = SemanticPlanner().plan("bright but not harsh", kSampleRate, 1.0f);
            const auto harshSummary = packProposalPlanSummary(harsh);
            expect(harshSummary.find("harsher") == std::string::npos);
            expect(harshSummary.find("3421") == std::string::npos);

            SemanticPlan oversized;
            oversized.interpretation = std::string(200, 'w');
            oversized.fit.valid = true;
            const auto clipped = packProposalPlanSummary(oversized);
            expect(clipped.size() <= 128);
            expect(!clipped.empty());

            auto held = air;
            SemanticContextAdjustment adj;
            adj.dimension = SemanticDimension::Brightness;
            adj.scale = 0.44f;
            adj.contextualized = true;
            held.contextAdjustments = { adj };
            const auto heldSummary = packProposalPlanSummary(held);
            expect(heldSummary.find("src 44%") != std::string::npos);
            expect(heldSummary.find("10861") == std::string::npos);

            SemanticPlan italian;
            italian.interpretation.clear();
            for (int i = 0; i < 200; ++i)
                italian.interpretation += "à";
            const auto italianSummary = packProposalPlanSummary(italian);
            std::size_t italianPoints = 0;
            for (std::size_t i = 0; i < italianSummary.size(); )
            {
                const unsigned char c = static_cast<unsigned char>(italianSummary[i]);
                const std::size_t width = (c & 0x80) == 0 ? 1
                    : (c & 0xE0) == 0xC0 ? 2
                    : (c & 0xF0) == 0xE0 ? 3
                    : 4;
                if (i + width > italianSummary.size())
                    break;
                i += width;
                ++italianPoints;
            }
            expect(italianPoints <= 128);
            expect(italianSummary.find("Hz") == std::string::npos);
            expect(italianSummary.find("dB") == std::string::npos);
            expect(italianSummary.find("à") != std::string::npos);
        }
    }
};

static SemanticIntentCompilerTest gSemanticIntentCompilerTest;

#endif
