#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberPhraseParser.h"
#include "../GUI/Ember/EmberApplyCommit.h"

#include <array>
#include <cmath>
#include <vector>

/*
 * SPECCHIO Apply ADDS the amber ghosts to the EQ. It does not replace it.
 *
 * Marco, 2026-09-11: "apply dovrebbe aggiungere non sostituire". Measured before
 * this change at 76fab2de: on Vocal Clarity every phrase reduced five bands to
 * one and removed the Low Cut, and in Ableton a second Apply erased the first.
 *
 * These tests call EmberApply::commitGhosts, which is the function
 * EmberV2Shell::finishApply calls — not a copy of it.
 *
 * NOT covered: the 280 ms morph and the shell's state machine around the call,
 * the refusal message (there is none: the shell stays in FRASE), and host
 * behaviour.
 */
namespace
{
using P = AIEqualizerAudioProcessor;
constexpr int kSurgical = static_cast<int>(ParametricEQProcessor::CurveMode::Surgical);
constexpr int kLegacy   = static_cast<int>(ParametricEQProcessor::CurveMode::Legacy);

std::array<P::BandState, (size_t) P::maxBands> snapshot(const P& p)
{
    std::array<P::BandState, (size_t) P::maxBands> s {};
    for (int i = 0; i < P::maxBands; ++i)
        s[(size_t) i] = p.getBandState(i);
    return s;
}

void pump(P& p, int blocks)
{
    juce::Random rnd(20260911);
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

/** Four live, distinct, user-made bands. */
void seedFourLive(P& p)
{
    EmberApply::writeLiveCount(p, 4);
    p.setBandGeometry(0, 120.0f,  3.0f, 0.8f, 1, true);   // Low Shelf
    p.setBandGeometry(1, 450.0f, -2.5f, 1.2f, 2, true);   // Peak
    p.setBandGeometry(2, 3500.0f, 2.0f, 1.0f, 2, true);   // Peak
    p.setBandGeometry(3, 9000.0f, -1.5f, 0.9f, 3, true);  // High Shelf
}
} // namespace

class EmberApplyAddTest : public juce::UnitTest
{
public:
    EmberApplyAddTest() : juce::UnitTest("Ember Apply adds", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        testDoDPhraseAddsOneBand();
        testEnabledBandsAreNeverTaken();
        testDisabledSlotInsideTheCountIsReused();
        testWrittenSlotStartsClean();
        testTwoGhostsGrowTheCountContiguously();
        testNoRoomMeansNothingIsWritten();
        testPreviousBandsStillSound();
        testQuantisation();
    }

private:
    void expectUntouched(const std::array<P::BandState, (size_t) P::maxBands>& before,
                         const P& p, int first, int last, const juce::String& where)
    {
        for (int i = first; i <= last; ++i)
            expect(P::bandStatesEquivalent(before[(size_t) i], p.getBandState(i)),
                   where + ": band " + juce::String(i) + " changed");
    }

    void testDoDPhraseAddsOneBand()
    {
        beginTest("DoD phrase on a dirty EQ adds one band and leaves the others alone");

        P p; p.prepareToPlay(48000.0, 512);
        seedFourLive(p);
        const auto before = snapshot(p);

        const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("add a peak at 2khz with -4db gain and 2.5 q."));

        expect(r.committed, "should commit");
        expectEquals((int) r.slots.size(), 1);
        if (r.slots.size() == 1) expectEquals(r.slots[0], 4);
        expectEquals(r.previousLiveCount, 4);
        expectEquals(r.newLiveCount, 5);
        expectEquals(EmberApply::readLiveCount(p), 5, "APVTS numActiveBands");
        expectEquals(p.getNumActiveBands(), 5, "processor count");

        const auto b = p.getBandState(4);
        expect(b.enabled);
        expectEquals(b.type, 2);
        expectWithinAbsoluteError(b.frequency, 2000.0f, 0.5f);
        expectWithinAbsoluteError(b.gain, -4.0f, 0.01f);
        expectWithinAbsoluteError(b.q, 2.50f, 0.005f);

