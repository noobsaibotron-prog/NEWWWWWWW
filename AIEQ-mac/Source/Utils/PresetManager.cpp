#include "PresetManager.h"
#include <algorithm>

//==============================================================================
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts_)
    : apvts(apvts_)
{
    loadFactoryPresets();
}

void PresetManager::loadFactoryPresets()
{
    // Avoid reloading if already populated to prevent repeated IO/work
    if (!factoryPresets.empty())
        return;

    factoryPresets.clear();
    createDefaultFactoryPresets();
    AIEQ_LOG_INFO("Loaded " + juce::String(factoryPresets.size()) + " factory presets");
}

void PresetManager::createDefaultFactoryPresets()
{
    // Vocals preset
    factoryPresets.push_back(createPreset("Vocals - Bright", "Vocals", 
        "Bright vocal preset with presence boost and sibilance control",
        [](juce::AudioProcessorValueTreeState& apvts) {
            // Set source profile
            if (auto* param = apvts.getParameter("sourceProfile"))
                param->setValueNotifyingHost(param->convertTo0to1(1.0f)); // Vocals
            
            // Typical vocal EQ: slight low cut, presence boost, high shelf
            // Band 0: Low cut at 80Hz
            if (auto* type = apvts.getParameter("band0Type"))
                type->setValueNotifyingHost(type->convertTo0to1(0.0f)); // LowCut
            if (auto* freq = apvts.getParameter("band0Freq"))
                freq->setValueNotifyingHost(freq->convertTo0to1(80.0f));
            
            // Band 1: Presence boost at 3kHz
            if (auto* freq = apvts.getParameter("band1Freq"))
                freq->setValueNotifyingHost(freq->convertTo0to1(3000.0f));
            if (auto* gain = apvts.getParameter("band1Gain"))
                gain->setValueNotifyingHost(gain->convertTo0to1(2.0f));
            if (auto* q = apvts.getParameter("band1Q"))
                q->setValueNotifyingHost(q->convertTo0to1(1.5f));
        }));
    
    // Drums preset
    factoryPresets.push_back(createPreset("Drums - Punchy", "Drums",
        "Punchy drum preset with low-end emphasis and high-end clarity",
        [](juce::AudioProcessorValueTreeState& apvts) {
            if (auto* param = apvts.getParameter("sourceProfile"))
                param->setValueNotifyingHost(param->convertTo0to1(2.0f)); // Drums
            
            // Low shelf boost
            if (auto* freq = apvts.getParameter("band0Freq"))
                freq->setValueNotifyingHost(freq->convertTo0to1(60.0f));
            if (auto* gain = apvts.getParameter("band0Gain"))
                gain->setValueNotifyingHost(gain->convertTo0to1(3.0f));
            if (auto* type = apvts.getParameter("band0Type"))
                type->setValueNotifyingHost(type->convertTo0to1(1.0f)); // LowShelf
        }));
    
    // Bass preset
    factoryPresets.push_back(createPreset("Bass - Deep", "Bass",
        "Deep bass preset with sub-bass emphasis",
        [](juce::AudioProcessorValueTreeState& apvts) {
            if (auto* param = apvts.getParameter("sourceProfile"))
                param->setValueNotifyingHost(param->convertTo0to1(3.0f)); // Bass
            
            // Sub-bass boost
            if (auto* freq = apvts.getParameter("band0Freq"))
                freq->setValueNotifyingHost(freq->convertTo0to1(40.0f));
            if (auto* gain = apvts.getParameter("band0Gain"))
                gain->setValueNotifyingHost(gain->convertTo0to1(4.0f));
        }));
    
    // Master preset
    factoryPresets.push_back(createPreset("Master - Balanced", "Master",
        "Balanced master bus preset",
        [](juce::AudioProcessorValueTreeState& apvts) {
            if (auto* param = apvts.getParameter("sourceProfile"))
                param->setValueNotifyingHost(param->convertTo0to1(5.0f)); // Master
        }));
    
    // EDM preset
    factoryPresets.push_back(createPreset("EDM - Bright", "EDM",
        "Bright EDM preset with high-end sparkle",
        [](juce::AudioProcessorValueTreeState& apvts) {
            if (auto* param = apvts.getParameter("sourceProfile"))
                param->setValueNotifyingHost(param->convertTo0to1(6.0f)); // EDM
            
            // High shelf boost
            int lastBand = 7;
            if (auto* freq = apvts.getParameter("band" + juce::String(lastBand) + "Freq"))
                freq->setValueNotifyingHost(freq->convertTo0to1(10000.0f));
            if (auto* gain = apvts.getParameter("band" + juce::String(lastBand) + "Gain"))
                gain->setValueNotifyingHost(gain->convertTo0to1(2.0f));
            if (auto* type = apvts.getParameter("band" + juce::String(lastBand) + "Type"))
                type->setValueNotifyingHost(type->convertTo0to1(3.0f)); // HighShelf
        }));
}

PresetManager::Preset PresetManager::createPreset(
    const juce::String& name,
    const juce::String& category,
    const juce::String& description,
    std::function<void(juce::AudioProcessorValueTreeState&)> setupFunc)
{
    Preset preset;
    preset.name = name;
    preset.category = category;
    preset.description = description;
    
    // Save current state
    auto currentState = apvts.copyState();
    
    // Apply setup function directly to apvts (temporarily)
    setupFunc(apvts);
    
    // Get the modified state
    preset.state = apvts.copyState();
    
    // Restore original state
    apvts.replaceState(currentState);
    
    return preset;
}

