#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/LockFreeStructures.h"
#include "BiquadCoefficients.h"
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
        
        // Sidechain
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
        
        bool sidechainEnabled = false;
        float sidechainFreq = 1000.0f;
        float sidechainQ = 1.0f;
    };
    
    //==============================================================================
    struct BandMeter
    {
        float inputLevel = -100.0f;
        float gainReduction = 0.0f;
        float outputLevel = -100.0f;
    };

    //==============================================================================
    DynamicEQProcessor();
    ~DynamicEQProcessor() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
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
    
    //==============================================================================
    // Metering
    [[nodiscard]] BandMeter getBandMeter(int bandIndex) const;
    [[nodiscard]] float getTotalGainReduction() const;
    
    //==============================================================================
    // Magnitude response (for GUI curve drawing)
    [[nodiscard]] float getMagnitudeForFrequency(float freq, double sampleRate) const;

private:
    //==============================================================================
    // Processing state for each band (audio thread only)
    struct BandState
    {
        BiquadCoeffs eqCoeffs;
        BiquadCoeffs scCoeffs;

        std::array<BiquadState, 2> eqFiltersL;
        std::array<BiquadState, 2> eqFiltersR;
        BiquadState scFilterL, scFilterR;
        
        float envelopeL = 0.0f;
        float envelopeR = 0.0f;
        float currentGain = 0.0f;
        float targetGain = 0.0f;
        float scFreqApplied = 0.0f;
        float scQApplied = 1.0f;
        
        // Cached attack/release coefficients
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        
        // Metering (atomic for thread-safe reads)
        std::atomic<float> meterInputLevel { -100.0f };
        std::atomic<float> meterGainReduction { 0.0f };
        std::atomic<float> meterOutputLevel { -100.0f };
        
        uint64_t lastVersion = 0;
        bool prepared = false;
    };
    
    void updateBandCoefficients(int bandIndex);
    void updateAttackReleaseCoeffs(int bandIndex);
    [[nodiscard]] float calculateDynamicGain(float inputLevelDb,
                                             int dynMode,
                                             float threshold,
                                             float ratio,
                                             float knee,
                                             float range) const;
    [[nodiscard]] float computeSoftKnee(float inputDb, float threshold, float ratio, float knee) const;
    
    [[nodiscard]] BiquadCoeffs makeEQCoefficients(
        int filterType, float freq, float gain, float q) const;
    
    //==============================================================================
    // LOCK-FREE ARCHITECTURE
    //==============================================================================
    
    // Atomic parameters for each band
    std::array<AtomicDynamicBandParams, maxBands> bandParams;
    
    // Processing state (audio thread only)
    std::array<BandState, maxBands> bandStates;
    
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
    
    // Sample rate and block size
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
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
