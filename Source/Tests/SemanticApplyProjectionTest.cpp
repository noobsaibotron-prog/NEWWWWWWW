#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "Support/TestParameters.h"

#include <cmath>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;
using Adjustment = SemanticEQEngine::SemanticEQAdjustment;
using Quality = SemanticEQEngine::SemanticQuality;
using Policy = Processor::SemanticApplyPolicy;

Adjustment makeAdj (Quality quality,
                    float frequency,
                    float gain,
                    float q = 1.2f,
                    int type = 2,
                    bool enabled = true)
{
    Adjustment adj;
    adj.sourceQuality = quality;
    adj.frequency = frequency;
    adj.gain = gain;
    adj.q = q;
    adj.filterType = type;
    adj.enabled = enabled;
    adj.confidence = 1.0f;
    return adj;
}

void occupyEverySlot (Processor& proc)
{
    for (int i = 0; i < Processor::maxBands; ++i)
    {
        auto band = proc.getBandState (i);
        band.enabled = true;
        band.gain = (i % 2 == 0) ? 4.5f : -4.5f;
        band.frequency = 80.0f * static_cast<float> (i + 1);
        proc.setBandState (i, band);
    }
}

/** APPLY writes the APVTS choice; the audio-thread atomic is refreshed later
    by updateEQFromParameters(). Projection/commit tests must read APVTS. */
int apvtsActiveBandCount (Processor& proc)
{
    auto* raw = proc.getAPVTS().getRawParameterValue ("numActiveBands");
    if (raw == nullptr)
        return 0;
    const int idx = static_cast<int> (std::round (raw->load()));
    return idx + 1;
}

void restoreFactoryFreeHighSlots (juce::UnitTest& test, Processor& proc)
{
    for (int i = 0; i < Processor::maxBands; ++i)
        aieq::test::setBool (test, proc.getAPVTS(),
                             "band" + juce::String (i) + "Enabled", i < 8);
}

void pumpAudio (Processor& proc)
{
    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);
}
} // namespace

