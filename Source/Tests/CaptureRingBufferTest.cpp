#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Core/LockFreeStructures.h"
#include <vector>

using AIEQCore::LockFreeRingBuffer;

/**
 * P2-CAPTURE-FIX - LockFreeRingBuffer is now a circular OVERWRITE retro-capture ring.
 *
 * Contract under test: readMono() returns the MOST RECENT N samples in CHRONOLOGICAL order (the actual
 * "last N seconds"), never drops on a full ring, and is non-consuming. The OLD juce::AbstractFifo version
 * returned the OLDEST samples and dropped new audio once full - every assertion below would FAIL on it.
 */
class CaptureRingBufferTest : public juce::UnitTest
{
public:
    CaptureRingBufferTest() : juce::UnitTest ("CaptureRingBuffer", "Core") {}

    void runTest() override
    {
        testMostRecentChronologicalAfterOverwrite();
        testWrapAndPartialRead();
        testBlockBiggerThanCapacity();
        testStereoMonoMix();
        testEmptyAndAvailability();
    }

private:
    // Push n samples of a 1-channel ramp continuing from `next` (so absolute values are unique & exact).
    static void pushRamp (LockFreeRingBuffer& ring, int& next, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, static_cast<float> (next + i));
        ring.push (b);
        next += n;
    }

    void testMostRecentChronologicalAfterOverwrite()
    {
        beginTest ("readMono returns the MOST RECENT N in chronological order after overwrite");

        LockFreeRingBuffer ring;
        const int cap = 10;
        ring.prepare (1, cap);

        int next = 0;
        pushRamp (ring, next, 7);
        pushRamp (ring, next, 8);
        pushRamp (ring, next, 10);   // 25 samples total (values 0..24); cap=10 -> ring holds 15..24

        std::vector<float> out;
        const int got = ring.readMono (out, cap);
        expectEquals (got, cap, "should return capacity samples");
        expectEquals ((int) out.size(), cap);
        for (int i = 0; i < cap; ++i)
            expectWithinAbsoluteError (out[(size_t) i], static_cast<float> (15 + i), 1.0e-6f,
                                       "expected the LAST N (15..24); the old FIFO returned the OLDEST");
    }

    void testWrapAndPartialRead()
    {
        beginTest ("readMono reassembles a physically WRAPPED window; partial read returns the most recent");

        LockFreeRingBuffer ring;
        const int cap = 10;
        ring.prepare (1, cap);

        int next = 0;
        pushRamp (ring, next, 6);    // values 0..5  -> ring[0..5]
        pushRamp (ring, next, 8);    // values 6..13 -> wraps: ring[6..9]=6..9, ring[0..3]=10..13; cursor=14

        // Most recent 10 span the buffer end: 4,5,6,7,8,9,10,11,12,13.
        std::vector<float> out;
        const int got = ring.readMono (out, cap);
        expectEquals (got, cap);
        for (int i = 0; i < cap; ++i)
            expectWithinAbsoluteError (out[(size_t) i], static_cast<float> (4 + i), 1.0e-6f,
                                       "wrapped window must come back in chronological order");

        // Most recent 4 = 10,11,12,13.
        ring.readMono (out, 4);
        expectEquals ((int) out.size(), 4);
        for (int i = 0; i < 4; ++i)
            expectWithinAbsoluteError (out[(size_t) i], static_cast<float> (10 + i), 1.0e-6f);
    }

    void testBlockBiggerThanCapacity()
    {
        beginTest ("a single block larger than capacity keeps its LAST `capacity` samples; readMono is non-consuming");

        LockFreeRingBuffer ring;
        const int cap = 10;
        ring.prepare (1, cap);

        juce::AudioBuffer<float> big (1, 30);
        for (int i = 0; i < 30; ++i)
            big.setSample (0, i, static_cast<float> (i));   // values 0..29
        const int written = ring.push (big);
        expectEquals (written, 30, "push reports the full block size (overwrite is normal, not a drop)");

        std::vector<float> out;
        ring.readMono (out, cap);                            // last 10 of the block = 20..29
        for (int i = 0; i < cap; ++i)
            expectWithinAbsoluteError (out[(size_t) i], static_cast<float> (20 + i), 1.0e-6f);

        std::vector<float> out2;                             // non-consuming: same window again
        ring.readMono (out2, cap);
        expectEquals ((int) out2.size(), cap);
        for (int i = 0; i < cap; ++i)
            expectWithinAbsoluteError (out2[(size_t) i], out[(size_t) i], 1.0e-6f, "readMono must not consume");
    }

    void testStereoMonoMix()
    {
        beginTest ("stereo ring mono-mixes (L+R)/2 in chronological order");

        LockFreeRingBuffer ring;
        ring.prepare (2, 8);

        juce::AudioBuffer<float> b (2, 8);
        for (int i = 0; i < 8; ++i)
        {
            b.setSample (0, i, static_cast<float> (i));
            b.setSample (1, i, static_cast<float> (i + 100));
        }
        ring.push (b);

        std::vector<float> out;
        ring.readMono (out, 8);
        for (int i = 0; i < 8; ++i)
            expectWithinAbsoluteError (out[(size_t) i], static_cast<float> (i) + 50.0f, 1.0e-6f); // (i + (i+100))/2
    }

    void testEmptyAndAvailability()
    {
        beginTest ("empty ring returns nothing; getNumReady caps at capacity after overflow");

        LockFreeRingBuffer ring;
        ring.prepare (1, 10);

        std::vector<float> out;
        expectEquals (ring.readMono (out, 5), 0, "empty ring yields 0 samples");
        expect (out.empty());
        expectEquals (ring.getNumReady(), 0);

        int next = 0;
        pushRamp (ring, next, 4);
        expectEquals (ring.getNumReady(), 4);
        pushRamp (ring, next, 20);                 // overflow
        expectEquals (ring.getNumReady(), 10, "getNumReady caps at capacity after overflow");
    }
};

// JUCE auto-registers unit tests via static construction.
static CaptureRingBufferTest gCaptureRingBufferTest;
