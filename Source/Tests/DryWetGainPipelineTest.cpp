#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"

#include <cmath>
#include <vector>

/**
 * Regression: Output Gain is a post-mix trim; Auto Gain is wet-only makeup.
 *
 * After moving dry/wet mix past wet-padding, the previous single applyGain sat
 * on the wet path and left 0% wet insensitive to Output Gain. These tests pin
 * the split: auto-gain → pad → mix → output trim → bypass to ungained dry.
 */
class DryWetGainPipelineTest final : public juce::UnitTest
{
public:
    DryWetGainPipelineTest()
        : juce::UnitTest ("Dry/wet gain pipeline", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        testOutputTrimOnZeroFiftyHundredMix();
        testOutputTrimAcrossPhaseModes();
        testAutoGainDoesNotLeakOntoDry();
        testAutoGainReducesBoostedWet();
        testBypassDropsOutputTrim();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 256;
    static constexpr float kRatioTol = 0.12f;

    static void setChoice (juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& id,
                           int index)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
    }

    static void setFloat (juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id,
                          float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    static void setBool (juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id,
                         bool value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (value ? 1.0f : 0.0f);
    }

    enum class Phase : int
    {
        zeroLatency = 0,
        natural = 1,
        linear = 2
    };

    static void configure (AIEqualizerAudioProcessor& processor,
                           Phase phase,
                           float dryWet,
                           float outputGainDB,
                           bool autoGain,
                           float band0GainDB)
    {
        auto& apvts = processor.getAPVTS();
        setBool (apvts, "bypass", false);
        setBool (apvts, "aiEnabled", false);
        setBool (apvts, "dynEqEnabled", false);
        setChoice (apvts, "qualityMode", 0);
        setChoice (apvts, "msMode", 0);
        setChoice (apvts, "numActiveBands", 0);
        setChoice (apvts, "phaseMode", static_cast<int> (phase));
        if (phase == Phase::natural)
            setChoice (apvts, "oversamplingFactor", 2);
        setFloat (apvts, "dryWet", dryWet);
        setFloat (apvts, "outputGain", outputGainDB);
        setBool (apvts, "autoGain", autoGain);
        setChoice (apvts, "band0Type", 2); // Peak
        setFloat (apvts, "band0Freq", 1000.0f);
        setFloat (apvts, "band0Q", 1.0f);
        setFloat (apvts, "band0Gain", band0GainDB);
    }

    static void processSilence (AIEqualizerAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    }

    static int warmupBlocks (const AIEqualizerAudioProcessor& processor, int extra)
    {
        const int latency = juce::jmax (0, processor.getLatencySamples());
        return (latency / kBlockSize) + extra;
    }

    static float peakAbs (const std::vector<float>& samples)
    {
        float best = 0.0f;
        for (float s : samples)
            best = juce::jmax (best, std::abs (s));
        return best;
    }

    static std::vector<float> captureImpulse (AIEqualizerAudioProcessor& processor,
                                              int extraWarmup = 24)
    {
        processSilence (processor, warmupBlocks (processor, extraWarmup));

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processor.processBlock (buffer, midi);

        const int latency = processor.getLatencySamples();
        std::vector<float> samples;
        samples.reserve (static_cast<size_t> ((latency / kBlockSize) + 6) * kBlockSize);
        for (int i = 0; i < kBlockSize; ++i)
            samples.push_back (buffer.getSample (0, i));

        const int flushBlocks = (latency / kBlockSize) + 4;
        for (int b = 0; b < flushBlocks; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                samples.push_back (buffer.getSample (0, i));
        }
        return samples;
    }

    static float impulsePeak (Phase phase,
                              float dryWet,
                              float outputGainDB,
                              bool autoGain = false,
                              float band0GainDB = 0.0f,
                              int extraWarmup = 24)
    {
        AIEqualizerAudioProcessor processor;
        configure (processor, phase, dryWet, outputGainDB, autoGain, band0GainDB);
        processor.prepareToPlay (kSampleRate, kBlockSize);
        if (phase == Phase::linear)
            processor.forceLinearIRReady();
        const float peak = peakAbs (captureImpulse (processor, extraWarmup));
        processor.releaseResources();
        return peak;
    }

    static float renderSineRms (AIEqualizerAudioProcessor& processor, int blocks)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 1000.0 / kSampleRate;
        float lastRms = 0.0f;

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float s = static_cast<float> (std::sin (phase));
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
                phase += inc;
                if (phase > 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
            }
            processor.processBlock (buffer, midi);
            lastRms = buffer.getRMSLevel (0, 0, kBlockSize);
        }
        return lastRms;
    }

    static float sineRms (Phase phase,
                          float dryWet,
                          float outputGainDB,
                          bool autoGain,
                          float band0GainDB,
                          int blocks)
    {
        AIEqualizerAudioProcessor processor;
        configure (processor, phase, dryWet, outputGainDB, autoGain, band0GainDB);
        processor.prepareToPlay (kSampleRate, kBlockSize);
        if (phase == Phase::linear)
            processor.forceLinearIRReady();
        const float rms = renderSineRms (processor, blocks);
        processor.releaseResources();
        return rms;
    }

    static void expectRatio (juce::UnitTest& test,
                             float actual,
                             float expected,
                             const juce::String& label)
    {
        test.expect (expected > 1.0e-6f, label + ": expected ratio");
        const float err = std::abs (actual - expected) / expected;
        test.expect (err <= kRatioTol,
                     label + ": ratio " + juce::String (actual, 3)
                     + " vs " + juce::String (expected, 3));
    }

    void testOutputTrimOnZeroFiftyHundredMix()
    {
        beginTest ("Output Gain scales 0/50/100% wet equally at -6/0/+6 dB");

        const float mixPct[] = { 0.0f, 50.0f, 100.0f };
        for (float mix : mixPct)
        {
            const float peak0 = impulsePeak (Phase::zeroLatency, mix, 0.0f);
            expect (peak0 > 0.25f, "ZL mix " + juce::String (mix) + " at 0 dB has a peak");

            const float peakP6 = impulsePeak (Phase::zeroLatency, mix, 6.0f);
            const float peakM6 = impulsePeak (Phase::zeroLatency, mix, -6.0f);
            expectRatio (*this, peakP6 / peak0, 2.0f,
                         "ZL mix " + juce::String (mix) + " +6 dB");
            expectRatio (*this, peakM6 / peak0, 0.5f,
                         "ZL mix " + juce::String (mix) + " -6 dB");
        }
    }

    void testOutputTrimAcrossPhaseModes()
    {
        beginTest ("Output Gain +6 dB scales 0% and 100% wet in Natural and Linear");

        for (auto phase : { Phase::natural, Phase::linear })
        {
            const char* name = (phase == Phase::natural) ? "Natural" : "Linear";
            for (float mix : { 0.0f, 100.0f })
            {
                const float peak0 = impulsePeak (phase, mix, 0.0f);
                const float peakP6 = impulsePeak (phase, mix, 6.0f);
                expect (peak0 > 0.15f, juce::String (name) + " mix " + juce::String (mix));
                expectRatio (*this, peakP6 / peak0, 2.0f,
                             juce::String (name) + " mix " + juce::String (mix) + " +6 dB");
            }
        }
    }

    void testAutoGainDoesNotLeakOntoDry()
    {
        beginTest ("Auto Gain on a boosted wet path does not change 0% wet");

        const float dryFlat = impulsePeak (Phase::zeroLatency, 0.0f, 0.0f, false, 0.0f);
        const float dryBoostedAuto = impulsePeak (Phase::zeroLatency, 0.0f, 0.0f, true, 12.0f);
        expectRatio (*this, dryBoostedAuto / dryFlat, 1.0f,
                     "ZL 0% wet auto-gain must not leak onto dry");

        const float natDry = impulsePeak (Phase::natural, 0.0f, 0.0f, false, 0.0f);
        const float natDryAuto = impulsePeak (Phase::natural, 0.0f, 0.0f, true, 12.0f);
        expectRatio (*this, natDryAuto / natDry, 1.0f,
                     "Natural 0% wet auto-gain must not leak onto dry");
    }

    void testAutoGainReducesBoostedWet()
    {
        beginTest ("Auto Gain makeup reduces boosted 100% wet toward dry level");

        constexpr int kBlocks = 250;
        const float dry = sineRms (Phase::zeroLatency, 0.0f, 0.0f, false, 0.0f, kBlocks);
        const float wetBoostOff = sineRms (Phase::zeroLatency, 100.0f, 0.0f, false, 12.0f, kBlocks);
        const float wetBoostOn = sineRms (Phase::zeroLatency, 100.0f, 0.0f, true, 12.0f, kBlocks);

        expect (wetBoostOff > dry * 1.5f,
                "Boosted 1 kHz wet without auto-gain should exceed dry");
        expect (wetBoostOn < wetBoostOff * 0.85f,
                "Auto Gain should pull boosted wet RMS down");
        expect (std::abs (wetBoostOn - dry) < std::abs (wetBoostOff - dry),
                "Auto Gain should move boosted wet toward dry");
    }

    void testBypassDropsOutputTrim()
    {
        beginTest ("Bypass active keeps trim; bypassed and completed fade drop it");

        const float activeGained = impulsePeak (Phase::zeroLatency, 100.0f, 6.0f);
        expect (activeGained > 1.5f, "Active +6 dB wet should be gained");

        {
            AIEqualizerAudioProcessor processor;
            configure (processor, Phase::zeroLatency, 100.0f, 6.0f, false, 0.0f);
            processor.prepareToPlay (kSampleRate, kBlockSize);
            processSilence (processor, 24);

            setBool (processor.getAPVTS(), "bypass", true);
            // Crossfade length is 2×worstCaseLatency + 512, not getLatencySamples().
            processSilence (processor, 48);

            const float bypassed = peakAbs (captureImpulse (processor, 4));
            expect (bypassed > 0.5f && bypassed < 1.25f,
                    "Steady bypass must be ungained dry, peak=" + juce::String (bypassed, 3));
            processor.releaseResources();
        }

        {
            AIEqualizerAudioProcessor processor;
            configure (processor, Phase::zeroLatency, 100.0f, 6.0f, false, 0.0f);
            processor.prepareToPlay (kSampleRate, kBlockSize);
            processSilence (processor, 24);
            setBool (processor.getAPVTS(), "bypass", true);

            juce::AudioBuffer<float> buffer (2, kBlockSize);
            juce::MidiBuffer midi;
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            processor.processBlock (buffer, midi);
            const float firstFadePeak = peakAbs (std::vector<float> (
                buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlockSize));

            expect (firstFadePeak > 0.4f,
                    "First bypass-fade block still has signal");
            expect (firstFadePeak < activeGained * 1.05f,
                    "Bypass fade must not exceed the gained wet peak");
            processor.releaseResources();
        }
    }
};

static DryWetGainPipelineTest dryWetGainPipelineTest;
