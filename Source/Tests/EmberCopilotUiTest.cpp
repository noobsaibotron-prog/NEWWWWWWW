#if JUCE_UNIT_TESTS

#include <juce_gui_basics/juce_gui_basics.h>

#include "../AI/SemanticPlanner.h"
#include "../GUI/Ember/EmberCopilotPlanAdapter.h"
#include "../GUI/Ember/EmberV2Shell.h"
#include "../PluginProcessor.h"

#include <limits>

namespace
{
std::vector<AIEqualizerAudioProcessor::BandState> bandSnapshot(
    const AIEqualizerAudioProcessor& processor)
{
    std::vector<AIEqualizerAudioProcessor::BandState> out;
    out.reserve(AIEqualizerAudioProcessor::maxBands);
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        out.push_back(processor.getBandState(i));
    return out;
}

bool sameBands(const std::vector<AIEqualizerAudioProcessor::BandState>& a,
               const std::vector<AIEqualizerAudioProcessor::BandState>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].enabled != b[i].enabled || a[i].type != b[i].type
            || std::abs(a[i].frequency - b[i].frequency) > 0.01f
            || std::abs(a[i].gain - b[i].gain) > 0.001f
            || std::abs(a[i].q - b[i].q) > 0.001f)
            return false;
    }
    return true;
}

template <typename ComponentType>
ComponentType* findDescendantWithID(juce::Component& root, const juce::String& id)
{
    for (int i = 0; i < root.getNumChildComponents(); ++i)
    {
        auto* child = root.getChildComponent(i);
        if (child->getComponentID() == id)
            return dynamic_cast<ComponentType*>(child);

        if (auto* match = findDescendantWithID<ComponentType>(*child, id))
            return match;
    }

    return nullptr;
}
} // namespace

class EmberCopilotUiTest : public juce::UnitTest
{
public:
    EmberCopilotUiTest()
        : juce::UnitTest("Ember Specchio Copilot proposal UI", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        AIEQPerceptual::SemanticPlanner planner;
        const auto plan = planner.plan("more air", 48000.0, 1.0f);

        beginTest("canonical Semantic plan maps to an exact fail-closed ghost vector");
        expect(plan.valid && plan.fit.valid && ! plan.fit.bands.empty());
        const auto ghosts = EmberCopilot::ghostsFromPlan(plan);
        expectEquals(static_cast<int>(ghosts.size()),
                     static_cast<int>(plan.fit.bands.size()));
        if (! ghosts.empty())
        {
            expectWithinAbsoluteError(ghosts.front().hz,
                                      plan.fit.bands.front().frequencyHz, 0.001f);
            expectWithinAbsoluteError(ghosts.front().db,
                                      plan.fit.bands.front().gainDb, 0.001f);
            expectWithinAbsoluteError(ghosts.front().q,
                                      plan.fit.bands.front().q, 0.001f);
        }

        auto invalid = plan;
        invalid.fit.bands.front().gainDb = std::numeric_limits<float>::infinity();
        expect(EmberCopilot::ghostsFromPlan(invalid).empty(),
               "one invalid band must reject the complete preview");

        beginTest("staging is visible in Specchio and never mutates DSP");
        AIEqualizerAudioProcessor processor;
        processor.prepareToPlay(48000.0, 512);
        EmberV2Shell shell(processor, nullptr);
        shell.setBounds(0, 0, 980, 640);

        bool linkChanged = false;
        bool pairRequested = false;
        shell.onCopilotLinkChanged = [&](bool enabled) { linkChanged = enabled; };
        shell.onCopilotPair = [&] { pairRequested = true; };

        auto* menu = findDescendantWithID<juce::Button>(shell, "emberV2Menu");
        expect(menu != nullptr);
        if (menu != nullptr && menu->onClick)
            menu->onClick();
        auto* link = findDescendantWithID<juce::ToggleButton>(
            shell, "emberV2CopilotLink");
        expect(link != nullptr && link->isVisible());
        if (link != nullptr)
        {
            link->setToggleState(true, juce::dontSendNotification);
            if (link->onClick) link->onClick();
        }
        expect(linkChanged, "Overflow Link must be wired to the shell callback");

        EmberProposal::LinkUiState connected;
        connected.linkEnabled = true;
        connected.connected = true;
        connected.authenticated = true;
        connected.statusText = "Connected";
        shell.setCopilotUi(connected);
        auto* pair = findDescendantWithID<juce::Button>(shell, "emberV2CopilotPair");
        expect(pair != nullptr && pair->isEnabled());
        if (pair != nullptr && pair->onClick)
            pair->onClick();
        expect(pairRequested, "Overflow Pair must target this shell instance");

        const auto beforeStage = bandSnapshot(processor);
        shell.beginCopilotProposal("more air");
        expect(shell.hasCopilotProposal());
        auto* source = findDescendantWithID<juce::Label>(
            shell, "emberV2CopilotSource");
        auto* input = findDescendantWithID<juce::TextEditor>(
            shell, "emberV2PhraseInput");
        auto* apply = findDescendantWithID<juce::Button>(shell, "emberV2Apply");
        expect(source != nullptr && source->isVisible());
        expect(input != nullptr && input->isReadOnly() && input->getText() == "more air");
        expect(apply != nullptr && ! apply->isVisible(),
               "Apply stays hidden while the planner is running");

        const std::string planHash(64, 'a');
        expect(shell.stageCopilotPlan("more air", plan, planHash));
        expect(sameBands(beforeStage, bandSnapshot(processor)),
               "preview must not write processor bands");
        expect(apply != nullptr && apply->isVisible());

        beginTest("only the local Apply gesture commits the displayed proposal");
        int applyRequests = 0;
        std::string reviewedHash;
        shell.onCopilotProposalApply = [&](const std::string& hash)
        {
            ++applyRequests;
            reviewedHash = hash;
            const auto adjustments = processor.getSemanticEngine().adjustmentsFromPlan(plan);
            return processor.applySemanticAdjustments(
                adjustments,
                AIEqualizerAudioProcessor::SemanticApplyPolicy::RequireCompletePlan).complete();
        };
        if (apply != nullptr && apply->onClick)
            apply->onClick();
        shell.completeApplyForTests();
        expectEquals(applyRequests, 1);
        expectEquals(juce::String(reviewedHash), juce::String(planHash));
        expect(! shell.hasCopilotProposal());
        expect(! sameBands(beforeStage, bandSnapshot(processor)),
               "Apply must commit the reviewed ghost vector");
        bool hasSemanticBand = false;
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
            hasSemanticBand = hasSemanticBand || processor.isSemanticManagedBand(i);
        expect(hasSemanticBand,
               "Copilot Apply must preserve canonical Semantic provenance");

        beginTest("Reject clears the proposal without touching processor bands");
        const auto beforeReject = bandSnapshot(processor);
        int rejected = 0;
        shell.onCopilotProposalRejected = [&] { ++rejected; };
        shell.beginCopilotProposal("more air");
        expect(shell.stageCopilotPlan("more air", plan, planHash));
        auto* reject = findDescendantWithID<juce::Button>(
            shell, "emberV2CopilotReject");
        expect(reject != nullptr && reject->isVisible());
        if (reject != nullptr && reject->onClick)
            reject->onClick();
        expectEquals(rejected, 1);
        expect(! shell.hasCopilotProposal());
        expect(sameBands(beforeReject, bandSnapshot(processor)));
    }
};

static EmberCopilotUiTest emberCopilotUiTest;

#endif
