#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>

/**
 * Click / dropout audit matching DynamicEQSpectralArtifactTest:
 * second-difference ratio vs a pure-tone bound, quiet-region bursts, NaN/Inf.
 */
namespace aieq::test
{
struct ToneTransitionAudit
{
    bool hasNaN = false;
    bool hasInf = false;
    float peakAbs = 0.0f;
    float maxDelta = 0.0f;
    float secondDiffRatio = 0.0f;
    int quietBurstSamples = 0;
    int dropoutSamples = 0;
};

inline ToneTransitionAudit auditToneTransition (const juce::AudioBuffer<float>& buffer,
                                                int startSample,
                                                int numSamples,
                                                double sampleRate,
                                                double freqHz)
{
    ToneTransitionAudit audit;
    if (buffer.getNumChannels() == 0 || numSamples <= 4)
        return audit;

    startSample = juce::jmax (0, startSample);
    const int end = juce::jmin (buffer.getNumSamples(), startSample + numSamples);
    const float* ch0 = buffer.getReadPointer (0);
    const double w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;

    int dropoutRun = 0;
    int maxDropout = 0;
    float maxSecondDiff = 0.0f;

    for (int i = startSample; i < end; ++i)
    {
        const float v = ch0[i];
        if (std::isnan (v)) audit.hasNaN = true;
        if (std::isinf (v)) audit.hasInf = true;
        audit.peakAbs = juce::jmax (audit.peakAbs, std::abs (v));

        if (i > startSample)
        {
            const float delta = std::abs (v - ch0[i - 1]);
            audit.maxDelta = juce::jmax (audit.maxDelta, delta);
        }

        if (std::abs (v) < 1.0e-5f)
            ++dropoutRun;
        else
            dropoutRun = 0;
        maxDropout = juce::jmax (maxDropout, dropoutRun);
    }

    audit.dropoutSamples = maxDropout;

    for (int i = startSample + 1; i < end - 1; ++i)
    {
        const float secondDiff = std::abs (ch0[i + 1] - 2.0f * ch0[i] + ch0[i - 1]);
        maxSecondDiff = juce::jmax (maxSecondDiff, secondDiff);
    }

    const float legit = audit.peakAbs * static_cast<float> (w * w);
    audit.secondDiffRatio = maxSecondDiff / juce::jmax (legit, 1.0e-9f);

    const float stepThresh = 0.25f * juce::jmax (audit.peakAbs, 1.0e-6f);
    for (int i = startSample + 1; i < end; ++i)
    {
        if (std::abs (ch0[i] - ch0[i - 1]) > stepThresh
            && std::abs (ch0[i]) < 0.5f * audit.peakAbs)
            ++audit.quietBurstSamples;
    }

    return audit;
}

inline void expectCleanToneTransition (juce::UnitTest& test,
                                       const ToneTransitionAudit& audit,
                                       const juce::String& label)
{
    test.expect (! audit.hasNaN, label + ": NaN");
    test.expect (! audit.hasInf, label + ": Inf");
    test.expect (audit.secondDiffRatio < 8.0f,
                 label + ": secondDiffRatio=" + juce::String (audit.secondDiffRatio, 2));
    test.expect (audit.quietBurstSamples == 0,
                 label + ": quiet-region bursts=" + juce::String (audit.quietBurstSamples));
    test.expect (audit.dropoutSamples <= 8,
                 label + ": dropout=" + juce::String (audit.dropoutSamples));
}
} // namespace aieq::test
