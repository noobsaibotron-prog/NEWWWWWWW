#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

//==============================================================================
/**
 * Spectrum Analyzer - High-quality real-time FFT analysis
 * 
 * Features:
 * - 4096-point FFT for good frequency resolution
 * - Smooth attack/release for visual display
 * - Peak Hold with configurable decay
 * - Thread-safe spectrum access
 * - Configurable overlap and windowing
 */
class SpectrumAnalyzer
{
public:
    //==============================================================================
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder; // 4096
    static constexpr int numBins = fftSize / 2 + 1; // 2049
    
    //==============================================================================
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() = default;

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void process(const juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    const std::vector<float>& getSmoothedSpectrum() const { return smoothedSpectrum; }
    const std::vector<float>& getRawSpectrum() const { return rawSpectrum; }
    const std::vector<float>& getPeakHoldSpectrum() const { return peakHoldSpectrum; }
    
    float getMagnitudeForFrequency(float frequency) const;
    int getBinForFrequency(float frequency) const;
    float getFrequencyForBin(int bin) const;
    
    double getSampleRate() const { return currentSampleRate; }
    
    //==============================================================================
    void setAttackTime(float ms) { attackTimeMs = ms; updateSmoothingCoeffs(); }
    void setReleaseTime(float ms) { releaseTimeMs = ms; updateSmoothingCoeffs(); }
    
    // Peak Hold
    void setPeakHoldEnabled(bool enabled) { peakHoldEnabled = enabled; }
    bool isPeakHoldEnabled() const { return peakHoldEnabled; }
    void setPeakHoldDecayTime(float seconds) { peakHoldDecayTime = seconds; }
    void resetPeakHold();
    
private:
    //==============================================================================
    void processFFT();
    void updateSmoothingCoeffs();
    
    //==============================================================================
    double currentSampleRate = 44100.0;
    
    // FFT
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{static_cast<size_t>(fftSize), 
                                                juce::dsp::WindowingFunction<float>::hann};
    
    // FIFO for sample collection
    std::vector<float> fifoBuffer;
    int fifoIndex = 0;
    std::atomic<bool> fftDataReady{false};
    
    // FFT data
    std::vector<float> fftData;
    std::vector<float> rawSpectrum;
    std::vector<float> smoothedSpectrum;
    
    // Smoothing
    float attackTimeMs = 5.0f;
    float releaseTimeMs = 100.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    
    // Peak Hold
    std::vector<float> peakHoldSpectrum;
    bool peakHoldEnabled = true;
    float peakHoldDecayTime = 2.0f;  // seconds
    float peakDecayRate = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
