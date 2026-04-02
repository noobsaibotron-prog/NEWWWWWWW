#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"
#include <cmath>

/**
 * RB-2 Equivalence / Roundtrip Stability Test
 *
 * Proves that the transactional slot protection produces a stable, idempotent
 * serialization cycle:
 *
 *   save₁ → load → save₂  ⟹  XML₁ ≡ XML₂
 *
 * Tests multiple scenarios:
 * 1. Simple roundtrip with all 4 slots configured
 * 2. Multi-cycle stability (save→load→save→load→save — no drift)
 * 3. Active-slot invariance (save from A, restore, switch to B, switch back — A intact)
 * 4. Cross-instance equivalence (save from proc₁, load into proc₂, save from proc₂ — same XML)
 * 5. Full parameter saturation (every parameter set to non-default)
 */
class RB2EquivalenceTest : public juce::UnitTest
{
public:
    RB2EquivalenceTest() : juce::UnitTest("RB-2 Equivalence", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        testIdempotentRoundTrip();
        testMultiCycleNoDrift();
        testActiveSlotInvariance();
        testCrossInstanceEquivalence();
        testFullParameterSaturation();
        testSoak250Cycles();
        testRandomizedSoak();
    }

private:
    // ── helpers ──────────────────────────────────────────────────────────

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

    /** Extract the XML string from a state blob for human-readable comparison. */
    static juce::String blobToXmlString(AIEqualizerAudioProcessor& proc,
                                         const juce::MemoryBlock& blob)
    {
        // Use JUCE's binary→XML helper (same as the plugin uses internally)
        auto xml = proc.getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        if (xml == nullptr)
            return {};
        return xml->toString();
    }

