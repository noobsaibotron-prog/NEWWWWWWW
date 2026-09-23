#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberApplyCommit.h"
#include "../GUI/Ember/EmberPhraseParser.h"

#include <cmath>
#include <memory>

/*
 * When the host lowers "Number of Bands", the bands past the new count stop
 * sounding and leave the curve.
 *
 * Seen by Claude in Ableton on 2026-09-22 (build a0033784): after a Specchio
 * Apply grew the count from 8 to 9, Edit > Undo "Cambia Number of Bands" put
 * the count back to 8 but the 2 kHz band stayed on the graph. Reproduced here
 * on 2026-09-23: after the host set the count back, the node left the graph
 * (it reads the count) while the EQ curve AND the audio kept the -4 dB band.
 *
 * Cause: updateEQFromParameters() folds the count into targetBandEnabled for
 * every band, but applySmoothedBandParams() only applies bands below the
 * effective count, so a band that falls out of the count keeps its last
 * enabled state in the four EQ processors. setNumActiveBands(), the direct
 * call Apply uses, disables them; the parameter path did not.
 *
 * The host is modelled the way JUCE's VST3 wrapper sets a parameter outside
 * playback: setValue, then sendValueChangedMessageToListeners.
 *
 * NOT covered: the processor-side state restore (setStateInformation), the
 * projected-B DSP source, and Ableton's own undo grouping.
 */
namespace
{
using P = AIEqualizerAudioProcessor;
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

void pump(P& p, int blocks)
{
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        p.processBlock(buf, midi);
    }
}

float curveDbAt(P& p, float hz)
{
    float f = hz, m = 1.0f;
    p.getEQProcessor().getMagnitudeForFrequencyArray(&f, &m, 1, p.getSampleRate());
    return juce::Decibels::gainToDecibels(juce::jmax(1.0e-6f, m), -96.0f);
}

/** Steady-state gain of a sine at `hz` through processBlock, left channel, in dB. */
float audioDbAt(P& p, double hz)
{
    juce::AudioBuffer<float> buf(2, kBlock);
    juce::MidiBuffer midi;
    double phase = 0.0, inEnergy = 0.0, outEnergy = 0.0;
    const double step = juce::MathConstants<double>::twoPi * hz / kSr;
    for (int b = 0; b < 120; ++b)
    {
        const bool measure = b >= 80;
        for (int i = 0; i < kBlock; ++i)
        {
            const float s = 0.25f * (float) std::sin(phase);
            phase += step;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
            if (measure) inEnergy += (double) s * s;
        }
        p.processBlock(buf, midi);
        if (measure)
            for (int i = 0; i < kBlock; ++i)
                outEnergy += (double) buf.getSample(0, i) * buf.getSample(0, i);
    }
    return (float) (10.0 * std::log10(outEnergy / inEnergy));
}

/** JUCE's VST3 wrapper, host sets a parameter outside playback. */
void hostSetsCount(P& p, int count)
{
    auto* param = p.getAPVTS().getParameter("numActiveBands");
    const float normalised = param->convertTo0to1((float) (count - 1));
    param->setValue(normalised);
    param->sendValueChangedMessageToListeners(normalised);
}
} // namespace

class HostBandCountShrinkTest : public juce::UnitTest
{
public:
    HostBandCountShrinkTest() : juce::UnitTest("Host band count shrink", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        auto owner = std::make_unique<P>();
        auto& p = *owner;
        p.prepareToPlay(kSr, kBlock);
        pump(p, 4);

        beginTest("a Specchio Apply grows the count and the band sounds");
        const float freshCurve2k = curveDbAt(p, 2000.0f);
        const float freshAudio2k = audioDbAt(p, 2000.0);
        const auto r = EmberApply::commitGhosts(
            p, EmberPhrase::parse("add a peak at 2khz with -4db gain and 2.5 q"));
        pump(p, 4);
        expect(r.committed, "Apply commits");
        expectEquals(r.newLiveCount, r.previousLiveCount + 1, "Apply adds one band");
        expectWithinAbsoluteError(curveDbAt(p, 2000.0f), freshCurve2k - 4.0f, 0.05f, "curve after Apply");
        expectWithinAbsoluteError(audioDbAt(p, 2000.0), freshAudio2k - 4.0f, 0.05f, "audio after Apply");

        beginTest("the host puts the count back: the band past it leaves curve and audio");
        hostSetsCount(p, r.previousLiveCount);
        pump(p, 4);
        expectEquals(p.getNumActiveBands(), r.previousLiveCount, "processor count follows the host");
        expectWithinAbsoluteError(curveDbAt(p, 2000.0f), freshCurve2k, 0.05f,
                                  "curve must drop the band past the count");
        expectWithinAbsoluteError(audioDbAt(p, 2000.0), freshAudio2k, 0.05f,
                                  "audio must drop the band past the count");

        beginTest("the host raises the count again: the band sounds again");
        hostSetsCount(p, r.newLiveCount);
        pump(p, 4);
        expectEquals(p.getNumActiveBands(), r.newLiveCount, "processor count follows the host");
        expectWithinAbsoluteError(curveDbAt(p, 2000.0f), freshCurve2k - 4.0f, 0.05f, "curve after redo");
        expectWithinAbsoluteError(audioDbAt(p, 2000.0), freshAudio2k - 4.0f, 0.05f, "audio after redo");
    }
};

static HostBandCountShrinkTest hostBandCountShrinkTest;

#endif
