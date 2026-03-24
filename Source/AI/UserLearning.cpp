#include "UserLearning.h"
#include <cmath>

//==============================================================================
UserLearningSystem::UserLearningSystem()
{
    // Initialize frequency preference regions
    currentProfile.frequencyPrefs.resize(static_cast<size_t>(numFrequencyRegions));
    
    for (int i = 0; i < numFrequencyRegions; ++i)
    {
        currentProfile.frequencyPrefs[static_cast<size_t>(i)].centerFreq = getRegionCenterFreq(i);
    }
    
    // Try to load from default location
    loadFromDefaultLocation();
}

//==============================================================================
void UserLearningSystem::recordAdjustment(const UserAdjustment& adjustment)
{
    if (!learningEnabled)
        return;
    
    // Add to recent adjustments
    recentAdjustments.push_back(adjustment);
    if (recentAdjustments.size() > maxRecentAdjustments)
        recentAdjustments.pop_front();
    
    // Update preferences
    updatePreferences(adjustment);
    
    currentProfile.totalAdjustments++;
    currentProfile.lastUpdated = juce::Time::currentTimeMillis();
    
    // Auto-save periodically
    if (currentProfile.totalAdjustments % 10 == 0)
        saveToDefaultLocation();
}

void UserLearningSystem::recordAISuggestionAccepted(const juce::String& /*suggestionType*/, 
                                                     float /*freq*/, float /*gain*/, float /*q*/)
{
    currentProfile.aiSuggestionsAccepted++;
}

void UserLearningSystem::recordAISuggestionRejected(const juce::String& /*suggestionType*/)
{
    currentProfile.aiSuggestionsRejected++;

    // When the user rejects AI suggestions, reduce the correction strength preference
    // slightly so future suggestions become less aggressive. The decay is deliberately
    // small (2 %) so a single rejection doesn't overly distort the learned profile;
    // repeated rejections will converge the preference toward more conservative values.
    constexpr float kDecayRate = 0.02f;
    currentProfile.correctionStrengthPref = juce::jlimit(
        0.2f, 1.0f,
        currentProfile.correctionStrengthPref * (1.0f - kDecayRate));

    // Auto-save periodically (same cadence as recordAdjustment)
    if (currentProfile.aiSuggestionsRejected % 5 == 0)
        saveToDefaultLocation();
}

void UserLearningSystem::recordAISuggestionCorrected(const juce::String& suggestionType,
                                                      float suggestedGain, float actualGain,
                                                      float suggestedQ, float actualQ)
{
    currentProfile.aiSuggestionsCorrected++;
    
    // Learn from the correction
    UserAdjustment adjustment;
    adjustment.gainBefore = suggestedGain;
    adjustment.gainAfter = actualGain;
    adjustment.qBefore = suggestedQ;
    adjustment.qAfter = actualQ;
    adjustment.correctedAISuggestion = true;
    adjustment.aiSuggestionType = suggestionType;
    
    // This will influence future AI suggestions for this type of problem
    updatePreferences(adjustment);
}

//==============================================================================
void UserLearningSystem::updatePreferences(const UserAdjustment& adjustment)
{
    float freq = adjustment.freqAfter;
    float gainDelta = adjustment.gainAfter - adjustment.gainBefore;
    float q = adjustment.qAfter;
    
    // Update global preferences
    currentProfile.overallGainBias = (1.0f - learningRate) * currentProfile.overallGainBias + 
                                      learningRate * gainDelta;
    currentProfile.qWidthPreference = (1.0f - learningRate) * currentProfile.qWidthPreference + 
                                       learningRate * q;
    
    // Update frequency-specific preferences
    updateFrequencyPreference(currentProfile.frequencyPrefs, freq, gainDelta, q);
    
    // Update genre-specific preferences if genre is known
    if (adjustment.context.genre.isNotEmpty())
    {
        auto& genrePrefs = currentProfile.genrePrefs[adjustment.context.genre];
        if (genrePrefs.empty())
        {
            genrePrefs.resize(static_cast<size_t>(numFrequencyRegions));
            for (int i = 0; i < numFrequencyRegions; ++i)
                genrePrefs[static_cast<size_t>(i)].centerFreq = getRegionCenterFreq(i);
        }
        updateFrequencyPreference(genrePrefs, freq, gainDelta, q);
    }
}

