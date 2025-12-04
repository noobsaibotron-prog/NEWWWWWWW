#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/ParametricEQProcessor.h"
#include "DSP/DynamicEQProcessor.h"
#include "AI/AIEngine.h"
#include "AI/ReferenceMatcher.h"
#include "AI/UserLearning.h"
#include "AI/SemanticEQEngine.h"

//==============================================================================
/**
 * AI Equalizer Audio Processor V2
 * 
 * Features:
 * - 8-band parametric EQ
 * - Real-time spectrum analysis (Pre/Post EQ)
 * - AI-powered problem detection with Source Profiles
 * - Automatic correction with approval workflow
 * - A/B Comparison
 * - Auto-Gain compensation
 * - Reference track matching
 * - User preference learning
 */
class AIEqualizerAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AIEqualizerAudioProcessor();
    ~AIEqualizerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Component access
    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }
    const SpectrumAnalyzer& getSpectrumAnalyzer() const { return spectrumAnalyzer; }
    
    // Post-EQ spectrum (for overlay display)
    SpectrumAnalyzer& getPostEQAnalyzer() { return postEQAnalyzer; }
    const SpectrumAnalyzer& getPostEQAnalyzer() const { return postEQAnalyzer; }
    
    ParametricEQProcessor& getEQProcessor() { return eqProcessor; }
    const ParametricEQProcessor& getEQProcessor() const { return eqProcessor; }
    
    DynamicEQProcessor& getDynamicEQProcessor() { return dynamicEQProcessor; }
    const DynamicEQProcessor& getDynamicEQProcessor() const { return dynamicEQProcessor; }
    
    AIEngine& getAIEngine() { return aiEngine; }
    const AIEngine& getAIEngine() const { return aiEngine; }
    
    ReferenceMatcher& getReferenceMatcher() { return referenceMatcher; }
    const ReferenceMatcher& getReferenceMatcher() const { return referenceMatcher; }
    
    UserLearningSystem& getUserLearning() { return userLearning; }
    const UserLearningSystem& getUserLearning() const { return userLearning; }
    
    SemanticEQEngine& getSemanticEngine() { return semanticEngine; }
    const SemanticEQEngine& getSemanticEngine() const { return semanticEngine; }
    
    //==============================================================================
    double getSampleRate() const { return currentSampleRate; }
    
    // Parameter tree
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    //==============================================================================
    // EQ Band control
    static constexpr int numBands = 8;
    
    struct BandState
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int type = 2; // Peak
        bool enabled = true;
    };
    
    BandState getBandState(int bandIndex) const;
    void setBandState(int bandIndex, const BandState& state);
    
    // Apply AI corrections to EQ bands
    void applyAICorrections();
    
    //==============================================================================
    // A/B Comparison
    enum class ABState { A, B };
    
    ABState getCurrentABState() const { return currentABState; }
    void setABState(ABState state);
    void copyAtoB();
    void copyBtoA();
    void swapAB();
    
    //==============================================================================
    // Auto-Gain
    bool isAutoGainEnabled() const { return autoGainEnabled; }
    void setAutoGainEnabled(bool enabled) { autoGainEnabled = enabled; }
    float getAutoGainCompensation() const { return autoGainCompensation; }
    
    //==============================================================================
    // Source Profile
    void setSourceProfile(AIEngine::SourceProfile profile);
    AIEngine::SourceProfile getSourceProfile() const { return aiEngine.getSourceProfile(); }
    
    //==============================================================================
    // Undo/Redo for AI Corrections
    bool canUndo() const { return !undoHistory.empty(); }
    bool canRedo() const { return !redoHistory.empty(); }
    void undo();
    void redo();
    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;
    int getUndoStackSize() const { return static_cast<int>(undoHistory.size()); }
    int getRedoStackSize() const { return static_cast<int>(redoHistory.size()); }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void updateEQFromParameters();
    void calculateAutoGain();
    void saveCurrentStateToSlot(ABState slot);
    void loadStateFromSlot(ABState slot);
    
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    
    // DSP components
    SpectrumAnalyzer spectrumAnalyzer;      // Pre-EQ spectrum
    SpectrumAnalyzer postEQAnalyzer;        // Post-EQ spectrum
    ParametricEQProcessor eqProcessor;
    DynamicEQProcessor dynamicEQProcessor;  // Dynamic EQ (FabFilter-style)
    
    // AI components
    AIEngine aiEngine;
    ReferenceMatcher referenceMatcher;
    UserLearningSystem userLearning;
    SemanticEQEngine semanticEngine;
    
    // State
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    // Parameter update flag
    std::atomic<bool> parametersNeedUpdate{true};
    
    //==============================================================================
    // A/B Comparison storage
    struct EQSlot
    {
        std::array<BandState, numBands> bands;
        float outputGain = 0.0f;
    };
    
    EQSlot slotA, slotB;
    ABState currentABState = ABState::A;
    
    //==============================================================================
    // Auto-Gain
    bool autoGainEnabled = false;
    float autoGainCompensation = 0.0f;
    float preEQRMS = 0.0f;
    float postEQRMS = 0.0f;
    static constexpr float rmsSmoothing = 0.95f;
    
    //==============================================================================
    // Undo/Redo History for AI Corrections
    struct EQSnapshot
    {
        std::array<BandState, numBands> bands;
        juce::String description;
        juce::int64 timestamp;
    };
    
    std::vector<EQSnapshot> undoHistory;
    std::vector<EQSnapshot> redoHistory;
    static constexpr int maxHistorySize = 20;
    
    void pushUndoState(const juce::String& description);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessor)
};
