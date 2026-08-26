#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"
#include "Support/TestParameters.h"
#include "Support/ToneTransitionAudit.h"

#include <array>
#include <cmath>

/**
 * Six phase-mode directions at 48/96 kHz and blocks 64/256.
 * Existing Phase Mode Switch Continuity covers 0↔1 and 0↔2 at 48k/128 only,
 * with looser energyRatio/maxDelta gates. This suite uses the DynEQ
 * second-difference click metric and holds oversampling constant so OS
 * transitions are not mixed into the phase matrix.
 */
class PhaseTransitionMatrixTest final : public juce::UnitTest
{
public:
    PhaseTransitionMatrixTest()
        : juce::UnitTest ("Phase transition matrix", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        const std::array<std::pair<int, int>, 6> directions {{
            { 0, 1 }, { 1, 0 },
            { 0, 2 }, { 2, 0 },
            { 1, 2 }, { 2, 1 }
        }};
        const double rates[] = { 48000.0, 96000.0 };
        const int blocks[] = { 64, 256 };

        for (double sr : rates)
            for (int blockSize : blocks)
                for (auto [from, to] : directions)
                    runOne (sr, blockSize, from, to);
    }

private:
    static constexpr double kToneHz = 997.0;
    static constexpr float kAmp = 0.25f;

    static const char* nameFor (int mode)
    {
        switch (mode)
        {
            case 0: return "ZL";
            case 1: return "Natural";
            default: return "Linear";
        }
    }

    void configure (AIEqualizerAudioProcessor& processor,
                    juce::AudioProcessorValueTreeState& apvts,
                    int phase)
    {
        processor.setNumActiveBands (1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = static_cast<float> (kToneHz);
        band.gain = 6.0f;
        band.q = 1.0f;
        band.type = 2;
        band.enabled = true;
        band.solo = false;
        processor.setBandState (0, band);

        aieq::test::setBool (*this, apvts, "bypass", false);
        aieq::test::setBool (*this, apvts, "aiEnabled", false);
        aieq::test::setBool (*this, apvts, "autoGain", false);
        aieq::test::setBool (*this, apvts, "dynEqEnabled", false);
        aieq::test::setFloat (*this, apvts, "dryWet", 100.0f);
        aieq::test::setFloat (*this, apvts, "outputGain", 0.0f);
        aieq::test::setChoice (*this, apvts, "qualityMode", 0);
        aieq::test::setChoice (*this, apvts, "msMode", 0);
        aieq::test::setChoice (*this, apvts, "numActiveBands", 0);
        aieq::test::setChoice (*this, apvts, "oversamplingFactor", 0);
        aieq::test::setChoice (*this, apvts, "phaseMode", phase);
        if (phase == 2)
            processor.forceLinearIRReady();
    }

    void runOne (double sampleRate, int blockSize, int from, int to)
    {
        const juce::String label = juce::String (nameFor (from)) + "→" + nameFor (to)
            + " @" + juce::String (sampleRate, 0) + "/" + juce::String (blockSize);
        beginTest (label);

        AIEqualizerAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);
        auto& apvts = processor.getAPVTS();
        configure (processor, apvts, from);

        // The latency-aligned state machine uses two maximum-latency windows
        // to establish the incoming history, then a 1024-sample audible blend.
        // Keep both the initial configured mode and the measured switch well
        // beyond that complete transition; otherwise a Natural/Linear "from"
        // case would switch again while its setup transition was still active.
        constexpr int completeTransitionWindow = 12288;
        const int padWarmup = (completeTransitionWindow + blockSize - 1) / blockSize;
        const int postSwitch = (completeTransitionWindow + blockSize - 1) / blockSize;
        const int kSwitch = padWarmup;
        const int kBlocks = kSwitch + postSwitch;
        juce::AudioBuffer<float> captured (2, kBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;
        const double w = juce::MathConstants<double>::twoPi * kToneHz / sampleRate;

        for (int b = 0; b < kBlocks; ++b)
        {
            if (b == kSwitch)
            {
                aieq::test::setChoice (*this, apvts, "phaseMode", to);
                if (to == 2)
                    processor.forceLinearIRReady();
            }

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = block.getWritePointer (ch);
                for (int i = 0; i < blockSize; ++i)
                    data[i] = kAmp * static_cast<float> (
                        std::sin (w * static_cast<double> (b * blockSize + i)));
            }
            processor.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const auto audit = aieq::test::auditToneTransition (
            captured, juce::jmax (0, (kSwitch - 2) * blockSize),
            juce::jmin (captured.getNumSamples() - juce::jmax (0, (kSwitch - 2) * blockSize),
                        postSwitch * blockSize),
            sampleRate, kToneHz);
        logMessage (label + " secondDiffRatio=" + juce::String (audit.secondDiffRatio, 2)
                    + " quietBursts=" + juce::String (audit.quietBurstSamples)
                    + " dropout=" + juce::String (audit.dropoutSamples)
                    + " maxDelta=" + juce::String (audit.maxDelta, 4)
                    + " peakAbs=" + juce::String (audit.peakAbs, 4));
        aieq::test::expectCleanToneTransition (*this, audit, label);

        const auto after = processor.getBandState (0);
        logMessage (label + " band type=" + juce::String (after.type)
                    + " gain=" + juce::String (after.gain, 4)
                    + " freq=" + juce::String (after.frequency, 1)
                    + " enabled=" + juce::String (static_cast<int> (after.enabled)));
        expectWithinAbsoluteError (after.gain, 6.0f, 0.15f,
                                   label + ": gain must survive");
        expectEquals (after.type, 2, label + ": type must survive");
        expect (after.enabled, label + ": enable must survive");
        processor.releaseResources();
    }
};

static PhaseTransitionMatrixTest phaseTransitionMatrixTest;
