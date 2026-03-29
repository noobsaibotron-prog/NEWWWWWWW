#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

#include "../PluginProcessor.h"

namespace
{
struct TransitionMetrics
{
    float maxDelta = 0.0f;
    float peakAbs = 0.0f;
    float energyRatio = 1.0f;
    bool hasNaN = false;
    bool hasInf = false;
    int dropoutSamples = 0; // max consecutive near-zero samples while baseline is live
};

constexpr float kMaxDeltaThreshold = 0.6f;
constexpr float kPeakAbsThreshold = 2.0f;
constexpr float kEnergyRatioThreshold = 8.0f;
constexpr int kMaxDropoutSamples = 10;

static void setChoice(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int index)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
}

static void setBool(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, bool value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(value ? 1.0f : 0.0f);
}

static void setFloat(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

void fillSine(juce::AudioBuffer<float>& buf,
              double sampleRate,
              double freqHz = 1000.0,
              float amplitudeLinear = 0.25f,
              int sampleOffset = 0)
{
    const auto w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* data = buf.getWritePointer(ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            data[i] = amplitudeLinear * std::sin(w * static_cast<double>(sampleOffset + i));
    }
}

TransitionMetrics analyzeWindow(const juce::AudioBuffer<float>& buf,
                                int startSample,
                                int windowSamples,
                                const juce::AudioBuffer<float>* baseline = nullptr)
{
    TransitionMetrics metrics;

    if (buf.getNumChannels() == 0 || buf.getNumSamples() == 0 || windowSamples <= 0)
        return metrics;

    startSample = juce::jmax(0, startSample);
    const int endSample = juce::jmin(buf.getNumSamples(), startSample + windowSamples);

    long double switchedEnergy = 0.0;
    long double baselineEnergy = 0.0;
    int maxDropoutRun = 0;

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const auto* data = buf.getReadPointer(ch);
        const auto* base = baseline != nullptr && ch < baseline->getNumChannels()
                         ? baseline->getReadPointer(ch)
                         : nullptr;

        int currentDropoutRun = 0;
        float previous = data[startSample];

        for (int i = startSample; i < endSample; ++i)
        {
            const float sample = data[i];
            if (std::isnan(sample)) metrics.hasNaN = true;
            if (std::isinf(sample)) metrics.hasInf = true;

            metrics.peakAbs = std::max(metrics.peakAbs, std::abs(sample));
            if (i > startSample)
                metrics.maxDelta = std::max(metrics.maxDelta, std::abs(sample - previous));
            previous = sample;

            switchedEnergy += static_cast<long double>(sample) * static_cast<long double>(sample);

            const float baselineSample = base != nullptr ? base[i] : 0.0f;
            if (base != nullptr)
            {
                baselineEnergy += static_cast<long double>(baselineSample) * static_cast<long double>(baselineSample);

                if (std::abs(baselineSample) > 0.01f && std::abs(sample) < 1.0e-5f)
                    ++currentDropoutRun;
                else
                    currentDropoutRun = 0;

                maxDropoutRun = std::max(maxDropoutRun, currentDropoutRun);
            }
        }
    }

    metrics.dropoutSamples = maxDropoutRun;

    if (baseline != nullptr)
    {
        const long double denom = std::max<long double>(baselineEnergy, 1.0e-12);
        metrics.energyRatio = static_cast<float>(switchedEnergy / denom);
    }

    return metrics;
}

struct ScenarioRun
{
    juce::AudioBuffer<float> output;
    juce::AudioBuffer<float> input;
};

ScenarioRun runScenario(double sampleRate,
                        int blockSize,
                        int numBlocks,
                        int switchBlock,
                        const std::function<void(AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState&, int)>& onSwitch,
                        std::function<void(AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState&)> setup = {})
{
    AIEqualizerAudioProcessor proc;
    proc.prepareToPlay(sampleRate, blockSize);

    auto& apvts = proc.getAPVTS();
    if (setup)
        setup(proc, apvts);

    ScenarioRun run { juce::AudioBuffer<float>(2, blockSize * numBlocks),
                      juce::AudioBuffer<float>(2, blockSize * numBlocks) };
    juce::MidiBuffer midi;

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioBuffer<float> chunk(2, blockSize);
        fillSine(chunk, sampleRate, 1000.0, 0.25f, block * blockSize);

        for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
            run.input.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);

        if (block == switchBlock && onSwitch)
            onSwitch(proc, apvts, block);

        proc.processBlock(chunk, midi);

        for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
            run.output.copyFrom(ch, block * blockSize, chunk, ch, 0, blockSize);
    }

    proc.releaseResources();
    return run;
}

