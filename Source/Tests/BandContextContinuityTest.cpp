#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../GUI/BandContextActions.h"
#include "../PluginProcessor.h"
#include "Support/TestParameters.h"
#include "Support/ToneTransitionAudit.h"

#include <array>
#include <cmath>

/**
 * Radial/classic context commands must mutate BandState through the shared
 * helper, and type changes on a live tone must stay click-clean.
 *
 * Dropout is asserted only for types that still pass 997 Hz. LowCut / HighCut /
 * Notch at the probe frequency are supposed to collapse RMS; that is the
 * filter witness, not a click. Click gates (secondDiffRatio, quiet bursts)
 * still apply to every splice, including Peak→Notch.
 */
class BandContextContinuityTest final : public juce::UnitTest
{
public:
    BandContextContinuityTest()
        : juce::UnitTest ("Band context continuity", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        testHelperMutatesBandState();
        testTypeChangesOnLiveTone();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 256;
    static constexpr double kToneHz = 997.0;

    void primePeakBand (AIEqualizerAudioProcessor& processor)
    {
        processor.setNumActiveBands (1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = static_cast<float> (kToneHz);
        band.gain = 6.0f;
        band.q = 1.2f;
        band.type = 2;
        band.enabled = true;
        band.solo = false;
        processor.setBandState (0, band);

        auto& apvts = processor.getAPVTS();
        aieq::test::setBool (*this, apvts, "bypass", false);
        aieq::test::setBool (*this, apvts, "aiEnabled", false);
        aieq::test::setBool (*this, apvts, "dynEqEnabled", false);
        aieq::test::setFloat (*this, apvts, "dryWet", 100.0f);
        aieq::test::setFloat (*this, apvts, "outputGain", 0.0f);
        aieq::test::setChoice (*this, apvts, "phaseMode", 0);
        aieq::test::setChoice (*this, apvts, "numActiveBands", 0);
        aieq::test::setChoice (*this, apvts, "oversamplingFactor", 0);
    }

    static bool typePassesProbe (int type)
    {
        switch (type)
        {
            case 1: // Low Shelf
            case 2: // Peak
            case 3: // High Shelf
            case 6: // Band Pass at the probe frequency
                return true;
            default:
                return false;
        }
    }

    void testHelperMutatesBandState()
    {
        beginTest ("Shared helper emits type/gain/enable changes on BandState");

        AIEqualizerAudioProcessor processor;
        processor.prepareToPlay (kSampleRate, kBlockSize);
        primePeakBand (processor);

        using CT = BandRadialMenu::CommandType;
        expect (aieq::gui::applyBandContextCommand (
                    processor, 0, { CT::setFilterType, 3 }),
                "High Shelf command must apply");
        expectEquals (processor.getBandState (0).type, 3, "type became High Shelf");

        expect (aieq::gui::applyBandContextCommand (
                    processor, 0, { CT::resetGain, 0 }));
        expectWithinAbsoluteError (processor.getBandState (0).gain, 0.0f, 1.0e-4f);

        const bool wasEnabled = processor.getBandState (0).enabled;
        expect (aieq::gui::applyBandContextCommand (
                    processor, 0, { CT::toggleEnabled, 0 }));
        expect (processor.getBandState (0).enabled != wasEnabled);

        processor.releaseResources();
    }

    void testTypeChangesOnLiveTone()
    {
        beginTest ("Peak/shelf/cut/notch/band-pass type changes on 997 Hz stay click-clean");

        const std::array<int, 6> types { 0, 1, 3, 4, 5, 6 };
        const char* names[] = {
            "LowCut", "LowShelf", "Peak", "HighShelf", "HighCut", "Notch", "BandPass"
        };

        constexpr int kPre = 8;
        constexpr int kPost = 8;
        constexpr int kSwitch = kPre;
        constexpr int kBlocks = kPre + kPost;
        const double w = juce::MathConstants<double>::twoPi * kToneHz / kSampleRate;

        for (int type : types)
        {
            AIEqualizerAudioProcessor processor;
            processor.prepareToPlay (kSampleRate, kBlockSize);
            primePeakBand (processor);

            juce::AudioBuffer<float> captured (2, kBlocks * kBlockSize);
            juce::AudioBuffer<float> block (2, kBlockSize);
            juce::MidiBuffer midi;
            const juce::String label = juce::String ("Peak→") + names[type];

            for (int b = 0; b < kBlocks; ++b)
            {
                if (b == kSwitch)
                {
                    expect (aieq::gui::applyBandContextCommand (
                                processor, 0,
                                { BandRadialMenu::CommandType::setFilterType, type }),
                            label + " command must change type");
                    expectEquals (processor.getBandState (0).type, type, label);
                }

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = block.getWritePointer (ch);
                    for (int i = 0; i < kBlockSize; ++i)
                        data[i] = 0.25f * static_cast<float> (
                            std::sin (w * static_cast<double> (b * kBlockSize + i)));
                }
                processor.processBlock (block, midi);
                for (int ch = 0; ch < 2; ++ch)
                    captured.copyFrom (ch, b * kBlockSize, block, ch, 0, kBlockSize);
            }

            const auto audit = aieq::test::auditToneTransition (
                captured, (kSwitch - 2) * kBlockSize, 6 * kBlockSize,
                kSampleRate, kToneHz);
            logMessage (label + " secondDiffRatio=" + juce::String (audit.secondDiffRatio, 2)
                        + " quietBursts=" + juce::String (audit.quietBurstSamples)
                        + " dropout=" + juce::String (audit.dropoutSamples)
                        + " peakAbs=" + juce::String (audit.peakAbs, 4));

            expect (! audit.hasNaN, label + ": NaN");
            expect (! audit.hasInf, label + ": Inf");
            expect (audit.secondDiffRatio < 8.0f,
                    label + ": secondDiffRatio=" + juce::String (audit.secondDiffRatio, 2));
            expect (audit.quietBurstSamples == 0,
                    label + ": quiet-region bursts=" + juce::String (audit.quietBurstSamples));

            float rmsPre = 0.0f;
            float rmsPost = 0.0f;
            const int preN = 2 * kBlockSize;
            const int postN = 2 * kBlockSize;
            for (int i = 0; i < preN; ++i)
                rmsPre += captured.getSample (0, (kSwitch - 2) * kBlockSize + i)
                        * captured.getSample (0, (kSwitch - 2) * kBlockSize + i);
            for (int i = 0; i < postN; ++i)
                rmsPost += captured.getSample (0, (kSwitch + 4) * kBlockSize + i)
                         * captured.getSample (0, (kSwitch + 4) * kBlockSize + i);
            rmsPre = std::sqrt (rmsPre / static_cast<float> (preN));
            rmsPost = std::sqrt (rmsPost / static_cast<float> (postN));
            logMessage (label + " rmsPre=" + juce::String (rmsPre, 4)
                        + " rmsPost=" + juce::String (rmsPost, 4));

            if (typePassesProbe (type))
            {
                expect (audit.dropoutSamples <= 8,
                        label + ": dropout=" + juce::String (audit.dropoutSamples));
            }

            expect (std::abs (rmsPost - rmsPre) > 0.01f * juce::jmax (rmsPre, 1.0e-6f),
                    label + ": type change must move 997 Hz RMS");

            processor.releaseResources();
        }
    }
};

static BandContextContinuityTest bandContextContinuityTest;
