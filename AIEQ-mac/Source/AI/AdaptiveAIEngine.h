#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>

// Forward declaration to avoid circular dependency
class AIEngine;

//==============================================================================
/**
 * Adaptive AI Engine for Extreme Dynamic Signals
 * 
 * Adapts detection and correction algorithms based on signal characteristics:
 * - Extreme dynamics (very quiet to very loud)
 * - Rapid level changes
 * - Transient-heavy content
 * - Sparse signals (long silences)
 * 
 * Features:
 * - Dynamic threshold adaptation
 * - Transient-aware processing
 * - Sparse signal handling
 * - Level-dependent sensitivity
 * - Adaptive temporal smoothing
 */
class AdaptiveAIEngine
{
public:
    //==========================================================================
    enum class SignalCharacteristic
    {
        Normal,             // Normal dynamic range
        ExtremeDynamic,     // Very wide dynamic range
        TransientHeavy,    // Many transients
        Sparse,            // Long silences, sparse content
        Compressed,        // Highly compressed (low DR)
        Overdriven         // Clipping/overdrive present
    };
    
    struct SignalAnalysis
    {
        SignalCharacteristic characteristic = SignalCharacteristic::Normal;
        float dynamicRange = 0.0f;          // Dynamic range in dB
        float peakToRMS = 0.0f;             // Peak-to-RMS ratio in dB
        float transientDensity = 0.0f;      // Transients per second
        float silenceRatio = 0.0f;          // Ratio of silence (0-1)
        float compressionRatio = 0.0f;      // Estimated compression (0-1)
        float clippingAmount = 0.0f;        // Amount of clipping (0-1)
        float confidence = 0.0f;            // Confidence in analysis (0-1)
    };
    
    struct AdaptiveConfig
    {
        float detectionThreshold = 0.5f;     // Base detection threshold
        float temporalSmoothing = 0.1f;      // Temporal smoothing factor
        float sensitivity = 0.5f;            // Base sensitivity
        bool enableTransientMode = true;     // Enable transient-aware processing
        bool enableSparseMode = true;        // Enable sparse signal handling
    };
    
    //==========================================================================
    AdaptiveAIEngine();
    ~AdaptiveAIEngine() = default;
    
    //==========================================================================
    // Signal Analysis
    SignalAnalysis analyzeSignal(const juce::AudioBuffer<float>& buffer, double sampleRate);
    SignalAnalysis analyzeSignal(const std::vector<float>& spectrum, double sampleRate);
    
    //==========================================================================
    // Adaptive Configuration
    AdaptiveConfig getAdaptiveConfig(const SignalAnalysis& analysis) const;
    void applyAdaptiveConfig(AIEngine& aiEngine, const AdaptiveConfig& config);
    
    //==========================================================================
    // Transient Detection
    struct Transient
    {
        juce::int64 samplePosition = 0;
        float amplitude = 0.0f;
        float frequency = 0.0f;              // Dominant frequency
        float duration = 0.0f;                // Duration in seconds
    };
    
    std::vector<Transient> detectTransients(const juce::AudioBuffer<float>& buffer, 
                                           double sampleRate);
    
    //==========================================================================
    // Sparse Signal Handling
    bool isSparseSignal(const juce::AudioBuffer<float>& buffer, 
                       double sampleRate,
                       float silenceThreshold = -60.0f) const;
    
    float calculateSilenceRatio(const juce::AudioBuffer<float>& buffer,
                               double sampleRate,
                               float silenceThreshold = -60.0f) const;
    
    //==========================================================================
    // Dynamic Range Analysis
    float calculateDynamicRange(const juce::AudioBuffer<float>& buffer) const;
    float calculatePeakToRMS(const juce::AudioBuffer<float>& buffer) const;
    float estimateCompressionRatio(const juce::AudioBuffer<float>& buffer) const;
    
    //==========================================================================
    // Configuration
    void setEnabled(bool enabled) { isEnabled = enabled; }
    bool getEnabled() const { return isEnabled; }
    
    void setAdaptationSpeed(float speed) { adaptationSpeed = juce::jlimit(0.0f, 1.0f, speed); }
    float getAdaptationSpeed() const { return adaptationSpeed; }
    
    void setMinDynamicRange(float minDR) { minDynamicRange = minDR; }
    float getMinDynamicRange() const { return minDynamicRange; }
    
    void setMaxDynamicRange(float maxDR) { maxDynamicRange = maxDR; }
    float getMaxDynamicRange() const { return maxDynamicRange; }
    
private:
    //==========================================================================
    // Internal analysis
    SignalCharacteristic classifySignal(const SignalAnalysis& analysis) const;
    float calculateTransientDensity(const std::vector<Transient>& transients, double duration) const;
    
    //==========================================================================
    // History for temporal analysis
    struct AnalysisFrame
    {
        SignalAnalysis analysis;
        juce::int64 timestamp = 0;
    };
    
    std::deque<AnalysisFrame> analysisHistory;
    static constexpr size_t maxHistorySize = 100;  // ~10 seconds at 10 FPS
    
    //==========================================================================
    // State
    std::atomic<bool> isEnabled{true};
    std::atomic<float> adaptationSpeed{0.5f};
    float minDynamicRange = 10.0f;          // Minimum DR to consider "dynamic"
    float maxDynamicRange = 60.0f;          // Maximum DR to consider "extreme"
    
    mutable std::mutex historyMutex;
    
    // Transient detection parameters
    float transientThreshold = 0.3f;         // Threshold for transient detection
    float transientMinDuration = 0.001f;     // Minimum transient duration (1ms)
    float transientMaxDuration = 0.1f;      // Maximum transient duration (100ms)

    // FFT resources for transient frequency estimation (preallocated).
    static constexpr int transientFftOrder = 10;            // 1024-point FFT
    static constexpr int transientFftSize  = 1 << transientFftOrder;
    juce::dsp::FFT transientFft { transientFftOrder };
    std::vector<float> transientFftBuffer;  // size 2 * transientFftSize (real+imag)
    std::vector<float> transientWindow;     // Hann window
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdaptiveAIEngine)
};