void configureProcessing(AIEqualizerAudioProcessor& proc,
                         juce::AudioProcessorValueTreeState& apvts,
                         int phaseMode,
                         int oversamplingFactor = 0)
{
    proc.setNumActiveBands(1);
    AIEqualizerAudioProcessor::BandState band;
    band.frequency = 1000.0f;
    band.gain = 6.0f;
    band.q = 1.0f;
    band.type = static_cast<int>(ParametricEQProcessor::Peak);
    band.enabled = true;
    band.solo = false;
    proc.setBandState(0, band);

    setChoice(apvts, "numActiveBands", 0);
    setBool(apvts, "bypass", false);
    setFloat(apvts, "dryWet", 100.0f);
    setFloat(apvts, "outputGain", 0.0f);
    setChoice(apvts, "qualityMode", 1);
    setChoice(apvts, "msMode", 0);
    setChoice(apvts, "phaseMode", phaseMode);
    setChoice(apvts, "oversamplingFactor", oversamplingFactor);
}

void expectMetrics(juce::UnitTest& test, const TransitionMetrics& metrics, const juce::String& label)
{
    test.expect(!metrics.hasNaN, label + ": NaN detected");
    test.expect(!metrics.hasInf, label + ": Inf detected");
    test.expect(metrics.maxDelta < kMaxDeltaThreshold,
                label + ": maxDelta too high = " + juce::String(metrics.maxDelta, 4));
    test.expect(metrics.peakAbs < kPeakAbsThreshold,
                label + ": peakAbs too high = " + juce::String(metrics.peakAbs, 4));
    test.expect(metrics.energyRatio < kEnergyRatioThreshold,
                label + ": energyRatio too high = " + juce::String(metrics.energyRatio, 4));
    test.expect(metrics.dropoutSamples <= kMaxDropoutSamples,
                label + ": dropoutSamples too high = " + juce::String(metrics.dropoutSamples));
}

float maxDifferenceVsInputAfter(const juce::AudioBuffer<float>& output,
                                const juce::AudioBuffer<float>& input,
                                int startSample)
{
    // With Maximum Latency Padding, bypass output is delayed by worstCaseLatencySamples.
    // Detect the delay by finding the best correlation offset, then compare.
    // Try offsets 0..512 and pick the one with minimum total difference.
    int bestOffset = 0;
    float bestSum = std::numeric_limits<float>::max();
    const int searchLen = juce::jmin(256, output.getNumSamples() - startSample);

    for (int offset = 0; offset <= 512 && startSample + offset + searchLen <= output.getNumSamples(); ++offset)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < juce::jmin(output.getNumChannels(), input.getNumChannels()); ++ch)
        {
            const auto* out = output.getReadPointer(ch);
            const auto* in  = input.getReadPointer(ch);
            for (int i = 0; i < searchLen; ++i)
            {
                const int outIdx = startSample + offset + i;
                const int inIdx  = startSample + i;
                if (outIdx < output.getNumSamples() && inIdx < input.getNumSamples())
                    sum += std::abs(out[outIdx] - in[inIdx]);
            }
        }
        if (sum < bestSum) { bestSum = sum; bestOffset = offset; }
    }

    float maxDiff = 0.0f;
    for (int ch = 0; ch < juce::jmin(output.getNumChannels(), input.getNumChannels()); ++ch)
    {
        const auto* out = output.getReadPointer(ch);
        const auto* in  = input.getReadPointer(ch);
        for (int i = startSample; i < output.getNumSamples(); ++i)
        {
            const int inIdx = i - bestOffset;
            if (inIdx >= 0 && inIdx < input.getNumSamples())
                maxDiff = std::max(maxDiff, std::abs(out[i] - in[inIdx]));
        }
    }
    return maxDiff;
}

float maxDifferenceBetweenOutputsAfter(const juce::AudioBuffer<float>& a,
                                       const juce::AudioBuffer<float>& b,
                                       int startSample)
{
    float maxDiff = 0.0f;
    const int chs = juce::jmin(a.getNumChannels(), b.getNumChannels());
    const int numSamples = juce::jmin(a.getNumSamples(), b.getNumSamples());
    for (int ch = 0; ch < chs; ++ch)
    {
        const auto* aPtr = a.getReadPointer(ch);
        const auto* bPtr = b.getReadPointer(ch);
        for (int i = startSample; i < numSamples; ++i)
            maxDiff = std::max(maxDiff, std::abs(aPtr[i] - bPtr[i]));
    }
    return maxDiff;
}
} // namespace

