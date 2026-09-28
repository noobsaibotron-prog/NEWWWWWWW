#pragma once

/*  DIAGNOSTIC ONLY — lets a headless test host trigger SPECCHIO Apply.

    Active only when EMBER_DIAG_SELFTEST_DIR names a directory. A 20 ms timer
    on the plugin's message thread looks for a file "apply" there; its content
    is "hz db q type". The file is consumed (deleted) and the same code path as
    EmberV2Shell::finishApply's local branch runs: commitGhosts with the count
    mode from EMBER_APPLY_COUNT_DEFER_MS, then scheduleLiveCountFlush when the
    count is pending. The UI state machine around it (bar, graph morph) is NOT
    exercised. Result is written to "apply.done".

    Lives on the diag branch only, never merged.                                */

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberApplyCommit.h"

namespace EmberDiag
{
class ApplySelfTest final : private juce::Timer, private juce::DeletedAtShutdown
{
public:
    static void startIfRequested (AIEqualizerAudioProcessor& p)
    {
        const auto dir = juce::SystemStats::getEnvironmentVariable ("EMBER_DIAG_SELFTEST_DIR", {});
        if (dir.isNotEmpty())
            new ApplySelfTest (p, juce::File (dir));   // owns itself; DeletedAtShutdown
    }

private:
    ApplySelfTest (AIEqualizerAudioProcessor& p, juce::File d) : processor (&p), dir (std::move (d))
    {
        startTimer (20);
    }

    void timerCallback() override
    {
        auto* p = processor.get();
        if (p == nullptr) { stopTimer(); delete this; return; }

        const auto trigger = dir.getChildFile ("apply");
        if (! trigger.existsAsFile()) return;

        const auto tokens = juce::StringArray::fromTokens (trigger.loadFileAsString(), " \n", "");
        trigger.deleteFile();
        if (tokens.size() < 4) { dir.getChildFile ("apply.done").replaceWithText ("error bad trigger\n"); return; }

        EmberGhostBand g;
        g.hz = tokens[0].getFloatValue();
        g.db = tokens[1].getFloatValue();
        g.q = tokens[2].getFloatValue();
        g.type = tokens[3].getIntValue();

        // Same as EmberV2Shell::finishApply, local-phrase branch.
        const int countDeferMs = EmberApply::countDeferMsFromEnvironment();
        const auto result = EmberApply::commitGhosts (*p, { g },
            countDeferMs >= 0 ? EmberApply::CountWrite::Deferred : EmberApply::CountWrite::Immediate);
        if (result.countPending)
            EmberApply::scheduleLiveCountFlush (*p, countDeferMs);

        juce::String slots;
        for (auto s : result.slots) slots << s << " ";
        dir.getChildFile ("apply.done").replaceWithText (
            "committed=" + juce::String ((int) result.committed)
            + " pending=" + juce::String ((int) result.countPending)
            + " prev=" + juce::String (result.previousLiveCount)
            + " new=" + juce::String (result.newLiveCount)
            + " slots=" + slots.trim() + "\n");
    }

    juce::WeakReference<AIEqualizerAudioProcessor> processor;
    juce::File dir;
};
} // namespace EmberDiag
