#if JUCE_UNIT_TESTS

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include "../DSP/DynamicEQProcessor.h"
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

class DynamicEQSpectralArtifactTest : public juce::UnitTest
{
public:
    DynamicEQSpectralArtifactTest()
        : juce::UnitTest("DynamicEQ Spectral Artifact", "DSP") {}

    void runTest() override
    {
        testNoSpectralArtifactAtControlSliceRate();
        testSustainedCompressionEnvelopeStability();
        testThresholdDragRemainsSpectrallyClean();
        testMultiBandCumulativeNoiseFloor();
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kChannels = 2;
    static constexpr int kBlockSize = 512;
    static constexpr int kFftOrder = 16;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr double kBinHz = kSampleRate / static_cast<double>(kFftSize);
    static constexpr double kControlSliceRateHz = kSampleRate / 16.0;

    static float alignedFreqForBin(int bin) noexcept
    {
        return static_cast<float>(kBinHz * static_cast<double>(bin));
    }

    static int binForHz(double hz) noexcept
    {
        return juce::jlimit(0, (kFftSize / 2) - 1,
                            static_cast<int>(std::llround(hz / kBinHz)));
    }

    static DynamicEQProcessor::DynamicBandParams makeCompressBand(float freqHz,
                                                                  float gainDb = 12.0f,
                                                                  float q = 2.0f)
    {
        DynamicEQProcessor::DynamicBandParams params;
        params.frequency = freqHz;
        params.gain = gainDb;
        params.q = q;
        params.filterType = 2; // Peak
        params.enabled = true;
        params.dynamicMode = DynamicEQProcessor::DynamicMode_Compress;
        params.threshold = -30.0f;
        params.ratio = 8.0f;
        params.attackMs = 1.0f;
        params.releaseMs = 50.0f;
        params.range = 24.0f;
        params.knee = 0.0f;
        params.detection = DynamicEQProcessor::DetectionMode_RMS;
        return params;
    }

    static std::unique_ptr<DynamicEQProcessor> makePrepared()
    {
        auto proc = std::make_unique<DynamicEQProcessor>();
        proc->prepare(kSampleRate, kBlockSize, kChannels);
        proc->setGlobalMix(1.0f);
        proc->setAutoMakeup(false);
        return proc;
    }

    template <typename Generator>
    static juce::AudioBuffer<float> renderSignal(DynamicEQProcessor& proc,
                                                 int totalSamples,
                                                 Generator&& generator)
    {
        juce::AudioBuffer<float> output(kChannels, totalSamples);
        output.clear();

        juce::AudioBuffer<float> block(kChannels, kBlockSize);
        int rendered = 0;

        while (rendered < totalSamples)
        {
            const int thisBlock = juce::jmin(kBlockSize, totalSamples - rendered);
            block.clear();

            for (int ch = 0; ch < kChannels; ++ch)
            {
                auto* data = block.getWritePointer(ch);
                for (int i = 0; i < thisBlock; ++i)
                    data[i] = generator(rendered + i, ch);
            }

            if (thisBlock != kBlockSize)
            {
                juce::AudioBuffer<float> shortBlock(kChannels, thisBlock);
                for (int ch = 0; ch < kChannels; ++ch)
                    shortBlock.copyFrom(ch, 0, block, ch, 0, thisBlock);
                proc.process(shortBlock);
                for (int ch = 0; ch < kChannels; ++ch)
                    output.copyFrom(ch, rendered, shortBlock, ch, 0, thisBlock);
            }
            else
            {
                proc.process(block);
                for (int ch = 0; ch < kChannels; ++ch)
                    output.copyFrom(ch, rendered, block, ch, 0, thisBlock);
            }

            rendered += thisBlock;
        }

        return output;
    }

    static std::vector<float> computeMagnitudeSpectrumDb(const juce::AudioBuffer<float>& buffer,
                                                         int startSample,
                                                         int channel = 0)
    {
        std::vector<float> fftData(static_cast<size_t>(kFftSize * 2), 0.0f);
        const auto* input = buffer.getReadPointer(channel, startSample);
        for (int i = 0; i < kFftSize; ++i)
            fftData[static_cast<size_t>(i)] = input[i];

        juce::dsp::FFT fft(kFftOrder);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        std::vector<float> spectrumDb(static_cast<size_t>(kFftSize / 2), -300.0f);
        for (int i = 0; i < kFftSize / 2; ++i)
            spectrumDb[static_cast<size_t>(i)] =
                juce::Decibels::gainToDecibels(fftData[static_cast<size_t>(i)], -300.0f);
        return spectrumDb;
    }

    static float maxDbOutsideBands(const std::vector<float>& spectrumDb,
                                   const std::vector<std::pair<float, float>>& allowedBandsHz,
                                   float minFreqHz = 20.0f)
    {
        float maxDb = -300.0f;

        for (size_t bin = 0; bin < spectrumDb.size(); ++bin)
        {
            const float freqHz = static_cast<float>(static_cast<double>(bin) * kBinHz);
            if (freqHz < minFreqHz)
                continue;

            bool allowed = false;
            for (const auto& band : allowedBandsHz)
            {
                if (freqHz >= band.first && freqHz <= band.second)
                {
                    allowed = true;
                    break;
                }
            }

            if (!allowed)
                maxDb = juce::jmax(maxDb, spectrumDb[bin]);
        }

        return maxDb;
    }

    static float maxDbNearHz(const std::vector<float>& spectrumDb,
                             float centerHz,
                             int radiusBins = 2)
    {
        const int center = binForHz(centerHz);
        const int start = juce::jmax(0, center - radiusBins);
        const int end = juce::jmin(static_cast<int>(spectrumDb.size()) - 1, center + radiusBins);

        float maxDb = -300.0f;
        for (int bin = start; bin <= end; ++bin)
            maxDb = juce::jmax(maxDb, spectrumDb[static_cast<size_t>(bin)]);
        return maxDb;
    }

    static std::vector<float> makeRmsEnvelope(const juce::AudioBuffer<float>& buffer,
                                              int startSample,
                                              int hopSize,
                                              int count,
                                              int channel = 0)
    {
        std::vector<float> envelope(static_cast<size_t>(count), 0.0f);
        const auto* data = buffer.getReadPointer(channel);

        for (int frame = 0; frame < count; ++frame)
        {
            const int offset = startSample + frame * hopSize;
            double sum = 0.0;
            for (int i = 0; i < hopSize; ++i)
            {
                const double sample = static_cast<double>(data[offset + i]);
                sum += sample * sample;
            }
            envelope[static_cast<size_t>(frame)] =
                static_cast<float>(std::sqrt(sum / static_cast<double>(hopSize)));
        }

        return envelope;
    }

    static std::vector<float> computeMagnitudeSpectrumDb(const std::vector<float>& signal)
    {
        std::vector<float> fftData(static_cast<size_t>(signal.size() * 2), 0.0f);
        for (size_t i = 0; i < signal.size(); ++i)
            fftData[i] = signal[i];

        int order = 0;
        int size = static_cast<int>(signal.size());
        while (size > 1)
        {
            size >>= 1;
            ++order;
        }
        juce::dsp::FFT fft(order);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        std::vector<float> spectrumDb(signal.size() / 2, -300.0f);
        for (size_t i = 0; i < spectrumDb.size(); ++i)
            spectrumDb[i] = juce::Decibels::gainToDecibels(fftData[i], -300.0f);
        return spectrumDb;
    }

    void testNoSpectralArtifactAtControlSliceRate()
    {
        beginTest("No spectral artifact at control-slice-related frequencies");

        constexpr int kToneBin = 1365;
        constexpr double kModHz = 8.0;
        constexpr float kCarrierAmp = 0.55f;
        constexpr float kDepth = 0.45f;
        constexpr int kTotalSeconds = 8;
        const float toneHz = alignedFreqForBin(kToneBin);
        const int totalSamples = static_cast<int>(kSampleRate) * kTotalSeconds;

        auto proc = makePrepared();
        proc->setBandParams(0, makeCompressBand(toneHz));

        const auto output = renderSignal(
            *proc, totalSamples,
            [toneHz](int sampleIndex, int /*channel*/)
            {
                const double t = static_cast<double>(sampleIndex) / kSampleRate;
                const double env = kCarrierAmp * (1.0 + kDepth * std::sin(juce::MathConstants<double>::twoPi * kModHz * t));
                const double carrier = std::sin(juce::MathConstants<double>::twoPi * static_cast<double>(toneHz) * t);
                return static_cast<float>(env * carrier);
            });

        const int analysisStart = totalSamples - kFftSize;
        const auto spectrumDb = computeMagnitudeSpectrumDb(output, analysisStart);
        const float fundamentalDb = spectrumDb[static_cast<size_t>(kToneBin)];

        // The intended AM sidebands stay clustered around the carrier. Anything
        // well outside that band is a candidate crackle / coefficient-update spur.
        const std::vector<std::pair<float, float>> allowedBandsHz {
            { toneHz - 400.0f, toneHz + 400.0f }
        };
        const float maxFarSpurDb = maxDbOutsideBands(spectrumDb, allowedBandsHz, 100.0f);

        const std::array<float, 6> controlSliceFamilyHz {
            static_cast<float>(kControlSliceRateHz),
            static_cast<float>(2.0 * kControlSliceRateHz),
            toneHz + static_cast<float>(kControlSliceRateHz),
            toneHz - static_cast<float>(kControlSliceRateHz),
            toneHz + static_cast<float>(2.0 * kControlSliceRateHz),
            toneHz - static_cast<float>(2.0 * kControlSliceRateHz)
        };

        float maxControlSliceFamilyDb = -300.0f;
        for (float freqHz : controlSliceFamilyHz)
        {
            if (freqHz > 20.0f && freqHz < static_cast<float>(kSampleRate * 0.5))
                maxControlSliceFamilyDb = juce::jmax(maxControlSliceFamilyDb,
                                                     maxDbNearHz(spectrumDb, freqHz, 2));
        }

        const float farSpurSuppressionDb = fundamentalDb - maxFarSpurDb;
        const float controlSliceSuppressionDb = fundamentalDb - maxControlSliceFamilyDb;

        logMessage("carrier @ " + juce::String(toneHz, 3)
                   + " Hz = " + juce::String(fundamentalDb, 1) + " dB");
        logMessage("max far spur = " + juce::String(maxFarSpurDb, 1)
                   + " dB, suppression = " + juce::String(farSpurSuppressionDb, 1) + " dB");
        logMessage("max control-slice family spur = " + juce::String(maxControlSliceFamilyDb, 1)
                   + " dB, suppression = " + juce::String(controlSliceSuppressionDb, 1) + " dB");

        expect(farSpurSuppressionDb > 50.0f,
               "Unexpected far-out spectral residue under continuous compression modulation");
        expect(controlSliceSuppressionDb > 45.0f,
               "Potential control-slice-related spur family is too hot");
    }

    void testSustainedCompressionEnvelopeStability()
    {
        beginTest("Sustained compression envelope remains spectrally stable");

        // Use a carrier that completes an integer number of cycles in the
        // envelope-analysis hop, otherwise plain RMS batching itself creates
        // deterministic measurement ripple unrelated to the DynEQ path.
        constexpr int kToneBin = 1536; // 1125 Hz => exactly 3 cycles per 128-sample hop @ 48 kHz
        constexpr int kTotalSeconds = 12;
        constexpr int kEnvelopeHop = 128;
        constexpr int kEnvelopeFrames = 2048; // power-of-two for FFT

        const float toneHz = alignedFreqForBin(kToneBin);
        const int totalSamples = static_cast<int>(kSampleRate) * kTotalSeconds;

        auto proc = makePrepared();
        proc->setBandParams(0, makeCompressBand(toneHz));

        const auto output = renderSignal(
            *proc, totalSamples,
            [toneHz](int sampleIndex, int /*channel*/)
            {
                const double t = static_cast<double>(sampleIndex) / kSampleRate;
                return 0.5f * static_cast<float>(
                    std::sin(juce::MathConstants<double>::twoPi * static_cast<double>(toneHz) * t));
            });

        const int envelopeSpan = kEnvelopeFrames * kEnvelopeHop;
        const int envelopeStart = totalSamples - envelopeSpan;
        auto envelope = makeRmsEnvelope(output, envelopeStart, kEnvelopeHop, kEnvelopeFrames);

        const float meanRms = std::accumulate(envelope.begin(), envelope.end(), 0.0f)
                            / static_cast<float>(envelope.size());
        double variance = 0.0;
        for (float value : envelope)
        {
            const double diff = static_cast<double>(value - meanRms);
            variance += diff * diff;
        }
        const float stddevRms = static_cast<float>(
            std::sqrt(variance / static_cast<double>(envelope.size())));
        const float relativeStdDb = juce::Decibels::gainToDecibels(
            std::max(stddevRms / std::max(meanRms, 1.0e-9f), 1.0e-9f), -300.0f);

        const auto envelopeSpectrumDb = computeMagnitudeSpectrumDb(envelope);
        const float dcDb = envelopeSpectrumDb[0];
        float maxAcDb = -300.0f;
        for (size_t i = 1; i < envelopeSpectrumDb.size(); ++i)
            maxAcDb = juce::jmax(maxAcDb, envelopeSpectrumDb[i]);
        const float dcSuppressionDb = dcDb - maxAcDb;

        logMessage("mean envelope RMS = " + juce::String(meanRms, 6));
        logMessage("relative envelope stddev = " + juce::String(relativeStdDb, 1) + " dB");
        logMessage("envelope DC = " + juce::String(dcDb, 1)
                   + " dB, strongest AC line = " + juce::String(maxAcDb, 1)
                   + " dB, suppression = " + juce::String(dcSuppressionDb, 1) + " dB");

        expect(relativeStdDb < -45.0f,
               "Sustained compression envelope is varying too much in steady state");
        expect(dcSuppressionDb > 55.0f,
               "Steady-state envelope shows unexpectedly strong periodic modulation");
    }

    void testThresholdDragRemainsSpectrallyClean()
    {
        beginTest("Threshold drag on an active band remains spectrally clean");

        constexpr int kToneBin = 1365;
        constexpr double kThresholdModHz = 1.5;
        constexpr int kTotalSeconds = 8;

        const float toneHz = alignedFreqForBin(kToneBin);
        const int totalSamples = static_cast<int>(kSampleRate) * kTotalSeconds;

        auto proc = makePrepared();
        auto params = makeCompressBand(toneHz);
        params.threshold = -20.0f;
        proc->setBandParams(0, params);

        juce::AudioBuffer<float> output(kChannels, totalSamples);
        output.clear();
        juce::AudioBuffer<float> block(kChannels, kBlockSize);

        int rendered = 0;
        while (rendered < totalSamples)
        {
            const double tBlock = static_cast<double>(rendered) / kSampleRate;
            params.threshold = static_cast<float>(
                -20.0 + 18.0 * std::sin(juce::MathConstants<double>::twoPi * kThresholdModHz * tBlock));
            proc->setBandParams(0, params);

            const int thisBlock = juce::jmin(kBlockSize, totalSamples - rendered);
            block.clear();

            for (int ch = 0; ch < kChannels; ++ch)
            {
                auto* data = block.getWritePointer(ch);
                for (int i = 0; i < thisBlock; ++i)
                {
                    const double t = static_cast<double>(rendered + i) / kSampleRate;
                    data[i] = 0.5f * static_cast<float>(
                        std::sin(juce::MathConstants<double>::twoPi * static_cast<double>(toneHz) * t));
                }
            }

            if (thisBlock != kBlockSize)
            {
                juce::AudioBuffer<float> shortBlock(kChannels, thisBlock);
                for (int ch = 0; ch < kChannels; ++ch)
                    shortBlock.copyFrom(ch, 0, block, ch, 0, thisBlock);
                proc->process(shortBlock);
                for (int ch = 0; ch < kChannels; ++ch)
                    output.copyFrom(ch, rendered, shortBlock, ch, 0, thisBlock);
            }
            else
            {
                proc->process(block);
                for (int ch = 0; ch < kChannels; ++ch)
                    output.copyFrom(ch, rendered, block, ch, 0, thisBlock);
            }

            rendered += thisBlock;
        }

        const int analysisStart = totalSamples - kFftSize;
        const auto spectrumDb = computeMagnitudeSpectrumDb(output, analysisStart);
        const float fundamentalDb = spectrumDb[static_cast<size_t>(kToneBin)];

        const std::vector<std::pair<float, float>> allowedBandsHz {
            { toneHz - 350.0f, toneHz + 350.0f }
        };
        const float maxFarSpurDb = maxDbOutsideBands(spectrumDb, allowedBandsHz, 100.0f);
        const float maxControlSliceFamilyDb = juce::jmax(
            maxDbNearHz(spectrumDb, static_cast<float>(kControlSliceRateHz), 2),
            juce::jmax(
                maxDbNearHz(spectrumDb, toneHz + static_cast<float>(kControlSliceRateHz), 2),
                maxDbNearHz(spectrumDb, toneHz - static_cast<float>(kControlSliceRateHz), 2)));

        const float farSpurSuppressionDb = fundamentalDb - maxFarSpurDb;
        const float controlSliceSuppressionDb = fundamentalDb - maxControlSliceFamilyDb;

        logMessage("threshold-drag carrier = " + juce::String(fundamentalDb, 1) + " dB");
        logMessage("threshold-drag max far spur = " + juce::String(maxFarSpurDb, 1)
                   + " dB, suppression = " + juce::String(farSpurSuppressionDb, 1) + " dB");
        logMessage("threshold-drag control-slice family = " + juce::String(maxControlSliceFamilyDb, 1)
                   + " dB, suppression = " + juce::String(controlSliceSuppressionDb, 1) + " dB");

        expect(farSpurSuppressionDb > 45.0f,
               "Threshold drag creates too much far-out spectral residue");
        expect(controlSliceSuppressionDb > 40.0f,
               "Threshold drag creates a hot control-slice-related spur family");
    }

    void testMultiBandCumulativeNoiseFloor()
    {
        beginTest("Multi-band dynamic ownership does not build an excessive spur floor");

        constexpr std::array<int, 4> kToneBins { 341, 683, 1365, 2731 };
        constexpr std::array<double, 4> kCarrierPhases {
            0.0, 0.7, 1.4, 2.1
        };
        constexpr std::array<double, 4> kModPhases {
            0.0, 1.1, 2.2, 3.3
        };
        constexpr double kModHz = 7.0;
        constexpr float kToneAmp = 0.12f;
        constexpr float kDepth = 0.35f;
        constexpr int kTotalSeconds = 8;

        const int totalSamples = static_cast<int>(kSampleRate) * kTotalSeconds;
        auto proc = makePrepared();

        for (size_t i = 0; i < kToneBins.size(); ++i)
            proc->setBandParams(static_cast<int>(i), makeCompressBand(alignedFreqForBin(kToneBins[i])));

        const auto output = renderSignal(
            *proc, totalSamples,
            [=](int sampleIndex, int /*channel*/)
            {
                const double t = static_cast<double>(sampleIndex) / kSampleRate;
                double sample = 0.0;
                for (size_t i = 0; i < kToneBins.size(); ++i)
                {
                    const double carrierHz = alignedFreqForBin(kToneBins[i]);
                    const double env = 1.0 + kDepth * std::sin(
                        juce::MathConstants<double>::twoPi * kModHz * t + kModPhases[i]);
                    sample += static_cast<double>(kToneAmp) * env
                           * std::sin(juce::MathConstants<double>::twoPi * carrierHz * t + kCarrierPhases[i]);
                }
                return static_cast<float>(sample);
            });

        const int analysisStart = totalSamples - kFftSize;
        const auto spectrumDb = computeMagnitudeSpectrumDb(output, analysisStart);

        std::vector<std::pair<float, float>> allowedBandsHz;
        float strongestWantedDb = -300.0f;
        for (int toneBin : kToneBins)
        {
            const float freqHz = alignedFreqForBin(toneBin);
            allowedBandsHz.emplace_back(freqHz - 300.0f, freqHz + 300.0f);
            strongestWantedDb = juce::jmax(strongestWantedDb,
                                           maxDbNearHz(spectrumDb, freqHz, 1));
        }

        const float maxUnwantedDb = maxDbOutsideBands(spectrumDb, allowedBandsHz, 100.0f);
        const float suppressionDb = strongestWantedDb - maxUnwantedDb;

        logMessage("strongest wanted tone = " + juce::String(strongestWantedDb, 1) + " dB");
        logMessage("max unwanted spur floor = " + juce::String(maxUnwantedDb, 1)
                   + " dB, suppression = " + juce::String(suppressionDb, 1) + " dB");

        expect(suppressionDb > 45.0f,
               "Concurrent dynamic bands produce too much far-out spectral residue");
    }
};

static DynamicEQSpectralArtifactTest dynamicEQSpectralArtifactTest;

#endif // JUCE_UNIT_TESTS
