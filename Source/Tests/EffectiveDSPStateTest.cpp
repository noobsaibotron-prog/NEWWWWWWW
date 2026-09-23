#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../DSP/EffectiveDSPState.h"
#include "Support/TestParameters.h"

#include <array>
#include <cstring>
#include <cmath>
#include <memory>
#include <type_traits>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;
using Adjustment = SemanticEQEngine::SemanticEQAdjustment;
using Quality = SemanticEQEngine::SemanticQuality;
using Policy = Processor::SemanticApplyPolicy;

static_assert(std::is_trivially_copyable_v<EmberDSP::PackedBandDSPState>);
static_assert(std::is_trivially_copyable_v<EmberDSP::EffectiveDSPState>);
static_assert(Processor::maxBands == EmberDSP::kEffectiveDSPBandCount);

Adjustment makeAdj (Quality quality, float frequency, float gain)
{
    Adjustment adj;
    adj.sourceQuality = quality;
    adj.frequency = frequency;
    adj.gain = gain;
    adj.q = 1.2f;
    adj.filterType = 2;
    adj.enabled = true;
    adj.confidence = 1.0f;
    return adj;
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

juce::String committedDigest (Processor& proc)
{
    juce::String d;
    for (int i = 0; i < Processor::maxBands; ++i)
    {
        const auto b = proc.getBandState (i);
        d << i << ":" << b.frequency << "," << b.gain << "," << b.q << ","
          << b.type << "," << (int) b.enabled << "," << (int) b.solo << ","
          << b.slope << "," << b.curveMode << "," << b.dynMode << ";";
    }
    if (auto* raw = proc.getAPVTS().getRawParameterValue ("numActiveBands"))
        d << "n=" << raw->load();
    if (auto* dyn = proc.getAPVTS().getRawParameterValue ("dynEqEnabled"))
        d << "d=" << dyn->load();
    return d;
}

bool packedBandsClose (const EmberDSP::PackedBandDSPState& a,
                       const EmberDSP::PackedBandDSPState& b)
{
    return std::abs (a.frequency - b.frequency) <= 1.0f
        && std::abs (a.gain - b.gain) <= 0.05f
        && std::abs (a.q - b.q) <= 0.02f
        && std::abs (a.sidechainFrequency - b.sidechainFrequency) <= 1.0f
        && std::abs (a.sidechainQ - b.sidechainQ) <= 0.02f
        && std::abs (a.dynThreshold - b.dynThreshold) <= 0.05f
        && std::abs (a.dynRatio - b.dynRatio) <= 0.02f
        && std::abs (a.dynAttack - b.dynAttack) <= 0.05f
        && std::abs (a.dynRelease - b.dynRelease) <= 0.05f
        && std::abs (a.dynRange - b.dynRange) <= 0.05f
        && std::abs (a.dynKnee - b.dynKnee) <= 0.05f
        && a.type == b.type
        && a.slope == b.slope
        && a.curveMode == b.curveMode
        && a.dynMode == b.dynMode
        && a.dynTrigger == b.dynTrigger
        && a.detectionMode == b.detectionMode
        && a.detectorSource == b.detectorSource
        && a.enabled == b.enabled
        && a.solo == b.solo
        && a.bandOwnedByDynamicStage == b.bandOwnedByDynamicStage;
}

bool dspPayloadClose (const EmberDSP::EffectiveDSPState& a,
                      const EmberDSP::EffectiveDSPState& b)
{
    if (a.effectiveActiveBandCount != b.effectiveActiveBandCount)
        return false;
    if (a.dynEqEnabled != b.dynEqEnabled)
        return false;
    for (int i = 0; i < EmberDSP::kEffectiveDSPBandCount; ++i)
        if (! packedBandsClose (a.bands[static_cast<std::size_t> (i)],
                                b.bands[static_cast<std::size_t> (i)]))
            return false;
    return true;
}
} // namespace

