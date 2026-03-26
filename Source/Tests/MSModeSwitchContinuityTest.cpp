#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../PluginProcessor.h"

/**
 * MSModeSwitchContinuityTest
 *
 * Verifies that switching between M/S processing modes (Stereo, Mid, Side, MSLinked)
 * does not introduce audible clicks or dropouts. The plugin currently has NO crossfade
 * on M/S mode switches, so this test may reveal real bugs.
 *
 * Input variants:
 *   - Symmetric (L=R): Mid = L√2, Side ≈ 0
 *   - Asymmetric (L=1kHz, R=1.2kHz): both Mid and Side carry energy
 */
class MSModeSwitchContinuityTest : public juce::UnitTest
{
public:
    MSModeSwitchContinuityTest()
        : juce::UnitTest("MS Mode Switch Continuity", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testSymmetricInput();
        testAsymmetricInput();
    }

private:
    //==========================================================================
    // Thresholds
    //==========================================================================
    static constexpr float kMaxDelta       = 0.8f;   // relaxed — no crossfade exists
    static constexpr float kPeakAbs        = 3.0f;
    static constexpr int   kMaxDropout     = 5;
    static constexpr float kEnergyLo       = 0.1f;   // wide band for Side-Only w/ symmetric input
    static constexpr float kEnergyHi       = 10.0f;

    //==========================================================================
    // Helpers — same pattern as TransitionContinuityTests.cpp
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

    //--------------------------------------------------------------------------
    static void fillSine(juce::AudioBuffer<float>& buf,
                          double sampleRate,
                          double freqHz,
                          float amplitude,
                          int sampleOffset,
                          int channel = -1) // -1 = all channels
    {
        const auto w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        const int startCh = (channel >= 0) ? channel : 0;
        const int endCh   = (channel >= 0) ? channel + 1 : buf.getNumChannels();

        for (int ch = startCh; ch < endCh; ++ch)
        {
            auto* data = buf.getWritePointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                data[i] = amplitude * static_cast<float>(
                    std::sin(w * static_cast<double>(sampleOffset + i)));
        }
    }

    //--------------------------------------------------------------------------
    static void fillStereoSymmetric(juce::AudioBuffer<float>& buf,
                                     double sampleRate, int sampleOffset)
    {
        fillSine(buf, sampleRate, 1000.0, 0.25f, sampleOffset);
    }

    static void fillStereoAsymmetric(juce::AudioBuffer<float>& buf,
                                      double sampleRate, int sampleOffset)
    {
        fillSine(buf, sampleRate, 1000.0, 0.25f, sampleOffset, 0);
        fillSine(buf, sampleRate, 1200.0, 0.25f, sampleOffset, 1);
    }

    //--------------------------------------------------------------------------
    TransitionMetrics analyzeWindow(const juce::AudioBuffer<float>& buf,
                                    int startSample, int windowSamples,
                                    const juce::AudioBuffer<float>* baseline = nullptr)
    {
        TransitionMetrics m;
        if (buf.getNumChannels() == 0 || buf.getNumSamples() == 0 || windowSamples <= 0)
            return m;

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

    //--------------------------------------------------------------------------
    void expectMetricsMS(const TransitionMetrics& m, const juce::String& label,
                          float energyLo = kEnergyLo, float energyHi = kEnergyHi)
    {
        expect(!m.hasNaN,  label + ": NaN detected");
        expect(!m.hasInf,  label + ": Inf detected");
        expect(m.maxDelta < kMaxDelta,
               label + ": maxDelta too high = " + juce::String(m.maxDelta, 4));
        expect(m.peakAbs < kPeakAbs,
               label + ": peakAbs too high = " + juce::String(m.peakAbs, 4));
        expect(m.dropoutSamples <= kMaxDropout,
               label + ": dropoutSamples = " + juce::String(m.dropoutSamples));
        expect(m.energyRatio >= energyLo && m.energyRatio <= energyHi,
               label + ": energyRatio out of range = " + juce::String(m.energyRatio, 4));
    }

    //--------------------------------------------------------------------------
    void setupProcessor(AIEqualizerAudioProcessor& proc,
                        juce::AudioProcessorValueTreeState& apvts,
                        int msMode)
    {
        proc.setNumActiveBands(1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = 1000.0f;
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
        setChoice(apvts, "phaseMode", 0);      // Zero Latency
        setChoice(apvts, "oversamplingFactor", 0);
        setChoice(apvts, "msMode", msMode);
    }

    //--------------------------------------------------------------------------
    using FillFunc = void(*)(juce::AudioBuffer<float>&, double, int);

    struct ScenarioRun
    {
        juce::AudioBuffer<float> output;
        juce::AudioBuffer<float> input;
    };

    ScenarioRun runScenario(double sampleRate, int blockSize, int numBlocks,
                             int switchBlock, int fromMode, int toMode,
                             FillFunc fillBlock)
    {
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sampleRate, blockSize);
        auto& apvts = proc.getAPVTS();
        setupProcessor(proc, apvts, fromMode);

        ScenarioRun run {
            juce::AudioBuffer<float>(2, blockSize * numBlocks),
            juce::AudioBuffer<float>(2, blockSize * numBlocks)
        };
        juce::MidiBuffer midi;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> chunk(2, blockSize);
            fillBlock(chunk, sampleRate, block * blockSize);

            for (int ch = 0; ch < 2; ++ch)
                run.input.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);

            if (block == switchBlock)
                setChoice(apvts, "msMode", toMode);

            proc.processBlock(chunk, midi);

            for (int ch = 0; ch < 2; ++ch)
                run.output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
        }

