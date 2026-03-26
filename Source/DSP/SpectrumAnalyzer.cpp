#include "SpectrumAnalyzer.h"
#include <cmath>
#if AIEQ_GUI_DEBUG
#include "../Utils/DebugLog.h"
#endif

SpectrumAnalyzer::SpectrumAnalyzer()
{
    // FIX RT-SAFETY: Pre-allocate ALL FFT states (Low, Medium, High, Max)
    // This eliminates heap allocations when changing resolution during playback
    
    const int resolutionOrders[4] = { 10, 11, 12, 13 }; // Low, Medium, High, Max
    
    for (int i = 0; i < 4; ++i)
    {
        auto& state = fftStates[i];
        state.fftOrder = resolutionOrders[i];
        state.fftSize = 1 << state.fftOrder;
        state.numBins = state.fftSize / 2;
        
        // Allocate FFT objects
        state.fft = std::make_unique<juce::dsp::FFT>(state.fftOrder);
        state.window = std::make_unique<juce::dsp::WindowingFunction<float>>(
            static_cast<size_t>(state.fftSize),
            juce::dsp::WindowingFunction<float>::hann);
        
        // Allocate buffers
        state.fftData.resize(static_cast<size_t>(state.fftSize * 2), 0.0f);
        
        for (int j = 0; j < 2; ++j)
        {
            state.spectrumBuffers[j].resize(static_cast<size_t>(state.numBins), 0.0f);
            state.spectrumDBBuffers[j].resize(static_cast<size_t>(state.numBins), minDecibels);
            state.peakHoldBuffers[j].resize(static_cast<size_t>(state.numBins), minDecibels);
        }
    }
    
    // Set default active state (High = index 2)
    activeStateIndex.store(2, std::memory_order_relaxed);
    
    // Update cached values from default state
    const auto& defaultState = fftStates[2];
    fftOrder = defaultState.fftOrder;
    fftSize = defaultState.fftSize;
    numBins = defaultState.numBins;
    
    // Pre-allocate FIFO buffer (shared across all resolutions)
    fifoBuffer.resize(static_cast<size_t>(fifo.getTotalSize()), 0.0f);
    
    updateSmoothingCoeffs();
    updateDecayFactor(30.0f); // assume ~30 Hz GUI timer default
}

void SpectrumAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo.reset();
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    overlapBuffer.clear();
    
    auto& state = getActiveState();
    std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
    
    for (int i = 0; i < 2; ++i)
    {
        std::fill(state.spectrumBuffers[i].begin(), state.spectrumBuffers[i].end(), 0.0f);
        std::fill(state.spectrumDBBuffers[i].begin(), state.spectrumDBBuffers[i].end(), minDecibels);
        std::fill(state.peakHoldBuffers[i].begin(), state.peakHoldBuffers[i].end(), minDecibels);
    }
    
    newDataAvailable.store(false);
}

void SpectrumAnalyzer::resetPeakHold()
{
    auto& state = getActiveState();
    for (int i = 0; i < 2; ++i)
        std::fill(state.peakHoldBuffers[i].begin(), state.peakHoldBuffers[i].end(), minDecibels);
}

//==============================================================================
// AUDIO THREAD (lock-free)
//==============================================================================
void SpectrumAnalyzer::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    // If we are reconfiguring resolution/FIFO, drop incoming audio block to avoid races
    if (reconfiguring.load(std::memory_order_acquire))
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples == 0 || numChannels == 0)
        return;
    
    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    if (size1 + size2 == 0)
        return; // FIFO full, drop gracefully
    
    auto mixSample = [&](int srcIndex) -> float
    {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sample += buffer.getSample(ch, srcIndex);
        return sample / static_cast<float>(numChannels);
    };
    
    if (size1 > 0)
    {
        for (int i = 0; i < size1; ++i)
            fifoBuffer[static_cast<size_t>(start1 + i)] = mixSample(i);
    }
    if (size2 > 0)
    {
        for (int i = 0; i < size2; ++i)
            fifoBuffer[static_cast<size_t>(start2 + i)] = mixSample(size1 + i);
    }
    
    fifo.finishedWrite(size1 + size2);
    newDataAvailable.store(true);
}

