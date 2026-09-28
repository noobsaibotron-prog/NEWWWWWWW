#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberApplyCommit.h"

#include <vector>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
 // Linux/Windows: JUCE's own queue pump (juce_MessageManager.cpp), no modal loops needed.
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue(bool returnIfNoPendingMessages); }
#endif

/*
 * What SPECCHIO Apply tells the HOST, write by write.
 *
 * Diagnosis 2026-09-23 in Ableton (VST3 wrapper log): after Apply 8→9,
 * "Annulla Cambia Number of Bands" wrote numActiveBands = 0.47061 instead of
 * 0.30435; after 9→10 it wrote 1.0. Both values are the NEW value of the write
 * two places before the count in Apply's host sequence:
 *   8→9 : ... Enabled=1.0, SidechainFreq=0.47061 (1000 Hz), Threshold, Count
 *   9→10: ... Enabled=1.0, Threshold, Count
 * SidechainFreq and Threshold were written only because the fresh slot came
 * from BandState{} (1000 Hz, -24 dB) instead of the parameter defaults
 * (the band's own frequency, -20 dB).
 *
 * The listener below receives exactly what juce_audio_plugin_client_VST3 turns
 * into beginEdit/performEdit/endEdit for the host.
 *
 * NOT covered: how Ableton groups those edits into one undo step. Only a run
 * in Live can say whether a separate count write restores the right value.
 */
namespace
{
using P = AIEqualizerAudioProcessor;

struct HostWriteLog final : juce::AudioProcessorListener
{
    explicit HostWriteLog(P& processorToWatch) : p(processorToWatch) { p.addListener(this); }
    ~HostWriteLog() override { p.removeListener(this); }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int index, float newValue) override
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[index]))
            writes.push_back({ ranged->paramID, newValue });
    }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}

    juce::StringArray ids() const
    {
        juce::StringArray out;
        for (const auto& w : writes)
            out.add(w.id);
        return out;
    }

    struct Write { juce::String id; float value; };
    P& p;
    std::vector<Write> writes;
};

EmberGhostBand ghost2k()
{
    EmberGhostBand g;
    g.type = 2; g.hz = 2000.0f; g.db = -4.0f; g.q = 2.5f;
    return g;
}

float defaultPlain(P& p, const juce::String& id)
{
    auto* param = p.getAPVTS().getParameter(id);
    return param != nullptr ? param->convertFrom0to1(param->getDefaultValue()) : -1.0e9f;
}

float currentPlain(P& p, const juce::String& id)
{
    auto* param = p.getAPVTS().getParameter(id);
    return param != nullptr ? param->convertFrom0to1(param->getValue()) : -1.0e9f;
}

void pump(P& p, int blocks)
{
    juce::Random rnd(20260928);
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample(ch, i, (rnd.nextFloat() * 2.0f - 1.0f) * 0.05f);
        p.processBlock(buf, midi);
    }
}

float appliedDbAt(P& p, float hz)
{
    float f = hz, m = 1.0f;
    p.getEQProcessor().getMagnitudeForFrequencyArray(&f, &m, 1, p.getSampleRate());
    return juce::Decibels::gainToDecibels(juce::jmax(1.0e-6f, m), -96.0f);
}
} // namespace

