#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"
#include <cmath>

class DynEQRoutingOwnershipTest : public juce::UnitTest
{
public:
    DynEQRoutingOwnershipTest()
        : juce::UnitTest("DynEQ Routing Ownership", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testMasterOffFallsBackToStaticEQ();
        testDynModeOffDoesNotDoubleProcess();
        testActiveCompressionStillDivergesFromStaticReference();
        testRapidMasterToggleStaysFinite();
        testLinearPhaseDoesNotAddStaticIRContribution();
        testLowCutCompressFallsBackToStaticEQ();
        testHighCutExpandFallsBackToStaticEQ();
        testNotchCompressFallsBackToStaticEQ();
        testBandPassExpandFallsBackToStaticEQ();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kChannels = 2;
    static constexpr float kToneFreq = 1000.0f;

    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void setBool(juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& id, bool value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
    }

    static void setFloat(juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    }

    static AIEqualizerAudioProcessor::BandState makeBaseBand()
    {
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = kToneFreq;
        band.gain = 12.0f;
        band.q = 1.0f;
        band.type = static_cast<int>(ParametricEQProcessor::Peak);
        band.enabled = true;
        band.solo = false;
        band.dynMode = DynamicEQProcessor::DynamicMode_Off;
        band.dynThreshold = -30.0f;
        band.dynRatio = 8.0f;
        band.dynAttack = 1.0f;
        band.dynRelease = 80.0f;
        band.dynRange = 24.0f;
        band.dynKnee = 0.0f;
        return band;
    }

    static void configureProcessor(AIEqualizerAudioProcessor& proc,
                                   juce::AudioProcessorValueTreeState& apvts,
                                   const AIEqualizerAudioProcessor::BandState& band,
                                   bool dynEqEnabled,
                                   int phaseMode = 0)
    {
        proc.setNumActiveBands(1);
        proc.setBandState(0, band);

        setBool(apvts, "dynEqEnabled", dynEqEnabled);
        setFloat(apvts, "dynEqMix", 100.0f);
        setBool(apvts, "dynAutoMakeup", false);
        setFloat(apvts, "dryWet", 100.0f);
        setFloat(apvts, "outputGain", 0.0f);
        setBool(apvts, "bypass", false);
        setChoice(apvts, "qualityMode", 0);
        setChoice(apvts, "phaseMode", phaseMode);
        setChoice(apvts, "oversamplingFactor", 0);
    }

    static juce::AudioBuffer<float> makeTone(int totalBlocks,
                                             float amplitude = 0.5f,
                                             float freqHz = kToneFreq)
    {
        juce::AudioBuffer<float> signal(kChannels, totalBlocks * kBlockSize);
        for (int ch = 0; ch < kChannels; ++ch)
        {
            auto* data = signal.getWritePointer(ch);
            for (int i = 0; i < signal.getNumSamples(); ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
                data[i] = amplitude * std::sin(juce::MathConstants<float>::twoPi * freqHz * t);
            }
        }
        return signal;
    }

    static void prime(AIEqualizerAudioProcessor& proc, int blocks)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> silence(kChannels, kBlockSize);
        silence.clear();
        for (int i = 0; i < blocks; ++i)
            proc.processBlock(silence, midi);
    }

    static juce::AudioBuffer<float> processSignal(AIEqualizerAudioProcessor& proc,
                                                  const juce::AudioBuffer<float>& input)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> output(kChannels, input.getNumSamples());
        juce::AudioBuffer<float> block(kChannels, kBlockSize);

        const int blocks = input.getNumSamples() / kBlockSize;
        for (int b = 0; b < blocks; ++b)
        {
            const int offset = b * kBlockSize;
            for (int ch = 0; ch < kChannels; ++ch)
                block.copyFrom(ch, 0, input, ch, offset, kBlockSize);

            proc.processBlock(block, midi);

            for (int ch = 0; ch < kChannels; ++ch)
                output.copyFrom(ch, offset, block, ch, 0, kBlockSize);
        }

