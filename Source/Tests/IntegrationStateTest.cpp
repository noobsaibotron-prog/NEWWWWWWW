#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

/**
 * Integration-style checks on plugin state, preset round-trip, bypass and oversampling/latency reporting.
 * These tests run in Debug and link against the plugin shared code.
 */
class IntegrationStateTest : public juce::UnitTest
{
public:
    IntegrationStateTest() : juce::UnitTest("AIEqualizer Integration", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm); // ensure MessageManager exists; current thread becomes message thread

        testStateRoundTrip();
        testPresetSchemaRoundTrip();
        testDynamicABStateRoundTrip();
        testHostParameterSurfaceGoldenList();
        testBypassPassThrough();
    }

private:
    struct ExpectedParameter
    {
        juce::String id;
        int versionHint = 1;
    };

    static std::vector<ExpectedParameter> expectedHostParameters()
    {
        std::vector<ExpectedParameter> ids;

        const char* const globals[] = {
            "outputGain",
            "dryWet",
            "bypass",
            "autoGain",
            "qualityMode",
            "phaseMode",
            "msMode",
            "oversamplingFactor",
            "aiSensitivity",
            "aiStrength",
            "aiEnabled",
            "sourceProfile",
            "showPreSpectrum",
            "showPostSpectrum",
            "showDeltaSpectrum",
            "analyzerResolution",
            "analyzerSpeed",
            "analyzerSlope",
            "showPeakHold",
            "analyzerPeakHold",
            "analyzerPeakDecay",
            "spectrumTilt",
            "pianoRollOverlay",
            "highContrastMode",
            "learningEnabled",
            "numActiveBands",
        };

        for (const auto* id : globals)
            ids.push_back({ id, 1 });

        const char* const bandSuffixes[] = {
            "Freq",
            "Gain",
            "Q",
            "Type",
            "Enabled",
            "Solo",
            "Slope",
            "DynMode",
            "Threshold",
            "Ratio",
            "Attack",
            "Release",
            "Range",
            "Knee",
        };

        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            for (const auto* suffix : bandSuffixes)
                ids.push_back({ "band" + juce::String(band) + suffix, 1 });

        ids.push_back({ "dynEqEnabled", 1 });
        ids.push_back({ "dynEqMix", 1 });
        ids.push_back({ "dynAutoMakeup", 1 });
        ids.push_back({ "dynamicCorrections", 2 });

        return ids;
    }

    void testHostParameterSurfaceGoldenList()
    {
        beginTest("Host parameter IDs and version hints remain append-only");

        AIEqualizerAudioProcessor proc;
        const auto expected = expectedHostParameters();
        const auto& params = proc.getParameters();

        expect(static_cast<size_t>(params.size()) == expected.size(),
               "Host parameter count changed: expected " + juce::String(static_cast<int>(expected.size()))
               + ", got " + juce::String(params.size()));

        const auto count = std::min(static_cast<size_t>(params.size()), expected.size());
        juce::StringArray seen;

        for (size_t i = 0; i < count; ++i)
        {
            auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(params[static_cast<int>(i)]);
            expect(withID != nullptr, "Host parameter at index " + juce::String(static_cast<int>(i))
                                      + " has no stable ParameterID");

            const juce::String actualID = withID != nullptr ? withID->getParameterID() : juce::String{};
            expect(actualID == expected[i].id,
                   "Host parameter ID mismatch at index " + juce::String(static_cast<int>(i))
                   + ": expected '" + expected[i].id + "', got '" + actualID + "'");

            expect(params[static_cast<int>(i)]->getVersionHint() == expected[i].versionHint,
                   "Host parameter version mismatch for '" + expected[i].id + "': expected "
                   + juce::String(expected[i].versionHint) + ", got "
                   + juce::String(params[static_cast<int>(i)]->getVersionHint()));

            expect(! seen.contains(actualID), "Duplicate host parameter ID: " + actualID);
            seen.add(actualID);
        }

        if (! expected.empty())
        {
            expect(expected.back().id == "dynamicCorrections", "Golden list must keep D1 as the append-only tail");
            if (! params.isEmpty())
            {
                auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(params.getLast());
                expect(withID != nullptr && withID->getParameterID() == "dynamicCorrections",
                       "dynamicCorrections must remain the final createParameters() entry");
            }
        }
    }

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

    void primeBands(AIEqualizerAudioProcessor& proc)
    {
        proc.setNumActiveBands(3);
        AIEqualizerAudioProcessor::BandState b0{ 500.0f, -3.0f, 0.9f, static_cast<int>(ParametricEQProcessor::Peak), true, false };
        AIEqualizerAudioProcessor::BandState b1{ 2000.0f, 4.0f, 2.0f, static_cast<int>(ParametricEQProcessor::HighShelf), true, false };
        AIEqualizerAudioProcessor::BandState b2{ 8000.0f, -2.0f, 1.4f, static_cast<int>(ParametricEQProcessor::LowShelf), true, false };
        proc.setBandState(0, b0);
        proc.setBandState(1, b1);
        proc.setBandState(2, b2);
    }

    void testStateRoundTrip()
    {
        beginTest("State round-trip preserves bands and globals");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        primeBands(proc);
        setChoice(apvts, "oversamplingFactor", 2); // 4x
        setChoice(apvts, "qualityMode", 1);        // HQ
        setBool(apvts, "bypass", true);
        setFloat(apvts, "outputGain", 3.0f);

        const auto orig0 = proc.getBandState(0);
        const auto orig1 = proc.getBandState(1);
        const auto orig2 = proc.getBandState(2);
        const int origLatency = proc.getLatencySamples();

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        // Mutate
        proc.setNumActiveBands(1);
        setChoice(apvts, "oversamplingFactor", 0);
        setBool(apvts, "bypass", false);
        setFloat(apvts, "outputGain", -6.0f);

        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        auto restored0 = proc.getBandState(0);
        auto restored1 = proc.getBandState(1);
        auto restored2 = proc.getBandState(2);

        expectWithinAbsoluteError(restored0.frequency, orig0.frequency, 1.0f);
        expectWithinAbsoluteError(restored1.frequency, orig1.frequency, 1.0f);
        expectWithinAbsoluteError(restored2.frequency, orig2.frequency, 1.0f);
        expect(restored0.enabled && restored1.enabled && restored2.enabled);

        auto* osParam = apvts.getRawParameterValue("oversamplingFactor");
        auto* bypassParam = apvts.getRawParameterValue("bypass");
        expect(osParam != nullptr);
        expect(bypassParam != nullptr);
        if (osParam) expectWithinAbsoluteError(osParam->load(), 2.0f, 0.01f);
        if (bypassParam) expect(bypassParam->load() > 0.5f);

        expect(proc.getLatencySamples() >= 0);
        expect(proc.getLatencySamples() == origLatency); // worst-case latency should be stable across state load
    }

    void testPresetSchemaRoundTrip()
    {
        beginTest("Preset schema wraps APVTS Parameters and accepts legacy direct Parameters");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();
        auto& presets = proc.getPresetManager();

        setFloat(apvts, "outputGain", 4.0f);

        PresetManager::Preset source;
        source.name = "Schema Test";
        source.category = "User";
        source.description = "Preset schema regression";
        source.state = apvts.copyState();

        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("aieq-preset-schema", "", false);
        expect(tempDir.createDirectory());

        auto canonicalFile = tempDir.getChildFile("canonical.xml");
        expect(presets.exportPreset(source, canonicalFile));

        auto canonicalXml = juce::XmlDocument::parse(canonicalFile);
        expect(canonicalXml != nullptr);
        if (canonicalXml != nullptr)
        {
            expect(canonicalXml->hasTagName("AIEqualizerPreset"));
            auto* stateXml = canonicalXml->getChildByName("State");
            expect(stateXml != nullptr);
            if (stateXml != nullptr)
                expect(stateXml->getChildByName("Parameters") != nullptr);
        }

        auto importedCanonical = presets.importPreset(canonicalFile);
        expect(importedCanonical.state.isValid());
        expect(importedCanonical.state.hasType(apvts.state.getType()));

        setFloat(apvts, "outputGain", -6.0f);
        expect(presets.loadPreset(importedCanonical));
        if (auto* outputGain = apvts.getRawParameterValue("outputGain"))
            expectWithinAbsoluteError(outputGain->load(), 4.0f, 0.01f);

        auto legacyRoot = std::make_unique<juce::XmlElement>("AIEqualizerPreset");
        legacyRoot->setAttribute("name", "Legacy Direct Parameters");
        legacyRoot->setAttribute("category", "User");
        legacyRoot->addChildElement(source.state.createXml().release());
        auto legacyFile = tempDir.getChildFile("legacy-direct.xml");
        expect(legacyRoot->writeTo(legacyFile));

        auto importedLegacy = presets.importPreset(legacyFile);
        expect(importedLegacy.state.isValid());
        expect(importedLegacy.state.hasType(apvts.state.getType()));

        setFloat(apvts, "outputGain", -9.0f);
        expect(presets.loadPreset(importedLegacy));
        if (auto* outputGain = apvts.getRawParameterValue("outputGain"))
            expectWithinAbsoluteError(outputGain->load(), 4.0f, 0.01f);

        auto invalidRoot = std::make_unique<juce::XmlElement>("AIEqualizerPreset");
        auto invalidState = std::make_unique<juce::XmlElement>("State");
        invalidState->addChildElement(new juce::XmlElement("NotParameters"));
        invalidRoot->addChildElement(invalidState.release());
        auto invalidFile = tempDir.getChildFile("invalid-wrapper.xml");
        expect(invalidRoot->writeTo(invalidFile));

        auto importedInvalid = presets.importPreset(invalidFile);
        expect(!importedInvalid.state.isValid());

        PresetManager::Preset badPreset;
        badPreset.name = "Bad Wrapper";
        badPreset.state = juce::ValueTree("State");
        expect(!presets.loadPreset(badPreset));

        expect(tempDir.deleteRecursively());
    }

    void testDynamicABStateRoundTrip()
    {
        beginTest("A/B slots preserve per-band Dynamic EQ state across save/load");
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& apvts = proc.getAPVTS();

        proc.setNumActiveBands(1);
        AIEqualizerAudioProcessor::BandState band0;
        band0.frequency = 1000.0f;
        band0.gain = 12.0f;
        band0.q = 1.0f;
        band0.type = static_cast<int>(ParametricEQProcessor::Peak);
        band0.enabled = true;
        band0.solo = false;
        band0.dynMode = 0;
        band0.dynThreshold = 0.0f;
        band0.dynRatio = 2.0f;
        band0.dynAttack = 10.0f;
        band0.dynRelease = 100.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 6.0f;
        proc.setBandState(0, band0);
        setBool(apvts, "dynEqEnabled", false);
        setFloat(apvts, "dynEqMix", 0.0f);
        setBool(apvts, "dynAutoMakeup", false);

        // Save A with Dynamic EQ effectively off.
        proc.setABState(AIEqualizerAudioProcessor::ABState::B);

        // Configure B with aggressive dynamic compression.
        setBool(apvts, "dynEqEnabled", true);
        setFloat(apvts, "dynEqMix", 100.0f);
        setBool(apvts, "dynAutoMakeup", true);
        band0.dynMode = 1;
        band0.dynThreshold = -30.0f;
        band0.dynRatio = 8.0f;
        band0.dynAttack = 1.0f;
        band0.dynRelease = 50.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 0.0f;
        proc.setBandState(0, band0);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);

        AIEqualizerAudioProcessor restored;
        restored.prepareToPlay(48000.0, 512);
        restored.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        auto& restoredAPVTS = restored.getAPVTS();

        restored.setABState(AIEqualizerAudioProcessor::ABState::A);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynMode"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Threshold"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Ratio"))
            expectWithinAbsoluteError(p->load(), 2.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqEnabled"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqMix"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynAutoMakeup"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);

        restored.setABState(AIEqualizerAudioProcessor::ABState::B);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynMode"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Threshold"))
            expectWithinAbsoluteError(p->load(), -30.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Ratio"))
            expectWithinAbsoluteError(p->load(), 8.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Attack"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Release"))
            expectWithinAbsoluteError(p->load(), 50.0f, 0.1f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Range"))
            expectWithinAbsoluteError(p->load(), 24.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0Knee"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqEnabled"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynEqMix"))
            expectWithinAbsoluteError(p->load(), 100.0f, 0.05f);
        if (auto* p = restoredAPVTS.getRawParameterValue("dynAutoMakeup"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
    }

    void testBypassPassThrough()
    {
        beginTest("Bypass leaves buffer untouched");
        AIEqualizerAudioProcessor proc;
        const int blockSize = 128;
        proc.prepareToPlay(48000.0, blockSize);
        auto& apvts = proc.getAPVTS();
        setBool(apvts, "bypass", true);

        // With Maximum Latency Padding, bypass output is delayed by worstCaseLatencySamples.
        // Fill the delay line first by processing enough silent blocks, then verify
        // that the delayed output matches the original input.
        const int latency = proc.getLatencySamples();
        const int warmupBlocks = (latency / blockSize) + 3; // extra blocks for safety

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> warmup(2, blockSize);
        for (int b = 0; b < warmupBlocks; ++b)
        {
            warmup.clear();
            proc.processBlock(warmup, midi);
        }

        // Now send our test signal — it will come out `latency` samples later
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 1, 0.5f);
        juce::AudioBuffer<float> original(buffer);
        proc.processBlock(buffer, midi);

        // If latency == 0, output should match immediately
        if (latency == 0)
        {
            bool equal = true;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* data = buffer.getReadPointer(ch);
                auto* ref = original.getReadPointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    if (std::abs(data[i] - ref[i]) > 1.0e-6f)
                    {
                        equal = false;
                        break;
                    }
                }
                if (!equal) break;
            }
            expect(equal);
        }
        else
        {
            // Output is delayed — collect enough output blocks to find our test signal
            std::vector<float> outputL, outputR;
            for (int i = 0; i < blockSize; ++i)
            {
                outputL.push_back(buffer.getSample(0, i));
                outputR.push_back(buffer.getSample(1, i));
            }
            // Process more blocks to flush
            const int flushBlocks = (latency / blockSize) + 2;
            for (int b = 0; b < flushBlocks; ++b)
            {
                juce::AudioBuffer<float> flush(2, blockSize);
                flush.clear();
                proc.processBlock(flush, midi);
                for (int i = 0; i < blockSize; ++i)
                {
                    outputL.push_back(flush.getSample(0, i));
                    outputR.push_back(flush.getSample(1, i));
                }
            }

            // Find our impulse in the delayed output (should appear at offset `latency`)
            bool foundL = false, foundR = false;
            for (int i = 0; i < static_cast<int>(outputL.size()); ++i)
            {
                if (!foundL && std::abs(outputL[static_cast<size_t>(i)] - 1.0f) < 1.0e-4f)
                    foundL = true;
                if (!foundR && std::abs(outputR[static_cast<size_t>(i)] - 0.5f) < 1.0e-4f)
                    foundR = true;
            }
            expect(foundL, "Expected impulse in L channel after delay");
            expect(foundR, "Expected impulse in R channel after delay");
        }
    }
};

static IntegrationStateTest integrationStateTest;
