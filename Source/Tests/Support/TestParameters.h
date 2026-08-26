#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

/**
 * APVTS helpers that fail the calling unit test when an ID is missing.
 * Silent `if (auto* p = getParameter(id))` is how DynEQ master enable was
 * never actually engaged (wrong id `dynamicEQEnabled`).
 */
namespace aieq::test
{
inline juce::RangedAudioParameter* requireParameter (
    juce::UnitTest& test,
    juce::AudioProcessorValueTreeState& apvts,
    const juce::String& id)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
    test.expect (parameter != nullptr, "missing APVTS parameter: " + id);
    return parameter;
}

inline void setChoice (juce::UnitTest& test,
                       juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& id,
                       int index)
{
    if (auto* parameter = requireParameter (test, apvts, id))
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (static_cast<float> (index)));
}

inline void setBool (juce::UnitTest& test,
                     juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& id,
                     bool value)
{
    if (auto* parameter = requireParameter (test, apvts, id))
        parameter->setValueNotifyingHost (value ? 1.0f : 0.0f);
}

inline void setFloat (juce::UnitTest& test,
                      juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& id,
                      float value)
{
    if (auto* parameter = requireParameter (test, apvts, id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace aieq::test
