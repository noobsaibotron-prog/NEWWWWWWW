#pragma once

/**
 * AI Equalizer Audio Processor V2.1 - Production-Grade Refactored Architecture
 * 
 * ARCHITECTURAL CHANGES (Commercial Plugin Standards):
 * =====================================================
 * 
 * 1. REAL-TIME SAFETY (Wait-Free / Lock-Free):
 *    - Eliminated all std::mutex from audio path
 *    - Replaced captureLock with lock-free ring buffer (juce::AbstractFifo)
 *    - Replaced historyMutex with message-thread-only HistoryManager
 *    - Zero heap allocations after prepareToPlay()
 * 
 * 2. DECOUPLED ARCHITECTURE:
 *    - Separated capture logic into CaptureService
 *    - Separated undo/redo into HistoryManager
 *    - Added AICommandQueue for lock-free AI→Audio communication
 *    - Parameter snapshot per-block to eliminate atomic overhead in loops
 * 
 * 3. THREAD-SAFE STATE ACCESS:
 *    - AtomicBandState with version counter for consistent reads
 *    - ProcessBlockParameters snapshot loaded once per block
 *    - All GUI-accessed state uses relaxed atomics where appropriate
 * 
 * 4. MODERN C++20:
 *    - std::thread with stop flags for safe thread lifecycle (RAII)
 *    - [[nodiscard]] on all query methods
 *    - std::span for buffer passing where beneficial
 * 
 * 5. PERFORMANCE OPTIMIZATIONS:
 *    - Parameter values cached in local struct at block start
 *    - SIMD-aligned buffers (64-byte alignment for AVX-512)
 *    - Denormal protection via juce::ScopedNoDenormals
 *    - Reduced DSP instance bloat with active-only processing
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <thread>  // std::thread and std::stop flags are in <thread> in C++20

#include "Core/LockFreeStructures.h"
#include "Core/CaptureService.h"
#include "Core/HistoryManager.h"
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/ParametricEQProcessor.h"
#include "DSP/DynamicEQProcessor.h"
#include "DSP/LinearPhaseProcessor.h"
#include "Core/OSCParameterServer.h"
#include "AI/AIEngine.h"
#include "AI/ReferenceMatcher.h"
#include "AI/UserLearning.h"
#include "AI/SemanticEQEngine.h"
#include "Utils/Logger.h"
#include "Utils/PresetManager.h"

// Smoothed parameter helper for band-level automation (block-level smoothing)
#include <juce_dsp/juce_dsp.h>
//==============================================================================
/**
 * AI Equalizer Audio Processor V2.1
 * 
 * Features:
 * - 8-24 band parametric EQ with interactive spectrum
 * - Real-time spectrum analysis (Pre/Post EQ)
 * - AI-powered problem detection with Source Profiles
 * - Dynamic EQ (FabFilter Pro-Q style): Compress/Expand/Gate per band
 * - Linear Phase mode with zero-artifact IR crossfade
 * - A/B/C/D Comparison with instant switching
 * - Auto-Gain compensation (RMS-based)
 * - Semantic Control: natural language → EQ adjustments
 * - Reference track matching
 * - User preference learning
 * 
 * Thread Model:
 * - Audio Thread: processBlock, real-time safe operations only
 * - Message Thread: GUI, parameter changes, undo/redo
 * - IR Builder Thread: Linear phase IR generation (background)
 * - AI Analysis: Runs on message thread timer, results via command queue
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

    //==============================================================================
    // Processing Mode Enums
    //==============================================================================
    enum class PhaseMode { ZeroLatency = 0, NaturalPhase, LinearPhase };
    enum class MSMode { Stereo = 0, Mid, Side, MSLinked };

    //==============================================================================
    // Component Access (all [[nodiscard]] for API safety)
    //==============================================================================
    [[nodiscard]] SpectrumAnalyzer& getSpectrumAnalyzer() noexcept { return spectrumAnalyzer; }
    [[nodiscard]] const SpectrumAnalyzer& getSpectrumAnalyzer() const noexcept { return spectrumAnalyzer; }
    
    [[nodiscard]] SpectrumAnalyzer& getPostEQAnalyzer() noexcept { return postEQAnalyzer; }
    [[nodiscard]] const SpectrumAnalyzer& getPostEQAnalyzer() const noexcept { return postEQAnalyzer; }
    
    [[nodiscard]] ParametricEQProcessor& getEQProcessor() noexcept { return eqProcessor; }
    [[nodiscard]] const ParametricEQProcessor& getEQProcessor() const noexcept { return eqProcessor; }
    
    [[nodiscard]] DynamicEQProcessor& getDynamicEQProcessor() noexcept { return dynamicEQProcessor; }
    [[nodiscard]] const DynamicEQProcessor& getDynamicEQProcessor() const noexcept { return dynamicEQProcessor; }

    // Dynamic EQ metering (GUI-safe, independent of processing path)
    [[nodiscard]] DynamicEQProcessor::BandMeter getDynamicBandMeter(int bandIndex) const noexcept;
    [[nodiscard]] float getDynamicTotalGainReduction() const noexcept;
    
    [[nodiscard]] AIEngine& getAIEngine() noexcept { return aiEngine; }
    [[nodiscard]] const AIEngine& getAIEngine() const noexcept { return aiEngine; }
    
    [[nodiscard]] ReferenceMatcher& getReferenceMatcher() noexcept { return referenceMatcher; }
    [[nodiscard]] const ReferenceMatcher& getReferenceMatcher() const noexcept { return referenceMatcher; }
    
    [[nodiscard]] UserLearningSystem& getUserLearning() noexcept { return userLearning; }
    [[nodiscard]] const UserLearningSystem& getUserLearning() const noexcept { return userLearning; }
    
    [[nodiscard]] SemanticEQEngine& getSemanticEngine() noexcept { return semanticEngine; }
    [[nodiscard]] const SemanticEQEngine& getSemanticEngine() const noexcept { return semanticEngine; }
    
    [[nodiscard]] PresetManager& getPresetManager() { 
        jassert(presetManager != nullptr); 
        return *presetManager; 
    }
    [[nodiscard]] const PresetManager& getPresetManager() const { 
        jassert(presetManager != nullptr); 
        return *presetManager; 
    }
    [[nodiscard]] bool hasPresetManager() const noexcept { return presetManager != nullptr; }
    
    //==============================================================================
    // Sample Rate and State Accessors
    //==============================================================================
    [[nodiscard]] double getSampleRate() const noexcept { return currentSampleRate.load(std::memory_order_relaxed); }
    
    // Safety check for GUI access - returns true only after prepareToPlay has completed
    [[nodiscard]] bool isProcessorReady() const noexcept { return processorReady.load(std::memory_order_acquire); }
    
    // Dirty flags for GUI updates (lock-free)
    [[nodiscard]] bool consumeSpectrumDataReady() noexcept { return spectrumDataReady.exchange(false, std::memory_order_acquire); }
    [[nodiscard]] bool consumeAIProblemsChanged() noexcept { return aiProblemsChanged.exchange(false, std::memory_order_acquire); }
    [[nodiscard]] uint64_t getParameterChangeCounter() const noexcept { return parameterChangeCounter.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t getBlockClampEvents() const noexcept { return blockClampEvents.load(std::memory_order_relaxed); }

    // Output peak metering (GUI reads, audio thread writes)
    [[nodiscard]] float getOutputPeakLeft() const noexcept { return outputPeakLeft.load(std::memory_order_relaxed); }
    [[nodiscard]] float getOutputPeakRight() const noexcept { return outputPeakRight.load(std::memory_order_relaxed); }

    // Parameter tree access
    [[nodiscard]] juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    [[nodiscard]] PhaseMode getCurrentPhaseMode() const noexcept { return currentPhaseMode.load(std::memory_order_relaxed); }
    [[nodiscard]] MSMode getCurrentMSMode() const noexcept { return currentMSMode.load(std::memory_order_relaxed); }
    
    //==============================================================================
    // EQ Band Control
    //==============================================================================
    static constexpr int maxBands = 24;
    
    /**
     * BandState - Immutable snapshot of a single EQ band
     * Used for GUI display and state serialization.
     * NOTE: Reading this from audio thread uses AtomicBandState for consistency.
     */
    struct BandState
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int type = 2; // Peak
        bool enabled = true;
        bool solo = false;
    };
    
    /**
     * Get band state (thread-safe, uses version-counted atomics)
     * Safe to call from any thread.
     */
    [[nodiscard]] BandState getBandState(int bandIndex) const;
    
    /**
     * Set band state (message thread only)
     * Changes are applied through APVTS for host automation support.
     */
    void setBandState(int bandIndex, const BandState& state);
    
    [[nodiscard]] int getNumActiveBands() const noexcept { return numActiveBands.load(std::memory_order_relaxed); }
    void setNumActiveBands(int n) noexcept;
    void markParametersChanged() noexcept { parameterChangeCounter.fetch_add(1, std::memory_order_relaxed); }

    // Semantic EQ application (message thread only)
    void applySemanticAdjustments(const std::vector<SemanticEQEngine::SemanticEQAdjustment>& adjustments);
    
    //==============================================================================
    // AI Corrections (message thread only)
    //==============================================================================
    
    /**
     * Apply all approved AI corrections
     * Uses HistoryManager for undo support.
     */
    void applyAICorrections();
    
    /**
     * Apply a single specific correction
     */
    void applySingleCorrection(const AIEngine::Correction& correction);
    
    /**
     * Queue a correction command for the audio thread (lock-free)
     * Used for real-time AI adjustments without blocking.
     */
    void queueAICommand(const AIEQCore::AICommand& command) noexcept;
    
    //==============================================================================
    // A/B/C/D Comparison
    //==============================================================================
    enum class ABState { A, B, C, D };
    
    [[nodiscard]] ABState getCurrentABState() const noexcept { return currentABState.load(std::memory_order_relaxed); }
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
    // Auto-Gain (thread-safe accessors)
    //==============================================================================
    [[nodiscard]] bool isAutoGainEnabled() const noexcept { return autoGainEnabled.load(std::memory_order_relaxed); }
    void setAutoGainEnabled(bool enabled) noexcept { autoGainEnabled.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] float getAutoGainCompensation() const noexcept { return autoGainCompensation.load(std::memory_order_relaxed); }

    //==============================================================================
    // Linear Phase Support
    //==============================================================================
    void triggerEQCurveUpdate();        // Mark IR dirty
    void updateLinearPhaseIRIfNeeded(); // Swap in pre-built IR if ready
    void requestIRBuild();              // Signal background IR builder
    
    //==============================================================================
    // Source Profile
    //==============================================================================
    void setSourceProfile(AIEngine::SourceProfile profile);
    [[nodiscard]] AIEngine::SourceProfile getSourceProfile() const { return aiEngine.getSourceProfile(); }

    //==============================================================================
    // Audio Capture (using lock-free CaptureService)
    //==============================================================================
    void captureAudioSnapshotMs(int lengthMs);
    [[nodiscard]] bool startManualCapture();
    void stopManualCapture();
    [[nodiscard]] bool isCapturing() const noexcept { return captureService.isCapturing(); }
    [[nodiscard]] const std::vector<float>& getCapturedAudioMono() const { return captureService.getCapturedAudioMono(); }
    [[nodiscard]] bool isCaptureBufferSafeToRead() const noexcept { return captureBufferReady.load(std::memory_order_acquire); }
    void getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples = 1024) const;
    [[nodiscard]] double getCapturedSampleRate() const noexcept { return captureService.getCapturedSampleRate(); }
    [[nodiscard]] bool analyzeCapturedAudioSnapshot();
    void setCaptureLengthMs(int lengthMs) noexcept { captureService.setCaptureLengthMs(lengthMs); }
    [[nodiscard]] int getCaptureLengthMs() const noexcept { return captureService.getCaptureLengthMs(); }
    
    void setAutoCaptureEnabled(bool enabled) noexcept { captureService.setAutoCaptureEnabled(enabled); }
    [[nodiscard]] bool isAutoCaptureEnabled() const noexcept { return captureService.isAutoCaptureEnabled(); }
    
    //==============================================================================
    // Undo/Redo (message thread only, using HistoryManager)
    //==============================================================================
    [[nodiscard]] bool canUndo() const { return historyManager.canUndo(); }
    [[nodiscard]] bool canRedo() const { return historyManager.canRedo(); }
    void undo() { historyManager.undo(); }
    void redo() { historyManager.redo(); }
    [[nodiscard]] juce::String getUndoDescription() const { return historyManager.getUndoDescription(); }
    [[nodiscard]] juce::String getRedoDescription() const { return historyManager.getRedoDescription(); }
    [[nodiscard]] int getUndoStackSize() const { return historyManager.getUndoStackSize(); }
    [[nodiscard]] int getRedoStackSize() const { return historyManager.getRedoStackSize(); }
    
    /**
     * Push current state to undo stack (call before making changes)
     */
    void pushUndoState(const juce::String& description) { historyManager.pushUndoState(description); }

