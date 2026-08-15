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
    // Lifecycle-only path. The processor has already stopped audio callbacks,
    // while this mutex excludes a GUI processFFT() that may have started just
    // before processorReady was lowered.
    std::lock_guard<std::mutex> lock(processingMutex);
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    resetUnlocked();
}

void SpectrumAnalyzer::reset()
{
    std::lock_guard<std::mutex> lock(processingMutex);
    resetUnlocked();
}

void SpectrumAnalyzer::resetUnlocked()
{
    fifo.reset();
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    overlapBuffer.clear();
    lastProcessMs = 0.0;

    // Do not write GUI-visible FFT vectors from a host lifecycle thread. A
    // message-thread timer may have passed its processorReady guard immediately
    // before the lifecycle transition. Defer visual-buffer clearing until the
    // next GUI-owned processFFT() pass.
    visualResetPending = true;

    // The legacy RT snapshot consumer is quiescent because processorReady is
    // already false under the B1 lifecycle contract.
    rtSnapshotMailbox.reset();
    rtLastConsumedSnapshot = {};
    rtHasLastConsumedSnapshot = false;
    newDataAvailable.store(false, std::memory_order_release);
}

void SpectrumAnalyzer::applyPendingVisualReset()
{
    if (!visualResetPending)
        return;

    for (auto& state : fftStates)
    {
        std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
        for (int i = 0; i < 2; ++i)
        {
            std::fill(state.spectrumBuffers[i].begin(), state.spectrumBuffers[i].end(), 0.0f);
            std::fill(state.spectrumDBBuffers[i].begin(), state.spectrumDBBuffers[i].end(), minDecibels);
            std::fill(state.peakHoldBuffers[i].begin(), state.peakHoldBuffers[i].end(), minDecibels);
        }
    }
    activeBufferIndex.store(0, std::memory_order_relaxed);
    overlapBuffer.clear();
    visualResetPending = false;
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

    // GUI/lifecycle serialization only. The audio thread never takes this lock;
    // its sole analyzer operation is the SPSC FIFO producer in pushSamples().
    std::lock_guard<std::mutex> lock(processingMutex);
    applyRequestedConfiguration();
    applyPendingVisualReset();

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

    newDataAvailable.store(false, std::memory_order_release);

    if (framesProcessed > 0)
    {
        const int publishedBuffer = activeBufferIndex.load(std::memory_order_acquire);
        publishRTSnapshot(state, publishedBuffer);

        // FIX 2: Bump version so GUI knows spectrum paths need rebuild
        spectrumVersion.fetch_add(1, std::memory_order_release);
    }

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
    const float sr = static_cast<float>(std::max(44100.0, currentSampleRate.load(std::memory_order_relaxed)));
    const float hopSize = static_cast<float>(fftSize) / 2.0f;
    const float actualUpdateRate = std::max(10.0f, sr / hopSize);
    attackCoeff = 1.0f - std::exp(-1.0f / (attackTimeMs * 0.001f * actualUpdateRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTimeMs * 0.001f * actualUpdateRate));
}

void SpectrumAnalyzer::setSpeed(Speed s) noexcept
{
    const int idx = (s == Speed::Fast) ? 0 : (s == Speed::Slow ? 2 : 1);
    requestedSpeedIndex.store(idx, std::memory_order_release);
}

void SpectrumAnalyzer::setPeakHoldDecayTime(float seconds)
{
    peakHoldDecayTime = juce::jlimit(0.2f, 10.0f, seconds);
    // Approximate decay as 100 dB over the chosen time
    decayDBPerSecond.store(100.0f / peakHoldDecayTime);
    updateDecayFactor(30.0f); // assume ~30 Hz GUI updates
}

void SpectrumAnalyzer::setFFTResolution(Resolution res) noexcept
{
    const int idx = juce::jlimit(0, 3, static_cast<int>(res) - 10);
    requestedStateIndex.store(idx, std::memory_order_release);
}

