#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../AI/SemanticPlanner.h"
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
