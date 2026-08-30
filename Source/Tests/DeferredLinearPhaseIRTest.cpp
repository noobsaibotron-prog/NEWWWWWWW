#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"
#include "Support/TestParameters.h"
#include "Support/ToneTransitionAudit.h"

#include <cmath>
#include <memory>

/**
 * Deferred Linear entry: request Linear while the IR is pinned unavailable,
 * then inject a production-shaped (centred) IR. The skipCrossfade drain +
 * unaligned first-load blend must fail these gates; after the pending +
 * finishAligned fix they must pass.
 *
 * Tolerances are declared before any capture is observed.
 */
class DeferredLinearPhaseIRTest final : public juce::UnitTest
{
public:
    DeferredLinearPhaseIRTest()
        : juce::UnitTest ("Deferred linear IR entry", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        const double rates[] = { 48000.0, 96000.0 };
        const int blocks[] = { 64, 128, 256 };
        for (double sr : rates)
            for (int blockSize : blocks)
                runDeferredZlToLinear (sr, blockSize, false);

        beginTest ("Linear requested after prepareToPlay with IR unavailable");
        runLinearAfterPrepare (48000.0, 128);

        beginTest ("Startup Linear loaded before prepareToPlay");
        runTrueStartupInLinear (48000.0, 128);

        beginTest ("Return to ZL while Linear entry is still pending");
        runReturnToZlWhilePending (48000.0, 128);

        beginTest ("Natural pending cancelled to Natural @48k/128");
        runNaturalPendingTo (48000.0, 128, 1, "Natural pending cancelled to Natural @48k/128");

        beginTest ("Natural pending then ZL @48k/128");
        runNaturalPendingTo (48000.0, 128, 0, "Natural pending then ZL @48k/128");

        // Product-default DynEQ+HQ: full ToneTransitionAudit gates. Restoring
        // a second dynamicEQProcessor.process on the same instance in one
        // finishAligned quantum reintroduces the ~17-sample hole and must fail.
        runDeferredZlToLinear (48000.0, 128, true);
        runDeferredZlToLinear (96000.0, 128, true);
    }

private:
    // Pre-registered gates (do not edit after seeing a result)
    static constexpr double kToneHz = 997.0;
    static constexpr float kAmp = 0.25f;
    static constexpr int kObserveSamples = 12288;
    static constexpr float kMaxSecondDiffRatio = 8.0f;
    static constexpr int kMaxQuietBurst = 0;
    static constexpr int kMaxDropout = 8;
    static constexpr int kMaxExactZeroRun = 8;
    static constexpr float kMaxDelta = 0.50f;
    static constexpr float kMinWindowRmsRatio = 0.25f;
    static constexpr int kRmsWindow64 = 64;
    static constexpr int kRmsWindow128 = 128;
    static constexpr int kRefRmsSamples = 2048;

    struct ExtraAudit
    {
        aieq::test::ToneTransitionAudit tone;
        int exactZeroRun = 0;
        float minRms64 = 0.0f;
        float minRms128 = 0.0f;
        float preRms = 0.0f;
        float postRms = 0.0f;
    };

    static float rmsOf (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        start = juce::jmax (0, start);
        const int end = juce::jmin (buffer.getNumSamples(), start + count);
        if (end <= start)
            return 0.0f;
        double acc = 0.0;
        const float* ch0 = buffer.getReadPointer (0);
        for (int i = start; i < end; ++i)
            acc += static_cast<double> (ch0[i]) * static_cast<double> (ch0[i]);
        return static_cast<float> (std::sqrt (acc / static_cast<double> (end - start)));
    }

    static int exactZeroRunOf (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        const int end = juce::jmin (buffer.getNumSamples(), start + count);
        const float* ch0 = buffer.getReadPointer (0);
        int run = 0;
        int best = 0;
        for (int i = start; i < end; ++i)
        {
            if (std::abs (ch0[i]) < 1.0e-12f)
                ++run;
            else
                run = 0;
            best = juce::jmax (best, run);
        }
        return best;
    }

