#if JUCE_UNIT_TESTS && defined(__APPLE__)   // JUCE_MAC is not defined before the includes

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberApplyCommit.h"
#include "../GUI/Ember/EmberV2Shell.h"

#include <CoreFoundation/CoreFoundation.h>

#include <array>
#include <memory>

/*
 * A typed phrase stays amber until Apply. Nothing else writes the EQ.
 *
 * Seen by Claude in Ableton on 2026-09-22 (build a0033784): the DoD phrase was
 * written into the Specchio field through macOS Accessibility, the plugin
 * window then lost focus, and the amber ghost was found committed although
 * Apply had not been pressed. Reading a0033784 found a single write path —
 * EmberBar Apply / Return -> EmberV2Shell::beginApply -> finishApply ->
 * EmberApply::commitGhosts — and on 2026-09-23 the same sequence with a focus
 * loss did NOT commit in Ableton. The commit came from outside the plugin.
 *
 * This test keeps it that way. It builds the real EmberV2Shell and runs the
 * real macOS message loop, so TextEditor's posted onTextChange / onFocusLost
 * messages, Button::triggerClick and the shell's 45 Hz timer (which drives the
 * 280 ms Apply morph) are delivered as they are in a host.
 *
 * It fails if focus loss, the shell being hidden, or time alone commits the
 * amber ghosts — e.g. EmberBar wiring editor.onFocusLost to onApply.
 *
 * The phrase is set with TextEditor::setText, which is exactly what the
 * editor's AccessibilityTextInterface::setText calls; the editor exposes no
 * accessibility actions, so AXPress/AXConfirm cannot reach onReturnKey.
 *
 * NOT covered: a real window, real keyboard focus (the shell is never on the
 * desktop, so focusLost is called directly and grabKeyboardFocus asserts),
 * Ableton hiding the plugin window, the Copilot proposal path (see
 * EmberCopilotUiTest), and non-mac message loops.
 */
namespace
{
using P = AIEqualizerAudioProcessor;

constexpr const char* kPhrase = "add a peak at 2khz with -4db gain and 2.5 q";

/** Longer than the 280 ms Apply morph: a commit started by an event has landed
    before the next check, so a failure names the event that caused it. */
constexpr int kSettleMs = 400;

/** Runs the real message loop: posted component messages and Timer callbacks. */
void runMessageLoopFor(int milliseconds)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + milliseconds;
    while (juce::Time::getMillisecondCounterHiRes() < end)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, false);
}

std::array<P::BandState, (size_t) P::maxBands> snapshot(const P& p)
{
    std::array<P::BandState, (size_t) P::maxBands> s {};
    for (int i = 0; i < P::maxBands; ++i)
        s[(size_t) i] = p.getBandState(i);
    return s;
}

/** The dirty EQ of the Ableton check: two live bands far from 2 kHz. */
void seedDirtyEq(P& p)
{
    EmberApply::writeLiveCount(p, 2);
    p.setBandGeometry(0,   45.0f, -3.8f, 0.7f, 1, true);  // Low Shelf
    p.setBandGeometry(1, 7900.0f,  3.4f, 1.0f, 2, true);  // Peak
}

template <typename T>
T* findDescendant(juce::Component& root)
{
    for (auto* child : root.getChildren())
    {
        if (auto* typed = dynamic_cast<T*>(child))
            return typed;
        if (auto* deeper = findDescendant<T>(*child))
            return deeper;
    }
    return nullptr;
}

template <typename T>
T* findDescendantWithID(juce::Component& root, const juce::String& id)
{
    for (auto* child : root.getChildren())
    {
        if (child->getComponentID() == id)
            return dynamic_cast<T*>(child);
        if (auto* deeper = findDescendantWithID<T>(*child, id))
            return deeper;
    }
    return nullptr;
}
} // namespace

