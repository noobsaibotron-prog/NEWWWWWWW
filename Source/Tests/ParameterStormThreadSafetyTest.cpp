#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <latch>
#include <thread>
#include <vector>
#include "../PluginProcessor.h"

/**
 * Thread-safety regression — in-process reproducer of pluginval 1.0.4
 * "Parameter thread safety" (see REPORTS/KNOWN_ISSUE_pluginval_s10_param_thread_safety.md).
 *
 * pluginval crashes seed-dependently (EXC_BAD_ACCESS in the VST3 wrapper's
 * ClientRemappedBuffer teardown — a downstream-corruption signature; reproducer seed
 * 0x782104d at strictness 8). The crash was isolated to a deterministic spatial bug:
 * the msMode transition path sent the full preallocated transition buffer into an
 * oversampler sized for the current block. This test keeps the pluginval-style storm in
 * the product harness so ASan/TSan keep guarding the fixed path.
 *
 * Workload (models pluginval's test semantics):
 *  - one "audio" thread hammering processBlock()  (pluginval also drives it off-message)
 *  - two writer threads hammering setValueNotifyingHost() across ALL parameters —
 *    listeners fire on the CALLING thread, same delivery as the VST3 controller path
 *  - one writer dedicated to the transition-machinery params (phaseMode / msMode /
 *    oversamplingFactor / qualityMode), which arm crossfades, pendingReset and IR rebuilds
 *
 * Expected GREEN after the msMode-transition overflow fix. Coverage gap (documented):
 * the VST3 wrapper layer itself is not exercised here — the end-to-end judge remains
 * pluginval with the pinned seed.
 *
 * The workload is progress-based rather than wall-clock based. This matters under
 * sanitizers, where a fixed sleep can expire before the audio worker receives enough
 * CPU time and turn a clean product run into a false RED.
 */
class ParameterStormThreadSafetyTest : public juce::UnitTest
{
public:
    ParameterStormThreadSafetyTest()
        : juce::UnitTest("Parameter Storm Thread Safety", "ThreadSafety") {}

    void runTest() override
    {
        beginTest("processBlock survives a concurrent parameter storm");

        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm); // ensure MessageManager exists; current thread becomes message thread

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(44100.0, blockSize);

        const auto& params = proc.getParameters();
        expect(params.size() > 0, "processor exposes parameters");

        // Transition-machinery params: these arm crossfades / pendingReset / IR rebuilds.
        std::vector<juce::RangedAudioParameter*> modeParams;
        for (const char* id : { "phaseMode", "msMode", "oversamplingFactor", "qualityMode" })
            if (auto* p = proc.getAPVTS().getParameter(id))
                modeParams.push_back(p);

        constexpr int minAudioBlocks = 64;
        constexpr int minOverlappingWrites = 128;
        constexpr int maxAudioBlocks = 512;

        std::latch workersReady { 4 };
        std::latch startStorm { 1 };
        std::atomic<bool> stop { false };
        std::atomic<bool> audioInCallback { false };
        std::atomic<bool> nonFinite { false };
        std::atomic<int>  blocksProcessed { 0 };
        std::atomic<int>  overlappingWrites { 0 };
        std::atomic<int>  parameterWrites { 0 };
        std::atomic<int>  modeWrites { 0 };

        std::thread audioThread([&]
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            juce::Random rng(0x51027A0D); // fixed seed: deterministic input signal
            workersReady.count_down();
            startStorm.wait();

            while (blocksProcessed.load(std::memory_order_relaxed) < maxAudioBlocks)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* d = buffer.getWritePointer(ch);
                    for (int i = 0; i < blockSize; ++i)
                        d[i] = rng.nextFloat() * 0.5f - 0.25f;
                }

                audioInCallback.store(true, std::memory_order_release);
                proc.processBlock(buffer, midi);
                audioInCallback.store(false, std::memory_order_release);
                blocksProcessed.fetch_add(1, std::memory_order_relaxed);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    const auto* d = buffer.getReadPointer(ch);
                    for (int i = 0; i < blockSize; ++i)
                        if (!std::isfinite(d[i]))
                        {
                            nonFinite.store(true, std::memory_order_relaxed);
                            break;
                        }
                }

                if (blocksProcessed.load(std::memory_order_relaxed) >= minAudioBlocks
                    && overlappingWrites.load(std::memory_order_relaxed) >= minOverlappingWrites)
                    break;

                std::this_thread::yield();
            }

            stop.store(true, std::memory_order_release);
        });

        auto writerFn = [&](juce::int64 seed)
        {
            juce::Random rng(seed);
            workersReady.count_down();
            startStorm.wait();
            while (!stop.load(std::memory_order_acquire))
            {
                auto* p = params[rng.nextInt(params.size())];
                p->setValueNotifyingHost(rng.nextFloat());
                parameterWrites.fetch_add(1, std::memory_order_relaxed);
                if (audioInCallback.load(std::memory_order_acquire))
                    overlappingWrites.fetch_add(1, std::memory_order_relaxed);
            }
        };
        std::thread writer1(writerFn, (juce::int64) 0xA1EC0FFEE);
        std::thread writer2(writerFn, (juce::int64) 0xB2D15EA5E);

        std::thread modeWriter([&]
        {
            juce::Random rng(0xC3A11D0);
            workersReady.count_down();
            startStorm.wait();
            while (!stop.load(std::memory_order_acquire) && !modeParams.empty())
            {
                auto* p = modeParams[(size_t) rng.nextInt((int) modeParams.size())];
                p->setValueNotifyingHost(rng.nextFloat());
                modeWrites.fetch_add(1, std::memory_order_relaxed);
                if (audioInCallback.load(std::memory_order_acquire))
                    overlappingWrites.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });

        workersReady.wait();
        startStorm.count_down();
        audioThread.join();
        writer1.join();
        writer2.join();
        modeWriter.join();

        proc.releaseResources();

        logMessage("blocks processed during storm: " + juce::String(blocksProcessed.load())
                   + ", overlapping writes: " + juce::String(overlappingWrites.load())
                   + ", parameter writes: " + juce::String(parameterWrites.load())
                   + ", mode writes: " + juce::String(modeWrites.load()));
        expect(blocksProcessed.load() >= minAudioBlocks,
               "audio thread completed the fixed minimum workload");
        expect(overlappingWrites.load() >= minOverlappingWrites,
               "parameter writers actually overlapped processBlock");
        expect(parameterWrites.load() > 0 && modeWrites.load() > 0,
               "all parameter-storm writers actually ran");
        expect(!nonFinite.load(), "storm produced non-finite output samples");
    }

private:
    static constexpr int blockSize = 512;
};

static ParameterStormThreadSafetyTest parameterStormThreadSafetyTest;
