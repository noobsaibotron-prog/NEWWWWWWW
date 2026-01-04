#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <mutex>
#include <cmath>

//==============================================================================
/**
 * Dynamic EQ Processor - FabFilter Pro-Q / TDR Nova Style
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
    static constexpr int maxBands = 8;
    
    enum class DetectionMode
    {
        Peak,
        RMS
    };
    
    enum class DynamicMode
    {
        Off,        // Static EQ (traditional)
        Compress,   // Reduce gain when above threshold
        Expand,     // Increase gain when above threshold (upward)
        Gate        // Cut when below threshold
    };
    
    //==============================================================================
    struct DynamicBandParams
    {
        // EQ parameters
        float frequency = 1000.0f;      // Hz
        float gain = 0.0f;              // dB (static gain, target for dynamic)
        float q = 1.0f;
        int filterType = 2;             // 0=LowCut, 1=LowShelf, 2=Peak, 3=HighShelf, 4=HighCut, 5=Notch, 6=BandPass
        bool enabled = true;
        
        // Dynamic parameters
        DynamicMode dynamicMode = DynamicMode::Off;
        float threshold = -20.0f;       // dB - level at which dynamics kick in
        float ratio = 2.0f;             // 1:1 to inf:1 (1 = no compression)
        float attackMs = 10.0f;         // ms
        float releaseMs = 100.0f;       // ms
        float range = 24.0f;            // dB - max gain reduction/boost
        float knee = 6.0f;              // dB - soft knee width (0 = hard)
        DetectionMode detection = DetectionMode::RMS;
        
        // Sidechain (internal)
        bool sidechainEnabled = false;
        float sidechainFreq = 1000.0f;  // Listen to this freq for detection
        float sidechainQ = 1.0f;
    };
    
    //==============================================================================
    struct BandMeter
    {
        float inputLevel = -100.0f;     // dB
        float gainReduction = 0.0f;     // dB (negative = reduction)
        float outputLevel = -100.0f;    // dB
    };

    //==============================================================================
    DynamicEQProcessor();
    ~DynamicEQProcessor() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Band management
    void setBandParams(int bandIndex, const DynamicBandParams& params);
    DynamicBandParams getBandParams(int bandIndex) const;
    void setBandEnabled(int bandIndex, bool enabled);
    void setDynamicMode(int bandIndex, DynamicMode mode);
    
    //==============================================================================
    // Global controls
    void setGlobalMix(float mix) { globalMix = juce::jlimit(0.0f, 1.0f, mix); }
    float getGlobalMix() const { return globalMix; }
    
    void setAutoMakeup(bool enabled) { autoMakeupEnabled = enabled; }
    bool isAutoMakeupEnabled() const { return autoMakeupEnabled; }
    
    void setLookahead(float ms) { lookaheadMs = juce::jlimit(0.0f, 20.0f, ms); }
    float getLookahead() const { return lookaheadMs; }

    // Recompute lookahead buffer when lookahead changes (e.g., quality mode switch)
    void updateLookaheadBuffer(double sampleRate, int samplesPerBlock, int channels);
    
    //==============================================================================
    // Metering
    BandMeter getBandMeter(int bandIndex) const;
    float getTotalGainReduction() const;
    
    //==============================================================================
    // Magnitude response (for GUI curve drawing)
    float getMagnitudeForFrequency(float freq, double sampleRate) const;

private:
    //==============================================================================
    struct BandState
    {
        // Filter coefficients
        juce::dsp::IIR::Coefficients<float>::Ptr eqCoeffs;
        juce::dsp::IIR::Coefficients<float>::Ptr scCoeffs; // Sidechain filter
        
        // Per-channel filter states
        std::array<juce::dsp::IIR::Filter<float>, 2> eqFiltersL;
        std::array<juce::dsp::IIR::Filter<float>, 2> eqFiltersR;
        juce::dsp::IIR::Filter<float> scFilterL, scFilterR;
        
        // Envelope follower state (per channel)
        float envelopeL = 0.0f;
        float envelopeR = 0.0f;
        
        // Current dynamic gain (smoothed)
        float currentGain = 0.0f;
        float targetGain = 0.0f;
        
        // Metering
        BandMeter meter;
        
        // Coefficients need update flag
        bool needsUpdate = true;
    };
    
    void updateBandCoefficients(int bandIndex);
    float calculateDynamicGain(float inputLevelDb, const DynamicBandParams& params) const;
    float computeSoftKnee(float inputDb, float threshold, float ratio, float knee) const;
    
    juce::dsp::IIR::Coefficients<float>::Ptr makeEQCoefficients(
        int filterType, float freq, float gain, float q) const;
    
    //==============================================================================
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int numChannels = 2;
    
    std::array<DynamicBandParams, maxBands> bandParams;
    std::array<BandState, maxBands> bandStates;
    mutable std::mutex paramsMutex;
    
    // Global settings
    float globalMix = 1.0f;
    bool autoMakeupEnabled = false;
    float lookaheadMs = 0.0f;
    
    // Lookahead delay line
    juce::AudioBuffer<float> lookaheadBuffer;
    int lookaheadWritePos = 0;
    int lookaheadSamples = 0;
    
    // Smoothing coefficients (calculated from attack/release)
    std::array<float, maxBands> attackCoeffs;
    std::array<float, maxBands> releaseCoeffs;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQProcessor)
};

