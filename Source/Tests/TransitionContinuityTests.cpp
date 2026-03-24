#if JUCE_UNIT_TESTS

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <functional>
#include <vector>

#include "../DSP/LinearPhaseProcessor.h"
#include "../DSP/ParametricEQProcessor.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kChannels = 2;

struct TransitionMetrics
{
    float maxAbs = 0.0f;
    float maxDelta = 0.0f;
    float rms = 0.0f;
    float energy = 0.0f;
    bool hasNaN = false;
    bool hasInf = false;
    int nearZeroRun = 0;
    int longestNearZeroRun = 0;
};

float computeRms(const std::vector<float>& data)
{
    if (data.empty())
        return 0.0f;

    long double sum = 0.0;
    for (float v : data)
        sum += static_cast<long double>(v) * static_cast<long double>(v);

    return std::sqrt(static_cast<float>(sum / static_cast<long double>(data.size())));
}

TransitionMetrics analyzeWindow(const std::vector<float>& data, int startSample, int endSample)
{
    TransitionMetrics metrics;
    if (data.empty())
        return metrics;

    startSample = juce::jlimit(0, static_cast<int>(data.size()) - 1, startSample);
    endSample = juce::jlimit(startSample + 1, static_cast<int>(data.size()), endSample);

    long double sumSq = 0.0;
    int count = 0;
    int zeroRun = 0;
    float previous = data[static_cast<size_t>(startSample)];

    for (int i = startSample; i < endSample; ++i)
    {
        const float sample = data[static_cast<size_t>(i)];

        if (std::isnan(sample))
            metrics.hasNaN = true;
        if (std::isinf(sample))
            metrics.hasInf = true;

        metrics.maxAbs = std::max(metrics.maxAbs, std::abs(sample));
        if (i > startSample)
            metrics.maxDelta = std::max(metrics.maxDelta, std::abs(sample - previous));

        const float absSample = std::abs(sample);
        if (absSample < 1.0e-6f)
        {
            ++zeroRun;
            metrics.longestNearZeroRun = std::max(metrics.longestNearZeroRun, zeroRun);
        }
        else
        {
            zeroRun = 0;
        }

        sumSq += static_cast<long double>(sample) * static_cast<long double>(sample);
        previous = sample;
        ++count;
    }

    metrics.energy = static_cast<float>(sumSq);
    metrics.rms = (count > 0) ? std::sqrt(static_cast<float>(sumSq / static_cast<long double>(count))) : 0.0f;
    metrics.nearZeroRun = zeroRun;
    return metrics;
}

TransitionMetrics analyzeDifferenceWindow(const std::vector<float>& baseline,
                                          const std::vector<float>& switched,
                                          int startSample,
                                          int endSample)
{
    jassert(baseline.size() == switched.size());

    std::vector<float> diff;
    diff.reserve(baseline.size());
    for (size_t i = 0; i < baseline.size(); ++i)
        diff.push_back(switched[i] - baseline[i]);

    return analyzeWindow(diff, startSample, endSample);
}

std::vector<float> renderParametricScenario(int blockSize,
                                            int numBlocks,
                                            int switchBlock,
                                            const std::function<void(ParametricEQProcessor&, int)>& transition,
                                            bool applyTransition)
{
    ParametricEQProcessor processor;
    processor.prepare(kSampleRate, blockSize, kChannels);

    const int bandIndex = processor.addBand(1000.0f, 0.0f, 1.0f, static_cast<int>(ParametricEQProcessor::Peak));
    processor.setBandEnabled(bandIndex, true);
    processor.setBypass(false);

    std::vector<float> output;
    output.reserve(static_cast<size_t>(blockSize * numBlocks));

    double phase = 0.0;
    const double phaseStep = juce::MathConstants<double>::twoPi * 1000.0 / kSampleRate;

    for (int blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
    {
        if (applyTransition && blockIndex == switchBlock)
            transition(processor, bandIndex);

        juce::AudioBuffer<float> buffer(kChannels, blockSize);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            double localPhase = phase;
            for (int i = 0; i < blockSize; ++i)
            {
                data[i] = 0.25f * std::sin(static_cast<float>(localPhase));
                localPhase += phaseStep;
            }
        }

        phase += phaseStep * static_cast<double>(blockSize);
        processor.process(buffer);

        const auto* read = buffer.getReadPointer(0);
        output.insert(output.end(), read, read + blockSize);
    }

    return output;
}

std::vector<float> renderLinearPhaseScenario(int blockSize,
                                             int numBlocks,
                                             int switchBlock,
                                             const std::function<void(LinearPhaseProcessor&)>& transition,
                                             bool applyTransition)
{
    LinearPhaseProcessor processor;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = kSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = kChannels;
    processor.prepare(spec);

    std::vector<float> unity(LinearPhaseProcessor::fftSize / 2, 0.0f);
    processor.updateImpulseResponse(unity, kSampleRate);

    std::vector<float> output;
    output.reserve(static_cast<size_t>(blockSize * numBlocks));

    double phase = 0.0;
    const double phaseStep = juce::MathConstants<double>::twoPi * 440.0 / kSampleRate;

    for (int blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
    {
        if (applyTransition && blockIndex == switchBlock)
            transition(processor);

        juce::AudioBuffer<float> buffer(kChannels, blockSize);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            double localPhase = phase;
            for (int i = 0; i < blockSize; ++i)
            {
                data[i] = 0.2f * std::sin(static_cast<float>(localPhase));
                localPhase += phaseStep;
            }
        }

        phase += phaseStep * static_cast<double>(blockSize);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        processor.process(context);

        const auto* read = buffer.getReadPointer(0);
        output.insert(output.end(), read, read + blockSize);
    }

    return output;
}
} // namespace

