#pragma once

#include "../DSP/DefaultBandFrequencies.h"

#include <juce_core/juce_core.h>

#include <cstdint>

namespace AIEQStateSchema
{

inline constexpr int currentVersion = 1;
inline constexpr int legacyCurveMode = 0;
inline constexpr int surgicalCurveMode = 1;
inline constexpr int bandCount = static_cast<int>(AIEQDSP::defaultBandFrequencies.size());

enum class LoadKind
{
    Reject,
    LegacyWithoutSchema,
    Current
};

[[nodiscard]] inline LoadKind classify(const juce::ValueTree& state)
{
    if (! state.isValid() || ! state.hasProperty("stateSchemaVersion"))
        return state.isValid() ? LoadKind::LegacyWithoutSchema : LoadKind::Reject;

    const auto version = state.getProperty("stateSchemaVersion");
    int64_t exactVersion = 0;

    if (version.isInt())
        exactVersion = static_cast<int>(version);
    else if (version.isInt64())
        exactVersion = static_cast<int64_t>(version);
    else if (version.isString()
             && version.toString() == juce::String(currentVersion))
        exactVersion = currentVersion;
    else
        return LoadKind::Reject;

    return exactVersion == currentVersion ? LoadKind::Current : LoadKind::Reject;
}

inline void stampCurrent(juce::ValueTree& state)
{
    state.setProperty("stateSchemaVersion", currentVersion, nullptr);
}

inline void setOrAppendParameterValue(juce::ValueTree& state,
                                      const juce::String& parameterID,
                                      float normalizedValue)
{
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild(i);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == parameterID)
        {
            child.setProperty("value", normalizedValue, nullptr);
            return;
        }
    }

    juce::ValueTree child("PARAM");
    child.setProperty("id", parameterID, nullptr);
    child.setProperty("value", normalizedValue, nullptr);
    state.addChild(std::move(child), -1, nullptr);
}

inline void appendParameterValueIfMissing(juce::ValueTree& state,
                                          const juce::String& parameterID,
                                          float parameterValue)
{
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto child = state.getChild(i);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == parameterID)
            return;
    }

    juce::ValueTree child("PARAM");
    child.setProperty("id", parameterID, nullptr);
    child.setProperty("value", parameterValue, nullptr);
    state.addChild(std::move(child), -1, nullptr);
}

inline void migrateLegacyCurveModes(juce::ValueTree& state)
{
    for (int i = 0; i < bandCount; ++i)
        setOrAppendParameterValue(state,
                                  "band" + juce::String(i) + "CurveMode",
                                  static_cast<float>(legacyCurveMode));

    stampCurrent(state);
}

inline void ensureDynamicTriggers(juce::ValueTree& state)
{
    for (int i = 0; i < bandCount; ++i)
        appendParameterValueIfMissing(state,
                                      "band" + juce::String(i) + "DynTrigger",
                                      0.0f); // Above
}

// The detector controls were added as append-only host parameters after
// DynTrigger. Older schema-v1 states are still valid, but must receive explicit
// defaults so loading them cannot inherit stale values from the live instance.
inline void ensureDynamicDetectorSurface(juce::ValueTree& state)
{
    for (int i = 0; i < bandCount; ++i)
    {
        const auto prefix = "band" + juce::String(i);
        appendParameterValueIfMissing(state, prefix + "DetectionMode", 1.0f); // RMS
        appendParameterValueIfMissing(state, prefix + "DetectorSource", 0.0f); // Internal wideband
        appendParameterValueIfMissing(
            state, prefix + "SidechainFreq",
            AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)]);
        appendParameterValueIfMissing(state, prefix + "SidechainQ", 1.0f);
    }
}

// Normalises a candidate copy before it reaches APVTS::replaceState(). The
// caller retains the original tree, so Reject is transactional by construction.
[[nodiscard]] inline LoadKind prepareForLoad(juce::ValueTree& candidate)
{
    const auto kind = classify(candidate);
    if (kind == LoadKind::LegacyWithoutSchema)
        migrateLegacyCurveModes(candidate);
    if (kind != LoadKind::Reject)
    {
        ensureDynamicTriggers(candidate);
        ensureDynamicDetectorSurface(candidate);
    }
    return kind;
}

} // namespace AIEQStateSchema
