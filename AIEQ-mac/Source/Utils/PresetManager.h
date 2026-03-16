#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "Logger.h"

//==============================================================================
/**
 * Factory Preset Manager for AI Equalizer Pro
 * 
 * Features:
 * - Load presets from XML/JSON files
 * - Genre-based presets (Vocals, Drums, Bass, Master, EDM, etc.)
 * - Save/load user presets
 * - Preset categories
 */
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category;  // "Vocals", "Drums", "Bass", "Master", "EDM", "User", etc.
        juce::String description;
        juce::ValueTree state;  // APVTS state
        juce::File filePath;     // For user presets
    };
    
    PresetManager(juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager() = default;
    
    //==============================================================================
    // Factory presets (built-in)
    void loadFactoryPresets();
    std::vector<Preset> getFactoryPresets() const { return factoryPresets; }
    std::vector<Preset> getPresetsByCategory(const juce::String& category) const;
    
    //==============================================================================
    // User presets
    juce::File getUserPresetsDirectory() const;
    std::vector<Preset> getUserPresets() const;
    bool saveUserPreset(const juce::String& name, const juce::String& category = "User");
    bool loadPreset(const Preset& preset);
    bool deleteUserPreset(const Preset& preset);
    
    //==============================================================================
    // Preset operations
    bool exportPreset(const Preset& preset, const juce::File& targetFile) const;
    Preset importPreset(const juce::File& file) const;
    
    //==============================================================================
    // Get all presets (factory + user)
    std::vector<Preset> getAllPresets() const;
    std::vector<juce::String> getCategories() const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factoryPresets;
    
    void createDefaultFactoryPresets();
    Preset createPreset(const juce::String& name, const juce::String& category,
                       const juce::String& description,
                       std::function<void(juce::AudioProcessorValueTreeState&)> setupFunc);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

