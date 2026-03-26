#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../PluginProcessor.h"

/**
 * SoloModeTransitionTest
 *
 * Verifies that enabling/disabling solo acoustic monitor and dragging frequency
 * during solo are click-free. The plugin has:
 *   - Enable crossfade: 256 samples (~5ms @ 48kHz) with IIR warmup
 *   - Disable: may lack fade-out (potential real bug)
 *   - Freq drag during solo: coefficient update without reset
 */
class SoloModeTransitionTest : public juce::UnitTest
{
public:
    SoloModeTransitionTest()
        : juce::UnitTest("Solo Mode Transition", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testSoloEnable();
        testSoloDisable();
        testFreqDragDuringSolo();
    }

private:
    //==========================================================================
    // Thresholds
    //==========================================================================
    static constexpr float kMaxDelta   = 0.6f;
    static constexpr float kPeakAbs    = 2.5f;
    static constexpr int   kMaxDropout = 10;
    static constexpr float kEnergyHi   = 8.0f;

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

    void expectMetricsSolo(const TransitionMetrics& m, const juce::String& label)
    {
        expect(!m.hasNaN, label + ": NaN detected");
        expect(!m.hasInf, label + ": Inf detected");
        expect(m.maxDelta < kMaxDelta,
               label + ": maxDelta too high = " + juce::String(m.maxDelta, 4));
        expect(m.peakAbs < kPeakAbs,
               label + ": peakAbs too high = " + juce::String(m.peakAbs, 4));
        expect(m.dropoutSamples <= kMaxDropout,
               label + ": dropoutSamples = " + juce::String(m.dropoutSamples));
        expect(m.energyRatio < kEnergyHi,
               label + ": energyRatio too high = " + juce::String(m.energyRatio, 4));
    }

    void setupProcessor(AIEqualizerAudioProcessor& proc,
                        juce::AudioProcessorValueTreeState& apvts,
                        bool soloEnabled)
    {
        proc.setNumActiveBands(1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = 1000.0f;
        band.gain      = 6.0f;
        band.q         = 1.0f;
        band.type      = static_cast<int>(ParametricEQProcessor::Peak);
        band.enabled   = true;
        band.solo      = soloEnabled;
        proc.setBandState(0, band);

        setChoice(apvts, "numActiveBands", 0);
        setBool(apvts,   "bypass", false);
        setFloat(apvts,  "dryWet", 100.0f);
        setFloat(apvts,  "outputGain", 0.0f);
        setChoice(apvts, "qualityMode", 1);
        setChoice(apvts, "phaseMode", 0);
        setChoice(apvts, "oversamplingFactor", 0);
        setChoice(apvts, "msMode", 0);
    }

    //==========================================================================
    // Sub-test A: Enable solo
    //==========================================================================
    void testSoloEnable()
    {
        beginTest("Solo enable transition (crossfade 256 samples)");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 60;
        constexpr int switchBlock = 30;

        // Baseline: no switch, solo stays off
        AIEqualizerAudioProcessor procBase;
        procBase.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procBase, procBase.getAPVTS(), false);

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

        // Switched: enable solo at switchBlock
        AIEqualizerAudioProcessor procSw;
        procSw.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procSw, procSw.getAPVTS(), false);

