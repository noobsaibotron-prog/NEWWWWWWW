#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "MLEngine.h"
#include "MultiTrackUnmasking.h"
#include "NeuralNetworkWrapper.h"
#include "AdaptiveAIEngine.h"
#include "OnlineLearningSystem.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <array>
#include <memory>

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
        EDM,        // Tolerates more bass/sub, brightness
        Techno      // Techno-specific: sub focus, kick resonances, hi-hat harshness
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

        // Tipo di filtro EQ suggerito per questa correzione
        enum class FilterType
        {
            Peak = 0,      // Filtro parametrico standard
            LowShelf,      // Shelf basse frequenze
            HighShelf,     // Shelf alte frequenze  
            LowCut,        // High-pass filter
            HighCut,       // Low-pass filter
            Notch          // Notch filter (rejection)
        };

        FilterType suggestedFilter = FilterType::Peak;

        // Slope per filtri shelf/cut (dB/ottava): 6, 12, 18, 24
        int filterSlope = 12;
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
    void analyzeSpectrum(const std::vector<float>& spectrum, bool force = false);
    
    /** 
     * Process corrections by applying dynamic EQ filtering to the buffer.
     * Only processes approved corrections with CorrectionMode != Off.
     */
    void processCorrections(juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Settings
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }
    
    void setSensitivity(float s) { sensitivity.store(juce::jlimit(0.0f, 1.0f, s), std::memory_order_relaxed); }
    float getSensitivity() const { return sensitivity.load(std::memory_order_relaxed); }
    
    void setStrength(float s);
    float getStrength() const { return strength.load(std::memory_order_relaxed); }
    
    void setCorrectionMode(CorrectionMode mode) { correctionMode = mode; }
    CorrectionMode getCorrectionMode() const { return correctionMode; }
    
    // Source profile
    void setSourceProfile(SourceProfile profile);
    SourceProfile getSourceProfile() const { return static_cast<SourceProfile>(sourceProfile.load(std::memory_order_relaxed)); }
    static juce::String getProfileName(SourceProfile profile);
    
    //==============================================================================
    // Corrections management
    std::vector<Correction> getPendingCorrections() const;
    std::vector<Correction> getApprovedCorrections() const;
    std::vector<Correction> getApprovedCorrectionsForUI() const;
    
    void approveCorrection(int index);
    void approveAllCorrections();
    void rejectCorrection(int index);
    void clearCorrections();
    void clearApprovedCorrections();  // Clear only approved corrections, keep pending
    
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
    static juce::String getFilterTypeName(Correction::FilterType type);
    static juce::String getGenreName(DetectedGenre genre);
    static juce::String getBandName(float frequency);  // Human-readable band name
    
    // Get scaled correction (apply strength factor)
    Correction getScaledCorrection(const Correction& c) const;
    
    //==============================================================================
    // Intelligent band assignment
    //==============================================================================
    
    /**
     * Filter and prioritize corrections based on severity/confidence thresholds
     * @param minSeverity Minimum severity threshold (0-1)
     * @param minConfidence Minimum confidence threshold (0-1)
     * @return Filtered and sorted corrections (by priority = severity * confidence)
     */
    std::vector<Correction> getFilteredAndPrioritizedCorrections(
        float minSeverity = 0.25f,
        float minConfidence = 0.55f) const;
    
    /**
     * Merge nearby corrections (within 1/3 octave) to avoid duplicate bands
     * @param corrections Input corrections
     * @return Merged corrections
     */
    std::vector<Correction> mergeNearbyCorrections(const std::vector<Correction>& corrections) const;
    
    //==============================================================================
    // Advanced AI Systems Integration
    //==============================================================================
    
    // Feature flags - public getters/setters
    void setMultiTrackUnmaskingEnabled(bool isEnabled) { enableMultiTrackUnmasking = isEnabled; }
    bool isMultiTrackUnmaskingEnabled() const { return enableMultiTrackUnmasking; }
    
    void setNeuralNetworksEnabled(bool isEnabled) { enableNeuralNetworks = isEnabled; }
    bool isNeuralNetworksEnabled() const { return enableNeuralNetworks; }
    
    void setAdaptiveProcessingEnabled(bool isEnabled) { enableAdaptiveProcessing = isEnabled; }
    bool isAdaptiveProcessingEnabled() const { return enableAdaptiveProcessing; }
    
    void setOnlineLearningEnabled(bool isEnabled) { enableOnlineLearning = isEnabled; }
    bool isOnlineLearningEnabled() const { return enableOnlineLearning; }
    
    // Multi-track unmasking
    void updateMultiTrackAnalysis(const juce::String& trackId, const std::vector<float>& spectrum);
    std::vector<MultiTrackUnmasking::UnmaskingCorrection> getUnmaskingCorrections(double sampleRate);
    
    // Adaptive processing
    void updateAdaptiveAnalysis(const juce::AudioBuffer<float>& buffer, double sampleRate);
    AdaptiveAIEngine::AdaptiveConfig getCurrentAdaptiveConfig() const;
    
    // Neural network
    bool loadNeuralModel(const juce::File& modelFile, NeuralNetworkWrapper::ModelType type);
    NeuralNetworkWrapper::InferenceResult runNeuralInference(const std::vector<float>& input);
    
    // Online learning
    void addLearningSample(const std::vector<float>& input, const std::vector<float>& target, 
                          const juce::String& source = "auto");
    void performOnlineLearningUpdate();

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
    Correction::FilterType selectOptimalFilterType(
        ProblemType problem,
        float frequency,
        float bandwidth,
        float peakHeight) const;
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
    std::atomic<float> sensitivity { 0.5f };  // FIX RT-SAFETY: atomic for thread-safe access
    std::atomic<float> strength { 0.7f };     // FIX RT-SAFETY: atomic for thread-safe access
    CorrectionMode correctionMode = CorrectionMode::Suggest;
    std::atomic<int> sourceProfile { static_cast<int>(SourceProfile::Generic) };  // FIX RT-SAFETY: atomic
    
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
        
        // Advanced temporal analysis
        float stability = 0.0f;        // Temporal stability (0-1)
        float consistency = 0.0f;      // Consistency across frames (0-1)
        float attackDecay = 0.0f;      // Attack/decay rate (positive = stable, negative = transient)
        std::vector<float> magnitudeHistory;  // Last N magnitudes for pattern analysis
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
    
    // Advanced detection improvements - FASE 1
    float computeZScore(const std::vector<float>& spectrum, int centerBin, int window) const;  // Local z-score
    float computeZScoreAtFrequency(float frequency, int window = 21) const;  // Thread-safe z-score
    bool hasTemporalConsensus(const PeakCandidate& peak, int minFrames = 3, float magToleranceDb = 2.5f) const;  // Multi-frame consensus
    bool isContextuallyNormal(ProblemType type, float frequency) const;  // Profile-aware whitelist
    float calculatePercentile(const std::vector<float>& data, float percentile) const;  // Calculate percentile (0-1)
    float calculateAdaptiveThresholdPercentile(float baseThreshold) const;  // Threshold based on percentile
    void analyzeTemporalPattern(PeakCandidate& peak) const;  // Analyze temporal pattern for peak
    float crossValidateDetection(ProblemType type, float frequency, float magnitude) const;  // Cross-validate detection
    
    // FASE 2: Harmonic Analysis, Spectral Coherence, Dynamic Range Normalization
    float findFundamentalFrequency(float minFreq = 50.0f, float maxFreq = 500.0f) const;  // Find f0 using autocorrelation
    bool isHarmonicPeak(float peakFreq, float fundamentalFreq, float tolerance = 0.05f) const;  // Check if peak is harmonic (f0, 2f0, 3f0...)
    float calculateDynamicRange() const;  // Calculate signal dynamic range (dB)
    float normalizeThresholdByDynamicRange(float baseThreshold, float dynamicRange) const;  // Adjust threshold based on DR
    float analyzeSpectralCoherence(ProblemType type, float frequency, float bandwidth) const;  // Pattern matching for problem types
    float getSpectralPatternScore(ProblemType type, float centerFreq, float bandwidth) const;  // Score how well pattern matches problem type
    
    //==========================================================================
    // LOCK-FREE TRIPLE-BUFFERED SPECTRUM ACCESS
    //==========================================================================
    // Triple-buffer pattern for lock-free producer/consumer:
    // - Writer (AI thread) writes to writeIndex buffer
    // - Writer swaps writeIndex with readyIndex when done
    // - Reader (GUI/other threads) swaps readyIndex with readIndex to get latest
    // - No locks required, all operations are atomic
    //
    // This replaces std::mutex spectrumMutex for RT-safe spectrum access.
    //==========================================================================
    
    struct SpectrumSnapshot
    {
        std::array<float, numBins> bins {};
        float rmsLevel = -60.0f;
        float avgRmsLevel = -40.0f;
        uint64_t version = 0;  // Non-atomic for copy, version stored in separate atomic
        
        void fill(const std::vector<float>& data, float rms, float avgRms) noexcept
        {
            const size_t copySize = std::min(data.size(), bins.size());
            std::copy_n(data.begin(), copySize, bins.begin());
            // Zero remaining bins if input is smaller
            if (copySize < bins.size())
                std::fill(bins.begin() + static_cast<long>(copySize), bins.end(), -100.0f);
            rmsLevel = rms;
            avgRmsLevel = avgRms;
            ++version;
        }
        
        // Make copyable
        SpectrumSnapshot() = default;
        SpectrumSnapshot(const SpectrumSnapshot& other) = default;
        SpectrumSnapshot& operator=(const SpectrumSnapshot& other) = default;
    };
    
    // Triple-buffer: indices are always 0, 1, or 2
    std::array<SpectrumSnapshot, 3> spectrumBuffers;
    mutable std::atomic<int> spectrumReadIndex { 0 };
    std::atomic<int> spectrumWriteIndex { 1 };
    mutable std::atomic<int> spectrumReadyIndex { 2 };
    
    // Publish new spectrum (called from AI/analysis thread)
    void publishSpectrum(const std::vector<float>& spectrum, float rms, float avgRms) noexcept
    {
        const int writeIdx = spectrumWriteIndex.load(std::memory_order_relaxed);
        spectrumBuffers[static_cast<size_t>(writeIdx)].fill(spectrum, rms, avgRms);
        
        // Swap write buffer with ready buffer (atomic exchange)
        int expected = spectrumReadyIndex.load(std::memory_order_relaxed);
        while (!spectrumReadyIndex.compare_exchange_weak(expected, writeIdx,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed)) {}
        spectrumWriteIndex.store(expected, std::memory_order_relaxed);
    }
    
    // Read latest spectrum snapshot (lock-free, can be called from any thread)
    [[nodiscard]] SpectrumSnapshot readSpectrumSnapshot() const noexcept
    {
        // Swap read buffer with ready buffer to get latest
        int expected = spectrumReadyIndex.load(std::memory_order_relaxed);
        int readIdx = spectrumReadIndex.load(std::memory_order_relaxed);
        
        while (!spectrumReadyIndex.compare_exchange_weak(expected, readIdx,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed)) {}
        spectrumReadIndex.store(expected, std::memory_order_relaxed);
        
        return spectrumBuffers[static_cast<size_t>(expected)];
    }
    
    // Legacy compatibility: get spectrum as vector (allocates, not RT-safe but convenient)
    [[nodiscard]] std::vector<float> getCurrentSpectrumCopy() const
    {
        const auto snapshot = readSpectrumSnapshot();
        return std::vector<float>(snapshot.bins.begin(), snapshot.bins.end());
    }
    
    // Internal spectrum storage for functions that need direct access
    // (e.g., updateSpectrumHistory, calculateBandEnergyUnlocked)
    // This is updated from the triple-buffer during analysis
    std::vector<float> currentSpectrum;
    mutable std::mutex spectrumMutex;  // Only for internal updateSpectrumHistory access
    
    std::vector<Correction> pendingCorrections;
    
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
    
    // Double-buffered approved corrections (audio reads lock-free)
    std::array<std::vector<Correction>, 2> approvedCorrectionBuffers;
    std::atomic<int> activeCorrectionsIndex { 0 };
    
    // Mutex only for writes from GUI/AI threads
    mutable std::mutex correctionsWriteMutex;
    
    // Cached coefficients double-buffer
    struct CachedCorrectionCoeffs
    {
        std::array<BiquadCoefficients, maxCorrections> coeffs;
        std::array<bool, maxCorrections> enabled {};
        int numActive = 0;
    };
    std::array<CachedCorrectionCoeffs, 2> cachedCoeffs;
    std::atomic<int> activeCoefficientIndex { 0 };
    std::atomic<bool> correctionCoeffsNeedUpdate { false };
    
    std::atomic<DetectedGenre> detectedGenre{DetectedGenre::Unknown};
    std::atomic<bool> newAnalysisAvailable{false};
    
    // Analysis history
    std::vector<AnalysisSnapshot> analysisHistory;
    mutable std::mutex historyMutex;
    
    int analysisCounter = 0;
    static constexpr int analysisInterval = 1;  // REDUCED: Analyze every frame for immediate detection
    
    //==============================================================================
    // Per-channel, per-correction filter states
    std::array<std::array<FilterState, maxChannels>, maxCorrections> filterStates;
    
    // Helper to update cached coefficients for a buffer index
    void updateCachedCoefficients(int bufferIndex, const std::vector<Correction>& corrections);
    
    //==========================================================================
    // ML Engine for improved detection
    MLEngine mlEngine;
    bool useMLDetection = true;  // Enable ML-based detection
    
    void detectProblemsWithML();  // ML-enhanced detection
    
    //==========================================================================
    // Advanced AI Systems
    std::unique_ptr<MultiTrackUnmasking> multiTrackUnmasking;
    std::unique_ptr<NeuralNetworkWrapper> neuralNetwork;
    std::unique_ptr<AdaptiveAIEngine> adaptiveEngine;
    std::unique_ptr<OnlineLearningSystem> onlineLearning;
    
    // Advanced features flags
    bool enableMultiTrackUnmasking = false;
    bool enableNeuralNetworks = false;
    bool enableAdaptiveProcessing = false;
    bool enableOnlineLearning = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEngine)
};
