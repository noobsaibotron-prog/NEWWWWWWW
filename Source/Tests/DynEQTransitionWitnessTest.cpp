#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"
#include "Support/TestParameters.h"
#include "Support/ToneTransitionAudit.h"

#include <cmath>
#include <vector>

/**
 * DynEQ master must actually compress, and ON↔OFF on a live 997 Hz tone must
 * stay click-clean. Wrong APVTS id `dynamicEQEnabled` is not a parameter.
 */
class DynEQTransitionWitnessTest final : public juce::UnitTest
{
public:
    DynEQTransitionWitnessTest()
        : juce::UnitTest ("DynEQ transition witness", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        testWrongIdDoesNotExist();
        testCompressionWitness();
        testMasterOnOffTransitions();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 256;
    static constexpr double kToneHz = 997.0;
    static constexpr float kAmp = 0.45f;

    void armCompressingPeak (juce::AudioProcessorValueTreeState& apvts)
    {
        aieq::test::setBool (*this, apvts, "bypass", false);
        aieq::test::setBool (*this, apvts, "aiEnabled", false);
        aieq::test::setBool (*this, apvts, "autoGain", false);
        aieq::test::setFloat (*this, apvts, "dryWet", 100.0f);
        aieq::test::setFloat (*this, apvts, "outputGain", 0.0f);
        aieq::test::setChoice (*this, apvts, "phaseMode", 0);
        aieq::test::setChoice (*this, apvts, "qualityMode", 0);
        aieq::test::setChoice (*this, apvts, "numActiveBands", 0);
        aieq::test::setChoice (*this, apvts, "band0Type", 2);
        aieq::test::setFloat (*this, apvts, "band0Freq", static_cast<float> (kToneHz));
        aieq::test::setFloat (*this, apvts, "band0Gain", 0.0f);
        aieq::test::setFloat (*this, apvts, "band0Q", 2.0f);
        aieq::test::setBool (*this, apvts, "band0Enabled", true);
        aieq::test::setChoice (*this, apvts, "band0DynMode",
                               DynamicEQProcessor::DynamicMode_Compress);
        aieq::test::setFloat (*this, apvts, "band0Threshold", -24.0f);
        aieq::test::setFloat (*this, apvts, "band0Ratio", 8.0f);
        aieq::test::setFloat (*this, apvts, "band0Range", 24.0f);
        aieq::test::setFloat (*this, apvts, "band0Attack", 1.0f);
        aieq::test::setFloat (*this, apvts, "band0Release", 40.0f);
        aieq::test::setFloat (*this, apvts, "dynEqMix", 100.0f);
    }

    static void fillTone (juce::AudioBuffer<float>& buffer, int sampleOffset)
    {
        const double w = juce::MathConstants<double>::twoPi * kToneHz / kSampleRate;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = kAmp * static_cast<float> (
                    std::sin (w * static_cast<double> (sampleOffset + i)));
        }
    }

    void testWrongIdDoesNotExist()
    {
        beginTest ("dynamicEQEnabled is not an APVTS id; dynEqEnabled is");

        AIEqualizerAudioProcessor processor;
        processor.prepareToPlay (kSampleRate, kBlockSize);
        auto& apvts = processor.getAPVTS();
        expect (apvts.getParameter ("dynamicEQEnabled") == nullptr,
                "typo id must not exist");
        expect (apvts.getParameter ("dynEqEnabled") != nullptr,
                "dynEqEnabled must exist");
        processor.releaseResources();
    }

    void testCompressionWitness()
    {
        beginTest ("Master ON compresses 997 Hz: |GR|>=1 dB and RMS diverges from OFF");

        AIEqualizerAudioProcessor processor;
        processor.prepareToPlay (kSampleRate, kBlockSize);
        auto& apvts = processor.getAPVTS();
        armCompressingPeak (apvts);

        juce::AudioBuffer<float> block (2, kBlockSize);
        juce::MidiBuffer midi;

        aieq::test::setBool (*this, apvts, "dynEqEnabled", true);
        float rmsOn = 0.0f;
        float gr = 0.0f;
        for (int b = 0; b < 80; ++b)
        {
            fillTone (block, b * kBlockSize);
            processor.processBlock (block, midi);
            rmsOn = block.getRMSLevel (0, 0, kBlockSize);
            gr = processor.getDynamicBandMeter (0).gainReduction;
        }

        expect (std::abs (gr) >= 1.0f,
                "compression witness |GR|=" + juce::String (gr, 3) + " dB");

        aieq::test::setBool (*this, apvts, "dynEqEnabled", false);
        float rmsOff = 0.0f;
        for (int b = 0; b < 80; ++b)
        {
            fillTone (block, (80 + b) * kBlockSize);
            processor.processBlock (block, midi);
            rmsOff = block.getRMSLevel (0, 0, kBlockSize);
        }

        expect (rmsOff > rmsOn * 1.05f,
                "master OFF RMS must rise vs compressing ON (on="
                    + juce::String (rmsOn, 4) + " off=" + juce::String (rmsOff, 4) + ")");
        processor.releaseResources();
    }

    void testMasterOnOffTransitions()
    {
        beginTest ("DynEQ ON→OFF and OFF→ON on a 997 Hz tone stay click-clean");

        for (const bool startOn : { true, false })
        {
            AIEqualizerAudioProcessor processor;
            processor.prepareToPlay (kSampleRate, kBlockSize);
            auto& apvts = processor.getAPVTS();
            armCompressingPeak (apvts);
            aieq::test::setBool (*this, apvts, "dynEqEnabled", startOn);

            constexpr int kBlocks = 60;
            constexpr int kSwitch = 30;
            juce::AudioBuffer<float> captured (2, kBlocks * kBlockSize);
            juce::AudioBuffer<float> block (2, kBlockSize);
            juce::MidiBuffer midi;

            for (int b = 0; b < kBlocks; ++b)
            {
                if (b == kSwitch)
                    aieq::test::setBool (*this, apvts, "dynEqEnabled", ! startOn);
                fillTone (block, b * kBlockSize);
                processor.processBlock (block, midi);
                for (int ch = 0; ch < 2; ++ch)
                    captured.copyFrom (ch, b * kBlockSize, block, ch, 0, kBlockSize);
            }

            const auto audit = aieq::test::auditToneTransition (
                captured, (kSwitch - 2) * kBlockSize, 8 * kBlockSize, kSampleRate, kToneHz);
            const juce::String label = startOn ? "ON→OFF" : "OFF→ON";
            aieq::test::expectCleanToneTransition (*this, audit, label);
            processor.releaseResources();
        }
    }
};

static DynEQTransitionWitnessTest dynEqTransitionWitnessTest;
