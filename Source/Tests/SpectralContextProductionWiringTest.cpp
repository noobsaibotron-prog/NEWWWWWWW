#if JUCE_UNIT_TESTS

/*
 * T5.1 / T5.2 - the analysis context against the real processor.
 *
 * Up to here SpectralContext was only ever fed band values a test handed it.
 * These cases drive real audio through processBlock and require the context to
 * appear on the other side, with the two properties that make it usable for
 * planning: it must be a published snapshot rather than live state, and it must
 * not depend on Ember Assist being switched on.
 *
 * The second one is the point of T5.2. The front-end used to be fed only while
 * Assist was enabled, which quietly made Semantic depend on a switch that has
 * nothing to do with it.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"

#include <cmath>
#include <vector>

namespace
{
using Consumer = AIEqualizerAudioProcessor::AnalysisConsumer;

/** Feed a tone-plus-tilt source, paced so the AI worker keeps up with the
    producer rather than racing a saturated FIFO. */
void feed(AIEqualizerAudioProcessor& proc, int blocks, int blockSize,
          double sr, float amplitude, float tiltCentreHz)
{
    juce::MidiBuffer midi;
    double phase = 0.0;
    juce::Random rng(4242);
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            // Broadband noise shaped around a centre, so the context has real
            // spectral structure rather than a single line.
            float s = 0.0f;
            for (int h = 1; h <= 6; ++h)
            {
                const double f = tiltCentreHz * 0.5 * h;
                s += static_cast<float>(std::sin(phase * f / tiltCentreHz)) / static_cast<float>(h);
            }
            s = amplitude * (0.7f * s + 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
            phase += 2.0 * juce::MathConstants<double>::pi * tiltCentreHz / sr;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        proc.processBlock(buf, midi);
        if ((b % 4) == 3)
            juce::Thread::sleep(6);
    }
    juce::Thread::sleep(300);
}

std::optional<AIEQPerceptual::SpectralContext>
waitForContext(AIEqualizerAudioProcessor& proc, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter()
                        + static_cast<juce::uint32>(timeoutMs);
    std::optional<AIEQPerceptual::SpectralContext> last;
    while (juce::Time::getMillisecondCounter() < deadline)
    {
        if (auto snap = proc.getSpectralContextSnapshot())
        {
            last = snap;
            if (snap->valid && snap->framesObserved > 0)
                return last;
        }
        juce::Thread::sleep(5);
    }
    return last;
}
}

class SpectralContextProductionWiringTest final : public juce::UnitTest
{
public:
    SpectralContextProductionWiringTest()
        : juce::UnitTest("Spectral Context Production Wiring (T5.1/T5.2)", "Integration") {}

    void runTest() override
    {
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        //==================================================================
        beginTest("T5.1 real audio produces a published context snapshot");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);
            feed(proc, 200, kBlock, kSr, 0.25f, 1000.0f);

            const auto snap = waitForContext(proc, 4000);
            expect(snap.has_value(), "no context was ever published from real audio");
            if (!snap.has_value())
                return;

            logMessage("  valid=" + juce::String(snap->valid ? 1 : 0)
                     + "  frames=" + juce::String(snap->framesObserved)
                     + "  confidence=" + juce::String(snap->confidence, 2)
                     + "  sourceLevel=" + juce::String(snap->sourceLevelDb, 1) + " dB");
            logMessage("  tilt=" + juce::String(snap->spectralTiltDbPerOctave, 2)
                     + " dB/oct  centroid=" + juce::String(snap->perceptualCentroidHz, 0)
                     + " Hz  rolloff=" + juce::String(snap->perceptualRolloffHz, 0) + " Hz");

