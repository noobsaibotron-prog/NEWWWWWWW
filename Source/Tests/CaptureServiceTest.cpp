#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/CaptureService.h"

namespace
{
juce::AudioBuffer<float> makeMonoRamp(int startValue, int numSamples)
{
    juce::AudioBuffer<float> buffer(1, numSamples);
    for (int i = 0; i < numSamples; ++i)
        buffer.setSample(0, i, static_cast<float>(startValue + i));
    return buffer;
}

void expectSequence(juce::UnitTest& test,
                    const std::vector<float>& actual,
                    std::initializer_list<float> expected)
{
    test.expectEquals(static_cast<int>(actual.size()), static_cast<int>(expected.size()));

    int i = 0;
    for (const auto value : expected)
    {
        if (i < static_cast<int>(actual.size()))
            test.expectWithinAbsoluteError(actual[static_cast<size_t>(i)], value, 1.0e-6f);
        ++i;
    }
}
} // namespace

class CaptureServiceTest final : public juce::UnitTest
{
public:
    CaptureServiceTest()
        : juce::UnitTest("CaptureService retroactive capture", "Core") {}

    void runTest() override
    {
        beginTest("LockFreeRingBuffer snapshots are most-recent and non-consuming");
        {
            AIEQCore::LockFreeRingBuffer ring;
            ring.prepare(1, 5);

            auto first = makeMonoRamp(1, 3);
            expectEquals(ring.push(first), 3);
            expectEquals(ring.getNumReady(), 3);

            std::vector<float> mono;
            expectEquals(ring.readMono(mono, 2), 2);
            expectSequence(*this, mono, { 2.0f, 3.0f });

            expectEquals(ring.readMono(mono, 3), 3);
            expectSequence(*this, mono, { 1.0f, 2.0f, 3.0f });

            auto second = makeMonoRamp(4, 4);
            expectEquals(ring.push(second), 4);
            expectEquals(ring.getNumReady(), 5);
            expectEquals(ring.readMono(mono, 5), 5);
            expectSequence(*this, mono, { 3.0f, 4.0f, 5.0f, 6.0f, 7.0f });
        }

        beginTest("CaptureService captureSnapshotMs returns the latest requested window after overflow");
        {
            AIEQCore::CaptureService capture;
            capture.prepare(10.0, 1, 4);

            auto longRamp = makeMonoRamp(0, 250);
            expectEquals(capture.pushSamples(longRamp), 0);

            capture.captureSnapshotMs(5000);
            const auto& mono = capture.getCapturedAudioMono();
            expectEquals(static_cast<int>(mono.size()), 50);

            if (mono.size() == 50)
            {
                expectWithinAbsoluteError(mono.front(), 200.0f, 1.0e-6f);
                expectWithinAbsoluteError(mono.back(), 249.0f, 1.0e-6f);
            }
        }
    }
};

static CaptureServiceTest captureServiceTest;
