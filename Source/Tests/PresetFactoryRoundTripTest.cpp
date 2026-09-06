/*
 * Factory presets must survive their own load.
 *
 * The bug this exists for: PresetManager built factory states by writing the
 * NORMALISED parameter value into the APVTS "value" property. APVTS stores the
 * DENORMALISED value there — replaceState() hands it straight to
 * ParameterAdapter::setDenormalisedValue. So 3000 Hz was written as 0.63, read
 * back as 0.63 Hz and clamped to the 20 Hz range minimum; -2.0 dB was written as
 * 0.45 and read back as +0.45 dB. Every preset that positioned bands collapsed
 * onto the bottom of every range, which is why they all sounded alike.
 *
 * The assertion is deliberately a ROUND-TRIP, not a table of frequencies: it
 * pins the storage contract without pinning anyone's voicing choices, so
 * re-tuning a preset cannot make this test fail, while any future convention
 * mismatch will.
 */
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"
#include "../Utils/PresetManager.h"

#include <map>

namespace
{
class PresetFactoryRoundTripTest final : public juce::UnitTest
{
public:
    PresetFactoryRoundTripTest()
        : juce::UnitTest("Factory preset round-trip", "Integration") {}

    void runTest() override
    {
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        auto& pm = proc.getPresetManager();
        auto& apvts = proc.getAPVTS();
        const auto presets = pm.getFactoryPresets();

        beginTest("Every factory preset declares a loadable state");

        expect(! presets.empty(), "No factory presets were created at all.");

        for (const auto& p : presets)
            expect(pm.loadPreset(p), "loadPreset refused factory preset: " + p.name);

        beginTest("What a preset stores is what the parameter holds after loading");

        int checked = 0;
        for (const auto& p : presets)
        {
            if (! pm.loadPreset(p))
                continue;

            for (int i = 0; i < p.state.getNumChildren(); ++i)
            {
                const auto child = p.state.getChild(i);
                if (! child.hasType("PARAM"))
                    continue;

                const auto id = child.getProperty("id").toString();
                auto* param = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id));
                if (param == nullptr)
                    continue;

                const auto stored = static_cast<float>(child.getProperty("value"));
                const auto live   = param->convertFrom0to1(param->getValue());
                const auto span   = param->getNormalisableRange().end
                                  - param->getNormalisableRange().start;
                const auto tol    = juce::jmax(1.0e-3f, std::abs(span) * 1.0e-4f);

                if (std::abs(stored - live) > tol)
                {
                    expect(false,
                           p.name + " / " + id + ": state says "
                           + juce::String(stored, 4) + " but the parameter holds "
                           + juce::String(live, 4)
                           + ". A normalised value written into the denormalised "
                             "\"value\" property collapses to the range minimum.");
                    return;
                }
                ++checked;
            }
        }

        logMessage("  parameters round-tripped: " + juce::String(checked)
                   + " across " + juce::String((int) presets.size()) + " presets");
        expect(checked > 0, "No parameters were compared — the walk found nothing.");

        beginTest("Loading different presets produces different plugin state");

        std::map<juce::String, juce::StringArray> byState;
        for (const auto& p : presets)
        {
            if (! pm.loadPreset(p))
                continue;

            juce::String key;
            for (auto* param : proc.getParameters())
                if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                    key << w->paramID << "=" << juce::String(w->getValue(), 6) << ";";

            byState[key].add(p.name);
        }

        for (const auto& entry : byState)
            expect(entry.second.size() == 1,
                   "These presets leave the plugin in an identical state: "
                   + entry.second.joinIntoString(", "));

        logMessage("  distinct states: " + juce::String((int) byState.size())
                   + " of " + juce::String((int) presets.size()));
    }
};

static PresetFactoryRoundTripTest presetFactoryRoundTripTest;
} // namespace
