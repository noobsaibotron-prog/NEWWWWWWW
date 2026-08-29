#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../AI/SemanticPlanner.h"
#include "../GUI/SemanticControlPanel.h"
#include "../GUI/SemanticIntentMap.h"
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

AIEQPerceptual::SemanticProtectedRange airFence()
{
    AIEQPerceptual::SemanticProtectedRange range;
    range.minFrequencyHz = 8000.0f;
    range.maxFrequencyHz = 18000.0f;
    range.maxAbsDeltaDb = 0.25f;
    range.sourcePhrase = "air lock";
    return range;
}

bool waitForPlanMap (SemanticControlPanel& panel,
                     EmberUI::SemanticIntentMapState& lastMap,
                     EmberUI::SemanticIntentMapPhase expected,
                     int timeoutMs = 8000)
{
    const auto deadline = juce::Time::getMillisecondCounter()
                        + static_cast<juce::uint32> (juce::jmax (0, timeoutMs));
    for (;;)
    {
        panel.timerCallback();
        if (lastMap.phase == expected)
            return true;
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;
        juce::Thread::sleep (1);
    }
}
}

class SemanticProtectedRangeHostTest final : public juce::UnitTest
{
public:
    SemanticProtectedRangeHostTest()
        : juce::UnitTest ("Semantic protected range host contract", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest ("session restore does not invent a user Hz fence");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            juce::MemoryBlock before;
            proc.getStateInformation (before);

            const auto planned = AIEQPerceptual::SemanticPlanner().plan (
                "more air", kSr, 1.0f, {}, { airFence() });
            expect (planned.valid);
            expectEquals ((int) planned.target.protectedRegions.size(), 1);

            juce::MemoryBlock afterPlan;
            proc.getStateInformation (afterPlan);
            expectEquals ((int) afterPlan.getSize(), (int) before.getSize(),
                          "planning must not write protect state into the session blob");

            proc.setStateInformation (before.getData(), static_cast<int> (before.getSize()));
            const auto replay = AIEQPerceptual::SemanticPlanner().plan ("more air", kSr);
            expect (replay.target.protectedRegions.empty(),
                    "restore must fail closed: a bare PLAN has no user fence");
        }

        beginTest ("processor RAM fences are not persisted and restore clears them");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            juce::MemoryBlock before;
            proc.getStateInformation (before);
            expect (proc.addUserProtectedRange (airFence()));
            expectEquals ((int) proc.getUserProtectedRanges().size(), 1);

            juce::MemoryBlock afterStore;
            proc.getStateInformation (afterStore);
            expectEquals ((int) afterStore.getSize(), (int) before.getSize());
            expect (before == afterStore, "RAM fences must not enter the session blob");

            proc.setStateInformation (before.getData(), static_cast<int> (before.getSize()));
            expect (proc.getUserProtectedRanges().empty(),
                    "restore must drop user Hz fences rather than reconstruct them");
        }

        beginTest ("PLAN snapshots processor fences only through the panel callback");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            AIEQPerceptual::SemanticProtectedRange lowFence;
            lowFence.minFrequencyHz = 80.0f;
            lowFence.maxFrequencyHz = 250.0f;
            lowFence.sourcePhrase = "low lock";
            expect (proc.addUserProtectedRange (lowFence));

            SemanticEQEngine engine;
            SemanticControlPanel unwired (engine);
            unwired.setSampleRate (kSr);
            EmberUI::SemanticIntentMapState unwiredMap;
            unwired.onIntentMapChanged = [&] (const EmberUI::SemanticIntentMapState& state)
            {
                unwiredMap = state;
            };
            auto* unwiredInput = dynamic_cast<juce::TextEditor*> (
                unwired.findChildWithID ("semanticCommandInput"));
            auto* unwiredPlan = dynamic_cast<juce::TextButton*> (
                unwired.findChildWithID ("semanticPlanButton"));
            expect (unwiredInput != nullptr && unwiredPlan != nullptr);
            if (unwiredInput != nullptr && unwiredPlan != nullptr)
            {
                unwiredInput->setText ("more air", juce::dontSendNotification);
                expect (unwiredInput->getText().trim() == "more air");
                unwiredPlan->onClick();
                expect (waitForPlanMap (unwired, unwiredMap, EmberUI::SemanticIntentMapPhase::Ready));
                bool sawUser = false;
                for (const auto& region : unwiredMap.protectRegions)
                    if (region.sourceId.find (AIEQPerceptual::kUserProtectSourceIdPrefix) == 0)
                        sawUser = true;
                expect (! sawUser, "unwired panel must not read processor fences");
            }

            SemanticControlPanel wired (engine);
            wired.setSampleRate (kSr);
            wired.onRequestProtectedRanges = [&proc]()
            {
                return proc.getUserProtectedRanges();
            };
            EmberUI::SemanticIntentMapState wiredMap;
            wired.onIntentMapChanged = [&] (const EmberUI::SemanticIntentMapState& state)
            {
                wiredMap = state;
            };
            auto* wiredInput = dynamic_cast<juce::TextEditor*> (
                wired.findChildWithID ("semanticCommandInput"));
            auto* wiredPlan = dynamic_cast<juce::TextButton*> (
                wired.findChildWithID ("semanticPlanButton"));
            expect (wiredInput != nullptr && wiredPlan != nullptr);
            if (wiredInput != nullptr && wiredPlan != nullptr)
            {
                wiredInput->setText ("more air", juce::dontSendNotification);
                expect (wiredInput->getText().trim() == "more air");
                wiredPlan->onClick();
                expect (waitForPlanMap (wired, wiredMap, EmberUI::SemanticIntentMapPhase::Ready));
                bool sawUser = false;
                for (const auto& region : wiredMap.protectRegions)
                {
                    if (region.sourceId.find (AIEQPerceptual::kUserProtectSourceIdPrefix) == 0)
                    {
                        sawUser = true;
                        expect (region.kind == AIEQPerceptual::SemanticConstraintKind::ProtectRange);
                        expectWithinAbsoluteError (region.minFrequencyHz, 80.0f, 5.0f);
                        expectWithinAbsoluteError (region.maxFrequencyHz, 250.0f, 5.0f);
                    }
                }
                expect (sawUser, "wired PLAN must project the processor fence");
            }
        }

        beginTest ("APPLY of a normal Semantic plan still completes with a fence unused");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            for (int i = 0; i < Processor::maxBands; ++i)
                aieq::test::setBool (*this, proc.getAPVTS(),
                                     "band" + juce::String (i) + "Enabled", i < 8);
            const auto result = proc.applySemanticAdjustments (
                { makeAdj (Quality::Air, 12000.0f, 1.8f) },
                Policy::RequireCompletePlan);
            expect (result.complete());
            expectEquals (static_cast<int> (result.appliedBandSlots.size()), 1);
        }
    }
};

static SemanticProtectedRangeHostTest semanticProtectedRangeHostTest;

#endif
