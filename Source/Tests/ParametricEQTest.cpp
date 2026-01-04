#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/ParametricEQProcessor.h"
#include "../Core/LockFreeStructures.h"
#include <cmath>
#include <limits>

/**
 * Comprehensive tests for ParametricEQProcessor (DSP category).
 * Focuses on correctness, stability, and edge cases without external deps.
 */
class ParametricEQTest : public juce::UnitTest
{
public:
    ParametricEQTest() : juce::UnitTest("ParametricEQProcessor", "DSP") {}

    void runTest() override
    {
        testBasicGainBoost();
        testBasicGainCut();
        testAllFilterTypes();
        testFrequencyAccuracy();
        testQFactorBehavior();
        testMultiBandInteraction();
        testNumericalStability();
        testParameterChanges();
        testEdgeCases();
        testBypassFunctionality();
        testDifferentSampleRates();
        testDifferentBufferSizes();
    }

private:
    static constexpr double kDefaultSampleRate = 48000.0;
    static constexpr int kDefaultBlockSize = 512;
    static constexpr int kDefaultChannels = 2;
    static constexpr float kTolerance = 0.5f; // dB tolerance for measurements

    juce::AudioBuffer<float> generateSineWave(float frequency, double sampleRate, int numSamples, int channels = 2)
    {
        juce::AudioBuffer<float> buffer(channels, numSamples);
        const float angularFreq = juce::MathConstants<float>::twoPi * frequency / static_cast<float>(sampleRate);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::sin(angularFreq * static_cast<float>(i));
        }
        return buffer;
    }

    juce::AudioBuffer<float> generateImpulse(int numSamples, int channels = 2)
    {
        juce::AudioBuffer<float> buffer(channels, numSamples);
        buffer.clear();
        for (int ch = 0; ch < channels; ++ch)
            buffer.setSample(ch, 0, 1.0f);
        return buffer;
    }

    float measureRMS(const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        const float* data = buffer.getReadPointer(channel);
        const int numSamples = buffer.getNumSamples();
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sumSquares += data[i] * data[i];
        return std::sqrt(sumSquares / static_cast<float>(numSamples));
    }

    float linearToDb(float linear) { return 20.0f * std::log10(std::max(linear, 1e-10f)); }

    bool isBufferClean(const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (std::isnan(data[i]) || std::isinf(data[i]))
                    return false;
        }
        return true;
    }

    bool hasNoDenormals(const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (data[i] != 0.0f && std::abs(data[i]) < std::numeric_limits<float>::min())
                    return false;
        }
        return true;
    }

    void testBasicGainBoost()
    {
        beginTest("Peak filter applies positive gain correctly");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);

        const float targetFreq = 1000.0f;
        const float gainDb = 6.0f;
        const float q = 1.0f;

        const int band = proc.addBand(targetFreq, gainDb, q, ParametricEQProcessor::Peak);
        proc.setBandEnabled(band, true);

        auto buffer = generateSineWave(targetFreq, kDefaultSampleRate, kDefaultBlockSize);
        const float inputRMS = measureRMS(buffer);

        for (int block = 0; block < 8; ++block)
        {
            auto blockBuffer = generateSineWave(targetFreq, kDefaultSampleRate, kDefaultBlockSize);
            proc.process(blockBuffer);
            if (block == 7)
            {
                const float outputRMS = measureRMS(blockBuffer);
                const float measuredGainDb = linearToDb(outputRMS / inputRMS);
                expectWithinAbsoluteError(measuredGainDb, gainDb, kTolerance + 1.0f);
            }
        }
    }

    void testBasicGainCut()
    {
        beginTest("Peak filter applies negative gain correctly");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);

        const int band = proc.addBand(1000.0f, -6.0f, 1.0f, ParametricEQProcessor::Peak);
        proc.setBandEnabled(band, true);

        for (int block = 0; block < 8; ++block)
        {
            auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
            proc.process(buffer);
        }

        auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float inputRMS = measureRMS(buffer);
        proc.process(buffer);
        const float outputRMS = measureRMS(buffer);
        expectLessThan(outputRMS, inputRMS);
    }

    void testAllFilterTypes()
    {
        beginTest("All filter types initialize and process without error");
        const std::vector<ParametricEQProcessor::FilterType> types = {
            ParametricEQProcessor::Peak,
            ParametricEQProcessor::LowShelf,
            ParametricEQProcessor::HighShelf,
            ParametricEQProcessor::HighCut,   // low-pass
            ParametricEQProcessor::LowCut,    // high-pass
            ParametricEQProcessor::Notch};

        for (auto type : types)
        {
            ParametricEQProcessor proc;
            proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
            const int band = proc.addBand(1000.0f, 0.0f, 1.0f, type);
            proc.setBandEnabled(band, true);
            auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
            proc.process(buffer);
            expect(isBufferClean(buffer));
        }

        beginTest("LowShelf boosts low frequencies");
        ParametricEQProcessor procLow;
        procLow.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procLow.setBandEnabled(procLow.addBand(500.0f, 6.0f, 0.707f, ParametricEQProcessor::LowShelf), true);
        auto lowBuffer = generateSineWave(100.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float lowInputRMS = measureRMS(lowBuffer);
        procLow.process(lowBuffer);
        expectGreaterThan(measureRMS(lowBuffer), lowInputRMS);

        beginTest("HighShelf boosts high frequencies");
        ParametricEQProcessor procHigh;
        procHigh.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procHigh.setBandEnabled(procHigh.addBand(2000.0f, 6.0f, 0.707f, ParametricEQProcessor::HighShelf), true);
        auto highBuffer = generateSineWave(8000.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float highInputRMS = measureRMS(highBuffer);
        procHigh.process(highBuffer);
        expectGreaterThan(measureRMS(highBuffer), highInputRMS);

        beginTest("Notch filter attenuates target frequency");
        ParametricEQProcessor procNotch;
        procNotch.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procNotch.setBandEnabled(procNotch.addBand(1000.0f, -24.0f, 10.0f, ParametricEQProcessor::Notch), true);
        for (int i = 0; i < 8; ++i)
        {
            auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
            procNotch.process(buffer);
        }
        auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float inputRMS = measureRMS(buffer);
        procNotch.process(buffer);
        // Allow generous margin; just ensure the notch attenuates audibly.
        expectLessThan(measureRMS(buffer) / inputRMS, 0.7f);
    }

    void testFrequencyAccuracy()
    {
        beginTest("Filter affects target frequency more than off-frequency");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        proc.setBandEnabled(proc.addBand(1000.0f, 12.0f, 1.0f, ParametricEQProcessor::Peak), true);

        auto targetBuffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize * 8);
        const float targetInputRMS = measureRMS(targetBuffer);
        proc.process(targetBuffer);
        const float targetGain = measureRMS(targetBuffer) / targetInputRMS;

        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        proc.setBandEnabled(proc.addBand(1000.0f, 12.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto offBuffer = generateSineWave(5000.0f, kDefaultSampleRate, kDefaultBlockSize * 8);
        const float offInputRMS = measureRMS(offBuffer);
        proc.process(offBuffer);
        const float offGain = measureRMS(offBuffer) / offInputRMS;

        expectGreaterThan(targetGain, offGain);
    }

    void testQFactorBehavior()
    {
        beginTest("Higher Q produces narrower bandwidth");
        ParametricEQProcessor procNarrow;
        procNarrow.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procNarrow.setBandEnabled(procNarrow.addBand(1000.0f, 12.0f, 8.0f, ParametricEQProcessor::Peak), true);

        ParametricEQProcessor procWide;
        procWide.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procWide.setBandEnabled(procWide.addBand(1000.0f, 12.0f, 0.5f, ParametricEQProcessor::Peak), true);

        const float testFreq = 2800.0f;
        auto narrowBuffer = generateSineWave(testFreq, kDefaultSampleRate, kDefaultBlockSize * 8);
        procNarrow.process(narrowBuffer);
        auto wideBuffer = generateSineWave(testFreq, kDefaultSampleRate, kDefaultBlockSize * 8);
        procWide.process(wideBuffer);
        expectGreaterThan(measureRMS(wideBuffer), measureRMS(narrowBuffer));
    }

    void testMultiBandInteraction()
    {
        beginTest("Multiple bands can be added and process correctly");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        proc.addBand(100.0f, 3.0f, 1.0f, ParametricEQProcessor::LowShelf);
        proc.addBand(500.0f, -3.0f, 2.0f, ParametricEQProcessor::Peak);
        proc.addBand(2000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        proc.addBand(8000.0f, -6.0f, 0.707f, ParametricEQProcessor::HighShelf);
        for (int i = 0; i < 4; ++i)
            proc.setBandEnabled(i, true);
        auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc.process(buffer);
        expect(isBufferClean(buffer));

        beginTest("Bands can be removed dynamically");
        ParametricEQProcessor proc2;
        proc2.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int band1 = proc2.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        const int band2 = proc2.addBand(2000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        proc2.setBandEnabled(band1, true);
        proc2.setBandEnabled(band2, true);
        auto buffer1 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc2.process(buffer1);
        proc2.removeBand(band1);
        auto buffer2 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc2.process(buffer2);
        expect(isBufferClean(buffer2));
    }

    void testNumericalStability()
    {
        beginTest("No NaN or Inf with extreme gain values");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        proc.setBandEnabled(proc.addBand(1000.0f, 24.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc.process(buffer);
        expect(isBufferClean(buffer));

        beginTest("No NaN or Inf with extreme Q values");
        ParametricEQProcessor procQ;
        procQ.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procQ.setBandEnabled(procQ.addBand(1000.0f, 6.0f, 20.0f, ParametricEQProcessor::Peak), true);
        auto bufferQ = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        procQ.process(bufferQ);
        expect(isBufferClean(bufferQ));

        beginTest("No denormals after decay");
        ParametricEQProcessor procD;
        procD.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procD.setBandEnabled(procD.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto impulse = generateImpulse(kDefaultBlockSize);
        procD.process(impulse);
        for (int i = 0; i < 100; ++i)
        {
            juce::AudioBuffer<float> silent(kDefaultChannels, kDefaultBlockSize);
            silent.clear();
            procD.process(silent);
            if (i > 50)
                expect(hasNoDenormals(silent));
        }

        beginTest("Handles zero input gracefully");
        ParametricEQProcessor procZero;
        procZero.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procZero.setBandEnabled(procZero.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        juce::AudioBuffer<float> silent(kDefaultChannels, kDefaultBlockSize);
        silent.clear();
        procZero.process(silent);
        expect(isBufferClean(silent));
    }

    void testParameterChanges()
    {
        beginTest("Frequency can be changed in real-time");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int band = proc.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        proc.setBandEnabled(band, true);
        auto buffer1 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc.process(buffer1);
        proc.setBandFrequency(band, 2000.0f);
        auto buffer2 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc.process(buffer2);
        expect(isBufferClean(buffer2));

        beginTest("Gain can be changed in real-time");
        ParametricEQProcessor procG;
        procG.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int bandG = procG.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        procG.setBandEnabled(bandG, true);
        auto bufferG = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        procG.process(bufferG);
        procG.setBandGain(bandG, -6.0f);
        auto bufferG2 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        procG.process(bufferG2);
        expect(isBufferClean(bufferG2));

        beginTest("Q can be changed in real-time");
        ParametricEQProcessor procQ;
        procQ.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int bandQ = procQ.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak);
        procQ.setBandEnabled(bandQ, true);
        auto bufferQ = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        procQ.process(bufferQ);
        procQ.setBandQ(bandQ, 8.0f);
        auto bufferQ2 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        procQ.process(bufferQ2);
        expect(isBufferClean(bufferQ2));
    }

    void testBypassFunctionality()
    {
        beginTest("Disabled band does not affect signal");
        ParametricEQProcessor proc;
        proc.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int band = proc.addBand(1000.0f, 12.0f, 1.0f, ParametricEQProcessor::Peak);
        proc.setBandEnabled(band, false);
        auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float inputRMS = measureRMS(buffer);
        proc.process(buffer);
        expectWithinAbsoluteError(measureRMS(buffer), inputRMS, 0.001f);

        beginTest("Band can be toggled during processing");
        ParametricEQProcessor proc2;
        proc2.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        const int band2 = proc2.addBand(1000.0f, 12.0f, 1.0f, ParametricEQProcessor::Peak);
        proc2.setBandEnabled(band2, true);
        auto buffer1 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        proc2.process(buffer1);
        const float boostedRMS = measureRMS(buffer1);
        proc2.setBandEnabled(band2, false);
        auto buffer2 = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize);
        const float inputRMS2 = measureRMS(buffer2);
        proc2.process(buffer2);
        const float bypassedRMS = measureRMS(buffer2);
        expectGreaterThan(boostedRMS, bypassedRMS);
        expectWithinAbsoluteError(bypassedRMS, inputRMS2, 0.01f);
    }

    void testEdgeCases()
    {
        beginTest("Handles Nyquist frequency correctly");
        ParametricEQProcessor proc;
        const double sampleRate = 44100.0;
        proc.prepare(sampleRate, kDefaultBlockSize, kDefaultChannels);
        proc.setBandEnabled(proc.addBand(static_cast<float>(sampleRate * 0.9 / 2.0), 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto buffer = generateSineWave(static_cast<float>(sampleRate * 0.9 / 2.0), sampleRate, kDefaultBlockSize);
        proc.process(buffer);
        expect(isBufferClean(buffer));

        beginTest("Handles very low frequency correctly");
        ParametricEQProcessor procLow;
        procLow.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        procLow.setBandEnabled(procLow.addBand(20.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto bufferLow = generateSineWave(20.0f, kDefaultSampleRate, kDefaultBlockSize * 16);
        procLow.process(bufferLow);
        expect(isBufferClean(bufferLow));

        beginTest("Handles single-sample buffer");
        ParametricEQProcessor procSingle;
        procSingle.prepare(kDefaultSampleRate, 1, kDefaultChannels);
        procSingle.setBandEnabled(procSingle.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        juce::AudioBuffer<float> bufferSingle(kDefaultChannels, 1);
        bufferSingle.setSample(0, 0, 1.0f);
        bufferSingle.setSample(1, 0, 1.0f);
        procSingle.process(bufferSingle);
        expect(isBufferClean(bufferSingle));

        beginTest("Handles empty buffer without crash");
        ParametricEQProcessor procEmpty;
        procEmpty.prepare(kDefaultSampleRate, kDefaultBlockSize, kDefaultChannels);
        juce::AudioBuffer<float> emptyBuffer;
        procEmpty.process(emptyBuffer);
        expect(true);

        beginTest("Handles mono signal");
        ParametricEQProcessor procMono;
        procMono.prepare(kDefaultSampleRate, kDefaultBlockSize, 1);
        procMono.setBandEnabled(procMono.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
        auto bufferMono = generateSineWave(1000.0f, kDefaultSampleRate, kDefaultBlockSize, 1);
        procMono.process(bufferMono);
        expect(isBufferClean(bufferMono));
    }

    void testDifferentSampleRates()
    {
        beginTest("Works at common sample rates");
        const std::vector<double> sampleRates = {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};
        for (double sr : sampleRates)
        {
            ParametricEQProcessor proc;
            proc.prepare(sr, kDefaultBlockSize, kDefaultChannels);
            proc.setBandEnabled(proc.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
            auto buffer = generateSineWave(1000.0f, sr, kDefaultBlockSize);
            proc.process(buffer);
            expect(isBufferClean(buffer));
        }
    }

    void testDifferentBufferSizes()
    {
        beginTest("Works with various buffer sizes");
        const std::vector<int> bufferSizes = {32, 64, 128, 256, 512, 1024, 2048, 4096};
        for (int size : bufferSizes)
        {
            ParametricEQProcessor proc;
            proc.prepare(kDefaultSampleRate, size, kDefaultChannels);
            proc.setBandEnabled(proc.addBand(1000.0f, 6.0f, 1.0f, ParametricEQProcessor::Peak), true);
            auto buffer = generateSineWave(1000.0f, kDefaultSampleRate, size);
            proc.process(buffer);
            expect(isBufferClean(buffer));
        }
    }
};

static ParametricEQTest parametricEQTest;

