/*
 * FA-001 containment witness.
 *
 * The full audit reported an ownership/concurrency race in CaptureService. For
 * the first beta the decision was containment rather than a fifth concurrent
 * redesign: capture is an accessory feature, so the service is simply never
 * armed and the reported race has no way to occur in the product.
 *
 * Containment is only worth anything if it is checked. A constant that someone
 * flips back, or a fourth entry point added later that forgets the gate, would
 * silently re-expose the finding. This test asserts the behaviour at every
 * arming path rather than asserting the value of the flag, so it keeps holding
 * if the implementation changes shape.
 *
 * It is deliberately NOT a claim that FA-001 is fixed. When the race is
 * genuinely resolved, kCaptureEnabledForShipping goes true and this test is
 * expected to fail — that failure is the reminder to replace it with a real
 * concurrency witness.
 */
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <cmath>

class AICaptureDisabledWitnessTest : public juce::UnitTest
{
public:
    AICaptureDisabledWitnessTest()
        : juce::UnitTest("AIEqualizer Capture containment (FA-001)", "Integration") {}

    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;

    static void pump(AIEqualizerAudioProcessor& proc, double& phase, int blocks)
    {
        juce::MidiBuffer midi;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 440.0 / kSampleRate;
        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buffer(2, kBlockSize);
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float s = 0.25f * static_cast<float>(std::sin(phase));
                phase += inc;
                if (phase >= 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
            }
            proc.processBlock(buffer, midi);
        }
    }

    void runTest() override
    {
        beginTest("No arming path can put the processor into a capturing state");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);

        double phase = 0.0;
        pump(proc, phase, 40);

        expect(! proc.isCapturing(),
               "Processor reported capturing before anything asked it to.");

        // 1. The manual button path, which is the one the UI drives.
        expect(! proc.startManualCapture(),
               "startManualCapture() succeeded while capture is contained for FA-001.");
        expect(! proc.isCapturing(),
               "startManualCapture() left the service armed even though it returned false.");

        // 2. The retroactive path. Gating only the button would leave this open.
        proc.captureAudioSnapshotMs(500);
        expect(! proc.isCapturing(),
               "captureAudioSnapshotMs() armed the service.");

        // 3. The analysis path, which is what actually starts a worker thread.
        expect(! proc.analyzeCapturedAudioSnapshot(),
               "analyzeCapturedAudioSnapshot() ran while capture is contained.");

        pump(proc, phase, 40);
        expect(! proc.isCapturing(),
               "The service armed itself during ordinary audio processing.");

        beginTest("Containment does not disturb ordinary processing");

        // Capture being off must cost the audio path nothing: the ring is still
        // fed by processBlock, and that has to stay harmless.
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float s = 0.2f * static_cast<float>(std::sin(0.05 * i));
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        proc.processBlock(buffer, midi);

        bool finite = true;
        for (int ch = 0; ch < buffer.getNumChannels() && finite; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite(buffer.getSample(ch, i))) { finite = false; break; }

        expect(finite, "Output went non-finite with capture contained.");

        beginTest("Lifecycle with capture contained leaves no worker behind");

        proc.releaseResources();
        expect(! proc.isCapturing(), "Still capturing after releaseResources().");
        expect(! proc.startManualCapture(),
               "startManualCapture() succeeded after releaseResources().");
    }
};

static AICaptureDisabledWitnessTest aiCaptureDisabledWitnessTest;
