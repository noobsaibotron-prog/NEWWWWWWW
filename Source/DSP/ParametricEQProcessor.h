#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/LockFreeStructures.h"
#include "BiquadCoefficients.h"
#include <array>
#include <atomic>

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
    
    //==============================================================================
    // Atomic band parameters (lock-free, trivially copyable)
    struct alignas(AIEQCore::kCacheLineSize) AtomicBandParams
    {
        std::atomic<float> frequency { 1000.0f };
        std::atomic<float> gain { 0.0f };
        std::atomic<float> q { 1.0f };
        std::atomic<int> type { static_cast<int>(Peak) };
        std::atomic<bool> enabled { false };
        std::atomic<bool> solo { false };
        std::atomic<bool> vintageMode { false };
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
        int numActiveStages = 1;
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
    };

    //==============================================================================
    ParametricEQProcessor();
    ~ParametricEQProcessor() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
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
    void setBandSolo(int index, bool solo);
    void setBandVintageMode(int index, bool vintage);
    
    void setBandParameters(int index, float freq, float gain, float q, int type);
    void setBandSlope(int index, int slope);  // 0=12, 1=24, 2=48 dB/oct (LowCut/HighCut only)
    
    [[nodiscard]] float getBandFrequency(int index) const;
    [[nodiscard]] float getBandGain(int index) const;
    [[nodiscard]] float getBandQ(int index) const;
    [[nodiscard]] int getBandType(int index) const;
    [[nodiscard]] bool isBandEnabled(int index) const;
    [[nodiscard]] bool isBandSolo(int index) const;
    [[nodiscard]] int getBandSlope(int index) const;
    
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
    void updateCoefficientsForBand(int index);
    
    [[nodiscard]] BiquadCoeffs makeCoefficients(
        FilterType type, float freq, float gain, float q, double sampleRate) const;
    
    //==============================================================================
    // LOCK-FREE ARCHITECTURE
    //==============================================================================
    
    // Atomic parameters for each band (written by message thread, read by audio thread)
    std::array<AtomicBandParams, 24> bandParams;
    
    // Processing state for each band (audio thread only)
    std::array<BandProcessingState, 24> bandStates;
    
    // Number of active bands (atomic)
    std::atomic<int> numActiveBands { 0 };
    
    // Global settings (atomic)
    std::atomic<float> outputGain { 1.0f };
    std::atomic<bool> bypassed { false };
    
    // Sample rate and block size (set in prepare, read in process)
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentBlockSize { 512 };
    int numChannels = 2;
    
    // Prepare flag (ensures safe initialization)
    std::atomic<bool> isPrepared { false };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQProcessor)
};
