#if JUCE_UNIT_TESTS

#include <juce_dsp/juce_dsp.h>
#include "../DSP/LinearPhaseProcessor.h"

/** Basic smoke test for the linear-phase IR builder.
    - Builds a flat-magnitude IR
    - Processes an impulse
    - Verifies no NaN/Inf and non-zero output
*/
class LinearPhaseIRSmokeTest : public juce::UnitTest
{
public:
    LinearPhaseIRSmokeTest() : juce::UnitTest("LinearPhaseIRSmokeTest", "DSP") {}

    void runTest() override
    {
        beginTest("IR build and process impulse");

        LinearPhaseProcessor lp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        lp.prepare(spec);

        // Flat 0 dB magnitude for N/2 bins
        std::vector<float> mag(LinearPhaseProcessor::fftSize / 2, 0.0f);
        lp.updateImpulseResponse(mag, spec.sampleRate);

        // Process enough samples to fill at least one OLA hop (hopSize = 4096)
        // and drain some output. Use multiple blocks.
        const int hopSize = static_cast<int>(LinearPhaseProcessor::hopSize);
        juce::AudioBuffer<float> buf(2, hopSize);

        float maxAbs = 0.0f;
        for (int pass = 0; pass < 4; ++pass)
        {
            buf.clear();
            if (pass == 0)
            {
                buf.setSample(0, 0, 1.0f);
                buf.setSample(1, 0, 1.0f);
            }
            juce::dsp::AudioBlock<float> block(buf);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp.process(ctx);

            auto* p = buf.getReadPointer(0);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                expect(std::isfinite(p[i]), "Found non-finite sample");
                maxAbs = juce::jmax(maxAbs, std::abs(p[i]));
            }
        }
        expect(maxAbs > 0.0f, "IR output is silent");

        beginTest("Boosted magnitude still finite");
        // Add a +6 dB bump around bin 1000
        const size_t bin = 1000;
        if (bin < mag.size())
            mag[bin] = 6.0f;
        lp.updateImpulseResponse(mag, spec.sampleRate);

        maxAbs = 0.0f;
        for (int pass = 0; pass < 4; ++pass)
        {
            buf.clear();
            if (pass == 0)
            {
                buf.setSample(0, 0, 1.0f);
                buf.setSample(1, 0, 1.0f);
            }
            juce::dsp::AudioBlock<float> block2(buf);
            juce::dsp::ProcessContextReplacing<float> ctx2(block2);
            lp.process(ctx2);

            auto* p = buf.getReadPointer(0);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                expect(std::isfinite(p[i]), "Found non-finite sample after boost");
                maxAbs = juce::jmax(maxAbs, std::abs(p[i]));
            }
        }
        expect(maxAbs > 0.0f, "Boosted IR output is silent");
    }
};

static LinearPhaseIRSmokeTest linearPhaseIRSmokeTest;

#endif // JUCE_UNIT_TESTS

