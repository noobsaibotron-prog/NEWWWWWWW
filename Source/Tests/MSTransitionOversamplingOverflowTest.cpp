#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

/**
 * QUARANTINED deterministic reproducer — SINGLE-THREADED, no concurrency at all.
 *
 * Discriminant experiment for the pluginval "Parameter thread safety" crash: the
 * param-storm detector (ParameterStormThreadSafetyTest) pinned a heap-buffer-overflow in
 * Oversampling2TimesPolyphaseIIR::processSamplesUp (juce_Oversampling.cpp:355) reached
 * from processBlock's msMode-transition crossfade (PluginProcessor.cpp:2388). Static
 * reading predicts the bug is SPATIAL, not a data race:
 *
 *   preallocatedMaxSamples = jmax(samplesPerBlock * 4, 32768)          (:863)
 *   msModeTransitionBuffer.setSize(ch, preallocatedMaxSamples)         (:886)  -> 32768
 *   oversampler2x/4x->initProcessing(samplesPerBlock)                  (:1031) -> sized 512
 *   crossfade Case A (old mode not MSLinked, phase != LinearPhase):
 *     processStereoForPhaseMode(msModeTransitionBuffer, mode, false)   (:2388)
 *       -> processNaturalStereo builds its AudioBlock with
 *          targetBuffer.getNumSamples() == 32768, NOT this block's blockSamples (:1859)
 *       -> processSamplesUp writes 2x32768 samples into a stage buffer sized 2x512
 *          == heap-buffer-overflow at exactly "0 bytes after" the stage region.
 *
 * So a SEQUENTIAL msMode flip (Stereo -> MSLinked) between two blocks, with
 * NaturalPhase + oversampling active, should reproduce the overflow with zero threads.
 * If this test crashes under ASan -> the root cause is deterministic buffer-size plumbing
 * (fix: pass blockSamples-limited views to the transition processing paths).
 * If it survives while the storm test still crashes -> concurrency is genuinely required
 * and the diagnosis must continue on the lifecycle/race track.
 *
 * EXPECTED RED under ASan while the bug is live. Quarantined: compiled only into
 * AIEqualizerPro_ThreadSafetyTests (EXCLUDE_FROM_ALL, no ctest). After the fix this
 * becomes the regression test and graduates to a blocking gate.
 */
class MSTransitionOversamplingOverflowTest : public juce::UnitTest
{
public:
    MSTransitionOversamplingOverflowTest()
        : juce::UnitTest("MS Transition Oversampling Overflow", "ThreadSafety") {}

    void runTest() override
    {
        beginTest("single-threaded msMode flip must not overflow the oversampler stage");

        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(44100.0, blockSize);
        auto& apvts = proc.getAPVTS();

        // NaturalPhase (the oversampled path) + explicit 2x oversampling.
        setChoice(apvts, "phaseMode",
                  static_cast<int>(AIEqualizerAudioProcessor::PhaseMode::NaturalPhase));
        setChoice(apvts, "oversamplingFactor", 1); // 2x

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        juce::Random rng(0x0D15C0);

        // Drain the phase-mode transition crossfade so NaturalPhase is fully active.
        processBlocks(proc, buffer, midi, rng, 40);

        // Arm the msMode crossfade: Stereo -> MSLinked (Case A, old side not linked,
        // which is the processStereoForPhaseMode(msModeTransitionBuffer, ...) branch).
        setChoice(apvts, "msMode",
                  static_cast<int>(AIEqualizerAudioProcessor::MSMode::MSLinked));

        // The very next blocks run the transition path. Under ASan the overflow (if
        // spatial/deterministic) fires here — no concurrency involved.
        processBlocks(proc, buffer, midi, rng, 8);

        proc.releaseResources();
        expect(true, "survived: overflow did NOT reproduce single-threaded "
                     "(diagnosis must continue on the concurrency track)");
    }

private:
    static constexpr int blockSize = 512;

    static void setChoice(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& id, int index)
    {
        auto* p = apvts.getParameter(id);
        if (p != nullptr)
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
    }

    static void processBlocks(AIEqualizerAudioProcessor& proc,
                              juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midi,
                              juce::Random& rng,
                              int count)
    {
        for (int b = 0; b < count; ++b)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* d = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    d[i] = rng.nextFloat() * 0.5f - 0.25f;
            }
            proc.processBlock(buffer, midi);
        }
    }
};

static MSTransitionOversamplingOverflowTest msTransitionOversamplingOverflowTest;