std::vector<PresetManager::Preset> PresetManager::getPresetsByCategory(const juce::String& category) const
{
    std::vector<Preset> result;
    for (const auto& preset : factoryPresets)
    {
        if (preset.category == category)
            result.push_back(preset);
    }
    
    // Add user presets in this category
    auto userPresets = getUserPresets();
    for (const auto& preset : userPresets)
    {
        if (preset.category == category)
            result.push_back(preset);
    }
    
    return result;
}

juce::File PresetManager::getUserPresetsDirectory() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto presetDir = appData.getChildFile("AIEqualizerPro").getChildFile("Presets");
    if (!presetDir.exists())
        presetDir.createDirectory();
    return presetDir;
}

std::vector<PresetManager::Preset> PresetManager::getUserPresets() const
{
    std::vector<Preset> result;
    auto presetDir = getUserPresetsDirectory();
    
    if (!presetDir.exists())
        return result;
    
    auto files = presetDir.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& file : files)
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !xml->hasTagName("AIEqualizerPreset"))
            continue;

        auto* stateXml = xml->getChildByName("State");
        if (stateXml == nullptr || !stateXml->hasTagName("State"))
        {
            AIEQ_LOG_WARNING("Preset skipped (invalid or missing State): " + file.getFileName());
            continue;
        }

        Preset preset;
        preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        preset.category = xml->getStringAttribute("category", "User");
        preset.description = xml->getStringAttribute("description", "");
        preset.filePath = file;
        preset.state = juce::ValueTree::fromXml(*stateXml);

        if (preset.state.isValid())
            result.push_back(preset);
        else
            AIEQ_LOG_WARNING("Preset skipped (state invalid): " + file.getFileName());
    }
    
    return result;
}

bool PresetManager::saveUserPreset(const juce::String& name, const juce::String& category)
{
    try
    {
        auto presetDir = getUserPresetsDirectory();
        auto presetFile = presetDir.getChildFile(name.replaceCharacters(" /\\", "___") + ".xml");
        
        auto xml = std::make_unique<juce::XmlElement>("AIEqualizerPreset");
        xml->setAttribute("name", name);
        xml->setAttribute("category", category);
        xml->setAttribute("version", "2.1.0");
        
        // Save current APVTS state
        auto state = apvts.copyState();
        xml->addChildElement(state.createXml().release());
        
        if (xml->writeTo(presetFile))
        {
            AIEQ_LOG_INFO("Saved user preset: " + name);
            return true;
        }
        else
        {
            AIEQ_LOG_ERROR("Failed to save preset: " + name);
            return false;
        }
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("Exception saving preset: " + name);
        return false;
    }
}

bool PresetManager::loadPreset(const Preset& preset)
{
    try
    {
        apvts.replaceState(preset.state);
        AIEQ_LOG_INFO("Loaded preset: " + preset.name);
        return true;
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("Exception loading preset: " + preset.name);
        return false;
    }
}

bool PresetManager::deleteUserPreset(const Preset& preset)
{
    if (preset.filePath.exists())
    {
        if (preset.filePath.deleteFile())
        {
            AIEQ_LOG_INFO("Deleted preset: " + preset.name);
            return true;
        }
    }
    return false;
}

std::vector<PresetManager::Preset> PresetManager::getAllPresets() const
{
    auto all = factoryPresets;
    auto user = getUserPresets();
    all.insert(all.end(), user.begin(), user.end());
    return all;
}

std::vector<juce::String> PresetManager::getCategories() const
{
    std::vector<juce::String> categories;
    for (const auto& preset : getAllPresets())
    {
        if (std::find(categories.begin(), categories.end(), preset.category) == categories.end())
            categories.push_back(preset.category);
    }
    return categories;
}

bool PresetManager::exportPreset(const Preset& preset, const juce::File& targetFile) const
{
    try
    {
        auto xml = std::make_unique<juce::XmlElement>("AIEqualizerPreset");
        xml->setAttribute("name", preset.name);
        xml->setAttribute("category", preset.category);
        xml->setAttribute("description", preset.description);
        xml->setAttribute("version", "2.1.0");
        xml->addChildElement(preset.state.createXml().release());
        return xml->writeTo(targetFile);
    }
    catch (...)
    {
        return false;
    }
}

PresetManager::Preset PresetManager::importPreset(const juce::File& file) const
{
    Preset preset;
    try
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !xml->hasTagName("AIEqualizerPreset"))
        {
            AIEQ_LOG_WARNING("Import failed: invalid preset root in " + file.getFileName());
            return preset;
        }

        auto* stateXml = xml->getChildByName("State");
        if (stateXml == nullptr || !stateXml->hasTagName("State"))
        {
            AIEQ_LOG_WARNING("Import failed: missing State in " + file.getFileName());
            return preset;
        }

        preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        preset.category = xml->getStringAttribute("category", "User");
        preset.description = xml->getStringAttribute("description", "");
        preset.filePath = file;
        preset.state = juce::ValueTree::fromXml(*stateXml);
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("Exception importing preset: " + file.getFullPathName());
    }
    return preset;
}