class EmberApplyHostWritesTest : public juce::UnitTest
{
public:
    EmberApplyHostWritesTest() : juce::UnitTest("Ember Apply host writes", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        testPristineSlotWritesOnlyTheGhost(8);
        testPristineSlotWritesOnlyTheGhost(9);
        testDeferredCountIsItsOwnHostStep();
        testDeferredBandStaysSilentUntilTheCount();
        testStaleDeferredCountIsDropped();
        testScheduledFlushRunsOnTheMessageLoop(0);
        testScheduledFlushRunsOnTheMessageLoop(50);
        testScheduledFlushSurvivesTheProcessorGoingAway();
        testEnvironmentSwitch();
    }

private:
    /** Runs the real message loop (callAsync and Timer deliveries) for `ms`.
        Not stopDispatchLoop(): that shuts the queue for good. */
    static void runLoop(int ms)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + ms;
        while (juce::Time::getMillisecondCounterHiRes() < until)
        {
#if JUCE_MAC
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, true);
#else
            if (! juce::detail::dispatchNextMessageOnSystemQueue(true))
                juce::Thread::sleep(1);
#endif
        }
    }

    /** The shell's path: commit deferred, then scheduleLiveCountFlush. */
    void testScheduledFlushRunsOnTheMessageLoop(int delayMs)
    {
        beginTest("Scheduled flush after " + juce::String(delayMs) + " ms: the count arrives alone, once");

        P p; p.prepareToPlay(48000.0, 512);
        HostWriteLog log(p);
        const auto r = EmberApply::commitGhosts(p, { ghost2k() }, EmberApply::CountWrite::Deferred);
        expect(r.countPending);
        EmberApply::scheduleLiveCountFlush(p, delayMs);

        expectEquals(EmberApply::readHostLiveCount(p), 8, "nothing written synchronously");
        runLoop(delayMs + 150);

        expectEquals(EmberApply::readHostLiveCount(p), 9, "the count arrived");
        expectEquals(p.getNumActiveBands(), 9, "processor count follows");
        int countWrites = 0;
        for (const auto& w : log.writes)
            countWrites += (w.id == "numActiveBands") ? 1 : 0;
        expectEquals(countWrites, 1, "exactly one host write of the count");
        expect(log.writes.back().id == "numActiveBands", "and it is the last thing the host hears");
        expect(p.emberPendingLiveCount().count == 0, "nothing left pending");
    }

    void testScheduledFlushSurvivesTheProcessorGoingAway()
    {
        beginTest("A flush scheduled on a processor that is gone does nothing (no crash)");

        {
            P p; p.prepareToPlay(48000.0, 512);
            EmberApply::commitGhosts(p, { ghost2k() }, EmberApply::CountWrite::Deferred);
            EmberApply::scheduleLiveCountFlush(p, 30);
        }   // plugin removed before the timer fires
        runLoop(200);
        expect(true, "reached after the timer");
    }

    void testEnvironmentSwitch()
    {
        beginTest("EMBER_APPLY_COUNT_DEFER_MS: unset/invalid = immediate, digits = deferred");

        // Cannot set the environment portably from here; check the parser's
        // contract on what it reads today (unset in the test runner).
        expectEquals(EmberApply::countDeferMsFromEnvironment(), -1, "unset means immediate");
        expectEquals(EmberApply::parseCountDeferMs(""), -1);
        expectEquals(EmberApply::parseCountDeferMs("abc"), -1);
        expectEquals(EmberApply::parseCountDeferMs("-5"), -1);
        expectEquals(EmberApply::parseCountDeferMs("0"), 0);
        expectEquals(EmberApply::parseCountDeferMs(" 250 "), 250);
        expectEquals(EmberApply::parseCountDeferMs("99999"), 2000, "clamped");
    }
    void testDeferredCountIsItsOwnHostStep()
    {
        beginTest("Deferred count: the bands now, the count later as one host write");

        P p; p.prepareToPlay(48000.0, 512);
        HostWriteLog log(p);

        const auto r = EmberApply::commitGhosts(p, { ghost2k() }, EmberApply::CountWrite::Deferred);
        expect(r.committed);
        expect(r.countPending);
        expectEquals(r.newLiveCount, 9);
        expectEquals(EmberApply::readHostLiveCount(p), 8, "host count not written with the bands");
        expectEquals(EmberApply::readLiveCount(p), 9, "committed count includes the pending band");
        expect(! log.ids().contains("numActiveBands"), "no count write in the band step");

        // A second Apply before the flush takes the next slot, not the same one.
        auto second = ghost2k();
        second.hz = 5000.0f;
        const auto r2 = EmberApply::commitGhosts(p, { second }, EmberApply::CountWrite::Deferred);
        if (r2.slots.size() == 1)
            expectEquals(r2.slots[0], 9);
        expectEquals(EmberApply::readLiveCount(p), 10);

        log.writes.clear();
        EmberApply::flushPendingLiveCount(p);
        expectEquals(log.ids().joinIntoString(", "), juce::String("numActiveBands"), "flush = one host write");
        expectEquals(EmberApply::readHostLiveCount(p), 10);
        expectEquals(p.getNumActiveBands(), 10);

        log.writes.clear();
        EmberApply::flushPendingLiveCount(p);
        expect(log.writes.empty(), "a second flush writes nothing");
    }

    void testDeferredBandStaysSilentUntilTheCount()
    {
        beginTest("Deferred count: the written band is silent until the count grows");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::commitGhosts(p, { ghost2k() }, EmberApply::CountWrite::Deferred);
        pump(p, 40);
        expectWithinAbsoluteError(appliedDbAt(p, 2000.0f), 0.0f, 0.1f, "before the flush");

        EmberApply::flushPendingLiveCount(p);
        pump(p, 40);
        expectWithinAbsoluteError(appliedDbAt(p, 2000.0f), -4.0f, 0.3f, "after the flush");
    }

    void testStaleDeferredCountIsDropped()
    {
        beginTest("A deferred count is dropped when the host count moved before the flush");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::commitGhosts(p, { ghost2k() }, EmberApply::CountWrite::Deferred);
        EmberApply::writeLiveCount(p, 6);   // host undo or preset load in between
        expectEquals(EmberApply::readLiveCount(p), 6, "stale pending count ignored");

        EmberApply::flushPendingLiveCount(p);
        expectEquals(EmberApply::readHostLiveCount(p), 6, "flush leaves the host count alone");
    }

    /** Default EQ (8 live bands), grown by Apply as in Live (8→9, then 9→10),
        then one logged Apply into `slot`. */
    void testPristineSlotWritesOnlyTheGhost(int slot)
    {
        beginTest("Apply into pristine slot " + juce::String(slot)
                  + " notifies the host of the ghost geometry and the count, nothing else");

        P p; p.prepareToPlay(48000.0, 512);
        for (int grow = 8; grow < slot; ++grow)
        {
            auto earlier = ghost2k();
            earlier.hz = 300.0f + 100.0f * (float) grow;
            EmberApply::commitGhosts(p, { earlier });
        }
        expectEquals(EmberApply::readLiveCount(p), slot, "setup: live count");

        HostWriteLog log(p);
        const auto r = EmberApply::commitGhosts(p, { ghost2k() });

        logMessage("host writes: " + log.ids().joinIntoString(", "));

        expect(r.committed);
        if (r.slots.size() == 1)
            expectEquals(r.slots[0], slot);

        const juce::String b = "band" + juce::String(slot);
        const juce::StringArray expected { b + "Freq", b + "Gain", b + "Q", b + "Enabled", "numActiveBands" };
        expectEquals(log.ids().joinIntoString(", "), expected.joinIntoString(", "), "host write sequence");

        for (const auto* id : { "Threshold", "SidechainFreq", "SidechainQ", "Ratio", "Attack",
                                "Release", "Range", "Knee", "DynMode", "Solo", "Slope", "CurveMode" })
            expectWithinAbsoluteError(currentPlain(p, b + id), defaultPlain(p, b + id), 1.0e-4f,
                                      b + id + " stays at its parameter default");
    }
};

static EmberApplyHostWritesTest emberApplyHostWritesTest;

#endif