class EffectiveDSPStateTest final : public juce::UnitTest
{
public:
    EffectiveDSPStateTest()
        : juce::UnitTest ("Effective DSP state", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;
        // Each Processor is ~800 KB. Debug builds give every block-scoped local its
        // own stack slot, so ten of them in this one frame overflow the 8 MB
        // main-thread stack. Keep them on the heap.

        beginTest ("1. payload is trivially copyable");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            pumpAudio (proc);
            const auto a = proc.snapshotCommittedEffectiveDSPState();
            EmberDSP::EffectiveDSPState b;
            std::memcpy (&b, &a, sizeof (a));
            expectEquals (b.effectiveActiveBandCount, a.effectiveActiveBandCount);
            expectEquals ((int) b.source, (int) a.source);
            expectEquals ((int) b.dynEqEnabled, (int) a.dynEqEnabled);
            expect (b.projectionBaseEpoch == a.projectionBaseEpoch);
            expect (std::memcmp (&b.bands, &a.bands, sizeof (a.bands)) == 0);
            proc.releaseResources();
        }

        beginTest ("2. selector A keeps effective count equal to committed");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            pumpAudio (proc);
            expect (proc.getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            expectEquals (proc.getEffectiveActiveBandCount(),
                          proc.getNumActiveBands());
            const auto committed = proc.snapshotCommittedEffectiveDSPState();
            expectEquals (committed.effectiveActiveBandCount,
                          proc.getNumActiveBands());
            expectEquals ((int) committed.source,
                          (int) EmberDSP::EffectiveDSPSource::CommittedA);

            aieq::test::setChoice (*this, proc.getAPVTS(), "numActiveBands", 15);
            pumpAudio (proc);
            expectEquals (proc.getEffectiveActiveBandCount(),
                          proc.getNumActiveBands());
            expectEquals (proc.getNumActiveBands(), 16);
            expect (proc.getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            proc.releaseResources();
        }

        beginTest ("3. committed snapshot matches getBandState after a block");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            aieq::test::setFloat (*this, proc.getAPVTS(), "band0Gain", 3.5f);
            aieq::test::setFloat (*this, proc.getAPVTS(), "band1Freq", 250.0f);
            pumpAudio (proc);

            const auto snap = proc.snapshotCommittedEffectiveDSPState();
            expectEquals (snap.effectiveActiveBandCount, proc.getNumActiveBands());
            for (int i = 0; i < Processor::maxBands; ++i)
            {
                const auto band = proc.getBandState (i);
                const auto& packed = snap.bands[static_cast<std::size_t> (i)];
                expect (std::abs (packed.frequency - band.frequency) <= 1.0f,
                        "freq mismatch on band " + juce::String (i));
                expect (std::abs (packed.gain - band.gain) <= 0.05f,
                        "gain mismatch on band " + juce::String (i));
                expect (std::abs (packed.q - band.q) <= 0.02f,
                        "Q mismatch on band " + juce::String (i));
                expectEquals (packed.type, band.type);
                expectEquals (packed.dynMode, band.dynMode);
            }
            const auto band0 = proc.getBandState (0);
            expect (std::abs (snap.bands[0].gain - 3.5f) <= 0.05f);
            expect (std::abs (band0.gain - 3.5f) <= 0.05f);
            proc.releaseResources();
        }

        beginTest ("4. projected snapshot fills B without mutating APVTS");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            pumpAudio (proc);

