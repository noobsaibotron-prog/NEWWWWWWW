#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "MLEngine.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <array>

//==============================================================================
/**
 * AI Engine - Enhanced Intelligent frequency problem detection and correction
 * 
 * Features:
 * - Detects: resonances, harshness, muddiness, boxyness, sibilance
 * - Source profiles: Vocals, Drums, Master, Bass, Synth, EDM
 * - Genre detection for electronic music
 * - Analysis history for comparison
 * - Dynamic EQ correction processing
 * - Thread-safe operation
 * 
 * Enhanced v2.0:
 * - Multi-scale peak detection for improved accuracy
 * - Temporal smoothing to reduce false positives
 * - Adaptive thresholds based on signal RMS
 * - Precise bandwidth calculation via -3dB points
 * - Exponential sensitivity curves for finer control
 */
class AIEngine
{
public:
    //==============================================================================
    enum class ProblemType
    {
        None,
        Resonance,      // Sharp peak
        Harshness,      // 1-8 kHz excess
        Muddiness,      // 200-500 Hz buildup
        Boxyness,       // 400-800 Hz resonance
        Sibilance,      // 5-10 kHz excess
        LowEndBoom,     // Sub/bass buildup
        ThinSound,      // Lack of low-mids
        DullSound       // Lack of highs
    };
    
    enum class CorrectionMode
    {
        Off,
        Suggest,
        Gradual,
        Automatic
    };
    
    enum class DetectedGenre
    {
        Unknown,
        Techno,
        Industrial,
        Jungle,
        Breakbeat,
        DubTechno,
        DeepHouse,
        Ambient
    };
    
    //==============================================================================
    // Source profiles for context-aware analysis
    enum class SourceProfile
    {
        Generic,    // Default - balanced detection
        Vocals,     // High sibilance sensitivity, boxyness 300-400Hz
        Drums,      // Sub resonances, less harshness sensitivity
        Bass,       // Focus on sub/low frequencies
        Synth,      // Wide range, resonance focus
        Master,     // Less severe thresholds for full mix
        EDM         // Tolerates more bass/sub, brightness
    };
    
    //==============================================================================
    struct Correction
    {
        ProblemType type = ProblemType::None;
        float frequency = 1000.0f;
        float suggestedGain = 0.0f;
        float suggestedQ = 1.0f;
        float severity = 0.0f;      // 0-1 normalized
        float confidence = 0.0f;    // 0-1 confidence level
        bool approved = false;
        juce::String description;
    };
    
    //==============================================================================
    // Analysis snapshot for history
    struct AnalysisSnapshot
    {
        std::vector<Correction> corrections;
        juce::int64 timestamp;
        DetectedGenre genre;
    };

    //==============================================================================
    AIEngine();
    ~AIEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void analyzeSpectrum(const std::vector<float>& spectrum);
    
    /** 
     * Process corrections by applying dynamic EQ filtering to the buffer.
     * Only processes approved corrections with CorrectionMode != Off.
     */
    void processCorrections(juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Settings
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }
    
    void setSensitivity(float s) { sensitivity = juce::jlimit(0.0f, 1.0f, s); }
    float getSensitivity() const { return sensitivity; }
    
    void setStrength(float s);
    float getStrength() const { return strength; }
    
    void setCorrectionMode(CorrectionMode mode) { correctionMode = mode; }
    CorrectionMode getCorrectionMode() const { return correctionMode; }
    
    // Source profile
    void setSourceProfile(SourceProfile profile);
    SourceProfile getSourceProfile() const { return sourceProfile; }
    static juce::String getProfileName(SourceProfile profile);
    
    //==============================================================================
    // Corrections management
    std::vector<Correction> getPendingCorrections() const;
    std::vector<Correction> getApprovedCorrections() const;
    
    void approveCorrection(int index);
    void approveAllCorrections();
    void rejectCorrection(int index);
    void clearCorrections();
    
    //==============================================================================
    // Analysis state
    bool isNewAnalysisAvailable() const { return newAnalysisAvailable.load(); }
    void clearNewAnalysisFlag() { newAnalysisAvailable = false; }
    
    // Analysis history (last 5 snapshots)
    std::vector<AnalysisSnapshot> getAnalysisHistory() const;
    void clearHistory();
    
    //==============================================================================
    // Genre
    DetectedGenre getDetectedGenre() const { return detectedGenre.load(); }
    
    //==============================================================================
    // Utility
    static juce::String getProblemTypeName(ProblemType type);
    static juce::String getGenreName(DetectedGenre genre);
    static juce::String getBandName(float frequency);  // Human-readable band name
    
    // Get scaled correction (apply strength factor)
    Correction getScaledCorrection(const Correction& c) const;

private:
    void detectProblems();
    void detectResonances(float threshold);
    void detectHarshness(float threshold);
    void detectMuddiness(float threshold);
    void detectBoxyness();
    void detectSibilance();
    void detectLowEndBoom();
    void detectThinSound();
    void detectDullSound();
    void detectGenre();
    
    void saveAnalysisSnapshot();
    void applyProfileThresholds();
    
    float calculateBandEnergy(float lowFreq, float highFreq);
    float calculateBandEnergyUnlocked(float lowFreq, float highFreq) const;  // Internal - caller must hold spectrumMutex
    float findPeakInRange(float lowFreq, float highFreq);     // Find peak frequency in range
    float findLowestInRange(float lowFreq, float highFreq);   // Find lowest energy frequency in range
    float binToFrequency(int bin) const;
    int frequencyToBin(float frequency) const;
    
