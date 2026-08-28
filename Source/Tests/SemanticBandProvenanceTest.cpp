#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "Support/TestParameters.h"

#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;
using Adjustment = SemanticEQEngine::SemanticEQAdjustment;
using Quality = SemanticEQEngine::SemanticQuality;
using Policy = Processor::SemanticApplyPolicy;

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

int countManaged (const Processor& proc)
{
    int n = 0;
    for (int i = 0; i < Processor::maxBands; ++i)
        if (proc.isSemanticManagedBand (i))
            ++n;
    return n;
}
}

class SemanticBandProvenanceTest final : public juce::UnitTest
{
public:
    SemanticBandProvenanceTest()
        : juce::UnitTest ("Semantic band provenance", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest ("fresh processor claims no Semantic-managed slots");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            expectEquals (countManaged (proc), 0);
            expect (! proc.isSemanticManagedBand (-1));
            expect (! proc.isSemanticManagedBand (Processor::maxBands));
        }

        beginTest ("successful APPLY marks only the written slots");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Air, 12000.0f, 1.8f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            expectEquals (static_cast<int> (result.appliedBandSlots.size()), 1);
            const int slot = result.appliedBandSlots.front();
            expect (proc.isSemanticManagedBand (slot));
            expectEquals (countManaged (proc), 1);
            for (int i = 0; i < Processor::maxBands; ++i)
                if (i != slot)
                    expect (! proc.isSemanticManagedBand (i));
        }

        beginTest ("in-tolerance drift keeps provenance; material edit clears it");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Warmth, 180.0f, 2.0f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            const int slot = result.appliedBandSlots.front();

            auto within = proc.getBandState (slot);
            within.gain += 0.02f;
            proc.setBandState (slot, within);
            expect (proc.isSemanticManagedBand (slot),
                    "gain inside bandStatesEquivalent must stay Semantic-managed");

            auto diverged = proc.getBandState (slot);
            diverged.gain += 1.5f;
            proc.setBandState (slot, diverged);
            expect (! proc.isSemanticManagedBand (slot),
                    "material manual edit must drop provenance immediately");
        }

        beginTest ("graph geometry edits also drop provenance");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Presence, 4000.0f, 1.2f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            const int slot = result.appliedBandSlots.front();
            const auto before = proc.getBandState (slot);
            proc.setBandGeometry (slot, before.frequency * 1.5f, before.gain,
                                  before.q, before.type, before.enabled);
            expect (! proc.isSemanticManagedBand (slot));
        }

        beginTest ("undo of APPLY drops provenance; redo restores matching bands");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Air, 11000.0f, 1.4f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            const int slot = result.appliedBandSlots.front();
            expect (proc.isSemanticManagedBand (slot));
            expect (proc.canUndo());
            proc.undo();
            expect (! proc.isSemanticManagedBand (slot));
            if (proc.canRedo())
            {
                proc.redo();
                expect (proc.isSemanticManagedBand (slot),
                        "redo back onto lastApplied must show Semantic-managed");
            }
        }

        beginTest ("A/B switch fail-closes on the loaded slot and returns on the applied one");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Air, 13000.0f, 1.1f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            const int slot = result.appliedBandSlots.front();
            expect (proc.isSemanticManagedBand (slot));

            proc.setABState (Processor::ABState::B);
            expect (! proc.isSemanticManagedBand (slot),
                    "factory B must not inherit a Semantic claim");

            proc.setABState (Processor::ABState::A);
            expect (proc.isSemanticManagedBand (slot),
                    "returning to A restores bands that still match last APPLY");
        }

        beginTest ("preset restore never reconstructs a Semantic-managed claim");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            restoreFactoryFreeHighSlots (*this, proc);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Air, 12500.0f, 1.6f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            const int slot = result.appliedBandSlots.front();
            expect (proc.isSemanticManagedBand (slot));

            juce::MemoryBlock blob;
            proc.getStateInformation (blob);
            expect (blob.getSize() > 0);
            proc.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
            expectEquals (countManaged (proc), 0,
                          "session/preset load must fail closed without serialized provenance");
        }

        beginTest ("atomic reject does not mint provenance");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            for (int i = 0; i < Processor::maxBands; ++i)
            {
                auto band = proc.getBandState (i);
                band.enabled = true;
                band.gain = (i % 2 == 0) ? 4.5f : -4.5f;
                proc.setBandState (i, band);
            }
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Warmth, 180.0f, 1.0f) },
                Policy::RequireCompletePlan);
            expect (result.atomicRejected);
            expectEquals (countManaged (proc), 0);
        }
    }
};

static SemanticBandProvenanceTest semanticBandProvenanceTest;

#endif