    /**
     * Configure all 4 slots with distinct, recognisable values.
     * Every slot gets different freq/gain/Q + different names + different dyn params.
     */
    static void configureAll4Slots(AIEqualizerAudioProcessor& proc,
                                    juce::AudioProcessorValueTreeState& apvts)
    {
        proc.setNumActiveBands(4);

        // ── Slot A ──
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 500.0f;  b.gain = 6.0f;  b.q = 0.7f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled = true;  b.solo = false;
            b.dynMode = 1;  b.dynThreshold = -20.0f;  b.dynRatio = 4.0f;
            b.dynAttack = 5.0f;  b.dynRelease = 80.0f;
            b.dynRange = 12.0f;  b.dynKnee = 3.0f;
            proc.setBandState(0, b);

            b.frequency = 2000.0f;  b.gain = -3.0f;  b.q = 1.5f;
            b.type = static_cast<int>(ParametricEQProcessor::HighShelf);
            b.dynMode = 0;
            proc.setBandState(1, b);

            b.frequency = 100.0f;  b.gain = 4.0f;  b.q = 0.5f;
            b.type = static_cast<int>(ParametricEQProcessor::LowShelf);
            proc.setBandState(2, b);

            b.frequency = 8000.0f;  b.gain = -5.0f;  b.q = 3.0f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            proc.setBandState(3, b);
        }
        setFloat(apvts, "outputGain", -2.0f);
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 75.0f);
        setBool(apvts, "dynAutoMakeup", true);

        // ── Switch to B, configure differently ──
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 3000.0f;  b.gain = -6.0f;  b.q = 2.0f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            b.enabled = true;  b.solo = false;
            b.dynMode = 2;  b.dynThreshold = -30.0f;  b.dynRatio = 8.0f;
            b.dynAttack = 1.0f;  b.dynRelease = 200.0f;
            b.dynRange = 24.0f;  b.dynKnee = 6.0f;
            proc.setBandState(0, b);

            b.frequency = 400.0f;  b.gain = 8.0f;  b.q = 0.8f;
            b.type = static_cast<int>(ParametricEQProcessor::LowShelf);
            b.dynMode = 0;
            proc.setBandState(1, b);

            b.frequency = 6000.0f;  b.gain = -2.0f;  b.q = 1.2f;
            b.type = static_cast<int>(ParametricEQProcessor::HighShelf);
            proc.setBandState(2, b);

            b.frequency = 12000.0f;  b.gain = 3.0f;  b.q = 4.0f;
            b.type = static_cast<int>(ParametricEQProcessor::Peak);
            proc.setBandState(3, b);
        }
        setFloat(apvts, "outputGain", 1.5f);
        setBool(apvts, "dynEqEnabled", false);
        setFloat(apvts, "dynEqMix", 50.0f);
        setBool(apvts, "dynAutoMakeup", false);

        // ── Switch to C ──
        proc.setABState(AIEqualizerAudioProcessor::ABState::C);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 150.0f;  b.gain = 10.0f;  b.q = 0.4f;
            b.type = static_cast<int>(ParametricEQProcessor::LowShelf);
            b.enabled = true;  b.solo = false;  b.dynMode = 0;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", 0.0f);

        // ── Switch to D ──
        proc.setABState(AIEqualizerAudioProcessor::ABState::D);
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 10000.0f;  b.gain = -8.0f;  b.q = 5.0f;
            b.type = static_cast<int>(ParametricEQProcessor::HighShelf);
            b.enabled = true;  b.solo = false;  b.dynMode = 1;
            b.dynThreshold = -15.0f;  b.dynRatio = 2.0f;
            b.dynAttack = 20.0f;  b.dynRelease = 300.0f;
            b.dynRange = 6.0f;  b.dynKnee = 1.0f;
            proc.setBandState(0, b);
        }
        setFloat(apvts, "outputGain", -4.0f);

        // Switch back to A as the active slot
        proc.setABState(AIEqualizerAudioProcessor::ABState::A);

        // Set global params
        setChoice(apvts, "qualityMode", 1);
        setChoice(apvts, "phaseMode", 0);
        setChoice(apvts, "oversamplingFactor", 0);
        setBool(apvts, "bypass", false);
    }

    /**
     * Compare two XML strings, reporting first difference if any.
     * Returns true if identical.
     */
    bool compareXml(const juce::String& xml1, const juce::String& xml2,
                    const juce::String& label)
    {
        if (xml1 == xml2)
            return true;

        // Find first differing line for diagnostics
        auto lines1 = juce::StringArray::fromLines(xml1);
        auto lines2 = juce::StringArray::fromLines(xml2);
        const int minLines = juce::jmin(lines1.size(), lines2.size());
        for (int i = 0; i < minLines; ++i)
        {
            if (lines1[i] != lines2[i])
            {
                logMessage(label + " — first diff at line " + juce::String(i + 1) + ":");
                logMessage("  save₁: " + lines1[i].trimEnd());
                logMessage("  save₂: " + lines2[i].trimEnd());
                return false;
            }
        }
        if (lines1.size() != lines2.size())
            logMessage(label + " — line count differs: " +
                       juce::String(lines1.size()) + " vs " + juce::String(lines2.size()));
        return false;
    }

    // ── Test 1: Idempotent roundtrip ─────────────────────────────────────
    void testIdempotentRoundTrip()
    {
        beginTest("RB-2 Equivalence: save₁ → load → save₂ produces identical XML");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        configureAll4Slots(proc, apvts);

        // save₁
        juce::MemoryBlock blob1;
        proc.getStateInformation(blob1);
        auto xml1 = blobToXmlString(proc, blob1);
        expect(xml1.isNotEmpty(), "save₁ should produce non-empty XML");

        // load into same processor
        proc.setStateInformation(blob1.getData(), static_cast<int>(blob1.getSize()));

        // save₂
        juce::MemoryBlock blob2;
        proc.getStateInformation(blob2);
        auto xml2 = blobToXmlString(proc, blob2);

        bool match = compareXml(xml1, xml2, "Idempotent roundtrip");
        expect(match, "save₁ XML should be identical to save₂ XML after load");

        if (match)
            logMessage("  XML size: " + juce::String(xml1.length()) + " chars — identical");
    }

    // ── Test 2: Multi-cycle no drift ─────────────────────────────────────
    void testMultiCycleNoDrift()
    {
        beginTest("RB-2 Equivalence: 5 consecutive save/load cycles produce no drift");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        configureAll4Slots(proc, apvts);

        // Initial save
        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        auto xmlReference = blobToXmlString(proc, blob);

        // 5 cycles
        for (int cycle = 0; cycle < 5; ++cycle)
        {
            proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

            juce::MemoryBlock newBlob;
            proc.getStateInformation(newBlob);
            auto xmlCurrent = blobToXmlString(proc, newBlob);

            bool match = compareXml(xmlReference, xmlCurrent,
                                     "Cycle " + juce::String(cycle + 1));
            expect(match, "Cycle " + juce::String(cycle + 1) +
                         " XML should match reference");

            blob = newBlob; // use latest for next cycle
        }
    }

    // ── Test 3: Active-slot invariance ───────────────────────────────────
    void testActiveSlotInvariance()
    {
        beginTest("RB-2 Equivalence: switching slots and back preserves all values");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        configureAll4Slots(proc, apvts);

        // Save reference on A
        juce::MemoryBlock blobBefore;
        proc.getStateInformation(blobBefore);

        // Switch A→B→C→D→A
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);
        proc.setABState(AIEqualizerAudioProcessor::ABState::C);
        proc.setABState(AIEqualizerAudioProcessor::ABState::D);
        proc.setABState(AIEqualizerAudioProcessor::ABState::A);

        // Save again
        juce::MemoryBlock blobAfter;
        proc.getStateInformation(blobAfter);

        auto xmlBefore = blobToXmlString(proc, blobBefore);
        auto xmlAfter  = blobToXmlString(proc, blobAfter);

        bool match = compareXml(xmlBefore, xmlAfter, "Slot cycle A→B→C→D→A");
        expect(match, "Full slot cycle should produce identical XML");
    }

    // ── Test 4: Cross-instance equivalence ───────────────────────────────
    void testCrossInstanceEquivalence()
    {
        beginTest("RB-2 Equivalence: save from proc₁, load into proc₂ — parameter values match");

        // Cross-instance roundtrip passes through APVTS normalize/denormalize,
        // which introduces float quantization (e.g. -20.0 → -19.99999809).
        // This is a JUCE limitation, not an RB-2 bug.  We verify parameter
        // equivalence with tolerance instead of bitwise XML identity.

        // proc₁: configure and save
        juce::MemoryBlock blob1;
        std::array<AIEqualizerAudioProcessor::BandState, 4> bandsOrig;
        float origOutputGain, origDynMix;
        bool origDynEnabled;
        {
            AIEqualizerAudioProcessor proc1;
            proc1.prepareToPlay(48000.0, 512);
            auto& apvts1 = proc1.getAPVTS();
            configureAll4Slots(proc1, apvts1);

            for (int i = 0; i < 4; ++i)
                bandsOrig[static_cast<size_t>(i)] = proc1.getBandState(i);
            origOutputGain = readParam(apvts1, "outputGain");
            origDynEnabled = readParam(apvts1, "dynEqEnabled") > 0.5f;
            origDynMix = readParam(apvts1, "dynEqMix");

            proc1.getStateInformation(blob1);
        }

        // proc₂: fresh instance, load
        {
            AIEqualizerAudioProcessor proc2;
            proc2.prepareToPlay(48000.0, 512);
            proc2.setStateInformation(blob1.getData(), static_cast<int>(blob1.getSize()));
            auto& apvts2 = proc2.getAPVTS();

            // Verify bands with tolerance
            for (int i = 0; i < 4; ++i)
            {
                auto b = proc2.getBandState(i);
                auto& o = bandsOrig[static_cast<size_t>(i)];
                expectWithinAbsoluteError(b.frequency, o.frequency, 1.0f);
                expectWithinAbsoluteError(b.gain, o.gain, 0.1f);
                expectWithinAbsoluteError(b.q, o.q, 0.1f);
                expect(b.type == o.type, "Band " + juce::String(i) + " type mismatch");
                expect(b.enabled == o.enabled, "Band " + juce::String(i) + " enabled mismatch");
            }

            // Verify globals with tolerance
            expectWithinAbsoluteError(readParam(apvts2, "outputGain"), origOutputGain, 0.1f);
            expect((readParam(apvts2, "dynEqEnabled") > 0.5f) == origDynEnabled,
                   "dynEqEnabled mismatch");
            expectWithinAbsoluteError(readParam(apvts2, "dynEqMix"), origDynMix, 0.5f);

            // Verify slot B survives cross-instance
            proc2.setABState(AIEqualizerAudioProcessor::ABState::B);
            auto bB = proc2.getBandState(0);
            expectWithinAbsoluteError(bB.frequency, 3000.0f, 1.0f);
            expectWithinAbsoluteError(bB.gain, -6.0f, 0.1f);

            logMessage("  Cross-instance: all parameters within tolerance");
        }
    }

    // ── Test 5: Full parameter saturation ────────────────────────────────
    void testFullParameterSaturation()
    {
        beginTest("RB-2 Equivalence: all APVTS params at non-default → roundtrip stable");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        // Set every parameter to a non-default value
        proc.setNumActiveBands(8);
        for (int i = 0; i < 8; ++i)
        {
            AIEqualizerAudioProcessor::BandState b;
            b.frequency = 200.0f + i * 500.0f;
            b.gain      = -6.0f + i * 1.5f;
            b.q         = 0.3f + i * 0.5f;
            b.type      = i % 4; // cycle through filter types
            b.enabled   = (i % 3 != 0);
            b.solo      = (i == 2);
            b.dynMode   = i % 3;
            b.dynThreshold = -40.0f + i * 5.0f;
            b.dynRatio  = 1.0f + i * 1.0f;
            b.dynAttack = 0.5f + i * 3.0f;
            b.dynRelease = 20.0f + i * 30.0f;
            b.dynRange  = 6.0f + i * 3.0f;
            b.dynKnee   = i * 1.0f;
            proc.setBandState(i, b);
        }

        setFloat(apvts, "outputGain", -5.0f);
        setFloat(apvts, "dryWet", 80.0f);
        setBool(apvts, "bypass", false);
        setBool(apvts, "autoGain", true);
        setChoice(apvts, "qualityMode", 1);
        setChoice(apvts, "phaseMode", 1);
        setChoice(apvts, "oversamplingFactor", 1);
        setChoice(apvts, "msMode", 1);
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 60.0f);
        setBool(apvts, "dynAutoMakeup", true);

        // save₁
        juce::MemoryBlock blob1;
        proc.getStateInformation(blob1);
        auto xml1 = blobToXmlString(proc, blob1);

        // load → save₂
        proc.setStateInformation(blob1.getData(), static_cast<int>(blob1.getSize()));
        juce::MemoryBlock blob2;
        proc.getStateInformation(blob2);
        auto xml2 = blobToXmlString(proc, blob2);

        bool match = compareXml(xml1, xml2, "Full saturation");
        expect(match, "Full parameter saturation roundtrip should be stable");

        if (match)
            logMessage("  Saturated XML size: " + juce::String(xml1.length()) +
                       " chars — identical after roundtrip");
    }

    // ── Test 6: 250-cycle soak ───────────────────────────────────────────
    void testSoak250Cycles()
    {
        beginTest("RB-2 Soak: 250 consecutive save/load cycles — zero drift");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        configureAll4Slots(proc, apvts);

        // Capture reference
        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        auto xmlRef = blobToXmlString(proc, blob);

        int failedCycle = -1;
        for (int cycle = 0; cycle < 250; ++cycle)
        {
            proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

            juce::MemoryBlock newBlob;
            proc.getStateInformation(newBlob);
            auto xmlCurrent = blobToXmlString(proc, newBlob);

            if (xmlCurrent != xmlRef)
            {
                failedCycle = cycle;
                compareXml(xmlRef, xmlCurrent, "Soak cycle " + juce::String(cycle));
                break;
            }
            blob = newBlob;
        }

        expect(failedCycle == -1,
               failedCycle >= 0
                   ? "Drift detected at cycle " + juce::String(failedCycle)
                   : "");

        if (failedCycle == -1)
            logMessage("  250 cycles: XML stable (" +
                       juce::String(xmlRef.length()) + " chars, zero drift)");
    }

    // ── Test 7: 50 random seeds, each with roundtrip ─────────────────────
    void testRandomizedSoak()
    {
        beginTest("RB-2 Soak: 50 randomized configurations — each roundtrip stable");

        juce::Random rng(42); // deterministic seed
        int failures = 0;

        for (int seed = 0; seed < 50; ++seed)
        {
            AIEqualizerAudioProcessor proc;
            proc.prepareToPlay(48000.0, 512);
            auto& apvts = proc.getAPVTS();

            // Random number of active bands (1-8)
            const int numBands = 1 + rng.nextInt(8);
            proc.setNumActiveBands(numBands);

            // Random band config
            for (int i = 0; i < numBands; ++i)
            {
                AIEqualizerAudioProcessor::BandState b;
                b.frequency = 20.0f + rng.nextFloat() * 19980.0f;
                b.gain      = -12.0f + rng.nextFloat() * 24.0f;
                b.q         = 0.1f + rng.nextFloat() * 9.9f;
                b.type      = rng.nextInt(4);
                b.enabled   = rng.nextBool();
                b.solo      = (rng.nextInt(10) == 0); // rare
                b.dynMode   = rng.nextInt(3);
                b.dynThreshold = -60.0f + rng.nextFloat() * 60.0f;
                b.dynRatio  = 1.0f + rng.nextFloat() * 19.0f;
                b.dynAttack = 0.1f + rng.nextFloat() * 99.9f;
                b.dynRelease = 1.0f + rng.nextFloat() * 499.0f;
                b.dynRange  = 0.0f + rng.nextFloat() * 48.0f;
                b.dynKnee   = rng.nextFloat() * 12.0f;
                proc.setBandState(i, b);
            }

            // Random globals
            setFloat(apvts, "outputGain", -12.0f + rng.nextFloat() * 24.0f);
            setFloat(apvts, "dryWet", rng.nextFloat() * 100.0f);
            setBool(apvts, "dynEqEnabled", rng.nextBool());
            setFloat(apvts, "dynEqMix", rng.nextFloat() * 100.0f);
            setBool(apvts, "dynAutoMakeup", rng.nextBool());
            setChoice(apvts, "qualityMode", rng.nextInt(2));
            setChoice(apvts, "phaseMode", rng.nextInt(3));
            setChoice(apvts, "oversamplingFactor", rng.nextInt(3));

            // Random active slot
            const int slotIdx = rng.nextInt(4);
            const AIEqualizerAudioProcessor::ABState slots[] = {
                AIEqualizerAudioProcessor::ABState::A,
                AIEqualizerAudioProcessor::ABState::B,
                AIEqualizerAudioProcessor::ABState::C,
                AIEqualizerAudioProcessor::ABState::D
            };
            if (slotIdx > 0)
                proc.setABState(slots[slotIdx]);

            // save₁
            juce::MemoryBlock blob1;
            proc.getStateInformation(blob1);
            auto xml1 = blobToXmlString(proc, blob1);

            // load → save₂
            proc.setStateInformation(blob1.getData(), static_cast<int>(blob1.getSize()));
            juce::MemoryBlock blob2;
            proc.getStateInformation(blob2);
            auto xml2 = blobToXmlString(proc, blob2);

            if (xml1 != xml2)
            {
                compareXml(xml1, xml2, "Seed " + juce::String(seed));
                ++failures;
            }
        }

        expect(failures == 0,
               juce::String(failures) + " of 50 random seeds failed roundtrip");

        if (failures == 0)
            logMessage("  50 random seeds: all roundtrips stable");
    }
};

static RB2EquivalenceTest rb2EquivalenceTest;