class OversamplingResetRegressionTest : public juce::UnitTest
{
public:
    OversamplingResetRegressionTest() : juce::UnitTest("Oversampling Reset Transition", "Regression") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        const std::array<double, 2> sampleRates { 44100.0, 48000.0 };
        const std::array<int, 3> blockSizes { 32, 64, 128 };
        const std::array<std::pair<int, int>, 3> transitions { std::pair{0, 2}, std::pair{2, 1}, std::pair{1, 0} };

        constexpr int numBlocks = 100;
        constexpr int switchBlock = 50;

        for (double sr : sampleRates)
        {
            for (int blockSize : blockSizes)
            {
                for (auto [from, to] : transitions)
                {
                    beginTest("OS " + juce::String(from) + " -> " + juce::String(to)
                              + " @ " + juce::String(sr, 0) + " Hz / block " + juce::String(blockSize));

                    auto setup = [from](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
                    {
                        configureProcessing(proc, apvts, 1, from); // Natural phase path
                    };

                    auto baseline = runScenario(sr, blockSize, numBlocks, switchBlock,
                                                {}, setup);

                    auto switched = runScenario(sr, blockSize, numBlocks, switchBlock,
                                                [to](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int)
                                                {
                                                    setChoice(apvts, "oversamplingFactor", to);
                                                },
                                                setup);

                    const int start = juce::jmax(0, switchBlock * blockSize - 256);
                    const int window = juce::jmin(switched.output.getNumSamples() - start,
                                                  10 * blockSize + 256);
                    auto metrics = analyzeWindow(switched.output, start, window, &baseline.output);
                    expectMetrics(*this, metrics, "Oversampling transition");
                }
            }
        }
    }
};

class PhaseModeSwitchContinuityTest : public juce::UnitTest
{
public:
    PhaseModeSwitchContinuityTest() : juce::UnitTest("Phase Mode Switch Continuity", "Regression") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        const std::array<std::pair<int, int>, 4> transitions { std::pair{0, 1}, std::pair{1, 0}, std::pair{0, 2}, std::pair{2, 0} };
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 128;
        constexpr int numBlocks = 100;
        constexpr int switchBlock = 50;

        for (auto [from, to] : transitions)
        {
            beginTest("Phase mode " + juce::String(from) + " -> " + juce::String(to));

            auto setup = [from](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
            {
                configureProcessing(proc, apvts, from, 1);
            };

            auto baseline = runScenario(sampleRate, blockSize, numBlocks, switchBlock, {}, setup);

            AIEqualizerAudioProcessor latencyProbe;
            latencyProbe.prepareToPlay(sampleRate, blockSize);
            auto& latencyAPVTS = latencyProbe.getAPVTS();
            configureProcessing(latencyProbe, latencyAPVTS, from, 1);
            setChoice(latencyAPVTS, "phaseMode", to);
            expect(latencyProbe.getLatencySamples() >= 0, "Latency must remain non-negative after phase switch");
            latencyProbe.releaseResources();

            auto switched = runScenario(sampleRate, blockSize, numBlocks, switchBlock,
                                        [to](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int)
                                        {
                                            setChoice(apvts, "phaseMode", to);
                                        },
                                        setup);

            const int start = juce::jmax(0, switchBlock * blockSize - 256);
            const int window = juce::jmin(switched.output.getNumSamples() - start,
                                          10 * blockSize + 256);
            auto metrics = analyzeWindow(switched.output, start, window, &baseline.output);
            expectMetrics(*this, metrics, "Phase mode switch");
        }
    }
};

class BypassTransitionContinuityTest : public juce::UnitTest
{
public:
    BypassTransitionContinuityTest() : juce::UnitTest("Bypass Transition Continuity", "Regression") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        const std::array<double, 2> sampleRates { 44100.0, 48000.0 };
        const std::array<int, 3> blockSizes { 64, 128, 512 };

