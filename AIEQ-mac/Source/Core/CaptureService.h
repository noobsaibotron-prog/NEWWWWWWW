#pragma once

/**
 * CaptureService - Lock-Free Audio Capture for AI Analysis
 * 
 * Provides real-time safe audio capture using lock-free ring buffers.
 * The audio thread writes samples without blocking, while the GUI/AI thread
 * can request snapshots for analysis at any time.
 * 
 * Features:
 * - Zero allocations in audio thread after prepare()
 * - Lock-free ring buffer using juce::AbstractFifo
 * - Manual capture with pre-allocated buffer
 * - Auto-capture trigger on energy peaks
 * - Thread-safe preview for waveform display
 */

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "LockFreeStructures.h"
#include <atomic>
#include <vector>

namespace AIEQCore
{

class CaptureService
{
public:
    static constexpr int kMaxCaptureSeconds = 20;
    static constexpr int kDefaultCaptureLengthMs = 5000;
    
    CaptureService() = default;
    ~CaptureService() = default;
    
    // Non-copyable, non-movable (owns resources)
    CaptureService(const CaptureService&) = delete;
    CaptureService& operator=(const CaptureService&) = delete;
    
    //==========================================================================
    // Initialization (call from message thread during prepareToPlay)
    //==========================================================================
    
    /**
     * Prepare all buffers for the given sample rate and channel count.
     * This allocates all memory needed for capture - no allocations happen after this.
     */
    void prepare(double sampleRate, int numChannels, int maxBlockSize)
    {
        jassert(sampleRate > 0 && sampleRate <= 192000.0);
        jassert(numChannels > 0 && numChannels <= 2);
        jassert(maxBlockSize > 0);
        
        currentSampleRate = sampleRate;
        channelCount = juce::jlimit(1, 2, numChannels);
        
        // Ring buffer for retroactive capture (last N seconds), sized to handle burst blocks
        const int ringCapacity = juce::jmax(static_cast<int>(sampleRate * kMaxCaptureSeconds),
                                            maxBlockSize * 4); // keep a few blocks of headroom
        ringBuffer.prepare(channelCount, ringCapacity);
        
        // Manual capture buffer (pre-allocated, mono/stereo interleaved)
        // Size: max seconds * sample rate * channelCount (clamped to 1-2)
        const size_t manualCapacity = static_cast<size_t>(sampleRate * kMaxCaptureSeconds
                                                         * static_cast<size_t>(channelCount));
        manualCaptureBuffer.resize(manualCapacity, 0.0f);
        manualCaptureWritePos.store(0, std::memory_order_relaxed);
        manualCaptureMaxSamples = manualCapacity;
        isManualCapturing.store(false, std::memory_order_relaxed);
        
        // Reset state
        lastEnergyLevel = 0.0f;
        autoCaptureCooldown = 0;
        
        prepared = true;
    }
    
    /**
     * Release resources (call from releaseResources)
     */
    void release()
    {
        isManualCapturing.store(false, std::memory_order_relaxed);
        ringBuffer.clear();
        prepared = false;
    }
    
    //==========================================================================
    // Real-time safe methods (call from audio thread)
    //==========================================================================
    
    /**
     * Push audio samples to capture buffers (RT-SAFE)
     * Call this from processBlock for every buffer.
     * 
     * @param buffer Audio buffer to capture
     * @return Number of samples dropped due to buffer overflow (debug info)
     */
    [[nodiscard]] int pushSamples(const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (!prepared)
            return 0;
        
        const int numSamples = buffer.getNumSamples();
        int dropped = 0;
        
        // Push to ring buffer (always, for retroactive capture)
        const int written = ringBuffer.push(buffer);
        if (written < numSamples)
            dropped = numSamples - written;
        
        // Handle manual capture
        if (isManualCapturing.load(std::memory_order_acquire))
        {
            pushToManualCapture(buffer);
        }
        
        // Handle auto-capture trigger
        if (autoCaptureEnabled.load(std::memory_order_relaxed))
        {
            processAutoCaptureLogic(buffer);
        }
        
        return dropped;
    }
    
    //==========================================================================
    // Manual Capture Control (call from message thread)
    //==========================================================================
    
