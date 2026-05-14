#pragma once

/**
 * Lock-Free Data Structures for Real-Time Audio Processing
 * 
 * Implements wait-free and lock-free patterns for professional audio plugin development.
 * All structures are designed for single-producer/single-consumer scenarios typical
 * in audio processing (audio thread writes, GUI/AI thread reads).
 * 
 * Standards: Real-time safe, zero heap allocation after initialization, cache-aligned.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <cstring>
#include <type_traits>

namespace AIEQCore
{

//==============================================================================
/**
 * Cache line size for alignment (64 bytes for most modern CPUs, 128 for Apple M1/M2)
 */
#if defined(__APPLE__) && defined(__aarch64__)
    static constexpr size_t kCacheLineSize = 128;
#else
    static constexpr size_t kCacheLineSize = 64;
#endif

//==============================================================================
/**
 * AtomicSnapshot<T> - Triple-buffered atomic state exchange
 * 
 * Allows the audio thread to read state without locking while another thread
 * updates it. Uses three buffers: one for reading, one for writing, one ready.
 * 
 * Pattern: Producer (GUI/AI) writes to back buffer, swaps to ready.
 *          Consumer (Audio) swaps ready to front, reads from front.
 * 
 * Requirements: T must be trivially copyable.
 */
template<typename T>
class alignas(kCacheLineSize) AtomicSnapshot
{
public:
    static_assert(std::is_trivially_copyable_v<T>, 
                  "AtomicSnapshot<T> requires trivially copyable type");

    AtomicSnapshot()
    {
        buffers[0] = T{};
        buffers[1] = T{};
        buffers[2] = T{};
        readIndex.store(0, std::memory_order_relaxed);
        writeIndex.store(1, std::memory_order_relaxed);
        readyIndex.store(2, std::memory_order_relaxed);
    }

    /**
     * Publish new state (call from producer thread only)
     * Lock-free: completes in bounded time on any single thread,
     * but uses a CAS retry loop (not strictly wait-free).
     */
    void publish(const T& newState) noexcept
    {
        const int writeIdx = writeIndex.load(std::memory_order_relaxed);
        buffers[writeIdx] = newState;

        // Swap write and ready indices (bounded retry for RT safety)
        int expected = readyIndex.load(std::memory_order_relaxed);
        int retries = 0;
        while (!readyIndex.compare_exchange_weak(expected, writeIdx,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)
               && ++retries < 16)
        {
            // CAS failed, expected updated to current value, retry
        }

        // If CAS failed after max retries, force-store to prevent audio thread stall
        if (retries >= 16)
            readyIndex.store(writeIdx, std::memory_order_release);

        writeIndex.store(expected, std::memory_order_relaxed);
    }

    /**
     * Read latest state (call from consumer thread only)
     * Wait-free: always completes in bounded time
     */
    [[nodiscard]] T read() const noexcept
    {
        // Try to swap readIndex with readyIndex to get the latest published buffer.
        //
        // CAS semantics:
        //  - If CAS succeeds: readyIndex was `expected` (the ready buffer), now becomes
        //    `currentRead` (our old read buffer). `expected` still holds the ready index
        //    → we adopt it as our new read buffer.
        //  - If CAS fails: another thread changed readyIndex between our load and CAS.
        //    `expected` is updated to the NEW readyIndex value (still the latest ready
        //    buffer) → we adopt it as our new read buffer.
        //  - In both cases: `expected` ends up pointing to the newest data.
        //
        // This is wait-free: at most one CAS attempt, no retry loop.
        int expected = readyIndex.load(std::memory_order_acquire);
        int currentRead = readIndex.load(std::memory_order_relaxed);

        if (expected != currentRead)
        {
            readyIndex.compare_exchange_strong(expected, currentRead,
                                                std::memory_order_release,
                                                std::memory_order_acquire);
            if (expected != currentRead)
            {
                readIndex.store(expected, std::memory_order_relaxed);
            }
        }

        return buffers[readIndex.load(std::memory_order_relaxed)];
    }

