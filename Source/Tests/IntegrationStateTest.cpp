#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"
#include "../DSP/DefaultBandFrequencies.h"

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
        testFactoryPresetUtf8RoundTrip();
        testDynamicABStateRoundTrip();
        testDynTriggerRoundTripAndLegacyPrecedence();
        testDetectorSurfaceRoundTripAndMigration();
        testGraphGeometryEditPreservesAdvancedState();
        testHostParameterSurfaceGoldenList();
        testCurveModeSchemaRoundTrip();
        testLegacyStateMigratesCurveMode();
        testFutureSchemaIsRejectedTransactionally();
        testUndoRedoCoversEntireHostSurface();
        testCorruptStateIsRejectedTransactionally();
        testDefaultFrequenciesStayInsideHostRange();
        testBypassPassThrough();
        testDefaultBandCountIsProcessed();
    }

private:
    struct ExpectedParameter
    {
        juce::String id;
        int versionHint = 1;
    };

    void testDefaultBandCountIsProcessed()
    {
        beginTest("Default band count: a fresh instance processes 8 bands, and reload does not change the sound");

        // numActiveBands defaults to index 7 of 24. APVTS keeps a parameter's
        // default unsnapped until the parameter is first notified, and
        // float32(7/23) * 23 is 6.9999995: a plain int cast turned 8 bands into 7
        // on every new instance, while the host showed 8.
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        const float band8Hz = AIEQDSP::defaultBandFrequencies[7]; // 500 Hz

        // A sine at band 8's centre through processBlock: output over input level, in dB.
        const auto levelAtBand8Db = [&](AIEqualizerAudioProcessor& proc)
        {
            juce::AudioBuffer<float> buffer(2, blockSize);
            juce::MidiBuffer midi;
            const double increment = juce::MathConstants<double>::twoPi * band8Hz / sampleRate;
            const int settleBlocks = static_cast<int>(sampleRate) / blockSize; // about 1 s
            const int measureBlocks = settleBlocks / 2;
            double phase = 0.0, inEnergy = 0.0, outEnergy = 0.0;

            for (int block = 0; block < settleBlocks + measureBlocks; ++block)
            {
                const bool measuring = block >= settleBlocks;
                for (int i = 0; i < blockSize; ++i)
                {
                    const float sample = 0.1f * static_cast<float>(std::sin(phase));
                    phase += increment;
                    buffer.setSample(0, i, sample);
                    buffer.setSample(1, i, sample);
                    if (measuring)
                        inEnergy += static_cast<double>(sample) * sample;
                }

                proc.processBlock(buffer, midi);

                if (measuring)
                    for (int i = 0; i < blockSize; ++i)
                        outEnergy += static_cast<double>(buffer.getSample(0, i)) * buffer.getSample(0, i);
            }

            return static_cast<float>(10.0 * std::log10(outEnergy / inEnergy));
        };

        AIEqualizerAudioProcessor fresh;
        fresh.prepareToPlay(sampleRate, blockSize);

        auto* count = dynamic_cast<juce::AudioParameterChoice*>(fresh.getAPVTS().getParameter("numActiveBands"));
        expect(count != nullptr, "numActiveBands must be a choice parameter");
        if (count == nullptr)
            return;

        expectEquals(count->getIndex() + 1, 8, "the host sees the default of 8 bands");
        expectEquals(fresh.getNumActiveBands(), 8, "the audio path must process the 8 bands the host sees");

        // Boost band 8 through its own gain parameter; the count is never touched.
        auto* gain = fresh.getAPVTS().getParameter("band7Gain");
        gain->beginChangeGesture();
        gain->setValueNotifyingHost(gain->convertTo0to1(12.0f));
        gain->endChangeGesture();

        const float freshDb = levelAtBand8Db(fresh);
        expectWithinAbsoluteError(freshDb, 12.0f, 0.5f, "band 8 at +12 dB must be heard on a fresh instance");

        juce::MemoryBlock saved;
        fresh.getStateInformation(saved);

        AIEqualizerAudioProcessor reloaded;
        reloaded.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        reloaded.prepareToPlay(sampleRate, blockSize);

        const float reloadedDb = levelAtBand8Db(reloaded);
        expectWithinAbsoluteError(reloadedDb, freshDb, 0.1f, "saving and reloading must not change the sound");
    }

    void testGraphGeometryEditPreservesAdvancedState()
    {
        beginTest("Graph geometry edit preserves every advanced per-band field");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 128);

        auto state = proc.getBandState(0);
        state.solo = true;
        state.slope = 2;
        state.curveMode = static_cast<int>(ParametricEQProcessor::CurveMode::Legacy);
        state.dynMode = DynamicEQProcessor::DynamicMode_Compress;
        state.dynTrigger = DynamicEQProcessor::TriggerSide_Below;
        state.detectionMode = DynamicEQProcessor::DetectionMode_Peak;
        state.detectorSource = DynamicEQProcessor::DetectorSource_ExternalFiltered;
        state.sidechainFrequency = 4321.0f;
        state.sidechainQ = 4.25f;
        state.dynThreshold = -31.0f;
        state.dynRatio = 7.0f;
        state.dynAttack = 4.0f;
        state.dynRelease = 222.0f;
        state.dynRange = 17.0f;
        state.dynKnee = 3.0f;
        proc.setBandState(0, state);

        proc.setBandGeometry(0, 2345.0f, -5.0f, 1.75f,
                             static_cast<int>(ParametricEQProcessor::Peak), true);

        const auto edited = proc.getBandState(0);
        expectWithinAbsoluteError(edited.frequency, 2345.0f, 1.0f);
        expectWithinAbsoluteError(edited.gain, -5.0f, 0.05f);
        expectWithinAbsoluteError(edited.q, 1.75f, 0.02f);
        expect(edited.solo);
        expectEquals(edited.slope, 2);
        expectEquals(edited.curveMode, state.curveMode);
        expectEquals(edited.dynMode, state.dynMode);
        expectEquals(edited.dynTrigger, state.dynTrigger);
        expectEquals(edited.detectionMode, state.detectionMode);
        expectEquals(edited.detectorSource, state.detectorSource);
        expectWithinAbsoluteError(edited.sidechainFrequency, state.sidechainFrequency, 1.0f);
        expectWithinAbsoluteError(edited.sidechainQ, state.sidechainQ, 0.02f);
        expectWithinAbsoluteError(edited.dynThreshold, state.dynThreshold, 0.05f);
        expectWithinAbsoluteError(edited.dynRatio, state.dynRatio, 0.02f);
        expectWithinAbsoluteError(edited.dynAttack, state.dynAttack, 0.1f);
        expectWithinAbsoluteError(edited.dynRelease, state.dynRelease, 0.5f);
        expectWithinAbsoluteError(edited.dynRange, state.dynRange, 0.05f);
        expectWithinAbsoluteError(edited.dynKnee, state.dynKnee, 0.05f);
    }

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

        // CurveMode was added append-only after the complete pre-existing host
        // surface. Keeping these entries at the tail preserves every old index.
        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "CurveMode", 2 });

        // DynTrigger is a second append-only block. It must never be inserted
        // into the historical per-band group or ahead of CurveMode.
        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "DynTrigger", 2 });

        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "DetectionMode", 2 });
        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "DetectorSource", 2 });
        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "SidechainFreq", 2 });
        for (int band = 0; band < AIEqualizerAudioProcessor::maxBands; ++band)
            ids.push_back({ "band" + juce::String(band) + "SidechainQ", 2 });

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
            const auto dynamicCorrectionsIndex = expected.size()
                - 6u * static_cast<size_t>(AIEqualizerAudioProcessor::maxBands) - 1u;
            expect(expected[dynamicCorrectionsIndex].id == "dynamicCorrections",
                   "CurveMode parameters must be appended after the old host surface");
            expect(expected[dynamicCorrectionsIndex
                            + static_cast<size_t>(AIEqualizerAudioProcessor::maxBands)].id
                       == "band23CurveMode",
                   "DynTrigger must be appended after the complete CurveMode block");
            const auto blockSize = static_cast<size_t>(AIEqualizerAudioProcessor::maxBands);
            expect(expected[dynamicCorrectionsIndex + 2u * blockSize].id == "band23DynTrigger",
                   "DetectionMode must follow the complete DynTrigger block");
            expect(expected[dynamicCorrectionsIndex + 3u * blockSize].id == "band23DetectionMode",
                   "DetectorSource must follow the complete DetectionMode block");
            expect(expected[dynamicCorrectionsIndex + 4u * blockSize].id == "band23DetectorSource",
                   "SidechainFreq must follow the complete DetectorSource block");
            expect(expected[dynamicCorrectionsIndex + 5u * blockSize].id == "band23SidechainFreq",
                   "SidechainQ must follow the complete SidechainFreq block");
            expect(expected.back().id == "band23SidechainQ",
                   "SidechainQ append-only block must remain the host-surface tail");
            if (! params.isEmpty())
            {
                auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(params.getLast());
                expect(withID != nullptr && withID->getParameterID() == "band23SidechainQ",
                       "band23SidechainQ must remain the final createParameters() entry");
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

    void testDynTriggerRoundTripAndLegacyPrecedence()
    {
        beginTest("DynTrigger is append-only, round-trips, and legacy Gate overrides it");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 64);
        auto& apvts = proc.getAPVTS();
        auto* trigger = apvts.getRawParameterValue("band0DynTrigger");
        expect(trigger != nullptr);
        if (trigger == nullptr)
            return;

        expectWithinAbsoluteError(trigger->load(),
                                  static_cast<float>(DynamicEQProcessor::TriggerSide_Above),
                                  0.0f);

        setChoice(apvts, "band0DynMode", DynamicEQProcessor::DynamicMode_Expand);
        setChoice(apvts, "band0DynTrigger", DynamicEQProcessor::TriggerSide_Below);

        juce::AudioBuffer<float> buffer(2, 64);
        buffer.clear();
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);
        auto live = proc.getDynamicEQProcessor().getBandParams(0);
        expectEquals(live.dynamicMode, DynamicEQProcessor::DynamicMode_Expand);
        expectEquals(live.triggerSide, DynamicEQProcessor::TriggerSide_Below);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        setChoice(apvts, "band0DynTrigger", DynamicEQProcessor::TriggerSide_Above);
        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        expectWithinAbsoluteError(trigger->load(),
                                  static_cast<float>(DynamicEQProcessor::TriggerSide_Below),
                                  0.0f);

        // Host value 3 keeps its historical cardinality and always means
        // Expand+Below, even if current/new automation writes DynTrigger=Above.
        setChoice(apvts, "band0DynTrigger", DynamicEQProcessor::TriggerSide_Above);
        setChoice(apvts, "band0DynMode", DynamicEQProcessor::DynamicMode_Gate);
        proc.processBlock(buffer, midi);
        live = proc.getDynamicEQProcessor().getBandParams(0);
        expectEquals(live.dynamicMode, DynamicEQProcessor::DynamicMode_Gate);
        expectEquals(live.triggerSide, DynamicEQProcessor::TriggerSide_Below);
        expectWithinAbsoluteError(trigger->load(),
                                  static_cast<float>(DynamicEQProcessor::TriggerSide_Above),
                                  0.0f,
                                  "Legacy override must not mutate the separately automated parameter");

        // Early schema-v1 states predate DynTrigger. Loading one must append
        // Above explicitly rather than inherit the value from the current
        // instance; DynMode=3 still resolves to Below at the DSP boundary.
        auto xml = proc.getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        expect(xml != nullptr);
        if (xml != nullptr)
        {
            auto oldV1 = juce::ValueTree::fromXml(*xml);
            for (int i = oldV1.getNumChildren(); --i >= 0;)
            {
                auto child = oldV1.getChild(i);
                const auto id = child.getProperty("id").toString();
                if (child.hasType("PARAM") && id.endsWith("DynTrigger"))
                    oldV1.removeChild(i, nullptr);
                else if (child.hasType("PARAM") && id == "band0DynMode")
                    child.setProperty("value", 3.0f, nullptr); // APVTS stores the real choice index
            }

            setChoice(apvts, "band0DynTrigger", DynamicEQProcessor::TriggerSide_Below);
            juce::MemoryBlock oldV1Blob;
            auto oldV1Xml = oldV1.createXml();
            juce::AudioProcessor::copyXmlToBinary(*oldV1Xml, oldV1Blob);
            proc.setStateInformation(oldV1Blob.getData(), static_cast<int>(oldV1Blob.getSize()));

            expectWithinAbsoluteError(trigger->load(),
                                      static_cast<float>(DynamicEQProcessor::TriggerSide_Above),
                                      0.0f,
                                      "Missing DynTrigger inherited stale live state");
            proc.processBlock(buffer, midi);
            live = proc.getDynamicEQProcessor().getBandParams(0);
            expectEquals(live.dynamicMode, DynamicEQProcessor::DynamicMode_Gate);
            expectEquals(live.triggerSide, DynamicEQProcessor::TriggerSide_Below);
        }
    }

    void testDetectorSurfaceRoundTripAndMigration()
    {
        beginTest("Detector controls are append-only, reach DSP, round-trip, and migrate explicitly");

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 64);
        auto& apvts = proc.getAPVTS();

        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectionMode")->load(),
                                  static_cast<float>(DynamicEQProcessor::DetectionMode_RMS), 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectorSource")->load(),
                                  static_cast<float>(DynamicEQProcessor::DetectorSource_InternalWideband), 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainFreq")->load(),
                                  AIEQDSP::defaultBandFrequencies[0], 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainQ")->load(), 1.0f, 0.01f);

        setChoice(apvts, "band0DetectionMode", DynamicEQProcessor::DetectionMode_Peak);
        setChoice(apvts, "band0DetectorSource", DynamicEQProcessor::DetectorSource_ExternalFiltered);
        setFloat(apvts, "band0SidechainFreq", 3210.0f);
        setFloat(apvts, "band0SidechainQ", 3.25f);

        juce::AudioBuffer<float> buffer(2, 64);
        buffer.clear();
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);
        auto live = proc.getDynamicEQProcessor().getBandParams(0);
        expectEquals(live.detection, DynamicEQProcessor::DetectionMode_Peak);
        expectEquals(live.detectorSource, DynamicEQProcessor::DetectorSource_ExternalFiltered);
        expect(live.sidechainEnabled, "Filtered source must enable the detector filter");
        expectWithinAbsoluteError(live.sidechainFreq, 3210.0f, 0.01f);
        expectWithinAbsoluteError(live.sidechainQ, 3.25f, 0.01f);

        const auto band = proc.getBandState(0);
        expectEquals(band.detectionMode, DynamicEQProcessor::DetectionMode_Peak);
        expectEquals(band.detectorSource, DynamicEQProcessor::DetectorSource_ExternalFiltered);
        expectWithinAbsoluteError(band.sidechainFrequency, 3210.0f, 0.01f);
        expectWithinAbsoluteError(band.sidechainQ, 3.25f, 0.01f);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        setChoice(apvts, "band0DetectionMode", DynamicEQProcessor::DetectionMode_RMS);
        setChoice(apvts, "band0DetectorSource", DynamicEQProcessor::DetectorSource_InternalWideband);
        setFloat(apvts, "band0SidechainFreq", 800.0f);
        setFloat(apvts, "band0SidechainQ", 0.5f);
        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectionMode")->load(), 0.0f, 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectorSource")->load(), 3.0f, 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainFreq")->load(), 3210.0f, 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainQ")->load(), 3.25f, 0.01f);

        auto xml = proc.getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        expect(xml != nullptr);
        if (xml == nullptr)
            return;

        auto oldV1 = juce::ValueTree::fromXml(*xml);
        for (int i = oldV1.getNumChildren(); --i >= 0;)
        {
            const auto child = oldV1.getChild(i);
            if (! child.hasType("PARAM"))
                continue;
            const auto id = child.getProperty("id").toString();
            if (id.endsWith("DetectionMode") || id.endsWith("DetectorSource")
                || id.endsWith("SidechainFreq") || id.endsWith("SidechainQ"))
                oldV1.removeChild(i, nullptr);
        }

        setChoice(apvts, "band0DetectionMode", DynamicEQProcessor::DetectionMode_Peak);
        setChoice(apvts, "band0DetectorSource", DynamicEQProcessor::DetectorSource_ExternalFiltered);
        setFloat(apvts, "band0SidechainFreq", 7777.0f);
        setFloat(apvts, "band0SidechainQ", 7.0f);

        juce::MemoryBlock oldV1Blob;
        auto oldV1Xml = oldV1.createXml();
        juce::AudioProcessor::copyXmlToBinary(*oldV1Xml, oldV1Blob);
        proc.setStateInformation(oldV1Blob.getData(), static_cast<int>(oldV1Blob.getSize()));

        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectionMode")->load(), 1.0f, 0.01f,
                                  "Missing DetectionMode inherited stale live state");
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0DetectorSource")->load(), 0.0f, 0.01f,
                                  "Missing DetectorSource inherited stale live state");
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainFreq")->load(),
                                  AIEQDSP::defaultBandFrequencies[0], 0.01f,
                                  "Missing SidechainFreq inherited stale live state");
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0SidechainQ")->load(), 1.0f, 0.01f,
                                  "Missing SidechainQ inherited stale live state");
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

        // A genuinely pre-v1 APVTS preset has neither the root schema marker
        // nor CurveMode parameters. PresetManager must apply the same migration
        // as host state recall before it calls replaceState().
        PresetManager::Preset preV1Preset = source;
        preV1Preset.name = "Pre-v1 CurveMode Migration";
        preV1Preset.state = source.state.createCopy();
        preV1Preset.state.removeProperty("stateSchemaVersion", nullptr);
        for (int i = preV1Preset.state.getNumChildren(); --i >= 0;)
        {
            const auto child = preV1Preset.state.getChild(i);
            if (child.hasType("PARAM"))
            {
                const auto id = child.getProperty("id").toString();
                if (id.endsWith("CurveMode") || id.endsWith("DynTrigger")
                    || id.endsWith("DetectionMode") || id.endsWith("DetectorSource")
                    || id.endsWith("SidechainFreq") || id.endsWith("SidechainQ"))
                    preV1Preset.state.removeChild(i, nullptr);
            }
        }

        setChoice(apvts, "band0CurveMode", 1);
        expect(presets.loadPreset(preV1Preset));
        expectEquals(static_cast<int>(apvts.state.getProperty("stateSchemaVersion")), 1);
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            auto* mode = apvts.getRawParameterValue(
                "band" + juce::String(i) + "CurveMode");
            expect(mode != nullptr);
            if (mode != nullptr)
                expectWithinAbsoluteError(mode->load(), 0.0f, 0.0f);
            auto* triggerParam = apvts.getRawParameterValue(
                "band" + juce::String(i) + "DynTrigger");
            expect(triggerParam != nullptr);
            if (triggerParam != nullptr)
                expectWithinAbsoluteError(triggerParam->load(), 0.0f, 0.0f);
            const auto prefix = "band" + juce::String(i);
            auto* detection = apvts.getRawParameterValue(prefix + "DetectionMode");
            auto* sourceParam = apvts.getRawParameterValue(prefix + "DetectorSource");
            auto* scFreq = apvts.getRawParameterValue(prefix + "SidechainFreq");
            auto* scQ = apvts.getRawParameterValue(prefix + "SidechainQ");
            expect(detection != nullptr && sourceParam != nullptr && scFreq != nullptr && scQ != nullptr);
            if (detection != nullptr)
                expectWithinAbsoluteError(detection->load(), 1.0f, 0.01f);
            if (sourceParam != nullptr)
                expectWithinAbsoluteError(sourceParam->load(), 0.0f, 0.01f);
            if (scFreq != nullptr)
                expectWithinAbsoluteError(
                    scFreq->load(), AIEQDSP::defaultBandFrequencies[static_cast<size_t>(i)], 0.01f);
            if (scQ != nullptr)
                expectWithinAbsoluteError(scQ->load(), 1.0f, 0.01f);
        }

        // Unsupported preset schemas must not partially alter the live APVTS.
        PresetManager::Preset futurePreset = source;
        futurePreset.name = "Unsupported Future Schema";
        futurePreset.state = source.state.createCopy();
        futurePreset.state.setProperty("stateSchemaVersion", 2, nullptr);
        setFloat(apvts, "outputGain", -7.0f);
        setChoice(apvts, "band0CurveMode", 0);
        expect(! presets.loadPreset(futurePreset));
        expectWithinAbsoluteError(apvts.getRawParameterValue("outputGain")->load(),
                                  -7.0f, 0.01f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0CurveMode")->load(),
                                  0.0f, 0.0f);

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

    void testFactoryPresetUtf8RoundTrip()
    {
        beginTest("Factory preset descriptions use explicit UTF-8 and round-trip exactly");

        AIEqualizerAudioProcessor proc;
        auto& manager = proc.getPresetManager();
        const auto presets = manager.getFactoryPresets();
        expectEquals(static_cast<int>(presets.size()), 22, "Factory preset count changed");

        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("aieq-factory-preset-utf8", "", false);
        expect(tempDir.createDirectory());

        constexpr juce::juce_wchar emDash = 0x2014;
        constexpr juce::juce_wchar replacementCharacter = 0xfffd;
        int descriptionsWithEmDash = 0;

        for (size_t index = 0; index < presets.size(); ++index)
        {
            const auto& preset = presets[index];
            expect(!preset.description.containsChar(replacementCharacter),
                   "Factory preset description contains U+FFFD: " + preset.name);

            if (!preset.description.containsChar(emDash))
                continue;

            ++descriptionsWithEmDash;
            const auto file = tempDir.getChildFile("preset-" + juce::String(static_cast<int>(index)) + ".xml");
            expect(manager.exportPreset(preset, file), "Failed to export UTF-8 preset: " + preset.name);
            const auto imported = manager.importPreset(file);
            expect(imported.description == preset.description,
                   "UTF-8 description changed during XML round-trip: " + preset.name);
        }

        expectEquals(descriptionsWithEmDash, 8,
                     "Expected exactly the eight signed factory descriptions with an em dash");
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
        band0.dynTrigger = DynamicEQProcessor::TriggerSide_Above;
        band0.detectionMode = DynamicEQProcessor::DetectionMode_RMS;
        band0.detectorSource = DynamicEQProcessor::DetectorSource_InternalWideband;
        band0.sidechainFrequency = 1000.0f;
        band0.sidechainQ = 1.0f;
        band0.dynThreshold = 0.0f;
        band0.dynRatio = 2.0f;
        band0.dynAttack = 10.0f;
        band0.dynRelease = 100.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 6.0f;
        band0.curveMode = static_cast<int>(ParametricEQProcessor::CurveMode::Legacy);
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
        band0.dynTrigger = DynamicEQProcessor::TriggerSide_Below;
        band0.detectionMode = DynamicEQProcessor::DetectionMode_Peak;
        band0.detectorSource = DynamicEQProcessor::DetectorSource_ExternalFiltered;
        band0.sidechainFrequency = 2500.0f;
        band0.sidechainQ = 4.0f;
        band0.dynThreshold = -30.0f;
        band0.dynRatio = 8.0f;
        band0.dynAttack = 1.0f;
        band0.dynRelease = 50.0f;
        band0.dynRange = 24.0f;
        band0.dynKnee = 0.0f;
        band0.curveMode = static_cast<int>(ParametricEQProcessor::CurveMode::Surgical);
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
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynTrigger"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DetectionMode"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DetectorSource"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0SidechainFreq"))
            expectWithinAbsoluteError(p->load(), 1000.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0SidechainQ"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
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
        if (auto* p = restoredAPVTS.getRawParameterValue("band0CurveMode"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);

        restored.setABState(AIEqualizerAudioProcessor::ABState::B);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynMode"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DynTrigger"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DetectionMode"))
            expectWithinAbsoluteError(p->load(), 0.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0DetectorSource"))
            expectWithinAbsoluteError(p->load(), 3.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0SidechainFreq"))
            expectWithinAbsoluteError(p->load(), 2500.0f, 0.01f);
        if (auto* p = restoredAPVTS.getRawParameterValue("band0SidechainQ"))
            expectWithinAbsoluteError(p->load(), 4.0f, 0.01f);
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
        if (auto* p = restoredAPVTS.getRawParameterValue("band0CurveMode"))
            expectWithinAbsoluteError(p->load(), 1.0f, 0.01f);
    }

    void testCurveModeSchemaRoundTrip()
    {
        beginTest("Schema v1 and CurveMode round-trip are explicit");

        AIEqualizerAudioProcessor proc;
        auto& apvts = proc.getAPVTS();
        expectEquals(static_cast<int>(apvts.state.getProperty("stateSchemaVersion")), 1);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0CurveMode")->load(),
                                  1.0f, 0.0f);

        setChoice(apvts, "band0CurveMode", 0);
        setChoice(apvts, "band1CurveMode", 1);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        auto xml = proc.getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        expect(xml != nullptr);
        if (xml != nullptr)
        {
            auto saved = juce::ValueTree::fromXml(*xml);
            expectEquals(static_cast<int>(saved.getProperty("stateSchemaVersion")), 1);
        }

        setChoice(apvts, "band0CurveMode", 1);
        setChoice(apvts, "band1CurveMode", 0);
        proc.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

        expectWithinAbsoluteError(apvts.getRawParameterValue("band0CurveMode")->load(),
                                  0.0f, 0.0f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band1CurveMode")->load(),
                                  1.0f, 0.0f);
    }

    void testLegacyStateMigratesCurveMode()
    {
        beginTest("Pre-v1 state migrates every CurveMode and A/B slot to Legacy");

        AIEqualizerAudioProcessor source;
        juce::MemoryBlock currentBlob;
        source.getStateInformation(currentBlob);
        auto xml = source.getXmlFromBinary(currentBlob.getData(),
                                           static_cast<int>(currentBlob.getSize()));
        expect(xml != nullptr);
        if (xml == nullptr)
            return;

        auto legacy = juce::ValueTree::fromXml(*xml);
        legacy.removeProperty("stateSchemaVersion", nullptr);
        for (int i = legacy.getNumChildren(); --i >= 0;)
        {
            auto child = legacy.getChild(i);
            if (child.hasType("PARAM"))
            {
                const auto id = child.getProperty("id").toString();
                if (id.endsWith("CurveMode") || id.endsWith("DynTrigger"))
                    legacy.removeChild(i, nullptr);
            }
        }
        for (const auto slotName : { "SlotA", "SlotB", "SlotC", "SlotD" })
        {
            auto slot = legacy.getChildWithName(slotName);
            for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
            {
                auto band = slot.getChildWithName("band" + juce::String(i));
                if (band.isValid())
                {
                    band.removeProperty("curveMode", nullptr);
                    band.removeProperty("dynTrigger", nullptr);
                }
            }
        }

        juce::MemoryBlock legacyBlob;
        auto legacyXml = legacy.createXml();
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyBlob);

        AIEqualizerAudioProcessor restored;
        restored.setStateInformation(legacyBlob.getData(), static_cast<int>(legacyBlob.getSize()));
        auto& restoredState = restored.getAPVTS();
        expectEquals(static_cast<int>(restoredState.state.getProperty("stateSchemaVersion")), 1);
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            auto* mode = restoredState.getRawParameterValue(
                "band" + juce::String(i) + "CurveMode");
            expect(mode != nullptr);
            if (mode != nullptr)
                expectWithinAbsoluteError(mode->load(), 0.0f, 0.0f);
            auto* trigger = restoredState.getRawParameterValue(
                "band" + juce::String(i) + "DynTrigger");
            expect(trigger != nullptr);
            if (trigger != nullptr)
                expectWithinAbsoluteError(trigger->load(), 0.0f, 0.0f);
        }

        restored.setABState(AIEqualizerAudioProcessor::ABState::B);
        expectWithinAbsoluteError(restored.getBandState(0).curveMode,
                                  static_cast<int>(ParametricEQProcessor::CurveMode::Legacy), 0);
    }

    void testFutureSchemaIsRejectedTransactionally()
    {
        beginTest("Unsupported future state schema is a transactional no-op");

        AIEqualizerAudioProcessor proc;
        auto& apvts = proc.getAPVTS();
        setFloat(apvts, "outputGain", 5.0f);
        setChoice(apvts, "band0CurveMode", 0);

        juce::MemoryBlock blob;
        proc.getStateInformation(blob);
        auto xml = proc.getXmlFromBinary(blob.getData(), static_cast<int>(blob.getSize()));
        expect(xml != nullptr);
        if (xml == nullptr)
            return;

        auto future = juce::ValueTree::fromXml(*xml);
        future.setProperty("stateSchemaVersion", 2, nullptr);
        for (int i = 0; i < future.getNumChildren(); ++i)
        {
            auto child = future.getChild(i);
            if (child.hasType("PARAM") && child.getProperty("id").toString() == "outputGain")
                child.setProperty("value", -12.0f, nullptr);
            if (child.hasType("PARAM") && child.getProperty("id").toString() == "band0CurveMode")
                child.setProperty("value", 1.0f, nullptr);
        }

        juce::MemoryBlock futureBlob;
        auto futureXml = future.createXml();
        juce::AudioProcessor::copyXmlToBinary(*futureXml, futureBlob);
        proc.setStateInformation(futureBlob.getData(), static_cast<int>(futureBlob.getSize()));

        expectWithinAbsoluteError(apvts.getRawParameterValue("outputGain")->load(),
                                  5.0f, 1.0e-6f);
        expectWithinAbsoluteError(apvts.getRawParameterValue("band0CurveMode")->load(),
                                  0.0f, 0.0f);
        expectEquals(static_cast<int>(apvts.state.getProperty("stateSchemaVersion")), 1);
    }

    void testUndoRedoCoversEntireHostSurface()
    {
        beginTest("Undo/redo restores every host parameter, including globals and dynamic flags");

        AIEqualizerAudioProcessor proc;
        const auto& params = proc.getParameters();
        std::vector<float> before;
        std::vector<float> after;
        before.reserve(static_cast<size_t>(params.size()));
        after.reserve(static_cast<size_t>(params.size()));

        for (int i = 0; i < params.size(); ++i)
        {
            const float requested = 0.11f + 0.67f
                * static_cast<float>((i * 37) % 101) / 100.0f;
            params[i]->setValueNotifyingHost(requested);
            before.push_back(params[i]->getValue());
        }

        proc.pushUndoState("Complete host surface");
        expect(proc.canUndo());

        for (int i = 0; i < params.size(); ++i)
        {
            const float requested = 0.17f + 0.71f
                * static_cast<float>((i * 53 + 19) % 101) / 100.0f;
            params[i]->setValueNotifyingHost(requested);
            after.push_back(params[i]->getValue());
        }

        proc.undo();
        expect(proc.canRedo());
        for (int i = 0; i < params.size(); ++i)
            expectWithinAbsoluteError(params[i]->getValue(), before[static_cast<size_t>(i)],
                                      1.0e-6f,
                                      "Undo missed parameter index " + juce::String(i));

        proc.redo();
        for (int i = 0; i < params.size(); ++i)
            expectWithinAbsoluteError(params[i]->getValue(), after[static_cast<size_t>(i)],
                                      1.0e-6f,
                                      "Redo missed parameter index " + juce::String(i));
    }

    void testCorruptStateIsRejectedTransactionally()
    {
        beginTest("Corrupt host state is a no-op");

        AIEqualizerAudioProcessor proc;
        auto& apvts = proc.getAPVTS();
        setFloat(apvts, "outputGain", 5.0f);
        setBool(apvts, "dynEqEnabled", true);
        setChoice(apvts, "phaseMode", 1);

        const float gainBefore = apvts.getParameter("outputGain")->getValue();
        const float dynamicBefore = apvts.getParameter("dynEqEnabled")->getValue();
        const float phaseBefore = apvts.getParameter("phaseMode")->getValue();

        const std::array<unsigned char, 13> corrupt {
            0x41, 0x49, 0x45, 0x51, 0xff, 0x00, 0x13,
            0x37, 0xde, 0xad, 0xbe, 0xef, 0x7f
        };
        proc.setStateInformation(corrupt.data(), static_cast<int>(corrupt.size()));

        expectWithinAbsoluteError(apvts.getParameter("outputGain")->getValue(), gainBefore, 0.0f);
        expectWithinAbsoluteError(apvts.getParameter("dynEqEnabled")->getValue(), dynamicBefore, 0.0f);
        expectWithinAbsoluteError(apvts.getParameter("phaseMode")->getValue(), phaseBefore, 0.0f);
    }

    void testDefaultFrequenciesStayInsideHostRange()
    {
        beginTest("All static and dynamic band defaults are within 20 Hz to 20 kHz");

        AIEqualizerAudioProcessor proc;
        auto& apvts = proc.getAPVTS();
        DynamicEQProcessor dynamic;

        float previous = 0.0f;
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            const auto id = "band" + juce::String(i) + "Freq";
            auto* value = apvts.getRawParameterValue(id);
            expect(value != nullptr);
            if (value == nullptr)
                continue;

            const float staticFrequency = value->load();
            const float dynamicFrequency = dynamic.getBandParams(i).frequency;
            expect(staticFrequency >= 20.0f && staticFrequency <= 20000.0f, id);
            expect(dynamicFrequency >= 20.0f && dynamicFrequency <= 20000.0f, id + " dynamic");
            expectWithinAbsoluteError(dynamicFrequency, staticFrequency, 1.0f, id + " mismatch");
            expect(staticFrequency > previous, id + " must be strictly increasing");
            previous = staticFrequency;
        }
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