void UserLearningSystem::updateFrequencyPreference(std::vector<FrequencyPreference>& prefs,
                                                    float freq, float gainDelta, float q)
{
    int regionIdx = getRegionForFreq(freq);
    if (regionIdx < 0 || regionIdx >= static_cast<int>(prefs.size()))
        return;
    
    auto& pref = prefs[static_cast<size_t>(regionIdx)];
    
    // Exponential moving average
    pref.avgGainAdjustment = (1.0f - learningRate) * pref.avgGainAdjustment + learningRate * gainDelta;
    pref.avgQPreference = (1.0f - learningRate) * pref.avgQPreference + learningRate * q;
    pref.sampleCount++;
    pref.confidence = std::min(1.0f, static_cast<float>(pref.sampleCount) / 
                               static_cast<float>(minSamplesForConfidence));
}

//==============================================================================
float UserLearningSystem::getPreferredGainForFrequency(float freq, const juce::String& genre) const
{
    // Try genre-specific first
    if (genre.isNotEmpty())
    {
        auto it = currentProfile.genrePrefs.find(genre);
        if (it != currentProfile.genrePrefs.end() && !it->second.empty())
        {
            float genreGain = interpolatePreference(it->second, freq, true);
            if (std::abs(genreGain) > 0.01f)
                return genreGain;
        }
    }
    
    // Fall back to global
    return interpolatePreference(currentProfile.frequencyPrefs, freq, true);
}

float UserLearningSystem::getPreferredQForFrequency(float freq, const juce::String& genre) const
{
    // Try genre-specific first
    if (genre.isNotEmpty())
    {
        auto it = currentProfile.genrePrefs.find(genre);
        if (it != currentProfile.genrePrefs.end() && !it->second.empty())
        {
            float genreQ = interpolatePreference(it->second, freq, false);
            if (genreQ > 0.1f)
                return genreQ;
        }
    }
    
    // Fall back to global
    float q = interpolatePreference(currentProfile.frequencyPrefs, freq, false);
    return q > 0.1f ? q : currentProfile.qWidthPreference;
}

float UserLearningSystem::getSuggestedCorrectionStrength() const
{
    return currentProfile.correctionStrengthPref;
}

void UserLearningSystem::adjustAISuggestion(float& gain, float& q, float freq, 
                                            const juce::String& /*problemType*/) const
{
    if (!learningEnabled || currentProfile.totalAdjustments < minSamplesForConfidence)
        return;
    
    // Get learned preferences
    float preferredGain = getPreferredGainForFrequency(freq);
    float preferredQ = getPreferredQForFrequency(freq);
    
    // Blend AI suggestion with learned preference
    float blendFactor = 0.3f; // 30% weight to learned preference
    
    // Adjust gain (user might prefer stronger or weaker corrections)
    if (std::abs(preferredGain) > 0.1f)
    {
        // If user typically adjusts in same direction, strengthen; opposite, weaken
        if ((gain < 0 && preferredGain < 0) || (gain > 0 && preferredGain > 0))
        {
            gain *= (1.0f + blendFactor * std::abs(preferredGain) / 6.0f);
        }
        else
        {
            gain *= (1.0f - blendFactor * 0.5f);
        }
    }
    
    // Adjust Q toward preferred value
    q = (1.0f - blendFactor) * q + blendFactor * preferredQ;
    
    // Apply overall gain bias
    gain += currentProfile.overallGainBias * blendFactor;
}

//==============================================================================
float UserLearningSystem::interpolatePreference(const std::vector<FrequencyPreference>& prefs,
                                                 float freq, bool forGain) const
{
    if (prefs.empty())
        return forGain ? 0.0f : 1.0f;
    
    // Find surrounding preferences
    int idx1 = -1, idx2 = -1;
    
    for (size_t i = 0; i < prefs.size(); ++i)
    {
        if (prefs[i].centerFreq <= freq)
            idx1 = static_cast<int>(i);
        if (prefs[i].centerFreq >= freq && idx2 < 0)
            idx2 = static_cast<int>(i);
    }
    
    if (idx1 < 0) idx1 = 0;
    if (idx2 < 0) idx2 = static_cast<int>(prefs.size()) - 1;
    if (idx1 == idx2)
    {
        const auto& p = prefs[static_cast<size_t>(idx1)];
        return forGain ? p.avgGainAdjustment * p.confidence : p.avgQPreference;
    }
    
    // Interpolate
    const auto& p1 = prefs[static_cast<size_t>(idx1)];
    const auto& p2 = prefs[static_cast<size_t>(idx2)];
    
    float logFreq = std::log2(freq);
    float logF1 = std::log2(p1.centerFreq);
    float logF2 = std::log2(p2.centerFreq);
    // Guard against identical center frequencies (division by zero)
    if (std::abs(logF2 - logF1) < 1e-6f)
    {
        return forGain ? p1.avgGainAdjustment * p1.confidence : p1.avgQPreference;
    }
    float t = (logFreq - logF1) / (logF2 - logF1);
    t = juce::jlimit(0.0f, 1.0f, t);
    
    if (forGain)
    {
        float g1 = p1.avgGainAdjustment * p1.confidence;
        float g2 = p2.avgGainAdjustment * p2.confidence;
        return (1.0f - t) * g1 + t * g2;
    }
    else
    {
        return (1.0f - t) * p1.avgQPreference + t * p2.avgQPreference;
    }
}

