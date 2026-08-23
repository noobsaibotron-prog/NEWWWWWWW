#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/LockFreeStructures.h"
#include "BiquadCoefficients.h"
#include "CutFilterDesigner.h"
#include "TptSvf.h"
#include <array>
#include <atomic>
#include <cstdint>

//==============================================================================
/**
 * Parametric EQ Processor - Multi-band parametric equalizer
 * 
 * LOCK-FREE ARCHITECTURE (Elite/Top-Tier Standard):
 * - Zero mutex in audio path
 * - Atomic parameters with version counter
 * - Wait-free reads from audio thread
 * - Lock-free writes from message thread
 * 
 * Features:
 * - Up to 24 fully parametric bands
 * - Multiple filter types per band
 * - Real-time coefficient updates
 * - Magnitude response calculation
 * - Zero latency processing
 */
class ParametricEQProcessor
{
public:
    //==============================================================================
    // Filter types
    enum FilterType
    {
        LowCut = 0,     // High-pass filter
        LowShelf,       // Low shelf
        Peak,           // Parametric peak/notch
        HighShelf,      // High shelf
        HighCut,        // Low-pass filter
        Notch,          // Notch filter
        BandPass,       // Band-pass filter
        VintageLowShelf, // Pultec-style low shelf with analog modeling
        VintageHighShelf // Pultec-style high shelf with analog modeling
    };

    enum class CurveMode : uint8_t
    {
        Legacy = 0,
        Surgical
    };
    
    //==============================================================================
    // Atomic band parameters (lock-free, trivially copyable)
    struct alignas(AIEQCore::kCacheLineSize) AtomicBandParams
    {
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gain { 0.0f };
        std::atomic<float> q { 1.0f };
        std::atomic<int> type { static_cast<int>(Peak) };
        std::atomic<bool> enabled { false };
        std::atomic<bool> audioBypass { false };  // When true, band is skipped in audio processing
                                                   // but still visible in magnitude response (GUI curve).
                                                   // Used by DynEQ: static EQ bypasses band while
                                                   // DynamicEQProcessor handles it dynamically.
        std::atomic<bool> solo { false };
        std::atomic<bool> vintageMode { false };
        std::atomic<uint8_t> curveMode { static_cast<uint8_t>(CurveMode::Legacy) };
        std::atomic<int> slope { 0 };  // 0=12dB/oct, 1=24dB/oct, 2=48dB/oct (LowCut/HighCut only)
        std::atomic<uint64_t> version { 0 };  // Incremented on any change
        
        void set(float freq, float g, float qVal, int t, bool en, bool s = false, bool vintage = false, int sl = 0) noexcept
        {
            frequency.store(freq, std::memory_order_relaxed);
            gain.store(g, std::memory_order_relaxed);
            q.store(qVal, std::memory_order_relaxed);
            type.store(t, std::memory_order_relaxed);
            enabled.store(en, std::memory_order_relaxed);
            solo.store(s, std::memory_order_relaxed);
            vintageMode.store(vintage, std::memory_order_relaxed);
            slope.store(juce::jlimit(0, 2, sl), std::memory_order_relaxed);
            version.fetch_add(1, std::memory_order_release);
        }
    };
    
    //==============================================================================
    // Processing state for each band (audio thread only, no sharing)
    // Uses stack-allocated BiquadCoeffs/BiquadState — zero heap allocations
    struct BandProcessingState
    {
        static constexpr int maxFilterStages = 4;
        std::array<BiquadState, maxFilterStages> filtersL;
        std::array<BiquadState, maxFilterStages> filtersR;
        std::array<BiquadCoeffs, maxFilterStages> coefficients;
        AIEQDSP::TptSvfState tptStateL;
        AIEQDSP::TptSvfState tptStateR;
        AIEQDSP::TptSvfCoefficients tptCoefficients;
        int numActiveStages = 1;
        bool useTpt = false;
        bool appliedVintage = false;
        uint64_t lastVersion = 0;  // Track when coefficients need update
        bool prepared = false;
    };
    
