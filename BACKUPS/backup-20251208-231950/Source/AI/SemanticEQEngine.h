#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <map>
#include <array>
#include <memory>
#include <unordered_map>
#include <string>

//==============================================================================
/**
 * Semantic EQ Engine - Natural Language to EQ Parameters
 * 
 * This is the core innovation: users express desires in human terms
 * ("I want more Air", "Make it warmer") and the AI translates this
 * into precise EQ adjustments.
 * 
 * Key Features:
 * - Semantic descriptor vocabulary (Air, Warmth, Punch, Body, Clarity, etc.)
 * - Context-aware mapping (what "Air" means depends on current spectrum)
 * - User preference learning integration
 * - Smooth morphing between semantic states
 * - Multi-language support (English, Italian, etc.)
 * 
 * Architecture:
 * - SemanticDescriptor: Defines a timbral quality with EQ mappings
 * - SemanticLayer: Neural network layer for learned mappings
 * - User adaptation via UserLearningSystem feedback
 */
class SemanticEQEngine
{
public:
    //==========================================================================
    // Semantic descriptors - The vocabulary of sound qualities
    //==========================================================================
    enum class SemanticQuality
    {
        // Brightness / High Frequency
        Air,            // 8-16 kHz presence, airy, open
        Brilliance,     // 6-12 kHz sparkle, shimmer
        Presence,       // 3-6 kHz forward, cutting through
        Sizzle,         // 8-12 kHz crispy highs
        
        // Warmth / Low-Mid
        Warmth,         // 100-300 Hz fullness, analog feel
        Body,           // 200-500 Hz weight, substance
        Thickness,      // 150-400 Hz dense, heavy
        Richness,       // 200-800 Hz complex harmonics
        
        // Attack / Definition
        Punch,          // 2-5 kHz attack, impact
        Bite,           // 1-3 kHz aggression, edge
        Snap,           // 3-6 kHz transient definition
        Attack,         // Broad transient enhancement
        
        // Smoothness / Reduction
        Smoothness,     // Reduce 2-4 kHz harshness
        Softness,       // Roll off high frequencies
        Darkness,       // Reduce highs, boost lows
        Gentleness,     // Smooth everything
        
        // Bass / Sub
        Weight,         // 40-100 Hz sub presence
        Foundation,     // 60-150 Hz bass foundation
        Rumble,         // 20-60 Hz sub-bass
        Tightness,      // Reduce 100-200 Hz boom
        
        // Mid Character
        Clarity,        // Enhance mids, reduce mud
        Definition,     // 1-4 kHz articulation
        Focus,          // Narrow Q boosts
        Width,          // Stereo and frequency spread
        
        // Special
        Vintage,        // Analog-style EQ curve
        Modern,         // Clean, hi-fi curve
        Aggressive,     // Strong presence + punch
        Smooth,         // Gentle, rolled off
        
        NumQualities
    };
    
    static constexpr int numQualities = static_cast<int>(SemanticQuality::NumQualities);
    
    //==========================================================================
    // EQ adjustment generated from semantic input
    //==========================================================================
    struct SemanticEQAdjustment
    {
        float frequency = 1000.0f;    // Hz
        float gain = 0.0f;            // dB
        float q = 1.0f;               // Q factor
        int filterType = 2;           // 0=LowCut, 1=LowShelf, 2=Peak, 3=HighShelf, 4=HighCut, 5=Notch, 6=BandPass
        bool enabled = true;
        
        // Metadata
        SemanticQuality sourceQuality;
        float confidence = 1.0f;
        juce::String description;
    };
    
    //==========================================================================
    // Semantic state - represents current "timbral goal"
    //==========================================================================
    struct SemanticState
    {
        // Quality amounts (-1 to +1, where 0 = neutral)
        std::array<float, numQualities> qualities{};
        
