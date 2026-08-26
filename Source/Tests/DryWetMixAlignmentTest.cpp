#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"

#include <cmath>
#include <vector>

/**
 * Regression: the global dry/wet mix must run AFTER wet-padding.
 *
 * Natural Phase and padded Zero Latency delay dry by the host-visible latency
 * at capture, then pad wet so the two meet. Mixing before that pad delayed dry
 * a second time: 100% wet hid it; lowering the mix produced a pad-length flam.
 */
class DryWetMixAlignmentTest final : public juce::UnitTest
{
public:
    DryWetMixAlignmentTest()
        : juce::UnitTest ("Dry/wet mix alignment", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        testNaturalPhaseFiftyPercentIsSinglePeak();
        testZeroLatencyAfterNaturalKeepsAlignment();
        testFreshZeroLatencyFiftyPercentStaysAtSampleZero();
        testNaturalDryAndWetPeaksCoincide();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 256;
    static constexpr int kNearRadius = 128;
    static constexpr float kFarPeakLimit = 0.18f;

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

    static void flattenProcessing (AIEqualizerAudioProcessor& processor)
    {
        auto& apvts = processor.getAPVTS();
        setBool (apvts, "bypass", false);
        setBool (apvts, "autoGain", false);
        setBool (apvts, "aiEnabled", false);
        setBool (apvts, "dynEqEnabled", false);
        setChoice (apvts, "qualityMode", 0);
        setChoice (apvts, "msMode", 0);
        setFloat (apvts, "outputGain", 0.0f);
        setChoice (apvts, "numActiveBands", 0);

        if (auto* gain = apvts.getParameter ("band0Gain"))
            gain->setValueNotifyingHost (gain->convertTo0to1 (0.0f));
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

    static int warmupBlocksFor (const AIEqualizerAudioProcessor& processor)
    {
        const int latency = juce::jmax (0, processor.getLatencySamples());
        return (latency / kBlockSize) + 8;
    }

    struct Capture
    {
        std::vector<float> samples;
        int latency = 0;
    };

    static Capture captureImpulse (AIEqualizerAudioProcessor& processor)
    {
        Capture capture;
        capture.latency = processor.getLatencySamples();
        processSilence (processor, warmupBlocksFor (processor));

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processor.processBlock (buffer, midi);

        capture.samples.reserve (static_cast<size_t> ((capture.latency / kBlockSize) + 6) * kBlockSize);
        for (int i = 0; i < kBlockSize; ++i)
            capture.samples.push_back (buffer.getSample (0, i));

        const int flushBlocks = (capture.latency / kBlockSize) + 4;
        for (int b = 0; b < flushBlocks; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                capture.samples.push_back (buffer.getSample (0, i));
        }

        return capture;
    }

    static int peakIndex (const std::vector<float>& samples)
    {
        int best = 0;
        float bestAbs = -1.0f;
        for (int i = 0; i < static_cast<int> (samples.size()); ++i)
        {
            const float a = std::abs (samples[static_cast<size_t> (i)]);
            if (a > bestAbs)
            {
                bestAbs = a;
                best = i;
            }
        }
        return best;
    }

    static float peakAbs (const std::vector<float>& samples, int index)
    {
        if (index < 0 || index >= static_cast<int> (samples.size()))
            return 0.0f;
        return std::abs (samples[static_cast<size_t> (index)]);
    }

    static int farthestPeakIndex (const std::vector<float>& samples, int mainIndex)
    {
        int best = -1;
        float bestAbs = 0.0f;
        for (int i = 0; i < static_cast<int> (samples.size()); ++i)
        {
            if (std::abs (i - mainIndex) <= kNearRadius)
                continue;
            const float a = std::abs (samples[static_cast<size_t> (i)]);
            if (a > bestAbs)
            {
                bestAbs = a;
                best = i;
            }
        }
        return best;
    }

    static void expectSingleAlignedPeak (juce::UnitTest& test,
                                         const Capture& capture,
                                         const juce::String& label)
    {
        test.expect (capture.samples.size() > 32, label + ": captured impulse");
        const int main = peakIndex (capture.samples);
        const float mainAmp = peakAbs (capture.samples, main);
        test.expect (mainAmp > 0.25f, label + ": main peak too small ("
                     + juce::String (mainAmp, 4) + ")");

        if (capture.latency == 0)
        {
            test.expect (main <= 8, label + ": zero-latency peak drifted to sample "
                         + juce::String (main));
        }
        else
        {
            test.expect (std::abs (main - capture.latency) <= kNearRadius,
                         label + ": peak at " + juce::String (main)
                         + " vs latency " + juce::String (capture.latency));
        }

        const int far = farthestPeakIndex (capture.samples, main);
        if (far >= 0)
        {
            const float farAmp = peakAbs (capture.samples, far);
            const float ratio = farAmp / juce::jmax (1.0e-6f, mainAmp);
            test.expect (ratio < kFarPeakLimit,
                         label + ": second peak at sample " + juce::String (far)
                         + " (" + juce::String (ratio, 3)
                         + " of main) — dry/wet delay mismatch");
        }
    }

    static void configureNatural (AIEqualizerAudioProcessor& processor)
    {
        flattenProcessing (processor);
        setChoice (processor.getAPVTS(), "phaseMode", 1); // Natural Phase
        setChoice (processor.getAPVTS(), "oversamplingFactor", 2); // 4x: matches worst-case OS latency
    }

    void testNaturalPhaseFiftyPercentIsSinglePeak()
    {
        beginTest ("Natural Phase 50% wet is one peak at reported latency");

        AIEqualizerAudioProcessor processor;
        setFloat (processor.getAPVTS(), "dryWet", 50.0f);
        configureNatural (processor);
        processor.prepareToPlay (kSampleRate, kBlockSize);

        expect (processor.getLatencySamples() > kNearRadius,
                "Natural Phase must advertise a padded latency plan");

        const auto capture = captureImpulse (processor);
        expectSingleAlignedPeak (*this, capture, "Natural 50%");
        processor.releaseResources();
    }

    void testZeroLatencyAfterNaturalKeepsAlignment()
    {
        beginTest ("Zero Latency after Natural keeps 50% wet aligned (deferred contraction)");

        AIEqualizerAudioProcessor processor;
        configureNatural (processor);
        setFloat (processor.getAPVTS(), "dryWet", 50.0f);
        processor.prepareToPlay (kSampleRate, kBlockSize);

        const int naturalLatency = processor.getLatencySamples();
        expect (naturalLatency > kNearRadius, "Natural latency");

        setChoice (processor.getAPVTS(), "phaseMode", 0); // Zero Latency
        expectEquals (processor.getLatencySamples(), naturalLatency,
                      "Live ZL must not contract PDC");

        processSilence (processor, warmupBlocksFor (processor));
        const auto capture = captureImpulse (processor);
        expectSingleAlignedPeak (*this, capture, "ZL-after-Natural 50%");
        processor.releaseResources();
    }

    void testFreshZeroLatencyFiftyPercentStaysAtSampleZero()
    {
        beginTest ("Fresh Zero Latency 50% wet stays at sample zero");

        AIEqualizerAudioProcessor processor;
        flattenProcessing (processor);
        setChoice (processor.getAPVTS(), "phaseMode", 0);
        setFloat (processor.getAPVTS(), "dryWet", 50.0f);
        processor.prepareToPlay (kSampleRate, kBlockSize);

        expectEquals (processor.getLatencySamples(), 0);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::MidiBuffer midi;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        processor.processBlock (buffer, midi);

        std::vector<float> out (static_cast<size_t> (kBlockSize));
        for (int i = 0; i < kBlockSize; ++i)
            out[static_cast<size_t> (i)] = buffer.getSample (0, i);

        expect (peakAbs (out, 0) > 0.5f, "Fresh ZL 50% wet impulse left sample zero");
        const int far = farthestPeakIndex (out, 0);
        if (far >= 0)
        {
            expect (peakAbs (out, far) < kFarPeakLimit * peakAbs (out, 0),
                    "Fresh ZL 50% wet must not split into a delayed second peak");
        }
        processor.releaseResources();
    }

    void testNaturalDryAndWetPeaksCoincide()
    {
        beginTest ("Natural Phase 0% and 100% wet peak at the same sample");

        auto run = [this] (float dryWet) -> Capture
        {
            AIEqualizerAudioProcessor processor;
            configureNatural (processor);
            setFloat (processor.getAPVTS(), "dryWet", dryWet);
            processor.prepareToPlay (kSampleRate, kBlockSize);
            auto capture = captureImpulse (processor);
            processor.releaseResources();
            return capture;
        };

        const auto dryOnly = run (0.0f);
        const auto wetOnly = run (100.0f);
        const auto mixed = run (50.0f);

        const int dryPeak = peakIndex (dryOnly.samples);
        const int wetPeak = peakIndex (wetOnly.samples);
        const int mixPeak = peakIndex (mixed.samples);

        expect (std::abs (dryPeak - wetPeak) <= 8,
                "Dry and wet peaks split by " + juce::String (std::abs (dryPeak - wetPeak))
                + " samples (mix-before-pad regression)");
        expect (std::abs (mixPeak - wetPeak) <= 8,
                "50% mix peak drifted from wet-only by "
                + juce::String (std::abs (mixPeak - wetPeak)) + " samples");

        const float wetAmp = peakAbs (wetOnly.samples, wetPeak);
        const float mixAmp = peakAbs (mixed.samples, mixPeak);
        expect (mixAmp > 0.70f * wetAmp,
                "50% mix peak collapsed (" + juce::String (mixAmp, 4)
                + " vs wet " + juce::String (wetAmp, 4)
                + ") — incoherent dry/wet sum");
    }
};

static DryWetMixAlignmentTest dryWetMixAlignmentTest;