//==============================================================================
void UserLearningSystem::setCurrentProfile(const juce::String& name)
{
    auto it = savedProfiles.find(name);
    if (it != savedProfiles.end())
    {
        currentProfile = it->second;
    }
}

void UserLearningSystem::createNewProfile(const juce::String& name)
{
    UserProfile newProfile;
    newProfile.name = name;
    newProfile.frequencyPrefs.resize(static_cast<size_t>(numFrequencyRegions));
    for (int i = 0; i < numFrequencyRegions; ++i)
        newProfile.frequencyPrefs[static_cast<size_t>(i)].centerFreq = getRegionCenterFreq(i);
    
    savedProfiles[name] = newProfile;
    currentProfile = newProfile;
}

void UserLearningSystem::deleteProfile(const juce::String& name)
{
    savedProfiles.erase(name);
}

juce::StringArray UserLearningSystem::getAvailableProfiles() const
{
    juce::StringArray profiles;
    for (const auto& pair : savedProfiles)
        profiles.add(pair.first);
    return profiles;
}

//==============================================================================
void UserLearningSystem::saveToFile(const juce::File& file) const
{
    // FIX: juce::var() is null by default — getDynamicObject() returns nullptr → crash.
    // Must create the DynamicObject first and wrap it in a var.
    auto* root = new juce::DynamicObject();

    root->setProperty("currentProfile", profileToVar(currentProfile));

    // Save all profiles
    juce::var profilesArray;
    for (const auto& pair : savedProfiles)
    {
        juce::var profileData = profileToVar(pair.second);
        if (auto* profileObj = profileData.getDynamicObject())
            profileObj->setProperty("name", pair.first);
        profilesArray.append(profileData);
    }
    root->setProperty("profiles", profilesArray);

    juce::var data(root);
    juce::String json = juce::JSON::toString(data);
    file.replaceWithText(json);
}

bool UserLearningSystem::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;
    
    juce::String json = file.loadFileAsString();
    juce::var data = juce::JSON::parse(json);
    
    if (!data.isObject())
        return false;
    
    // Load current profile
    if (data.hasProperty("currentProfile"))
        currentProfile = varToProfile(data["currentProfile"]);
    
    // Load all profiles
    if (data.hasProperty("profiles"))
    {
        savedProfiles.clear();
        juce::var profiles = data["profiles"];
        if (profiles.isArray())
        {
            for (int i = 0; i < profiles.size(); ++i)
            {
                UserProfile profile = varToProfile(profiles[i]);
                if (profiles[i].hasProperty("name"))
                    profile.name = profiles[i]["name"].toString();
                if (!profile.name.isEmpty())
                    savedProfiles[profile.name] = profile;
            }
        }
    }
    
    return true;
}

void UserLearningSystem::saveToDefaultLocation() const
{
    juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginData = appData.getChildFile("AIEqualizerPro");
    pluginData.createDirectory();
    saveToFile(pluginData.getChildFile("user_preferences.json"));
}

bool UserLearningSystem::loadFromDefaultLocation()
{
    juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginData = appData.getChildFile("AIEqualizerPro");
    return loadFromFile(pluginData.getChildFile("user_preferences.json"));
}

//==============================================================================
juce::var UserLearningSystem::profileToVar(const UserProfile& profile) const
{
    juce::DynamicObject* obj = new juce::DynamicObject();
    
    obj->setProperty("name", profile.name);
    obj->setProperty("overallGainBias", profile.overallGainBias);
    obj->setProperty("qWidthPreference", profile.qWidthPreference);
    obj->setProperty("correctionStrengthPref", profile.correctionStrengthPref);
    obj->setProperty("totalAdjustments", profile.totalAdjustments);
    obj->setProperty("aiSuggestionsAccepted", profile.aiSuggestionsAccepted);
    obj->setProperty("aiSuggestionsCorrected", profile.aiSuggestionsCorrected);
    
    // Save frequency preferences
    juce::var freqPrefsArray;
    for (const auto& pref : profile.frequencyPrefs)
    {
        juce::DynamicObject* prefObj = new juce::DynamicObject();
        prefObj->setProperty("centerFreq", pref.centerFreq);
        prefObj->setProperty("avgGainAdjustment", pref.avgGainAdjustment);
        prefObj->setProperty("avgQPreference", pref.avgQPreference);
        prefObj->setProperty("sampleCount", pref.sampleCount);
        freqPrefsArray.append(juce::var(prefObj));
    }
    obj->setProperty("frequencyPrefs", freqPrefsArray);
    
    return juce::var(obj);
}

