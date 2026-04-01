#pragma once
/**
 * LockFreeAudioFIFO — SPSC mono audio FIFO for the metrological spectrum pipeline.
 *
 * One writer (audio thread), one reader (GUI/timer thread).
 * Uses juce::AbstractFifo for index management (no locks, no allocations on push).
 *
 * Template parameter T must be a scalar type (float, double).
 * Only mono data is stored — callers must mix down to mono before pushing.
 */

#include <juce_core/juce_core.h>
#include <vector>
#include <cstring>
#include <algorithm>

template <typename T>
class LockFreeAudioFIFO
{
public:
    LockFreeAudioFIFO() = default;

    /** Prepare the FIFO with a given capacity in samples.
     *  Must be called from the message thread before first use. */
    void prepare (int capacitySamples)
    {
        jassert (capacitySamples > 0);
        fifo.setTotalSize (capacitySamples);
        buffer.assign (static_cast<size_t> (capacitySamples), T {});
    }

    /** Push numSamples mono samples into the FIFO.
     *  RT-safe: no allocations, no locks. */
    void push (const T* src, int numSamples) noexcept
    {
        const auto scope = fifo.write (numSamples);

        if (scope.blockSize1 > 0)
            std::memcpy (buffer.data() + scope.startIndex1, src,
                         static_cast<size_t> (scope.blockSize1) * sizeof (T));

        if (scope.blockSize2 > 0)
            std::memcpy (buffer.data() + scope.startIndex2, src + scope.blockSize1,
                         static_cast<size_t> (scope.blockSize2) * sizeof (T));
    }

    /** Push a multi-channel AudioBuffer as mono mix.
     *  RT-safe: mixes channels inline, no heap allocation. */
    void pushStereoMix (const juce::AudioBuffer<T>& audioBuffer) noexcept
    {
        const int numSamples = audioBuffer.getNumSamples();
        const int numCh      = audioBuffer.getNumChannels();
        if (numSamples == 0 || numCh == 0)
            return;

        const auto scope = fifo.write (numSamples);

        auto writeMono = [&] (int startIndex, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                T mono = T {};
                for (int c = 0; c < numCh; ++c)
                    mono += audioBuffer.getSample (c, i);
                buffer[static_cast<size_t> (startIndex + i)] = mono / static_cast<T> (numCh);
            }
        };

        writeMono (scope.startIndex1, scope.blockSize1);
        if (scope.blockSize2 > 0)
        {
            // Second block starts from where blockSize1 left off in the source buffer
            for (int i = 0; i < scope.blockSize2; ++i)
            {
                T mono = T {};
                for (int c = 0; c < numCh; ++c)
                    mono += audioBuffer.getSample (c, scope.blockSize1 + i);
                buffer[static_cast<size_t> (scope.startIndex2 + i)] = mono / static_cast<T> (numCh);
            }
        }
    }

    /** Pull exactly numSamples into dest.
     *  Returns the actual number of samples pulled (< numSamples if FIFO starved).
     *  Called from GUI/timer thread only. */
    size_t pullAudioBlock (T* dest, size_t numSamples) noexcept
    {
        const int available = fifo.getNumReady();
        if (available <= 0)
            return 0;

        const int toPull = std::min (static_cast<int> (numSamples), available);
        const auto scope = fifo.read (toPull);

        if (scope.blockSize1 > 0)
            std::memcpy (dest, buffer.data() + scope.startIndex1,
                         static_cast<size_t> (scope.blockSize1) * sizeof (T));

        if (scope.blockSize2 > 0)
            std::memcpy (dest + scope.blockSize1, buffer.data() + scope.startIndex2,
                         static_cast<size_t> (scope.blockSize2) * sizeof (T));

        return static_cast<size_t> (toPull);
    }

    int getNumReady() const noexcept { return fifo.getNumReady(); }
    int getCapacity() const noexcept { return fifo.getTotalSize(); }

    /** Reset FIFO state (call from message thread only). */
    void reset() noexcept { fifo.reset(); }

private:
    juce::AbstractFifo fifo { 1 };
    std::vector<T>     buffer;
};