            const auto before = committedDigest (proc);
            const auto committedBefore = proc.snapshotCommittedEffectiveDSPState();
            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Air, 12000.0f, 1.8f)
            };
            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            expect (! projection.atomicRejected);
            expectEquals (static_cast<int> (projection.appliedBandSlots.size()), 1);

            const auto snapshot = proc.snapshotProjectedEffectiveDSPState (projection);
            expect (snapshot.publishable, "fresh projection must be publishable");
            expect (! snapshot.epochStale);
            const auto& projected = snapshot.state;
            expectEquals ((int) projected.source,
                          (int) EmberDSP::EffectiveDSPSource::ProjectedB);
            expectEquals (projected.effectiveActiveBandCount,
                          projection.resultingActiveBandCount);
            expect (projected.projectionBaseEpoch == projection.projectionBaseEpoch);
            expect (projected.previewGeneration == projection.previewGeneration);
            expectEquals (committedDigest (proc), before,
                          "snapshotProjectedEffectiveDSPState mutated committed state");

            const int slot = projection.appliedBandSlots[0];
            expect (std::abs (projected.bands[static_cast<std::size_t> (slot)].frequency
                              - 12000.0f) <= 1.0f);
            expect (std::abs (committedBefore.bands[static_cast<std::size_t> (slot)].frequency
                              - 12000.0f) > 1.0f);
            expect (proc.getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            proc.releaseResources();
        }

        beginTest ("5. post-apply committed payload matches pre-apply projected B");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            pumpAudio (proc);

            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Presence, 4000.0f, 1.5f)
            };
            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            const auto snapshot = proc.snapshotProjectedEffectiveDSPState (projection);
            expect (snapshot.publishable);
            const auto& projected = snapshot.state;
            const auto result = proc.applySemanticAdjustments (
                adj, Policy::RequireCompletePlan);
            expect (! result.atomicRejected);
            pumpAudio (proc);

            const auto committed = proc.snapshotCommittedEffectiveDSPState();
            expectEquals ((int) committed.source,
                          (int) EmberDSP::EffectiveDSPSource::CommittedA);
            expect (dspPayloadClose (committed, projected),
                    "Preview-B DSP payload must equal post-apply committed payload");
            expectEquals (proc.getEffectiveActiveBandCount(),
                          proc.getNumActiveBands());
            proc.releaseResources();
        }

        beginTest ("6. epochs: band params bump projection base, mix params bump context");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            pumpAudio (proc);

            const auto base0 = proc.getProjectionBaseEpoch();
            const auto ctx0 = proc.getAuditionContextEpoch();

            aieq::test::setFloat (*this, proc.getAPVTS(), "band0Gain", -2.0f);
            const auto base1 = proc.getProjectionBaseEpoch();
            const auto ctx1 = proc.getAuditionContextEpoch();
            expect (base1 > base0, "band gain must bump projectionBaseEpoch");
            expectEquals ((juce::int64) ctx1, (juce::int64) ctx0,
                          "band gain must not bump auditionContextEpoch");

            aieq::test::setChoice (*this, proc.getAPVTS(), "phaseMode", 1);
            const auto base2 = proc.getProjectionBaseEpoch();
            const auto ctx2 = proc.getAuditionContextEpoch();
            expectEquals ((juce::int64) base2, (juce::int64) base1,
                          "phaseMode must not bump projectionBaseEpoch");
            expect (ctx2 > ctx1, "phaseMode must bump auditionContextEpoch");

            aieq::test::setFloat (*this, proc.getAPVTS(), "dryWet", 50.0f);
            expect (proc.getAuditionContextEpoch() > ctx2);
            expectEquals ((juce::int64) proc.getProjectionBaseEpoch(),
                          (juce::int64) base2);

            const auto ctxAfterDryWet = proc.getAuditionContextEpoch();
            aieq::test::setBool (*this, proc.getAPVTS(), "dynEqEnabled", false);
            expect (proc.getAuditionContextEpoch() > ctxAfterDryWet);
            expectEquals ((juce::int64) proc.getProjectionBaseEpoch(),
                          (juce::int64) base2);
            proc.releaseResources();
        }

        beginTest ("7. old projection is stale after a later band edit");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            pumpAudio (proc);

            const auto adj = std::vector<Adjustment> {
                makeAdj (Quality::Air, 12000.0f, 1.8f)
            };
            const auto projection = proc.buildSemanticApplyProjection (
                adj, Policy::RequireCompletePlan);
            expect (! projection.epochStale);
            expect (projection.projectionBaseEpoch == proc.getProjectionBaseEpoch());
            const auto fresh = proc.snapshotProjectedEffectiveDSPState (projection);
            expect (fresh.publishable);
            expect (fresh.state.projectionBaseEpoch == projection.projectionBaseEpoch);

            aieq::test::setFloat (*this, proc.getAPVTS(), "band0Gain", -3.0f);
            expect (proc.getProjectionBaseEpoch() > projection.projectionBaseEpoch);

            const auto stale = proc.snapshotProjectedEffectiveDSPState (projection);
            expect (! stale.publishable, "held projection must not publish after A moves");
            expect (stale.epochStale);
            expect (stale.state.projectionBaseEpoch == projection.projectionBaseEpoch,
                    "stale payload must keep the origin epoch, not stamp now");
            expect (stale.state.projectionBaseEpoch != proc.getProjectionBaseEpoch());
            proc.releaseResources();
        }

        beginTest ("8. two plans against the same A get distinct previewGeneration");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            pumpAudio (proc);

            const auto first = proc.buildSemanticApplyProjection (
                std::vector<Adjustment> { makeAdj (Quality::Air, 12000.0f, 1.8f) },
                Policy::RequireCompletePlan);
            const auto second = proc.buildSemanticApplyProjection (
                std::vector<Adjustment> { makeAdj (Quality::Presence, 4000.0f, 1.5f) },
                Policy::RequireCompletePlan);
            expect (! first.epochStale);
            expect (! second.epochStale);
            expect (first.projectionBaseEpoch == second.projectionBaseEpoch);
            expect (first.previewGeneration != 0);
            expect (second.previewGeneration != 0);
            expect (first.previewGeneration != second.previewGeneration,
                    "plan identity must not collapse into the A epoch");
            proc.releaseResources();
        }

        beginTest ("9. dynEq mix/makeup and prepareToPlay bump audition context");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            pumpAudio (proc);

            const auto ctx0 = proc.getAuditionContextEpoch();
            const auto base0 = proc.getProjectionBaseEpoch();
            aieq::test::setFloat (*this, proc.getAPVTS(), "dynEqMix", 30.0f);
            expect (proc.getAuditionContextEpoch() > ctx0);
            expectEquals ((juce::int64) proc.getProjectionBaseEpoch(),
                          (juce::int64) base0);

            const auto ctx1 = proc.getAuditionContextEpoch();
            aieq::test::setBool (*this, proc.getAPVTS(), "dynAutoMakeup", true);
            expect (proc.getAuditionContextEpoch() > ctx1);
            expectEquals ((juce::int64) proc.getProjectionBaseEpoch(),
                          (juce::int64) base0);

            const auto ctx2 = proc.getAuditionContextEpoch();
            proc.prepareToPlay (kSr, kBlock);
            expect (proc.getAuditionContextEpoch() > ctx2,
                    "prepareToPlay must invalidate the audition context");
            proc.releaseResources();
        }

        beginTest ("10. mix listeners stay epoch-only and do not republish the EQ");
        {
            auto procOwner = std::make_unique<Processor>();
            auto& proc = *procOwner;
            proc.prepareToPlay (kSr, kBlock);
            pumpAudio (proc);
            pumpAudio (proc);

            const auto param0 = proc.getParameterChangeCounter();
            const auto curve0 = proc.getEQCurveChangeCounter();
            const auto ctx0 = proc.getAuditionContextEpoch();
            const auto base0 = proc.getProjectionBaseEpoch();

            aieq::test::setFloat (*this, proc.getAPVTS(), "dryWet", 50.0f);
            aieq::test::setFloat (*this, proc.getAPVTS(), "outputGain", -1.0f);
            expect (proc.getAuditionContextEpoch() > ctx0);
            expectEquals ((juce::int64) proc.getParameterChangeCounter(),
                          (juce::int64) param0,
                          "dryWet/outputGain must not bump parameterChangeCounter");
            expectEquals ((juce::int64) proc.getEQCurveChangeCounter(),
                          (juce::int64) curve0,
                          "dryWet/outputGain must not bump eqCurveChangeCounter");
            expectEquals ((juce::int64) proc.getProjectionBaseEpoch(),
                          (juce::int64) base0);

            pumpAudio (proc);
            expectEquals ((juce::int64) proc.getParameterChangeCounter(),
                          (juce::int64) param0);
            expectEquals ((juce::int64) proc.getEQCurveChangeCounter(),
                          (juce::int64) curve0,
                          "mix edits must not republish the EQ curve from processBlock");
            expect (proc.getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            expectEquals (proc.getEffectiveActiveBandCount(),
                          proc.getNumActiveBands());
            proc.releaseResources();
        }
    }
};

static EffectiveDSPStateTest effectiveDSPStateTest;
