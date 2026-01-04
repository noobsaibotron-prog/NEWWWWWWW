#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <mutex>
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/ParametricEQProcessor.h"
#include "DSP/DynamicEQProcessor.h"
#include "DSP/LinearPhaseProcessor.h"
#include "AI/AIEngine.h"
#include "AI/ReferenceMatcher.h"
#include "AI/UserLearning.h"
#include "AI/SemanticEQEngine.h"
#include "Utils/Logger.h"
#include "Utils/PresetManager.h"

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
class AIEqualizerAudioProcessor : public juce::AudioProcessor,
                                  public juce::AudioProcessorValueTreeState::Listener
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

    enum class PhaseMode { ZeroLatency = 0, NaturalPhase, LinearPhase };
    
    enum class MSMode { Stereo = 0, Mid, Side, MSLinked };

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
    
    PresetManager& getPresetManager() { return *presetManager; }
    const PresetManager& getPresetManager() const { return *presetManager; }
    
    //==============================================================================
    double getSampleRate() const { return currentSampleRate; }
    
    // Parameter tree
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    PhaseMode getCurrentPhaseMode() const noexcept { return currentPhaseMode.load(); }
    MSMode getCurrentMSMode() const noexcept { return currentMSMode.load(); }
    
    //==============================================================================
    // EQ Band control
    static constexpr int maxBands = 24;
    
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
    int getNumActiveBands() const { return numActiveBands; }
    void setNumActiveBands(int n);
    
    // Apply AI corrections to EQ bands
    void applyAICorrections();  // Apply all approved corrections
    void applySingleCorrection(const AIEngine::Correction& correction);  // Apply only one specific correction
    
    //==============================================================================
    // A/B/C/D Comparison
    enum class ABState { A, B, C, D };
    
    ABState getCurrentABState() const { return currentABState; }
    void setABState(ABState state);
    void copyAtoB();
    void copyBtoA();
    void copyAtoC();
    void copyAtoD();
    void copyBtoC();
    void copyBtoD();
    void copyCtoD();
    void swapAB();
    void swapCD();
    
    //==============================================================================
    // Auto-Gain
    bool isAutoGainEnabled() const { return autoGainEnabled; }
    void setAutoGainEnabled(bool enabled) { autoGainEnabled = enabled; }
    float getAutoGainCompensation() const { return autoGainCompensation; }

    // Linear-phase support
    void triggerEQCurveUpdate();        // mark IR dirty
    void updateLinearPhaseIRIfNeeded(); // rebuild IR once when needed
    void requestIRBuild();              // signal background builder
    
    //==============================================================================
    // Source Profile
    void setSourceProfile(AIEngine::SourceProfile profile);
    AIEngine::SourceProfile getSourceProfile() const { return aiEngine.getSourceProfile(); }

    //==============================================================================
    // Audio capture for analysis
    void captureAudioSnapshotMs(int lengthMs);  // Retroactive capture (last N ms)
    bool startManualCapture();                  // Start manual capture
    void stopManualCapture();                  // Stop and finalize manual capture
    bool isCapturing() const { return isManualCapturing; }
    const std::vector<float>& getCapturedAudioMono() const { return capturedAudioMono; }
    double getCapturedSampleRate() const { return capturedSampleRate; }
    bool analyzeCapturedAudioSnapshot(); // runs AI analysis on last captured mono
    void setCaptureLengthMs(int lengthMs) { captureLengthMs = juce::jlimit(1000, captureMaxSeconds * 1000, lengthMs); }
    int getCaptureLengthMs() const { return captureLengthMs; }
    
    // Auto-capture trigger (captures on energy peak)
    void setAutoCaptureEnabled(bool enabled) { autoCaptureEnabled = enabled; }
    bool isAutoCaptureEnabled() const { return autoCaptureEnabled; }
    
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
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateEQFromParameters();
    void ensureBandCount(int count);
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
    ParametricEQProcessor eqProcessorHQ;
    DynamicEQProcessor dynamicEQProcessorHQ;
    
    // Mid/Side processing chains
    ParametricEQProcessor eqProcessorMid;
    ParametricEQProcessor eqProcessorSide;
    DynamicEQProcessor dynamicEQProcessorMid;
    DynamicEQProcessor dynamicEQProcessorSide;
    juce::AudioBuffer<float> msBuffer;  // Temporary buffer for M/S processing
    
    // M/S encoding/decoding helpers (JUCE doesn't have built-in M/S classes)
    void encodeMidSide(juce::AudioBuffer<float>& buffer);
    void decodeMidSide(juce::AudioBuffer<float>& buffer);
    
    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> naturalOversampler;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    juce::AudioBuffer<float> naturalOversampledBuffer;
    int naturalPhaseLatency = 64; // updated in prepare
    std::atomic<int> oversamplingFactor { 0 }; // 0=off, 1=2x, 2=4x
    
    // Linear phase (partitioned convolution for low latency)
    std::array<std::unique_ptr<LinearPhaseProcessor>, 2> linearPhaseProcessors;
    std::atomic<int> activeIRIndex { 0 };
    std::atomic<int> readyIRIndex { -1 };
    
    // AI components
    AIEngine aiEngine;
    ReferenceMatcher referenceMatcher;
    UserLearningSystem userLearning;
    SemanticEQEngine semanticEngine;
    
    // Utilities
    std::unique_ptr<PresetManager> presetManager;
    
    // State
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    std::atomic<PhaseMode> currentPhaseMode { PhaseMode::ZeroLatency };
    std::atomic<MSMode> currentMSMode { MSMode::Stereo };
    std::atomic<bool> eqCurveNeedsUpdate { true };
    
    // Parameter update flag
    std::atomic<bool> parametersNeedUpdate{true};
    int analyzerResolutionCached = 2; // 0=1024,1=2048,2=4096,3=8192
    int analyzerSpeedCached = 1;      // 0=Fast,1=Medium,2=Slow
    int lastReportedLatency = 0;
    
    //==============================================================================
    // A/B/C/D Comparison storage
    struct EQSlot
    {
        std::vector<BandState> bands;
        float outputGain = 0.0f;
        juce::String name;  // Optional slot name
    };
    
    EQSlot slotA, slotB, slotC, slotD;
    ABState currentABState = ABState::A;
    
    //==============================================================================
    // Auto-Gain
    bool autoGainEnabled = false;
    float autoGainCompensation = 0.0f;
    float preEQRMS = 0.0f;
    float postEQRMS = 0.0f;
    static constexpr float rmsSmoothing = 0.95f;

    // Quality mode cache (0=Zero Latency, 1=High Quality)
    int qualityModeCached = 0;
    
    //==============================================================================
    // Undo/Redo History for AI Corrections
    struct EQSnapshot
    {
        std::vector<BandState> bands;
        juce::String description;
        juce::int64 timestamp;
    };
    
    std::vector<EQSnapshot> undoHistory;
    std::vector<EQSnapshot> redoHistory;
    static constexpr int maxHistorySize = 20;
    
    void pushUndoState(const juce::String& description);
    std::vector<juce::String> eqParameterIDs;

    // IR build (background)
    std::atomic<bool> irBuilderShouldExit { false };
    juce::WaitableEvent irBuildEvent;
    std::thread irBuilderThread;

    //==============================================================================
    // Capture buffer (ring) for on-demand analysis
    void pushToCaptureRing(const juce::AudioBuffer<float>& buffer);
    juce::AudioBuffer<float> captureRing;
    int captureRingWritePos = 0;
    int captureRingLength = 0; // samples per channel
    static constexpr int captureMaxSeconds = 20;
    int captureLengthMs = captureMaxSeconds * 1000;
    std::vector<float> capturedAudioMono;
    double capturedSampleRate = 44100.0;
    // CRITICAL FIX: Replace SpinLock with std::mutex for better thread-safety
    // SpinLock can cause priority inversion and doesn't work well with multiple plugin instances
    mutable std::mutex captureLock;
    
    // Manual capture (START/STOP) - THREAD-SAFE
    std::atomic<bool> isManualCapturing { false };
    std::vector<float> manualCaptureBuffer;
    std::atomic<int> manualCaptureSamples { 0 };
    
    // Auto-capture trigger
    bool autoCaptureEnabled = false;
    float lastEnergyLevel = 0.0f;
    float energyThreshold = 0.3f;  // Threshold for auto-capture trigger
    int autoCaptureCooldown = 0;    // Frames to wait before next auto-capture

    // Active bands
    int numActiveBands = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessor)
};
