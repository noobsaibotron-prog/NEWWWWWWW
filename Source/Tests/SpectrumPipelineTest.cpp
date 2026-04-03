#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/LockFreeAudioFIFO.h"
#include "../GUI/NewSpectrumPipeline.h"
#include <cmath>

/**
 * Spectrum Pipeline Tests — validates the staging buffer fix for the
 * partial-pull sample loss bug that caused spectrum freezes.
 *
 * Root cause: LockFreeAudioFIFO::pullAudioBlock() consumes partial data
 * when fewer than hopSize samples are available. Without staging, those
 * samples were silently discarded.
 *
 * The fix adds a staging buffer in the consumer that accumulates partial
 * pulls until a full hop is available.
 */
class SpectrumPipelineTest : public juce::UnitTest
{
public:
    SpectrumPipelineTest() : juce::UnitTest("Spectrum Pipeline", "Integration") {}

    void runTest() override
    {
        testPartialPullNoSampleLoss();
        testFragmentedInputProducesHops();
        testBacklogCatchUp();
        testStatsInstrumentation();
    }

private:

    /** Push N samples of a 1kHz sine into a FIFO. */
    static void pushSine (LockFreeAudioFIFO<float>& fifo, int numSamples,
                           double sampleRate, int numChannels)
    {
        // pushStereoMix expects an AudioBuffer
        juce::AudioBuffer<float> buf (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                buf.setSample (ch, s, 0.5f * std::sin (
                    static_cast<float> (2.0 * juce::MathConstants<double>::pi *
                                        1000.0 * s / sampleRate)));
            }
        }
        fifo.pushStereoMix (buf);
    }

    // ── Test 1: Partial pull no longer loses samples ─────────────────────
    void testPartialPullNoSampleLoss()
    {
        beginTest("Partial FIFO pull does not lose samples (staging accumulates)");

        constexpr double sr = 48000.0;
        constexpr size_t fftOrder = 11;  // 2048-point FFT, hopSize = 1024
        constexpr size_t hopSize = 1024;

        LockFreeAudioFIFO<float> preFIFO, postFIFO;
        preFIFO.prepare (65536);
        postFIFO.prepare (65536);

        NewSpectrumPipeline pipeline (preFIFO, postFIFO, fftOrder, sr);

        // Push LESS than one hop (e.g. 500 samples) — this is the partial pull scenario
        pushSine (preFIFO, 500, sr, 1);
        pushSine (postFIFO, 500, sr, 1);

        // First tick: should NOT produce a hop (not enough data)
        bool result1 = pipeline.process (512);
        expect (!result1, "First tick with 500 samples should not produce a hop");

        // Push another 500 — now staging has 1000, still < 1024
        pushSine (preFIFO, 500, sr, 1);
        pushSine (postFIFO, 500, sr, 1);
        bool result2 = pipeline.process (512);
        expect (!result2, "Second tick with 1000 total should not produce a hop");

        // Push 100 more — now staging has 1100 > 1024 → should produce a hop
        pushSine (preFIFO, 100, sr, 1);
        pushSine (postFIFO, 100, sr, 1);
        bool result3 = pipeline.process (512);
        expect (result3, "Third tick with 1100 total should produce a hop");

        // Verify stats: 1100 samples pulled, 1 hop completed, 76 residual
        auto stats = pipeline.getStats();
        expect (stats.pre.samplesPulled == 1100,
                "Should have pulled 1100 samples total, got " +
                juce::String (stats.pre.samplesPulled));
        expect (stats.pre.hopsCompleted == 1,
                "Should have completed 1 hop, got " +
                juce::String (stats.pre.hopsCompleted));
    }

    // ── Test 2: Fragmented input continuously produces hops ──────────────
    void testFragmentedInputProducesHops()
    {
        beginTest("Fragmented input (small blocks) continuously produces hops");

        constexpr double sr = 48000.0;
        constexpr size_t fftOrder = 11;  // hopSize = 1024
        constexpr int blockSize = 256;   // smaller than hopSize

        LockFreeAudioFIFO<float> preFIFO, postFIFO;
        preFIFO.prepare (65536);
        postFIFO.prepare (65536);

        NewSpectrumPipeline pipeline (preFIFO, postFIFO, fftOrder, sr);

        // Simulate 100 GUI ticks, each receiving one 256-sample block
        // At hopSize=1024, we expect ~1 hop every 4 ticks
        int totalHops = 0;
        for (int tick = 0; tick < 100; ++tick)
        {
            pushSine (preFIFO, blockSize, sr, 1);
            pushSine (postFIFO, blockSize, sr, 1);

            if (pipeline.process (512))
                ++totalHops;
        }

        // 100 ticks × 256 samples = 25600 samples / 1024 hopSize = 25 hops expected
        // Allow some tolerance for initial accumulation
        expect (totalHops >= 20 && totalHops <= 25,
                "Expected ~25 hops from 100×256 samples, got " + juce::String (totalHops));

        auto stats = pipeline.getStats();
        logMessage ("  Fragmented: " + juce::String (stats.pre.hopsCompleted) +
                    " hops, " + juce::String (stats.pre.samplesPulled) + " samples pulled"
                    ", noUpdateTicks=" + juce::String (stats.noUpdateTicks));
    }

    // ── Test 3: Backlog catch-up (burst then drain) ──────────────────────
    void testBacklogCatchUp()
    {
        beginTest("Backlog: burst of data is caught up within reasonable ticks");

        constexpr double sr = 48000.0;
        constexpr size_t fftOrder = 11;  // hopSize = 1024

        LockFreeAudioFIFO<float> preFIFO, postFIFO;
        preFIFO.prepare (65536);
        postFIFO.prepare (65536);

        NewSpectrumPipeline pipeline (preFIFO, postFIFO, fftOrder, sr);

        // Push a big burst: 16384 samples (16 hops worth)
        pushSine (preFIFO, 16384, sr, 1);
        pushSine (postFIFO, 16384, sr, 1);

        // First tick should drain up to kMaxHopsPerTick (16)
        pipeline.process (512);

        auto stats = pipeline.getStats();
        expect (stats.pre.hopsCompleted >= 14,
                "Burst of 16384 should complete at least 14 hops in one tick, got " +
                juce::String (stats.pre.hopsCompleted));

        logMessage ("  Backlog: " + juce::String (stats.pre.hopsCompleted) +
                    " hops in first tick, peak backlog=" +
                    juce::String (stats.pre.backlogPeak));
    }

    // ── Test 4: Stats instrumentation is populated ───────────────────────
    void testStatsInstrumentation()
    {
        beginTest("Pipeline stats instrumentation reports correct values");

        constexpr double sr = 48000.0;
        constexpr size_t fftOrder = 11;  // hopSize = 1024

        LockFreeAudioFIFO<float> preFIFO, postFIFO;
        preFIFO.prepare (65536);
        postFIFO.prepare (65536);

        NewSpectrumPipeline pipeline (preFIFO, postFIFO, fftOrder, sr);

        // Empty tick — should increment noUpdateTicks
        pipeline.process (512);
        auto s1 = pipeline.getStats();
        expect (s1.noUpdateTicks == 1, "Empty tick should increment noUpdateTicks");
        expect (s1.pre.samplesPulled == 0, "No data pushed → 0 samples pulled");
        expect (s1.pre.hopsCompleted == 0, "No data → 0 hops");

        // Push exactly 2 hops worth
        pushSine (preFIFO, 2048, sr, 1);
        pushSine (postFIFO, 2048, sr, 1);
        pipeline.process (512);

        auto s2 = pipeline.getStats();
        expect (s2.pre.samplesPulled == 2048,
                "Should have pulled 2048, got " + juce::String (s2.pre.samplesPulled));
        expect (s2.pre.hopsCompleted == 2,
                "2048 samples / 1024 hop = 2 hops, got " +
                juce::String (s2.pre.hopsCompleted));

        // Reset and verify
        pipeline.resetStats();
        auto s3 = pipeline.getStats();
        expect (s3.pre.samplesPulled == 0, "Reset should zero samplesPulled");
        expect (s3.noUpdateTicks == 0, "Reset should zero noUpdateTicks");
    }
};

static SpectrumPipelineTest spectrumPipelineTest;