    //==============================================================================
    // Public read-only band info (for GUI)
    struct BandInfo
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int type = static_cast<int>(Peak);
        bool enabled = false;
        bool solo = false;
        bool vintageMode = false;
        CurveMode curveMode = CurveMode::Legacy;
    };

    //==============================================================================
    ParametricEQProcessor();
    ~ParametricEQProcessor() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
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
        if (index < 0 || index >= getMaxBands())
            return BiquadValidationFailure::InvalidDomain;
        return static_cast<BiquadValidationFailure>(
            bandValidationFailures[static_cast<size_t>(index)].load(std::memory_order_acquire));
    }
    [[nodiscard]] uint64_t getNumericalFaultCount() const noexcept
    {
        return numericalFaultCount.load(std::memory_order_acquire);
    }
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Band management (all lock-free)
    int addBand(float freq, float gain, float q, int type);
    void removeBand(int index);
    void clearAllBands();
    
    [[nodiscard]] int getNumBands() const noexcept { return numActiveBands.load(std::memory_order_acquire); }
    static constexpr int getMaxBands() { return 24; }
    
    //==============================================================================
    // Band parameters (all lock-free, wait-free)
    void setBandFrequency(int index, float freq);
    void setBandGain(int index, float gain);
    void setBandQ(int index, float q);
    void setBandType(int index, int type);
    void setBandEnabled(int index, bool enabled);
    void setBandAudioBypass(int index, bool bypass);  // Bypass audio only, keep visual
    void setBandSolo(int index, bool solo);
    void setBandVintageMode(int index, bool vintage);
    void setBandCurveMode(int index, CurveMode mode);
    
    void setBandParameters(int index, float freq, float gain, float q, int type);
    void setBandSlope(int index, int slope);  // 0=12, 1=24, 2=48 dB/oct (LowCut/HighCut only)
    void clearBandFilterState(int index) noexcept;  // Zero biquad v1/v2 for clean topology change

    /// Begin a per-band output crossfade: saves current coefficients + filter state,
    /// resets the live state, and crossfades old→new over fadeSamples.
    /// Call BEFORE setBandParameters() updates the type.
    void beginBandCrossfade(int index, int fadeSamples = 128) noexcept;

    /// Begin a whole-chain crossfade: saves ALL active bands' coefficients + filter state,
    /// resets live state, and in process() runs BOTH old chain and new chain, blending at output.
    /// Use for A/B switch where per-band crossfade in cascade doesn't work correctly.
    void beginWholeChainCrossfade(int fadeSamples = 2048) noexcept;

    [[nodiscard]] float getBandFrequency(int index) const;
    [[nodiscard]] float getBandGain(int index) const;
    [[nodiscard]] float getBandQ(int index) const;
    [[nodiscard]] int getBandType(int index) const;
    [[nodiscard]] bool isBandEnabled(int index) const;
    [[nodiscard]] bool isBandSolo(int index) const;
    [[nodiscard]] int getBandSlope(int index) const;
    [[nodiscard]] CurveMode getBandCurveMode(int index) const;
    
    // Get complete band info (for GUI display)
    [[nodiscard]] BandInfo getBandInfo(int index) const;
    
    //==============================================================================
    // Magnitude response (for GUI curve drawing)
    [[nodiscard]] float getMagnitudeForFrequency(float freq, double sampleRate) const;
    void getMagnitudeForFrequencyArray(const float* frequencies, float* magnitudes,
                                        size_t numPoints, double sampleRate) const;
    // Like getMagnitudeForFrequencyArray but adds gainOffsets[bandIdx] to each band's gain.
    // Used by dynamic EQ overlay to compute the "live" curve with GR applied per band.
    void getMagnitudeForFrequencyArrayWithGainOffsets(const float* frequencies, float* magnitudes,
                                                       size_t numPoints, double sampleRate,
                                                       const float* gainOffsets, int numOffsets) const;
    
    //==============================================================================
    // Global controls (atomic, lock-free)
    void setOutputGain(float gainDB) { outputGain.store(juce::Decibels::decibelsToGain(gainDB), std::memory_order_relaxed); }
    [[nodiscard]] float getOutputGain() const { return juce::Decibels::gainToDecibels(outputGain.load(std::memory_order_relaxed)); }
    
    void setBypass(bool shouldBypass) { bypassed.store(shouldBypass, std::memory_order_relaxed); }
    [[nodiscard]] bool isBypassed() const { return bypassed.load(std::memory_order_relaxed); }