    /**
     * Start manual capture
     * @return true if capture started successfully
     */
    [[nodiscard]] bool startManualCapture() noexcept
    {
        // Use CAS to ensure only one capture can start
        bool expected = false;
        if (!isManualCapturing.compare_exchange_strong(expected, true,
                                                        std::memory_order_acq_rel))
        {
            return false; // Already capturing
        }
        
        // Reset write position (buffer is pre-allocated)
        manualCaptureWritePos.store(0, std::memory_order_release);
        capturedSampleRate = currentSampleRate;
        
        return true;
    }
    
    /**
     * Stop manual capture and convert to mono
     */
    void stopManualCapture()
    {
        // Atomically stop capturing
        if (!isManualCapturing.exchange(false, std::memory_order_acq_rel))
            return; // Wasn't capturing
        
        // Audio thread won't write anymore, safe to read
        const size_t totalSamples = manualCaptureWritePos.load(std::memory_order_acquire);
        
        if (totalSamples == 0)
        {
            capturedAudioMono.clear();
            return;
        }
        
        // Convert interleaved audio to mono (supports mono or stereo capture)
        const int channels = juce::jmax(1, channelCount);
        const size_t numAudioSamples = totalSamples / static_cast<size_t>(channels);
        
        capturedAudioMono.resize(numAudioSamples);
        
        for (size_t i = 0; i < numAudioSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                const size_t idx = i * static_cast<size_t>(channels) + ch;
                if (idx < totalSamples && idx < manualCaptureBuffer.size())
                {
                    sum += manualCaptureBuffer[idx];
                }
            }
            capturedAudioMono[i] = sum / static_cast<float>(channels);
        }
    }
    
    /**
     * Check if manual capture is in progress
     */
    [[nodiscard]] bool isCapturing() const noexcept
    {
        return isManualCapturing.load(std::memory_order_acquire);
    }
    
    /**
     * Get preview of manual capture (for waveform display)
     * @param outMono Output vector for downsampled mono waveform
     * @param maxSamples Maximum number of samples in preview
     */
    void getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples) const
    {
        outMono.clear();
        
        if (!isManualCapturing.load(std::memory_order_acquire) || maxSamples == 0)
            return;
        
        const size_t totalSamples = manualCaptureWritePos.load(std::memory_order_acquire);
        if (totalSamples == 0)
            return;
        
        const int channels = juce::jmax(1, channelCount);
        const size_t numAudioSamples = totalSamples / static_cast<size_t>(channels);
        
        if (numAudioSamples == 0)
            return;
        
        const size_t step = std::max<size_t>(1, numAudioSamples / maxSamples);
        outMono.reserve(std::min<size_t>(maxSamples, numAudioSamples / step + 1));
        
        for (size_t i = 0; i < numAudioSamples; i += step)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                const size_t idx = i * static_cast<size_t>(channels) + ch;
                if (idx < totalSamples && idx < manualCaptureBuffer.size())
                    sum += manualCaptureBuffer[idx];
            }
            outMono.push_back(sum / static_cast<float>(channels));
        }
    }
    
    //==========================================================================
    // Retroactive Capture (call from message thread)
    //==========================================================================
    
    /**
     * Capture the last N milliseconds of audio from ring buffer
     * @param lengthMs Length in milliseconds to capture
     */
    void captureSnapshotMs(int lengthMs)
    {
        if (!prepared || lengthMs <= 0)
            return;
        
        const int samplesRequested = static_cast<int>(currentSampleRate * 
                                                       (static_cast<double>(lengthMs) / 1000.0));
        
        ringBuffer.readMono(capturedAudioMono, samplesRequested);
        capturedSampleRate = currentSampleRate;
    }
    
    /**
     * Get the captured audio (mono)
     */
    [[nodiscard]] const std::vector<float>& getCapturedAudioMono() const noexcept
    {
        return capturedAudioMono;
    }
    
    /**
     * Get the sample rate of captured audio
     */
    [[nodiscard]] double getCapturedSampleRate() const noexcept
    {
        return capturedSampleRate;
    }
    
    //==========================================================================
    // Auto-Capture Configuration
    //==========================================================================
    
    void setAutoCaptureEnabled(bool enabled) noexcept
    {
        autoCaptureEnabled.store(enabled, std::memory_order_relaxed);
    }
    
    [[nodiscard]] bool isAutoCaptureEnabled() const noexcept
    {
        return autoCaptureEnabled.load(std::memory_order_relaxed);
    }
    
    void setEnergyThreshold(float threshold) noexcept
    {
        energyThreshold = threshold;
    }
    
    void setCaptureLengthMs(int lengthMs) noexcept
    {
        captureLengthMs = juce::jlimit(1000, kMaxCaptureSeconds * 1000, lengthMs);
    }
    
    [[nodiscard]] int getCaptureLengthMs() const noexcept
    {
        return captureLengthMs;
    }
    
    //==========================================================================
    // Debug Statistics
    //==========================================================================
    
    [[nodiscard]] uint32_t getDropCount() const noexcept
    {
        return dropCount.load(std::memory_order_relaxed);
    }
    
    void resetDropCount() noexcept
    {
        dropCount.store(0, std::memory_order_relaxed);
    }