        // Convenience accessors
        float getAir() const { return qualities[static_cast<int>(SemanticQuality::Air)]; }
        float getWarmth() const { return qualities[static_cast<int>(SemanticQuality::Warmth)]; }
        float getPunch() const { return qualities[static_cast<int>(SemanticQuality::Punch)]; }
        float getClarity() const { return qualities[static_cast<int>(SemanticQuality::Clarity)]; }
        
        void setQuality(SemanticQuality q, float amount) {
            qualities[static_cast<int>(q)] = juce::jlimit(-1.0f, 1.0f, amount);
        }
        
        float getQuality(SemanticQuality q) const {
            return qualities[static_cast<int>(q)];
        }
        
        // Reset all to neutral
        void reset() { qualities.fill(0.0f); }
    };
    
    //==========================================================================
    // Internal descriptor definition
    //==========================================================================
    struct QualityDefinition
    {
        SemanticQuality quality;
        juce::String name;              // English name
        juce::String nameIT;            // Italian name
        juce::String description;       // What it does
        
        // Primary frequency bands affected
        struct FrequencyBand {
            float centerFreq;
            float q;
            float maxGain;              // Max boost/cut at full amount
            int filterType;
        };
        std::vector<FrequencyBand> bands;
        
        // Complementary adjustments (e.g., Air also slightly reduces mud)
        struct ComplementaryAdjustment {
            SemanticQuality quality;
            float factor;               // How much to apply (0-1)
        };
        std::vector<ComplementaryAdjustment> complementary;
        
        // Opposite quality (for inverse relationships)
        SemanticQuality opposite = SemanticQuality::NumQualities;
        
        // Context modifiers
        bool affectedByBass = false;    // Adjust based on bass content
        bool affectedByBrightness = false;
    };

    //==========================================================================
    // Constructor / Destructor
    //==========================================================================
    SemanticEQEngine();
    ~SemanticEQEngine();
    
    void initialize();
    void reset();
    
    //==========================================================================
    // Main API - Convert semantic desires to EQ changes
    //==========================================================================
    
    /**
     * Generate EQ adjustments from current semantic state
     * @param spectrum Current audio spectrum (for context-aware mapping)
     * @param sampleRate Current sample rate
     * @return Vector of EQ adjustments to apply
     */
    std::vector<SemanticEQAdjustment> generateEQFromState(
        const std::vector<float>& spectrum,
        double sampleRate) const;
    
    /**
     * Generate EQ for a single quality change
     * @param quality The semantic quality to adjust
     * @param amount Amount of adjustment (-1 to +1)
     * @param spectrum Current spectrum for context
     * @param sampleRate Current sample rate
     */
    std::vector<SemanticEQAdjustment> generateEQForQuality(
        SemanticQuality quality,
        float amount,
        const std::vector<float>& spectrum,
        double sampleRate) const;
    
    /**
     * Parse natural language input to semantic adjustments
     * @param input User text like "more air" or "warmer sound"
     * @return Parsed semantic adjustments
     */
    std::vector<std::pair<SemanticQuality, float>> parseNaturalLanguage(
        const juce::String& input) const;
    
    //==========================================================================
    // State management
    //==========================================================================
    void setSemanticState(const SemanticState& state) { currentState = state; }
    const SemanticState& getSemanticState() const { return currentState; }
    
    void setQuality(SemanticQuality quality, float amount);
    float getQuality(SemanticQuality quality) const;
    
    void resetState() { currentState.reset(); }
    
    //==========================================================================
    // Morphing and transitions
    //==========================================================================
    void morphToState(const SemanticState& target, float time_ms);
    void updateMorph(float deltaTime_ms);
    bool isMorphing() const { return morphProgress < 1.0f; }
    
    //==========================================================================
    // User learning integration
    //==========================================================================
    
    /**
     * Called when user manually adjusts EQ after semantic change
     * This teaches the system what the user actually wanted
     */
    void learnFromUserAdjustment(SemanticQuality quality, float originalAmount,
                                  float userFreq, float userGain, float userQ);
    