    /**
     * Get current read buffer without swap attempt (fastest path)
     */
    [[nodiscard]] const T& readCurrent() const noexcept
    {
        return buffers[readIndex.load(std::memory_order_acquire)];
    }

private:
    std::array<T, 3> buffers;
    mutable std::atomic<int> readIndex;
    std::atomic<int> writeIndex;
    mutable std::atomic<int> readyIndex;
    
    // Padding to prevent false sharing
    char padding[kCacheLineSize - sizeof(std::atomic<int>) * 3];
};

//==============================================================================
/**
 * SPSCQueue - Single-Producer Single-Consumer Lock-Free Queue
 * 
 * Fixed-capacity queue for passing commands/data between threads without locking.
 * Uses power-of-2 size for fast modulo via bit masking.
 * 
 * Typical use: GUI/AI thread enqueues commands, audio thread dequeues and executes.
 */
template<typename T, size_t Capacity>
class alignas(kCacheLineSize) SPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0, 
                  "SPSCQueue capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SPSCQueue<T> requires trivially copyable type");

public:
    SPSCQueue() : head(0), tail(0) {}

    /**
     * Try to enqueue an item (producer only)
     * @return true if successful, false if queue is full
     */
    [[nodiscard]] bool tryPush(const T& item) noexcept
    {
        const size_t currentTail = tail.load(std::memory_order_relaxed);
        const size_t nextTail = (currentTail + 1) & kMask;
        
        if (nextTail == head.load(std::memory_order_acquire))
            return false; // Queue full
        
        buffer[currentTail] = item;
        tail.store(nextTail, std::memory_order_release);
        return true;
    }

    /**
     * Try to dequeue an item (consumer only)
     * @return true if successful, false if queue is empty
     */
    [[nodiscard]] bool tryPop(T& item) noexcept
    {
        const size_t currentHead = head.load(std::memory_order_relaxed);
        
        if (currentHead == tail.load(std::memory_order_acquire))
            return false; // Queue empty
        
        item = buffer[currentHead];
        head.store((currentHead + 1) & kMask, std::memory_order_release);
        return true;
    }

    /**
     * Check if queue is empty (approximate, for UI feedback only)
     */
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return head.load(std::memory_order_relaxed) == tail.load(std::memory_order_relaxed);
    }

    /**
     * Get approximate number of items in queue
     */
    [[nodiscard]] size_t sizeApprox() const noexcept
    {
        const size_t h = head.load(std::memory_order_relaxed);
        const size_t t = tail.load(std::memory_order_relaxed);
        return (t - h) & kMask;
    }

    /**
     * Clear all items (producer only, when consumer is not running)
     */
    void clear() noexcept
    {
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr size_t kMask = Capacity - 1;
    
    alignas(kCacheLineSize) std::atomic<size_t> head;
    alignas(kCacheLineSize) std::atomic<size_t> tail;
    alignas(kCacheLineSize) std::array<T, Capacity> buffer;
};

//==============================================================================
/**
 * LockFreeRingBuffer - Real-time safe audio capture ring buffer
 * 
 * Uses juce::AbstractFifo internally for efficient lock-free operation.
 * Pre-allocates all memory in prepare() to ensure zero allocations in push().
 */
class LockFreeRingBuffer
{
public:
    LockFreeRingBuffer() = default;
    
    /**
     * Initialize the ring buffer (call from message thread during prepare)
     * @param numChannels Number of audio channels (1 or 2)
     * @param numSamples Total capacity in samples per channel
     */
    void prepare(int numChannels, int numSamples)
    {
        jassert(numChannels > 0 && numChannels <= 2);
        jassert(numSamples > 0);
        
        channels = numChannels;
        fifo.setTotalSize(numSamples);
        
        buffer.setSize(numChannels, numSamples, false, true, false);
        buffer.clear();
    }

