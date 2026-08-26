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
#include <functional>
#include <mutex>
#include <thread>  // std::thread and std::stop flags are in <thread> in C++20

#include "Core/LockFreeStructures.h"
#include "Core/LockFreeAudioFIFO.h"
#include "Core/LatestValueMailbox.h"
#include "Core/CaptureService.h"
#include "Core/HistoryManager.h"
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/ParametricEQProcessor.h"
#include "DSP/DynamicEQProcessor.h"
#include "DSP/LinearPhaseProcessor.h"
#include "DSP/BoundedSlewLimiter.h"
#include "Core/OSCParameterServer.h"
#include "AI/AIEngine.h"
#include "AI/PerceptualFrontEnd.h"
#include "AI/ReferenceMatcher.h"
#include "AI/SpectralContext.h"
#include "AI/UserLearning.h"
#include "AI/SemanticEQEngine.h"
#include "AI/DynamicCorrectionEngine.h"
#include "Utils/Logger.h"
#include "Utils/PresetManager.h"
#if AIEQ_GUI_DEBUG
#include "Utils/DebugLog.h"
#endif

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
                                  public juce::AudioProcessorValueTreeState::Listener,
                                  private juce::AsyncUpdater
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
    enum class BypassPhase { Active, FadingToBypass, Bypassed, FadingToActive };

    //==============================================================================
    // Component Access (all [[nodiscard]] for API safety)
    //==============================================================================
    [[nodiscard]] SpectrumAnalyzer& getSpectrumAnalyzer() noexcept { return spectrumAnalyzer; }
    [[nodiscard]] const SpectrumAnalyzer& getSpectrumAnalyzer() const noexcept { return spectrumAnalyzer; }

    [[nodiscard]] SpectrumAnalyzer& getPostEQAnalyzer() noexcept { return postEQAnalyzer; }
    [[nodiscard]] const SpectrumAnalyzer& getPostEQAnalyzer() const noexcept { return postEQAnalyzer; }

    // Metrological spectrum pipeline FIFOs (SPSC, audio thread writes, GUI thread reads)
    [[nodiscard]] LockFreeAudioFIFO<float>& getPreEqFifo() noexcept  { return preEqSpectrumFifo; }
    [[nodiscard]] LockFreeAudioFIFO<float>& getPostEqFifo() noexcept { return postEqSpectrumFifo; }
    
    [[nodiscard]] ParametricEQProcessor& getEQProcessor() noexcept { return eqProcessor; }
    [[nodiscard]] const ParametricEQProcessor& getEQProcessor() const noexcept { return eqProcessor; }
    
    [[nodiscard]] DynamicEQProcessor& getDynamicEQProcessor() noexcept { return dynamicEQProcessor; }
    [[nodiscard]] const DynamicEQProcessor& getDynamicEQProcessor() const noexcept { return dynamicEQProcessor; }

    // Dynamic EQ metering (GUI-safe, independent of processing path)
    [[nodiscard]] DynamicEQProcessor::BandMeter getDynamicBandMeter(int bandIndex) const noexcept;
    [[nodiscard]] float getDynamicTotalGainReduction() const noexcept;
    [[nodiscard]] DynamicEQProcessor::DetectorAvailability
        getDynamicDetectorAvailability(int bandIndex) const noexcept;
    
    [[nodiscard]] AIEngine& getAIEngine() noexcept { return aiEngine; }
    [[nodiscard]] const AIEngine& getAIEngine() const noexcept { return aiEngine; }
    
    [[nodiscard]] ReferenceMatcher& getReferenceMatcher() noexcept { return referenceMatcher; }
    [[nodiscard]] const ReferenceMatcher& getReferenceMatcher() const noexcept { return referenceMatcher; }
    
    [[nodiscard]] UserLearningSystem& getUserLearning() noexcept { return userLearning; }
    [[nodiscard]] const UserLearningSystem& getUserLearning() const noexcept { return userLearning; }
    
    /** T5.1 - message thread. The freshest deterministic tonal context the AI
        worker has published, or nullopt if none is available yet.

        Deliberately a SNAPSHOT and not a live reference: planning must freeze
        what the source looked like when the user pressed PLAN, and the worker
        must never reach into analysis state that keeps moving underneath it. */
    [[nodiscard]] std::optional<AIEQPerceptual::SpectralContext> getSpectralContextSnapshot() noexcept;

    /** T5.2 - who currently needs the analysis front-end running.
    
        Historically the front-end was fed only while Ember Assist was enabled,
        which silently made every other consumer depend on a switch that has
        nothing to do with them: with Assist off, Semantic would go
        source-blind and Match later would too. Analysis is infrastructure, not
        a feature of Assist, so it runs while ANY consumer needs it and the
        consumers stay independent of each other. */
    enum class AnalysisConsumer : std::uint32_t
    {
        Assist   = 1u << 0,
        Semantic = 1u << 1,
        Match    = 1u << 2
    };

    void setAnalysisConsumer(AnalysisConsumer consumer, bool needed) noexcept
    {
        const auto bit = static_cast<std::uint32_t>(consumer);
        if (needed)
            analysisConsumerMask.fetch_or(bit, std::memory_order_acq_rel);
        else
            analysisConsumerMask.fetch_and(~bit, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool isAnalysisConsumerActive(AnalysisConsumer consumer) const noexcept
    {
        return (analysisConsumerMask.load(std::memory_order_acquire)
                & static_cast<std::uint32_t>(consumer)) != 0u;
    }

    [[nodiscard]] bool analysisNeeded() const noexcept
    {
        return analysisConsumerMask.load(std::memory_order_acquire) != 0u;
    }

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
    // Counter incremented ONLY when curve-affecting params change
    // (Freq/Gain/Q/Type/Enabled/Slope/CurveMode).
    // Use this for EQ curve rebuild decisions — ignores DynEQ, metering, phase, etc.
    [[nodiscard]] uint64_t getEQCurveChangeCounter() const noexcept { return eqCurveChangeCounter.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32_t getBlockClampEvents() const noexcept { return blockClampEvents.load(std::memory_order_relaxed); }

    // Click detector — GUI reads these on its timer; clickEventCount resets on read
    [[nodiscard]] uint32_t consumeClickEvents() noexcept
    {
        return clickEventCount.exchange(0, std::memory_order_relaxed);
    }
    [[nodiscard]] uint8_t getClickLastCheckpoint() const noexcept
    {
        return clickLastCheckpoint.load(std::memory_order_relaxed);
    }
    static constexpr const char* kClickCheckpointNames[] = {
        "INPUT", "pre-EQ", "post-EQ", "crossfade", "output", "bypass"
    };

    // Output peak metering (GUI reads, audio thread writes)
    [[nodiscard]] float getOutputPeakLeft() const noexcept { return outputPeakLeft.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getSafetyLimiterFaultCount() const noexcept
    {
        return safetyLimiterFaultCount.load(std::memory_order_acquire);
    }
    [[nodiscard]] float getOutputPeakRight() const noexcept { return outputPeakRight.load(std::memory_order_relaxed); }

    // Parameter tree access
    [[nodiscard]] juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    [[nodiscard]] PhaseMode getCurrentPhaseMode() const noexcept { return currentPhaseMode.load(std::memory_order_relaxed); }

    /** The Dynamic EQ instance that is actually processing audio right now.

        There are four of them and only one runs at a time: the base instance in
        Zero Latency and Linear Phase, dynamicEQProcessorHQ in Natural Phase
        (which works on the oversampled buffer), and the Mid/Side pair in M/S.
        They all receive identical band parameters, but only the running one has
        live envelope state.

        The display reads that live state directly to draw the moving dynamic
        curve, so it has to read the running instance. Reading the base one
        unconditionally is what froze the curve the moment the user switched to
        Natural Phase: the instance it was watching had stopped processing, so
        its envelopes stayed at whatever value they last held. The band meters
        did not show the bug because they go through the mode-aware cache that
        updateDynamicMeterCacheFrom() fills from the running instance.

        M/S runs two independent instances against different content, and the
        curve is a single line: it follows Mid. That is a real half rather than
        a frozen whole. */
    [[nodiscard]] const DynamicEQProcessor& getActiveDynamicEQProcessorForDisplay() const noexcept;
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
        int slope = 0;
        int dynMode = 0;
        int dynTrigger = DynamicEQProcessor::TriggerSide_Above;
        int detectionMode = DynamicEQProcessor::DetectionMode_RMS;
        int detectorSource = DynamicEQProcessor::DetectorSource_InternalWideband;
        float sidechainFrequency = 1000.0f;
        float sidechainQ = 1.0f;
        float dynThreshold = -24.0f;
        float dynRatio = 2.0f;
        float dynAttack = 10.0f;
        float dynRelease = 100.0f;
        float dynRange = 24.0f;
        float dynKnee = 6.0f;
        int curveMode = static_cast<int>(ParametricEQProcessor::CurveMode::Surgical);
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

    // UI graph edits own only geometry.  Keeping this merge at processor level
    // prevents legacy node gestures from resetting newer per-band state.
    void setBandGeometry(int bandIndex, float frequency, float gain, float q,
                         int type, bool enabled);
    
    [[nodiscard]] int getNumActiveBands() const noexcept { return numActiveBands.load(std::memory_order_relaxed); }
    void setNumActiveBands(int n) noexcept;
    void markParametersChanged() noexcept
    {
        parameterChangeCounter.fetch_add(1, std::memory_order_relaxed);
#if AIEQ_GUI_DEBUG
        static std::atomic<int> debugParamChangeCount { 0 };
        static std::atomic<double> debugParamLastReport { 0.0 };
        int count = debugParamChangeCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (count % 200 == 0) // log every 200 changes to avoid flooding
        {
            double now = juce::Time::getMillisecondCounterHiRes();
            double last = debugParamLastReport.load(std::memory_order_relaxed);
            if (now - last > 2000.0)
            {
                debugParamLastReport.store(now, std::memory_order_relaxed);
                aieqDebugLog( "[PARAMS] ~%.0f changes/sec\n", count * 1000.0 / std::max(1.0, now - last));
                debugParamChangeCount.store(0, std::memory_order_relaxed);
            }
        }
#endif
    }

    // Semantic EQ application (message thread only)
    enum class SemanticApplyPolicy : int
    {
        BestEffortLegacy = 0,
        RequireCompletePlan
    };

    struct SemanticApplyResult
    {
        int requestedBands = 0;
        int appliedBands = 0;
        int rejectedBands = 0;
        bool atomicRejected = false;
        bool deferredToMessageThread = false;

        [[nodiscard]] bool complete() const noexcept
        {
            return !atomicRejected && !deferredToMessageThread
                && rejectedBands == 0 && appliedBands == requestedBands;
        }
    };

    [[nodiscard]] SemanticApplyResult applySemanticAdjustments(
        const std::vector<SemanticEQEngine::SemanticEQAdjustment>& adjustments,
        SemanticApplyPolicy policy = SemanticApplyPolicy::BestEffortLegacy);
    
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
    void triggerEQCurveUpdate();        // Mark curve/UI dirty
    void triggerLinearPhaseIRUpdate();  // Mark LP IR dirty + signal background builder
    void updateLinearPhaseIRIfNeeded(); // Swap in pre-built IR if ready
    void requestIRBuild();              // Signal background IR builder

    /** Test-only hook: inject a flat (Dirac delta) IR directly into the LP
        convolver, set linearIRLoaded = true and pin that fixture until the next
        prepareToPlay(). Background results are drained but cannot replace it.
        Enables deterministic tests that exercise LP routing rather than the
        asynchronous IR builder. */
    void forceLinearIRReady();
    
    //==============================================================================
    // Source Profile
    //==============================================================================
    void setSourceProfile(AIEngine::SourceProfile profile);
    [[nodiscard]] AIEngine::SourceProfile getSourceProfile() const { return aiEngine.getSourceProfile(); }

    //==============================================================================
    // Audio Capture (using lock-free CaptureService)
    //==============================================================================
    void captureAudioSnapshotMs(int lengthMs);
    /** FA-001: capture is disabled for the first beta.

        CaptureService has an open ownership/concurrency finding from the full
        audit, and capture is an accessory feature — shipping it would mean
        carrying a known P0 for something the core EQ does not need. Rather than
        opening a fifth concurrent redesign, the service is never armed: the two
        arming entry points and the analysis-thread starter all refuse while
        this is false, so the reported race has no way to occur in the product.

        The code is left compiled and its tests keep running, so re-enabling is
        one flag once FA-001 is genuinely fixed rather than a build revert.
        AICaptureDisabledWitnessTest holds the contract. */
    static constexpr bool kCaptureEnabledForShipping = false;

    /** Whether any arming path may proceed. The product answer is fixed by
        kCaptureEnabledForShipping; tests that genuinely exercise CaptureService
        opt in explicitly, so the component keeps its coverage and stays ready
        for whoever fixes FA-001. Out-of-line and backed by a file-static, so no
        data member is added — see the layout note above
        getAIFrontEndDiagnostics(). */
    [[nodiscard]] static bool isCaptureAllowed() noexcept;

    /** TEST-ONLY. Enabling this deliberately re-exposes the FA-001 surface, so
        it belongs only in tests that are about CaptureService itself. */
    static void setCaptureAllowedForTests(bool allowed) noexcept;

    [[nodiscard]] bool startManualCapture();
    void stopManualCapture();
    [[nodiscard]] bool isCapturing() const noexcept { return captureService.isCapturing(); }
    [[nodiscard]] const std::vector<float>& getCapturedAudioMono() const { return captureService.getCapturedAudioMono(); }
    [[nodiscard]] bool isCaptureBufferSafeToRead() const noexcept { return captureBufferReady.load(std::memory_order_acquire); }
    void getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples = 1024) const;
    [[nodiscard]] double getCapturedSampleRate() const noexcept { return captureService.getCapturedSampleRate(); }
    [[nodiscard]] bool analyzeCapturedAudioSnapshot();
    void resetAIAnalysisConcurrencyCountersForTests() noexcept
    {
        aiConcurrentAnalyses.store(0, std::memory_order_relaxed);
        aiMaxConcurrentAnalyses.store(0, std::memory_order_relaxed);
        aiAnalysisCallAttemptsForTests.store(0, std::memory_order_relaxed);
        aiAnalysisEnteredForTests.store(0, std::memory_order_relaxed);
    }
    [[nodiscard]] int getMaxConcurrentAIAnalysesForTests() const noexcept
    {
        return aiMaxConcurrentAnalyses.load(std::memory_order_relaxed);
    }
    [[nodiscard]] int getAIAnalysisEnteredForTests() const noexcept
    {
        return aiAnalysisEnteredForTests.load(std::memory_order_relaxed);
    }
    [[nodiscard]] int getAIAnalysisCallAttemptsForTests() const noexcept
    {
        return aiAnalysisCallAttemptsForTests.load(std::memory_order_relaxed);
    }
    void setAIAnalysisBlockForTests(bool shouldBlock) noexcept
    {
        aiAnalysisBlockForTests.store(shouldBlock, std::memory_order_release);
    }
    // UX "Diagnosi Stabile" (counter-check Finding #1): dedicated capture-completion
    // signal for the GUI freeze trigger. The generic aiProblemsChanged flag is also
    // posted by the live analysis loop (see aiAnalysisThreadFunc), so a freeze keyed
    // on it can capture the LIVE state instead of the capture result while the
    // transport is running. This consume-style accessor reads the flag set exclusively
    // by the capture-analysis finish() path in analyzeCapturedAudioSnapshot().
    [[nodiscard]] bool consumeCaptureAnalysisCompleted() noexcept { return captureAnalysisCompleted.exchange(false, std::memory_order_acq_rel); }
    [[nodiscard]] bool getCaptureAnalysisResult() const noexcept { return captureAnalysisResult.load(std::memory_order_acquire); }
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
    bool applyBandStateDelta(int bandIndex, const BandState& targetState, bool useGestures);
    void updateReportedLatency();
    void handleAsyncUpdate() override;
    [[nodiscard]] bool requiresPaddedLatencyPlan() const noexcept;
    void cacheParameterPointers();
    bool runCapturedAudioAnalysis();
    void analyzeSpectrumSerialized(const std::vector<float>& spectrum, bool force = false);
    void aiAnalysisThreadFunc();
    // Cold-path lifecycle barrier. These methods are never called from processBlock().
    // quiesceBackgroundWorkersForLifecycle() must complete before prepare/release
    // mutates state observed by IR/AI/capture workers.
    void quiesceBackgroundWorkersForLifecycle();
    void restartBackgroundWorkersAfterLifecycle();
    void clearDynamicMeterCache() noexcept;
    void updateDynamicMeterCacheFrom(const DynamicEQProcessor& src) noexcept;
    void updateDynamicMeterCacheFromMS(const DynamicEQProcessor& mid,
                                       const DynamicEQProcessor& side,
                                       bool includeMid,
                                       bool includeSide) noexcept;
    void primeBandSmoothers(double sampleRate);
    [[nodiscard]] bool applySmoothedBandParams(int blockSamples,
                                               bool paramsChanged = false);
    
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
    std::mutex aiAnalysisMutex;
    std::atomic<int> aiConcurrentAnalyses { 0 };
    std::atomic<int> aiMaxConcurrentAnalyses { 0 };
    std::atomic<int> aiAnalysisCallAttemptsForTests { 0 };
    std::atomic<int> aiAnalysisEnteredForTests { 0 };
    std::atomic<bool> aiAnalysisBlockForTests { false };
    
    //==============================================================================
    // APVTS (thread-safe parameter management)
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    
    //==============================================================================
    // DSP Components
    //==============================================================================
    SpectrumAnalyzer spectrumAnalyzer;      // Pre-EQ spectrum (legacy path)
    SpectrumAnalyzer postEQAnalyzer;        // Post-EQ spectrum (legacy path)

    // Metrological pipeline FIFOs — SPSC, audio thread → GUI thread
    // Capacity: 32768 samples (~680ms at 48kHz), handles large block hosts
    LockFreeAudioFIFO<float> preEqSpectrumFifo;
    LockFreeAudioFIFO<float> postEqSpectrumFifo;
    
    // Main EQ processors (Zero-Latency path)
    ParametricEQProcessor eqProcessor;
    DynamicEQProcessor dynamicEQProcessor;
    
    // High-Quality processors (Natural Phase path with oversampling)
    ParametricEQProcessor eqProcessorHQ;
    DynamicEQProcessor dynamicEQProcessorHQ;

    // The optional external sidechain is a distinct input bus.  It must never
    // be counted as a main-program channel or processed by the audible EQ.
    // Natural phase upsamples it through an identical mono chain so detector
    // and programme reach the HQ Dynamic EQ on the same time grid.  Linear
    // phase uses the dedicated delay below to align the detector with the
    // convolved programme before the intentional Dynamic EQ lookahead.
    std::unique_ptr<juce::dsp::Oversampling<float>> sidechainOversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> sidechainOversampler4x;
    alignas(64) juce::AudioBuffer<float> sidechainInputScratch;
    alignas(64) juce::AudioBuffer<float> linearAlignedSidechainBuffer;
    alignas(64) juce::AudioBuffer<float> linearSidechainDelayBuffer;
    int linearSidechainDelayWritePos = 0;
    int linearSidechainDelayBufferSize = 0;
    int linearPhasePreDynamicLatencySamples = 0;
    bool externalSidechainWasAvailable = false; // audio-thread owned after prepare

    // Dynamic EQ metering cache (aggregated across processing paths)
    struct DynamicMeterCacheEntry
    {
        std::atomic<float> input { -100.0f };
        std::atomic<float> gainReduction { 0.0f };
        std::atomic<float> output { -100.0f };
    };
    std::array<DynamicMeterCacheEntry, maxBands> dynamicMeterCache {};
    std::atomic<float> dynamicTotalGR { 0.0f };

    // Authority-bounded automation: frequency/Q move in log2 at <=100 oct/s;
    // gain moves linearly in dB at <=2400 dB/s. State advances by the exact
    // number of elapsed host samples and never depends on wall-clock timing.
    std::array<AIEQDSP::Log2SlewLimiter, maxBands> smoothedBandFreq {};
    std::array<AIEQDSP::LinearSlewLimiter, maxBands> smoothedBandGain {};
    std::array<AIEQDSP::Log2SlewLimiter, maxBands> smoothedBandQ {};
    std::array<float, maxBands> targetBandFreq {};
    std::array<float, maxBands> targetBandGain {};
    std::array<float, maxBands> targetBandQ {};
    std::array<int, maxBands> targetBandType {};
    std::array<int, maxBands> prevAppliedBandType {};  // track type for state clear on topology change
    std::array<int, maxBands> targetBandCurveMode {};
    std::array<int, maxBands> prevAppliedBandCurveMode {};
    std::array<int, maxBands> bandTopologyFadeSamplesRemaining {};
    int previousSmoothedBandBlockSamples { 0 };
    std::array<int, maxBands> targetBandSlope {};
    std::array<bool, maxBands> targetBandEnabled {};
    std::array<bool, maxBands> targetBandSolo {};
    std::array<DynamicEQProcessor::DynamicBandParams, maxBands> targetDynamicBandParams {};
    bool bandSmoothingPrimed { false };
    std::atomic<bool> correctionSmoothingActive { false };
    bool correctionSmoothingNeedsRateConfig { false };

    // Linear-phase delay compensation when IR is not ready
    juce::AudioBuffer<float> linearPhaseDelayBuffer;
    int linearPhaseDelayWritePos = 0;
    
    // Shadow processors for thread-safe IR building
    // Updated atomically when coefficients change, read by IR builder thread
    ParametricEQProcessor eqProcessorForIR;
    DynamicEQProcessor dynamicEQProcessorForIR;
    std::atomic<bool> irCoefficientsUpdated { false };
    std::atomic<bool> linearIRTestOverridePinned { false };
    
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
    alignas(64) juce::AudioBuffer<float> phaseTransitionBuffer;
    alignas(64) juce::AudioBuffer<float> msModeTransitionBuffer;
    alignas(64) juce::AudioBuffer<float> oversamplingTransitionBuffer;
    std::atomic<int> phaseTransitionFromMode { -1 };
    std::atomic<int> phaseTransitionSamplesRemaining { 0 };
    static constexpr int phaseTransitionCrossfadeSamples = 1024; // ~21ms @ 48kHz — longer fade needed for LP/Natural IIR state divergence
    std::atomic<int> oversamplingTransitionFromEffective { -1 };
    std::atomic<int> oversamplingTransitionSamplesRemaining { 0 };
    static constexpr int oversamplingTransitionCrossfadeSamples = 2048; // longer fade for 2x↔4x startup/warmup
    std::atomic<bool> bypassStateInitialized { false };

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

    // Bypass state machine — avoids click on bypass toggle
    std::atomic<BypassPhase> bypassPhase { BypassPhase::Active };
    std::atomic<int>  bypassCrossfadeRemaining { 0 };
    int   currentBypassCrossfadeSamples = 2400;
    // Bypass crossfade must be longer than worstCaseLatencySamples (2176) so that
    // the dry signal hasn't faded to silence by the time fresh wet signal appears
    // through the wet padding delay.  Computed in prepareToPlay.
    int bypassCrossfadeSamples = 4800;  // default ~100ms @ 48kHz, overridden in prepare
    void resetDSPStateForBypassExit();  // resets all IIR/convolver/oversampler state
    static constexpr int aiCorrectionCrossfadeSamples = 1024;  // ~21ms @ 48kHz
    static constexpr int abSwitchCrossfadeSamples = 2048;      // ~43ms @ 48kHz, bulk state restore is more discontinuous

    // Dry path delay line for phase-aligned bypass crossfade.
    // The dry signal is delayed by worstCaseLatencySamples so it matches
    // the wet (processed) path timing during crossfade.
    alignas(64) juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWritePos = 0;
    int dryDelayLength = 0;
    int dryDelayBufferSize = 0;

    // Wet output padding delay — compensates when actual DSP latency < worstCaseLatencySamples.
    alignas(64) juce::AudioBuffer<float> wetPaddingDelayBuffer;
    int wetPaddingWritePos = 0;
    int wetPaddingDelaySamples = 0;
    int wetPaddingBufferSize = 0;

    // Wet padding smoothing: ramp padSamples over the phase transition crossfade
    // window to avoid abrupt read-position jumps when switching phase modes.
    // Cat 2 Fix: ZL -> NaturalPhase (0->1) caused a ~15-sample discontinuity in
    // the ring read position, producing a click of ~0.6 amplitude on a 1kHz sine.
    // During a transition we fractional-read the ring buffer with a linear ramp
    // from wetPadRampStart to padSamples; outside transitions the integer path
    // is unchanged (zero overhead in the common case).
    int wetPadLastSamples = 0;    // padSamples applied in the previous block
    int wetPadRampStart   = 0;    // starting value for the active ramp
    bool wetPadRampActive = false;

    // Pending A/B whole-chain crossfade: armed by message thread BEFORE parameter
    // changes, consumed by audio thread BEFORE updateEQFromParameters() so the
    // snapshot captures the OLD filter state, not the already-updated one.
    std::atomic<bool> abCrossfadeSnapshotNeeded { false };
    std::array<std::atomic<bool>, maxBands> abCrossfadePendingBands {};
    // IR build debounce: accumulate rapid drag events, rebuild only after silence
    std::atomic<int64_t> irBuildRequestedAt { 0 };    // ms timestamp of last request
    static constexpr int64_t irBuildDebounceMs = 20;  // rebuild after 20ms of no changes
    // Was 80ms — needed before latest-wins crossfade system (Commit 4).
    // Now that storePrePartitionedIR queues mid-fade IRs in pendingPartitions
    // with full 4096-sample crossfade, rapid IR arrivals are safe.
    // 20ms allows ~3-4 IR updates/sec during drag, keeping spectral jumps small.
    
    //==============================================================================
    // Oversampling
    //==============================================================================
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    alignas(64) juce::AudioBuffer<float> naturalOversampledBuffer;
    int naturalPhaseLatency = 64;
    std::atomic<int> oversamplingFactor { 0 };          // user choice
    std::atomic<int> oversamplingEffectiveFactor { 0 }; // resolved (auto/off/2x/4x)
    std::atomic<bool> hqRuntimeReady { true };
    std::atomic<uint32_t> hqReconfigureFailures { 0 };
    
    //==============================================================================
    // Linear Phase (partitioned convolution)
    //==============================================================================
    std::array<std::unique_ptr<LinearPhaseProcessor>, 2> linearPhaseProcessors;
    std::atomic<int> activeIRIndex { 0 };
    std::atomic<int> readyIRIndex { -1 };
    std::array<std::atomic<bool>, 2> linearIRLoaded { false, false };
    std::atomic<int> consecutiveIRReadyBlocks { 0 };

    // LP IR first-load crossfade: when the IR becomes ready while we were in
    // fallback ZL-EQ mode, crossfade from ZL output to LP convolution output
    // over 1024 samples to avoid a hard cut.
    bool lpWasFallback = false;    // true while LP mode is active but IR not yet loaded
    int  lpFirstLoadCrossfadeRemaining = 0;
    static constexpr int lpFirstLoadCrossfadeSamples = 1024;
    juce::AudioBuffer<float> lpFirstLoadFallbackBuf;
    
    // IR crossfade for click-free transitions
    static constexpr int irCrossfadeSamples = 128;
    std::atomic<int> crossfadeSamplesRemaining { 0 };
    std::atomic<int> previousIRIndex { 0 };
    alignas(64) juce::AudioBuffer<float> crossfadeBuffer;

    // Builder -> audio latest-value handoff for the freq-domain IR (EC-004).
    //
    // The previous protocol claimed a buffer with readyIndex.exchange(-1) and
    // only then pinned it with readingIndex.store(ri). Between those two atomics
    // the slot was claimed but not yet marked, so the builder's
    // "while (wi == readingIndex)" guard still saw it as reusable and could
    // overwrite the IR the audio thread was copying. The comment called it an
    // ABA guard, but claim and pin were not one transition.
    //
    // Ownership states make the claim atomic: the consumer must win
    // READY->READING before it dereferences anything, and a READING slot is
    // never writable. Four slots (~64 KiB each, ~256 KiB total) keep the
    // producer able to publish while one slot is being read and one is in
    // flight. Latest wins: a lagging consumer drops intermediate IRs, which is
    // correct here — only the newest EQ curve matters.
    static constexpr size_t packedIRFloatCount =
        PartitionedConvolver::numParts * PartitionedConvolver::fftPartSize * 2;
    using PackedIRPayload = std::array<float, packedIRFloatCount>;
    LatestValueMailbox<PackedIRPayload, 4> pendingFreqIR;
    
    //==============================================================================
    // AI Components
    //==============================================================================
    AIEngine aiEngine;
    ReferenceMatcher referenceMatcher;
    UserLearningSystem userLearning;
    SemanticEQEngine semanticEngine;
    // D1 (AI-evolution): per-band dynamic correction engine (opt-in, OFF by
    // default — see setDynamicCorrectionsEnabled / publishDynamicCorrections).
    DynamicCorrectionEngine dynamicCorrectionEngine;
    uint32_t dynamicCorrectionsVersion = 0;   // message-thread publish counter
    // Semantic band ownership: (quality, band-ordinal) -> EQ slot.
    // A quality's definition can emit up to 3 bands (plus merged complementary
    // copies); keying by quality ALONE made every band of a multi-band quality
    // overwrite the same slot — only the last survived (AI-evolution fix A2).
    // Ordinals are stable because generateEQFromState returns adjustments
    // sorted by frequency and a definition's band frequencies are fixed.
    static constexpr int kMaxSemanticBandSlots = maxBands;
    std::array<std::array<int, kMaxSemanticBandSlots>,
               SemanticEQEngine::numQualities> semanticBandAssignments {};

    // Semantic band ownership is explicit. A semantic plan may only claim a
    // currently disabled band; it never steals an enabled/manual band. The
    // original state is kept so reset/reconciliation can restore the slot. If
    // the user manually edits a semantic-owned slot, ownership is relinquished
    // instead of overwriting that edit on the next semantic update.
    std::array<bool, maxBands> semanticBandOwned {};
    std::array<bool, maxBands> semanticBandHasSnapshot {};
    std::array<BandState, maxBands> semanticBandOriginalStates {};
    std::array<BandState, maxBands> semanticBandLastAppliedStates {};
    int semanticOriginalActiveBandCount = -1;
    int semanticLastRequestedActiveBandCount = -1;
    
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

    // M/S mode crossfade state — avoids click/pop on mode switch
    std::atomic<int> previousMSModeForCrossfade { static_cast<int>(MSMode::Stereo) };
    std::atomic<int> msModeTransitionSamplesRemaining { 0 };
    static constexpr int msModeTransitionCrossfadeSamples = 1024; // ~21ms @ 48kHz
    std::atomic<bool> eqCurveNeedsUpdate { true };
    std::atomic<bool> processorReady { false };  // Set true after prepareToPlay completes
    
    // Parameter update flag
    std::atomic<bool> parametersNeedUpdate { true };
    std::atomic<bool> pendingReset { false };
    std::atomic<bool> aiCorrectionCrossfadePending { false };
    int analyzerResolutionCached = 2;
    int analyzerSpeedCached = 1;
    int lastReportedLatency = 0;
    int worstCaseLatencySamples = 0;
    int worstCaseOversamplingLatency = 0;
    struct LatencyPlan
    {
        int maximumSamples = 0;
        std::atomic<int> activeSamples { 0 };
        std::atomic<bool> reductionDeferred { false };
    } latencyPlan;
    int preallocatedMaxSamples = 0;
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
        bool dynEqEnabled = true;
        float dynEqMix = 100.0f;
        bool dynAutoMakeup = false;
        juce::String name;
    };
    
    // slotMutex_ guards ALL reads/writes to slotA..D and their fields.
    // INVARIANT: NEVER acquire from the audio thread (processBlock reads APVTS, not slots).
    // Contention is only between host thread (get/setStateInformation) and message thread (UI).
    mutable std::recursive_mutex slotMutex_;
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
    
    // Wet-only auto-gain makeup (before pad/mix). Manual output trim is after mix.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAutoGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    
    // Quality mode cache
    int qualityModeCached = 0;
    
    //==============================================================================
    // IR Builder Thread (RAII with std::thread)
    // NOTE: WaitableEvent declared before jthread so event outlives thread during teardown
    juce::WaitableEvent irBuildEvent;
    std::thread irBuilderThread;
    std::atomic<bool> stopIRBuilder { false };
    
    // AI analysis offloaded from audio thread.
    // EC-001/B4: continuous detection is sourced exclusively from the headless
    // PerceptualFrontEnd (fixed 4096 FFT -> 2049 raw dB bins). SpectrumAnalyzer
    // is GUI-only and no longer participates in AI detection.
    static constexpr size_t aiSpectrumBins = 2049;
    juce::WaitableEvent aiSpectrumEvent;
    std::thread aiAnalysisThread;
    std::atomic<bool> stopAIAnalysis { false };
    // Set from the message thread (parameterChanged) when an AI knob moves while
    // no audio frames are flowing (transport stopped). Consumed by
    // aiAnalysisThreadFunc via exchange(false) to force ONE re-analysis of the
    // last spectrum. Declared after the thread members above is irrelevant: the
    // thread is always explicitly joined in the destructor before teardown.
    std::atomic<bool> aiPendingReanalysis { false };
    // True while the host is performing non-realtime/offline rendering.
    // The audio producer does not feed aiFrontEndFifo in this mode and the AI
    // worker suppresses forced re-analysis until realtime resumes.
    std::atomic<bool> aiOfflineRenderActive { false };

    // T5.1 - deterministic tonal context, fanned out from the SAME front-end
    // frame the detector already consumes. No second FFT, and the push is a
    // Welford update over ~122 bands with no allocation, so it can sit inside
    // the drain without re-introducing the cost T3.2 took out of it.
    // The accumulator is AI-thread-owned; the message thread only ever sees an
    // immutable published snapshot, never the live statistics.
    // Semantic is registered from construction: its planning must be able to
    // describe the source whether or not the user has ever switched Assist on.
    std::atomic<std::uint32_t> analysisConsumerMask {
        static_cast<std::uint32_t>(AnalysisConsumer::Semantic) };

    AIEQPerceptual::SpectralContextAccumulator aiSpectralContextAccumulator;
    LatestValueMailbox<AIEQPerceptual::SpectralContext, 4> spectralContextMailbox;

    // Sticky stream-integrity marker. The audio producer sets this whenever it
    // intentionally skips AI samples (offline/disabled) or the SPSC FIFO drops
    // samples. The worker clears it only after discarding queued history and
    // resetting frontend overlap, so even a very short offline pulse cannot be
    // missed between worker polls.
    std::atomic<bool> aiFrontEndDiscontinuityPending { false };
    std::atomic<juce::int64> aiFrontEndDroppedSamples { 0 };
    std::atomic<juce::int64> aiFrontEndDiscontinuities { 0 };

    // EC-001 observability: counts productive headless analyses executed from
    // PerceptualFrontEnd frames. Used by parity/regression tests.
    std::atomic<juce::int64> aiHeadlessAnalyses { 0 };

    // AI-owned perceptual front-end — PRODUCTION detection source (EC-001/B4).
    // The AI thread drains aiFrontEndFifo, a dedicated SPSC fifo (audio thread
    // producer, AI thread the only consumer), and feeds rawDb frames into
    // AIEngine at the preserved ~10 Hz input cadence. GUI SpectrumAnalyzer and
    // its pre/post FIFOs are display-only and physically isolated from detection.
    //
    // Lifecycle ownership is provided by B1/EC-005: prepare/release stop+join
    // the AI worker before rebuilding this storage, and reopen aiFrontEndReady
    // only after AIEngine/front-end/cadence state is fully prepared.
    PerceptualFrontEnd aiFrontEnd;
    LockFreeAudioFIFO<float> aiFrontEndFifo;         // dedicated: audio -> AI thread
    std::vector<float> aiFrontEndScratch;            // AI-thread pull buffer
    std::atomic<bool> aiFrontEndReady { false };
    std::atomic<bool> aiFrontEndDrainBusy { false };
    std::atomic<juce::int64> aiFrontEndFrames { 0 };
    std::atomic<double> aiFrontEndMeanNs { 0.0 };
    std::atomic<juce::int64> aiFrontEndMaxNs { 0 };

public:
    struct FrontEndDiagnostics
    {
        juce::int64 frames = 0;
        double meanMs = 0.0;
        double maxMs = 0.0;
        juce::int64 droppedSamples = 0;
        juce::int64 discontinuities = 0;
    };
    /** Observability of the production AI-owned perceptual front-end.
        DELIBERATELY out-of-line (defined in PluginProcessor.cpp): test targets
        compile this header with JUCE_UNIT_TESTS=1 while the plugin SharedCode
        does not, and AIEngine/MLEngine contain test-gated DATA members — so the
        object layout of everything declared AFTER `aiEngine` differs between
        the two worlds. An inline body in a test TU would read garbage offsets
        (measured: frames ~= nanoseconds-since-start). Out-of-line, the body is
        compiled once in SharedCode with the true layout. The underlying
        macro-gated-data landmine is tracked as a separate ticket. */
    [[nodiscard]] FrontEndDiagnostics getAIFrontEndDiagnostics() const noexcept;
    [[nodiscard]] juce::int64 getAIHeadlessAnalysisCountForTests() const noexcept;

    /** TEST-ONLY observer of every productive AI analysis, invoked on the AI
        worker thread right after the detector has consumed a frame.

        EC-001 is a claim about where the detector's input comes from, so the
        witness has to compare the inputs themselves and not merely count them:
        the sequence number, the end-sample position of the frontend frame the
        spectrum represents, and the 2049 dB bins actually handed over.

        Declared unconditionally and defined OUT-OF-LINE, for exactly the reason
        spelled out above getAIFrontEndDiagnostics(). A first attempt put the
        std::function and its mutex behind #if JUCE_UNIT_TESTS as DATA MEMBERS;
        that shifts the layout of everything after them between test TUs and
        SharedCode, and the setter — inlined into a test TU — wrote at the wrong
        offset. It failed as "mutex lock failed: Invalid argument". The observer
        now lives in a file-static inside PluginProcessor.cpp, so the class
        layout is identical in both worlds and the body is compiled once. */
    using AIAnalysisObserverForTests =
        std::function<void(juce::int64 sequence,
                           juce::int64 sourceEndSample,
                           const std::vector<float>& spectrum)>;

    void setAIAnalysisObserverForTests(AIAnalysisObserverForTests observer) noexcept;


    /** D1 (AI-evolution): opt-in per-band dynamic correction engine.
        Default OFF; with no published snapshot the engine is a strict no-op.
        publishDynamicCorrectionsFromApplied converts the exact merged/limited
        corrections assigned by applyAICorrections (cuts only) into a dynamic
        snapshot — call from the message thread before approved corrections are
        cleared. */
    void setDynamicCorrectionsEnabled(bool on) noexcept { dynamicCorrectionEngine.setEnabled(on); }
    [[nodiscard]] bool areDynamicCorrectionsEnabled() const noexcept { return dynamicCorrectionEngine.isEnabled(); }
    [[nodiscard]] const DynamicCorrectionEngine& getDynamicCorrectionEngine() const noexcept
    {
        return dynamicCorrectionEngine;
    }
private:

    /** Sole DynamicCorrectionEngine publisher.  Kept private so external UI,
        test and integration code cannot accidentally introduce a second
        producer into the single-writer mailbox.  applyAICorrections() is the
        public message-thread entry point. */
    void publishDynamicCorrectionsFromApplied(
        const std::vector<AIEngine::Correction>& appliedCorrections);

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
    std::atomic<uint64_t> safetyLimiterFaultCount { 0 };
    std::atomic<float> outputPeakRight { 0.0f };
    std::atomic<uint64_t> parameterChangeCounter { 0 };
    std::atomic<uint64_t> eqCurveChangeCounter { 0 };  // Only curve-affecting params
    // Atomic to prevent data race if prepareToPlay (message thread) overlaps with
    // processBlock (audio thread) during host reconfiguration. Relaxed ordering is
    // sufficient: the variable is effectively owned by the audio thread during processing.
    std::atomic<uint64_t> lastProcessedParameterChangeCounter { 0 };
    std::atomic<uint64_t> irCoeffVersion { 0 };
    std::atomic<uint32_t> blockClampEvents { 0 };

    // ── Click detector ───────────────────────────────────────────────────────
    // Counts glitch events detected in processBlock (threshold: delta > 0.25 linear).
    // Reset to 0 when GUI reads it with consumeClickEvents().
    std::atomic<uint32_t> clickEventCount { 0 };
    // Last checkpoint that triggered (RT-safe: short string index, not heap string)
    // 0=input 1=preEQ 2=postEQ 3=crossfade 4=output 5=bypass
    std::atomic<uint8_t>  clickLastCheckpoint { 0 };
    float                 clickPrevSample { 0.0f }; // last sample of previous block (ch0)

    // RT heartbeat — accumulates samples, fires logFromRTThread every ~5s
    int                   rtHeartbeatCounter { 0 };

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
        std::atomic<float>* dynTrigger = nullptr;
        std::atomic<float>* detectionMode = nullptr;
        std::atomic<float>* detectorSource = nullptr;
        std::atomic<float>* sidechainFreq = nullptr;
        std::atomic<float>* sidechainQ = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* dynRatio = nullptr;
        std::atomic<float>* dynAttack = nullptr;
        std::atomic<float>* dynRelease = nullptr;
        std::atomic<float>* dynKnee = nullptr;
        std::atomic<float>* dynRange = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* curveMode = nullptr;
    };
    
    std::array<CachedBandParams, maxBands> cachedParams {};
    std::atomic<float>* cachedOutputGain = nullptr;
    std::atomic<float>* cachedDryWet = nullptr;
    std::atomic<float>* cachedAutoGain = nullptr;
    std::atomic<float>* cachedDynamicCorrections = nullptr;  // D1 exposure toggle
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