    static float minWindowRms (const juce::AudioBuffer<float>& buffer,
                               int start, int count, int window)
    {
        const int end = juce::jmin (buffer.getNumSamples(), start + count);
        if (end - start < window)
            return rmsOf (buffer, start, end - start);
        float best = 1.0e9f;
        for (int i = start; i + window <= end; i += window)
            best = juce::jmin (best, rmsOf (buffer, i, window));
        return best;
    }

    ExtraAudit auditRange (const juce::AudioBuffer<float>& captured,
                           int start, int count,
                           double sampleRate,
                           int preStart, int postStart) const
    {
        ExtraAudit extra;
        extra.tone = aieq::test::auditToneTransition (
            captured, start, count, sampleRate, kToneHz);
        extra.exactZeroRun = exactZeroRunOf (captured, start, count);
        extra.minRms64 = minWindowRms (captured, start, count, kRmsWindow64);
        extra.minRms128 = minWindowRms (captured, start, count, kRmsWindow128);
        extra.preRms = rmsOf (captured, preStart, kRefRmsSamples);
        extra.postRms = rmsOf (captured, postStart, kRefRmsSamples);
        return extra;
    }

    void expectDeferredClean (const ExtraAudit& extra, const juce::String& label)
    {
        logMessage (label
                    + " secondDiffRatio=" + juce::String (extra.tone.secondDiffRatio, 2)
                    + " quietBursts=" + juce::String (extra.tone.quietBurstSamples)
                    + " dropout=" + juce::String (extra.tone.dropoutSamples)
                    + " exactZeroRun=" + juce::String (extra.exactZeroRun)
                    + " maxDelta=" + juce::String (extra.tone.maxDelta, 4)
                    + " minRms64=" + juce::String (extra.minRms64, 6)
                    + " minRms128=" + juce::String (extra.minRms128, 6)
                    + " preRms=" + juce::String (extra.preRms, 6)
                    + " postRms=" + juce::String (extra.postRms, 6));

        expect (! extra.tone.hasNaN, label + ": NaN");
        expect (! extra.tone.hasInf, label + ": Inf");
        expect (extra.tone.secondDiffRatio < kMaxSecondDiffRatio,
                label + ": secondDiffRatio=" + juce::String (extra.tone.secondDiffRatio, 2));
        expect (extra.tone.quietBurstSamples <= kMaxQuietBurst,
                label + ": quiet-region bursts=" + juce::String (extra.tone.quietBurstSamples));
        expect (extra.tone.dropoutSamples <= kMaxDropout,
                label + ": dropout=" + juce::String (extra.tone.dropoutSamples));
        expect (extra.exactZeroRun <= kMaxExactZeroRun,
                label + ": exact-zero run=" + juce::String (extra.exactZeroRun));
        expect (extra.tone.maxDelta < kMaxDelta,
                label + ": maxDelta=" + juce::String (extra.tone.maxDelta, 4));

        const float floorRms = kMinWindowRmsRatio * juce::jmin (extra.preRms, extra.postRms);
        expect (extra.minRms64 >= floorRms,
                label + ": minRms64=" + juce::String (extra.minRms64, 6)
                + " floor=" + juce::String (floorRms, 6));
        expect (extra.minRms128 >= floorRms,
                label + ": minRms128=" + juce::String (extra.minRms128, 6)
                + " floor=" + juce::String (floorRms, 6));
    }

    void configureIdle (AIEqualizerAudioProcessor& processor,
                        juce::AudioProcessorValueTreeState& apvts,
                        bool dynEqOn,
                        int phaseMode = 0)
    {
        processor.setNumActiveBands (1);
        AIEqualizerAudioProcessor::BandState band;
        band.frequency = static_cast<float> (kToneHz);
        band.gain = 0.0f;
        band.q = 1.0f;
        band.type = 2;
        band.enabled = true;
        band.solo = false;
        processor.setBandState (0, band);

        aieq::test::setBool (*this, apvts, "bypass", false);
        aieq::test::setBool (*this, apvts, "aiEnabled", false);
        aieq::test::setBool (*this, apvts, "autoGain", false);
        aieq::test::setBool (*this, apvts, "dynEqEnabled", dynEqOn);
        aieq::test::setFloat (*this, apvts, "dryWet", 100.0f);
        aieq::test::setFloat (*this, apvts, "outputGain", 0.0f);
        aieq::test::setChoice (*this, apvts, "qualityMode", dynEqOn ? 1 : 0);
        aieq::test::setChoice (*this, apvts, "msMode", 0);
        aieq::test::setChoice (*this, apvts, "numActiveBands", 0);
        aieq::test::setChoice (*this, apvts, "oversamplingFactor", 0);
        aieq::test::setChoice (*this, apvts, "phaseMode", phaseMode);
    }