private:
    //==========================================================================
    // RT-safe helper: push to manual capture buffer
    //==========================================================================
    void pushToManualCapture(const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int channelsToCopy = std::min(buffer.getNumChannels(), juce::jmax(1, channelCount));
        const size_t samplesToAdd = static_cast<size_t>(numSamples * channelsToCopy);
        
        const size_t currentPos = manualCaptureWritePos.load(std::memory_order_relaxed);
        
        // Check if we have space (no allocation!)
        if (currentPos + samplesToAdd > manualCaptureMaxSamples)
        {
            // Buffer full - stop capturing
            isManualCapturing.store(false, std::memory_order_release);
            return;
        }
        
        // Write interleaved samples
        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < channelsToCopy; ++ch)
            {
                const size_t idx = currentPos + static_cast<size_t>(s * channelsToCopy + ch);
                manualCaptureBuffer[idx] = buffer.getSample(ch, s);
            }
        }
        
        manualCaptureWritePos.store(currentPos + samplesToAdd, std::memory_order_release);
    }
    
    //==========================================================================
    // RT-safe helper: process auto-capture logic
    //==========================================================================
    void processAutoCaptureLogic(const juce::AudioBuffer<float>& buffer) noexcept
    {
        // Update cooldown
        if (autoCaptureCooldown > 0)
        {
            autoCaptureCooldown -= buffer.getNumSamples();
            return;
        }
        
        // Calculate energy
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        
        float energy = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                energy += data[s] * data[s];
            }
        }
        energy = std::sqrt(energy / static_cast<float>(numChannels * numSamples));
        
        // Trigger on energy spike
        if (energy > energyThreshold && energy > lastEnergyLevel * 1.5f)
        {
            if (!isManualCapturing.load(std::memory_order_relaxed))
            {
                // Start capture (atomic operation)
                bool expected = false;
                if (isManualCapturing.compare_exchange_strong(expected, true,
                                                               std::memory_order_acq_rel))
                {
                    manualCaptureWritePos.store(0, std::memory_order_release);
                    autoCaptureCooldown = static_cast<int>(currentSampleRate * 2.0); // 2s cooldown
                }
            }
        }
        
        lastEnergyLevel = energy;
    }
    
    //==========================================================================
    // State
    //==========================================================================
    bool prepared = false;
    double currentSampleRate = 44100.0;
    int channelCount = 2;
    
    // Lock-free ring buffer for retroactive capture
    LockFreeRingBuffer ringBuffer;
    
    // Manual capture (pre-allocated, no allocations in RT)
    std::vector<float> manualCaptureBuffer;  // Pre-allocated in prepare()
    std::atomic<size_t> manualCaptureWritePos { 0 };
    size_t manualCaptureMaxSamples = 0;
    std::atomic<bool> isManualCapturing { false };
    
    // Output buffer (filled by stopManualCapture or captureSnapshotMs)
    std::vector<float> capturedAudioMono;
    double capturedSampleRate = 44100.0;
    
    // Auto-capture
    std::atomic<bool> autoCaptureEnabled { false };
    float energyThreshold = 0.3f;
    float lastEnergyLevel = 0.0f;
    int autoCaptureCooldown = 0;
    int captureLengthMs = kDefaultCaptureLengthMs;
    
    // Debug
    std::atomic<uint32_t> dropCount { 0 };
};

} // namespace AIEQCore