class EmberPhraseCommitsOnlyOnApplyTest : public juce::UnitTest
{
public:
    EmberPhraseCommitsOnlyOnApplyTest()
        : juce::UnitTest("Ember phrase commits only on Apply", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        P processor;
        processor.prepareToPlay(48000.0, 512);
        seedDirtyEq(processor);
        const auto before = snapshot(processor);
        const int liveBefore = EmberApply::readLiveCount(processor);

        auto shell = std::make_unique<EmberV2Shell>(processor, nullptr);
        shell->setBounds(0, 0, 980, 640);
        shell->setVisible(true);   // components start hidden; an open editor is not

        beginTest("the typed phrase shows one amber Peak 2000 Hz / -4 dB / Q 2.5 and writes nothing");
        auto* bar   = findDescendant<EmberBar>(*shell);
        auto* graph = findDescendant<EmberGraph>(*shell);
        auto* input = findDescendantWithID<juce::TextEditor>(*shell, "emberV2PhraseInput");
        auto* apply = findDescendantWithID<juce::Button>(*shell, "emberV2Apply");
        expect(bar != nullptr && graph != nullptr && input != nullptr && apply != nullptr,
               "shell must contain bar, graph, phrase input and Apply");
        if (bar == nullptr || graph == nullptr || input == nullptr || apply == nullptr)
            return;

        bar->onEnterFrase();   // what a click on the bar or '/' does
        input->setText(kPhrase);
        runMessageLoopFor(kSettleMs);
        expectPending(processor, before, liveBefore, *graph, *input, *apply, "typed");

        beginTest("focus loss does not commit the amber ghosts");
        input->focusLost(juce::Component::focusChangedDirectly);
        runMessageLoopFor(kSettleMs);
        expectPending(processor, before, liveBefore, *graph, *input, *apply, "after focus loss");

        beginTest("hiding and showing the shell does not commit the amber ghosts");
        shell->setVisible(false);
        runMessageLoopFor(kSettleMs);
        shell->setVisible(true);
        runMessageLoopFor(kSettleMs);
        expectPending(processor, before, liveBefore, *graph, *input, *apply, "after hide/show");

        beginTest("time alone does not commit the amber ghosts");
        runMessageLoopFor(1000);   // > 3x the 280 ms Apply morph, at 45 Hz
        expectPending(processor, before, liveBefore, *graph, *input, *apply, "after 1 s");

        beginTest("one Apply commits the same three numbers and keeps the old bands");
        apply->triggerClick();
        runMessageLoopFor(600);
        expectEquals(EmberApply::readLiveCount(processor), liveBefore + 1,
                     "Apply adds exactly one live band");
        const auto added = processor.getBandState(liveBefore);
        expect(added.enabled, "added band is enabled");
        expectEquals(added.type, 2, "added band is a Peak");
        expectWithinAbsoluteError(added.frequency, 2000.0f, 0.5f, "added band Hz");
        expectWithinAbsoluteError(added.gain, -4.0f, 0.01f, "added band dB");
        expectWithinAbsoluteError(added.q, 2.5f, 0.01f, "added band Q");
        for (int i = 0; i < liveBefore; ++i)
            expect(P::bandStatesEquivalent(before[(size_t) i], processor.getBandState(i)),
                   "Apply changed old band " + juce::String(i));
        expect(input->getText().isEmpty(), "Apply clears the phrase");
        expect(graph->getEffectiveGhosts().empty(), "Apply clears the amber ghosts");

        shell.reset();
    }

private:
    void expectPending(P& processor,
                       const std::array<P::BandState, (size_t) P::maxBands>& before,
                       int liveBefore,
                       EmberGraph& graph,
                       juce::TextEditor& input,
                       juce::Button& apply,
                       const juce::String& where)
    {
        expectEquals(EmberApply::readLiveCount(processor), liveBefore,
                     where + ": live band count changed");
        for (int i = 0; i < P::maxBands; ++i)
            expect(P::bandStatesEquivalent(before[(size_t) i], processor.getBandState(i)),
                   where + ": band " + juce::String(i) + " changed");

        expectEquals(input.getText(), juce::String(kPhrase), where + ": phrase text");
        expect(apply.isVisible(), where + ": Apply must still be offered");

        const auto ghosts = graph.getEffectiveGhosts();
        expectEquals((int) ghosts.size(), 1, where + ": amber ghost count");
        if (ghosts.size() != 1)
            return;
        expectEquals(ghosts[0].type, 2, where + ": amber type is Peak");
        expectWithinAbsoluteError(ghosts[0].hz, 2000.0f, 0.5f, where + ": amber Hz");
        expectWithinAbsoluteError(ghosts[0].db, -4.0f, 0.01f, where + ": amber dB");
        expectWithinAbsoluteError(ghosts[0].q, 2.5f, 0.01f, where + ": amber Q");
    }
};

static EmberPhraseCommitsOnlyOnApplyTest emberPhraseCommitsOnlyOnApplyTest;

#endif
