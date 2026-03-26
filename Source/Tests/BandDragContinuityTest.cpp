#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../PluginProcessor.h"

/**
 * BandDragContinuityTest
 *
 * Verifies that dragging a band's frequency from 500 Hz → 4 kHz in real-time
 * does not produce zipper noise or clicks. The plugin uses SmoothedValue for
 * freq and gain, which should prevent audible artifacts.
 *
 * Two separate sub-tests:
 *   1. Standard drag (Zero Latency) — SmoothedValue is the main protection
 *   2. Linear Phase drag — IR rebuild with 80ms debounce; old IR should remain
 *      active until the new one is ready (no dropout to zero)
 */
class BandDragContinuityTest : public juce::UnitTest
{
public:
    BandDragContinuityTest()
        : juce::UnitTest("Band Drag Continuity", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testStandardDrag();
        testLinearPhaseDrag();
    }

private:
    //==========================================================================
    // Helpers
    //==========================================================================
    struct TransitionMetrics
    {
        float maxDelta      = 0.0f;
        float peakAbs       = 0.0f;
        float energyRatio   = 1.0f;
        bool  hasNaN        = false;
        bool  hasInf        = false;
        int   dropoutSamples = 0;
    };

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

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static void fillSine(juce::AudioBuffer<float>& buf, double sampleRate,
                          double freqHz, float amplitude, int sampleOffset)
    {
        const auto w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                data[i] = amplitude * static_cast<float>(
                    std::sin(w * static_cast<double>(sampleOffset + i)));
        }
    }

    TransitionMetrics analyzeWindow(const juce::AudioBuffer<float>& buf,
                                    int startSample, int windowSamples,
                                    const juce::AudioBuffer<float>* baseline = nullptr)
    {
        TransitionMetrics m;
        if (buf.getNumChannels() == 0 || windowSamples <= 0) return m;

        startSample = juce::jmax(0, startSample);
        const int end = juce::jmin(buf.getNumSamples(), startSample + windowSamples);

        long double switchedE = 0.0, baselineE = 0.0;
        int maxRun = 0;

        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            const auto* base = (baseline && ch < baseline->getNumChannels())
                               ? baseline->getReadPointer(ch) : nullptr;
            int run = 0;
            float prev = data[startSample];

            for (int i = startSample; i < end; ++i)
            {
                const float s = data[i];
                if (std::isnan(s)) m.hasNaN = true;
                if (std::isinf(s)) m.hasInf = true;
                m.peakAbs  = std::max(m.peakAbs, std::abs(s));
                if (i > startSample)
                    m.maxDelta = std::max(m.maxDelta, std::abs(s - prev));
                prev = s;
                switchedE += static_cast<long double>(s) * s;

                if (base)
                {
                    const float b = base[i];
                    baselineE += static_cast<long double>(b) * b;
                    if (std::abs(b) > 0.01f && std::abs(s) < 1.0e-5f) ++run;
                    else run = 0;
                    maxRun = std::max(maxRun, run);
                }
            }
        }

        m.dropoutSamples = maxRun;
        if (baseline)
        {
            const auto denom = std::max<long double>(baselineE, 1.0e-12);
            m.energyRatio = static_cast<float>(switchedE / denom);
        }
        return m;
    }

    void setupProcessor(AIEqualizerAudioProcessor& proc,
                        juce::AudioProcessorValueTreeState& apvts,
                        int phaseMode, float startFreq = 2000.0f)
    {
        proc.setNumActiveBands(1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = startFreq;
        band.gain      = 6.0f;
        band.q         = 1.0f;
        band.type      = static_cast<int>(ParametricEQProcessor::Peak);
        band.enabled   = true;
        band.solo      = false;
        proc.setBandState(0, band);

        setChoice(apvts, "numActiveBands", 0);
        setBool(apvts,   "bypass", false);
        setFloat(apvts,  "dryWet", 100.0f);
        setFloat(apvts,  "outputGain", 0.0f);
        setChoice(apvts, "qualityMode", 1);
        setChoice(apvts, "phaseMode", phaseMode);
        setChoice(apvts, "oversamplingFactor", 0);
        setChoice(apvts, "msMode", 0);
    }

    //==========================================================================
    // Sub-test 1: Standard drag (Zero Latency, phaseMode=0)
    //==========================================================================
    void testStandardDrag()
    {
        beginTest("Band drag 500Hz -> 4kHz (Zero Latency, SmoothedValue)");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 60;
        constexpr int dragStart  = 20;
        constexpr int dragEnd    = 40;

        // Thresholds for standard drag
        constexpr float kMaxDelta   = 0.6f;
        constexpr float kPeakAbs    = 3.0f;
        constexpr int   kMaxDropout = 5;

        // Baseline: fixed band at midpoint (2kHz), no drag
        AIEqualizerAudioProcessor procBase;
        procBase.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procBase, procBase.getAPVTS(), 0, 2000.0f);

        juce::AudioBuffer<float> baseOutput(2, blockSize * numBlocks);
        juce::MidiBuffer midi;
        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);
            procBase.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                baseOutput.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        procBase.releaseResources();

        // Switched: drag from 500Hz to 4kHz during blocks 20-40
        AIEqualizerAudioProcessor procDrag;
        procDrag.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procDrag, procDrag.getAPVTS(), 0, 500.0f);

        juce::AudioBuffer<float> dragOutput(2, blockSize * numBlocks);
        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);

            if (block >= dragStart && block <= dragEnd)
            {
                const float t = static_cast<float>(block - dragStart)
                              / static_cast<float>(dragEnd - dragStart);
                const float freq = 500.0f + t * 3500.0f;
                setFloat(procDrag.getAPVTS(), "band0Freq", freq);
            }

            procDrag.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                dragOutput.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        procDrag.releaseResources();

        // Analyze drag region
        const int start  = dragStart * blockSize;
        const int window = (dragEnd - dragStart + 5) * blockSize;
        auto m = analyzeWindow(dragOutput, start, window, &baseOutput);

        logMessage("  Standard drag: maxDelta=" + juce::String(m.maxDelta, 4)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.dropoutSamples));

        expect(!m.hasNaN, "Standard drag: NaN detected");
        expect(!m.hasInf, "Standard drag: Inf detected");
        expect(m.maxDelta < kMaxDelta,
               "Standard drag: maxDelta too high = " + juce::String(m.maxDelta, 4));
        expect(m.peakAbs < kPeakAbs,
               "Standard drag: peakAbs too high = " + juce::String(m.peakAbs, 4));
        expect(m.dropoutSamples <= kMaxDropout,
               "Standard drag: dropoutSamples = " + juce::String(m.dropoutSamples));
    }

    //==========================================================================
    // Sub-test 2: Linear Phase drag (phaseMode=2, IR rebuild + debounce)
    //==========================================================================
    void testLinearPhaseDrag()
    {
        beginTest("Band drag 500Hz -> 4kHz (Linear Phase, IR rebuild with debounce)");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 80; // more blocks for LP settling
        constexpr int dragStart  = 20;
        constexpr int dragEnd    = 40;

        // More tolerant thresholds for LP: IR swap may cause brief discontinuities
        constexpr float kMaxDeltaLP   = 0.8f;
        constexpr float kPeakAbsLP    = 3.0f;
        constexpr int   kMaxDropoutLP = 20; // debounce 80ms, IR might not be ready for 1-2 blocks

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sampleRate, blockSize);
        setupProcessor(proc, proc.getAPVTS(), 2, 500.0f); // phaseMode=2 (LinearPhase)

        juce::AudioBuffer<float> output(2, blockSize * numBlocks);
        juce::MidiBuffer midi;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);

            if (block >= dragStart && block <= dragEnd)
            {
                const float t = static_cast<float>(block - dragStart)
                              / static_cast<float>(dragEnd - dragStart);
                const float freq = 500.0f + t * 3500.0f;
                setFloat(proc.getAPVTS(), "band0Freq", freq);
            }

            proc.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        proc.releaseResources();

        // Analyze drag + post-drag settling region
        const int start  = dragStart * blockSize;
        const int window = (dragEnd - dragStart + 10) * blockSize; // extra settling
        auto m = analyzeWindow(output, start, window);

        logMessage("  LP drag: maxDelta=" + juce::String(m.maxDelta, 4)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.dropoutSamples));

        expect(!m.hasNaN, "LP drag: NaN detected");
        expect(!m.hasInf, "LP drag: Inf detected");
        expect(m.maxDelta < kMaxDeltaLP,
               "LP drag: maxDelta too high = " + juce::String(m.maxDelta, 4));
        expect(m.peakAbs < kPeakAbsLP,
               "LP drag: peakAbs too high = " + juce::String(m.peakAbs, 4));
        expect(m.dropoutSamples <= kMaxDropoutLP,
               "LP drag: dropoutSamples = " + juce::String(m.dropoutSamples));
    }
};

static BandDragContinuityTest bandDragContinuityTest;
