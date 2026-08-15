#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include "../Core/LatestValueMailbox.h"

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

    // Audio-thread-safe snapshot of the published smoothed dB spectrum.
    // GUI processFFT() publishes immutable fixed-size frames through a multi-slot
    // ownership mailbox; the audio consumer never reads a slot the GUI can write.
    // Returns the number of bins copied. RT-safe: bounded CAS, no allocation when
    // `dst` is already sized. This legacy AI bridge is removed by the headless-AI
    // migration; by-reference getters remain GUI-thread-only.
    int copySmoothedSpectrumInto(std::vector<float>& dst) const noexcept;
    
    float getMagnitudeForFrequency(float frequency) const;
    int getBinForFrequency(float frequency) const;
    float getFrequencyForBin(int bin) const;
    int getFFTSize() const { return fftSize; }
    int getNumBins() const { return numBins; }
    
    double getSampleRate() const { return currentSampleRate.load(std::memory_order_relaxed); }
    
    //==============================================================================
    void setAttackTime(float ms) { attackTimeMs = ms; updateSmoothingCoeffs(); }
    void setReleaseTime(float ms) { releaseTimeMs = ms; updateSmoothingCoeffs(); }
    // Thread-safe request. The actual GUI-owned analyzer state is applied at
    // the start of processFFT() (or synchronously by prepare() while audio is
    // quiescent). No FIFO/config mutation occurs on the audio thread.
    void setSpeed(Speed s) noexcept;
    Speed getSpeed() const { return speedMode; }
    
    // Peak Hold
    void setPeakHoldEnabled(bool enabled) { peakHoldEnabled = enabled; }
    bool isPeakHoldEnabled() const { return peakHoldEnabled; }
    void setPeakHoldDecayTime(float seconds);
    void resetPeakHold();

    // Resolution. setFFTResolution() is a lock-free request only; the GUI
    // consumer applies it before the next FFT pass.
    void setFFTResolution(Resolution res) noexcept;
    Resolution getFFTResolution() const { return resolution; }

    bool hasNewData() const { return newDataAvailable.load(); }

    // FIX 2: Version counter for spectrum path caching
    uint64_t getSpectrumVersion() const { return spectrumVersion.load(std::memory_order_acquire); }
    
private:
    //==============================================================================
    void updateSmoothingCoeffs();
    void rebuildFFT(int newOrder);
    void updateDecayFactor(float guiUpdateHz);
    void applyRequestedConfiguration(); // GUI/lifecycle thread, processingMutex held
    void resetUnlocked();               // lifecycle thread, processingMutex held
    void applyPendingVisualReset();      // GUI thread, processingMutex held
    
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

    static constexpr int maxSnapshotBins = 4096; // Max-resolution fftSize / 2
    struct RTSpectrumSnapshot
    {
        std::array<float, maxSnapshotBins> db {};
        int numBins = 0;
    };

    void publishRTSnapshot(const FFTState& state, int bufferIndex) noexcept;

    std::array<FFTState, 4> fftStates; // Low, Medium, High, Max
    std::atomic<int> activeStateIndex { 2 }; // Default: High (index 2)
    std::atomic<int> requestedStateIndex { 2 };
    std::atomic<int> requestedSpeedIndex { 1 }; // Fast=0, Medium=1, Slow=2

    // processFFT()/prepare()/reset() are never called by the audio thread.
    // Serializing those lifecycle/GUI operations prevents prepare/reset from
    // racing a message-thread FFT pass while the audio FIFO remains SPSC.
    mutable std::mutex processingMutex;

    // GUI -> audio snapshot mailbox used only by the legacy AI feed until the
    // headless PerceptualFrontEnd migration removes that dependency. Four slots
    // prevent the old 0->1->0 double-buffer ABA/torn-copy failure.
    mutable LatestValueMailbox<RTSpectrumSnapshot, 4> rtSnapshotMailbox;
    mutable RTSpectrumSnapshot rtLastConsumedSnapshot {}; // audio-consumer thread only
    mutable bool rtHasLastConsumedSnapshot = false;        // audio-consumer thread only

    std::atomic<double> currentSampleRate { 44100.0 };
    Resolution resolution = Resolution::High;
    Speed speedMode = Speed::Medium;
    double lastProcessMs = 0.0; // per-instance throttle for Max resolution
    bool visualResetPending = false; // guarded by processingMutex; consumed on GUI thread
    int fftOrder = static_cast<int>(Resolution::High);
    int fftSize = 1 << fftOrder;
    int numBins = fftSize / 2;
    
    // Lock-free FIFO for audio → GUI handoff
    static constexpr int fifoCapacity = 32768; // headroom for max FFT size
    juce::AbstractFifo fifo { fifoCapacity };
    std::vector<float> fifoBuffer;
    std::vector<float> overlapBuffer; // For 50% overlap FFT

    std::atomic<int> activeBufferIndex { 0 };
    
    // Helper to get active state (GUI thread only)
    inline FFTState& getActiveState() { return fftStates[activeStateIndex.load(std::memory_order_acquire)]; }
    inline const FFTState& getActiveState() const { return fftStates[activeStateIndex.load(std::memory_order_acquire)]; }
    
    // Smoothing & decay
    float attackTimeMs = 2.0f;
    float releaseTimeMs = 50.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    std::atomic<float> smoothingFactor { 0.7f };
    std::atomic<float> decayDBPerSecond { 30.0f };
    float decayFactor = 0.98f;
    
    // State
    std::atomic<bool> newDataAvailable { false };
    bool peakHoldEnabled = true;
    float peakHoldDecayTime = 2.0f;  // seconds

    // FIX 2: Version counter — incremented each time processFFT produces new data.
    // GUI can compare to avoid rebuilding spectrum paths when nothing changed.
    std::atomic<uint64_t> spectrumVersion { 0 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