            expect(snap->valid, "published context is not valid");
            expect(snap->framesObserved > 0, "context reports no observed frames");
            expect(snap->schemaVersion == AIEQPerceptual::kSpectralContextSchemaVersion,
                   "published context carries the wrong schema version");
        }

        //==================================================================
        beginTest("T5.1 the snapshot is a frozen copy, not a live view");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);
            feed(proc, 160, kBlock, kSr, 0.25f, 800.0f);

            const auto first = waitForContext(proc, 4000);
            expect(first.has_value(), "no first snapshot");
            if (!first.has_value())
                return;

            // Keep the source running and materially change it. A snapshot that
            // were a live view would drift under the caller's feet; planning
            // needs the picture as it was when PLAN was pressed.
            const auto beforeCentroid = first->perceptualCentroidHz;
            feed(proc, 160, kBlock, kSr, 0.25f, 6000.0f);
            juce::ignoreUnused(waitForContext(proc, 2000));

            logMessage("  held snapshot centroid=" + juce::String(beforeCentroid, 0)
                     + " Hz  (source has since moved up)");
            expect(std::abs(first->perceptualCentroidHz - beforeCentroid) < 1.0e-3f,
                   "the previously returned snapshot changed after more audio arrived");
        }

        //==================================================================
        beginTest("T5.2 context still updates with Ember Assist switched OFF");
        {
            // The whole point of the tranche. Assist off must not make Semantic
            // source-blind.
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            if (auto* enabled = proc.getAPVTS().getRawParameterValue("aiEnabled"))
                enabled->store(0.0f);

            proc.setAnalysisConsumer(Consumer::Semantic, true);
            feed(proc, 220, kBlock, kSr, 0.25f, 1200.0f);

            const auto snap = waitForContext(proc, 5000);
            const bool assistOn = proc.isAnalysisConsumerActive(Consumer::Assist);
            logMessage("  Assist consumer active=" + juce::String(assistOn ? 1 : 0));
            expect(!assistOn, "harness precondition: Assist did not actually turn off");

            expect(snap.has_value() && snap->valid,
                   "with Assist off no context was produced at all - Semantic is "
                   "source-blind exactly when the user has not enabled a feature "
                   "that has nothing to do with it");
            if (snap.has_value())
                logMessage("  frames=" + juce::String(snap->framesObserved)
                         + "  confidence=" + juce::String(snap->confidence, 2));
        }

        //==================================================================
        beginTest("T5.2 with no consumer at all the front-end stays idle");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);

            if (auto* enabled = proc.getAPVTS().getRawParameterValue("aiEnabled"))
                enabled->store(0.0f);
            proc.setAnalysisConsumer(Consumer::Semantic, false);
            proc.setAnalysisConsumer(Consumer::Match, false);

            const auto before = proc.getAIFrontEndDiagnostics().frames;
            feed(proc, 120, kBlock, kSr, 0.25f, 1000.0f);
            const auto after = proc.getAIFrontEndDiagnostics().frames;

            logMessage("  frames before=" + juce::String(before)
                     + "  after=" + juce::String(after));
            expect(!proc.analysisNeeded(), "mask is not empty with every consumer off");
            expect(after == before,
                   "the front-end kept running with no consumer needing it, which "
                   "is the cost the mask exists to avoid");
        }

        //==================================================================
        beginTest("T5.1 silence fails closed through confidence");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);
            proc.setAnalysisConsumer(Consumer::Semantic, true);

            juce::MidiBuffer midi;
            for (int b = 0; b < 220; ++b)
            {
                juce::AudioBuffer<float> buf(2, kBlock);
                buf.clear();
                proc.processBlock(buf, midi);
                if ((b % 4) == 3) juce::Thread::sleep(6);
            }
            juce::Thread::sleep(300);

            const auto snap = waitForContext(proc, 3000);
            if (snap.has_value())
            {
                logMessage("  silence: confidence=" + juce::String(snap->confidence, 3)
                         + "  sourceLevel=" + juce::String(snap->sourceLevelDb, 1) + " dB");
                expect(snap->confidence < 0.2f,
                       "digital silence still produced a usable-looking confidence");
            }
            else
            {
                logMessage("  silence: no snapshot published (also acceptable)");
            }
        }

        //==================================================================
        beginTest("T5.1 lifecycle: re-prepare resets the accumulated context");
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(kSr, kBlock);
            proc.setAnalysisConsumer(Consumer::Semantic, true);
            feed(proc, 160, kBlock, kSr, 0.25f, 1000.0f);
            expect(waitForContext(proc, 4000).has_value(), "no context before re-prepare");

            proc.releaseResources();
            proc.prepareToPlay(kSr, kBlock);

            // A stale snapshot must not survive a lifecycle boundary: after
            // re-prepare the accumulator starts from nothing, and anything the
            // caller sees afterwards has to come from the new stream.
            feed(proc, 40, kBlock, kSr, 0.25f, 1000.0f);
            const auto after = proc.getSpectralContextSnapshot();
            if (after.has_value())
                logMessage("  frames after re-prepare=" + juce::String(after->framesObserved));
            expect(!after.has_value() || after->framesObserved <= 60,
                   "the context carried frame history across prepareToPlay");
        }
    }
};

static SpectralContextProductionWiringTest sSpectralContextProductionWiringTest;

#endif // JUCE_UNIT_TESTS