    /**
     * Get learned offset for a quality (personalization)
     */
    struct LearnedOffset {
        float frequencyMult = 1.0f;   // Multiply center freq by this
        float gainMult = 1.0f;        // Multiply gain by this
        float qMult = 1.0f;           // Multiply Q by this
        int sampleCount = 0;
    };
    
    LearnedOffset getLearnedOffset(SemanticQuality quality) const;
    void setLearnedOffset(SemanticQuality quality, const LearnedOffset& offset);
    
    //==========================================================================
    // Configuration
    //==========================================================================
    void setIntensity(float intensity) { globalIntensity = juce::jlimit(0.0f, 2.0f, intensity); }
    float getIntensity() const { return globalIntensity; }
    
    void setContextAware(bool enabled) { contextAwareEnabled = enabled; }
    bool isContextAware() const { return contextAwareEnabled; }
    
    void setLanguage(const juce::String& lang) { currentLanguage = lang; }
    juce::String getLanguage() const { return currentLanguage; }
    
    //==========================================================================
    // Utility
    //==========================================================================
    static juce::String getQualityName(SemanticQuality quality);
    static juce::String getQualityNameLocalized(SemanticQuality quality, const juce::String& language);
    static juce::String getQualityDescription(SemanticQuality quality);
    static juce::StringArray getAllQualityNames();
    
    const QualityDefinition& getQualityDefinition(SemanticQuality quality) const;

private:
    //==========================================================================
    // Initialize quality definitions
    void initializeQualityDefinitions();
    
    // Context analysis
    float analyzeBassProportion(const std::vector<float>& spectrum, double sampleRate) const;
    float analyzeBrightness(const std::vector<float>& spectrum, double sampleRate) const;
    float analyzeMidContent(const std::vector<float>& spectrum, double sampleRate) const;
    
    // Adjust mapping based on spectrum context
    SemanticEQAdjustment adjustForContext(const SemanticEQAdjustment& base,
                                          const std::vector<float>& spectrum,
                                          double sampleRate) const;
    
    // Apply learned offsets
    SemanticEQAdjustment applyLearnedOffsets(const SemanticEQAdjustment& base,
                                             SemanticQuality quality) const;
    
    //==========================================================================
    // Quality definitions
    std::array<QualityDefinition, numQualities> qualityDefinitions;
    
    // Current semantic state
    SemanticState currentState;
    
    // Morphing
    SemanticState morphStartState;
    SemanticState morphTargetState;
    float morphProgress = 1.0f;
    float morphDuration = 0.0f;
    
    // Learned offsets from user behavior
    std::array<LearnedOffset, numQualities> learnedOffsets;
    
    // Settings
    float globalIntensity = 1.0f;
    bool contextAwareEnabled = true;
    juce::String currentLanguage = "en";
    
    // Natural language parsing
    struct LanguageMapping {
        juce::StringArray keywords;
        SemanticQuality quality;
        float defaultAmount;
        bool isNegative;  // "less X" vs "more X"
    };
    std::map<juce::String, std::vector<LanguageMapping>> languageMappings;
    
    void initializeLanguageMappings();
    void loadEmbeddings();
    
    float cosineSimilarity(const std::vector<float>& a,
                           const std::vector<float>& b) const;
    void addOrUpdateParsedResult(
        std::vector<std::pair<SemanticQuality, float>>& results,
        SemanticQuality quality,
        float amount) const;
    std::vector<float> runSeq2Seq(
        const std::vector<std::vector<float>>& tokenEmbeddings) const;
    
    struct Seq2SeqDecoder;
    std::unique_ptr<Seq2SeqDecoder> seq2seqDecoder;
    
    bool isInitialized = false;
    
    // Word embeddings (GloVe)
    static constexpr int embeddingDimension = 50;
    std::unordered_map<std::string, std::vector<float>> embeddings;
    bool embeddingsLoaded = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SemanticEQEngine)
};

