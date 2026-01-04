#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>

//==============================================================================
/**
 * Reference Matcher - Spectral matching against reference tracks
 * 
 * Features:
 * - Load and analyze reference audio files
 * - Capture reference from sidechain input
 * - Generate EQ match curve
 * - Smoothing and amount control
 * - Genre-specific reference presets
 */
class ReferenceMatcher
{
public:
    //==============================================================================
    enum class CaptureState
    {
        Idle,
        CapturingInput,
        CapturingReference,
        Ready
    };
    
    enum class MatchMode
    {
        Full,           // Match entire spectrum
        LowEnd,         // Focus on 20-200 Hz
        MidRange,       // Focus on 200-4000 Hz
        HighEnd,        // Focus on 4000-20000 Hz
        Custom          // User-defined range
    };

    //==============================================================================
    ReferenceMatcher();
    ~ReferenceMatcher() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    
    //==============================================================================
    // Reference loading
    bool loadReferenceFromFile(const juce::File& file);
    void captureFromSidechain(const juce::AudioBuffer<float>& buffer);
    void startInputCapture();
    void stopInputCapture();
    void captureInput(const juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Spectrum data
    void setInputSpectrum(const std::vector<float>& spectrum);
    const std::vector<float>& getReferenceSpectrum() const { return referenceSpectrum; }
    const std::vector<float>& getInputSpectrum() const { return averagedInputSpectrum; }
    const std::vector<float>& getMatchCurve() const { return matchCurve; }
    
    //==============================================================================
    // Match curve generation
    void calculateMatchCurve();
    void getMatchedMagnitudes(std::vector<float>& magnitudes,
                              const std::vector<float>& frequencies,
                              double sampleRate) const;
    
    //==============================================================================
    // Settings
    void setMatchAmount(float amount) { matchAmount = juce::jlimit(0.0f, 1.0f, amount); }
    float getMatchAmount() const { return matchAmount; }
    
    void setSmoothing(float smooth) { smoothingAmount = juce::jlimit(0.0f, 1.0f, smooth); }
    float getSmoothing() const { return smoothingAmount; }
    
    void setMatchMode(MatchMode mode) { matchMode = mode; }
    MatchMode getMatchMode() const { return matchMode; }
    
    void setCustomRange(float minFreq, float maxFreq);
    
    void setMaxBoost(float db) { maxBoostDB = juce::jlimit(0.0f, 24.0f, db); }
    float getMaxBoost() const { return maxBoostDB; }
    
    void setMaxCut(float db) { maxCutDB = juce::jlimit(0.0f, 24.0f, db); }
    float getMaxCut() const { return maxCutDB; }
    
    //==============================================================================
    // State
    CaptureState getState() const { return captureState.load(); }
    bool hasReference() const { return !referenceSpectrum.empty(); }
    bool hasInputCapture() const { return inputCaptureCount > 0; }
    float getCaptureProgress() const;
    
    //==============================================================================
    // Built-in reference presets (genre-based targets)
    void loadPresetReference(const juce::String& presetName);
    juce::StringArray getAvailablePresets() const;

private:
    //==============================================================================
    void analyzeAudioBuffer(const juce::AudioBuffer<float>& buffer, std::vector<float>& outputSpectrum);
    void smoothSpectrum(std::vector<float>& spectrum, float amount);
    float getFrequencyWeight(float freq) const;
    int freqToBin(float freq) const;
    float binToFreq(int bin) const;
    
    //==============================================================================
    double currentSampleRate = 44100.0;
    int fftSize = 4096;
    int numBins = 2049;
    
    // Spectrum data
    std::vector<float> referenceSpectrum;
    std::vector<float> averagedInputSpectrum;
    std::vector<float> matchCurve;
    mutable std::mutex spectrumMutex;
    
    // Capture state
    std::atomic<CaptureState> captureState{CaptureState::Idle};
    int inputCaptureCount = 0;
    int referenceCaptureCount = 0;
    static constexpr int targetCaptureCount = 150; // ~3 seconds at 50Hz
    
    // Settings
    float matchAmount = 0.5f;
    float smoothingAmount = 0.5f;
    MatchMode matchMode = MatchMode::Full;
    float customMinFreq = 20.0f;
    float customMaxFreq = 20000.0f;
    float maxBoostDB = 12.0f;
    float maxCutDB = 12.0f;
    
    // FFT for file analysis
    juce::dsp::FFT fft{12};
    juce::dsp::WindowingFunction<float> window{4096, juce::dsp::WindowingFunction<float>::hann};
    
    // Built-in presets
    std::map<juce::String, std::vector<float>> presetCurves;
    void initializePresets();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceMatcher)
};