void SpectrumAnalyzer::applyRequestedConfiguration()
{
    const int newIndex = juce::jlimit(0, 3, requestedStateIndex.load(std::memory_order_acquire));
    const int currentIndex = activeStateIndex.load(std::memory_order_relaxed);

    if (newIndex != currentIndex)
    {
        auto& newState = fftStates[static_cast<size_t>(newIndex)];
        activeStateIndex.store(newIndex, std::memory_order_release);
        fftOrder = newState.fftOrder;
        fftSize = newState.fftSize;
        numBins = newState.numBins;
        resolution = static_cast<Resolution>(newState.fftOrder);

        // Resolution changes are GUI-owned. Do NOT reset the shared SPSC FIFO:
        // a producer may be inside pushSamples(). Keeping queued time-domain
        // samples is safe; clearing overlap forces a clean full-size first frame.
        overlapBuffer.clear();
        std::fill(newState.fftData.begin(), newState.fftData.end(), 0.0f);
        for (int i = 0; i < 2; ++i)
        {
            std::fill(newState.spectrumBuffers[i].begin(), newState.spectrumBuffers[i].end(), 0.0f);
            std::fill(newState.spectrumDBBuffers[i].begin(), newState.spectrumDBBuffers[i].end(), minDecibels);
            std::fill(newState.peakHoldBuffers[i].begin(), newState.peakHoldBuffers[i].end(), minDecibels);
        }
        activeBufferIndex.store(0, std::memory_order_relaxed);
        newDataAvailable.store(false, std::memory_order_release);
        updateSmoothingCoeffs();
    }

    const int requestedSpeed = juce::jlimit(0, 2, requestedSpeedIndex.load(std::memory_order_acquire));
    const int currentSpeed = (speedMode == Speed::Fast) ? 0 : (speedMode == Speed::Slow ? 2 : 1);
    if (requestedSpeed != currentSpeed)
    {
        speedMode = requestedSpeed == 0 ? Speed::Fast : (requestedSpeed == 2 ? Speed::Slow : Speed::Medium);
        switch (speedMode)
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

void SpectrumAnalyzer::publishRTSnapshot(const FFTState& state, int bufferIndex) noexcept
{
    RTSpectrumSnapshot snapshot {};
    snapshot.numBins = juce::jlimit(0, maxSnapshotBins, state.numBins);
    if (snapshot.numBins > 0)
    {
        const auto& src = state.spectrumDBBuffers[bufferIndex];
        std::copy_n(src.begin(), snapshot.numBins, snapshot.db.begin());
    }
    (void) rtSnapshotMailbox.publish(snapshot);
}

int SpectrumAnalyzer::copySmoothedSpectrumInto(std::vector<float>& dst) const noexcept
{
    // Single legacy RT consumer (the audio-thread AI feed). The GUI publishes
    // immutable snapshots through an ownership mailbox; unlike the historical
    // two-buffer index seqlock, a writer can never reclaim a READING slot, so
    // 0->1->0 buffer flips cannot create a torn copy.
    auto view = rtSnapshotMailbox.acquireLatest();
    if (view)
    {
        rtLastConsumedSnapshot = *view.payload;
        rtHasLastConsumedSnapshot = true;
        rtSnapshotMailbox.release(view);
    }

    if (!rtHasLastConsumedSnapshot)
        return 0;

    const int n = static_cast<int>(std::min(dst.size(),
        static_cast<size_t>(juce::jmax(0, rtLastConsumedSnapshot.numBins))));
    if (n > 0)
        std::copy_n(rtLastConsumedSnapshot.db.begin(), n, dst.begin());
    return n;
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
    int bin = static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate.load(std::memory_order_relaxed)));
    return juce::jlimit(0, numBins - 1, bin);
}

float SpectrumAnalyzer::getFrequencyForBin(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(currentSampleRate.load(std::memory_order_relaxed)) / static_cast<float>(fftSize);
}
