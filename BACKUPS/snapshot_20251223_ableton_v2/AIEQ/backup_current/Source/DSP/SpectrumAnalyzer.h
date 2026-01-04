#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include <memory>

//==============================================================================
class SpectrumAnalyzer
{
public:
    enum class Resolution
    {
        Low = 10,     // 1024
        Medium = 11,  // 2048
        High = 12,    // 4096
        Max = 13      // 8192
    };

    enum class Speed
    {
        Fast,
        Medium,
        Slow
    };

    //==============================================================================
    static constexpr float minDecibels = -120.0f;
    static constexpr float maxDecibels = 12.0f;
    
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
    int getFFTSize() const { return fftSize; }
    int getNumBins() const { return numBins; }
    
    double getSampleRate() const { return currentSampleRate; }
    
    //==============================================================================
    void setAttackTime(float ms) { attackTimeMs = ms; updateSmoothingCoeffs(); }
    void setReleaseTime(float ms) { releaseTimeMs = ms; updateSmoothingCoeffs(); }
    void setSpeed(Speed s);
    Speed getSpeed() const { return speedMode; }
    
    // Peak Hold
    void setPeakHoldEnabled(bool enabled) { peakHoldEnabled = enabled; }
    bool isPeakHoldEnabled() const { return peakHoldEnabled; }
    void setPeakHoldDecayTime(float seconds) { peakHoldDecayTime = seconds; }
    void resetPeakHold();

    // Resolution
    void setFFTResolution(Resolution res);
    Resolution getFFTResolution() const { return resolution; }
    
private:
    //==============================================================================
    void processFFT();
    void updateSmoothingCoeffs();
    void rebuildFFT(int newOrder);
    
    //==============================================================================
    double currentSampleRate = 44100.0;
    Resolution resolution = Resolution::High;
    Speed speedMode = Speed::Medium;
    int fftOrder = static_cast<int>(Resolution::High);
    int fftSize = 1 << fftOrder;
    int numBins = fftSize / 2 + 1;
    
    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
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
    juce::SpinLock dataLock;
    
    // Peak Hold
    std::vector<float> peakHoldSpectrum;
    bool peakHoldEnabled = true;
    float peakHoldDecayTime = 2.0f;  // seconds
    float peakDecayRate = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
