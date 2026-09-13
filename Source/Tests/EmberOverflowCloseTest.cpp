#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberV2Shell.h"

#include <memory>

/*
 * The overflow panel's Close button gives the space back to the graph.
 *
 * Seen by Marco in Ableton on 2026-09-13 and reproduced by Claude on 2026-09-14:
 * after "..." then Close, graph and meters stayed 280 px narrower with an empty
 * strip on the right. The menu button and Esc called the shell's resized();
 * Close only hid the panel.
 *
 * The test builds the real EmberV2Shell and drives the real header menu callback
 * and the real Close button handler.
 *
 * NOT covered: repaint timing in a host, and the Esc path (already relayouts).
 */
namespace
{
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

juce::TextButton* findButton(juce::Component& root, const juce::String& text)
{
    for (auto* child : root.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText() == text)
                return button;
    return nullptr;
}
} // namespace

class EmberOverflowCloseTest : public juce::UnitTest
{
public:
    EmberOverflowCloseTest() : juce::UnitTest("Ember overflow Close", "Integration") {}

    void runTest() override
    {
        beginTest("Close gives the overflow width back to the graph, like the menu button");

        auto processor = std::make_unique<AIEqualizerAudioProcessor>();
        processor->prepareToPlay(48000.0, 512);
        auto shell = std::make_unique<EmberV2Shell>(*processor, nullptr);
        shell->setSize(1000, 640);

        auto* header   = findDescendant<EmberHeader>(*shell);
        auto* graph    = findDescendant<EmberGraph>(*shell);
        auto* overflow = findDescendant<EmberOverflow>(*shell);
        expect(header != nullptr && graph != nullptr && overflow != nullptr,
               "shell must contain header, graph and overflow");
        if (header == nullptr || graph == nullptr || overflow == nullptr)
            return;

        auto* close = findButton(*overflow, "Close");
        expect(close != nullptr, "overflow must contain a Close button");
        if (close == nullptr)
            return;

        const int fullWidth = graph->getWidth();

        header->onMenu();
        expect(overflow->isVisible(), "menu opens the overflow");
        expectEquals(graph->getWidth(), fullWidth - EmberTokens::overflowW,
                     "open overflow takes its width from the graph");

        close->onClick();
        expect(! overflow->isVisible(), "Close hides the overflow");
        expectEquals(graph->getWidth(), fullWidth,
                     "after Close the graph must regain the overflow width");

        shell.reset();
        processor.reset();
    }
};

static EmberOverflowCloseTest emberOverflowCloseTest;

#endif