    //==============================================================================
    // Constants
    static constexpr int fftSize = 4096;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int maxHistorySize = 5;
    static constexpr int maxCorrections = 8;
    static constexpr int maxChannels = 2;
    
    // Detection thresholds and constants
    static constexpr float minGainThreshold = 0.1f;  // Minimum gain to process (dB)
    static constexpr float minSpectrumLevel = -60.0f;  // Minimum spectrum level for analysis (dB)
    static constexpr float minSampleRate = 8000.0f;   // Minimum valid sample rate (Hz)
    static constexpr float maxSampleRate = 192000.0f; // Maximum valid sample rate (Hz)
    static constexpr float minQValue = 0.1f;          // Minimum Q value
    static constexpr float maxQValue = 100.0f;        // Maximum Q value
    static constexpr float minFrequency = 20.0f;      // Minimum frequency (Hz)
    static constexpr float strengthChangeThreshold = 0.01f; // Threshold for strength change detection
    
    double currentSampleRate = 44100.0;
    
    bool enabled = true;
    float sensitivity = 0.5f;
    float strength = 0.7f;
    CorrectionMode correctionMode = CorrectionMode::Suggest;
    SourceProfile sourceProfile = SourceProfile::Generic;
    
    // Profile-specific thresholds
    struct ProfileThresholds
    {
        float resonanceThreshold = 6.0f;
        float harshnessThreshold = -15.0f;
        float muddinessThreshold = -20.0f;
        float boxyThreshold = -18.0f;
        float sibilanceThreshold = -12.0f;
        float lowEndThreshold = -10.0f;
        
        // Frequency ranges (can be adjusted per profile)
        float muddinessLow = 150.0f;
        float muddinessHigh = 400.0f;
        float boxyLow = 300.0f;
        float boxyHigh = 800.0f;
        float harshnessLow = 2000.0f;
        float harshnessHigh = 6000.0f;
        float sibilanceLow = 5000.0f;
        float sibilanceHigh = 10000.0f;
    };
    ProfileThresholds thresholds;
    
    //==========================================================================
    // Enhanced Detection v2.0 - Temporal smoothing and adaptive thresholds
    //==========================================================================
    
    // Temporal averaging buffers (3-frame history)
    static constexpr int temporalFrames = 3;
    std::vector<std::vector<float>> spectrumHistory;  // Last N spectrums for averaging
    int historyWriteIndex = 0;
    
    // RMS tracking for adaptive thresholds
    float currentRMS = -60.0f;
    float averageRMS = -40.0f;
    static constexpr float rmsSmoothing = 0.95f;
    
    // Enhanced peak detection data
    struct PeakCandidate
    {
        int bin = 0;
        float frequency = 0.0f;
        float magnitude = -100.0f;
        float peakHeight = 0.0f;       // dB above surroundings
        float bandwidth = 0.0f;        // Hz at -3dB points
        float calculatedQ = 1.0f;      // Calculated from bandwidth
        int frameCount = 0;            // How many frames this peak has been detected
    };
    std::vector<PeakCandidate> persistentPeaks;  // Peaks that persist across frames
    
    // Helper functions for enhanced detection
    void updateSpectrumHistory(const std::vector<float>& spectrum);
    std::vector<float> getTemporallySmoothedSpectrum() const;
    float calculateAdaptiveThreshold(float baseThreshold) const;
    float calculateBandwidth(int peakBin) const;  // Returns bandwidth in Hz at -3dB
    float bandwidthToQ(float frequency, float bandwidth) const;
    void updatePersistentPeaks(const std::vector<PeakCandidate>& newPeaks);
    float getSensitivityMultiplier() const;  // Exponential curve for sensitivity
    
    std::vector<float> currentSpectrum;
    mutable std::mutex spectrumMutex;
    
    std::vector<Correction> pendingCorrections;
    std::vector<Correction> approvedCorrections;
    mutable std::mutex correctionsMutex;
    
    std::atomic<DetectedGenre> detectedGenre{DetectedGenre::Unknown};
    std::atomic<bool> newAnalysisAvailable{false};
    
    // Analysis history
    std::vector<AnalysisSnapshot> analysisHistory;
    mutable std::mutex historyMutex;
    
    int analysisCounter = 0;
    static constexpr int analysisInterval = 15;
    
    //==============================================================================
    // Filter state for dynamic EQ processing (biquad Direct Form II Transposed)
    struct FilterState
    {
        float z1 = 0.0f;  // First delay element
        float z2 = 0.0f;  // Second delay element
    };
    
    // Biquad coefficients cache (recalculated only when corrections change)
    struct BiquadCoefficients
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        bool valid = false;
    };
    
    // Per-channel, per-correction filter states
    std::array<std::array<FilterState, maxChannels>, maxCorrections> filterStates;
    std::array<BiquadCoefficients, maxCorrections> coefficientCache;
    bool coefficientsNeedUpdate = true;
    
    // Helper to update coefficients when corrections change
    void updateBiquadCoefficients();
    
    //==========================================================================
    // ML Engine for improved detection
    MLEngine mlEngine;
    bool useMLDetection = true;  // Enable ML-based detection
    
    void detectProblemsWithML();  // ML-enhanced detection
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEngine)
};