    /**
     * Push audio samples into the ring buffer (audio thread, real-time safe)
     * @return Number of samples actually written (may be less if buffer full)
     */
    int push(const juce::AudioBuffer<float>& source) noexcept
    {
        const int numSamples = source.getNumSamples();
        if (numSamples == 0)
            return 0;
        
        const auto scope = fifo.write(numSamples);
        
        if (scope.blockSize1 > 0)
        {
            for (int ch = 0; ch < std::min(channels, source.getNumChannels()); ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch) + scope.startIndex1,
                           source.getReadPointer(ch),
                           static_cast<size_t>(scope.blockSize1) * sizeof(float));
            }
        }
        
        if (scope.blockSize2 > 0)
        {
            for (int ch = 0; ch < std::min(channels, source.getNumChannels()); ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch) + scope.startIndex2,
                           source.getReadPointer(ch) + scope.blockSize1,
                           static_cast<size_t>(scope.blockSize2) * sizeof(float));
            }
        }
        
        return scope.blockSize1 + scope.blockSize2;
    }

    /**
     * Read samples from the ring buffer (non-audio thread)
     * Reads the most recent N samples, converting to mono if requested.
     * @param destMono Output vector for mono samples
     * @param numSamples Number of samples to read
     * @return Actual number of samples read
     */
    int readMono(std::vector<float>& destMono, int numSamples)
    {
        const int available = fifo.getNumReady();
        const int toRead = std::min(numSamples, available);
        
        if (toRead == 0)
        {
            destMono.clear();
            return 0;
        }
        
        destMono.resize(static_cast<size_t>(toRead));
        
        // Calculate start position for most recent samples
        const auto scope = fifo.read(toRead);
        
        // Read and convert to mono
        int outIdx = 0;
        
        auto readBlock = [&](int startIndex, int blockSize)
        {
            for (int i = 0; i < blockSize; ++i, ++outIdx)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                {
                    sum += buffer.getSample(ch, startIndex + i);
                }
                destMono[static_cast<size_t>(outIdx)] = sum / static_cast<float>(channels);
            }
        };
        
        if (scope.blockSize1 > 0)
            readBlock(scope.startIndex1, scope.blockSize1);
        if (scope.blockSize2 > 0)
            readBlock(scope.startIndex2, scope.blockSize2);
        
        return toRead;
    }

    /**
     * Get number of samples currently available
     */
    [[nodiscard]] int getNumReady() const noexcept
    {
        return fifo.getNumReady();
    }

    /**
     * Clear the buffer
     */
    void clear() noexcept
    {
        fifo.reset();
    }

private:
    juce::AbstractFifo fifo { 1 }; // Will be resized in prepare()
    juce::AudioBuffer<float> buffer;
    int channels = 2;
};

//==============================================================================
/**
 * ParameterBlock - Cached parameter values for a single processBlock call
 * 
 * Loads all atomic parameter values once at the start of processBlock,
 * then uses local copies throughout processing to avoid repeated atomic loads.
 */
struct alignas(kCacheLineSize) BandParameterBlock
{
    float frequency = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
    int filterType = 2;  // Peak
    bool enabled = true;
    
    // Dynamic EQ parameters
    int dynamicMode = 0;  // Off
    float threshold = -20.0f;
    float ratio = 2.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float range = 24.0f;
    float knee = 6.0f;
};

static constexpr int kMaxBands = 24;

struct alignas(kCacheLineSize) ProcessBlockParameters
{
    std::array<BandParameterBlock, kMaxBands> bands;
    int numActiveBands = 8;
    float outputGainDB = 0.0f;
    float autoGainCompensationDB = 0.0f;
    bool autoGainEnabled = false;
    bool dynamicEQEnabled = true;
    bool bypassed = false;
    int phaseMode = 0;  // 0=ZeroLatency, 1=NaturalPhase, 2=LinearPhase
    int msMode = 0;     // 0=Stereo, 1=Mid, 2=Side, 3=MSLinked
    int oversamplingFactor = 0;
    