UserLearningSystem::UserProfile UserLearningSystem::varToProfile(const juce::var& v) const
{
    UserProfile profile;
    
    if (!v.isObject())
        return profile;
    
    profile.name = v.getProperty("name", "Default").toString();
    profile.overallGainBias = static_cast<float>(v.getProperty("overallGainBias", 0.0));
    profile.qWidthPreference = static_cast<float>(v.getProperty("qWidthPreference", 1.0));
    profile.correctionStrengthPref = static_cast<float>(v.getProperty("correctionStrengthPref", 0.7));
    profile.totalAdjustments = static_cast<int>(v.getProperty("totalAdjustments", 0));
    profile.aiSuggestionsAccepted = static_cast<int>(v.getProperty("aiSuggestionsAccepted", 0));
    profile.aiSuggestionsCorrected = static_cast<int>(v.getProperty("aiSuggestionsCorrected", 0));
    
    // Load frequency preferences
    juce::var freqPrefs = v.getProperty("frequencyPrefs", juce::var());
    if (freqPrefs.isArray())
    {
        profile.frequencyPrefs.clear();
        for (int i = 0; i < freqPrefs.size(); ++i)
        {
            FrequencyPreference pref;
            pref.centerFreq = static_cast<float>(freqPrefs[i].getProperty("centerFreq", 1000.0));
            pref.avgGainAdjustment = static_cast<float>(freqPrefs[i].getProperty("avgGainAdjustment", 0.0));
            pref.avgQPreference = static_cast<float>(freqPrefs[i].getProperty("avgQPreference", 1.0));
            pref.sampleCount = static_cast<int>(freqPrefs[i].getProperty("sampleCount", 0));
            pref.confidence = std::min(1.0f, static_cast<float>(pref.sampleCount) / 
                                       static_cast<float>(minSamplesForConfidence));
            profile.frequencyPrefs.push_back(pref);
        }
    }
    
    // Ensure we have all regions
    while (profile.frequencyPrefs.size() < static_cast<size_t>(numFrequencyRegions))
    {
        FrequencyPreference pref;
        pref.centerFreq = getRegionCenterFreq(static_cast<int>(profile.frequencyPrefs.size()));
        profile.frequencyPrefs.push_back(pref);
    }
    
    return profile;
}

//==============================================================================
float UserLearningSystem::getAISuggestionAcceptanceRate() const
{
    int total = currentProfile.aiSuggestionsAccepted + currentProfile.aiSuggestionsCorrected;
    if (total == 0) return 0.0f;
    return static_cast<float>(currentProfile.aiSuggestionsAccepted) / static_cast<float>(total);
}

juce::String UserLearningSystem::getMostAdjustedFrequencyRegion() const
{
    int maxIdx = 0;
    int maxCount = 0;
    
    for (size_t i = 0; i < currentProfile.frequencyPrefs.size(); ++i)
    {
        if (currentProfile.frequencyPrefs[i].sampleCount > maxCount)
        {
            maxCount = currentProfile.frequencyPrefs[i].sampleCount;
            maxIdx = static_cast<int>(i);
        }
    }
    
    float freq = getRegionCenterFreq(maxIdx);
    if (freq < 100) return "Sub Bass";
    if (freq < 300) return "Bass";
    if (freq < 800) return "Low Mids";
    if (freq < 2500) return "Mids";
    if (freq < 6000) return "High Mids";
    if (freq < 12000) return "Presence";
    return "Air";
}

//==============================================================================
float UserLearningSystem::getRegionCenterFreq(int region) const
{
    // Logarithmically spaced from 30 Hz to 16 kHz
    float minLog = std::log2(30.0f);
    float maxLog = std::log2(16000.0f);
    float step = (maxLog - minLog) / static_cast<float>(numFrequencyRegions - 1);
    return std::pow(2.0f, minLog + step * static_cast<float>(region));
}

int UserLearningSystem::getRegionForFreq(float freq) const
{
    float logFreq = std::log2(freq);
    float minLog = std::log2(30.0f);
    float maxLog = std::log2(16000.0f);
    float normalized = (logFreq - minLog) / (maxLog - minLog);
    return juce::jlimit(0, numFrequencyRegions - 1, 
                        static_cast<int>(normalized * static_cast<float>(numFrequencyRegions)));
}
