// Modular JUCE includes, as every other test in this suite does: the project
// does not generate the JuceHeader.h umbrella. juce_audio_basics is what
// supplies AudioBuffer and MathConstants.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../DSP/SpectrumAnalyzer.h"

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace
{
class SpectrumAnalyzerOwnershipTest final : public juce::UnitTest
{
public:
    SpectrumAnalyzerOwnershipTest()
        : juce::UnitTest("SpectrumAnalyzer ownership under live reconfiguration", "ThreadSafety")
    {
    }

    void runTest() override
    {
        beginTest("Audio push + GUI FFT/config + legacy RT snapshot stay coherent");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 256;
        constexpr int iterations = 6000;

        SpectrumAnalyzer analyzer;
        analyzer.prepare(sampleRate, blockSize);

        juce::AudioBuffer<float> block(2, blockSize);
        std::vector<float> snapshot(4096, SpectrumAnalyzer::minDecibels);

        std::atomic<bool> stopGui { false };
        std::atomic<int> guiPasses { 0 };
        std::atomic<int> invalidSnapshots { 0 };
        std::atomic<int> validSnapshots { 0 };

        std::thread guiThread([&]
        {
            while (!stopGui.load(std::memory_order_acquire))
            {
                analyzer.processFFT();
                guiPasses.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }

            // Drain a few final passes so the last request is observed even if
            // the producer stopped immediately after publishing it.
            for (int i = 0; i < 32; ++i)
                analyzer.processFFT();
        });

        double phase = 0.0;
        const double phaseInc = juce::MathConstants<double>::twoPi * 997.0 / sampleRate;

        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            if ((iteration % 19) == 0)
            {
                const int mode = (iteration / 19) & 3;
                analyzer.setFFTResolution(static_cast<SpectrumAnalyzer::Resolution>(10 + mode));
            }

            if ((iteration % 23) == 0)
            {
                const int speed = (iteration / 23) % 3;
                analyzer.setSpeed(speed == 0 ? SpectrumAnalyzer::Speed::Fast
                                             : speed == 2 ? SpectrumAnalyzer::Speed::Slow
                                                          : SpectrumAnalyzer::Speed::Medium);
            }

            for (int s = 0; s < blockSize; ++s)
            {
                const float value = 0.25f * static_cast<float>(std::sin(phase));
                phase += phaseInc;
                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
                block.setSample(0, s, value);
                block.setSample(1, s, value * 0.91f);
            }

            analyzer.pushSamples(block);

            // This is the legacy audio-thread AI consumer. It must never see a
            // torn buffer while processFFT() may publish several frames per call.
            if ((iteration % 5) == 0)
            {
                const int copied = analyzer.copySmoothedSpectrumInto(snapshot);
                if (copied > 0)
                {
                    const bool validSize = copied == 512 || copied == 1024
                                        || copied == 2048 || copied == 4096;
                    bool validValues = validSize;
                    for (int i = 0; i < copied && validValues; ++i)
                    {
                        const float value = snapshot[static_cast<size_t>(i)];
                        validValues = std::isfinite(value)
                                   && value >= SpectrumAnalyzer::minDecibels - 0.01f
                                   && value <= SpectrumAnalyzer::maxDecibels + 0.01f;
                    }

                    if (validValues)
                        validSnapshots.fetch_add(1, std::memory_order_relaxed);
                    else
                        invalidSnapshots.fetch_add(1, std::memory_order_relaxed);
                }
            }

            if ((iteration & 31) == 0)
                std::this_thread::yield();
        }

        // Final requested state. processFFT() applies requests before checking
        // FIFO availability, so it need not consume another frame to converge.
        analyzer.setFFTResolution(SpectrumAnalyzer::Resolution::Max);
        analyzer.setSpeed(SpectrumAnalyzer::Speed::Slow);

        stopGui.store(true, std::memory_order_release);
        guiThread.join();
        analyzer.processFFT();

        expect(guiPasses.load(std::memory_order_relaxed) > 0,
               "GUI consumer never executed processFFT()");
        expect(validSnapshots.load(std::memory_order_relaxed) > 0,
               "No published legacy spectrum snapshot was observed");
        expectEquals(invalidSnapshots.load(std::memory_order_relaxed), 0,
                     "Observed malformed/torn spectrum snapshot");
        expect(analyzer.getFFTResolution() == SpectrumAnalyzer::Resolution::Max,
               "Latest resolution request did not converge on GUI-owned state");
        expect(analyzer.getSpeed() == SpectrumAnalyzer::Speed::Slow,
               "Latest speed request did not converge on GUI-owned state");
    }
};

static SpectrumAnalyzerOwnershipTest spectrumAnalyzerOwnershipTest;
} // namespace