    // Version counter for change detection
    uint64_t version = 0;
};

//==============================================================================
/**
 * Command types for AI-to-Audio thread communication
 */
enum class AICommandType : uint8_t
{
    None = 0,
    ApplyCorrection,
    ClearCorrections,
    SetBandParameter,
    TriggerAnalysis
};

/**
 * AICommand - Single command from AI/GUI thread to audio thread
 * Fixed size for efficient queue storage
 */
struct alignas(16) AICommand
{
    AICommandType type = AICommandType::None;
    int bandIndex = 0;
    float frequency = 0.0f;
    float gain = 0.0f;
    float q = 0.0f;
    int filterType = 0;
    
    static AICommand makeCorrection(int band, float freq, float gainDB, float qVal, int type)
    {
        AICommand cmd;
        cmd.type = AICommandType::ApplyCorrection;
        cmd.bandIndex = band;
        cmd.frequency = freq;
        cmd.gain = gainDB;
        cmd.q = qVal;
        cmd.filterType = type;
        return cmd;
    }
    
    static AICommand makeClear()
    {
        AICommand cmd;
        cmd.type = AICommandType::ClearCorrections;
        return cmd;
    }
};

// Command queue with 64 slots (power of 2)
using AICommandQueue = SPSCQueue<AICommand, 64>;

//==============================================================================
/**
 * AtomicBandState - Lock-free band state with version counter
 * 
 * Uses a version counter to detect partial reads. The reader checks the version
 * before and after reading; if they differ, the read was interrupted by a write.
 */
struct AtomicBandState
{
    std::atomic<uint64_t> version { 0 };
    
    std::atomic<float> frequency { 1000.0f };
    std::atomic<float> gain { 0.0f };
    std::atomic<float> q { 1.0f };
    std::atomic<int> type { 2 };
    std::atomic<bool> enabled { true };
    
    /**
     * Write new state (producer only)
     */
    void write(float freq, float g, float qVal, int t, bool en) noexcept
    {
        // relaxed: we are not reading any shared data here, just incrementing the version
        version.fetch_add(1, std::memory_order_relaxed);
        
        frequency.store(freq, std::memory_order_relaxed);
        gain.store(g, std::memory_order_relaxed);
        q.store(qVal, std::memory_order_relaxed);
        type.store(t, std::memory_order_relaxed);
        enabled.store(en, std::memory_order_relaxed);
        
        version.fetch_add(1, std::memory_order_release);
    }
    
    /**
     * Read state with consistency check
     * @return true if read was consistent (version didn't change during read)
     */
    [[nodiscard]] bool tryRead(float& freq, float& g, float& qVal, int& t, bool& en) const noexcept
    {
        const uint64_t v1 = version.load(std::memory_order_acquire);
        
        // Check if write is in progress (odd version)
        if (v1 & 1)
            return false;
        
        freq = frequency.load(std::memory_order_relaxed);
        g = gain.load(std::memory_order_relaxed);
        qVal = q.load(std::memory_order_relaxed);
        t = type.load(std::memory_order_relaxed);
        en = enabled.load(std::memory_order_relaxed);
        
        const uint64_t v2 = version.load(std::memory_order_acquire);
        
        return v1 == v2;
    }
    
    /**
     * Read state, retrying if inconsistent (bounded retries)
     */
    void readConsistent(float& freq, float& g, float& qVal, int& t, bool& en) const noexcept
    {
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            if (tryRead(freq, g, qVal, t, en))
                return;
        }
        
        // Fallback: just read (may be slightly inconsistent, but safe)
        freq = frequency.load(std::memory_order_relaxed);
        g = gain.load(std::memory_order_relaxed);
        qVal = q.load(std::memory_order_relaxed);
        t = type.load(std::memory_order_relaxed);
        en = enabled.load(std::memory_order_relaxed);
    }
};

} // namespace AIEQCore

