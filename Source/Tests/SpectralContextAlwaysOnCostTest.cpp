#if JUCE_UNIT_TESTS

/*
 * T5.2a - what does an always-on Semantic analysis consumer actually cost?
 *
 * T5.2 registers Semantic from construction so that PLAN finds a mature context
 * instead of having to listen first. That is the better UX, but it also means
 * the front-end can run for the whole life of an instance even if the user
 * never opens Semantic. With thirty instances in a session the difference
 * between 0.05% and 3% of a core stops being academic, so the policy should
 * rest on a measurement rather than on the intuition that it is probably fine.
 *
 * Two costs, deliberately reported separately because they are paid in
 * different places:
 *
 *   audio thread  - only the FIFO push. This is the one with a deadline.
 *   AI thread     - the actual front-end work: mean frame cost times frame
 *                   rate, expressed as a fraction of one core.
 *
 * Assist is OFF throughout, so what is measured is the cost the Semantic
 * consumer adds on its own, which is exactly the decision at hand.
 *
 * WHAT THIS CAN AND CANNOT RESOLVE, because it matters for how the numbers are
 * read: on a loaded developer machine the per-block deltas are noise-dominated.
 * Repeated runs have produced deltas from -15 us to +12 us for identical work,
 * and the same arm has read 29 us and 45 us on consecutive runs. Individual
 * readings therefore mean nothing; only the order of magnitude does, and the
 * stable readings (larger blocks, more frames) are the ones worth quoting.
 * Anyone tempted to treat a single delta here as a regression signal should
 * run it several times first and look at the spread.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
using Consumer = AIEqualizerAudioProcessor::AnalysisConsumer;

struct Measurement
{
    double medianBlockUs = 0.0;
    double p95BlockUs = 0.0;
    double pfeMeanMs = 0.0;
    double pfeMaxMs = 0.0;
    juce::int64 frames = 0;
};

Measurement measure(bool semanticConsumer, double sr, int blockSize, int blocks)
{
    AIEqualizerAudioProcessor proc;
    proc.prepareToPlay(sr, blockSize);

    // Assist off in BOTH arms: the question is what Semantic adds by itself.
    if (auto* enabled = proc.getAPVTS().getRawParameterValue("aiEnabled"))
        enabled->store(0.0f);
    proc.setAnalysisConsumer(Consumer::Semantic, semanticConsumer);
    proc.setAnalysisConsumer(Consumer::Match, false);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf(2, blockSize);
    juce::Random rng(1234);
    double phase = 0.0;

    std::vector<double> samplesUs;
    samplesUs.reserve(static_cast<size_t>(blocks));

    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float s = 0.25f * (0.8f * static_cast<float>(std::sin(phase))
                                   + 0.2f * (rng.nextFloat() * 2.0f - 1.0f));
            phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / sr;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }

        const auto t0 = juce::Time::getHighResolutionTicks();
        proc.processBlock(buf, midi);
        const auto t1 = juce::Time::getHighResolutionTicks();

        // Pace the producer OUTSIDE the timed region. Without this the FIFO
        // overruns continuously, every overrun raises the discontinuity flag,
        // and the worker keeps resetting the front-end instead of running it -
        // which reports a background cost of zero for the entirely wrong
        // reason. The sleep is not inside the measurement, so it does not
        // affect the audio-thread figure.
        if ((b % 4) == 3)
            juce::Thread::sleep(2);

        // Discard a warm-up prefix: first blocks pay one-time costs that do not
        // belong to a steady-state figure.
        if (b >= blocks / 5)
            samplesUs.push_back(juce::Time::highResolutionTicksToSeconds(t1 - t0) * 1.0e6);
    }

    juce::Thread::sleep(400); // let the AI thread finish what it was given

    Measurement m;
    if (!samplesUs.empty())
    {
        std::sort(samplesUs.begin(), samplesUs.end());
        m.medianBlockUs = samplesUs[samplesUs.size() / 2];
        m.p95BlockUs = samplesUs[static_cast<size_t>(0.95 * (double) (samplesUs.size() - 1))];
    }
    const auto diag = proc.getAIFrontEndDiagnostics();
    m.pfeMeanMs = diag.meanMs;
    m.pfeMaxMs = diag.maxMs;
    m.frames = diag.frames;
    return m;
}
}

class SpectralContextAlwaysOnCostTest final : public juce::UnitTest
{
public:
    SpectralContextAlwaysOnCostTest()
        : juce::UnitTest("Spectral Context Always-On Cost (T5.2a)", "Performance") {}

    void runTest() override
    {
        beginTest("Cost of an always-on Semantic analysis consumer, Assist OFF");

        double worstAudioDeltaUs = 0.0;
        double worstCorePercent = 0.0;

        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            for (int block : { 64, 128, 256, 512 })
            {
                // Roughly two seconds of audio at each setting.
                // Fewer blocks than a raw burst, but paced, so the worker actually runs.
                const int blocks = std::max(400, static_cast<int>(1.5 * sr / block));

                const auto off = measure(false, sr, block, blocks);
                const auto on  = measure(true,  sr, block, blocks);

                const double deltaUs = on.medianBlockUs - off.medianBlockUs;
                const double blockPeriodUs = 1.0e6 * block / sr;
                const double audioPercent = 100.0 * deltaUs / blockPeriodUs;

                // Background cost: mean frame cost times the frame rate the hop
                // size implies, as a share of one core.
                const double framesPerSec = sr / static_cast<double>(PerceptualFrontEnd::kHopSize);
                const double corePercent = 100.0 * (on.pfeMeanMs * 1.0e-3) * framesPerSec;

                worstAudioDeltaUs = std::max(worstAudioDeltaUs, deltaUs);
                worstCorePercent = std::max(worstCorePercent, corePercent);

                logMessage("  " + juce::String(sr / 1000.0, 1) + " kHz / " + juce::String(block)
                    + " smp | processBlock median off=" + juce::String(off.medianBlockUs, 2)
                    + " on=" + juce::String(on.medianBlockUs, 2)
                    + " us (delta " + juce::String(deltaUs, 2) + " us = "
                    + juce::String(audioPercent, 3) + "% of the block period)");
                logMessage("      PFE frame mean=" + juce::String(on.pfeMeanMs, 3)
                    + " ms max=" + juce::String(on.pfeMaxMs, 3)
                    + " ms  frames=" + juce::String(on.frames)
                    + "  -> background " + juce::String(corePercent, 3) + "% of one core");
            }
        }

        logMessage("  WORST audio-thread delta: " + juce::String(worstAudioDeltaUs, 2) + " us/block");
        logMessage("  WORST background cost:    " + juce::String(worstCorePercent, 3) + "% of one core");

        // Deliberately generous bounds. These are not the acceptance criteria for
        // the policy - that is a product call on the numbers above. They exist so
        // that a future change which makes the always-on path an order of
        // magnitude more expensive fails here instead of being discovered in a
        // session with thirty instances.
        expect(worstAudioDeltaUs < 50.0,
               "the Semantic consumer adds more than 50 us per block on the audio "
               "thread; it is supposed to add only a FIFO push");
        expect(worstCorePercent < 10.0,
               "the always-on front-end costs more than 10% of a core per instance");
    }
};

static SpectralContextAlwaysOnCostTest sSpectralContextAlwaysOnCostTest;

#endif // JUCE_UNIT_TESTS
