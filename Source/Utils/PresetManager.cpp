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
    // Offline helpers: modify ValueTree directly without touching live APVTS.
    // This avoids audible glitches and host automation events during factory preset init.
    auto setParamInTree = [this](juce::ValueTree& state, const juce::String& paramId, float value)
    {
        // APVTS stores parameters as children with "id" property.
        // We need the parameter's normalization range to convert value→0-1.
        if (auto* param = apvts.getParameter(paramId))
        {
            float normalized = param->convertTo0to1(value);
            auto paramTree = state.getChildWithProperty("id", paramId);
            if (paramTree.isValid())
                paramTree.setProperty("value", normalized, nullptr);
        }
    };

    auto setBand = [&setParamInTree](juce::ValueTree& state, int band,
                      int type, float freq, float gain, float q)
    {
        juce::String p = "band" + juce::String(band);
        setParamInTree(state, p + "Type",    static_cast<float>(type));
        setParamInTree(state, p + "Freq",    freq);
        setParamInTree(state, p + "Gain",    gain);
        setParamInTree(state, p + "Q",       q);
        setParamInTree(state, p + "Enabled", 1.0f);
    };

    auto setProfile = [&setParamInTree](juce::ValueTree& state, float profile)
    {
        setParamInTree(state, "sourceProfile", profile);
    };

    auto setNumBands = [&setParamInTree](juce::ValueTree& state, int n)
    {
        setParamInTree(state, "numActiveBands", static_cast<float>(n));
    };

    //==========================================================================
    //  INIT
    //==========================================================================
    factoryPresets.push_back(createPreset("Init (Flat)", "Utility",
        "All bands at 0 dB — clean starting point",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f);
            setNumBands(a, 8);
            // All gains already default to 0
        }));

    //==========================================================================
    //  VOCALS
    //==========================================================================
    factoryPresets.push_back(createPreset("Vocal Clarity", "Vocals",
        "Clear, upfront vocal with controlled low-end and airy top",
        [&](juce::ValueTree& a) {
            setProfile(a, 1.0f); setNumBands(a, 5);
            setBand(a, 0,  0,    80, 0.0f, 1.0f);   // LowCut 80 Hz
            setBand(a, 1,  2,   300,-2.0f, 1.2f);    // Cut boxiness 300 Hz
            setBand(a, 2,  2,  3000, 2.5f, 1.5f);    // Presence 3 kHz
            setBand(a, 3,  2,  6000,-1.0f, 2.0f);    // Tame sibilance 6 kHz
            setBand(a, 4,  3, 12000, 2.0f, 0.7f);    // Air shelf 12 kHz
        }));

    factoryPresets.push_back(createPreset("Vocal Warmth", "Vocals",
        "Rich, warm vocal with body and smooth top-end",
        [&](juce::ValueTree& a) {
            setProfile(a, 1.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    60, 0.0f, 1.0f);    // LowCut 60 Hz
            setBand(a, 1,  2,   200, 2.5f, 0.8f);    // Warmth 200 Hz
            setBand(a, 2,  2,  4000, 1.5f, 1.2f);    // Gentle presence
            setBand(a, 3,  3, 10000, 1.0f, 0.7f);    // Smooth air
        }));

    factoryPresets.push_back(createPreset("Vocal De-Harsh", "Vocals",
        "Tame harsh upper mids without losing definition",
        [&](juce::ValueTree& a) {
            setProfile(a, 1.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    80, 0.0f, 1.0f);    // LowCut
            setBand(a, 1,  2,  2500, 1.5f, 1.0f);    // Presence lift below harsh zone
            setBand(a, 2,  2,  5000,-3.0f, 2.5f);    // Narrow harsh cut
            setBand(a, 3,  3, 11000, 2.0f, 0.7f);    // Restore air above
        }));

    factoryPresets.push_back(createPreset("Broadcast Voice", "Vocals",
        "Radio/podcast voice — proximity control, clarity, air",
        [&](juce::ValueTree& a) {
            setProfile(a, 1.0f); setNumBands(a, 5);
            setBand(a, 0,  0,   100, 0.0f, 1.0f);    // HPF 100 Hz
            setBand(a, 1,  2,   200,-1.5f, 1.0f);    // Tame proximity
            setBand(a, 2,  2,  3500, 3.0f, 1.5f);    // Clarity/presence
            setBand(a, 3,  2,  6500,-1.5f, 2.0f);    // De-ess zone
            setBand(a, 4,  3, 12000, 2.5f, 0.7f);    // Air
        }));

    //==========================================================================
    //  DRUMS
    //==========================================================================
    factoryPresets.push_back(createPreset("Drum Punch", "Drums",
        "Punchy drums — tight low-end, reduced mud, snappy attack",
        [&](juce::ValueTree& a) {
            setProfile(a, 2.0f); setNumBands(a, 5);
            setBand(a, 0,  1,    60, 3.0f, 0.7f);    // Low shelf boost
            setBand(a, 1,  2,   400,-2.5f, 1.0f);    // Cut mud
            setBand(a, 2,  2,  3000, 2.0f, 1.5f);    // Stick attack
            setBand(a, 3,  2,  8000, 1.5f, 0.8f);    // Shimmer
            setBand(a, 4,  3, 12000, 1.0f, 0.7f);    // Air
        }));

    factoryPresets.push_back(createPreset("Kick & Low Tom Focus", "Drums",
        "Sub emphasis and beater click for kick and low toms",
        [&](juce::ValueTree& a) {
            setProfile(a, 2.0f); setNumBands(a, 4);
            setBand(a, 0,  1,    50, 4.0f, 0.7f);    // Sub boost
            setBand(a, 1,  2,   300,-3.0f, 1.2f);    // Cut mud/ring
            setBand(a, 2,  2,  3500, 2.5f, 1.5f);    // Beater click
            setBand(a, 3,  4,  8000, 0.0f, 1.0f);    // LPF for focus
        }));

    factoryPresets.push_back(createPreset("Snare Crack", "Drums",
        "Tight snare with body, crack and sizzle",
        [&](juce::ValueTree& a) {
            setProfile(a, 2.0f); setNumBands(a, 4);
            setBand(a, 0,  0,   100, 0.0f, 1.0f);    // HPF remove rumble
            setBand(a, 1,  2,   200, 2.0f, 0.8f);    // Body/fatness
            setBand(a, 2,  2,  2500, 3.0f, 1.5f);    // Crack
            setBand(a, 3,  3,  8000, 2.0f, 0.7f);    // Wire sizzle
        }));

    //==========================================================================
    //  BASS
    //==========================================================================
    factoryPresets.push_back(createPreset("Bass Clarity", "Bass",
        "Defined bass with sub weight and midrange cut-through",
        [&](juce::ValueTree& a) {
            setProfile(a, 3.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    30, 0.0f, 1.0f);    // HPF sub-rumble
            setBand(a, 1,  2,    80, 3.0f, 0.8f);    // Sub weight
            setBand(a, 2,  2,   250,-2.0f, 1.0f);    // Cut mud
            setBand(a, 3,  2,   700, 2.0f, 1.5f);    // Note definition
        }));

    factoryPresets.push_back(createPreset("Bass Warmth", "Bass",
        "Warm, round bass with smooth top roll-off",
        [&](juce::ValueTree& a) {
            setProfile(a, 3.0f); setNumBands(a, 3);
            setBand(a, 0,  1,    60, 3.5f, 0.7f);    // Low shelf warm
            setBand(a, 1,  2,   150, 2.0f, 0.8f);    // Body
            setBand(a, 2,  3,  5000,-2.0f, 0.7f);    // Top roll-off
        }));

    //==========================================================================
    //  GUITAR
    //==========================================================================
    factoryPresets.push_back(createPreset("Electric Guitar Body", "Guitar",
        "Full-bodied electric with controlled mids and presence",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    80, 0.0f, 1.0f);    // HPF low rumble
            setBand(a, 1,  2,   250, 2.0f, 0.8f);    // Body
            setBand(a, 2,  2,   800,-2.0f, 1.5f);    // Cut honk/nasal
            setBand(a, 3,  2,  3000, 2.0f, 1.2f);    // Presence/bite
        }));

    factoryPresets.push_back(createPreset("Acoustic Guitar Shimmer", "Guitar",
        "Open, shimmering acoustic with reduced boom",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    60, 0.0f, 1.0f);    // HPF boom
            setBand(a, 1,  2,   200,-2.0f, 1.0f);    // Cut boominess
            setBand(a, 2,  2,  3000, 2.5f, 1.0f);    // String shimmer
            setBand(a, 3,  3, 10000, 2.0f, 0.7f);    // Air/sparkle
        }));

    //==========================================================================
    //  KEYS & SYNTH
    //==========================================================================
    factoryPresets.push_back(createPreset("Piano Clarity", "Keys",
        "Clean piano with reduced mud and clear attack",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 4);
            setBand(a, 0,  0,    40, 0.0f, 1.0f);    // HPF sub rumble
            setBand(a, 1,  2,   300,-2.0f, 1.0f);    // Cut mud
            setBand(a, 2,  2,  2500, 2.0f, 1.2f);    // Key attack clarity
            setBand(a, 3,  3,  8000, 1.5f, 0.7f);    // Brilliance
        }));

    factoryPresets.push_back(createPreset("Synth Wide", "Keys",
        "Wide synth pad with full low-end and sparkly top",
        [&](juce::ValueTree& a) {
            setProfile(a, 4.0f); setNumBands(a, 4);
            setBand(a, 0,  1,    50, 2.0f, 0.7f);    // Low shelf fullness
            setBand(a, 1,  2,   500,-1.5f, 1.0f);    // Reduce mid clutter
            setBand(a, 2,  2,  5000, 2.0f, 1.0f);    // Presence/edge
            setBand(a, 3,  3, 10000, 3.0f, 0.7f);    // High sparkle
        }));

    //==========================================================================
    //  MASTER BUS
    //==========================================================================
    factoryPresets.push_back(createPreset("Master Gentle", "Master",
        "Subtle master polish — low-end weight, air, slight mid scoop",
        [&](juce::ValueTree& a) {
            setProfile(a, 5.0f); setNumBands(a, 3);
            setBand(a, 0,  1,    40, 1.0f, 0.7f);    // Low shelf +1
            setBand(a, 1,  2,   350,-0.8f, 0.6f);    // Gentle mid scoop
            setBand(a, 2,  3, 10000, 1.0f, 0.7f);    // Air shelf +1
        }));

    factoryPresets.push_back(createPreset("Master Loud", "Master",
        "Aggressive master — punchy low-end, forward mids, bright top",
        [&](juce::ValueTree& a) {
            setProfile(a, 5.0f); setNumBands(a, 5);
            setBand(a, 0,  0,    25, 0.0f, 1.0f);    // HPF sub-rumble
            setBand(a, 1,  1,    50, 2.0f, 0.7f);    // Low shelf punch
            setBand(a, 2,  2,   400,-1.5f, 0.8f);    // Cut mud
            setBand(a, 3,  2,  2000, 1.5f, 1.0f);    // Forward presence
            setBand(a, 4,  3,  8000, 2.5f, 0.7f);    // Bright air
        }));

    factoryPresets.push_back(createPreset("Master Clean", "Master",
        "Transparent mastering polish — very subtle corrections",
        [&](juce::ValueTree& a) {
            setProfile(a, 5.0f); setNumBands(a, 3);
            setBand(a, 0,  1,    30, 0.5f, 0.7f);    // Hint of sub weight
            setBand(a, 1,  2,   500,-0.5f, 0.5f);    // Micro mid scoop
            setBand(a, 2,  3, 12000, 0.5f, 0.7f);    // Touch of air
        }));

    //==========================================================================
    //  EDM / ELECTRONIC
    //==========================================================================
    factoryPresets.push_back(createPreset("EDM Sizzle", "EDM",
        "Sizzling electronic — sub weight, scooped mids, bright top",
        [&](juce::ValueTree& a) {
            setProfile(a, 6.0f); setNumBands(a, 5);
            setBand(a, 0,  0,    25, 0.0f, 1.0f);    // HPF sub-sub
            setBand(a, 1,  2,    50, 3.0f, 0.8f);    // Sub weight
            setBand(a, 2,  2,   300,-2.0f, 0.8f);    // Scoop mids
            setBand(a, 3,  2,  5000, 2.0f, 1.0f);    // Sizzle
            setBand(a, 4,  3, 12000, 3.0f, 0.7f);    // Sparkle shelf
        }));

    factoryPresets.push_back(createPreset("EDM Warm Bass", "EDM",
        "Warm sub-heavy electronic with controlled top",
        [&](juce::ValueTree& a) {
            setProfile(a, 6.0f); setNumBands(a, 3);
            setBand(a, 0,  1,    40, 4.0f, 0.7f);    // Heavy sub shelf
            setBand(a, 1,  2,   200,-1.5f, 0.8f);    // Clean up low-mids
            setBand(a, 2,  2,  4000, 1.5f, 1.0f);    // Gentle presence
        }));

    //==========================================================================
    //  UTILITY / CREATIVE
    //==========================================================================
    factoryPresets.push_back(createPreset("Telephone Effect", "Creative",
        "Vintage telephone/radio band-pass filter",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 3);
            setBand(a, 0,  0,   300, 0.0f, 1.0f);    // HPF 300 Hz
            setBand(a, 1,  2,  1000, 3.0f, 0.8f);    // Mid presence
            setBand(a, 2,  4,  3500, 0.0f, 1.0f);    // LPF 3.5 kHz
        }));

    factoryPresets.push_back(createPreset("De-Mud", "Utility",
        "General-purpose mud removal — cleans 200-500 Hz range",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 3);
            setBand(a, 0,  2,   250,-2.5f, 0.8f);    // Wide mud cut
            setBand(a, 1,  2,   400,-1.5f, 1.2f);    // Narrow boxy cut
            setBand(a, 2,  3,  8000, 1.0f, 0.7f);    // Compensate air
        }));

    factoryPresets.push_back(createPreset("Sibilance Tamer", "Utility",
        "Broadband sibilance reduction for harsh sources",
        [&](juce::ValueTree& a) {
            setProfile(a, 0.0f); setNumBands(a, 3);
            setBand(a, 0,  2,  5500,-2.5f, 2.0f);    // Narrow 5.5k cut
            setBand(a, 1,  2,  7500,-2.0f, 2.0f);    // Narrow 7.5k cut
            setBand(a, 2,  3, 12000, 1.0f, 0.7f);    // Restore air above
        }));
}

PresetManager::Preset PresetManager::createPreset(
    const juce::String& name,
    const juce::String& category,
    const juce::String& description,
    std::function<void(juce::ValueTree&)> setupFunc)
{
    Preset preset;
    preset.name = name;
    preset.category = category;
    preset.description = description;

    // Build preset state offline: deep-copy the default APVTS state,
    // modify the copy via setupFunc, never touch the live APVTS.
    // Zero audible glitches, zero host automation events.
    preset.state = apvts.copyState().createCopy();
    setupFunc(preset.state);

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

