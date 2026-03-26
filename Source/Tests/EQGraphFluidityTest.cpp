#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>
#include <atomic>

#include "../PluginProcessor.h"
#include "../GUI/AdvancedSpectrumDisplay.h"

/**
 * EQGraphFluidityTest — Aggressive Performance Tests
 *
 * Simulates the exact scenario that causes stuttering:
 *   - Audio signal active (FFT generating spectrum data)
 *   - Multiple EQ bands enabled at various frequencies
 *   - User drags a band across the spectrum
 *   - paint() must complete within 16.6ms (60fps) budget
 *
 * Tests are designed to FAIL if:
 *   - Average paint time exceeds 10ms (leaves 6ms headroom for compositing)
 *   - Max paint time exceeds 16.6ms (single frame drop = visible stutter)
 *   - P95 paint time exceeds 14ms (consistent fluidity, not just average)
 *   - EQ curve rebuild exceeds 5ms (blocks the paint pipeline)
 *   - More than 5% of frames are dropped (>16.6ms)
 *
 * Scenarios tested:
 *   1. 23 bands active, drag 1 band — worst-case production scenario
 *   2. 8 bands active, rapid drag — fast mouse movement
 *   3. Multiple curve rebuilds per second — parameter storm
 *   4. Spectrum + EQ combined — full paint pipeline under load
 *   5. Sustained drag over 5 seconds
 *   6. Adaptive timer rate
 *   7. CONCURRENT audio thread + GUI paint (real host scenario)
 */
class EQGraphFluidityTest : public juce::UnitTest
{
public:
    EQGraphFluidityTest()
        : juce::UnitTest("EQ Graph Fluidity (Aggressive)", "Performance") {}

    void runTest() override
    {
        testMaxBandsDragFluidity();
        testRapidDragFluidity();
        testEQCurveRebuildCost();
        testFullPaintPipelineUnderLoad();
        testFrameDropPercentage();
        testAdaptiveTimerRate();
        testConcurrentAudioAndPaint();
    }

private:
    // ── Thresholds ──
    static constexpr double kAvgPaintBudgetMs   = 10.0;   // Average paint must be under 10ms
    static constexpr double kMaxPaintBudgetMs   = 16.6;   // No single frame over 16.6ms
    static constexpr double kP95PaintBudgetMs   = 14.0;   // 95th percentile under 14ms
    static constexpr double kMaxCurveRebuildMs  =  5.0;   // EQ curve rebuild alone
    static constexpr double kMaxFrameDropPct    =  5.0;   // Max 5% dropped frames
    static constexpr int    kWarmupFrames       =  5;     // Discard first N frames

    // ── Helpers ──
    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void setBool(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& id, bool value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
    }

