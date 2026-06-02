#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>

/**
 * Regression tests for live (real-time) propagation of the AI "Sensitivity" and
 * "Strength" knobs to the AI engine while audio is playing.
 *
 * Bug being guarded (fixed in d35360a4):
 *   `aiSensitivity` / `aiStrength` were never registered as APVTS parameter
 *   listeners, so parameterChanged() never fired for them. During steady
 *   playback (no other parameter being touched) parametersNeedUpdate stayed
 *   false, updateEQFromParameters() was not called, and setSensitivity()/
 *   setStrength() never pushed the new value to AIEngine. The knob looked
 *   "dead": amber problem bars and the AI problem panel did not update.
 *
 * These tests reproduce that exact scenario at the processor level:
 *   - change ONLY the AI knob (nothing else),
 *   - process a single audio block (simulating one buffer of playback),
 *   - assert the value reached AIEngine via getSensitivity()/getStrength().
 *
 * On the pre-fix code these assertions FAIL (the engine keeps the old value);
 * after the fix they PASS, proving the knob is live on the very next block.
 */
class SensitivityKnobLiveTest : public juce::UnitTest
{
public:
    SensitivityKnobLiveTest()
        : juce::UnitTest("AI Sensitivity Knob Live", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm); // current thread becomes the message thread

        testSensitivityReachesEngineNextBlock();
        testStrengthReachesEngineNextBlock();
        testRepeatedLiveTweaksTrackKnob();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int    kBlockSize  = 512;

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    // Fill a stereo buffer with a quiet sine so the blocks resemble real
    // playback (the propagation does not depend on signal content, but this
    // mirrors Marco's "while audio plays" scenario).
    static void fillPlaybackBuffer(juce::AudioBuffer<float>& buf, double& phase)
    {
        const double inc = 2.0 * juce::MathConstants<double>::pi * 220.0 / kSampleRate;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float s = 0.1f * static_cast<float>(std::sin(phase));
            phase += inc;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.setSample(ch, i, s);
        }
    }

    // Process one buffer of "playback".
    static void pumpOneBlock(AIEqualizerAudioProcessor& proc, double& phase)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;
        fillPlaybackBuffer(buffer, phase);
        proc.processBlock(buffer, midi);
    }

    // Run a few blocks so any update pending from construction/prepareToPlay is
    // flushed and the processor reaches steady state (parametersNeedUpdate clear).
    static void settle(AIEqualizerAudioProcessor& proc, double& phase)
    {
        for (int b = 0; b < 4; ++b)
            pumpOneBlock(proc, phase);
    }

    void testSensitivityReachesEngineNextBlock()
    {
        beginTest("Sensitivity knob reaches AI engine on the next block");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        double phase = 0.0;

        settle(proc, phase);

        // After settling the engine should hold the parameter default (0.5).
        expectWithinAbsoluteError(proc.getAIEngine().getSensitivity(), 0.5f, 1.0e-3f,
            "Baseline sensitivity should equal the parameter default after settling");

        // Each tweak changes ONLY aiSensitivity (steady playback, nothing else
        // touched) and must reach the engine after a single processed block.
        for (const float target : { 0.20f, 0.85f, 0.40f })
        {
            setFloat(apvts, "aiSensitivity", target);
            pumpOneBlock(proc, phase);

            expectWithinAbsoluteError(proc.getAIEngine().getSensitivity(), target, 1.0e-3f,
                "Sensitivity " + juce::String(target)
                    + " did not reach the engine within one block (knob is dead)");
        }
    }

    void testStrengthReachesEngineNextBlock()
    {
        beginTest("Strength knob reaches AI engine on the next block");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        double phase = 0.0;

        settle(proc, phase);

        expectWithinAbsoluteError(proc.getAIEngine().getStrength(), 0.7f, 1.0e-3f,
            "Baseline strength should equal the parameter default after settling");

        for (const float target : { 0.30f, 0.95f, 0.55f })
        {
            setFloat(apvts, "aiStrength", target);
            pumpOneBlock(proc, phase);

            expectWithinAbsoluteError(proc.getAIEngine().getStrength(), target, 1.0e-3f,
                "Strength " + juce::String(target)
                    + " did not reach the engine within one block");
        }
    }

    void testRepeatedLiveTweaksTrackKnob()
    {
        beginTest("Sensitivity tracks a continuous knob sweep during playback");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();
        double phase = 0.0;

        settle(proc, phase);

        // Simulate dragging the knob across its range while audio keeps playing:
        // every step must be reflected on the engine, monotonically.
        for (int step = 0; step <= 10; ++step)
        {
            const float target = static_cast<float>(step) / 10.0f; // 0.0 .. 1.0
            setFloat(apvts, "aiSensitivity", target);
            pumpOneBlock(proc, phase);

            expectWithinAbsoluteError(proc.getAIEngine().getSensitivity(), target, 1.0e-3f,
                "Sweep step " + juce::String(target) + " not reflected on the engine");
        }
    }
};

static SensitivityKnobLiveTest sensitivityKnobLiveTest;
