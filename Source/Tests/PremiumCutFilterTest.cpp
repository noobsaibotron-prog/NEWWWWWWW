#include <juce_core/juce_core.h>
#include "../DSP/CutFilterDesigner.h"
#include "../DSP/ParametricEQProcessor.h"
#include <cmath>

class PremiumCutFilterTest final : public juce::UnitTest
{
public:
    PremiumCutFilterTest() : juce::UnitTest("Premium Cut Filter", "AIEQ-DSP") {}

    void runTest() override
    {
        testButterworthCutoffAndStages();
        testSteeperSlopesHaveStrongerStopband();
        testAudioAndMagnitudePathsAgree();
    }

private:
    static double magnitude(const CutFilterDesigner::Design& design,
                            double frequency,
                            double sampleRate)
    {
        double result = 1.0;
        for (int stage = 0; stage < design.numStages; ++stage)
            result *= design.coefficients[static_cast<size_t>(stage)]
                .getMagnitudeForFrequency(frequency, sampleRate);
        return result;
    }

    void testButterworthCutoffAndStages()
    {
        beginTest("12/24/48 dB cuts use canonical Butterworth sections");
        constexpr double sampleRate = 48000.0;
        constexpr float cutoff = 1000.0f;

        for (const bool highPass : { false, true })
        {
            for (int slope = 0; slope < 3; ++slope)
            {
                const auto design = CutFilterDesigner::design(
                    highPass, slope, sampleRate, cutoff, 1.0f);
                expectEquals(design.numStages, slope == 0 ? 1 : (slope == 1 ? 2 : 4));

                const double cutoffDb = juce::Decibels::gainToDecibels(
                    magnitude(design, cutoff, sampleRate), -200.0);
                expectWithinAbsoluteError(cutoffDb, -3.0102999566, 0.02,
                    "Butterworth cascade is not -3.01 dB at cutoff");

                if (design.numStages > 1)
                {
                    expect(std::abs(design.coefficients[0].a2
                                    - design.coefficients[1].a2) > 1.0e-5f,
                           "Higher-order cut reused identical biquad sections");
                }
            }
        }
    }

    void testSteeperSlopesHaveStrongerStopband()
    {
        beginTest("Stopband attenuation increases monotonically with slope");
        constexpr double sampleRate = 48000.0;
        constexpr float cutoff = 1000.0f;

        double previousHighPass = 1.0;
        double previousLowPass = 1.0;
        for (int slope = 0; slope < 3; ++slope)
        {
            const auto highPass = CutFilterDesigner::design(
                true, slope, sampleRate, cutoff, 1.0f);
            const auto lowPass = CutFilterDesigner::design(
                false, slope, sampleRate, cutoff, 1.0f);
            const double highPassStop = magnitude(highPass, 125.0, sampleRate);
            const double lowPassStop = magnitude(lowPass, 8000.0, sampleRate);

            expect(highPassStop < previousHighPass);
            expect(lowPassStop < previousLowPass);
            previousHighPass = highPassStop;
            previousLowPass = lowPassStop;
        }
    }

    void testAudioAndMagnitudePathsAgree()
    {
        beginTest("48 dB audio path matches the published magnitude response");
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;
        constexpr float testFrequency = 500.0f;

        ParametricEQProcessor processor;
        processor.prepare(sampleRate, blockSize, 1);
        const int band = processor.addBand(
            1000.0f, 0.0f, 1.0f, ParametricEQProcessor::LowCut);
        processor.setBandSlope(band, 2);

        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        double phase = 0.0;
        const double phaseStep = juce::MathConstants<double>::twoPi
                                 * static_cast<double>(testFrequency) / sampleRate;

        for (int block = 0; block < 160; ++block)
        {
            juce::AudioBuffer<float> audio(1, blockSize);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float value = static_cast<float>(0.2 * std::sin(phase));
                phase += phaseStep;
                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
                audio.setSample(0, sample, value);
                if (block >= 80)
                    inputEnergy += static_cast<double>(value) * value;
            }

            processor.process(audio);
            if (block >= 80)
            {
                const auto* samples = audio.getReadPointer(0);
                for (int sample = 0; sample < blockSize; ++sample)
                    outputEnergy += static_cast<double>(samples[sample]) * samples[sample];
            }
        }

        const double measured = std::sqrt(outputEnergy / inputEnergy);
        const double published = processor.getMagnitudeForFrequency(
            testFrequency, sampleRate);
        const double errorDb = juce::Decibels::gainToDecibels(
            measured / published, -200.0);
        expectWithinAbsoluteError(errorDb, 0.0, 0.15,
            "Audio and GUI cut responses diverged");
    }
};

static PremiumCutFilterTest premiumCutFilterTest;
