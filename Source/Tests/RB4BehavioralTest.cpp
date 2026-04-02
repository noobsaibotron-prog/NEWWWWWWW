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

        // Lookahead in samples at 48kHz: 5ms * 48 = 240 samples
        constexpr int lookaheadSamples = 240;

        // Generate test signal: silence → 1kHz burst at 0.8 amplitude
        auto signal = makeStepSignal(sampleRate, blockSize, silenceBlocks,
                                      transientBlocks, 0.8f);

        const int transientStart = silenceBlocks * blockSize;

        // ── Process in Zero Latency mode ──
        float peakZL, rmsZL;
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(sampleRate, blockSize);
            auto& apvts = proc.getAPVTS();
            configureDynEQ(proc, apvts, 0); // ZL

            // Prime: run a few silent blocks to stabilize smoothers
            juce::AudioBuffer<float> prime(2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < 10; ++i) { prime.clear(); proc.processBlock(prime, midi); }

            auto outputZL = processSignal(proc, signal, blockSize);

            // Measure peak in first 2ms after transient start (96 samples @ 48kHz)
            peakZL = peakInRange(outputZL, transientStart, 96);
            rmsZL  = rmsInRange(outputZL, transientStart, 96);
        }

        // ── Process in High Quality mode ──
        float peakHQ, rmsHQ;
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(sampleRate, blockSize);
            auto& apvts = proc.getAPVTS();
            configureDynEQ(proc, apvts, 1); // HQ

            // Prime
            juce::AudioBuffer<float> prime(2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < 10; ++i) { prime.clear(); proc.processBlock(prime, midi); }

            auto outputHQ = processSignal(proc, signal, blockSize);

            // HQ output is delayed by lookahead samples — measure at compensated position
            peakHQ = peakInRange(outputHQ, transientStart + lookaheadSamples, 96);
            rmsHQ  = rmsInRange(outputHQ, transientStart + lookaheadSamples, 96);
        }

        // ── Log results ──
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

    // ── Test 2: lookaheadSamples atomic actually changes ─────────────────
    void testLookaheadSamplesActuallyChange()
    {
        beginTest("RB-4 Behavioral: lookaheadSamples value changes with qualityMode");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(sampleRate, blockSize);
        auto& apvts = proc.getAPVTS();

        // Start ZL
        setChoice(apvts, "qualityMode", 0);

        // Process a block to trigger the qualityMode change path in processBlock
        juce::AudioBuffer<float> buf(2, blockSize);
        juce::MidiBuffer midi;
        buf.clear();
        proc.processBlock(buf, midi);

        // Read lookahead from the dynamic EQ processor
        // We access it indirectly: process two identical signals, compare latency
        // Simpler: just verify that the output differs between ZL and HQ

        // Switch to HQ
        setChoice(apvts, "qualityMode", 1);
        buf.clear();
        proc.processBlock(buf, midi);

        // Generate a short impulse signal
        juce::AudioBuffer<float> impulse(2, blockSize);
        impulse.clear();
        impulse.setSample(0, 0, 1.0f);
        impulse.setSample(1, 0, 1.0f);

        // Process the impulse in HQ mode
        juce::AudioBuffer<float> hqImpulse(2, blockSize);
        hqImpulse.makeCopyOf(impulse);
        proc.processBlock(hqImpulse, midi);

        // Switch back to ZL
        setChoice(apvts, "qualityMode", 0);
        buf.clear();
        proc.processBlock(buf, midi);

        // Process same impulse in ZL mode
        juce::AudioBuffer<float> zlImpulse(2, blockSize);
        zlImpulse.makeCopyOf(impulse);
        proc.processBlock(zlImpulse, midi);

        // The outputs must differ (lookahead delays audio in HQ)
        bool differ = false;
        for (int s = 0; s < blockSize && !differ; ++s)
        {
            if (std::abs(hqImpulse.getSample(0, s) - zlImpulse.getSample(0, s)) > 1e-6f)
                differ = true;
        }
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

        juce::MidiBuffer midi;

        // Prime with silence
        juce::AudioBuffer<float> silence(2, blockSize);
        for (int i = 0; i < 10; ++i) { silence.clear(); proc.processBlock(silence, midi); }

        // Process a 1kHz tone block in ZL mode
        juce::AudioBuffer<float> toneZL(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            float v = 0.8f * std::sin(static_cast<float>(
                2.0 * juce::MathConstants<double>::pi * 1000.0 * s / sampleRate));
            toneZL.setSample(0, s, v);
            toneZL.setSample(1, s, v);
        }
        proc.processBlock(toneZL, midi);
        const float rmsBeforeSwitch = toneZL.getRMSLevel(0, 0, blockSize);

        // Now switch to HQ at runtime
        setChoice(apvts, "qualityMode", 1);

        // Let the switch take effect through several blocks
        for (int i = 0; i < 5; ++i) { silence.clear(); proc.processBlock(silence, midi); }

        // Process another tone block in HQ mode
        juce::AudioBuffer<float> toneHQ(2, blockSize);
        for (int s = 0; s < blockSize; ++s)
        {
            float v = 0.8f * std::sin(static_cast<float>(
                2.0 * juce::MathConstants<double>::pi * 1000.0 * s / sampleRate));
            toneHQ.setSample(0, s, v);
            toneHQ.setSample(1, s, v);
        }
        proc.processBlock(toneHQ, midi);
        const float rmsAfterSwitch = toneHQ.getRMSLevel(0, 0, blockSize);

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
