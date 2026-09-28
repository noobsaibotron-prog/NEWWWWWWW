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

    Host writes (2026-09-28): a written slot starts from the PARAMETER defaults,
    so the host is told only about the ghost geometry and the count. Starting
    from BandState{} also wrote SidechainFreq (1000 Hz instead of the band's own
    frequency) and Threshold (-24 instead of -20 dB), and Ableton's undo of
    Apply then put one of those values into "Number of Bands".

    The count can be written as a separate host step (CountWrite::Deferred):
    the band is complete but silent until flushPendingLiveCount() grows the
    count. Whether that gives Ableton a correct undo is for a run in Live to
    say; the shell only defers when EMBER_APPLY_COUNT_DEFER_MS is set.

    EmberV2Shell::finishApply() calls this, and the tests call this same function,
    so there is no copy of the commit logic to drift away from the shell. */
namespace EmberApply
{
using Processor = AIEqualizerAudioProcessor;

enum class CountWrite { Immediate, Deferred };

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
    bool countPending = false;   // Deferred: the count waits for flushPendingLiveCount()
    int previousLiveCount = 0;
    int newLiveCount = 0;
    std::vector<int> slots;   // slot written for each ghost, in ghost order
};

/** The count the host has: the APVTS parameter, not the processor's atomic,
    which is rewritten from the parameter only inside processBlock. */
inline int readHostLiveCount(Processor& processor)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor.getAPVTS().getParameter("numActiveBands")))
        return juce::jlimit(1, Processor::maxBands, choice->getIndex() + 1);
    return juce::jlimit(1, Processor::maxBands, processor.getNumActiveBands());
}

/** The committed band count: the host count, or a deferred count grown from it.
    A pending count grown from another host count is stale (preset load, host
    undo) and does not count. */
inline int readLiveCount(Processor& processor)
{
    const int host = readHostLiveCount(processor);
    const auto& pending = processor.emberPendingLiveCount();
    return (pending.count > host && pending.basedOn == host) ? pending.count : host;
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

/** Writes a deferred count as its own host step. Drops it when the host count
    moved in the meantime. Safe to call when nothing is pending. */
inline void flushPendingLiveCount(Processor& processor)
{
    auto& pending = processor.emberPendingLiveCount();
    const auto due = pending;
    pending = {};
    if (due.count > 0 && due.basedOn == readHostLiveCount(processor) && due.count > due.basedOn)
        writeLiveCount(processor, due.count);
}

/** Flush after `delayMs` (0 = next message-loop turn), on the message thread. */
inline void scheduleLiveCountFlush(Processor& processor, int delayMs)
{
    juce::WeakReference<Processor> weak(&processor);
    auto flush = [weak]
    {
        if (auto* p = weak.get())
            flushPendingLiveCount(*p);
    };
    if (delayMs <= 0)
        juce::MessageManager::callAsync(std::move(flush));
    else
        juce::Timer::callAfterDelay(delayMs, std::move(flush));
}

/** Experiment switch for the Live undo test: EMBER_APPLY_COUNT_DEFER_MS=0, 50,
    250… defers the count; unset or invalid keeps the immediate write. */
inline int parseCountDeferMs(const juce::String& raw);
inline int countDeferMsFromEnvironment()
{
    return parseCountDeferMs(juce::SystemStats::getEnvironmentVariable("EMBER_APPLY_COUNT_DEFER_MS", {}));
}

inline int parseCountDeferMs(const juce::String& raw)
{
    const auto value = raw.trim();
    if (value.isEmpty() || ! value.containsOnly("0123456789"))
        return -1;
    return juce::jlimit(0, 2000, value.getIntValue());
}

/** A slot as the parameter layout creates it, so resetting it writes nothing
    the host did not already have. */
inline Processor::BandState parameterDefaultBand(Processor& processor, int slot)
{
    Processor::BandState band;
    const auto prefix = "band" + juce::String(slot);
    auto plain = [&](const char* suffix, float fallback)
    {
        if (auto* param = processor.getAPVTS().getParameter(prefix + suffix))
            return param->convertFrom0to1(param->getDefaultValue());
        return fallback;
    };
    auto index = [&](const char* suffix, int fallback)
    {
        return juce::roundToInt(plain(suffix, static_cast<float>(fallback)));
    };

    band.frequency = plain("Freq", band.frequency);
    band.gain = plain("Gain", band.gain);
    band.q = plain("Q", band.q);
    band.type = index("Type", band.type);
    band.enabled = plain("Enabled", band.enabled ? 1.0f : 0.0f) >= 0.5f;
    band.solo = plain("Solo", band.solo ? 1.0f : 0.0f) >= 0.5f;
    band.slope = index("Slope", band.slope);
    band.curveMode = index("CurveMode", band.curveMode);
    band.dynMode = index("DynMode", band.dynMode);
    band.dynTrigger = index("DynTrigger", band.dynTrigger);
    band.detectionMode = index("DetectionMode", band.detectionMode);
    band.detectorSource = index("DetectorSource", band.detectorSource);
    band.sidechainFrequency = plain("SidechainFreq", band.sidechainFrequency);
    band.sidechainQ = plain("SidechainQ", band.sidechainQ);
    band.dynThreshold = plain("Threshold", band.dynThreshold);
    band.dynRatio = plain("Ratio", band.dynRatio);
    band.dynAttack = plain("Attack", band.dynAttack);
    band.dynRelease = plain("Release", band.dynRelease);
    band.dynRange = plain("Range", band.dynRange);
    band.dynKnee = plain("Knee", band.dynKnee);
    return band;
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

inline Result commitGhosts(Processor& processor, std::vector<EmberGhostBand> ghosts,
                           CountWrite countWrite = CountWrite::Immediate)
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
        auto fresh = parameterDefaultBand(processor, slot);
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

    // The count grows last: a slot past the old count is fully written before
    // it becomes live, so the audio thread never runs half a band.
    if (newCount != result.previousLiveCount)
    {
        if (countWrite == CountWrite::Deferred)
        {
            processor.emberPendingLiveCount() = { newCount, readHostLiveCount(processor) };
            result.countPending = true;
        }
        else
        {
            processor.emberPendingLiveCount() = {};
            writeLiveCount(processor, newCount);
        }
    }

    result.committed = true;
    result.newLiveCount = newCount;
    return result;
}
} // namespace EmberApply
