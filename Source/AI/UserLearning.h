#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>
#include <deque>

//==============================================================================
/**
 * User Learning System - Learns from user's EQ adjustments
 * 
 * Features:
 * - Records all user adjustments with context
 * - Builds preference profiles over time
 * - Provides personalized AI suggestions
 * - Persists learning across sessions
 * - Genre-specific learning
 */
class UserLearningSystem
{
public:
    //==============================================================================
    // Context for an adjustment
    struct AdjustmentContext
    {
        // Spectral context at time of adjustment
        float lowEnergy = 0.0f;      // 20-200 Hz average
        float midEnergy = 0.0f;      // 200-2000 Hz average
        float highEnergy = 0.0f;     // 2000-20000 Hz average
        
        // Problem context
        juce::String detectedProblem;
        float problemSeverity = 0.0f;
        
        // Genre context
        juce::String genre;
        
        // Time
        juce::int64 timestamp = 0;
    };
    
    // A recorded user adjustment
    struct UserAdjustment
    {
        int bandIndex = 0;
        
        // Before values
        float freqBefore = 1000.0f;
        float gainBefore = 0.0f;
        float qBefore = 1.0f;
        int typeBefore = 2; // Peak
        
        // After values
        float freqAfter = 1000.0f;
        float gainAfter = 0.0f;
        float qAfter = 1.0f;
        int typeAfter = 2;
        
        // Context
        AdjustmentContext context;
        
        // Was this a correction of an AI suggestion?
        bool correctedAISuggestion = false;
        juce::String aiSuggestionType;
    };
    
    // Learned preference for a frequency region
    struct FrequencyPreference
    {
        float centerFreq = 1000.0f;
        float avgGainAdjustment = 0.0f;
        float avgQPreference = 1.0f;
        int sampleCount = 0;
        float confidence = 0.0f;
    };
    
    // User profile containing all learned preferences
    struct UserProfile
    {
        juce::String name = "Default";
        
        // Global preferences
        float overallGainBias = 0.0f;          // Tendency to boost or cut overall
        float qWidthPreference = 1.0f;          // Preferred Q width
        float correctionStrengthPref = 0.7f;    // How strong user likes corrections
        
        // Frequency region preferences
        std::vector<FrequencyPreference> frequencyPrefs;
        
        // Genre-specific preferences
        std::map<juce::String, std::vector<FrequencyPreference>> genrePrefs;
        
        // Statistics
        int totalAdjustments = 0;
        int aiSuggestionsAccepted = 0;
        int aiSuggestionsCorrected = 0;
        int aiSuggestionsRejected = 0;

        juce::int64 lastUpdated = 0;
    };

    //==============================================================================
    UserLearningSystem();
    ~UserLearningSystem() = default;

    //==============================================================================
    // Recording adjustments
    void recordAdjustment(const UserAdjustment& adjustment);
    void recordAISuggestionAccepted(const juce::String& suggestionType, float freq, float gain, float q);
    void recordAISuggestionRejected(const juce::String& suggestionType);
    void recordAISuggestionCorrected(const juce::String& suggestionType, 
                                      float suggestedGain, float actualGain,
                                      float suggestedQ, float actualQ);
    
    //==============================================================================
    // Getting learned preferences
    float getPreferredGainForFrequency(float freq, const juce::String& genre = "") const;
    float getPreferredQForFrequency(float freq, const juce::String& genre = "") const;
    float getSuggestedCorrectionStrength() const;
    
    // Get modified AI suggestion based on learning
    void adjustAISuggestion(float& gain, float& q, float freq, const juce::String& problemType) const;
    
    //==============================================================================
    // Profile management
    void setCurrentProfile(const juce::String& name);
    juce::String getCurrentProfileName() const { return currentProfile.name; }
    const UserProfile& getCurrentProfile() const { return currentProfile; }
    
    void createNewProfile(const juce::String& name);
    void deleteProfile(const juce::String& name);
    juce::StringArray getAvailableProfiles() const;
    
    //==============================================================================
    // Persistence
    void saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);
    void saveToDefaultLocation() const;
    bool loadFromDefaultLocation();
    
    //==============================================================================
    // Settings
    void setLearningEnabled(bool enabled) { learningEnabled = enabled; }
    bool isLearningEnabled() const { return learningEnabled; }
    
    void setLearningRate(float rate) { learningRate = juce::jlimit(0.01f, 0.5f, rate); }
    float getLearningRate() const { return learningRate; }
    
    void setMinSamplesForConfidence(int samples) { minSamplesForConfidence = samples; }
    
    //==============================================================================
    // Analytics
    int getTotalAdjustments() const { return currentProfile.totalAdjustments; }
    float getAISuggestionAcceptanceRate() const;
    juce::String getMostAdjustedFrequencyRegion() const;

private:
    //==============================================================================
    void updatePreferences(const UserAdjustment& adjustment);
    void updateFrequencyPreference(std::vector<FrequencyPreference>& prefs, 
                                   float freq, float gainDelta, float q);
    int findNearestPreference(const std::vector<FrequencyPreference>& prefs, float freq) const;
    float interpolatePreference(const std::vector<FrequencyPreference>& prefs, 
                                float freq, bool forGain) const;
    
    juce::var profileToVar(const UserProfile& profile) const;
    UserProfile varToProfile(const juce::var& v) const;
    
    //==============================================================================
    UserProfile currentProfile;
    std::map<juce::String, UserProfile> savedProfiles;
    
    // Recent adjustments buffer for pattern detection
    std::deque<UserAdjustment> recentAdjustments;
    static constexpr size_t maxRecentAdjustments = 100;
    
    // Settings
    bool learningEnabled = true;
    float learningRate = 0.1f;
    int minSamplesForConfidence = 5;
    
    // Frequency regions for preference grouping
    static constexpr int numFrequencyRegions = 10;
    float getRegionCenterFreq(int region) const;
    int getRegionForFreq(float freq) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserLearningSystem)
};