class SemanticApplyProjectionTest final : public juce::UnitTest
{
public:
    SemanticApplyProjectionTest()
        : juce::UnitTest ("Semantic apply projection", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest ("1. single adjustment on a free slot");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Air, 12000.0f, 1.8f)
            };
            expectProjectionThenCommit (proc, adj, Policy::RequireCompletePlan, true);
            expectEquals (apvtsActiveBandCount (proc), Processor::maxBands);
        }

        beginTest ("2. multiple adjustments of the same quality/ordinal sequence");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Air, 11000.0f, 1.2f, 1.1f),
                makeAdj (Quality::Air, 14000.0f, 0.8f, 0.9f)
            };
            const auto committed = expectProjectionThenCommit (
                proc, adj, Policy::RequireCompletePlan, true);
            expectEquals (committed.appliedBands, 2);
            expectEquals (static_cast<int> (committed.appliedBandSlots.size()), 2);
            expect (committed.appliedBandSlots[0] != committed.appliedBandSlots[1],
                    "same-quality ordinals must occupy distinct slots");
        }

        beginTest ("3. plan that grows the committed active-band count");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const int beforeCount = apvtsActiveBandCount (proc);
            expect (beforeCount < Processor::maxBands);
            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Presence, 4000.0f, 1.5f)
            };
            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            expect (projection.writeGrownActiveBandCount);
            expectEquals (projection.grownActiveBandCount, Processor::maxBands);
            const auto committed = expectProjectionThenCommit (
                proc, adj, Policy::RequireCompletePlan, true, &projection);
            expectEquals (committed.resultingActiveBandCount, Processor::maxBands);
            expectEquals (apvtsActiveBandCount (proc), Processor::maxBands);
        }

        beginTest ("4. full capacity atomically rejects RequireCompletePlan");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            occupyEverySlot (proc);
            const auto beforeHistory = proc.getUndoStackSize();
            std::array<Processor::BandState, Processor::maxBands> before {};
            for (int i = 0; i < Processor::maxBands; ++i)
                before[static_cast<std::size_t> (i)] = proc.getBandState (i);

            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Warmth, 180.0f, 1.0f)
            };
            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            expect (projection.atomicRejected);
            expectEquals (projection.appliedBands, 0);
            expect (! projection.needsHistorySnapshot);

            const auto result = proc.applySemanticAdjustments (
                adj, Policy::RequireCompletePlan);
            expect (result.atomicRejected);
            expectEquals (result.appliedBands, 0);
            expectEquals (proc.getUndoStackSize(), beforeHistory);
            for (int i = 0; i < Processor::maxBands; ++i)
            {
                expect (Processor::bandStatesEquivalent (
                            proc.getBandState (i),
                            before[static_cast<std::size_t> (i)]),
                        "atomic reject mutated band " + juce::String (i));
            }
        }

        beginTest ("5. existing Semantic slot is reused");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Body, 250.0f, 1.4f, 0.8f)
            };
            expectProjectionThenCommit (proc, adj, Policy::RequireCompletePlan, true);
            const auto firstSlots = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan).appliedBandSlots;
            expectEquals (static_cast<int> (firstSlots.size()), 1);

            const auto second = expectProjectionThenCommit (
                proc, adj, Policy::RequireCompletePlan, true);
            expectEquals (static_cast<int> (second.appliedBandSlots.size()), 1);
            expectEquals (second.appliedBandSlots[0], firstSlots[0]);
            expect (! second.bandNeedsWrite[static_cast<std::size_t> (firstSlots[0])],
                    "identical re-apply of an owned slot must not rewrite APVTS");
        }

        beginTest ("6. obsolete Semantic slot restores the original snapshot");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            std::array<Processor::BandState, Processor::maxBands> original {};
            for (int i = 0; i < Processor::maxBands; ++i)
                original[static_cast<std::size_t> (i)] = proc.getBandState (i);

            const auto both = std::vector<Adjustment> {
                makeAdj (Quality::Air, 12500.0f, 2.0f),
                makeAdj (Quality::Warmth, 180.0f, 1.1f)
            };
            const auto first = expectProjectionThenCommit (
                proc, both, Policy::RequireCompletePlan, true);
            expectEquals (static_cast<int> (first.appliedBandSlots.size()), 2);
            const int airSlot = first.appliedBandSlots[0];
            const int warmthSlot = first.appliedBandSlots[1];
            expect (airSlot != warmthSlot);

            const auto warmthOnly = std::vector<Adjustment> {
                makeAdj (Quality::Warmth, 180.0f, 1.1f)
            };
            expectProjectionThenCommit (proc, warmthOnly, Policy::RequireCompletePlan, true);
            expectEquals (proc.buildSemanticApplyProjection (
                              warmthOnly, Policy::RequireCompletePlan).appliedBandSlots.front(),
                          warmthSlot);
            expect (Processor::bandStatesEquivalent (
                        proc.getBandState (airSlot),
                        original[static_cast<std::size_t> (airSlot)]),
                    "obsolete Air slot was not restored to the pre-Semantic snapshot");
        }

        beginTest ("7. manual takeover is preserved");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto air = std::vector<Adjustment> {
                makeAdj (Quality::Air, 12000.0f, 1.5f)
            };
            const auto first = expectProjectionThenCommit (
                proc, air, Policy::RequireCompletePlan, true);
            const int ownedSlot = first.appliedBandSlots.front();

            auto taken = proc.getBandState (ownedSlot);
            taken.gain = 6.5f;
            proc.setBandState (ownedSlot, taken);
            const auto afterTakeover = proc.getBandState (ownedSlot);

            const auto warmth = std::vector<Adjustment> {
                makeAdj (Quality::Warmth, 200.0f, 0.9f)
            };
            expectProjectionThenCommit (proc, warmth, Policy::RequireCompletePlan, true);
            expect (Processor::bandStatesEquivalent (
                        proc.getBandState (ownedSlot), afterTakeover),
                    "manual takeover was overwritten by a later Semantic plan");
        }

        beginTest ("8. empty/reset plan restores active-band count");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            pumpAudio (proc);
            const int originalCount = proc.getNumActiveBands();
            const auto air = std::vector<Adjustment> {
                makeAdj (Quality::Air, 13000.0f, 1.0f)
            };
            expectProjectionThenCommit (proc, air, Policy::RequireCompletePlan, true);
            expectEquals (apvtsActiveBandCount (proc), Processor::maxBands);
            // APPLY writes APVTS; the restore predicate reads the audio-thread
            // atomic, which updateEQFromParameters refreshes on the next block.
            pumpAudio (proc);

            const std::vector<Adjustment> empty;
            const auto resetProj = proc.buildSemanticApplyProjection (
                empty, Policy::RequireCompletePlan);
            expect (resetProj.writeRestoredActiveBandCount);
            expectEquals (resetProj.restoredActiveBandCount, originalCount);
            expectProjectionThenCommit (
                proc, empty, Policy::RequireCompletePlan, true, &resetProj);
            expectEquals (apvtsActiveBandCount (proc), originalCount);
        }

        beginTest ("9. previously Dynamic band becomes Semantic static");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            aieq::test::setChoice (*this, proc.getAPVTS(), "band23DynMode", 1);
            auto dynBand = proc.getBandState (23);
            expectEquals (dynBand.dynMode, 1);
            expect (! dynBand.enabled);

            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Clarity, 2500.0f, -1.2f, 1.4f)
            };
            const auto committed = expectProjectionThenCommit (
                proc, adj, Policy::RequireCompletePlan, true);
            expectEquals (static_cast<int> (committed.appliedBandSlots.size()), 1);
            expectEquals (committed.appliedBandSlots.front(), 23);
            const auto after = proc.getBandState (23);
            expectEquals (after.dynMode, 0);
            expect (after.enabled);
            expectEquals (after.dynTrigger, DynamicEQProcessor::TriggerSide_Above);
        }

        beginTest ("10. invalid quality stays unresolved and rejects complete plans");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            auto invalid = makeAdj (Quality::Air, 1000.0f, 0.5f);
            invalid.sourceQuality = static_cast<Quality> (-1);
            const auto adj = std::vector<Adjustment> { invalid };

            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            expect (projection.atomicRejected);
            expectEquals (projection.resolvableBands, 0);
            expectEquals (projection.appliedBands, 0);

            const auto beforeHistory = proc.getUndoStackSize();
            const auto result = proc.applySemanticAdjustments (
                adj, Policy::RequireCompletePlan);
            expect (result.atomicRejected);
            expectEquals (proc.getUndoStackSize(), beforeHistory);

            const auto bestEffort = proc.buildSemanticApplyProjection (
                adj, Policy::BestEffortLegacy);
            expect (! bestEffort.atomicRejected);
            expectEquals (bestEffort.appliedBands, 0);
            expectEquals (bestEffort.rejectedBands, 1);
        }
    }

