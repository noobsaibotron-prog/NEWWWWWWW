#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>

//==============================================================================
/**
 * Multi-Track Unmasking System
 * 
 * Analyzes multiple audio tracks simultaneously to detect and correct
 * frequency masking between tracks. Uses psychoacoustic models and
 * cross-track spectral analysis to identify masked frequencies.
 * 
 * Features:
 * - Cross-track masking detection
 * - Frequency-specific unmasking corrections
 * - Real-time multi-track analysis
 * - Adaptive threshold based on track relationships
 */
class MultiTrackUnmasking
{
public:
    //==========================================================================
    struct TrackInfo
    {
        juce::String trackId;
        juce::String trackName;
        std::vector<float> spectrum;      // Current spectrum (dB)
        float rmsLevel = 0.0f;             // RMS level in dB
        float peakLevel = 0.0f;            // Peak level in dB
        bool isActive = false;             // Is track currently playing
        juce::int64 lastUpdateTime = 0;   // Timestamp of last update
    };
    
    struct MaskingAnalysis
    {
        juce::String maskedTrackId;        // Track being masked
        juce::String maskingTrackId;       // Track causing masking
        float frequency = 0.0f;            // Frequency where masking occurs (Hz)
        float bandwidth = 0.0f;             // Bandwidth of masking (Hz)
        float maskingAmount = 0.0f;        // Amount of masking in dB (0-1 normalized)
        float suggestedGain = 0.0f;        // Suggested gain adjustment for masked track (dB)
        float confidence = 0.0f;            // Confidence in detection (0-1)
    };
    
    struct UnmaskingCorrection
    {
        juce::String trackId;
        float frequency = 0.0f;
        float gain = 0.0f;                 // Gain adjustment in dB
        float q = 1.0f;                    // Q factor for EQ
        float confidence = 0.0f;
    };
    
    //==========================================================================
    MultiTrackUnmasking();
    ~MultiTrackUnmasking() = default;
    
    //==========================================================================
    // Track Management
    void registerTrack(const juce::String& trackId, const juce::String& trackName = "");
    void unregisterTrack(const juce::String& trackId);
    void updateTrackSpectrum(const juce::String& trackId, const std::vector<float>& spectrum);
    void updateTrackLevels(const juce::String& trackId, float rmsLevel, float peakLevel);
    void setTrackActive(const juce::String& trackId, bool active);
    
    //==========================================================================
    // Analysis
    std::vector<MaskingAnalysis> analyzeMasking(double sampleRate);
    std::vector<UnmaskingCorrection> generateCorrections(double sampleRate, float sensitivity = 0.5f);
    
    //==========================================================================
    // Configuration
    void setEnabled(bool enabled) { isEnabled = enabled; }
    bool getEnabled() const { return isEnabled; }
    
    void setSensitivity(float sens) { sensitivity = juce::jlimit(0.0f, 1.0f, sens); }
    float getSensitivity() const { return sensitivity; }
    
    void setMinMaskingThreshold(float threshold) { minMaskingThreshold = threshold; }
    float getMinMaskingThreshold() const { return minMaskingThreshold; }
    
    //==========================================================================
    // Psychoacoustic Model Parameters
    void setCriticalBandwidth(float bandwidth) { criticalBandwidth = bandwidth; }
    float getCriticalBandwidth() const { return criticalBandwidth; }
    
    void setMaskingThresholdOffset(float offset) { maskingThresholdOffset = offset; }
    float getMaskingThresholdOffset() const { return maskingThresholdOffset; }
    
private:
    //==========================================================================
    // Psychoacoustic masking calculation
    float calculateMaskingThreshold(float maskerLevel, float maskerFreq, float probeFreq) const;
    float calculateCriticalBandwidth(float centerFreq) const;
    float calculateSpreadOfMasking(float maskerLevel, float maskerFreq, float probeFreq) const;
    
    //==========================================================================
    // Cross-track analysis
    std::vector<MaskingAnalysis> detectCrossTrackMasking(const std::vector<TrackInfo>& tracks, double sampleRate) const;
    float calculateMaskingAmount(const TrackInfo& masker, const TrackInfo& masked, 
                                 float frequency, double sampleRate) const;
    
    //==========================================================================
    // Frequency analysis
    float findPeakInRange(const std::vector<float>& spectrum, double sampleRate,
                          float minFreq, float maxFreq) const;
    float calculateBandEnergy(const std::vector<float>& spectrum, double sampleRate,
                             float lowFreq, float highFreq) const;
    
    //==========================================================================
    // State
    std::map<juce::String, TrackInfo> registeredTracks;
    mutable std::mutex tracksMutex;
    
    std::atomic<bool> isEnabled{true};
    std::atomic<float> sensitivity{0.5f};
    float minMaskingThreshold = 3.0f;        // Minimum masking in dB to consider
    float criticalBandwidth = 100.0f;         // Default critical bandwidth (Hz)
    float maskingThresholdOffset = 0.0f;       // Offset for masking threshold (dB)
    
    // Analysis cache
    std::vector<MaskingAnalysis> lastAnalysis;
    juce::int64 lastAnalysisTime = 0;
    static constexpr juce::int64 analysisIntervalMs = 100;  // Analyze every 100ms
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiTrackUnmasking)
};