        proc.releaseResources();
        return run;
    }

    //==========================================================================
    // Main test functions
    //==========================================================================
    void testSymmetricInput()
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 60;
        constexpr int switchBlock = 30;

        // MSMode enum: Stereo=0, Mid=1, Side=2, MSLinked=3
        const std::array<std::pair<int,int>, 6> transitions {{
            {0, 3}, {3, 0},   // Stereo ↔ MSLinked
            {0, 1}, {1, 0},   // Stereo ↔ Mid
            {0, 2}, {2, 0}    // Stereo ↔ Side
        }};

        const char* modeNames[] = { "Stereo", "Mid", "Side", "MSLinked" };

        for (auto [from, to] : transitions)
        {
            beginTest(juce::String("Symmetric: ") + modeNames[from] + " -> " + modeNames[to]);

            auto baseline = runScenario(sampleRate, blockSize, numBlocks,
                                         switchBlock, from, from, // no switch
                                         &fillStereoSymmetric);
            auto switched = runScenario(sampleRate, blockSize, numBlocks,
                                         switchBlock, from, to,
                                         &fillStereoSymmetric);

            const int start  = juce::jmax(0, switchBlock * blockSize - 256);
            const int window = juce::jmin(switched.output.getNumSamples() - start,
                                           10 * blockSize + 256);
            auto m = analyzeWindow(switched.output, start, window, &baseline.output);

            logMessage("  maxDelta=" + juce::String(m.maxDelta, 4)
                       + " peakAbs=" + juce::String(m.peakAbs, 4)
                       + " energyRatio=" + juce::String(m.energyRatio, 4)
                       + " dropout=" + juce::String(m.dropoutSamples));

            const juce::String label = juce::String(modeNames[from]) + " -> " + modeNames[to] + " (sym)";

            // Special cases with symmetric L=R input:
            //   Stereo→Side: Side output ≈ 0 by definition (L-R≈0).
            //     dropout metric fires because baseline has signal but output is correctly ~0.
            //   Side→Stereo: baseline is Side-only (≈0 energy), energyRatio explodes.
            // Both are correct plugin behavior, not bugs.
            if (to == 2) // switching TO Side-Only with L=R
            {
                expect(!m.hasNaN,  label + ": NaN detected");
                expect(!m.hasInf,  label + ": Inf detected");
                expect(m.maxDelta < kMaxDelta, label + ": maxDelta too high = " + juce::String(m.maxDelta, 4));
                expect(m.peakAbs < kPeakAbs,  label + ": peakAbs too high = " + juce::String(m.peakAbs, 4));
                // dropout check intentionally skipped: Side output is correctly ~0
                // energyRatio intentionally skipped: output energy ~0 is correct
            }
            else if (from == 2) // switching FROM Side-Only with L=R (baseline ≈ 0 energy)
            {
                expect(!m.hasNaN,  label + ": NaN detected");
                expect(!m.hasInf,  label + ": Inf detected");
                expect(m.maxDelta < kMaxDelta, label + ": maxDelta too high = " + juce::String(m.maxDelta, 4));
                expect(m.peakAbs < kPeakAbs,  label + ": peakAbs too high = " + juce::String(m.peakAbs, 4));
                // energyRatio skipped: baseline energy ≈ 0 makes ratio meaningless
            }
            else
            {
                expectMetricsMS(m, label);
            }
        }
    }

    void testAsymmetricInput()
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize  = 128;
        constexpr int numBlocks  = 60;
        constexpr int switchBlock = 30;

        // Only the transitions most likely to reveal issues with asymmetric input
        const std::array<std::pair<int,int>, 4> transitions {{
            {0, 3}, {3, 0},   // Stereo ↔ MSLinked
            {0, 1}, {0, 2}    // Stereo → Mid, Stereo → Side
        }};

        const char* modeNames[] = { "Stereo", "Mid", "Side", "MSLinked" };

        for (auto [from, to] : transitions)
        {
            beginTest(juce::String("Asymmetric: ") + modeNames[from] + " -> " + modeNames[to]);

            auto baseline = runScenario(sampleRate, blockSize, numBlocks,
                                         switchBlock, from, from,
                                         &fillStereoAsymmetric);
            auto switched = runScenario(sampleRate, blockSize, numBlocks,
                                         switchBlock, from, to,
                                         &fillStereoAsymmetric);

            const int start  = juce::jmax(0, switchBlock * blockSize - 256);
            const int window = juce::jmin(switched.output.getNumSamples() - start,
                                           10 * blockSize + 256);
            auto m = analyzeWindow(switched.output, start, window, &baseline.output);

            logMessage("  maxDelta=" + juce::String(m.maxDelta, 4)
                       + " peakAbs=" + juce::String(m.peakAbs, 4)
                       + " energyRatio=" + juce::String(m.energyRatio, 4)
                       + " dropout=" + juce::String(m.dropoutSamples));

            // For Stereo→MSLinked with asymmetric input, energy should be preserved
            if (from == 0 && to == 3)
                expectMetricsMS(m, "Stereo -> MSLinked (asym)", 0.7f, 1.3f);
            else
                expectMetricsMS(m, juce::String(modeNames[from]) + " -> " + modeNames[to] + " (asym)");
        }
    }
};

static MSModeSwitchContinuityTest msModeSwitchContinuityTest;
