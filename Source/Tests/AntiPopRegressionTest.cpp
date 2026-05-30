#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

#include "../PluginProcessor.h"

/**
 * AntiPopRegressionTest
 *
 * Targeted regression tests for the anti-pop stabilization work:
 *   1. Bypass state machine: filter reset on exit from steady-state
 *   2. DynamicEQ coefficient crossfade: no thrashing during drag
 *   3. LP IR latest-wins: rapid IR swaps don't produce clicks
 */
class AntiPopRegressionTest : public juce::UnitTest
{
public:
    AntiPopRegressionTest()
        : juce::UnitTest("Anti-Pop Regression", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testBypassSteadyStateExitNoClick();
        testBypassRapidToggleNoClick();
        testDynEQThresholdDragNoClick();
        testLPIRRapidSwapNoClick();

        // Linear Phase specific
        testDynEQThresholdDragLP();
        testLPBandDragPlusDynEQ();

        // Dynamic-band freq/Q/gain drag while actively compressing — the exact
        // gesture Marco reported as crackling, which no other test covered.
        testDynEQBandFreqDragWhileCompressing();

        // Same gesture but through the Natural-Phase 4x oversampled HQ path
        // (dynamicEQProcessorHQ). Marco's repro condition: oversampling 4x,
        // block 256-512. The sample-domain constants (control slice 16,
        // rate-limit 64, crossfade 16/128/256) span 1/4 the real time at 4x,
        // which is the leading suspect for the offline<->real divergence.
        testDynEQBandDragOversampled4x();

        // Adversarial / stress tests
        testAdversarialBlockSizeBypass();
        testStormBypassToggle();
        testStormParameterBurst();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int    kBlockSize  = 128;
    static constexpr float  kClickThreshold = 0.3f;

    //==========================================================================
    // Helpers
    //==========================================================================
    static void setBool(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& id, bool value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
    }

    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static void fillSine(juce::AudioBuffer<float>& buf, double freqHz,
                         float amplitude, int sampleOffset)
    {
        const auto w = juce::MathConstants<double>::twoPi * freqHz / kSampleRate;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                data[i] = amplitude * static_cast<float>(
                    std::sin(w * static_cast<double>(sampleOffset + i)));
        }
    }

    struct ClickMetrics
    {
        float maxDelta = 0.0f;
        int   clickCount = 0;
        float peakAbs = 0.0f;
        bool  hasNaN = false;
        bool  hasInf = false;
    };

    static ClickMetrics analyzeClicks(const juce::AudioBuffer<float>& buf,
                                       int startSample = 0)
    {
        ClickMetrics m;
        if (buf.getNumChannels() == 0 || buf.getNumSamples() == 0) return m;

        const float* ch0 = buf.getReadPointer(0);
        const int numSamples = buf.getNumSamples();

        for (int i = startSample; i < numSamples; ++i)
        {
            const float v = ch0[i];
            if (std::isnan(v)) m.hasNaN = true;
            if (std::isinf(v)) m.hasInf = true;
            m.peakAbs = std::max(m.peakAbs, std::abs(v));

            if (i > startSample)
            {
                const float delta = std::abs(v - ch0[i - 1]);
                m.maxDelta = std::max(m.maxDelta, delta);
                if (delta > kClickThreshold)
                    ++m.clickCount;
            }
        }
        return m;
    }

    void prepareProcessor(AIEqualizerAudioProcessor& proc)
    {
        proc.setPlayConfigDetails(2, 2, kSampleRate, kBlockSize);
        proc.prepareToPlay(kSampleRate, kBlockSize);
    }

