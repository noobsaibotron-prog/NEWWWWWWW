/*
 * T3.2 — Semantic integration harness.
 *
 * These are the invariants that the words "atomic", "one transaction" and
 * "stale plan" are supposed to name. Until they are witnessed against the real
 * processor and the real APVTS, those words are claims about intent rather than
 * about behaviour — and this project has already shipped four separate cases of
 * a test that looked like coverage and exercised nothing.
 *
 * Scope note, stated up front: A-E drive the real AIEqualizerAudioProcessor and
 * its APVTS. F and G drive SemanticPlanningService directly, because generation
 * invalidation and worker teardown are properties of the service and are far
 * more sharply testable without a GUI in the way. No pixels are involved
 * anywhere: the contract lives in the processor and the service, not in the
 * panel's paint().
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../AI/SemanticPlanner.h"
#include "../AI/SemanticPlanningService.h"

#include <string>
#include <vector>

namespace
{

/** A hash over everything a Semantic apply is allowed to touch, plus enough of
    the rest of the band to catch collateral damage. Compared as a string so a
    failure message can show what changed rather than two opaque integers. */
juce::String bandStateDigest(const AIEqualizerAudioProcessor& proc)
{
    juce::String d;
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        const auto b = proc.getBandState(i);
        // Frequency at 0.01 Hz. The B0 control shows a bare pushUndoState/undo
        // cycle already moves band 9 by 1e-4 Hz at 1 kHz with no Semantic
        // involved: that is the APVTS normalise/denormalise round-trip, not the
        // transaction under test. Comparing float bit patterns would assert on
        // that artefact forever. 0.01 Hz is still ~5 orders of magnitude finer
        // than anything audible or displayed, so nothing real can hide under it.
        d << i << ":"
          << juce::String(b.frequency, 2) << ","
          << juce::String(b.gain, 4) << ","
          << juce::String(b.q, 4) << ","
          << b.type << ","
          << (b.enabled ? 1 : 0) << ","
          << (b.solo ? 1 : 0) << ","
          << b.slope << ","
          << b.dynMode << ";";
    }
    return d;
}

/** Plan synchronously for the harness. This is the same call the worker makes;
    the async path is exercised separately in F. */
AIEQPerceptual::SemanticPlan planFor(const std::string& text, double sr, float intensity = 1.0f)
{
    return AIEQPerceptual::SemanticPlanner().plan(text, sr, intensity);
}

/** First differing band between two processors, or -1. A harness that only says
    "not equal" makes the reader guess; this makes the failure legible. */
juce::String firstBandDifference(const juce::String& a, const juce::String& b)
{
    auto split = [](const juce::String& d)
    {
        juce::StringArray parts;
        parts.addTokens(d, ";", "");
        return parts;
    };
    const auto pa = split(a), pb = split(b);
    for (int i = 0; i < juce::jmin(pa.size(), pb.size()); ++i)
        if (pa[i] != pb[i])
            return "band " + juce::String(i) + ":  before[" + pa[i] + "]  after[" + pb[i] + "]";
    return "(digests differ in length only)";
}

} // namespace

class SemanticIntegrationHarnessTest : public juce::UnitTest
{
public:
    SemanticIntegrationHarnessTest()
        : juce::UnitTest("Semantic Integration Harness (T3.2)", "Integration") {}

    void runTest() override
    {
        constexpr double kSr = 48000.0;
        const std::vector<float> flatSpectrum(2049, -60.0f);
        constexpr int kBlock = 512;
        const std::string kCommand = "warmer without mud";

        //======================================================================
        beginTest("A. PLAN is mutation-free");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            const auto before = bandStateDigest(proc);
            const int  histBefore = proc.getUndoStackSize();

            AIEQPerceptual::SemanticPlanningService svc;
            svc.start();
            svc.submit(kCommand, 1.0f, kSr);
            expect(svc.waitUntilQuiescent(8000), "planner did not settle in 8 s");
            const auto result = svc.takeCurrentResult();
            svc.stop();

            expect(result.has_value(), "no planning result produced");
            if (result.has_value())
                logMessage("  planned status=" + juce::String(
                    AIEQPerceptual::semanticPlanningStatusName(result->status))
                    + " bands=" + juce::String((int) result->plan.fit.bands.size()));

            const auto after = bandStateDigest(proc);
            expect(after == before,
                   "PLAN mutated band state. Planning must be side-effect free; "
                   "the worker has no processor access by construction, so a diff "
                   "here means something else on this path is writing.");
            expect(proc.getUndoStackSize() == histBefore,
                   "PLAN pushed an undo state; only APPLY may do that.");
        }