//==============================================================================
// GUI THREAD (heavy work)
//==============================================================================
void SpectrumAnalyzer::processFFT()
{
#if AIEQ_GUI_DEBUG
    static int fftDebugCallCount = 0;
    static double fftDebugTimeAccum = 0.0;
    static double fftDebugLastReport = 0.0;
    auto fftDebugStart = juce::Time::getMillisecondCounterHiRes();
#endif

    if (resolution == Resolution::Max)
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (nowMs - lastProcessMs < 30.0)
            return; // throttle heavy 8192-pt FFT on message thread
        lastProcessMs = nowMs;
    }

    // 50% overlap: we only need hopSize new samples to produce a new frame
    const int hopSize = fftSize / 2;
    const int available = fifo.getNumReady();
    if (available < hopSize)
        return;

    auto& state = getActiveState();
    auto& fftData = state.fftData;

    // Drain all available frames (process multiple hops if data accumulated)
    int framesProcessed = 0;
    constexpr int maxFramesPerCall = 4; // cap to avoid GUI stall

    while (fifo.getNumReady() >= hopSize && framesProcessed < maxFramesPerCall)
    {
        // Shift previous data left by hopSize, then read hopSize new samples at the end
        if (framesProcessed == 0 && overlapBuffer.empty())
        {
            // First ever frame: need full fftSize samples
            if (fifo.getNumReady() < fftSize)
                break;
            int s1, sz1, s2, sz2;
            fifo.prepareToRead(fftSize, s1, sz1, s2, sz2);
            if (sz1 > 0) std::copy(fifoBuffer.begin() + s1, fifoBuffer.begin() + s1 + sz1, fftData.begin());
            if (sz2 > 0) std::copy(fifoBuffer.begin() + s2, fifoBuffer.begin() + s2 + sz2, fftData.begin() + sz1);
            fifo.finishedRead(fftSize);

            // Save second half for overlap
            overlapBuffer.resize(static_cast<size_t>(hopSize));
            std::copy(fftData.begin() + hopSize, fftData.begin() + fftSize, overlapBuffer.begin());
        }
        else
        {
            // Overlap: reuse previous second half as first half
            std::copy(overlapBuffer.begin(), overlapBuffer.end(), fftData.begin());

            // Read hopSize new samples into second half
            int s1, sz1, s2, sz2;
            fifo.prepareToRead(hopSize, s1, sz1, s2, sz2);
            auto dest = fftData.begin() + hopSize;
            if (sz1 > 0) std::copy(fifoBuffer.begin() + s1, fifoBuffer.begin() + s1 + sz1, dest);
            if (sz2 > 0) std::copy(fifoBuffer.begin() + s2, fifoBuffer.begin() + s2 + sz2, dest + sz1);
            fifo.finishedRead(hopSize);

            // Save new second half for next overlap
            std::copy(fftData.begin() + hopSize, fftData.begin() + fftSize, overlapBuffer.begin());
        }

        // Zero-pad remainder and apply window + FFT
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

        if (state.window)
            state.window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
        if (state.fft)
            state.fft->performFrequencyOnlyForwardTransform(fftData.data());

        // Double-buffer swap
        const int readIndex  = activeBufferIndex.load(std::memory_order_acquire);
        const int writeIndex = 1 - readIndex;

        auto& magOut = state.spectrumBuffers[writeIndex];
        auto& dbOut = state.spectrumDBBuffers[writeIndex];
        auto& peakOut = state.peakHoldBuffers[writeIndex];

        const auto& prevDB = state.spectrumDBBuffers[readIndex];
        const auto& prevPeak = state.peakHoldBuffers[readIndex];

        for (int i = 0; i < numBins; ++i)
        {
            float mag = fftData[static_cast<size_t>(i)] / static_cast<float>(fftSize);
            mag = std::max(mag, 1e-10f);

            magOut[i] = mag;

            float magDB = 20.0f * std::log10(mag);
            magDB = juce::jlimit(minDecibels, maxDecibels, magDB);

            const bool rising = magDB > prevDB[i];
            const float coeff = rising ? attackCoeff : releaseCoeff;
            const float smoothedDB = prevDB[i] + coeff * (magDB - prevDB[i]);
            dbOut[i] = smoothedDB;

            float decayedPeak = prevPeak[i] * decayFactor;
            peakOut[i] = std::max(smoothedDB, decayedPeak);
        }

        activeBufferIndex.store(writeIndex, std::memory_order_release);
        ++framesProcessed;
    }

    newDataAvailable.store(false);

    // FIX 2: Bump version so GUI knows spectrum paths need rebuild
    if (framesProcessed > 0)
        spectrumVersion.fetch_add(1, std::memory_order_release);

#if AIEQ_GUI_DEBUG
    double fftDebugMs = juce::Time::getMillisecondCounterHiRes() - fftDebugStart;
    fftDebugTimeAccum += fftDebugMs;
    fftDebugCallCount++;
    double fftDebugNow = juce::Time::getMillisecondCounterHiRes();
    if (fftDebugNow - fftDebugLastReport > 2000.0)
    {
        aieqDebugLog( "[FFT] calls/sec=%.1f avgMs=%.2f fftSize=%d\n",
            fftDebugCallCount * 1000.0 / (fftDebugNow - fftDebugLastReport),
            fftDebugTimeAccum / std::max(1, fftDebugCallCount),
            getFFTSize());
        fftDebugCallCount = 0;
        fftDebugTimeAccum = 0.0;
        fftDebugLastReport = fftDebugNow;
    }
