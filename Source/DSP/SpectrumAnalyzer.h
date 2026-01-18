#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
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

    // AUDIO THREAD: lock-free push
    void pushSamples(const juce::AudioBuffer<float>& buffer);
    
    // GUI THREAD: perform FFT
    void processFFT();
    
    //==============================================================================
    // Lock-free getters (active buffer)
    const std::vector<float>& getSpectrum() const;
    const std::vector<float>& getSpectrumDB() const;
    const std::vector<float>& getPeakHold() const;

    // Backward-compatible getters
    const std::vector<float>& getSmoothedSpectrum() const { return getSpectrumDB(); }
    const std::vector<float>& getRawSpectrum() const { return getSpectrum(); }
    const std::vector<float>& getPeakHoldSpectrum() const { return getPeakHold(); }
    
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
    void setPeakHoldDecayTime(float seconds);
    void resetPeakHold();

    // Resolution
    void setFFTResolution(Resolution res);
    Resolution getFFTResolution() const { return resolution; }

    bool hasNewData() const { return newDataAvailable.load(); }
    
private:
    //==============================================================================
    void updateSmoothingCoeffs();
    void rebuildFFT(int newOrder);
    void updateDecayFactor(float guiUpdateHz);
    
    //==============================================================================
    // FIX RT-SAFETY: Pre-allocated FFT states for all resolutions
    struct FFTState
    {
        std::unique_ptr<juce::dsp::FFT> fft;
        std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
        std::vector<float> fftData;
        std::array<std::vector<float>, 2> spectrumBuffers;
        std::array<std::vector<float>, 2> spectrumDBBuffers;
        std::array<std::vector<float>, 2> peakHoldBuffers;
        int fftOrder = 12;
        int fftSize = 4096;
        int numBins = 2048;
    };
    
    std::array<FFTState, 4> fftStates; // Low, Medium, High, Max
    std::atomic<int> activeStateIndex { 2 }; // Default: High (index 2)
    
    double currentSampleRate = 44100.0;
    Resolution resolution = Resolution::High;
    Speed speedMode = Speed::Medium;
    double lastProcessMs = 0.0; // per-instance throttle for Max resolution
    int fftOrder = static_cast<int>(Resolution::High);
    int fftSize = 1 << fftOrder;
    int numBins = fftSize / 2;
    
    // Lock-free FIFO for audio → GUI handoff
    static constexpr int fifoCapacity = 32768; // headroom for max FFT size
    juce::AbstractFifo fifo { fifoCapacity };
    std::vector<float> fifoBuffer;
    
    std::atomic<int> activeBufferIndex { 0 };
    
    // Helper to get active state (GUI thread only)
    inline FFTState& getActiveState() { return fftStates[activeStateIndex.load(std::memory_order_acquire)]; }
    inline const FFTState& getActiveState() const { return fftStates[activeStateIndex.load(std::memory_order_acquire)]; }
    
    // Smoothing & decay
    float attackTimeMs = 5.0f;
    float releaseTimeMs = 100.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    std::atomic<float> smoothingFactor { 0.7f };
    std::atomic<float> decayDBPerSecond { 30.0f };
    float decayFactor = 0.98f;
    
    // State
    std::atomic<bool> newDataAvailable { false };
    bool peakHoldEnabled = true;
    float peakHoldDecayTime = 2.0f;  // seconds
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