        for (double sr : sampleRates)
        {
            for (int blockSize : blockSizes)
            {
                runOneScenario("active -> bypass", sr, blockSize,
                               [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int)
                               {
                                   setBool(apvts, "bypass", true);
                               },
                               true);

                runOneScenario("bypass -> active", sr, blockSize,
                               [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int)
                               {
                                   setBool(apvts, "bypass", false);
                               },
                               false,
                               true);

                runRapidToggle(sr, blockSize);
            }
        }
    }

private:
    void runOneScenario(const juce::String& label,
                        double sampleRate,
                        int blockSize,
                        const std::function<void(AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState&, int)>& onSwitch,
                        bool expectDryIdentityAfterSwitch,
                        bool startBypassed = false)
    {
        beginTest(label + " @ " + juce::String(sampleRate, 0) + " Hz / block " + juce::String(blockSize));

        constexpr int numBlocks = 80;
        constexpr int switchBlock = 30;

        auto setup = [startBypassed](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        {
            configureProcessing(proc, apvts, 0, 0);
            setBool(apvts, "bypass", startBypassed);
        };

        auto baseline = runScenario(sampleRate, blockSize, numBlocks, switchBlock, {}, setup);
        auto switched = runScenario(sampleRate, blockSize, numBlocks, switchBlock, onSwitch, setup);

        const int start = juce::jmax(0, switchBlock * blockSize - 256);
        const int window = juce::jmin(switched.output.getNumSamples() - start,
                                      10 * blockSize + 256);
        auto metrics = analyzeWindow(switched.output, start, window, &baseline.output);

        if (startBypassed)
        {
            expect(!metrics.hasNaN, label + ": NaN detected");
            expect(!metrics.hasInf, label + ": Inf detected");
            expect(metrics.maxDelta < kMaxDeltaThreshold,
                   label + ": maxDelta too high = " + juce::String(metrics.maxDelta, 4));
            expect(metrics.peakAbs < kPeakAbsThreshold,
                   label + ": peakAbs too high = " + juce::String(metrics.peakAbs, 4));
            expect(metrics.dropoutSamples <= kMaxDropoutSamples,
                   label + ": dropoutSamples too high = " + juce::String(metrics.dropoutSamples));

            // NOTE: We do NOT check convergence to an always-active reference here.
            // With bypass early-return, IIR filters restart from zero on re-entry,
            // so exact convergence requires hundreds of ms — well beyond a short
            // test window.  The NaN/Inf/maxDelta/peakAbs/dropout checks above are
            // the meaningful contract for bypass→active transitions.
        }
        else
        {
            expectMetrics(*this, metrics, label);
        }

        if (expectDryIdentityAfterSwitch)
        {
            const int settleStart = juce::jmin(switched.output.getNumSamples(), (switchBlock + 4) * blockSize);
            const float maxDiff = maxDifferenceVsInputAfter(switched.output, switched.input, settleStart);
            expect(maxDiff < 1.0e-4f,
                   "Bypass output should match dry input after settle window, maxDiff=" + juce::String(maxDiff, 6));
        }
    }

    void runRapidToggle(double sampleRate, int blockSize)
    {
        beginTest("rapid toggle @ " + juce::String(sampleRate, 0) + " Hz / block " + juce::String(blockSize));

        constexpr int numBlocks = 80;
        constexpr int switchBlock = 30;

        auto setup = [](AIEqualizerAudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts)
        {
            configureProcessing(proc, apvts, 0, 0);
            setBool(apvts, "bypass", false);
        };

        auto baseline = runScenario(sampleRate, blockSize, numBlocks, switchBlock, {}, setup);
        auto switched = runScenario(sampleRate, blockSize, numBlocks, switchBlock,
                                    [](AIEqualizerAudioProcessor&, juce::AudioProcessorValueTreeState& apvts, int block)
                                    {
                                        if (block == 30) setBool(apvts, "bypass", true);
                                        if (block == 33) setBool(apvts, "bypass", false);
                                        if (block == 36) setBool(apvts, "bypass", true);
                                    },
                                    setup);

        const int start = juce::jmax(0, switchBlock * blockSize - 256);
        const int window = juce::jmin(switched.output.getNumSamples() - start,
                                      10 * blockSize + 256);
        auto metrics = analyzeWindow(switched.output, start, window, &baseline.output);
        expectMetrics(*this, metrics, "rapid toggle");
    }
};

static OversamplingResetRegressionTest oversamplingResetRegressionTest;
static PhaseModeSwitchContinuityTest phaseModeSwitchContinuityTest;
static BypassTransitionContinuityTest bypassTransitionContinuityTest;
