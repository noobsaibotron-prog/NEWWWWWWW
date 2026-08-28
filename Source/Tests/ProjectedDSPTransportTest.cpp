#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../DSP/EffectiveDSPState.h"
#include "Support/TestParameters.h"

#if JUCE_MAC || JUCE_IOS
#include <malloc/malloc.h>
#endif

#include <memory>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;
using Adjustment = SemanticEQEngine::SemanticEQAdjustment;
using Quality = SemanticEQEngine::SemanticQuality;
using Policy = Processor::SemanticApplyPolicy;
using Outcome = Processor::ProjectedDSPTransportOutcome;

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

void drainParams (Processor& proc)
{
    pumpAudio (proc);
    pumpAudio (proc);
}

Processor::ProjectedDSPSnapshot snapshotFrom (juce::UnitTest& test,
                                              Processor& proc,
                                              Quality quality,
                                              float frequency,
                                              float gain)
{
    const auto projection = proc.buildSemanticApplyProjection (
        std::vector<Adjustment> { makeAdj (quality, frequency, gain) },
        Policy::RequireCompletePlan);
    test.expect (! projection.epochStale);
    test.expect (projection.previewGeneration != 0);
    auto snapshot = proc.snapshotProjectedEffectiveDSPState (projection);
    test.expect (snapshot.publishable);
    test.expect (snapshot.state.previewGeneration == projection.previewGeneration);
    return snapshot;
}

Processor::ProjectedDSPSnapshot makePublishableSnapshot (juce::UnitTest& test,
                                                         Processor& proc,
                                                         Quality quality,
                                                         float frequency,
                                                         float gain)
{
    restoreFactoryFreeHighSlots (test, proc);
    return snapshotFrom (test, proc, quality, frequency, gain);
}

juce::String committedDigest (Processor& proc)
{
    juce::String d;
    for (int i = 0; i < Processor::maxBands; ++i)
    {
        const auto b = proc.getBandState (i);
        d << i << ":" << b.frequency << "," << b.gain << "," << b.q << ","
          << (int) b.enabled << ";";
    }
    return d;
}
} // namespace

class ProjectedDSPTransportTest final : public juce::UnitTest
{
public:
    ProjectedDSPTransportTest()
        : juce::UnitTest ("Projected DSP transport", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;
        const auto makeProc = [&]()
        {
            auto proc = std::make_unique<Processor>();
            proc->prepareToPlay (kSr, kBlock);
            return proc;
        };

        beginTest ("1. publish/acquire/release preserves previewGeneration");
        {
            auto proc = makeProc();
            drainParams (*proc);
            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            const auto gen = snapshot.state.previewGeneration;
            expect (proc->publishProjectedDSPState (snapshot));
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Accepted);
            expect (proc->getLastAcceptedPreviewGeneration() == gen);
            expect (proc->getLastAcceptedProjectedDSPState().previewGeneration == gen);
            expect (proc->getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            proc->releaseResources();
        }

        beginTest ("2. newest published payload wins");
        {
            auto proc = makeProc();
            drainParams (*proc);
            restoreFactoryFreeHighSlots (*this, *proc);
            const auto first = snapshotFrom (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            const auto second = snapshotFrom (
                *this, *proc, Quality::Presence, 4000.0f, 1.5f);
            expect (first.state.previewGeneration != second.state.previewGeneration);
            expect (proc->publishProjectedDSPState (first));
            expect (proc->publishProjectedDSPState (second));
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Accepted);
            expect (proc->getLastAcceptedPreviewGeneration()
                        == second.state.previewGeneration);
            proc->releaseResources();
        }

        beginTest ("3. mailbox is per-instance");
        {
            auto procA = makeProc();
            auto procB = makeProc();
            drainParams (*procA);
            drainParams (*procB);
            const auto snapshot = makePublishableSnapshot (
                *this, *procA, Quality::Air, 12000.0f, 1.8f);
            expect (procA->publishProjectedDSPState (snapshot));
            pumpAudio (*procB);
            expectEquals ((int) procB->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Idle);
            expectEquals ((juce::int64) procB->getLastAcceptedPreviewGeneration(),
                          (juce::int64) 0);
            expectEquals ((juce::int64) procB->getProjectedDSPAcceptedCount(),
                          (juce::int64) 0);
            pumpAudio (*procA);
            expectEquals ((int) procA->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Accepted);
            procA->releaseResources();
            procB->releaseResources();
        }

        beginTest ("4. older previewGeneration is rejected");
        {
            auto proc = makeProc();
            drainParams (*proc);
            restoreFactoryFreeHighSlots (*this, *proc);
            const auto first = snapshotFrom (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            const auto second = snapshotFrom (
                *this, *proc, Quality::Presence, 4000.0f, 1.5f);
            expect (proc->publishProjectedDSPState (first));
            pumpAudio (*proc);
            expect (proc->publishProjectedDSPState (second));
            pumpAudio (*proc);
            expect (proc->getLastAcceptedPreviewGeneration()
                        == second.state.previewGeneration);
            const auto accepted = proc->getProjectedDSPAcceptedCount();
            const auto rejected = proc->getProjectedDSPRejectedCount();
            expect (proc->publishProjectedDSPState (first));
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::RejectedStaleGeneration);
            expect (proc->getLastAcceptedPreviewGeneration()
                        == second.state.previewGeneration);
            expectEquals ((int) proc->getProjectedDSPAcceptedCount(), (int) accepted);
            expect (proc->getProjectedDSPRejectedCount() > rejected);
            proc->releaseResources();
        }

        beginTest ("5. projectionBaseEpoch mismatch is rejected");
        {
            auto proc = makeProc();
            drainParams (*proc);
            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            expect (proc->publishProjectedDSPState (snapshot));
            aieq::test::setFloat (*this, proc->getAPVTS(), "band0Gain", -3.0f);
            expect (proc->getProjectionBaseEpoch() > snapshot.state.projectionBaseEpoch);
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::RejectedProjectionBaseEpoch);
            expectEquals ((juce::int64) proc->getLastAcceptedPreviewGeneration(),
                          (juce::int64) 0);
            proc->releaseResources();
        }

