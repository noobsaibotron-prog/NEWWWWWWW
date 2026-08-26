#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/LockFreeStructures.h"
#include "BiquadCoefficients.h"
#include "DynamicGainModel.h"
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
/**
 * Dynamic EQ Processor - FabFilter Pro-Q / TDR Nova Style
 * 
 * LOCK-FREE ARCHITECTURE (Elite/Top-Tier Standard):
 * - Zero mutex in audio path
 * - Atomic parameters with version counter
 * - Wait-free reads from audio thread
 * - Lock-free writes from message thread
 * 
 * Each band can operate in static or dynamic mode:
 * - Static: Traditional parametric EQ (fixed gain)
 * - Dynamic: Gain responds to input level (compressor/expander per band)
 * 
 * Features:
 * - Per-band threshold, ratio, attack, release, range, knee
 * - Sidechain filtering (listen to specific frequency range)
 * - RMS or Peak detection modes
 * - Lookahead for transparent compression
 * - Gain reduction metering per band
 */
class DynamicEQProcessor
{
public:
    //==============================================================================
    static constexpr int maxBands = AIEQCore::kMaxBands;
    
    // Use int for atomic operations (enum class not trivially copyable in all contexts)
    static constexpr int DetectionMode_Peak = 0;
    static constexpr int DetectionMode_RMS = 1;
    
    static constexpr int DynamicMode_Off = 0;
    static constexpr int DynamicMode_Compress = 1;
    static constexpr int DynamicMode_Expand = 2;
    static constexpr int DynamicMode_Gate = 3;

    static constexpr int TriggerSide_Above = 0;
    static constexpr int TriggerSide_Below = 1;

    static constexpr int DetectorSource_InternalWideband = 0;
    static constexpr int DetectorSource_InternalFiltered = 1;
    static constexpr int DetectorSource_ExternalWideband = 2;
    static constexpr int DetectorSource_ExternalFiltered = 3;

    enum class DetectorAvailability : uint8_t
    {
        Internal = 0,
        ExternalAvailable,
        ExternalUnavailable
    };
    
    //==============================================================================
    // Atomic parameters for each band (lock-free, trivially copyable)
    struct alignas(AIEQCore::kCacheLineSize) AtomicDynamicBandParams
    {
        // EQ parameters
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gain { 0.0f };
        std::atomic<float> q { 1.0f };
        std::atomic<int> filterType { 2 };  // Peak
        std::atomic<bool> enabled { true };
        
        // Dynamic parameters
        std::atomic<int> dynamicMode { DynamicMode_Off };
        std::atomic<float> threshold { -20.0f };
        std::atomic<float> ratio { 2.0f };
        std::atomic<float> attackMs { 10.0f };
        std::atomic<float> releaseMs { 100.0f };
        std::atomic<float> range { 24.0f };
        std::atomic<float> knee { 6.0f };
        std::atomic<int> detection { DetectionMode_RMS };
        std::atomic<int> triggerSide { TriggerSide_Above };
        std::atomic<int> detectorSource { DetectorSource_InternalWideband };
        
        // Compatibility switch for direct API clients that historically used
        // sidechainEnabled=true to request the internal detector band-pass.
        std::atomic<bool> sidechainEnabled { false };
        std::atomic<float> sidechainFreq { 1000.0f };
        std::atomic<float> sidechainQ { 1.0f };
        
        // Version counter for change detection
        std::atomic<uint64_t> version { 0 };
    };
    
    //==============================================================================
    // Plain struct for API compatibility (non-atomic version for get/set)
    struct DynamicBandParams
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int filterType = 2;
        bool enabled = true;
        
        int dynamicMode = DynamicMode_Off;
        float threshold = -20.0f;
        float ratio = 2.0f;
        float attackMs = 10.0f;
        float releaseMs = 100.0f;
        float range = 24.0f;
        float knee = 6.0f;
        int detection = DetectionMode_RMS;
        int triggerSide = TriggerSide_Above;
        int detectorSource = DetectorSource_InternalWideband;
        
