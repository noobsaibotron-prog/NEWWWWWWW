#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "../PluginProcessor.h"
#include "../GUI/BandControlPanel.h"

namespace
{
template <typename ComponentType>
ComponentType* childAs(BandControlPanel& panel, const char* componentID)
{
    return dynamic_cast<ComponentType*>(panel.findChildWithID(componentID));
}

int choiceValue(juce::AudioProcessorValueTreeState& apvts, const char* parameterID)
{
    if (const auto* value = apvts.getRawParameterValue(parameterID))
        return static_cast<int>(std::lround(value->load()));

    return -1;
}

void setBoolParameter(juce::AudioProcessorValueTreeState& apvts,
                      const char* parameterID,
                      bool enabled)
{
    if (auto* parameter = apvts.getParameter(parameterID))
        parameter->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
}
}

class BandControlPanelTest final : public juce::UnitTest
{
public:
    BandControlPanelTest()
        : juce::UnitTest("Band Control Panel parameter surface", "Integration") {}

    void runTest() override
    {
        auto* messageManager = juce::MessageManager::getInstance();
        juce::ignoreUnused(messageManager);

        AIEqualizerAudioProcessor processor;
        BandControlPanel panel(0, processor.getAPVTS());
        panel.setBounds(0, 0, 348, 250);

        auto* type = childAs<juce::ComboBox>(panel, "filterTypeSelector");
        auto* curve = childAs<juce::ComboBox>(panel, "curveModeSelector");
        auto* q = childAs<juce::Slider>(panel, "bandQControl");
        auto* fixedQ = childAs<juce::Label>(panel, "surgicalShelfFixedQ");
        auto* action = childAs<juce::ComboBox>(panel, "dynamicActionSelector");
        auto* detection = childAs<juce::ComboBox>(panel, "detectionModeSelector");
        auto* source = childAs<juce::ComboBox>(panel, "detectorSourceSelector");
        auto* availability = childAs<juce::Label>(panel, "detectorAvailabilityStatus");
        auto* scFrequency = childAs<juce::Slider>(panel, "sidechainFrequencyControl");
        auto* scQ = childAs<juce::Slider>(panel, "sidechainQControl");

        beginTest("Every premium per-band parameter has an attached UI control");
        expect(type != nullptr);
        expect(curve != nullptr);
        expect(q != nullptr);
        expect(fixedQ != nullptr);
        expect(action != nullptr);
        expect(detection != nullptr);
        expect(source != nullptr);
        expect(availability != nullptr);
        expect(scFrequency != nullptr);
        expect(scQ != nullptr);
        if (type == nullptr || curve == nullptr || q == nullptr || fixedQ == nullptr
            || action == nullptr || detection == nullptr || source == nullptr
            || availability == nullptr
            || scFrequency == nullptr || scQ == nullptr)
            return;

        beginTest("Surgical shelves show fixed 0.707 while preserving stored Q");
        expectEquals(type->getSelectedId(), 2);   // band 0 defaults to Low Shelf
        expectEquals(curve->getSelectedId(), 2); // fresh state defaults to Surgical
        expect(! q->isEnabled());
        expect(fixedQ->isVisible());
        expectEquals(fixedQ->getText(), juce::String("FIXED 0.707"));

        type->setSelectedId(3, juce::sendNotificationSync); // Peak Surgical
        expect(q->isEnabled());
        expect(! fixedQ->isVisible());
        expectEquals(choiceValue(processor.getAPVTS(), "band0Type"), 2);

        type->setSelectedId(4, juce::sendNotificationSync); // High Shelf Surgical
        expect(! q->isEnabled());
        expect(fixedQ->isVisible());

        action->setSelectedId(2, juce::sendNotificationSync); // DynEQ owns the shelf
        expect(q->isEnabled());
        expect(! fixedQ->isVisible());
        expect(! curve->isEnabled());

        setBoolParameter(processor.getAPVTS(), "dynEqEnabled", false);
        panel.refreshRuntimeSemantics();
        expect(! q->isEnabled());
        expect(fixedQ->isVisible());
        expect(curve->isEnabled());

        setBoolParameter(processor.getAPVTS(), "dynEqEnabled", true);
        setBoolParameter(processor.getAPVTS(), "band0Enabled", false);
        panel.refreshRuntimeSemantics();
        expect(! q->isEnabled());
        expect(fixedQ->isVisible());
        expect(curve->isEnabled());

        setBoolParameter(processor.getAPVTS(), "band0Enabled", true);
        panel.refreshRuntimeSemantics();
        expect(q->isEnabled());
        expect(! fixedQ->isVisible());
        expect(! curve->isEnabled());

        action->setSelectedId(1, juce::sendNotificationSync); // Back to static Surgical
        expect(! q->isEnabled());
        expect(fixedQ->isVisible());
        expect(curve->isEnabled());

        curve->setSelectedId(1, juce::sendNotificationSync); // Legacy shelf
        expect(q->isEnabled());
        expect(! fixedQ->isVisible());
        expectEquals(choiceValue(processor.getAPVTS(), "band0CurveMode"), 0);

        beginTest("Detector controls are contextual and Filtered alone exposes SC Freq/Q");
        expect(! detection->isVisible());
        expect(! source->isVisible());
        expect(! scFrequency->isVisible());
        expect(! scQ->isVisible());

        action->setSelectedId(2, juce::sendNotificationSync); // Compress
        expect(detection->isVisible());
        expect(source->isVisible());
        expect(! scFrequency->isVisible()); // Internal Wideband default
        expect(! scQ->isVisible());

        source->setSelectedId(2, juce::sendNotificationSync); // Internal Filtered
        expect(scFrequency->isVisible());
        expect(scQ->isVisible());
        expectEquals(choiceValue(processor.getAPVTS(), "band0DetectorSource"), 1);

        source->setSelectedId(3, juce::sendNotificationSync); // External Wideband
        expect(! scFrequency->isVisible());
        expect(! scQ->isVisible());
        expect(availability->isVisible());
        expectEquals(availability->getText(), juce::String("SC MISSING"));
        panel.setExternalDetectorAvailable(true);
        expectEquals(availability->getText(), juce::String("SC READY"));

        source->setSelectedId(4, juce::sendNotificationSync); // External Filtered
        expect(scFrequency->isVisible());
        expect(scQ->isVisible());
        detection->setSelectedId(1, juce::sendNotificationSync); // Peak
        expectEquals(choiceValue(processor.getAPVTS(), "band0DetectionMode"), 0);

        beginTest("setBandIndex rebinds every new attachment");
        panel.setBandIndex(1);
        expectEquals(panel.getBandIndex(), 1);
        expectEquals(type->getSelectedId(), 3);   // band 1 defaults to Peak
        expectEquals(curve->getSelectedId(), 2);  // Surgical
        expect(q->isEnabled());
        expect(! fixedQ->isVisible());
        expectEquals(action->getSelectedId(), 1); // Off
        expectEquals(source->getSelectedId(), 1); // Internal Wideband
        expect(! detection->isVisible());
        expect(! source->isVisible());
    }
};

static BandControlPanelTest bandControlPanelTest;
