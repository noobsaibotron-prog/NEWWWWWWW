#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"
#include <cmath>

/**
 * RB-4 Behavioral Verification Test
 *
 * Proves that the qualityMode / lookahead fix produces a REAL DSP effect:
 * when lookahead is enabled (HQ mode), the dynamic EQ's gain reduction
 * engages BEFORE the transient reaches the EQ filter, resulting in measurably
 * lower peak overshoot compared to Zero Latency mode.
 *
 * Test signal: silence → full-scale step → steady tone
 * Dynamic EQ: compress mode, strong boost, low threshold
 *
 * Expected: HQ peak < ZL peak in the first ms after the transient,
 * because the detector "sees ahead" by 5ms.
 */
class RB4BehavioralTest : public juce::UnitTest
{
public:
    RB4BehavioralTest() : juce::UnitTest("RB-4 Behavioral", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testLookaheadReducesTransientOvershoot();
        testLookaheadSamplesActuallyChange();
        testRuntimeSwitchChangesProcessing();
    }

private:
    // ── helpers ──────────────────────────────────────────────────────────

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

    static float readParam(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& id)
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return -9999.0f;
    }

    /**
     * Configure the processor for dynamic EQ compression test.
     * Band 0: Peak @ 1kHz, +12 dB boost, compress mode, low threshold.
     * This maximizes the difference between ZL and HQ.
     */
    static void configureDynEQ(AIEqualizerAudioProcessor& proc,
                                juce::AudioProcessorValueTreeState& apvts,
                                int qualityMode)
    {
        proc.setNumActiveBands(1);

        // Quality mode: 0 = Zero Latency, 1 = High Quality
        setChoice(apvts, "qualityMode", qualityMode);

        // Enable dynamic EQ at full mix
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 100.0f);
        setBool(apvts, "dynAutoMakeup", false);

        // Band 0: Peak @ 1kHz, strong boost
        AIEqualizerAudioProcessor::BandState b;
        b.frequency = 1000.0f;
        b.gain      = 12.0f;
        b.q         = 1.0f;
        b.type      = static_cast<int>(ParametricEQProcessor::Peak);
        b.enabled   = true;
        b.solo      = false;
        // Dynamic: compress mode, low threshold, high ratio, fast attack
        b.dynMode      = 1;    // compress
        b.dynThreshold = -40.0f;
        b.dynRatio     = 8.0f;
        b.dynAttack    = 1.0f;   // 1ms attack
        b.dynRelease   = 100.0f;
        b.dynRange     = 24.0f;
        b.dynKnee      = 0.0f;
        proc.setBandState(0, b);

        // Dry/wet 100%, output gain 0
        setFloat(apvts, "dryWet", 100.0f);
        setFloat(apvts, "outputGain", 0.0f);
        setBool(apvts, "bypass", false);

        // Phase mode: zero latency (to isolate the dynamic EQ lookahead effect)
        setChoice(apvts, "phaseMode", 0);
        // No oversampling (clean comparison)
        setChoice(apvts, "oversamplingFactor", 0);
    }

    /**
     * Generate a step transient signal:
     *   - silenceBlocks blocks of silence
     *   - transientBlocks blocks of 1kHz sine at specified amplitude
     *
     * Returns the full buffer.
     */
    static juce::AudioBuffer<float> makeStepSignal(double sampleRate,
                                                    int blockSize,
                                                    int silenceBlocks,
                                                    int transientBlocks,
                                                    float amplitude)
    {
        const int totalSamples = (silenceBlocks + transientBlocks) * blockSize;
        juce::AudioBuffer<float> signal(2, totalSamples);
        signal.clear();

        // Fill transient portion with 1kHz sine
        const double freq = 1000.0;
        const int transientStart = silenceBlocks * blockSize;
        for (int s = transientStart; s < totalSamples; ++s)
        {
            const float val = amplitude * std::sin(
                static_cast<float>(2.0 * juce::MathConstants<double>::pi * freq *
                                   (s - transientStart) / sampleRate));
            signal.setSample(0, s, val);
            signal.setSample(1, s, val);
        }
        return signal;
    }

    /**
     * Process a full signal through the processor, block by block.
     * Returns the output buffer.
     */
    static juce::AudioBuffer<float> processSignal(AIEqualizerAudioProcessor& proc,
                                                   const juce::AudioBuffer<float>& input,
                                                   int blockSize)
    {
        const int totalSamples = input.getNumSamples();
        const int numBlocks = totalSamples / blockSize;
        juce::AudioBuffer<float> output(2, totalSamples);
        output.clear();

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block(2, blockSize);

        for (int b = 0; b < numBlocks; ++b)
        {
            const int offset = b * blockSize;
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom(ch, 0, input, ch, offset, blockSize);

            proc.processBlock(block, midi);

            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom(ch, offset, block, ch, 0, blockSize);
        }
        return output;
    }

    /**
     * Measure peak absolute value in a sample range.
     */
    static float peakInRange(const juce::AudioBuffer<float>& buf,
                              int startSample, int numSamples)
    {
        float peak = 0.0f;
        const int end = juce::jmin(startSample + numSamples, buf.getNumSamples());
        for (int s = startSample; s < end; ++s)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const float v = std::abs(buf.getSample(ch, s));
                if (v > peak) peak = v;
            }
        }
        return peak;
    }

    /**
     * Measure RMS in a sample range.
     */
    static float rmsInRange(const juce::AudioBuffer<float>& buf,
                             int startSample, int numSamples)
    {
        double sum = 0.0;
        int count = 0;
        const int end = juce::jmin(startSample + numSamples, buf.getNumSamples());
        for (int s = startSample; s < end; ++s)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const float v = buf.getSample(ch, s);
                sum += static_cast<double>(v) * v;
                ++count;
            }
        }
        return count > 0 ? static_cast<float>(std::sqrt(sum / count)) : 0.0f;
    }

    // ── Test 1: Lookahead reduces transient overshoot ────────────────────
    void testLookaheadReducesTransientOvershoot()
    {
        beginTest("RB-4 Behavioral: HQ lookahead reduces transient overshoot vs ZL");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;
        constexpr int silenceBlocks = 20;   // ~100ms of silence to settle
        constexpr int transientBlocks = 40; // ~210ms of transient
        constexpr int measureSamples = 96;  // 2ms @ 48 kHz

        // Generate test signal: silence → 1kHz burst at 0.8 amplitude
        auto signal = makeStepSignal(sampleRate, blockSize, silenceBlocks,
                                      transientBlocks, 0.8f);

        const int transientStart = silenceBlocks * blockSize;

        // IMPORTANT: Maximum Latency Padding (PluginProcessor::prepareToPlay) sets
        // worstCaseLatencySamples = max(lpLatency, oversamplingLatency) ≈ 2176 samples
        // even when phaseMode=0 and oversamplingFactor=0. The plugin shifts the output
        // by that amount via an internal delay line, so the transient at input position
        // P appears at output position P + getLatencySamples(). The old version of this
        // test measured at transientStart (and transientStart + 240 for HQ), both of
        // which fall inside the silence region of the OUTPUT buffer — effectively
        // measuring zeros. We now align to transientStart + latency for each mode.

        // ── Process in Zero Latency mode ──
        float peakZL, rmsZL;
        int latencyZL;
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(sampleRate, blockSize);
            auto& apvts = proc.getAPVTS();
            configureDynEQ(proc, apvts, 0); // ZL
            latencyZL = proc.getLatencySamples();

            // Prime: enough silent blocks to fill the latency pipeline + stabilize smoothers
            const int primeBlocks = juce::jmax(10, (latencyZL + blockSize - 1) / blockSize + 4);
            juce::AudioBuffer<float> prime(2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < primeBlocks; ++i) { prime.clear(); proc.processBlock(prime, midi); }

            auto outputZL = processSignal(proc, signal, blockSize);

            const int measureStart = juce::jmin(outputZL.getNumSamples() - measureSamples,
                                                 transientStart + latencyZL);
            peakZL = peakInRange(outputZL, measureStart, measureSamples);
            rmsZL  = rmsInRange(outputZL, measureStart, measureSamples);
        }

        // ── Process in High Quality mode ──
        float peakHQ, rmsHQ;
        int latencyHQ;
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(sampleRate, blockSize);
            auto& apvts = proc.getAPVTS();
            configureDynEQ(proc, apvts, 1); // HQ
            latencyHQ = proc.getLatencySamples();

            const int primeBlocks = juce::jmax(10, (latencyHQ + blockSize - 1) / blockSize + 4);
            juce::AudioBuffer<float> prime(2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < primeBlocks; ++i) { prime.clear(); proc.processBlock(prime, midi); }

            auto outputHQ = processSignal(proc, signal, blockSize);

            // Align to where the transient actually reaches the HQ output.
            // proc.getLatencySamples() already accounts for any lookahead delay,
            // so we do NOT add lookaheadSamples manually (the old test did and
            // measured silence).
            const int measureStart = juce::jmin(outputHQ.getNumSamples() - measureSamples,
                                                 transientStart + latencyHQ);
            peakHQ = peakInRange(outputHQ, measureStart, measureSamples);
            rmsHQ  = rmsInRange(outputHQ, measureStart, measureSamples);
        }

        // ── Log results ──
        logMessage("  latencyZL=" + juce::String(latencyZL) +
                   " latencyHQ=" + juce::String(latencyHQ));
        {
            juce::String msg;
            msg << "ZL peak=" << juce::String(peakZL, 4)
                << " rms=" << juce::String(rmsZL, 4)
                << "  |  HQ peak=" << juce::String(peakHQ, 4)
                << " rms=" << juce::String(rmsHQ, 4);
            logMessage(msg);

            if (peakZL > 0.0f && peakHQ > 0.0f)
            {
                const float peakReductionDb = 20.0f * std::log10(peakHQ / peakZL);
                logMessage("  Peak reduction (HQ vs ZL): " +
                           juce::String(peakReductionDb, 2) + " dB");
            }
        }

        // ── Assertions ──
        // Primary: HQ transient peak must be lower than ZL (lookahead pre-applies GR)
        expect(peakHQ < peakZL,
               "HQ peak (" + juce::String(peakHQ, 4) +
               ") should be lower than ZL peak (" + juce::String(peakZL, 4) +
               ") — lookahead should reduce transient overshoot");

        // Secondary: the difference should be non-trivial (at least 0.5 dB)
        if (peakZL > 0.0f && peakHQ > 0.0f)
        {
            const float deltaDb = 20.0f * std::log10(peakZL / peakHQ);
            expect(deltaDb > 0.5f,
                   "Peak reduction should be > 0.5 dB, got " +
                   juce::String(deltaDb, 2) + " dB");
        }
    }

    // ── Test 2: HQ and ZL impulse responses differ ───────────────────────
    void testLookaheadSamplesActuallyChange()
    {
        beginTest("RB-4 Behavioral: HQ and ZL impulse responses differ (lookahead delay)");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;

        // Maximum Latency Padding means the plugin reports ≥2176 samples of latency
        // regardless of mode. A single 256-sample block cannot contain the impulse's
        // tail — the impulse sitting at sample 0 would still be inside the delay line
        // by the end of the first block. We therefore process multiple blocks, sized
        // to fully span the latency pipeline, and capture the concatenated output
        // for each mode using two FRESH processors (avoids state bleed between modes).

        // Probe latency from a ZL processor to size the capture window
        int latency = 0;
        {
            AIEqualizerAudioProcessor probe;
            probe.prepareToPlay(sampleRate, blockSize);
            auto& apvts = probe.getAPVTS();
            configureDynEQ(probe, apvts, 0);
            latency = probe.getLatencySamples();
            probe.releaseResources();
        }

        const int totalBlocks = juce::jmax(
            4, (latency + 2 * blockSize + blockSize - 1) / blockSize);
        const int totalSamples = totalBlocks * blockSize;

        auto processImpulse = [sampleRate, blockSize, totalBlocks]
                              (int qualityMode)
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(sampleRate, blockSize);
            auto& apvts = proc.getAPVTS();
            configureDynEQ(proc, apvts, qualityMode);

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> prime(2, blockSize);
            for (int i = 0; i < 10; ++i) { prime.clear(); proc.processBlock(prime, midi); }

            juce::AudioBuffer<float> output(2, totalBlocks * blockSize);
            output.clear();
            juce::AudioBuffer<float> block(2, blockSize);

            for (int b = 0; b < totalBlocks; ++b)
            {
                block.clear();
                if (b == 0)
                {
                    block.setSample(0, 0, 1.0f);
                    block.setSample(1, 0, 1.0f);
                }
                proc.processBlock(block, midi);
                for (int ch = 0; ch < 2; ++ch)
                    output.copyFrom(ch, b * blockSize, block, ch, 0, blockSize);
            }
            proc.releaseResources();
            return output;
        };

        auto zlOutput = processImpulse(0);
        auto hqOutput = processImpulse(1);

        // Compare: HQ should differ from ZL somewhere in the response
        bool differ = false;
        float maxDiff = 0.0f;
        for (int s = 0; s < totalSamples; ++s)
        {
            const float d = std::abs(hqOutput.getSample(0, s) - zlOutput.getSample(0, s));
            maxDiff = std::max(maxDiff, d);
            if (d > 1e-6f) differ = true;
        }

        logMessage("  latency=" + juce::String(latency) +
                   " totalSamples=" + juce::String(totalSamples) +
                   " maxDiff=" + juce::String(maxDiff, 6));

        expect(differ, "HQ and ZL impulse responses should differ (lookahead delay)");
    }

    // ── Test 3: runtime switch actually changes processing ───────────────
    void testRuntimeSwitchChangesProcessing()
    {
        beginTest("RB-4 Behavioral: runtime switch from ZL to HQ changes output");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sampleRate, blockSize);
        auto& apvts = proc.getAPVTS();
        configureDynEQ(proc, apvts, 0); // start ZL

        const int latency = proc.getLatencySamples();
        // Need to push enough tone samples through the latency pipeline before we can
        // measure steady-state RMS. Previously primed 10 silent blocks (2560 samples)
        // then processed ONE tone block and measured RMS on the output of that block —
        // but the output still contained silence-pipeline residue because the latency
        // pad is ≥2176 samples.
        const int latencyBlocks = (latency + blockSize - 1) / blockSize + 2;

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> silence(2, blockSize);

        // Prime with silence (enough to clear any constructor transient)
        for (int i = 0; i < latencyBlocks; ++i)
        {
            silence.clear();
            proc.processBlock(silence, midi);
        }

        // Helper: fill block with continuous 1kHz sine at the given global sample offset
        auto fillTone = [sampleRate, blockSize](juce::AudioBuffer<float>& block, int globalOffset)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                const float v = 0.8f * std::sin(static_cast<float>(
                    2.0 * juce::MathConstants<double>::pi * 1000.0 *
                    (globalOffset + s) / sampleRate));
                block.setSample(0, s, v);
                block.setSample(1, s, v);
            }
        };

        // Push tone through the ZL pipeline until the output reaches steady state.
        juce::AudioBuffer<float> toneBlock(2, blockSize);
        int globalSample = 0;
        for (int b = 0; b < latencyBlocks + 2; ++b)
        {
            fillTone(toneBlock, globalSample);
            globalSample += blockSize;
            proc.processBlock(toneBlock, midi);
        }
        // toneBlock now holds the OUTPUT of the last processed ZL block (steady state).
        const float rmsBeforeSwitch = toneBlock.getRMSLevel(0, 0, blockSize);

        // Switch to HQ at runtime
        setChoice(apvts, "qualityMode", 1);

        // Let the switch propagate: push enough tone blocks for the HQ pipeline to fill.
        for (int b = 0; b < latencyBlocks + 4; ++b)
        {
            fillTone(toneBlock, globalSample);
            globalSample += blockSize;
            proc.processBlock(toneBlock, midi);
        }
        // toneBlock now holds the OUTPUT of the last processed HQ block (steady state).
        const float rmsAfterSwitch = toneBlock.getRMSLevel(0, 0, blockSize);

        logMessage("  latency=" + juce::String(latency));
        logMessage("  RMS before switch (ZL): " + juce::String(rmsBeforeSwitch, 6));
        logMessage("  RMS after switch (HQ):  " + juce::String(rmsAfterSwitch, 6));

        // The outputs should differ — the lookahead delay + different GR timing
        // changes the output characteristics
        const float rmsDelta = std::abs(rmsBeforeSwitch - rmsAfterSwitch);
        expect(rmsDelta > 1e-4f,
               "Output should change after ZL→HQ switch. Delta=" +
               juce::String(rmsDelta, 6));
    }
};

static RB4BehavioralTest rb4BehavioralTest;
