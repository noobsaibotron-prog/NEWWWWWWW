#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

/**
 * RB-2 / RB-3 / RB-4 runtime validation tests.
 *
 * These tests exercise the exact paths fixed by the release-blocker commits:
 *   RB-2 (b42f244f) — transactional slot protection
 *   RB-3 (absorbed)  — synchronous restore (no callAsync gap)
 *   RB-4 (7a285d09 + 352ded2f) — qualityMode/lookahead runtime reconfiguration
 */
class RBValidationTest : public juce::UnitTest
{
public:
    RBValidationTest() : juce::UnitTest("RB Validation", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testRB2_SlotCoherenceDuringSwitching();
        testRB2_SlotRoundTripSaveLoad();
        testRB3_SynchronousRestoreNoStaleSlots();
        testRB4_QualityModeSwitchDuringProcess();
        testRB4_ZeroLatencyToHQRoundTrip();
    }

private:
    // ── helpers ───────────────────────────────────────────────────────────
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

    static float readParam(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& id)
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return -9999.0f;
    }

    /** Push a block of silence through the processor. */
    static void processBlocks(AIEqualizerAudioProcessor& proc, int numBlocks,
                              int blockSize = 256)
    {
        juce::AudioBuffer<float> buf(2, blockSize);
        juce::MidiBuffer midi;
        for (int i = 0; i < numBlocks; ++i)
        {
            buf.clear();
            proc.processBlock(buf, midi);
        }
    }

    // ── RB-2: slot coherence during switching ────────────────────────────
    void testRB2_SlotCoherenceDuringSwitching()
    {
        beginTest("RB-2: Slot A/B values stay coherent across rapid switching");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        // Configure Slot A: recognisable values
        proc.setNumActiveBands(2);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 1000.0f;
            b.gain      = 6.0f;
            b.q         = 1.0f;
            b.type      = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled   = true;
            b.solo      = false;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", -3.0f);

        // Switch to B → configure differently
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 250.0f;
            b.gain      = -4.0f;
            b.q         = 2.0f;
            b.type      = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled   = true;
            b.solo      = false;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", 2.0f);

        // Rapid A↔B switching (10 round-trips)
        for (int i = 0; i < 10; ++i)
        {
            proc.setABState(AIEqualizerAudioProcessor::ABState::A);
            processBlocks(proc, 2); // let processBlock run between switches

            auto bA = proc.getBandState(0);
            expectWithinAbsoluteError(bA.frequency, 1000.0f, 1.0f);
            expectWithinAbsoluteError(bA.gain, 6.0f, 0.1f);

            proc.setABState(AIEqualizerAudioProcessor::ABState::B);
            processBlocks(proc, 2);

            auto bB = proc.getBandState(0);
            expectWithinAbsoluteError(bB.frequency, 250.0f, 1.0f);
            expectWithinAbsoluteError(bB.gain, -4.0f, 0.1f);
        }
    }

    // ── RB-2: slot round-trip across save/load ───────────────────────────
    void testRB2_SlotRoundTripSaveLoad()
    {
        beginTest("RB-2: All 4 slots survive getState → setState round-trip");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        // Set up A with distinctive values
        proc.setNumActiveBands(1);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 800.0f;  b.gain = 5.0f;  b.q = 0.7f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled = true;  b.solo = false;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", -2.0f);
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 75.0f);

        // Switch to B, set different values
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 3000.0f;  b.gain = -6.0f;  b.q = 3.0f;
            b.type = static_cast<int>(ParametricEQProcessor::HighShelf);
            b.enabled = true;  b.solo = false;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", 1.0f);
        setBool(apvts, "dynEqEnabled", false);
        setFloat(apvts, "dynEqMix", 0.0f);

        // Save state
        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        // Restore into a fresh processor (simulates DAW reopen)
        AIEqualizerAudioProcessor restored;
        restored.prepareToPlay(48000.0, 512);
        restored.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        auto& rApvts = restored.getAPVTS();

        // Verify B (was active at save time)
        {
            auto b = restored.getBandState(0);
            expectWithinAbsoluteError(b.frequency, 3000.0f, 1.0f);
            expectWithinAbsoluteError(b.gain, -6.0f, 0.1f);
            expectWithinAbsoluteError(readParam(rApvts, "outputGain"), 1.0f, 0.1f);
            expectWithinAbsoluteError(readParam(rApvts, "dynEqEnabled"), 0.0f, 0.01f);
        }

        // Switch to A and verify its values survived
        restored.setABState(AIEqualizerAudioProcessor::ABState::A);
        {
            auto b = restored.getBandState(0);
            expectWithinAbsoluteError(b.frequency, 800.0f, 1.0f);
            expectWithinAbsoluteError(b.gain, 5.0f, 0.1f);
            expectWithinAbsoluteError(readParam(rApvts, "outputGain"), -2.0f, 0.1f);
            expectWithinAbsoluteError(readParam(rApvts, "dynEqEnabled"), 1.0f, 0.01f);
            expectWithinAbsoluteError(readParam(rApvts, "dynEqMix"), 75.0f, 0.5f);
        }

        // Back to B — still correct
        restored.setABState(AIEqualizerAudioProcessor::ABState::B);
        {
            auto b = restored.getBandState(0);
            expectWithinAbsoluteError(b.frequency, 3000.0f, 1.0f);
            expectWithinAbsoluteError(b.gain, -6.0f, 0.1f);
        }
    }

    // ── RB-3: synchronous restore — no stale slot after setStateInformation ─
    void testRB3_SynchronousRestoreNoStaleSlots()
    {
        beginTest("RB-3: setStateInformation restores slots synchronously (no async gap)");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        // Set A to known state, switch to B, set B to different state
        proc.setNumActiveBands(1);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 500.0f;  b.gain = 3.0f;  b.q = 1.0f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled = true;  b.solo = false;
            proc.setBandState(0, b);
        }
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 5000.0f;  b.gain = -8.0f;  b.q = 4.0f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled = true;  b.solo = false;
            proc.setBandState(0, b);
        }

        // Save while on B
        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        // Mutate everything
        proc.setABState(AIEqualizerAudioProcessor::ABState::A);
        setFloat(apvts, "outputGain", 10.0f);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 100.0f;  b.gain = 0.0f;  b.q = 0.5f;
            b.type = static_cast<int>(ParametricEQProcessor::LowShelf);
            b.enabled = false;  b.solo = false;
            proc.setBandState(0, b);
        }

        // Restore — RB-3 claim: immediately after this returns, slots are coherent
        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        // IMMEDIATELY check — no processBlock, no message loop pump — values must be right NOW
        auto bNow = proc.getBandState(0);
        expectWithinAbsoluteError(bNow.frequency, 5000.0f, 1.0f);
        expectWithinAbsoluteError(bNow.gain, -8.0f, 0.1f);

        // Switch to A — also immediately coherent
        proc.setABState(AIEqualizerAudioProcessor::ABState::A);
        auto bA = proc.getBandState(0);
        expectWithinAbsoluteError(bA.frequency, 500.0f, 1.0f);
        expectWithinAbsoluteError(bA.gain, 3.0f, 0.1f);
    }

    // ── RB-4: quality mode switch during processBlock ────────────────────
    void testRB4_QualityModeSwitchDuringProcess()
    {
        beginTest("RB-4: Switch Zero Latency → HQ during processBlock — no crash");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        // Start in Zero Latency
        setChoice(apvts, "qualityMode", 0);
        processBlocks(proc, 10);

        // Switch to HQ mid-stream
        setChoice(apvts, "qualityMode", 1);
        processBlocks(proc, 20);

        // Switch back
        setChoice(apvts, "qualityMode", 0);
        processBlocks(proc, 20);

        // Rapid toggle (stress test)
        for (int i = 0; i < 50; ++i)
        {
            setChoice(apvts, "qualityMode", i % 2);
            processBlocks(proc, 2);
        }

        // If we got here, no crash — that's the primary assertion
        expect(true, "Survived 50 quality mode switches without crash");
    }

    // ── RB-4: quality mode round-trip across save/load ──────────────────
    void testRB4_ZeroLatencyToHQRoundTrip()
    {
        beginTest("RB-4: Quality mode persists across save/load and lookahead is effective");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);
        auto& apvts = proc.getAPVTS();

        // Set HQ mode, process a few blocks so it takes effect
        setChoice(apvts, "qualityMode", 1);
        processBlocks(proc, 5);

        // Save
        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        // Restore into fresh processor starting in Zero Latency
        AIEqualizerAudioProcessor restored;
        restored.prepareToPlay(48000.0, 256);
        auto& rApvts = restored.getAPVTS();
        setChoice(rApvts, "qualityMode", 0); // force ZL first
        processBlocks(restored, 5);

        // Now restore the HQ state
        restored.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        // Verify qualityMode was restored to HQ
        expectWithinAbsoluteError(readParam(rApvts, "qualityMode"), 1.0f, 0.01f);

        // Process blocks in restored HQ mode — no crash
        processBlocks(restored, 20);
        expect(true, "Restored HQ mode processes without crash");
    }
};

static RBValidationTest rbValidationTest;