private:
    //==============================================================================
    // CROSSFADE COMMAND QUEUE (message → audio thread handoff)
    //
    // beginBandCrossfade / beginWholeChainCrossfade are called from the
    // message thread but need to mutate bandStates[] / bandCrossfades[] /
    // wholeChainXfade which are audio-thread-only. Direct mutation would
    // race with process() under the C++ memory model.
    //
    // Solution: message thread pushes a command, audio thread drains the
    // queue and executes the setup on-thread at the top of process().
    //
    // Addresses audit finding CRIT-1 (2026-04).
    //==============================================================================
    struct CrossfadeCommand
    {
        enum Kind : uint8_t { PerBand = 0, WholeChain = 1 };
        Kind kind = PerBand;
        int  bandIndex = 0;     // ignored when kind == WholeChain
        int  fadeSamples = 128;
    };

    // 32 slots is ample for realistic message-thread burst rates (UI drag,
    // preset recall, automation). If the queue fills, tryPush returns false
    // and the crossfade is dropped — worst case is a single audible click
    // on the dropped event, which is strictly better than the prior race.
    AIEQCore::SPSCQueue<CrossfadeCommand, 32> crossfadeCommands;

    // Audio-thread-only executors (formerly the bodies of the public methods).
    void executePerBandCrossfadeSetup(int index, int fadeSamples) noexcept;
    void executeWholeChainCrossfadeSetup(int fadeSamples) noexcept;

    //==============================================================================
    void updateCoefficientsForBand(int index);
    void resetRuntimeStateNoAllocation(double sampleRate, int samplesPerBlock, int channels) noexcept;
    [[nodiscard]] float processBiquadWithFault(BiquadState& state, float input,
                                               const BiquadCoeffs& coefficients) noexcept;
    [[nodiscard]] float processFilterPath(
        bool useTpt,
        AIEQDSP::TptSvfState& tptState,
        const AIEQDSP::TptSvfCoefficients& tptCoefficients,
        std::array<BiquadState, BandProcessingState::maxFilterStages>& filters,
        const std::array<BiquadCoeffs, BandProcessingState::maxFilterStages>& coefficients,
        int numStages,
        float input) noexcept;
    [[nodiscard]] float applyVintageSaturation(float input) noexcept;
    
    [[nodiscard]] BiquadCoeffs makeCoefficients(
        FilterType type, float freq, float gain, float q, double sampleRate) const;

    struct FilterDesign
    {
        std::array<BiquadCoeffs, BandProcessingState::maxFilterStages> coefficients {};
        AIEQDSP::TptSvfCoefficients tptCoefficients {};
        int numStages = 0;
        bool useTpt = false;
        BiquadValidationFailure failure = BiquadValidationFailure::IntentionalBypass;
    };

    [[nodiscard]] FilterDesign makeFilterDesign(
        FilterType type, float freq, float gain, float q, int slope,
        double sampleRate, CurveMode curveMode = CurveMode::Legacy) const;
    [[nodiscard]] static double getFilterDesignMagnitude(
        const FilterDesign& design, double frequency, double sampleRate) noexcept;
    
    //==============================================================================
    // LOCK-FREE ARCHITECTURE
    //==============================================================================
    
    // Atomic parameters for each band (written by message thread, read by audio thread)
    std::array<AtomicBandParams, 24> bandParams;
    
    // Processing state for each band (audio thread only)
    std::array<BandProcessingState, 24> bandStates;
    std::array<std::atomic<uint8_t>, 24> bandValidationFailures {};
    std::atomic<uint64_t> numericalFaultCount { 0 };

    // Per-band crossfade state for topology changes (audio thread only)
    struct BandCrossfade
    {
        static constexpr int maxFilterStages = BandProcessingState::maxFilterStages;
        std::array<BiquadCoeffs, maxFilterStages> oldCoeffs;
        std::array<BiquadState, maxFilterStages> oldFiltersL;
        std::array<BiquadState, maxFilterStages> oldFiltersR;
        AIEQDSP::TptSvfState oldTptStateL;
        AIEQDSP::TptSvfState oldTptStateR;
        AIEQDSP::TptSvfCoefficients oldTptCoefficients;
        int oldNumStages = 1;
        bool oldUseTpt = false;
        bool oldVintage = false;
        int remaining = 0;   // samples left in crossfade
        int total = 0;       // total crossfade length
    };
    std::array<BandCrossfade, 24> bandCrossfades {};

    // Whole-chain crossfade state (for A/B switch)
    // When active, process() runs entire old chain + entire new chain, blends at output.
    struct WholeChainCrossfade
    {
        int remaining = 0;
        int total = 0;
        int oldNumBands = 0;
        // Snapshot of ALL bands' old state (coefficients + filter state)
        struct OldBand
        {
            static constexpr int maxFilterStages = BandProcessingState::maxFilterStages;
            std::array<BiquadCoeffs, maxFilterStages> coeffs;
            std::array<BiquadState, maxFilterStages> filtersL;
            std::array<BiquadState, maxFilterStages> filtersR;
            AIEQDSP::TptSvfState tptStateL;
            AIEQDSP::TptSvfState tptStateR;
            AIEQDSP::TptSvfCoefficients tptCoefficients;
            int numStages = 1;
            bool useTpt = false;
            bool vintage = false;
            bool enabled = false;
            bool solo = false;
        };
        std::array<OldBand, 24> oldBands {};
        bool hadSolo = false;
    };
    WholeChainCrossfade wholeChainXfade {};

    // Number of active bands (atomic)
    std::atomic<int> numActiveBands { 0 };
    
    // Global settings (atomic)
    std::atomic<float> outputGain { 1.0f };
    std::atomic<bool> bypassed { false };
    
    // Sample rate and block size (set in prepare, read in process)
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic<bool> highPrecisionMode { false };
    int numChannels = 2;
    
    // Prepare flag (ensures safe initialization)
    std::atomic<bool> isPrepared { false };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQProcessor)
};
