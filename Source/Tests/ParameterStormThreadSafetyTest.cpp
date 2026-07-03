#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>
#include "../PluginProcessor.h"

/**
 * QUARANTINED thread-safety DETECTOR — in-process reproducer of pluginval 1.0.4
 * "Parameter thread safety" (see REPORTS/KNOWN_ISSUE_pluginval_s10_param_thread_safety.md).
 *
 * pluginval crashes seed-dependently (EXC_BAD_ACCESS in the VST3 wrapper's
 * ClientRemappedBuffer teardown — a downstream-corruption signature; reproducer seed
 * 0x782104d at strictness 8). Static analysis found parameterChanged() flags-only, so the
 * corrupting write is still unidentified. This test recreates the same workload inside the
 * product harness so the sanitizers can name the culprit at the faulting access instead of
 * a downstream detonation: run it under TSan (names the racing write, both stacks) and
 * ASan (catches the OOB variant) via build_sanitize.sh.
 *
 * Workload (models pluginval's test semantics):
 *  - one "audio" thread hammering processBlock()  (pluginval also drives it off-message)
 *  - two writer threads hammering setValueNotifyingHost() across ALL parameters —
 *    listeners fire on the CALLING thread, same delivery as the VST3 controller path
 *  - one writer dedicated to the transition-machinery params (phaseMode / msMode /
 *    oversamplingFactor / qualityMode), which arm crossfades, pendingReset and IR rebuilds
 *
 * EXPECTED RED (crash or sanitizer report) while the underlying race is unfixed. Compiled
 * ONLY into AIEqualizerPro_ThreadSafetyTests (EXCLUDE_FROM_ALL, no ctest entry); must NOT
 * join blocking gates until the fix lands, then it graduates. Coverage gap (documented):
 * the VST3 wrapper layer itself is not exercised here — the end-to-end judge remains
 * pluginval with the pinned seed.
 *
 * AIEQ_STORM_MS overrides the storm duration (default 2000 ms) for longer hunts.
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

        std::atomic<bool> stop { false };
        std::atomic<bool> nonFinite { false };
        std::atomic<int>  blocksProcessed { 0 };

        std::thread audioThread([&]
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            juce::Random rng(0x51027A0D); // fixed seed: deterministic input signal
            while (!stop.load(std::memory_order_relaxed))
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* d = buffer.getWritePointer(ch);
                    for (int i = 0; i < blockSize; ++i)
                        d[i] = rng.nextFloat() * 0.5f - 0.25f;
                }

                proc.processBlock(buffer, midi);
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
            }
        });

        auto writerFn = [&](juce::int64 seed)
        {
            juce::Random rng(seed);
            while (!stop.load(std::memory_order_relaxed))
            {
                auto* p = params[rng.nextInt(params.size())];
                p->setValueNotifyingHost(rng.nextFloat());
            }
        };
        std::thread writer1(writerFn, (juce::int64) 0xA1EC0FFEE);
        std::thread writer2(writerFn, (juce::int64) 0xB2D15EA5E);

        std::thread modeWriter([&]
        {
            juce::Random rng(0xC3A11D0);
            while (!stop.load(std::memory_order_relaxed) && !modeParams.empty())
            {
                auto* p = modeParams[(size_t) rng.nextInt((int) modeParams.size())];
                p->setValueNotifyingHost(rng.nextFloat());
                std::this_thread::yield();
            }
        });

        const int stormMs = juce::SystemStats::getEnvironmentVariable("AIEQ_STORM_MS", "2000").getIntValue();
        juce::Thread::sleep(juce::jmax(100, stormMs));
        stop.store(true, std::memory_order_relaxed);

        audioThread.join();
        writer1.join();
        writer2.join();
        modeWriter.join();

        proc.releaseResources();

        logMessage("blocks processed during storm: " + juce::String(blocksProcessed.load()));
        expect(blocksProcessed.load() > 10, "audio thread actually ran during the storm");
        expect(!nonFinite.load(), "storm produced non-finite output samples");
    }

private:
    static constexpr int blockSize = 512;
};

static ParameterStormThreadSafetyTest parameterStormThreadSafetyTest;
