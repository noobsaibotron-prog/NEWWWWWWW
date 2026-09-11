#pragma once
#include <cmath>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../PluginProcessor.h"
#include "EmberTokens.h"

/** SPECCHIO Apply — the single write from the amber ghosts (I) to the bands (F).

    ADDITIVE, by Marco's decision of 2026-09-11 ("apply dovrebbe aggiungere non
    sostituire"). Every band that is live before Apply stays exactly as it was.
    Before this, Apply replaced the whole EQ with the ghosts: on Vocal Clarity
    every phrase left one band and removed the Low Cut.

    Each ghost gets a slot of its own, lowest first:
      1. a disabled band inside the live count;
      2. otherwise the next slot past the live count, so the count grows
         contiguously and no stale slot is swept into it.
    An enabled band is never a candidate, whatever its gain: a cut filter sits
    at 0 dB and is still the user's filter, and a flat peak may be parked there
    on purpose (brief T1-4 / T1-5).

    Atomic: if there is not a slot for every ghost, nothing is written.

    EmberV2Shell::finishApply() calls this, and the tests call this same function,
    so there is no copy of the commit logic to drift away from the shell. */
namespace EmberApply
{
using Processor = AIEqualizerAudioProcessor;

/** Match createParameterLayout intervals (Freq 1 Hz, Gain 0.1 dB, Q 0.01). */
inline void quantizeGhostToApvts(EmberGhostBand& g) noexcept
{
    g.hz = std::round(juce::jlimit(20.0f, 20000.0f, g.hz));
    g.db = std::round(juce::jlimit(-24.0f, 24.0f, g.db) * 10.0f) * 0.1f;
    g.q  = std::round(juce::jlimit(0.1f, 10.0f, g.q) * 100.0f) * 0.01f;
    g.type = juce::jlimit(0, 8, g.type);
}

struct Result
{
    bool committed = false;
    int previousLiveCount = 0;
    int newLiveCount = 0;
    std::vector<int> slots;   // slot written for each ghost, in ghost order
};

/** The committed band count: the APVTS parameter, not the processor's atomic,
    which is rewritten from the parameter only inside processBlock. */
inline int readLiveCount(Processor& processor)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.getAPVTS().getParameter("numActiveBands")))
        return juce::jlimit(1, Processor::maxBands, choice->getIndex() + 1);
    return juce::jlimit(1, Processor::maxBands, processor.getNumActiveBands());
}

inline void writeLiveCount(Processor& processor, int count)
{
    const int clamped = juce::jlimit(1, Processor::maxBands, count);
    if (auto* param = processor.getAPVTS().getParameter("numActiveBands"))
    {
        // Choice index is 0-based (display "1".."24" → index count-1).
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(clamped - 1)));
        param->endChangeGesture();
    }
    processor.setNumActiveBands(clamped);
}

/** Slots for `needed` ghosts: disabled bands inside the live count, then
    consecutive slots past it. Fewer than `needed` entries means no room. */
inline std::vector<int> planSlots(const Processor& processor, int liveCount, int needed)
{
    std::vector<int> slots;
    for (int i = 0; i < liveCount && (int) slots.size() < needed; ++i)
        if (! processor.getBandState(i).enabled)
            slots.push_back(i);
    for (int i = liveCount; i < Processor::maxBands && (int) slots.size() < needed; ++i)
        slots.push_back(i);
    return slots;
}

inline Result commitGhosts(Processor& processor, std::vector<EmberGhostBand> ghosts)
{
    Result result;
    result.previousLiveCount = readLiveCount(processor);
    result.newLiveCount = result.previousLiveCount;

    for (auto& g : ghosts)
        quantizeGhostToApvts(g);

    if (ghosts.empty())
        return result;

    const auto slots = planSlots(processor, result.previousLiveCount, (int) ghosts.size());
    if (slots.size() < ghosts.size())
        return result;   // no room for every ghost: write nothing

    int newCount = result.previousLiveCount;

    for (size_t k = 0; k < ghosts.size(); ++k)
    {
        const auto& g = ghosts[k];
        const int slot = slots[k];

        // A complete, fresh band first, so nothing left in this slot from an
        // earlier use — dynamic mode, solo, slope, curve mode, sidechain —
        // comes along with the ghost. The preview is a static bell; the band
        // written must be one too.
        Processor::BandState fresh;
        fresh.frequency = g.hz;
        fresh.gain = g.db;
        fresh.q = g.q;
        fresh.type = g.type;
        fresh.enabled = true;
        processor.setBandState(slot, fresh);

        // setBandState skips differences under its tolerances (1 Hz, 0.05 dB,
        // 0.02 Q). The gate is the quantised ghost exactly, so write the
        // geometry once more through the exact path.
        processor.setBandGeometry(slot, g.hz, g.db, g.q, g.type, true);

        newCount = juce::jmax(newCount, slot + 1);
        result.slots.push_back(slot);
    }

    // The count grows last, in the same call: a slot past the old count is fully
    // written before it becomes live, so the audio thread never runs half a band.
    if (newCount != result.previousLiveCount)
        writeLiveCount(processor, newCount);

    result.committed = true;
    result.newLiveCount = newCount;
    return result;
}
} // namespace EmberApply