        bool sidechainEnabled = false;
        float sidechainFreq = 1000.0f;
        float sidechainQ = 1.0f;
    };
    
    //==============================================================================
    struct BandMeter
    {
        float inputLevel = -160.0f;
        float gainReduction = 0.0f;
        float outputLevel = -100.0f;
    };

    //==============================================================================
    DynamicEQProcessor();
    ~DynamicEQProcessor() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    [[nodiscard]] bool canReconfigureWithoutAllocation(double sampleRate,
                                                       int samplesPerBlock,
                                                       int numChannels) const noexcept;
    [[nodiscard]] bool reconfigureNoAllocation(double sampleRate,
                                               int samplesPerBlock,
                                               int numChannels) noexcept;
    void setHighPrecisionMode(bool enabled) noexcept
    {
        highPrecisionMode.store(enabled, std::memory_order_release);
    }
    [[nodiscard]] bool isHighPrecisionMode() const noexcept
    {
        return highPrecisionMode.load(std::memory_order_acquire);
    }
    [[nodiscard]] BiquadValidationFailure getBandValidationFailure(int index) const noexcept
    {
        if (index < 0 || index >= maxBands)
            return BiquadValidationFailure::InvalidDomain;
        return static_cast<BiquadValidationFailure>(
            bandValidationFailures[static_cast<size_t>(index)].load(std::memory_order_acquire));
    }
    [[nodiscard]] BiquadValidationFailure getSidechainValidationFailure(int index) const noexcept
    {
        if (index < 0 || index >= maxBands)
            return BiquadValidationFailure::InvalidDomain;
        return static_cast<BiquadValidationFailure>(
            sidechainValidationFailures[static_cast<size_t>(index)].load(std::memory_order_acquire));
    }
    [[nodiscard]] DetectorAvailability getDetectorAvailability(int index) const noexcept
    {
        if (index < 0 || index >= maxBands)
            return DetectorAvailability::ExternalUnavailable;
        return static_cast<DetectorAvailability>(
            detectorAvailability[static_cast<size_t>(index)].load(std::memory_order_acquire));
    }
    void reset();
    void process(juce::AudioBuffer<float>& buffer,
                 const juce::AudioBuffer<float>* externalDetector = nullptr);
    
    //==============================================================================
    // Band management (all lock-free)
    void setBandParams(int bandIndex, const DynamicBandParams& params);
    [[nodiscard]] DynamicBandParams getBandParams(int bandIndex) const;
    void setBandEnabled(int bandIndex, bool enabled);
    void setDynamicMode(int bandIndex, int mode);
    
    //==============================================================================
    // Global controls (atomic)
    void setGlobalMix(float mix) { globalMix.store(juce::jlimit(0.0f, 1.0f, mix), std::memory_order_relaxed); }
    [[nodiscard]] float getGlobalMix() const { return globalMix.load(std::memory_order_relaxed); }
    
    void setAutoMakeup(bool enabled) { autoMakeupEnabled.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool isAutoMakeupEnabled() const { return autoMakeupEnabled.load(std::memory_order_relaxed); }
    
    void setLookahead(float ms);
    [[nodiscard]] float getLookahead() const { return lookaheadMs.load(std::memory_order_relaxed); }

    void updateLookaheadBuffer(double sampleRate, int samplesPerBlock, int channels);

    [[nodiscard]] int getDryBufferCapacityForTests() const noexcept { return dryBuffer.getNumSamples(); }
    [[nodiscard]] int getLookaheadBufferCapacityForTests() const noexcept { return lookaheadBuffer.getNumSamples(); }
    [[nodiscard]] int getLookaheadSamplesForTests() const noexcept { return lookaheadSamples.load(std::memory_order_relaxed); }
    
    //==============================================================================
    // Metering
    [[nodiscard]] BandMeter getBandMeter(int bandIndex) const;
    [[nodiscard]] float getTotalGainReduction() const;
    
    //==============================================================================
    // Magnitude response (for GUI curve drawing)
    [[nodiscard]] float getMagnitudeForFrequency(float freq, double sampleRate) const;
    // Returns the dB delta that must be added to the displayed static EQ curve
    // for bands owned by the dynamic stage. This is replacement semantics, not
    // an additive extra stage: the GUI uses it to swap the static white band
    // shape with the audible live DynEQ transfer function.
    void evaluateDynamicReplacementDeltaDbForFrequencyArray(
        const float* frequenciesHz,
        float* deltaDbOut,
        size_t numPoints,
        double sampleRate) const noexcept;

private:
    //==============================================================================
    static constexpr int dynamicWarmupHistorySamples = 64;

    // Processing state for each band (audio thread only)
    struct BandState
    {
        BiquadCoeffs eqCoeffs;
        BiquadCoeffs scCoeffs;

        std::array<BiquadState, 2> eqFiltersL;
        std::array<BiquadState, 2> eqFiltersR;
        BiquadState scFilterL, scFilterR;
        
        // Peak mode stores a smoothed linear amplitude; RMS mode stores
        // smoothed power. Conversion to dB happens only after smoothing.
        double detectorEnvelope = 0.0;
        float currentGain = 0.0f;
        float staticFreqApplied = 0.0f;
        float staticGainApplied = 0.0f;
        float staticQApplied = 1.0f;
        int staticFilterTypeApplied = -1;
        float scFreqApplied = 0.0f;
        float scQApplied = 1.0f;
        bool sidechainEnabledApplied = false;
        int detectorSourceApplied = -1;
        int dynamicModeApplied = -1;
        int detectionModeApplied = -1;
        
        // Cached attack/release coefficients
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        
        // Metering (atomic for thread-safe reads)
        std::atomic<float> meterInputLevel { -160.0f };
        std::atomic<float> meterGainReduction { 0.0f };
        std::atomic<float> meterOutputLevel { -100.0f };

        // End-of-block mirrors of the values actually applied to audio.
        // The GUI live-curve evaluator consumes these instead of the
        // detector-domain meter values, which intentionally precede the
        // coefficient update cadence and its continuity crossfade.
        std::atomic<float> liveCurrentGainDb { 0.0f };
        std::atomic<float> liveEffectiveGainDb { 0.0f };
        float appliedEffectiveGainDb = 0.0f;

        std::array<float, dynamicWarmupHistorySamples> inputHistoryL {};
        std::array<float, dynamicWarmupHistorySamples> inputHistoryR {};
        int inputHistoryWritePos = 0;
        int inputHistoryCount = 0;

        // Hard rate-limit counter for coefficient rebuilds. Reset to 0 on every
        // rebuild trigger; incremented every sample in the dynamic branch.
        // Together with the raised epsilon, prevents the rebuild cascade that
        // was the audible crackle source under continuous compression.
        int samplesSinceLastRebuild = 0;

        uint64_t lastVersion = 0;
        bool prepared = false;
    };
    
    void updateBandCoefficients(int bandIndex);
    void updateDynamicTargets(int bandIndex);
    void updateSidechainState(int bandIndex);
    void updateAttackReleaseCoeffs(int bandIndex);
    void resetRuntimeStateNoAllocation(double sampleRate, int samplesPerBlock, int channels) noexcept;
    void pushBandInputHistory(BandState& state, float inputL, float inputR) noexcept;
    void warmBandFiltersFromHistory(BandState& state, const BiquadCoeffs& coeffs) noexcept;
    [[nodiscard]] float calculateDynamicGain(float inputLevelDb,
                                             int dynMode,
                                             int triggerSide,
                                             float staticGainDb,
                                             float threshold,
                                             float ratio,
                                             float knee,
                                             float range) const;
    [[nodiscard]] float computeAutoMakeupGainLinear() const noexcept;

    [[nodiscard]] BiquadCoeffs makeEQCoefficients(
        int filterType, float freq, float gain, float q) const;
    void beginCoeffCrossfade(int bandIndex,
                             const BiquadCoeffs& newCoeffs,
                             int fadeSamples) noexcept;

    // Per-band output crossfade: eliminates biquad coefficient-jump clicks.
    // Mirrors ParametricEQProcessor::beginBandCrossfade() approach.
    struct BandCrossfade
    {
        BiquadCoeffs oldCoeffs;
        std::array<BiquadState, 2> oldFiltersL;
        std::array<BiquadState, 2> oldFiltersR;
        int remaining = 0;
        int total = 0;
    };
    std::array<BandCrossfade, maxBands> bandCrossfades {};
    
    //==============================================================================
    // LOCK-FREE ARCHITECTURE
    //==============================================================================
    
    // Atomic parameters for each band
    std::array<AtomicDynamicBandParams, maxBands> bandParams;
    
    // Processing state (audio thread only)
    std::array<BandState, maxBands> bandStates;
    std::array<std::atomic<uint8_t>, maxBands> bandValidationFailures {};
    std::array<std::atomic<uint8_t>, maxBands> sidechainValidationFailures {};
    std::array<std::atomic<uint8_t>, maxBands> detectorAvailability {};
    
    // Smoothed dynamic parameters (per-band, updated on version change)
    std::array<juce::SmoothedValue<float>, maxBands> smoothedThresholds;
    std::array<juce::SmoothedValue<float>, maxBands> smoothedRatios;
    std::array<juce::SmoothedValue<float>, maxBands> smoothedRanges;
    std::array<juce::SmoothedValue<float>, maxBands> smoothedKnees;
    std::array<juce::SmoothedValue<float>, maxBands> smoothedSidechainFreq;
    std::array<juce::SmoothedValue<float>, maxBands> smoothedSidechainQ;
    
    // Global settings (atomic)
    std::atomic<float> globalMix { 1.0f };
    std::atomic<bool> autoMakeupEnabled { false };
    std::atomic<float> lookaheadMs { 0.0f };

    // Audio-thread only: previous block's auto-makeup gain. Used to ramp the
    // makeup application across blocks (applyGainRamp) instead of the previous
    // applyGain() that stamped a single value per block. Block-stepwise gain
    // was a second source of crackle, distinct from the coefficient-rebuild
    // crackle, and is audible when meterGainReduction modulates quickly.
    float lastAppliedMakeupGain = 1.0f;
    float lastAppliedMix = 1.0f;
    
    // Sample rate and block size
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic<bool> highPrecisionMode { false };
    int numChannels = 2;
    
    // Lookahead delay line
    juce::AudioBuffer<float> lookaheadBuffer;
    int lookaheadWritePos = 0;
    std::atomic<int> lookaheadSamples { 0 };

    // Pre-allocated dry buffer
    juce::AudioBuffer<float> dryBuffer;
    
    // Prepare flag
    std::atomic<bool> isPrepared { false };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQProcessor)
};