        beginTest ("6. auditionContextEpoch mismatch is rejected");
        {
            auto proc = makeProc();
            drainParams (*proc);
            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            expect (proc->publishProjectedDSPState (snapshot));
            const auto param0 = proc->getParameterChangeCounter();
            aieq::test::setFloat (*this, proc->getAPVTS(), "dryWet", 50.0f);
            expect (proc->getAuditionContextEpoch() > snapshot.state.auditionContextEpoch);
            expectEquals ((juce::int64) proc->getParameterChangeCounter(),
                          (juce::int64) param0);
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::RejectedAuditionContextEpoch);
            proc->releaseResources();
        }

        beginTest ("7. non-publishable projection never enters the mailbox");
        {
            auto proc = makeProc();
            drainParams (*proc);
            auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            snapshot.publishable = false;
            snapshot.epochStale = true;
            expect (! proc->publishProjectedDSPState (snapshot));
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::RejectedUnpublishable);
            const auto rejected = proc->getProjectedDSPRejectedCount();
            pumpAudio (*proc);
            expectEquals ((juce::int64) proc->getLastAcceptedPreviewGeneration(),
                          (juce::int64) 0);
            expectEquals ((juce::int64) proc->getProjectedDSPAcceptedCount(),
                          (juce::int64) 0);
            expectEquals ((int) proc->getProjectedDSPRejectedCount(), (int) rejected);
            proc->releaseResources();
        }

        beginTest ("8. consume is independent of needsParamUpdate and does not audition B");
        {
            auto proc = makeProc();
            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Presence, 4000.0f, 1.5f);
            drainParams (*proc);
            const auto param0 = proc->getParameterChangeCounter();
            const auto curve0 = proc->getEQCurveChangeCounter();
            const auto digest0 = committedDigest (*proc);
            expect (proc->publishProjectedDSPState (snapshot));
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Accepted);
            expectEquals ((juce::int64) proc->getParameterChangeCounter(),
                          (juce::int64) param0);
            expectEquals ((juce::int64) proc->getEQCurveChangeCounter(),
                          (juce::int64) curve0);
            expectEquals (committedDigest (*proc), digest0);
            expect (proc->getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            expectEquals (proc->getEffectiveActiveBandCount(),
                          proc->getNumActiveBands());
            proc->releaseResources();
        }

        beginTest ("9. VPA transport does not allocate in processBlock");
        {
            auto proc = makeProc();
            drainParams (*proc);
            for (int i = 0; i < 16; ++i)
                pumpAudio (*proc);

            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);

#if JUCE_MAC || JUCE_IOS
            malloc_statistics_t before {};
            malloc_zone_statistics (malloc_default_zone(), &before);
#endif
            for (int i = 0; i < 32; ++i)
            {
                expect (proc->publishProjectedDSPState (snapshot));
                pumpAudio (*proc);
            }
#if JUCE_MAC || JUCE_IOS
            malloc_statistics_t after {};
            malloc_zone_statistics (malloc_default_zone(), &after);
            const auto growth = after.size_in_use > before.size_in_use
                ? (after.size_in_use - before.size_in_use) : 0;
            expect (growth <= 65536,
                    "VPA mailbox consume must not allocate in processBlock; growth="
                        + juce::String ((juce::int64) growth));
#endif
            expect (proc->getProjectedDSPAcceptedCount() >= 1);
            expect (proc->getEffectiveDSPSource()
                        == EmberDSP::EffectiveDSPSource::CommittedA);
            proc->releaseResources();
        }

        beginTest ("10. prepareToPlay bumps context epoch and cannot recycle identity");
        {
            auto proc = makeProc();
            drainParams (*proc);
            const auto snapshot = makePublishableSnapshot (
                *this, *proc, Quality::Air, 12000.0f, 1.8f);
            const auto contextBefore = proc->getAuditionContextEpoch();
            expect (proc->publishProjectedDSPState (snapshot));
            proc->prepareToPlay (kSr, kBlock);
            expect (proc->getAuditionContextEpoch() > contextBefore);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Idle);
            expectEquals ((juce::int64) proc->getLastAcceptedPreviewGeneration(),
                          (juce::int64) 0);
            expectEquals ((juce::int64) proc->getLastAcceptedProjectedDSPState().previewGeneration,
                          (juce::int64) 0);
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::Idle);
            expect (proc->publishProjectedDSPState (snapshot));
            pumpAudio (*proc);
            expectEquals ((int) proc->getLastProjectedDSPTransportOutcome(),
                          (int) Outcome::RejectedAuditionContextEpoch);
            expectEquals ((juce::int64) proc->getLastAcceptedPreviewGeneration(),
                          (juce::int64) 0);
            proc->releaseResources();
        }
    }
};

static ProjectedDSPTransportTest projectedDSPTransportTest;