        expectUntouched(before, p, 0, 3, "dirty EQ");
    }

    void testEnabledBandsAreNeverTaken()
    {
        beginTest("An enabled band is never taken, not even a cut or a flat peak at 0 dB");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::writeLiveCount(p, 3);
        p.setBandGeometry(0, 80.0f,   0.0f, 1.0f, 0, true);   // Low Cut: gain 0, still a filter
        p.setBandGeometry(1, 1000.0f, 0.0f, 1.0f, 2, true);   // flat Peak the user parked
        p.setBandGeometry(2, 5000.0f, 2.0f, 1.0f, 2, true);
        const auto before = snapshot(p);

        const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("punch"));

        expect(r.committed);
        if (r.slots.size() == 1) expectEquals(r.slots[0], 3, "punch should land past the live bands");
        expectEquals(EmberApply::readLiveCount(p), 4);
        expectUntouched(before, p, 0, 2, "cut and flat peak");
        expectEquals(p.getBandState(0).type, 0, "the Low Cut is still a Low Cut");
    }

    void testDisabledSlotInsideTheCountIsReused()
    {
        beginTest("A disabled slot inside the live count is used before the count grows");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::writeLiveCount(p, 3);
        p.setBandGeometry(0, 200.0f, 2.0f, 1.0f, 2, true);
        p.setBandGeometry(1, 700.0f, 0.0f, 1.0f, 2, false);   // off
        p.setBandGeometry(2, 4000.0f, -2.0f, 1.0f, 2, true);
        const auto before = snapshot(p);

        const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("punch"));

        expect(r.committed);
        if (r.slots.size() == 1) expectEquals(r.slots[0], 1);
        expectEquals(EmberApply::readLiveCount(p), 3, "count should not grow");
        expectUntouched(before, p, 0, 0, "band before the hole");
        expectUntouched(before, p, 2, 2, "band after the hole");
        expect(p.getBandState(1).enabled, "the reused slot is now on");
    }

    void testWrittenSlotStartsClean()
    {
        beginTest("A written slot does not inherit dynamic, solo, slope or curve mode");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::writeLiveCount(p, 3);
        p.setBandGeometry(0, 200.0f, 2.0f, 1.0f, 2, true);
        p.setBandGeometry(2, 4000.0f, -2.0f, 1.0f, 2, true);

        auto dirtyHole = p.getBandState(1);
        dirtyHole.enabled = false;
        dirtyHole.solo = true;
        dirtyHole.dynMode = 1;
        dirtyHole.slope = 2;
        dirtyHole.curveMode = kLegacy;
        p.setBandState(1, dirtyHole);
        expect(p.getBandState(1).dynMode == 1 && p.getBandState(1).solo, "setup: slot 1 carries old state");

        const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("punch"));

        expect(r.committed);
        if (r.slots.size() == 1) expectEquals(r.slots[0], 1);
        const auto b = p.getBandState(1);
        expectEquals(b.dynMode, 0, "dynamic mode reset");
        expect(! b.solo, "solo reset");
        expectEquals(b.slope, 0, "slope reset");
        expectEquals(b.curveMode, kSurgical, "curve mode reset to the default");
        expectEquals(b.type, 2);
        expectWithinAbsoluteError(b.frequency, 95.0f, 0.5f);
        expectWithinAbsoluteError(b.gain, 1.7f, 0.01f);
    }

    void testTwoGhostsGrowTheCountContiguously()
    {
        beginTest("Two ghosts: hole first, then the next slot past the count");

        P p; p.prepareToPlay(48000.0, 512);
        seedFourLive(p);
        p.setBandGeometry(2, 3500.0f, 2.0f, 1.0f, 2, false);  // open a hole at 2
        const auto before = snapshot(p);

        const auto ghosts = EmberPhrase::parse("warmer and more punch");
        expectEquals((int) ghosts.size(), 2, "setup: two ghosts");

        const auto r = EmberApply::commitGhosts(p, ghosts);

        expect(r.committed);
        expectEquals((int) r.slots.size(), 2);
        if (r.slots.size() == 2)
        {
            expectEquals(r.slots[0], 2);
            expectEquals(r.slots[1], 4);
        }
        expectEquals(EmberApply::readLiveCount(p), 5);
        expectUntouched(before, p, 0, 1, "live bands");
        expectUntouched(before, p, 3, 3, "live band");
    }

    void testNoRoomMeansNothingIsWritten()
    {
        beginTest("Not enough free slots: nothing is written, the count does not move");

        {
            P p; p.prepareToPlay(48000.0, 512);
            EmberApply::writeLiveCount(p, P::maxBands);
            for (int i = 0; i < P::maxBands; ++i)
                p.setBandGeometry(i, 40.0f * (float) (i + 1), 0.5f, 1.0f, 2, true);
            const auto before = snapshot(p);

            const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("punch"));

            expect(! r.committed, "24 live bands: must refuse");
            expect(r.slots.empty());
            expectEquals(EmberApply::readLiveCount(p), P::maxBands);
            expectUntouched(before, p, 0, P::maxBands - 1, "full EQ");
        }
        {
            P p; p.prepareToPlay(48000.0, 512);
            EmberApply::writeLiveCount(p, P::maxBands - 1);
            for (int i = 0; i < P::maxBands - 1; ++i)
                p.setBandGeometry(i, 40.0f * (float) (i + 1), 0.5f, 1.0f, 2, true);
            const auto before = snapshot(p);

            const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("warmer and more punch"));

            expect(! r.committed, "one free slot for two ghosts: must refuse, not half-apply");
            expectEquals(EmberApply::readLiveCount(p), P::maxBands - 1);
            expectUntouched(before, p, 0, P::maxBands - 1, "nearly full EQ");
        }
    }

    void testPreviousBandsStillSound()
    {
        beginTest("After Apply the engine plays the old bands and the new one together");

        P p; p.prepareToPlay(48000.0, 512);
        EmberApply::writeLiveCount(p, 2);
        p.setBandGeometry(0, 120.0f, 4.0f, 1.0f, 2, true);
        p.setBandGeometry(1, 8000.0f, -3.0f, 1.0f, 2, true);
        pump(p, 60);
        expectWithinAbsoluteError(appliedDbAt(p, 120.0f), 4.0f, 0.3f, "setup @120 Hz");

        const auto r = EmberApply::commitGhosts(p, EmberPhrase::parse("add a peak at 2khz with -4db gain and 2.5 q."));
        expect(r.committed);
        pump(p, 60);

        expectWithinAbsoluteError(appliedDbAt(p, 120.0f), 4.0f, 0.3f, "old band still sounds @120 Hz");
        expectWithinAbsoluteError(appliedDbAt(p, 8000.0f), -3.0f, 0.3f, "old band still sounds @8 kHz");
        expectWithinAbsoluteError(appliedDbAt(p, 2000.0f), -4.0f, 0.3f, "new band sounds @2 kHz");
    }

    void testQuantisation()
    {
        beginTest("Written values are the APVTS-quantised ghost");

        P p; p.prepareToPlay(48000.0, 512);
        seedFourLive(p);

        EmberGhostBand g;
        g.type = 2; g.hz = 1999.6f; g.db = -4.04f; g.q = 2.504f;
        const auto r = EmberApply::commitGhosts(p, { g });

        expect(r.committed);
        if (r.slots.size() == 1)
        {
            const auto b = p.getBandState(r.slots[0]);
            expectWithinAbsoluteError(b.frequency, 2000.0f, 0.5f);
            expectWithinAbsoluteError(b.gain, -4.0f, 0.01f);
            expectWithinAbsoluteError(b.q, 2.50f, 0.005f);
        }
    }
};

static EmberApplyAddTest emberApplyAddTest;

#endif