        juce::AudioBuffer<float> swOutput(2, blockSize * numBlocks);
        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);

            if (block == switchBlock)
                setBool(procSw.getAPVTS(), "band0Solo", true);

            procSw.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                swOutput.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        procSw.releaseResources();

        const int start  = juce::jmax(0, switchBlock * blockSize - 256);
        const int window = juce::jmin(swOutput.getNumSamples() - start,
                                       10 * blockSize + 256);
        auto m = analyzeWindow(swOutput, start, window, &baseOutput);

        logMessage("  Enable: maxDelta=" + juce::String(m.maxDelta, 4)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.dropoutSamples));

        expectMetricsSolo(m, "Solo enable");
    }

    //==========================================================================
    // Sub-test B: Disable solo
    //==========================================================================
    void testSoloDisable()
    {
        beginTest("Solo disable transition");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 60;
        constexpr int switchBlock = 30;

        // Baseline: solo stays on
        AIEqualizerAudioProcessor procBase;
        procBase.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procBase, procBase.getAPVTS(), true);

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

        // Switched: disable solo at switchBlock
        AIEqualizerAudioProcessor procSw;
        procSw.prepareToPlay(sampleRate, blockSize);
        setupProcessor(procSw, procSw.getAPVTS(), true);

        juce::AudioBuffer<float> swOutput(2, blockSize * numBlocks);
        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);

            if (block == switchBlock)
                setBool(procSw.getAPVTS(), "band0Solo", false);

            procSw.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                swOutput.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        procSw.releaseResources();

        const int start  = juce::jmax(0, switchBlock * blockSize - 256);
        const int window = juce::jmin(swOutput.getNumSamples() - start,
                                       10 * blockSize + 256);
        auto m = analyzeWindow(swOutput, start, window, &baseOutput);

        logMessage("  Disable: maxDelta=" + juce::String(m.maxDelta, 4)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.dropoutSamples));

        expectMetricsSolo(m, "Solo disable");
    }

    //==========================================================================
    // Sub-test C: Freq drag during solo
    //==========================================================================
    void testFreqDragDuringSolo()
    {
        beginTest("Frequency drag during solo (1kHz -> 4kHz)");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize    = 128;
        constexpr int numBlocks    = 60;
        constexpr int dragStart    = 20;
        constexpr int dragEnd      = 40;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sampleRate, blockSize);
        setupProcessor(proc, proc.getAPVTS(), true); // solo on

        juce::AudioBuffer<float> output(2, blockSize * numBlocks);
        juce::MidiBuffer midi;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            // White-ish broadband input so solo bandpass has something at all frequencies
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = chunk.getWritePointer(ch);
                for (int i = 0; i < blockSize; ++i)
                {
                    // Simple pseudo-broadband: sum of sines
                    const int samp = block * blockSize + i;
                    const double t = static_cast<double>(samp) / sampleRate;
                    data[i] = 0.1f * static_cast<float>(
                        std::sin(juce::MathConstants<double>::twoPi * 500.0 * t) +
                        std::sin(juce::MathConstants<double>::twoPi * 1000.0 * t) +
                        std::sin(juce::MathConstants<double>::twoPi * 2000.0 * t) +
                        std::sin(juce::MathConstants<double>::twoPi * 4000.0 * t) +
                        std::sin(juce::MathConstants<double>::twoPi * 8000.0 * t));
                }
            }

            // Drag frequency from 1kHz to 4kHz over 20 blocks
            if (block >= dragStart && block <= dragEnd)
            {
                const float t = static_cast<float>(block - dragStart) /
                                static_cast<float>(dragEnd - dragStart);
                const float freq = 1000.0f + t * 3000.0f;
                setFloat(proc.getAPVTS(), "band0Freq", freq);
            }

            proc.processBlock(chunk, midi);
            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }
        proc.releaseResources();

        // Analyze the drag window
        const int start  = dragStart * blockSize;
        const int window = (dragEnd - dragStart + 5) * blockSize;
        auto m = analyzeWindow(output, start, window);

        logMessage("  Drag: maxDelta=" + juce::String(m.maxDelta, 4)
                   + " peakAbs=" + juce::String(m.peakAbs, 4)
                   + " dropout=" + juce::String(m.dropoutSamples));

        expect(!m.hasNaN, "Freq drag solo: NaN detected");
        expect(!m.hasInf, "Freq drag solo: Inf detected");
        expect(m.maxDelta < kMaxDelta,
               "Freq drag solo: maxDelta too high = " + juce::String(m.maxDelta, 4));
        expect(m.peakAbs < kPeakAbs,
               "Freq drag solo: peakAbs too high = " + juce::String(m.peakAbs, 4));
        expect(m.dropoutSamples <= kMaxDropout,
               "Freq drag solo: dropoutSamples = " + juce::String(m.dropoutSamples));
    }
};

static SoloModeTransitionTest soloModeTransitionTest;