        //======================================================================
        beginTest("B0. CONTROL: does undo round-trip the state at all, with no Semantic involved?");
        {
            // Attribution control. If a bare pushUndoState/undo cycle already
            // perturbs the digest, then any difference seen in B belongs to the
            // APVTS/History float round-trip and NOT to the Semantic transaction.
            // Without this control the two are indistinguishable and it would be
            // easy to report an APVTS artefact as a Semantic defect.
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            const auto before = bandStateDigest(proc);
            proc.pushUndoState("harness control");
            auto b = proc.getBandState(0);
            b.gain = 3.0f;
            proc.setBandState(0, b);
            proc.undo();
            const auto after = bandStateDigest(proc);

            if (after != before)
                logMessage("  CONTROL round-trip diff -> " + firstBandDifference(before, after));
            else
                logMessage("  CONTROL: bare undo is digest-exact");
        }

        beginTest("B. APPLY is one transaction and UNDO restores exactly");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            const auto plan = planFor(kCommand, kSr);
            const auto adjustments = proc.getSemanticEngine().adjustmentsFromPlan(plan);
            expect(!adjustments.empty(), "harness precondition: plan produced no adjustments");

            const auto before = bandStateDigest(proc);
            const int  histBefore = proc.getUndoStackSize();

            const auto r = proc.applySemanticAdjustments(
                adjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);

            logMessage("  requested=" + juce::String(r.requestedBands)
                       + " applied=" + juce::String(r.appliedBands)
                       + " rejected=" + juce::String(r.rejectedBands)
                       + " atomicRejected=" + juce::String(r.atomicRejected ? 1 : 0));

            expect(r.complete(), "apply did not complete on an empty EQ");
            const auto applied = bandStateDigest(proc);
            expect(applied != before, "apply reported success but changed nothing");

            expect(proc.getUndoStackSize() == histBefore + 1,
                   "APPLY must push exactly one undo state, got "
                   + juce::String(proc.getUndoStackSize() - histBefore));