class TransitionContinuityTests : public juce::UnitTest
{
public:
    TransitionContinuityTests()
        : juce::UnitTest("TransitionContinuityTests", "DSP") {}

    void runTest() override
    {
        testParametricBypassIdentityTransition();
        testLinearPhaseIRSwapContinuity();
        testLinearPhaseResetContinuity();
    }

private:
    void testParametricBypassIdentityTransition()
    {
        beginTest("Parametric bypass identity transition stays continuous");

        constexpr int blockSize = 128;
        constexpr int numBlocks = 20;
        constexpr int switchBlock = 8;
        const int switchSample = blockSize * switchBlock;
        const int windowRadius = blockSize;

        auto baseline = renderParametricScenario(
            blockSize, numBlocks, switchBlock,
            [](ParametricEQProcessor&, int) {},
            false);

        auto switched = renderParametricScenario(
            blockSize, numBlocks, switchBlock,
            [](ParametricEQProcessor& processor, int bandIndex)
            {
                juce::ignoreUnused(bandIndex);
                processor.setBypass(true);
            },
            true);

        auto diffMetrics = analyzeDifferenceWindow(baseline, switched,
                                                   switchSample - windowRadius,
                                                   switchSample + windowRadius);

        expect(!diffMetrics.hasNaN, "Bypass transition produced NaN values");
        expect(!diffMetrics.hasInf, "Bypass transition produced Inf values");
        expect(diffMetrics.maxDelta < 1.0e-4f,
               "Identity bypass transition should be effectively sample-continuous");
        expect(diffMetrics.maxAbs < 1.0e-4f,
               "Identity bypass transition should not alter the rendered signal");
    }

    void testLinearPhaseIRSwapContinuity()
    {
        beginTest("Linear-phase IR hot-swap stays bounded around transition");

        constexpr int blockSize = 256;
        constexpr int numBlocks = 40;
        constexpr int switchBlock = 14;
        const int switchSample = blockSize * switchBlock;
        const int windowRadius = blockSize * 2;

        auto baseline = renderLinearPhaseScenario(
            blockSize, numBlocks, switchBlock,
            [](LinearPhaseProcessor&) {},
            false);

        auto switched = renderLinearPhaseScenario(
            blockSize, numBlocks, switchBlock,
            [](LinearPhaseProcessor& processor)
            {
                std::vector<float> boost(LinearPhaseProcessor::fftSize / 2, 0.0f);
                std::fill(boost.begin(), boost.end(), 3.0f);
                processor.updateImpulseResponse(boost, kSampleRate);
            },
            true);

        auto switchedMetrics = analyzeWindow(switched,
                                             switchSample - windowRadius,
                                             switchSample + windowRadius);
        auto diffMetrics = analyzeDifferenceWindow(baseline, switched,
                                                   switchSample - windowRadius,
                                                   switchSample + windowRadius);

        expect(!switchedMetrics.hasNaN, "IR swap produced NaN values");
        expect(!switchedMetrics.hasInf, "IR swap produced Inf values");
        expect(switchedMetrics.longestNearZeroRun < blockSize / 2,
               "IR swap caused a suspicious near-silent dropout window");
        expect(diffMetrics.maxDelta < 0.25f,
               "IR hot-swap caused an excessive single-sample jump");
        expect(diffMetrics.maxAbs < 0.5f,
               "IR hot-swap difference burst is too large around the transition");
    }

    void testLinearPhaseResetContinuity()
    {
        beginTest("Linear-phase reset during playback remains finite and bounded");

        constexpr int blockSize = 128;
        constexpr int numBlocks = 36;
        constexpr int switchBlock = 12;
        const int switchSample = blockSize * switchBlock;
        const int windowRadius = blockSize * 2;

        auto baseline = renderLinearPhaseScenario(
            blockSize, numBlocks, switchBlock,
            [](LinearPhaseProcessor&) {},
            false);

        auto switched = renderLinearPhaseScenario(
            blockSize, numBlocks, switchBlock,
            [](LinearPhaseProcessor& processor)
            {
                processor.reset();
            },
            true);

        auto switchedMetrics = analyzeWindow(switched,
                                             switchSample - windowRadius,
                                             switchSample + windowRadius);
        auto diffMetrics = analyzeDifferenceWindow(baseline, switched,
                                                   switchSample - windowRadius,
                                                   switchSample + windowRadius);

        expect(!switchedMetrics.hasNaN, "Reset transition produced NaN values");
        expect(!switchedMetrics.hasInf, "Reset transition produced Inf values");
        expect(switchedMetrics.maxAbs < 1.0f,
               "Reset transition produced an abnormally large output peak");
        expect(diffMetrics.maxDelta < 0.35f,
               "Reset transition produced an excessive sample jump");
        expect(diffMetrics.maxAbs < 0.75f,
               "Reset transition produced an excessive burst against baseline");
    }
};

static TransitionContinuityTests transitionContinuityTests;

#endif // JUCE_UNIT_TESTS