        return output;
    }

    static float rmsDb(const juce::AudioBuffer<float>& buf, int startSample)
    {
        const int span = juce::jmax(1, buf.getNumSamples() - startSample);
        const float rms = buf.getRMSLevel(0, startSample, span);
        return juce::Decibels::gainToDecibels(rms, -120.0f);
    }

    static float maxAbsSample(const juce::AudioBuffer<float>& buf)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                peak = std::max(peak, std::abs(buf.getSample(ch, i)));
        return peak;
    }

    static float maxDelta(const juce::AudioBuffer<float>& buf)
    {
        float peakDelta = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            for (int i = 1; i < buf.getNumSamples(); ++i)
            {
                peakDelta = std::max(peakDelta,
                                     std::abs(buf.getSample(ch, i) - buf.getSample(ch, i - 1)));
            }
        }
        return peakDelta;
    }

    static bool isFinite(const juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (!std::isfinite(buf.getSample(ch, i)))
                    return false;
        return true;
    }

    void expectUnsupportedFilterFallsBackToStaticEQ(const juce::String& label,
                                                    int filterType,
                                                    int dynMode,
                                                    float bandFreq,
                                                    float bandQ,
                                                    float toneFreq)
    {
        AIEqualizerAudioProcessor routed;
        routed.prepareToPlay(kSampleRate, kBlockSize);
        auto& routedApvts = routed.getAPVTS();

        AIEqualizerAudioProcessor reference;
        reference.prepareToPlay(kSampleRate, kBlockSize);
        auto& refApvts = reference.getAPVTS();

        auto dynBand = makeBaseBand();
        dynBand.type = filterType;
        dynBand.frequency = bandFreq;
        dynBand.q = bandQ;
        dynBand.gain = 0.0f;
        dynBand.dynMode = dynMode;

        auto staticBand = dynBand;
        staticBand.dynMode = DynamicEQProcessor::DynamicMode_Off;

        configureProcessor(routed, routedApvts, dynBand, true);
        configureProcessor(reference, refApvts, staticBand, false);

        prime(routed, 12);
        prime(reference, 12);

        const auto signal = makeTone(24, 0.7f, toneFreq);
        const auto routedOut = processSignal(routed, signal);
        const auto refOut = processSignal(reference, signal);

        const int warmup = kBlockSize * 8;
        const float refRmsDb = rmsDb(refOut, warmup);
        const float diffDb = rmsDb(routedOut, warmup) - rmsDb(refOut, warmup);

        float maxSampleDiff = 0.0f;
        for (int ch = 0; ch < kChannels; ++ch)
        {
            for (int i = warmup; i < routedOut.getNumSamples(); ++i)
            {
                maxSampleDiff = std::max(maxSampleDiff,
                    std::abs(routedOut.getSample(ch, i) - refOut.getSample(ch, i)));
            }
        }

        logMessage(label + " RMS delta: " + juce::String(diffDb, 3)
                 + " dB, max sample diff: " + juce::String(maxSampleDiff, 5));
        expect(refRmsDb > -40.0f,
               label + " reference output should not be effectively silent");
        expectWithinAbsoluteError(diffDb, 0.0f, 0.5f,
                                  label + " should stay on the static EQ path");
        expect(maxSampleDiff < 0.02f,
               label + " should match the static EQ sample-by-sample after warmup");
    }

    void testMasterOffFallsBackToStaticEQ()
    {
        beginTest("Master OFF: dynamic-configured band still behaves like static EQ");

        AIEqualizerAudioProcessor routed;
        routed.prepareToPlay(kSampleRate, kBlockSize);
        auto& routedApvts = routed.getAPVTS();

        AIEqualizerAudioProcessor reference;
        reference.prepareToPlay(kSampleRate, kBlockSize);
        auto& refApvts = reference.getAPVTS();

        auto dynBand = makeBaseBand();
        dynBand.dynMode = DynamicEQProcessor::DynamicMode_Compress;
        auto staticBand = dynBand;
        staticBand.dynMode = DynamicEQProcessor::DynamicMode_Off;

        configureProcessor(routed, routedApvts, dynBand, false);
        configureProcessor(reference, refApvts, staticBand, false);

        prime(routed, 12);
        prime(reference, 12);

        const auto signal = makeTone(20);
        const auto routedOut = processSignal(routed, signal);
        const auto refOut = processSignal(reference, signal);

        const int warmup = kBlockSize * 6;
        const float diffDb = rmsDb(routedOut, warmup) - rmsDb(refOut, warmup);

        logMessage("Master OFF RMS delta: " + juce::String(diffDb, 3) + " dB");
        expectWithinAbsoluteError(diffDb, 0.0f, 0.5f,
                                  "Master OFF should fall back to the static EQ path");
    }

    void testDynModeOffDoesNotDoubleProcess()
    {
        beginTest("Master ON + Dyn Off: band is not double processed");

        AIEqualizerAudioProcessor routed;
        routed.prepareToPlay(kSampleRate, kBlockSize);
        auto& routedApvts = routed.getAPVTS();

        AIEqualizerAudioProcessor reference;
        reference.prepareToPlay(kSampleRate, kBlockSize);
        auto& refApvts = reference.getAPVTS();

        auto band = makeBaseBand();
        band.dynMode = DynamicEQProcessor::DynamicMode_Off;

        configureProcessor(routed, routedApvts, band, true);
        configureProcessor(reference, refApvts, band, false);

        prime(routed, 12);
        prime(reference, 12);

        const auto signal = makeTone(20);
        const auto routedOut = processSignal(routed, signal);
        const auto refOut = processSignal(reference, signal);

        const int warmup = kBlockSize * 6;
        const float diffDb = rmsDb(routedOut, warmup) - rmsDb(refOut, warmup);

        logMessage("Dyn Off RMS delta: " + juce::String(diffDb, 3) + " dB");
        expectWithinAbsoluteError(diffDb, 0.0f, 0.5f,
                                  "Dyn Off should match the static EQ reference");
    }

    void testActiveCompressionStillDivergesFromStaticReference()
    {
        beginTest("Master ON + Compress: output diverges from static reference");

        AIEqualizerAudioProcessor routed;
        routed.prepareToPlay(kSampleRate, kBlockSize);
        auto& routedApvts = routed.getAPVTS();

        AIEqualizerAudioProcessor reference;
        reference.prepareToPlay(kSampleRate, kBlockSize);
        auto& refApvts = reference.getAPVTS();

        auto dynBand = makeBaseBand();
        dynBand.dynMode = DynamicEQProcessor::DynamicMode_Compress;
        auto staticBand = dynBand;
        staticBand.dynMode = DynamicEQProcessor::DynamicMode_Off;

        configureProcessor(routed, routedApvts, dynBand, true);
        configureProcessor(reference, refApvts, staticBand, false);

        prime(routed, 12);
        prime(reference, 12);

        const auto signal = makeTone(24, 0.7f);
        const auto routedOut = processSignal(routed, signal);
        const auto refOut = processSignal(reference, signal);

        const int warmup = kBlockSize * 8;
        const float diffDb = rmsDb(routedOut, warmup) - rmsDb(refOut, warmup);

        logMessage("Active compression RMS delta: " + juce::String(diffDb, 3) + " dB");
        expect(diffDb < -2.0f,
               "Active DynEQ should reduce the effective boost versus static EQ");
    }

    void testRapidMasterToggleStaysFinite()
    {
        beginTest("Rapid master toggle remains finite and bounded");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& apvts = proc.getAPVTS();

        auto band = makeBaseBand();
        band.dynMode = DynamicEQProcessor::DynamicMode_Compress;
        configureProcessor(proc, apvts, band, false);

        prime(proc, 12);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block(kChannels, kBlockSize);
        auto tone = makeTone(1, 0.7f);

        juce::AudioBuffer<float> captured(kChannels, 60 * kBlockSize);
        int writePos = 0;
        for (int b = 0; b < 60; ++b)
        {
            if (b == 10 || b == 30 || b == 50)
                setBool(apvts, "dynEqEnabled", true);
            if (b == 20 || b == 40)
                setBool(apvts, "dynEqEnabled", false);

            for (int ch = 0; ch < kChannels; ++ch)
                block.copyFrom(ch, 0, tone, ch, 0, kBlockSize);

            proc.processBlock(block, midi);

            for (int ch = 0; ch < kChannels; ++ch)
                captured.copyFrom(ch, writePos, block, ch, 0, kBlockSize);
            writePos += kBlockSize;
        }

        const float peak = maxAbsSample(captured);
        const float delta = maxDelta(captured);
        logMessage("toggle peak=" + juce::String(peak, 4)
                 + " maxDelta=" + juce::String(delta, 4));

        expect(isFinite(captured), "Rapid master toggles must not produce NaN/Inf");
        expect(peak < 8.0f, "Rapid master toggles produced an implausibly large burst");
        expect(delta < 4.0f, "Rapid master toggles produced an excessive discontinuity");
    }

    void testLinearPhaseDoesNotAddStaticIRContribution()
    {
        beginTest("Linear Phase: dynamic-owned band does not get extra static IR contribution");

        AIEqualizerAudioProcessor zl;
        zl.prepareToPlay(kSampleRate, kBlockSize);
        auto& zlApvts = zl.getAPVTS();

        AIEqualizerAudioProcessor lp;
        lp.prepareToPlay(kSampleRate, kBlockSize);
        auto& lpApvts = lp.getAPVTS();

        auto band = makeBaseBand();
        band.dynMode = DynamicEQProcessor::DynamicMode_Compress;

        configureProcessor(zl, zlApvts, band, true, 0);
        configureProcessor(lp, lpApvts, band, true, 2);
        lp.forceLinearIRReady();

        prime(zl, 20);
        prime(lp, 40);

        const auto signal = makeTone(80, 0.7f);
        const auto zlOut = processSignal(zl, signal);
        const auto lpOut = processSignal(lp, signal);

        const int warmup = kBlockSize * 48;
        const float diffDb = rmsDb(lpOut, warmup) - rmsDb(zlOut, warmup);

        logMessage("LP vs ZL RMS delta: " + juce::String(diffDb, 3) + " dB");
        expectWithinAbsoluteError(diffDb, 0.0f, 0.75f,
                                  "LP path should not add a static IR contribution on top of DynEQ");
    }

    void testLowCutCompressFallsBackToStaticEQ()
    {
        beginTest("LowCut + Compress falls back to static EQ");

        expectUnsupportedFilterFallsBackToStaticEQ(
            "LowCut + Compress",
            static_cast<int>(ParametricEQProcessor::LowCut),
            DynamicEQProcessor::DynamicMode_Compress,
            1000.0f,
            0.707f,
            2000.0f);
    }

    void testHighCutExpandFallsBackToStaticEQ()
    {
        beginTest("HighCut + Expand falls back to static EQ");

        expectUnsupportedFilterFallsBackToStaticEQ(
            "HighCut + Expand",
            static_cast<int>(ParametricEQProcessor::HighCut),
            DynamicEQProcessor::DynamicMode_Expand,
            1000.0f,
            0.707f,
            250.0f);
    }

    void testNotchCompressFallsBackToStaticEQ()
    {
        beginTest("Notch + Compress falls back to static EQ");

        expectUnsupportedFilterFallsBackToStaticEQ(
            "Notch + Compress",
            static_cast<int>(ParametricEQProcessor::Notch),
            DynamicEQProcessor::DynamicMode_Compress,
            1000.0f,
            8.0f,
            250.0f);
    }

    void testBandPassExpandFallsBackToStaticEQ()
    {
        beginTest("BandPass + Expand falls back to static EQ");

        expectUnsupportedFilterFallsBackToStaticEQ(
            "BandPass + Expand",
            static_cast<int>(ParametricEQProcessor::BandPass),
            DynamicEQProcessor::DynamicMode_Expand,
            1000.0f,
            2.0f,
            1000.0f);
    }
};

static DynEQRoutingOwnershipTest dynEQRoutingOwnershipTest;