#endif
}

//==============================================================================
// Helpers
//==============================================================================
void SpectrumAnalyzer::updateSmoothingCoeffs()
{
    // Actual FFT update rate: with 50% overlap, hopSize = fftSize/2
    // At 48kHz with 4096 FFT: hop = 2048, rate = 48000/2048 = ~23Hz
    // Use a realistic estimate based on current FFT size and expected sample rate
    const float sr = static_cast<float>(std::max(44100.0, currentSampleRate));
    const float hopSize = static_cast<float>(fftSize) / 2.0f;
    const float actualUpdateRate = std::max(10.0f, sr / hopSize);
    attackCoeff = 1.0f - std::exp(-1.0f / (attackTimeMs * 0.001f * actualUpdateRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTimeMs * 0.001f * actualUpdateRate));
}

void SpectrumAnalyzer::setSpeed(Speed s)
{
    speedMode = s;
    switch (s)
    {
        case Speed::Fast:
            attackTimeMs = 1.5f;
            releaseTimeMs = 30.0f;
            break;
        case Speed::Slow:
            attackTimeMs = 8.0f;
            releaseTimeMs = 120.0f;
            break;
        case Speed::Medium:
        default:
            attackTimeMs = 2.0f;
            releaseTimeMs = 50.0f;
            break;
    }
    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::setPeakHoldDecayTime(float seconds)
{
    peakHoldDecayTime = juce::jlimit(0.2f, 10.0f, seconds);
    // Approximate decay as 100 dB over the chosen time
    decayDBPerSecond.store(100.0f / peakHoldDecayTime);
    updateDecayFactor(30.0f); // assume ~30 Hz GUI updates
}

void SpectrumAnalyzer::setFFTResolution(Resolution res)
{
    int newOrder = static_cast<int>(res);
    if (newOrder == fftOrder)
        return;
    
    // FIX RT-SAFETY: No more rebuildFFT! Just atomic swap to pre-allocated state
    int newIndex = newOrder - 10; // Map: 10→0(Low), 11→1(Medium), 12→2(High), 13→3(Max)
    newIndex = juce::jlimit(0, 3, newIndex);
    
    // Block audio writes while we swap state/reset FIFO
    reconfiguring.store(true, std::memory_order_release);

    activeStateIndex.store(newIndex, std::memory_order_release);
    
    // Update cached values from new state
    const auto& newState = fftStates[newIndex];
    fftOrder = newState.fftOrder;
    fftSize = newState.fftSize;
    numBins = newState.numBins;
    resolution = res;
    
    // Clear FIFO and overlap buffer (safe on GUI thread)
    fifo.reset();
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    overlapBuffer.clear(); // Force full-frame read on next processFFT
    newDataAvailable.store(false, std::memory_order_release);
    updateSmoothingCoeffs(); // Recalc for new FFT size

    reconfiguring.store(false, std::memory_order_release);
}

void SpectrumAnalyzer::rebuildFFT(int newOrder)
{
    // DEPRECATED: This function is no longer needed (kept for API compatibility)
    // Use setFFTResolution() instead
    Resolution res;
    switch (newOrder)
    {
        case 10: res = Resolution::Low; break;
        case 11: res = Resolution::Medium; break;
        case 13: res = Resolution::Max; break;
        case 12:
        default: res = Resolution::High; break;
    }
    setFFTResolution(res);
}

void SpectrumAnalyzer::updateDecayFactor(float guiUpdateHz)
{
    float dbPerSec = decayDBPerSecond.load();
    decayFactor = 1.0f - (dbPerSec / (guiUpdateHz * 100.0f));
    decayFactor = juce::jlimit(0.8f, 0.999f, decayFactor);
}


//==============================================================================
// Accessors
//==============================================================================
const std::vector<float>& SpectrumAnalyzer::getSpectrum() const
{
    const auto& state = getActiveState();
    return state.spectrumBuffers[activeBufferIndex.load()];
}

const std::vector<float>& SpectrumAnalyzer::getSpectrumDB() const
{
    const auto& state = getActiveState();
    return state.spectrumDBBuffers[activeBufferIndex.load()];
}

const std::vector<float>& SpectrumAnalyzer::getPeakHold() const
{
    const auto& state = getActiveState();
    return state.peakHoldBuffers[activeBufferIndex.load()];
}

float SpectrumAnalyzer::getMagnitudeForFrequency(float frequency) const
{
    int bin = getBinForFrequency(frequency);
    if (bin >= 0 && bin < numBins)
    {
        const auto& state = getActiveState();
        return state.spectrumDBBuffers[activeBufferIndex.load()][bin];
    }
    return minDecibels;
}

int SpectrumAnalyzer::getBinForFrequency(float frequency) const
{
    int bin = static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    return juce::jlimit(0, numBins - 1, bin);
}

float SpectrumAnalyzer::getFrequencyForBin(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
}