private:
    //==============================================================================
    // Private Methods
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateEQFromParameters();
    void ensureBandCount(int count);
    void calculateAutoGain();
    void saveCurrentStateToSlot(ABState slot);
    void loadStateFromSlot(ABState slot);
    void updateReportedLatency();
    void cacheParameterPointers();
    bool runCapturedAudioAnalysis();
    void aiAnalysisThreadFunc();
    void enqueueAISpectrum(const std::vector<float>& spectrum);
    void clearDynamicMeterCache() noexcept;
    void updateDynamicMeterCacheFrom(const DynamicEQProcessor& src) noexcept;
    void updateDynamicMeterCacheFromMS(const DynamicEQProcessor& mid,
                                       const DynamicEQProcessor& side,
                                       bool includeMid,
                                       bool includeSide) noexcept;
    void primeBandSmoothers(double sampleRate);
    void applySmoothedBandParams(int blockSamples);
    
    // M/S encoding/decoding helpers
    void encodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples);
    void decodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples);
    
    /**
     * Load all parameters into ProcessBlockParameters snapshot
     * Called once at the start of each processBlock for optimal performance.
     */
    void loadParameterSnapshot(AIEQCore::ProcessBlockParameters& params) noexcept;
    
    /**
     * Process any pending AI commands from the command queue (RT-safe)
     */
    void processAICommands() noexcept;
    
    //==============================================================================
    // Core Services (lock-free, thread-safe)
    //==============================================================================
    AIEQCore::CaptureService captureService;       // Lock-free audio capture
    AIEQCore::HistoryManager historyManager;       // Thread-safe undo/redo
    AIEQCore::AICommandQueue aiCommandQueue;       // Lock-free AI→Audio commands
    std::atomic<bool> captureBufferReady { false }; // FIX 3: guard GUI access to capture buffer
    std::atomic<bool> captureAnalysisInFlight { false };
    std::atomic<bool> captureAnalysisCompleted { false };
    std::atomic<bool> captureAnalysisResult { false };
    std::thread captureAnalysisThread;
    
    //==============================================================================
    // APVTS (thread-safe parameter management)
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    
    //==============================================================================
    // DSP Components
    //==============================================================================
    SpectrumAnalyzer spectrumAnalyzer;      // Pre-EQ spectrum
    SpectrumAnalyzer postEQAnalyzer;        // Post-EQ spectrum
    
    // Main EQ processors (Zero-Latency path)
    ParametricEQProcessor eqProcessor;
    DynamicEQProcessor dynamicEQProcessor;
    
    // High-Quality processors (Natural Phase path with oversampling)
    ParametricEQProcessor eqProcessorHQ;
    DynamicEQProcessor dynamicEQProcessorHQ;

    // Dynamic EQ metering cache (aggregated across processing paths)
    struct DynamicMeterCacheEntry
    {
        std::atomic<float> input { -100.0f };
        std::atomic<float> gainReduction { 0.0f };
        std::atomic<float> output { -100.0f };
    };
    std::array<DynamicMeterCacheEntry, maxBands> dynamicMeterCache {};
    std::atomic<float> dynamicTotalGR { 0.0f };

    // Smoothed band targets (frequency/gain) for anti-zippering automation
    std::array<juce::SmoothedValue<float>, maxBands> smoothedBandFreq {};
    std::array<juce::SmoothedValue<float>, maxBands> smoothedBandGain {};
    std::array<float, maxBands> targetBandFreq {};
    std::array<float, maxBands> targetBandGain {};
    std::array<float, maxBands> targetBandQ {};
    std::array<int, maxBands> targetBandType {};
    std::array<bool, maxBands> targetBandEnabled {};
    std::array<bool, maxBands> targetBandSolo {};
    bool bandSmoothingPrimed { false };

    // Linear-phase delay compensation when IR is not ready
    juce::AudioBuffer<float> linearPhaseDelayBuffer;
    int linearPhaseDelayWritePos = 0;
    
    // Shadow processors for thread-safe IR building
    // Updated atomically when coefficients change, read by IR builder thread
    ParametricEQProcessor eqProcessorForIR;
    DynamicEQProcessor dynamicEQProcessorForIR;
    std::atomic<bool> irCoefficientsUpdated { false };
    
    // Mid/Side processing chains
    ParametricEQProcessor eqProcessorMid;
    ParametricEQProcessor eqProcessorSide;
    DynamicEQProcessor dynamicEQProcessorMid;
    DynamicEQProcessor dynamicEQProcessorSide;
    
    // Pre-allocated M/S buffers (allocated in prepareToPlay, never resized)
    alignas(64) juce::AudioBuffer<float> msBuffer;
    alignas(64) juce::AudioBuffer<float> midProcessBuffer;
    alignas(64) juce::AudioBuffer<float> sideProcessBuffer;
    alignas(64) juce::AudioBuffer<float> dryBuffer;
    
    // Solo acoustic monitor (band-pass audition)
    juce::IIRFilter soloMonitorFilterL;
    juce::IIRFilter soloMonitorFilterR;
    juce::AudioBuffer<float> soloOutputBuffer;  // Pre-allocated in prepareToPlay
    juce::AudioBuffer<float> soloWarmupBuffer;  // Pre-allocated in prepareToPlay
    juce::AudioBuffer<float> preProcessingInputCopy;
    float lastSoloFreq  = -1.0f;   // cached to skip setCoefficients when unchanged
    float lastSoloQ     = -1.0f;
    bool  wasSoloed     = false;   // track transition to crossfade on enable/disable
    int   soloCrossfadeRemaining = 0;
    static constexpr int   soloCrossfadeSamples = 256;  // ~5ms @ 48kHz
    static constexpr float soloMakeupGainDB = 6.0f;

    // IR build debounce: accumulate rapid drag events, rebuild only after silence
    std::atomic<int64_t> irBuildRequestedAt { 0 };    // ms timestamp of last request
    static constexpr int64_t irBuildDebounceMs = 80;  // rebuild after 80ms of no changes
    
    //==============================================================================
    // Oversampling
    //==============================================================================
    std::unique_ptr<juce::dsp::Oversampling<float>> naturalOversampler;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    alignas(64) juce::AudioBuffer<float> naturalOversampledBuffer;
    int naturalPhaseLatency = 64;
    std::atomic<int> oversamplingFactor { 0 };          // user choice
    std::atomic<int> oversamplingEffectiveFactor { 0 }; // resolved (auto/off/2x/4x)
    
    //==============================================================================
    // Linear Phase (partitioned convolution)
    //==============================================================================
    std::array<std::unique_ptr<LinearPhaseProcessor>, 2> linearPhaseProcessors;
    std::atomic<int> activeIRIndex { 0 };
    std::atomic<int> readyIRIndex { -1 };
    std::array<std::atomic<bool>, 2> linearIRLoaded { false, false };
    int consecutiveIRReadyBlocks = 0;
    
    // IR crossfade for click-free transitions
    static constexpr int irCrossfadeSamples = 128;
    int crossfadeSamplesRemaining = 0;
    int previousIRIndex = 0;
    alignas(64) juce::AudioBuffer<float> crossfadeBuffer;

    // Lock-free double-buffer for freq-domain IR handoff (builder thread -> audio thread)
    // Protocol: builder writes to buffers[writeIndex], publishes via readyIndex (release).
    // Audio thread claims with readyIndex.exchange(-1, acquire), guards the read via
    // readingIndex so the builder never overwrites an in-progress read (ABA guard).
    struct PendingFreqIR {
        std::vector<float> buffers[2];   // Double buffer, pre-allocated in prepareToPlay
        std::atomic<int> readyIndex { -1 };   // Index of buffer with fresh data (-1 = none)
        std::atomic<int> writeIndex { 0 };    // Index builder will write to next
        std::atomic<int> readingIndex { -1 }; // Index audio thread is reading (-1 = none)
    };
    PendingFreqIR pendingFreqIR;
    
    //==============================================================================
    // AI Components
    //==============================================================================
    AIEngine aiEngine;
    ReferenceMatcher referenceMatcher;
    UserLearningSystem userLearning;
    SemanticEQEngine semanticEngine;
    std::array<int, SemanticEQEngine::numQualities> semanticBandAssignments {};
    
    //==============================================================================
    // Utilities
    //==============================================================================
    std::unique_ptr<PresetManager> presetManager;
    
    // OSC parameter server — exposes all APVTS parameters on port 11100
    std::unique_ptr<OSCParameterServer> oscParamServer;
    
    //==============================================================================
    // State (atomic for thread-safe access)
    //==============================================================================
    std::atomic<double> currentSampleRate { 44100.0 };
    int currentBlockSize = 512;
    int preparedNumInputChannels = 0;
    std::atomic<PhaseMode> currentPhaseMode { PhaseMode::ZeroLatency };
    std::atomic<MSMode> currentMSMode { MSMode::Stereo };
    std::atomic<bool> eqCurveNeedsUpdate { true };
    std::atomic<bool> processorReady { false };  // Set true after prepareToPlay completes
    
    // Parameter update flag
    std::atomic<bool> parametersNeedUpdate { true };
    std::atomic<bool> pendingReset { false };
    int analyzerResolutionCached = 2;
    int analyzerSpeedCached = 1;
    int lastReportedLatency = 0;
    int worstCaseLatencySamples = 0;
    int worstCaseOversamplingLatency = 0;
    int preallocatedMaxSamples = 0;
    int aiAnalysisSamples = 0;
    int aiAnalysisIntervalSamples = 0;
    int autoGainBlockCounter = 0;
    static constexpr int autoGainUpdateStride = 4;
    
    //==============================================================================
    // A/B/C/D Comparison Storage
    //==============================================================================
    struct EQSlot
    {
        std::array<BandState, maxBands> bands {};
        float outputGain = 0.0f;
        juce::String name;
    };
    
    EQSlot slotA, slotB, slotC, slotD;
    std::atomic<ABState> currentABState { ABState::A };
    
    //==============================================================================
    // Auto-Gain (all atomic for thread-safety)
    //==============================================================================
    std::atomic<bool> autoGainEnabled { false };
    std::atomic<float> autoGainCompensation { 0.0f };
    std::atomic<float> preEQRMS { 0.0f };
    std::atomic<float> postEQRMS { 0.0f };
    static constexpr float rmsSmoothing = 0.95f;
    
    // Smoothed output gain (prevents zipper noise)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    
    // Quality mode cache
    int qualityModeCached = 0;
    
    //==============================================================================
    // IR Builder Thread (RAII with std::thread)
    // NOTE: WaitableEvent declared before jthread so event outlives thread during teardown
    juce::WaitableEvent irBuildEvent;
    std::thread irBuilderThread;
    std::atomic<bool> stopIRBuilder { false };
    
    // AI analysis offloaded from audio thread
    static constexpr size_t aiSpectrumBins = 2049; // 4096 FFT -> 2049 bins
    static constexpr size_t aiSpectrumQueueCapacity = 8;
    using AISpectrumFrame = std::array<float, aiSpectrumBins>;
    std::vector<float> silentSpectrumBuffer;  // Pre-allocated silent spectrum for processBlock fallback
    AIEQCore::SPSCQueue<AISpectrumFrame, aiSpectrumQueueCapacity> aiSpectrumQueue;
    juce::WaitableEvent aiSpectrumEvent;
    std::thread aiAnalysisThread;
    std::atomic<bool> stopAIAnalysis { false };
    
    // IR builder thread function (runs in background)
    void irBuilderThreadFunc();
    
    //==============================================================================
    // Dirty Flags for GUI Updates
    //==============================================================================
    std::atomic<bool> spectrumDataReady { false };
    std::atomic<bool> meterDataReady { false };
    std::atomic<bool> aiProblemsChanged { false };

    // Output peak metering (lock-free, written by audio thread, read by GUI)
    std::atomic<float> outputPeakLeft { 0.0f };
    std::atomic<float> outputPeakRight { 0.0f };
    std::atomic<uint64_t> parameterChangeCounter { 0 };
    // Atomic to prevent data race if prepareToPlay (message thread) overlaps with
    // processBlock (audio thread) during host reconfiguration. Relaxed ordering is
    // sufficient: the variable is effectively owned by the audio thread during processing.
    std::atomic<uint64_t> lastProcessedParameterChangeCounter { 0 };
    std::atomic<uint64_t> irCoeffVersion { 0 };
    std::atomic<uint32_t> blockClampEvents { 0 };
    
    //==============================================================================
    // Cached Parameter Pointers (avoid map lookups in processBlock)
    //==============================================================================
    struct CachedBandParams
    {
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* solo = nullptr;
        std::atomic<float>* dynMode = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* dynRatio = nullptr;
        std::atomic<float>* dynAttack = nullptr;
        std::atomic<float>* dynRelease = nullptr;
        std::atomic<float>* dynKnee = nullptr;
        std::atomic<float>* dynRange = nullptr;
        std::atomic<float>* slope = nullptr;
    };
    
    std::array<CachedBandParams, maxBands> cachedParams {};
    std::atomic<float>* cachedOutputGain = nullptr;
    std::atomic<float>* cachedDryWet = nullptr;
    std::atomic<float>* cachedAutoGain = nullptr;
    std::atomic<float>* cachedDynEqEnabled = nullptr;
    std::atomic<float>* cachedNumActiveBands = nullptr;
    std::atomic<float>* cachedBypass = nullptr;
    std::atomic<float>* cachedQualityMode = nullptr;
    std::atomic<float>* cachedPhaseModeParam = nullptr;
    std::atomic<float>* cachedMSModeParam = nullptr;
    std::atomic<float>* cachedOversamplingParam = nullptr;
    static constexpr int oversamplingAutoIndex = 3; // "Auto" choice index
    std::atomic<float>* cachedAnalyzerResolution = nullptr;
    std::atomic<float>* cachedAnalyzerSpeed = nullptr;
    std::atomic<float>* cachedAIEnabled = nullptr;
    std::atomic<float>* cachedSourceProfile = nullptr;
    std::atomic<float>* cachedShowPostSpectrum = nullptr;
    std::atomic<float>* cachedAISensitivity = nullptr;
    std::atomic<float>* cachedAIStrength = nullptr;
    std::atomic<float>* cachedDynEqMix = nullptr;
    std::atomic<float>* cachedDynAutoMakeup = nullptr;
    std::atomic<bool> parametersCached { false };
    
    // Active bands count
    std::atomic<int> numActiveBands { 8 };
    
    // Parameter IDs for listener registration
    std::vector<juce::String> eqParameterIDs;

    // For safe async callbacks (prevents dangling pointer crash)
    JUCE_DECLARE_WEAK_REFERENCEABLE(AIEqualizerAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessor)
};
