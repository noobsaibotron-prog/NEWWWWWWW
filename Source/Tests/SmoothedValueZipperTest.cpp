#if JUCE_UNIT_TESTS

#include <juce_dsp/juce_dsp.h>

/** Simple continuity test for juce::SmoothedValue to spot zippering artifacts.
    It simulates a fast automation ramp and checks that successive samples
    are not jumping by an excessive delta.
*/
class SmoothedValueZipperTest : public juce::UnitTest
{
public:
    SmoothedValueZipperTest() : juce::UnitTest("SmoothedValueZipperTest", "DSP") {}

    void runTest() override
    {
        beginTest("Smoothing continuity");

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smooth;
        const double sampleRate = 48000.0;
        smooth.reset(sampleRate, 0.01); // 10 ms ramp
        smooth.setCurrentAndTargetValue(0.0f);
        smooth.setTargetValue(1.0f);

        const int totalSamples = static_cast<int>(sampleRate * 0.02); // 20 ms window
        float prev = smooth.getNextValue(); // first sample
        float maxDelta = 0.0f;

        for (int n = 1; n < totalSamples; ++n)
        {
            float v = smooth.getNextValue();
            maxDelta = juce::jmax(maxDelta, std::abs(v - prev));
            prev = v;
        }

        // In a 10 ms linear ramp from 0 -> 1, per-sample step at 48kHz is ~0.0021
        expect(maxDelta < 0.01f, "Detected large step – potential zippering");
    }
};

static SmoothedValueZipperTest smoothedValueZipperTest;

#endif // JUCE_UNIT_TESTS