    void fillTone (juce::AudioBuffer<float>& block, double sampleRate,
                   int absoluteSample) const
    {
        const double w = juce::MathConstants<double>::twoPi * kToneHz / sampleRate;
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* data = block.getWritePointer (ch);
            for (int i = 0; i < block.getNumSamples(); ++i)
                data[i] = kAmp * static_cast<float> (
                    std::sin (w * static_cast<double> (absoluteSample + i)));
        }
    }

    void runDeferredZlToLinear (double sampleRate, int blockSize, bool dynEqOn)
    {
        const juce::String label = juce::String ("ZL->LP deferred")
            + (dynEqOn ? " DynEQ+HQ" : "")
            + " @" + juce::String (sampleRate, 0) + "/" + juce::String (blockSize);
        beginTest (label);

        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        processor->prepareToPlay (sampleRate, blockSize);
        auto& apvts = processor->getAPVTS();
        configureIdle (*processor, apvts, dynEqOn);
        processor->forceLinearIRUnavailable();

        const int warmupBlocks = (kObserveSamples + blockSize - 1) / blockSize;
        const int pendingBlocks = warmupBlocks;
        const int postReadyBlocks = warmupBlocks;
        const int requestBlock = warmupBlocks;
        const int readyBlock = requestBlock + pendingBlocks;
        const int totalBlocks = readyBlock + postReadyBlocks;

        juce::AudioBuffer<float> captured (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            if (b == requestBlock)
                aieq::test::setChoice (*this, apvts, "phaseMode", 2);
            if (b == readyBlock)
                processor->forceLinearIRReadyCentered();

            fillTone (block, sampleRate, b * blockSize);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const int auditStart = requestBlock * blockSize;
        const int auditLen = captured.getNumSamples() - auditStart;
        const int preStart = juce::jmax (0, auditStart - kRefRmsSamples);
        const int postStart = juce::jmax (0, captured.getNumSamples() - kRefRmsSamples);
        const auto extra = auditRange (captured, auditStart, auditLen,
                                       sampleRate, preStart, postStart);
        expectDeferredClean (extra, label);
        processor->releaseResources();
    }

    void runLinearAfterPrepare (double sampleRate, int blockSize)
    {
        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        processor->prepareToPlay (sampleRate, blockSize);
        auto& apvts = processor->getAPVTS();
        configureIdle (*processor, apvts, false);
        processor->forceLinearIRUnavailable();
        aieq::test::setChoice (*this, apvts, "phaseMode", 2);

        const int pendingBlocks = (kObserveSamples + blockSize - 1) / blockSize;
        const int postReadyBlocks = pendingBlocks;
        const int readyBlock = pendingBlocks;
        const int totalBlocks = readyBlock + postReadyBlocks;

        juce::AudioBuffer<float> captured (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            if (b == readyBlock)
                processor->forceLinearIRReadyCentered();
            fillTone (block, sampleRate, b * blockSize);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const auto extra = auditRange (captured, 0, captured.getNumSamples(),
                                       sampleRate, 0,
                                       juce::jmax (0, captured.getNumSamples() - kRefRmsSamples));
        expectDeferredClean (extra, "Linear after prepare deferred @48k/128");
        processor->releaseResources();
    }

    void runTrueStartupInLinear (double sampleRate, int blockSize)
    {
        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        auto& apvts = processor->getAPVTS();
        aieq::test::setChoice (*this, apvts, "phaseMode", 2);
        processor->prepareToPlay (sampleRate, blockSize);
        processor->forceLinearIRUnavailable();
        configureIdle (*processor, apvts, false, 2);

        const int pendingBlocks = (kObserveSamples + blockSize - 1) / blockSize;
        const int postReadyBlocks = pendingBlocks;
        const int readyBlock = pendingBlocks;
        const int totalBlocks = readyBlock + postReadyBlocks;

        juce::AudioBuffer<float> captured (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            if (b == readyBlock)
                processor->forceLinearIRReadyCentered();
            fillTone (block, sampleRate, b * blockSize);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const auto extra = auditRange (captured, 0, captured.getNumSamples(),
                                       sampleRate, 0,
                                       juce::jmax (0, captured.getNumSamples() - kRefRmsSamples));
        expectDeferredClean (extra, "true startup Linear before prepare @48k/128");
        processor->releaseResources();
    }

    void runReturnToZlWhilePending (double sampleRate, int blockSize)
    {
        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        processor->prepareToPlay (sampleRate, blockSize);
        auto& apvts = processor->getAPVTS();
        configureIdle (*processor, apvts, false);
        processor->forceLinearIRUnavailable();

        const int warmupBlocks = (kObserveSamples + blockSize - 1) / blockSize;
        const int pendingBlocks = warmupBlocks / 2;
        const int requestBlock = warmupBlocks;
        const int cancelBlock = requestBlock + pendingBlocks;
        const int totalBlocks = cancelBlock + warmupBlocks;

        juce::AudioBuffer<float> captured (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            if (b == requestBlock)
                aieq::test::setChoice (*this, apvts, "phaseMode", 2);
            if (b == cancelBlock)
                aieq::test::setChoice (*this, apvts, "phaseMode", 0);

            fillTone (block, sampleRate, b * blockSize);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const int auditStart = requestBlock * blockSize;
        const auto extra = auditRange (
            captured, auditStart, captured.getNumSamples() - auditStart,
            sampleRate,
            juce::jmax (0, auditStart - kRefRmsSamples),
            juce::jmax (0, captured.getNumSamples() - kRefRmsSamples));
        expectDeferredClean (extra, "pending Linear cancelled to ZL @48k/128");
        processor->releaseResources();
    }

    void runNaturalPendingTo (double sampleRate, int blockSize, int destPhase,
                              const juce::String& label)
    {
        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        processor->prepareToPlay (sampleRate, blockSize);
        auto& apvts = processor->getAPVTS();
        configureIdle (*processor, apvts, false);

        const int settleBlocks = (kObserveSamples + blockSize - 1) / blockSize;
        const int pendingBlocks = settleBlocks / 2;
        const int naturalBlock = settleBlocks;
        const int requestBlock = naturalBlock + settleBlocks;
        const int destBlock = requestBlock + pendingBlocks;
        const int totalBlocks = destBlock + settleBlocks;

        juce::AudioBuffer<float> captured (2, totalBlocks * blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < totalBlocks; ++b)
        {
            if (b == naturalBlock)
                aieq::test::setChoice (*this, apvts, "phaseMode", 1);
            if (b == requestBlock)
            {
                processor->forceLinearIRUnavailable();
                aieq::test::setChoice (*this, apvts, "phaseMode", 2);
            }
            if (b == destBlock)
                aieq::test::setChoice (*this, apvts, "phaseMode", destPhase);

            fillTone (block, sampleRate, b * blockSize);
            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                captured.copyFrom (ch, b * blockSize, block, ch, 0, blockSize);
        }

        const int auditStart = requestBlock * blockSize;
        const auto extra = auditRange (
            captured, auditStart, captured.getNumSamples() - auditStart,
            sampleRate,
            juce::jmax (0, auditStart - kRefRmsSamples),
            juce::jmax (0, captured.getNumSamples() - kRefRmsSamples));
        expectDeferredClean (extra, label);
        processor->releaseResources();
    }
};

static DeferredLinearPhaseIRTest deferredLinearPhaseIRTest;