private:
    Processor::SemanticApplyProjection expectProjectionThenCommit (
        Processor& proc,
        const std::vector<Adjustment>& adjustments,
        Policy policy,
        bool expectComplete,
        const Processor::SemanticApplyProjection* prebuilt = nullptr)
    {
        const auto projection = prebuilt != nullptr
            ? *prebuilt
            : proc.buildSemanticApplyProjection (adjustments, policy);

        std::array<Processor::BandState, Processor::maxBands> before {};
        for (int i = 0; i < Processor::maxBands; ++i)
            before[static_cast<std::size_t> (i)] = proc.getBandState (i);

        const auto result = proc.applySemanticAdjustments (adjustments, policy);
        expectEquals (result.requestedBands, projection.requestedBands);
        expectEquals (result.appliedBands, projection.appliedBands);
        expectEquals (result.rejectedBands, projection.rejectedBands);
        expectEquals (static_cast<int> (result.atomicRejected),
                      static_cast<int> (projection.atomicRejected));
        expectEquals (static_cast<int> (result.appliedBandSlots.size()),
                      static_cast<int> (projection.appliedBandSlots.size()));
        for (std::size_t i = 0; i < result.appliedBandSlots.size(); ++i)
            expectEquals (result.appliedBandSlots[i], projection.appliedBandSlots[i]);

        if (expectComplete)
            expect (result.complete() && ! projection.atomicRejected);

        for (int i = 0; i < Processor::maxBands; ++i)
        {
            const auto committed = proc.getBandState (i);
            expect (Processor::bandStatesEquivalent (
                        committed, projection.resultingBands[static_cast<std::size_t> (i)]),
                    "committed BandState diverged from projection at slot "
                        + juce::String (i));
            if (! projection.bandNeedsWrite[static_cast<std::size_t> (i)])
            {
                expect (Processor::bandStatesEquivalent (
                            committed, before[static_cast<std::size_t> (i)]),
                        "slot marked read-only was rewritten: " + juce::String (i));
            }
        }

        expectEquals (apvtsActiveBandCount (proc), projection.resultingActiveBandCount);
        return projection;
    }
};

static SemanticApplyProjectionTest semanticApplyProjectionTest;
