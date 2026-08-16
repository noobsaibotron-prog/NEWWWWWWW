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

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

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

        beginTest("No audio reaches the capture ring while contained");

        // Arming is only half the contract. If processBlock still fed the
        // retroactive ring there would be a live writer with no reader, which
        // costs a per-block copy for a feature that cannot be used and leaves
        // the FA-001 surface warm. The preview is the observable end of that
        // ring, so an empty preview after real audio is the proof.
        {
            AIEqualizerAudioProcessor fresh;
            fresh.prepareToPlay(kSampleRate, kBlockSize);
            double p2 = 0.0;
            pump(fresh, p2, 200);

            std::vector<float> preview;
            fresh.getManualCapturePreview(preview, 8192);

            const bool anyNonZero =
                std::any_of(preview.begin(), preview.end(),
                            [](float v) { return std::abs(v) > 1.0e-9f; });

            logMessage("  preview samples after 200 blocks: "
                       + juce::String(static_cast<int>(preview.size()))
                       + ", any non-zero: " + (anyNonZero ? "yes" : "no"));
            expect(! anyNonZero,
                   "Audio reached the capture ring while capture is contained: the "
                   "shipping path is still paying for a feature it cannot use.");
        }

        beginTest("Hammering every capture API during live audio never arms it");

        // The FA-001 race needs a reader running against the audio writer. This
        // drives every public entry point as hard as it can while audio flows,
        // which is the shape that would expose it, and asserts the service never
        // becomes active. Under TSan this doubles as the containment witness.
        {
            AIEqualizerAudioProcessor fresh;
            fresh.prepareToPlay(kSampleRate, kBlockSize);

            std::atomic<bool> stop { false };
            std::atomic<int> observedCapturing { 0 };

            std::thread hammer([&]
            {
                while (! stop.load(std::memory_order_acquire))
                {
                    if (fresh.startManualCapture())
                        observedCapturing.fetch_add(1, std::memory_order_relaxed);
                    fresh.captureAudioSnapshotMs(120);
                    if (fresh.analyzeCapturedAudioSnapshot())
                        observedCapturing.fetch_add(1, std::memory_order_relaxed);
                    if (fresh.isCapturing())
                        observedCapturing.fetch_add(1, std::memory_order_relaxed);
                    fresh.stopManualCapture();

                    std::vector<float> preview;
                    fresh.getManualCapturePreview(preview, 1024);
                    std::this_thread::yield();
                }
            });

            double p3 = 0.0;
            pump(fresh, p3, 300);
            stop.store(true, std::memory_order_release);
            hammer.join();

            expectEquals(observedCapturing.load(std::memory_order_relaxed), 0,
                         "A capture entry point succeeded under concurrent hammering.");
            expect(! fresh.isCapturing(), "Left capturing after the hammer stopped.");
        }

        beginTest("Lifecycle with capture contained leaves no worker behind");

        proc.releaseResources();
        expect(! proc.isCapturing(), "Still capturing after releaseResources().");
        expect(! proc.startManualCapture(),
               "startManualCapture() succeeded after releaseResources().");
    }
};

static AICaptureDisabledWitnessTest aiCaptureDisabledWitnessTest;