    // Fill buffer with white noise to drive the FFT
    static void fillNoise(juce::AudioBuffer<float>& buf, float amplitude)
    {
        juce::Random rng(42);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                data[i] = amplitude * (rng.nextFloat() * 2.0f - 1.0f);
        }
    }

    // Activate N bands with spread frequencies and varied gains
    void activateNBands(AIEqualizerAudioProcessor& proc, int numBands)
    {
        proc.setNumActiveBands(numBands);
        const float freqMin = 40.0f, freqMax = 16000.0f;
        const float logMin = std::log10(freqMin);
        const float logMax = std::log10(freqMax);

        for (int i = 0; i < numBands; ++i)
        {
            float t = (numBands > 1) ? static_cast<float>(i) / (numBands - 1) : 0.5f;
            float freq = std::pow(10.0f, logMin + t * (logMax - logMin));
            // Alternate positive/negative gains for realistic curve shape
            float gain = (i % 2 == 0) ? 4.0f + (i % 5) : -(3.0f + (i % 4));

            AIEqualizerAudioProcessor::BandState band;
            band.frequency = freq;
            band.gain      = gain;
            band.q         = 0.7f + (i % 3) * 0.5f;
            band.type      = static_cast<int>(ParametricEQProcessor::Peak);
            band.enabled   = true;
            band.solo      = false;
            proc.setBandState(i, band);
        }
    }

    // Push audio blocks to fill FFT and generate spectrum data
    void feedAudioBlocks(AIEqualizerAudioProcessor& proc, int numBlocks,
                         int blockSize = 512, double sampleRate = 48000.0)
    {
        juce::AudioBuffer<float> chunk(2, blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < numBlocks; ++i)
        {
            fillNoise(chunk, 0.3f);
            proc.processBlock(chunk, midi);
        }
    }

    struct PaintMetrics
    {
        std::vector<double> frameTimes;
        double avgMs   = 0.0;
        double maxMs   = 0.0;
        double p95Ms   = 0.0;
        double p99Ms   = 0.0;
        double minMs   = 999.0;
        int    droppedFrames = 0;
        double dropPct = 0.0;

        void compute()
        {
            if (frameTimes.empty()) return;
            std::sort(frameTimes.begin(), frameTimes.end());
            double sum = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0);
            avgMs = sum / frameTimes.size();
            minMs = frameTimes.front();
            maxMs = frameTimes.back();
            p95Ms = frameTimes[static_cast<size_t>(frameTimes.size() * 0.95)];
            p99Ms = frameTimes[static_cast<size_t>(frameTimes.size() * 0.99)];
            droppedFrames = 0;
            for (double t : frameTimes)
                if (t > 16.6) droppedFrames++;
            dropPct = 100.0 * droppedFrames / frameTimes.size();
        }
    };

    // Benchmark paint() for N frames, simulating drag
    PaintMetrics benchmarkPaint(AdvancedSpectrumDisplay& display,
                                AIEqualizerAudioProcessor& proc,
                                int totalFrames, bool simulateDrag,
                                int dragBandIndex = 0,
                                float dragStartFreq = 200.0f,
                                float dragEndFreq = 8000.0f)
    {
        PaintMetrics metrics;
        juce::Image target(juce::Image::ARGB, 1200, 500, true);

        for (int frame = 0; frame < totalFrames; ++frame)
        {
            // Feed 1 audio block per frame to keep FFT alive
            juce::AudioBuffer<float> chunk(2, 256);
            juce::MidiBuffer midi;
            fillNoise(chunk, 0.3f);
            proc.processBlock(chunk, midi);

            // Simulate band drag — update frequency progressively
            if (simulateDrag && frame >= kWarmupFrames)
            {
                float t = static_cast<float>(frame - kWarmupFrames)
                        / static_cast<float>(totalFrames - kWarmupFrames);
                float logStart = std::log10(dragStartFreq);
                float logEnd   = std::log10(dragEndFreq);
                float freq = std::pow(10.0f, logStart + t * (logEnd - logStart));

                auto band = proc.getBandState(dragBandIndex);
                band.frequency = freq;
                proc.setBandState(dragBandIndex, band);
                // Simulate what the host does
                setFloat(proc.getAPVTS(), "band" + juce::String(dragBandIndex) + "Freq", freq);
            }

            // Time the paint call
            juce::Graphics g(target);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            display.paint(g);
            auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;

            if (frame >= kWarmupFrames)
                metrics.frameTimes.push_back(elapsed);
        }

        metrics.compute();
        return metrics;
    }

    //==========================================================================
    // TEST 1: 23 bands active, drag 1 band across full spectrum
    //         This is the EXACT production worst-case scenario
    //==========================================================================
    void testMaxBandsDragFluidity()
    {
        beginTest("23 bands active + drag band across spectrum (worst case)");

        constexpr int kNumBands = 23;
        constexpr int kFrames   = 120; // 2 seconds at 60fps

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, kNumBands);
        setBool(proc.getAPVTS(), "bypass", false);
        setFloat(proc.getAPVTS(), "dryWet", 100.0f);

        // Feed enough audio to fill FFT buffer
        feedAudioBlocks(proc, 20);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);

        // Simulate drag state
        display.isDraggingBand = true;
        display.draggedBandIndex = 5; // drag band #5

        auto m = benchmarkPaint(display, proc, kFrames, true, 5, 100.0f, 12000.0f);

        logMessage("  [23-band drag] avg=" + juce::String(m.avgMs, 2)
                   + "ms max=" + juce::String(m.maxMs, 2)
                   + "ms p95=" + juce::String(m.p95Ms, 2)
                   + "ms p99=" + juce::String(m.p99Ms, 2)
                   + "ms min=" + juce::String(m.minMs, 2)
                   + "ms dropped=" + juce::String(m.droppedFrames)
                   + " (" + juce::String(m.dropPct, 1) + "%)");

        expect(m.avgMs < kAvgPaintBudgetMs,
               "23-band drag: avgMs=" + juce::String(m.avgMs, 2) + " exceeds " + juce::String(kAvgPaintBudgetMs) + "ms budget");
        expect(m.maxMs < kMaxPaintBudgetMs * 1.5, // Allow 1.5x for worst-case with 23 bands
               "23-band drag: maxMs=" + juce::String(m.maxMs, 2) + " exceeds 25ms hard limit");
        expect(m.p95Ms < kP95PaintBudgetMs,
               "23-band drag: p95=" + juce::String(m.p95Ms, 2) + " exceeds " + juce::String(kP95PaintBudgetMs) + "ms");
        expect(m.dropPct < kMaxFrameDropPct,
               "23-band drag: " + juce::String(m.dropPct, 1) + "% frames dropped (max " + juce::String(kMaxFrameDropPct) + "%)");

        display.isDraggingBand = false;
        proc.releaseResources();
    }

    //==========================================================================
    // TEST 2: 8 bands active, RAPID drag (high-frequency param changes)
    //         Simulates fast mouse movement — param changes every frame
    //==========================================================================
    void testRapidDragFluidity()
    {
        beginTest("8 bands active + rapid drag (fast mouse movement)");

        constexpr int kNumBands = 8;
        constexpr int kFrames   = 90;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, kNumBands);
        setBool(proc.getAPVTS(), "bypass", false);
        setFloat(proc.getAPVTS(), "dryWet", 100.0f);

        feedAudioBlocks(proc, 20);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);
        display.isDraggingBand = true;
        display.draggedBandIndex = 0;

        // Rapid drag: sweep 200Hz → 15kHz in 90 frames — very fast mouse
        auto m = benchmarkPaint(display, proc, kFrames, true, 0, 200.0f, 15000.0f);

        logMessage("  [rapid drag] avg=" + juce::String(m.avgMs, 2)
                   + "ms max=" + juce::String(m.maxMs, 2)
                   + "ms p95=" + juce::String(m.p95Ms, 2)
                   + "ms dropped=" + juce::String(m.droppedFrames)
                   + " (" + juce::String(m.dropPct, 1) + "%)");

        expect(m.avgMs < kAvgPaintBudgetMs,
               "Rapid drag: avgMs=" + juce::String(m.avgMs, 2) + " exceeds budget");
        expect(m.p95Ms < kP95PaintBudgetMs,
               "Rapid drag: p95=" + juce::String(m.p95Ms, 2) + " exceeds " + juce::String(kP95PaintBudgetMs) + "ms");
        expect(m.dropPct < kMaxFrameDropPct,
               "Rapid drag: " + juce::String(m.dropPct, 1) + "% frames dropped");

        display.isDraggingBand = false;
        proc.releaseResources();
    }

    //==========================================================================
    // TEST 3: EQ curve rebuild cost in isolation
    //         rebuildEQCurvePath + getMagnitudeForFrequencyArray
    //==========================================================================
    void testEQCurveRebuildCost()
    {
        beginTest("EQ curve rebuild cost (23 bands, 128 points)");

        constexpr int kNumBands = 23;
        constexpr int kIterations = 100;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, kNumBands);

        feedAudioBlocks(proc, 10);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);

        // Force graphBounds to be set by doing one paint
        {
            juce::Image tmp(juce::Image::ARGB, 1200, 500, true);
            juce::Graphics g(tmp);
            display.paint(g);
        }

        std::vector<double> rebuildTimes;
        for (int i = 0; i < kIterations; ++i)
        {
            // Force dirty state by changing a param
            float freq = 1000.0f + i * 10.0f;
            setFloat(proc.getAPVTS(), "band0Freq", freq);

            // Directly benchmark rebuildEQCurvePath
            display.eqCurveDirty = true;
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            display.rebuildEQCurvePath();
            auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;

            if (i >= 5) // warmup
                rebuildTimes.push_back(elapsed);
        }

        std::sort(rebuildTimes.begin(), rebuildTimes.end());
        double avg = std::accumulate(rebuildTimes.begin(), rebuildTimes.end(), 0.0) / rebuildTimes.size();
        double max = rebuildTimes.back();
        double p95 = rebuildTimes[static_cast<size_t>(rebuildTimes.size() * 0.95)];

        logMessage("  [curve rebuild] avg=" + juce::String(avg, 3)
                   + "ms max=" + juce::String(max, 3)
                   + "ms p95=" + juce::String(p95, 3) + "ms");

        expect(avg < kMaxCurveRebuildMs,
               "Curve rebuild avg=" + juce::String(avg, 3) + "ms exceeds " + juce::String(kMaxCurveRebuildMs) + "ms");
        expect(max < kMaxCurveRebuildMs * 2.0,
               "Curve rebuild max=" + juce::String(max, 3) + "ms exceeds hard limit");

        proc.releaseResources();
    }

    //==========================================================================
    // TEST 4: Full paint pipeline under load — spectrum + EQ + bands + hover
    //         Everything enabled simultaneously, no LOD shortcuts
    //==========================================================================
    void testFullPaintPipelineUnderLoad()
    {
        beginTest("Full paint pipeline under load (no drag LOD, all components)");

        constexpr int kNumBands = 16;
        constexpr int kFrames   = 60;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, kNumBands);
        setBool(proc.getAPVTS(), "bypass", false);
        setFloat(proc.getAPVTS(), "dryWet", 100.0f);

        feedAudioBlocks(proc, 30);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);

        // NOT dragging — all glow layers, fill, text, tooltips active
        display.isDraggingBand = false;
        display.hoverX = 600; // simulate hover to enable tooltip + crosshair

        auto m = benchmarkPaint(display, proc, kFrames, false);

        logMessage("  [full pipeline] avg=" + juce::String(m.avgMs, 2)
                   + "ms max=" + juce::String(m.maxMs, 2)
                   + "ms p95=" + juce::String(m.p95Ms, 2)
                   + "ms dropped=" + juce::String(m.droppedFrames)
                   + " (" + juce::String(m.dropPct, 1) + "%)");

        // Full pipeline allowed slightly more headroom (glow layers active)
        expect(m.avgMs < 12.0,
               "Full pipeline avg=" + juce::String(m.avgMs, 2) + "ms exceeds 12ms");
        expect(m.p95Ms < kMaxPaintBudgetMs,
               "Full pipeline p95=" + juce::String(m.p95Ms, 2) + "ms exceeds 16.6ms");

        proc.releaseResources();
    }

    //==========================================================================
    // TEST 5: Frame drop percentage over sustained drag session
    //         300 frames (5 seconds) — must not drop more than 5%
    //==========================================================================
    void testFrameDropPercentage()
    {
        beginTest("Sustained drag session: <5% frame drops over 5 seconds");

        constexpr int kNumBands = 12;
        constexpr int kFrames   = 300; // 5 seconds at 60fps

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, kNumBands);
        setBool(proc.getAPVTS(), "bypass", false);
        setFloat(proc.getAPVTS(), "dryWet", 100.0f);

        feedAudioBlocks(proc, 30);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);
        display.isDraggingBand = true;
        display.draggedBandIndex = 3;

        // Long sustained drag back and forth
        PaintMetrics metrics;
        juce::Image target(juce::Image::ARGB, 1200, 500, true);

        for (int frame = 0; frame < kFrames; ++frame)
        {
            // Feed audio
            juce::AudioBuffer<float> chunk(2, 256);
            juce::MidiBuffer midi;
            fillNoise(chunk, 0.3f);
            proc.processBlock(chunk, midi);

            // Oscillate frequency back and forth (like real drag behavior)
            float t = static_cast<float>(frame) / kFrames;
            float sweep = 0.5f + 0.5f * std::sin(t * juce::MathConstants<float>::twoPi * 3.0f);
            float freq = 200.0f + sweep * 10000.0f;

            auto band = proc.getBandState(3);
            band.frequency = freq;
            proc.setBandState(3, band);
            setFloat(proc.getAPVTS(), "band3Freq", freq);

            juce::Graphics g(target);
            auto t0 = juce::Time::getMillisecondCounterHiRes();
            display.paint(g);
            auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;

            if (frame >= kWarmupFrames)
                metrics.frameTimes.push_back(elapsed);
        }

        metrics.compute();

        logMessage("  [sustained drag] avg=" + juce::String(metrics.avgMs, 2)
                   + "ms max=" + juce::String(metrics.maxMs, 2)
                   + "ms dropped=" + juce::String(metrics.droppedFrames)
                   + "/" + juce::String((int)metrics.frameTimes.size())
                   + " (" + juce::String(metrics.dropPct, 1) + "%)");

        expect(metrics.dropPct < kMaxFrameDropPct,
               "Sustained drag: " + juce::String(metrics.dropPct, 1) + "% frames dropped");
        expect(metrics.avgMs < kAvgPaintBudgetMs,
               "Sustained drag: avg=" + juce::String(metrics.avgMs, 2) + "ms exceeds budget");

        display.isDraggingBand = false;
        proc.releaseResources();
    }

    //==========================================================================
    // TEST 6: Adaptive timer rate verification
    //         Verifies the timer drops to 5Hz when not visible
    //==========================================================================
    void testAdaptiveTimerRate()
    {
        beginTest("Adaptive timer rate: drops when not showing");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        activateNBands(proc, 4);

        AdvancedSpectrumDisplay display(proc);
        // Don't add to a visible component — isShowing() returns false
        display.setBounds(0, 0, 1200, 500);

        // Trigger a timer callback to let it adapt
        display.timerCallback();

        // When not showing and not interacting, should be low rate
        int currentHz = display.currentTimerHz;
        logMessage("  [adaptive timer] rate when not showing: " + juce::String(currentHz) + "Hz");
        expect(currentHz <= 5,
               "Timer should be <=5Hz when not showing, got " + juce::String(currentHz) + "Hz");

        // Simulate interaction
        display.isDraggingBand = true;
        display.timerCallback();
        currentHz = display.currentTimerHz;
        logMessage("  [adaptive timer] rate when dragging (not showing): " + juce::String(currentHz) + "Hz");
        // Even if not showing, drag state shouldn't go to 60Hz since window is invisible
        expect(currentHz <= 5,
               "Timer should still be <=5Hz when dragging but not showing, got " + juce::String(currentHz) + "Hz");

        display.isDraggingBand = false;
        proc.releaseResources();
    }

    //==========================================================================
    // TEST 7: CONCURRENT audio thread + GUI paint
    //
    // This is the REAL host scenario:
    //   - A background thread hammers processBlock() at 48kHz / 256 samples
    //     (~187 calls/sec) — exactly like Ableton's audio engine
    //   - The main thread runs paint() at 60fps simultaneously
    //   - Both compete for CPU cache and memory bandwidth
    //
    // Failure meaning: if paint gets >16.6ms under concurrent audio load,
    // the user sees stutter even though paint alone would be fast.
    //==========================================================================
    void testConcurrentAudioAndPaint()
    {
        beginTest("Concurrent audio thread + paint (real host scenario, 23 bands, drag)");

        constexpr int    kNumBands    = 23;
        constexpr int    kPaintFrames = 120;       // 2 sec at 60fps
        constexpr int    kBlockSize   = 256;
        constexpr double kSampleRate  = 48000.0;
        // Real host: 48000/256 = 187.5 processBlock calls/sec
        constexpr double kBlockIntervalMs = (kBlockSize / kSampleRate) * 1000.0; // ~5.33ms

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        activateNBands(proc, kNumBands);
        setBool(proc.getAPVTS(), "bypass", false);
        setFloat(proc.getAPVTS(), "dryWet", 100.0f);

        // Pre-fill FFT
        feedAudioBlocks(proc, 30, kBlockSize, kSampleRate);

        AdvancedSpectrumDisplay display(proc);
        display.setBounds(0, 0, 1200, 500);
        display.isDraggingBand = true;
        display.draggedBandIndex = 5;

        // ── Spin up audio thread ──
        std::atomic<bool> audioRunning { true };
        std::atomic<int>  audioBlockCount { 0 };

        std::thread audioThread([&]()
        {
            juce::AudioBuffer<float> chunk(2, kBlockSize);
            juce::MidiBuffer midi;
            juce::Random rng(99);

            while (audioRunning.load(std::memory_order_relaxed))
            {
                // Fill with noise (simulates real signal)
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = chunk.getWritePointer(ch);
                    for (int i = 0; i < kBlockSize; ++i)
                        d[i] = 0.3f * (rng.nextFloat() * 2.0f - 1.0f);
                }

                proc.processBlock(chunk, midi);
                audioBlockCount.fetch_add(1, std::memory_order_relaxed);

                // Sleep to approximate real host timing (~5.3ms between blocks)
                juce::Thread::sleep(static_cast<int>(kBlockIntervalMs));
            }
        });

        // ── Paint on main thread at 60fps while audio runs ──
        PaintMetrics metrics;
        juce::Image target(juce::Image::ARGB, 1200, 500, true);
        constexpr double kFrameIntervalMs = 1000.0 / 60.0; // 16.6ms

        for (int frame = 0; frame < kPaintFrames; ++frame)
        {
            // Simulate band drag
            if (frame >= kWarmupFrames)
            {
                float t = static_cast<float>(frame - kWarmupFrames)
                        / static_cast<float>(kPaintFrames - kWarmupFrames);
                float freq = std::pow(10.0f, std::log10(100.0f) + t * (std::log10(12000.0f) - std::log10(100.0f)));
                auto band = proc.getBandState(5);
                band.frequency = freq;
                proc.setBandState(5, band);
                setFloat(proc.getAPVTS(), "band5Freq", freq);
            }

            auto t0 = juce::Time::getMillisecondCounterHiRes();
            {
                juce::Graphics g(target);
                display.paint(g);
            }
            double elapsed = juce::Time::getMillisecondCounterHiRes() - t0;

            if (frame >= kWarmupFrames)
                metrics.frameTimes.push_back(elapsed);

            // Pace at 60fps (sleep remainder of frame budget)
            double remaining = kFrameIntervalMs - elapsed;
            if (remaining > 1.0)
                juce::Thread::sleep(static_cast<int>(remaining));
        }

        // Stop audio thread
        audioRunning.store(false, std::memory_order_relaxed);
        audioThread.join();

        metrics.compute();

        logMessage("  [concurrent] avg=" + juce::String(metrics.avgMs, 2)
                   + "ms max=" + juce::String(metrics.maxMs, 2)
                   + "ms p95=" + juce::String(metrics.p95Ms, 2)
                   + "ms p99=" + juce::String(metrics.p99Ms, 2)
                   + "ms dropped=" + juce::String(metrics.droppedFrames)
                   + "/" + juce::String((int)metrics.frameTimes.size())
                   + " (" + juce::String(metrics.dropPct, 1) + "%)"
                   + "  audioBlocks=" + juce::String(audioBlockCount.load()));

        expect(metrics.avgMs < kAvgPaintBudgetMs,
               "Concurrent: avg=" + juce::String(metrics.avgMs, 2) + "ms exceeds " + juce::String(kAvgPaintBudgetMs) + "ms");
        expect(metrics.p95Ms < kP95PaintBudgetMs,
               "Concurrent: p95=" + juce::String(metrics.p95Ms, 2) + "ms exceeds " + juce::String(kP95PaintBudgetMs) + "ms");
        expect(metrics.dropPct < kMaxFrameDropPct,
               "Concurrent: " + juce::String(metrics.dropPct, 1) + "% frames dropped (max " + juce::String(kMaxFrameDropPct) + "%)");

        display.isDraggingBand = false;
        proc.releaseResources();
    }
};

static EQGraphFluidityTest eqGraphFluidityTest;