    //==========================================================================
    // Test 1: Bypass exit from steady-state — filter reset prevents pop
    //==========================================================================
    void testBypassSteadyStateExitNoClick()
    {
        beginTest("Bypass exit after long steady-state produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Enable band 0 with a strong peak at 1kHz
        setFloat(apvts, "band0Freq", 1000.0f);
        setFloat(apvts, "band0Gain", 12.0f);
        setFloat(apvts, "band0Q", 4.0f);
        setChoice(apvts, "band0Type", 2); // Peak
        setBool(apvts, "band0Enabled", true);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up: 20 blocks of active processing
        for (int b = 0; b < 20; ++b)
        {
            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Engage bypass
        setBool(apvts, "bypass", true);

        // Run crossfade to completion + many blocks of steady-state bypass
        // This is the critical scenario: filters sit idle for a long time
        for (int b = 0; b < 100; ++b)
        {
            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Disengage bypass — state machine should reset all filters
        setBool(apvts, "bypass", false);

        // Collect output over the crossfade window
        juce::AudioBuffer<float> collected(2, kBlockSize * 30);
        for (int b = 0; b < 30; ++b)
        {
            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);

        expect(!metrics.hasNaN, "NaN after bypass exit");
        expect(!metrics.hasInf, "Inf after bypass exit");
        expect(metrics.clickCount == 0,
               "Click detected on bypass exit: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  bypass exit: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Test 2: Bypass rapid toggle — state machine handles direction reversal
    //==========================================================================
    void testBypassRapidToggleNoClick()
    {
        beginTest("Bypass rapid toggle (every 3 blocks) produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        setFloat(apvts, "band0Freq", 2000.0f);
        setFloat(apvts, "band0Gain", 8.0f);
        setBool(apvts, "band0Enabled", true);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up
        for (int b = 0; b < 10; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Rapid toggle: bypass ON/OFF every 3 blocks for 60 blocks
        juce::AudioBuffer<float> collected(2, kBlockSize * 60);
        bool bypassed = false;
        for (int b = 0; b < 60; ++b)
        {
            if (b % 3 == 0)
            {
                bypassed = !bypassed;
                setBool(apvts, "bypass", bypassed);
            }
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in rapid toggle");
        expect(!metrics.hasInf, "Inf in rapid toggle");
        expect(metrics.clickCount == 0,
               "Click in rapid toggle: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  rapid toggle: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount));
    }

    //==========================================================================
    // Test 3: DynamicEQ threshold drag — crossfade prevents coefficient snap
    //==========================================================================
    void testDynEQThresholdDragNoClick()
    {
        beginTest("DynEQ threshold sweep produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Enable DynEQ, set band 0 to compress
        setBool(apvts, "dynamicEQEnabled", true);
        auto& dynProc = proc.getDynamicEQProcessor();
        DynamicEQProcessor::DynamicBandParams dp;
        dp.frequency = 1000.0f;
        dp.gain = 6.0f;
        dp.q = 2.0f;
        dp.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        dp.threshold = -40.0f;
        dp.ratio = 4.0f;
        dp.range = 24.0f;
        dp.knee = 6.0f;
        dp.enabled = true;
        dynProc.setBandParams(0, dp);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up
        for (int b = 0; b < 20; ++b)
        {
            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Sweep threshold from -40 to 0 over 40 blocks
        juce::AudioBuffer<float> collected(2, kBlockSize * 40);
        for (int b = 0; b < 40; ++b)
        {
            dp.threshold = -40.0f + (40.0f * static_cast<float>(b) / 39.0f);
            dynProc.setBandParams(0, dp);

            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in DynEQ sweep");
        expect(!metrics.hasInf, "Inf in DynEQ sweep");
        expect(metrics.clickCount == 0,
               "Click in DynEQ threshold sweep: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  DynEQ sweep: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount));
    }

    //==========================================================================
    // Test 4: LP IR rapid swap stress — latest-wins prevents mid-fade pop
    //==========================================================================
    void testLPIRRapidSwapNoClick()
    {
        beginTest("LP IR rapid swap stress produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Switch to Linear Phase mode
        setChoice(apvts, "phaseMode", 2); // LinearPhase

        // Enable band 0
        setFloat(apvts, "band0Freq", 500.0f);
        setFloat(apvts, "band0Gain", 6.0f);
        setBool(apvts, "band0Enabled", true);

        // Force IR ready so LP processes through convolver
        proc.forceLinearIRReady();

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up LP path
        for (int b = 0; b < 40; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Drag band freq from 500 → 4000 Hz every block (aggressive)
        juce::AudioBuffer<float> collected(2, kBlockSize * 60);
        for (int b = 0; b < 60; ++b)
        {
            float freq = 500.0f + (3500.0f * static_cast<float>(b) / 59.0f);
            setFloat(apvts, "band0Freq", freq);

            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in LP IR swap stress");
        expect(!metrics.hasInf, "Inf in LP IR swap stress");
        expect(metrics.clickCount == 0,
               "Click in LP IR swap: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  LP IR swap stress: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }
    //==========================================================================
    // Test 5: DynEQ threshold drag in LINEAR PHASE mode
    //
    // Test 3 runs in Zero Latency. This runs the same sweep in LP mode,
    // where DynEQ sits AFTER the convolver (PluginProcessor.cpp:2144).
    // Different code path — must be tested independently.
    //==========================================================================
    void testDynEQThresholdDragLP()
    {
        beginTest("DynEQ threshold sweep in Linear Phase produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Switch to Linear Phase
        setChoice(apvts, "phaseMode", 2);

        // Enable band 0 for LP convolver
        setFloat(apvts, "band0Freq", 1000.0f);
        setFloat(apvts, "band0Gain", 6.0f);
        setBool(apvts, "band0Enabled", true);

        // Force IR ready so LP processes through convolver
        proc.forceLinearIRReady();

        // Enable DynEQ with compressor on band 0
        setBool(apvts, "dynamicEQEnabled", true);
        auto& dynProc = proc.getDynamicEQProcessor();
        DynamicEQProcessor::DynamicBandParams dp;
        dp.frequency = 1000.0f;
        dp.gain = 6.0f;
        dp.q = 2.0f;
        dp.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        dp.threshold = -40.0f;
        dp.ratio = 4.0f;
        dp.range = 24.0f;
        dp.knee = 6.0f;
        dp.enabled = true;
        dynProc.setBandParams(0, dp);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up LP path
        for (int b = 0; b < 40; ++b)
        {
            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Sweep threshold -40 → 0 → -40 (triangle, 80 blocks)
        const int sweepBlocks = 80;
        juce::AudioBuffer<float> collected(2, kBlockSize * sweepBlocks);
        for (int b = 0; b < sweepBlocks; ++b)
        {
            float t = static_cast<float>(b) / static_cast<float>(sweepBlocks - 1);
            dp.threshold = (t < 0.5f)
                ? -40.0f + (80.0f * t)          // -40 → 0
                : 40.0f - (80.0f * t);          // 0 → -40
            dynProc.setBandParams(0, dp);

            fillSine(buf, 1000.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in DynEQ LP sweep");
        expect(!metrics.hasInf, "Inf in DynEQ LP sweep");
        expect(metrics.clickCount == 0,
               "Click in DynEQ LP sweep: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  DynEQ LP sweep: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Test 6: LP band drag + DynEQ simultaneously
    //
    // Worst case: user drags EQ band freq AND DynEQ threshold at the same
    // time in Linear Phase. Exercises convolver crossfade + DynEQ crossfade
    // simultaneously.
    //==========================================================================
    void testLPBandDragPlusDynEQ()
    {
        beginTest("LP band drag + DynEQ threshold drag simultaneously produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Switch to Linear Phase
        setChoice(apvts, "phaseMode", 2);

        // Enable band 0
        setFloat(apvts, "band0Freq", 500.0f);
        setFloat(apvts, "band0Gain", 8.0f);
        setFloat(apvts, "band0Q", 2.0f);
        setBool(apvts, "band0Enabled", true);

        proc.forceLinearIRReady();

        // Enable DynEQ
        setBool(apvts, "dynamicEQEnabled", true);
        auto& dynProc = proc.getDynamicEQProcessor();
        DynamicEQProcessor::DynamicBandParams dp;
        dp.frequency = 1000.0f;
        dp.gain = 6.0f;
        dp.q = 2.0f;
        dp.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        dp.threshold = -30.0f;
        dp.ratio = 3.0f;
        dp.range = 18.0f;
        dp.knee = 6.0f;
        dp.enabled = true;
        dynProc.setBandParams(0, dp);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up
        for (int b = 0; b < 40; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Simultaneous drag: band freq 500→4000 + DynEQ threshold -30→-5
        const int dragBlocks = 60;
        juce::AudioBuffer<float> collected(2, kBlockSize * dragBlocks);
        for (int b = 0; b < dragBlocks; ++b)
        {
            float t = static_cast<float>(b) / static_cast<float>(dragBlocks - 1);

            // Drag band freq
            setFloat(apvts, "band0Freq", 500.0f + 3500.0f * t);

            // Drag DynEQ threshold
            dp.threshold = -30.0f + 25.0f * t;
            dynProc.setBandParams(0, dp);

            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in LP+DynEQ combined drag");
        expect(!metrics.hasInf, "Inf in LP+DynEQ combined drag");
        expect(metrics.clickCount == 0,
               "Click in LP+DynEQ combined: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  LP+DynEQ combined: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Test 6b: Dynamic-band freq/Q/gain drag while the band is actively
    // compressing.
    //
    // This is the gesture Marco reported as crackling: grabbing a band that is
    // in Compress mode and dragging it around the spectrum. Two coefficient
    // update paths run at once on the same band:
    //   1. the version-bump (drag) rebuild of the live biquad with a long
    //      crossfade, and
    //   2. the per-sample dynamic control-slice rebuild toward the live
    //      effective gain (short crossfade, guarded by xfade.remaining <= 0).
    // The danger is the long drag crossfade starving the control slice while the
    // compressor's gain reduction is actively moving, producing an oversized
    // coefficient step (click) at fade end. We keep the compressor's GR moving
    // with an AM-modulated tone and drag freq+Q+gain densely, then assert the
    // full-processor output is click-free.
    //==========================================================================
    void testDynEQBandFreqDragWhileCompressing()
    {
        beginTest("DynEQ band freq/Q/gain drag while compressing produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        // Zero-latency / natural phase to isolate the DynEQ coefficient path
        // (no convolver crossfade in the picture).
        setChoice(apvts, "phaseMode", 0);

        // Drive the REAL parameter path through APVTS, not dynProc.setBandParams
        // directly: processBlock() re-pushes the smoothed APVTS band freq/gain/q
        // into all DynEQ processors every block, so a direct setBandParams call
        // would be clobbered. The dynamic band shares band0Freq/Gain/Q with the
        // static band; dragging those IS the drag gesture under test.
        setBool(apvts, "dynamicEQEnabled", true);
        setBool(apvts, "band0Enabled", true);
        setChoice(apvts, "band0Type", 2);                                   // Peak
        setChoice(apvts, "band0DynMode", DynamicEQProcessor::DynamicMode_Compress);
        setFloat(apvts, "band0Threshold", -30.0f);
        setFloat(apvts, "band0Ratio", 6.0f);
        setFloat(apvts, "band0Range", 18.0f);
        setFloat(apvts, "band0Knee", 3.0f);
        setFloat(apvts, "band0Attack", 2.0f);
        setFloat(apvts, "band0Release", 60.0f);
        setFloat(apvts, "band0Freq", 1000.0f);
        setFloat(apvts, "band0Gain", 8.0f);
        setFloat(apvts, "band0Q", 2.0f);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // AM-modulated carrier near the band so gain reduction keeps moving.
        const double carrierHz = 1000.0;
        const double modHz = 6.0;
        const float baseAmp = 0.5f;
        const float modDepth = 0.5f;

        auto fillAM = [&](juce::AudioBuffer<float>& b, int offset)
        {
            const auto wc = juce::MathConstants<double>::twoPi * carrierHz / kSampleRate;
            const auto wm = juce::MathConstants<double>::twoPi * modHz / kSampleRate;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto* d = b.getWritePointer(ch);
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const double n = static_cast<double>(offset + i);
                    const double env = 1.0 + static_cast<double>(modDepth) * std::sin(wm * n);
                    d[i] = static_cast<float>(static_cast<double>(baseAmp) * env * std::sin(wc * n));
                }
            }
        };

        // Warm up so envelope/GR reach steady tracking.
        for (int bk = 0; bk < 60; ++bk)
        {
            fillAM(buf, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Dense freq/Q/gain drag. Frequency stays around the carrier (700..1500)
        // so the band keeps compressing throughout the gesture.
        const int dragBlocks = 240; // ~0.64 s of continuous dragging
        juce::AudioBuffer<float> collected(2, kBlockSize * dragBlocks);
        for (int bk = 0; bk < dragBlocks; ++bk)
        {
            const double tBlock = static_cast<double>(bk) / static_cast<double>(dragBlocks);
            const double ph = juce::MathConstants<double>::twoPi * tBlock * 4.0; // ~4 sweeps

            setFloat(apvts, "band0Freq",
                     static_cast<float>(1000.0 * std::pow(2.0, 0.55 * std::sin(ph))));
            setFloat(apvts, "band0Q",
                     static_cast<float>(3.0 + 2.0 * std::sin(ph * 1.3 + 0.4)));   // 1 .. 5
            setFloat(apvts, "band0Gain",
                     static_cast<float>(8.0 + 4.0 * std::sin(ph * 0.7 + 1.0)));   // 4 .. 12 dB

            fillAM(buf, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, bk * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in DynEQ band freq drag");
        expect(!metrics.hasInf, "Inf in DynEQ band freq drag");
        expect(metrics.clickCount == 0,
               "Click while dragging an actively-compressing DynEQ band: count="
               + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  DynEQ band freq drag: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Oversampled (4x Natural Phase) variant of the DynEQ band drag.
    //
    // Routes audio through dynamicEQProcessorHQ, prepared at 4x the host rate.
    // This is the path that was never exercised by phaseMode-0 tests, and the
    // one Marco reports crackling. Uses a 256-sample host block (mid range of
    // Marco's reported 256-512).
    //==========================================================================
    void testDynEQBandDragOversampled4x()
    {
        beginTest("DynEQ band drag through 4x oversampled HQ path produces no click");

        constexpr int hostBlock = 256;

        AIEqualizerAudioProcessor proc;
        proc.setPlayConfigDetails(2, 2, kSampleRate, hostBlock);
        proc.prepareToPlay(kSampleRate, hostBlock);
        auto& apvts = proc.getAPVTS();

        // Natural Phase → HQ oversampled path; 4x oversampling factor.
        setChoice(apvts, "phaseMode", 1);          // Natural Phase
        setChoice(apvts, "oversamplingFactor", 2); // 4x

        setBool(apvts, "dynamicEQEnabled", true);
        setBool(apvts, "band0Enabled", true);
        setChoice(apvts, "band0Type", 2);          // Peak
        setChoice(apvts, "band0DynMode", DynamicEQProcessor::DynamicMode_Compress);
        setFloat(apvts, "band0Threshold", -30.0f);
        setFloat(apvts, "band0Ratio", 6.0f);
        setFloat(apvts, "band0Range", 18.0f);
        setFloat(apvts, "band0Knee", 3.0f);
        setFloat(apvts, "band0Attack", 2.0f);
        setFloat(apvts, "band0Release", 60.0f);
        setFloat(apvts, "band0Freq", 1000.0f);
        setFloat(apvts, "band0Gain", 8.0f);
        setFloat(apvts, "band0Q", 2.0f);

        juce::AudioBuffer<float> buf(2, hostBlock);
        juce::MidiBuffer midi;
        int samplePos = 0;

        const double carrierHz = 1000.0;
        const double modHz = 6.0;
        const float baseAmp = 0.5f;
        const float modDepth = 0.5f;

        auto fillAM = [&](juce::AudioBuffer<float>& b, int offset)
        {
            const auto wc = juce::MathConstants<double>::twoPi * carrierHz / kSampleRate;
            const auto wm = juce::MathConstants<double>::twoPi * modHz / kSampleRate;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
            {
                auto* d = b.getWritePointer(ch);
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const double n = static_cast<double>(offset + i);
                    const double env = 1.0 + static_cast<double>(modDepth) * std::sin(wm * n);
                    d[i] = static_cast<float>(static_cast<double>(baseAmp) * env * std::sin(wc * n));
                }
            }
        };

        // Warm up: settle the phase-mode/oversampling crossfades and the
        // envelope tracker before measuring.
        for (int bk = 0; bk < 80; ++bk)
        {
            fillAM(buf, samplePos);
            proc.processBlock(buf, midi);
            samplePos += hostBlock;
        }

        const int dragBlocks = 240;
        juce::AudioBuffer<float> collected(2, hostBlock * dragBlocks);
        for (int bk = 0; bk < dragBlocks; ++bk)
        {
            const double tBlock = static_cast<double>(bk) / static_cast<double>(dragBlocks);
            const double ph = juce::MathConstants<double>::twoPi * tBlock * 4.0;

            setFloat(apvts, "band0Freq",
                     static_cast<float>(1000.0 * std::pow(2.0, 0.55 * std::sin(ph))));
            setFloat(apvts, "band0Q",
                     static_cast<float>(3.0 + 2.0 * std::sin(ph * 1.3 + 0.4)));
            setFloat(apvts, "band0Gain",
                     static_cast<float>(8.0 + 4.0 * std::sin(ph * 0.7 + 1.0)));

            fillAM(buf, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, bk * hostBlock, buf, ch, 0, hostBlock);
            samplePos += hostBlock;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in 4x oversampled DynEQ band drag");
        expect(!metrics.hasInf, "Inf in 4x oversampled DynEQ band drag");
        expect(metrics.clickCount == 0,
               "Click while dragging actively-compressing DynEQ band at 4x OS: count="
               + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  DynEQ band drag @4x OS: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Test 7: Adversarial block sizes — bypass exit with hostile block sizes
    //
    // Real hosts don't always send 128-sample blocks. This test verifies
    // the bypass state machine and crossfade work correctly with block
    // sizes from 1 to 1024, including primes and off-by-one boundaries.
    //==========================================================================
    void testAdversarialBlockSizeBypass()
    {
        beginTest("Bypass exit with adversarial block sizes produces no click");

        const int blockSizes[] = { 1, 7, 31, 64, 127, 256, 513, 1024 };

        for (int bs : blockSizes)
        {
            AIEqualizerAudioProcessor proc;
            proc.setPlayConfigDetails(2, 2, kSampleRate, bs);
            proc.prepareToPlay(kSampleRate, bs);
            auto& apvts = proc.getAPVTS();

            setFloat(apvts, "band0Freq", 1000.0f);
            setFloat(apvts, "band0Gain", 12.0f);
            setFloat(apvts, "band0Q", 4.0f);
            setChoice(apvts, "band0Type", 2); // Peak
            setBool(apvts, "band0Enabled", true);

            juce::AudioBuffer<float> buf(2, bs);
            juce::MidiBuffer midi;
            int samplePos = 0;

            // Warm up: ~2560 samples worth
            for (int s = 0; s < 2560; s += bs)
            {
                fillSine(buf, 1000.0, 0.5f, samplePos);
                proc.processBlock(buf, midi);
                samplePos += bs;
            }

            // Engage bypass
            setBool(apvts, "bypass", true);

            // Steady-state bypass: ~12800 samples (crossfade completes + settle)
            for (int s = 0; s < 12800; s += bs)
            {
                fillSine(buf, 1000.0, 0.5f, samplePos);
                proc.processBlock(buf, midi);
                samplePos += bs;
            }

            // Disengage bypass
            setBool(apvts, "bypass", false);

            // Collect crossfade output: ~6144 samples
            const int collectBlocks = (6144 + bs - 1) / bs;
            juce::AudioBuffer<float> collected(2, collectBlocks * bs);
            for (int b = 0; b < collectBlocks; ++b)
            {
                fillSine(buf, 1000.0, 0.5f, samplePos);
                proc.processBlock(buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    collected.copyFrom(ch, b * bs, buf, ch, 0, bs);
                samplePos += bs;
            }

            auto metrics = analyzeClicks(collected);
            expect(!metrics.hasNaN, "NaN at blockSize=" + juce::String(bs));
            expect(!metrics.hasInf, "Inf at blockSize=" + juce::String(bs));
            expect(metrics.clickCount == 0,
                   "Click at blockSize=" + juce::String(bs)
                   + " count=" + juce::String(metrics.clickCount)
                   + " maxDelta=" + juce::String(metrics.maxDelta, 4));

            logMessage("  blockSize=" + juce::String(bs)
                       + ": maxDelta=" + juce::String(metrics.maxDelta, 4)
                       + " clicks=" + juce::String(metrics.clickCount));
        }
    }

    //==========================================================================
    // Test 6: Storm bypass toggle — toggle EVERY block for 200 blocks
    //
    // Much more aggressive than Test 2 (every 3 blocks). This hammers
    // the bypass state machine's proportional reversal logic, ensuring
    // that rapid direction changes never produce a discontinuity.
    //==========================================================================
    void testStormBypassToggle()
    {
        beginTest("Storm bypass toggle (every block × 200) produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        setFloat(apvts, "band0Freq", 2000.0f);
        setFloat(apvts, "band0Gain", 10.0f);
        setFloat(apvts, "band0Q", 3.0f);
        setChoice(apvts, "band0Type", 2);
        setBool(apvts, "band0Enabled", true);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up
        for (int b = 0; b < 10; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Storm: toggle bypass EVERY block for 200 blocks
        const int stormBlocks = 200;
        juce::AudioBuffer<float> collected(2, kBlockSize * stormBlocks);
        bool bypassed = false;
        for (int b = 0; b < stormBlocks; ++b)
        {
            bypassed = !bypassed;
            setBool(apvts, "bypass", bypassed);

            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        // Let it settle for 30 more blocks after the storm
        for (int b = 0; b < 30; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, (stormBlocks > 0 ? 0 : 0), buf, ch, 0, 0);
                // settle only — don't collect (buffer already full)
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in storm bypass");
        expect(!metrics.hasInf, "Inf in storm bypass");
        expect(metrics.clickCount == 0,
               "Click in storm bypass: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  storm bypass: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }

    //==========================================================================
    // Test 7: Storm parameter burst — change freq+gain+Q every block
    //
    // Simulates a user dragging multiple parameters simultaneously at
    // maximum speed. Each block gets new coefficients for band 0.
    // Verifies that the ParametricEQ crossfade handles rapid coefficient
    // changes without discontinuity.
    //==========================================================================
    void testStormParameterBurst()
    {
        beginTest("Storm parameter burst (freq+gain+Q every block × 100) produces no click");

        AIEqualizerAudioProcessor proc;
        prepareProcessor(proc);
        auto& apvts = proc.getAPVTS();

        setFloat(apvts, "band0Freq", 200.0f);
        setFloat(apvts, "band0Gain", 0.0f);
        setFloat(apvts, "band0Q", 1.0f);
        setChoice(apvts, "band0Type", 2);
        setBool(apvts, "band0Enabled", true);

        juce::AudioBuffer<float> buf(2, kBlockSize);
        juce::MidiBuffer midi;
        int samplePos = 0;

        // Warm up
        for (int b = 0; b < 20; ++b)
        {
            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            samplePos += kBlockSize;
        }

        // Storm: change 3 parameters every block for 100 blocks
        const int burstBlocks = 100;
        juce::AudioBuffer<float> collected(2, kBlockSize * burstBlocks);
        for (int b = 0; b < burstBlocks; ++b)
        {
            // Sweep freq 200 → 8000 Hz
            float freq = 200.0f + (7800.0f * static_cast<float>(b) / 99.0f);
            // Sweep gain 0 → 15 → 0 dB (triangle)
            float gain = (b < 50)
                ? (15.0f * static_cast<float>(b) / 49.0f)
                : (15.0f * static_cast<float>(99 - b) / 49.0f);
            // Sweep Q 0.5 → 10
            float q = 0.5f + (9.5f * static_cast<float>(b) / 99.0f);

            setFloat(apvts, "band0Freq", freq);
            setFloat(apvts, "band0Gain", gain);
            setFloat(apvts, "band0Q", q);

            fillSine(buf, 440.0, 0.5f, samplePos);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                collected.copyFrom(ch, b * kBlockSize, buf, ch, 0, kBlockSize);
            samplePos += kBlockSize;
        }

        auto metrics = analyzeClicks(collected);
        expect(!metrics.hasNaN, "NaN in parameter burst");
        expect(!metrics.hasInf, "Inf in parameter burst");
        expect(metrics.clickCount == 0,
               "Click in parameter burst: count=" + juce::String(metrics.clickCount)
               + " maxDelta=" + juce::String(metrics.maxDelta, 4));

        logMessage("  parameter burst: maxDelta=" + juce::String(metrics.maxDelta, 4)
                   + " clicks=" + juce::String(metrics.clickCount)
                   + " peakAbs=" + juce::String(metrics.peakAbs, 4));
    }
};

static AntiPopRegressionTest antiPopRegressionTest;