            proc.undo();
            const auto afterUndo = bandStateDigest(proc);
            if (afterUndo != before)
                logMessage("  UNDO diff -> " + firstBandDifference(before, afterUndo));
            expect(afterUndo == before,
                   "one UNDO did not restore the exact pre-apply state");
        }

        //======================================================================
        beginTest("C. Atomic rejection leaves zero mutations");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            // Occupy every slot with an ENABLED, user-owned band. Disabling bands
            // would have made them reclaimable, which is the opposite of the
            // condition under test.
            for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
            {
                auto b = proc.getBandState(i);
                b.enabled = true;
                b.gain = (i % 2 == 0) ? 4.5f : -4.5f;  // clearly manual, non-zero
                b.frequency = 80.0f * static_cast<float>(i + 1);
                proc.setBandState(i, b);
            }

            const auto plan = planFor(kCommand, kSr);
            const auto adjustments = proc.getSemanticEngine().adjustmentsFromPlan(plan);
            expect(!adjustments.empty(), "harness precondition: no adjustments to reject");

            const auto before = bandStateDigest(proc);
            const int  histBefore = proc.getUndoStackSize();

            const auto r = proc.applySemanticAdjustments(
                adjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);

            logMessage("  requested=" + juce::String(r.requestedBands)
                       + " applied=" + juce::String(r.appliedBands)
                       + " atomicRejected=" + juce::String(r.atomicRejected ? 1 : 0));

            expect(r.atomicRejected,
                   "every slot was manual and enabled, yet apply did not reject");
            expect(bandStateDigest(proc) == before,
                   "ATOMIC VIOLATION: rejected apply still mutated band state");
            expect(proc.getUndoStackSize() == histBefore,
                   "rejected apply pushed an undo state (history depth changed)");
        }

        //======================================================================
        beginTest("D. Reset reconciles back to the pre-Semantic state");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            const auto preSemantic = bandStateDigest(proc);

            const auto plan = planFor(kCommand, kSr);
            const auto adjustments = proc.getSemanticEngine().adjustmentsFromPlan(plan);
            const auto r = proc.applySemanticAdjustments(
                adjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);
            expect(r.complete(), "harness precondition: apply failed");
            expect(bandStateDigest(proc) != preSemantic, "apply changed nothing");

            // Reset = empty semantic state reconciled through the same endpoint.
            proc.getSemanticEngine().resetState();
            const auto resetAdjustments = proc.getSemanticEngine().generateEQFromState(flatSpectrum, kSr);
            const auto rr = proc.applySemanticAdjustments(
                resetAdjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);
            juce::ignoreUnused(rr);

            const auto afterReset = bandStateDigest(proc);
            expect(afterReset == preSemantic,
                   "Reset did not reconcile Semantic-owned bands back to their "
                   "pre-Semantic state. Stale Semantic bands left in the EQ is the "
                   "documented failure mode this asserts against.");
        }

        //======================================================================
        beginTest("E. Manual takeover survives a later Semantic reset");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            const auto plan = planFor(kCommand, kSr);
            const auto adjustments = proc.getSemanticEngine().adjustmentsFromPlan(plan);
            const auto r = proc.applySemanticAdjustments(
                adjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);
            expect(r.complete(), "harness precondition: apply failed");

            // Find a band the apply actually touched, then edit it as a user would.
            int touched = -1;
            for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
                if (std::abs(proc.getBandState(i).gain) > 0.01f) { touched = i; break; }

            expect(touched >= 0, "harness precondition: no band was modified by apply");
            if (touched < 0)
                return;

            auto edited = proc.getBandState(touched);
            // The gain parameter is NormalisableRange(-24, 24, 0.1): 9.25 is not
            // representable and would quantise to 9.3, which is a harness artefact,
            // not a product defect. Use a value on the step grid.
            edited.gain = 9.3f;
            edited.frequency = 777.0f;
            proc.setBandState(touched, edited);

            proc.getSemanticEngine().resetState();
            const auto resetAdjustments = proc.getSemanticEngine().generateEQFromState(flatSpectrum, kSr);
            const auto rr = proc.applySemanticAdjustments(
                resetAdjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan);
            juce::ignoreUnused(rr);

            const auto after = proc.getBandState(touched);
            logMessage("  user band " + juce::String(touched)
                       + " after reset: gain=" + juce::String(after.gain, 2)
                       + " freq=" + juce::String(after.frequency, 1));
            expect(std::abs(after.gain - 9.3f) < 0.05f
                       && std::abs(after.frequency - 777.0f) < 0.5f,
                   "Semantic reset overwrote a band the user had taken over. "
                   "Manual ownership must outrank Semantic.");
        }

        //======================================================================
        beginTest("F. A superseded plan can never become APPLY-able");
        {
            AIEQPerceptual::SemanticPlanningService svc;
            svc.start();

            const auto genOld = svc.submit("warmer without mud", 1.0f, kSr);
            const auto genNew = svc.submit("brighter without harshness", 1.0f, kSr);
            expect(genNew > genOld, "generation did not advance on resubmit");

            expect(svc.waitUntilQuiescent(8000), "planner did not settle");
            const auto result = svc.takeCurrentResult();

            expect(result.has_value(), "no result at all after two submits");
            if (result.has_value())
            {
                logMessage("  old=" + juce::String((int) genOld)
                           + " new=" + juce::String((int) genNew)
                           + " delivered=" + juce::String((int) result->generation));
                expect(result->generation == genNew,
                       "a superseded generation was published as current");
            }

            // An explicit invalidation must also make an in-flight answer unusable.
            svc.submit("more air", 1.0f, kSr);
            svc.invalidate();
            expect(svc.waitUntilQuiescent(8000), "planner did not settle after invalidate");
            expect(!svc.takeCurrentResult().has_value(),
                   "a result survived invalidate() and remained APPLY-able");

            svc.stop();
        }

        //======================================================================
        beginTest("G. Teardown during planning joins cleanly");
        {
            // Destroy the service while a fit is in flight. A timeout-based
            // teardown would race here; stop() is required to join.
            {
                AIEQPerceptual::SemanticPlanningService svc;
                svc.start();
                svc.submit("warmer without mud and more air", 1.0f, kSr);
                // no wait: destructor runs while the worker is busy
            }
            expect(true, "service destroyed during in-flight planning without hanging");

            // Repeated start/stop must be idempotent and must not leak a thread.
            AIEQPerceptual::SemanticPlanningService svc;
            svc.start();
            svc.start();
            svc.stop();
            svc.stop();
            expect(!svc.isPlanning(), "service still reports planning after stop");
        }
    }
};

static SemanticIntegrationHarnessTest sSemanticIntegrationHarnessTest;
