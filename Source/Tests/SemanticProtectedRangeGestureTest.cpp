#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "../GUI/AdvancedSpectrumDisplay.h"

#include <cmath>

namespace
{
using Processor = AIEqualizerAudioProcessor;

void layoutDisplay (AdvancedSpectrumDisplay& display)
{
    display.setBounds (0, 0, 1200, 500);
}

float hzToX (const juce::Rectangle<float>& graph, float hz)
{
    const float logMin = std::log10 (20.0f);
    const float logMax = std::log10 (20000.0f);
    const float p = (std::log10 (juce::jlimit (20.0f, 20000.0f, hz)) - logMin) / (logMax - logMin);
    return graph.getX() + p * graph.getWidth();
}

juce::MouseEvent makeEvent (juce::Component& component,
                            juce::Point<float> pos,
                            juce::ModifierKeys mods,
                            juce::Point<float> mouseDownPos,
                            bool dragged)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (source, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &component, &component, now, mouseDownPos, now, 1, dragged);
}

juce::ModifierKeys commandLeft()
{
    return juce::ModifierKeys (juce::ModifierKeys::commandModifier
                               | juce::ModifierKeys::leftButtonModifier);
}

juce::ModifierKeys plainLeft()
{
    return juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
}

void disableAllBands (Processor& proc)
{
    proc.setNumActiveBands (0);
}
}

class SemanticProtectedRangeGestureTest final : public juce::UnitTest
{
public:
    SemanticProtectedRangeGestureTest()
        : juce::UnitTest ("Semantic protected range graph gesture", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;

        beginTest ("Cmd/Ctrl+drag on empty graph commits a processor Hz fence");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            disableAllBands (proc);
            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            int changeNotifications = 0;
            display.onUserProtectedRangesChanged = [&] { ++changeNotifications; };
            const auto graph = display.getGraphBoundsF();
            expect (graph.getWidth() > 100.0f);

            const auto from = juce::Point<float> (hzToX (graph, 8000.0f), graph.getCentreY());
            const auto to   = juce::Point<float> (hzToX (graph, 18000.0f), graph.getCentreY());
            expect (graph.contains (from) && graph.contains (to));
            expect (proc.getUserProtectedRanges().empty());

            display.mouseDown (makeEvent (display, from, commandLeft(), from, false));
            expect (display.isDraggingProtect);
            expect (proc.getUserProtectedRanges().empty(),
                    "fence must commit on mouse-up, not during the drag");
            display.mouseDrag (makeEvent (display, to, commandLeft(), from, true));
            expect (proc.getUserProtectedRanges().empty());
            display.mouseUp (makeEvent (display, to, commandLeft(), from, true));

            const auto ranges = proc.getUserProtectedRanges();
            expectEquals ((int) ranges.size(), 1);
            expectEquals (changeNotifications, 1);
            if (! ranges.empty())
            {
                expectWithinAbsoluteError (ranges.front().minFrequencyHz, 8000.0f, 400.0f);
                expectWithinAbsoluteError (ranges.front().maxFrequencyHz, 18000.0f, 400.0f);
            }
            expect (display.semanticIntentMap.protectRegions.empty(),
                    "gesture must not write the Intent Map projection");
            expect (display.semanticIntentMap.phase == EmberUI::SemanticIntentMapPhase::Hidden);
        }

        beginTest ("unmodified empty drag does not create a fence");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            disableAllBands (proc);
            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            int changeNotifications = 0;
            display.onUserProtectedRangesChanged = [&] { ++changeNotifications; };
            const auto graph = display.getGraphBoundsF();
            const auto from = juce::Point<float> (hzToX (graph, 200.0f), graph.getCentreY());
            const auto to   = juce::Point<float> (hzToX (graph, 2000.0f), graph.getCentreY());
            display.mouseDown (makeEvent (display, from, plainLeft(), from, false));
            display.mouseDrag (makeEvent (display, to, plainLeft(), from, true));
            display.mouseUp (makeEvent (display, to, plainLeft(), from, true));
            expect (proc.getUserProtectedRanges().empty());
            expect (! display.isDraggingProtect);
            expectEquals (changeNotifications, 0);
        }

        beginTest ("Cmd/Ctrl+drag on a node still moves the band, not the fence");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            proc.setNumActiveBands (1);
            auto band = proc.getBandState (0);
            band.enabled = true;
            band.frequency = 1000.0f;
            band.gain = 0.0f;
            band.q = 1.0f;
            proc.setBandState (0, band);

            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            int changeNotifications = 0;
            display.onUserProtectedRangesChanged = [&] { ++changeNotifications; };
            const auto graph = display.getGraphBoundsF();
            const auto node = juce::Point<float> (hzToX (graph, 1000.0f), graph.getCentreY());
            const auto dragged = juce::Point<float> (node.x + 80.0f, node.y);
            expectEquals (display.getBandAtPosition (node), 0);

            display.mouseDown (makeEvent (display, node, commandLeft(), node, false));
            expect (display.isDraggingBand);
            expect (! display.isDraggingProtect);
            display.mouseDrag (makeEvent (display, dragged, commandLeft(), node, true));
            display.mouseUp (makeEvent (display, dragged, commandLeft(), node, true));

            expect (proc.getUserProtectedRanges().empty());
            expect (proc.getBandState (0).frequency > 1000.0f);
            expectEquals (changeNotifications, 0);
        }

        beginTest ("Cmd/Ctrl+click on an existing fence removes it");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            disableAllBands (proc);
            AIEQPerceptual::SemanticProtectedRange seeded;
            seeded.minFrequencyHz = 8000.0f;
            seeded.maxFrequencyHz = 18000.0f;
            expect (proc.addUserProtectedRange (seeded));

            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            int changeNotifications = 0;
            display.onUserProtectedRangesChanged = [&] { ++changeNotifications; };
            const auto graph = display.getGraphBoundsF();
            const auto click = juce::Point<float> (hzToX (graph, 12000.0f), graph.getCentreY());
            display.mouseDown (makeEvent (display, click, commandLeft(), click, false));
            display.mouseUp (makeEvent (display, click, commandLeft(), click, false));
            expect (proc.getUserProtectedRanges().empty());
            expectEquals (changeNotifications, 1);
        }

        beginTest ("tiny Cmd/Ctrl+click on empty graph is a no-op");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            disableAllBands (proc);
            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            const auto graph = display.getGraphBoundsF();
            const auto click = juce::Point<float> (hzToX (graph, 400.0f), graph.getCentreY());
            display.mouseDown (makeEvent (display, click, commandLeft(), click, false));
            display.mouseUp (makeEvent (display, click, commandLeft(), click, false));
            expect (proc.getUserProtectedRanges().empty());
        }

        beginTest ("Escape cancels an in-progress fence without publishing a change");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            disableAllBands (proc);
            AdvancedSpectrumDisplay display (proc);
            layoutDisplay (display);
            int changeNotifications = 0;
            display.onUserProtectedRangesChanged = [&] { ++changeNotifications; };
            const auto graph = display.getGraphBoundsF();
            const auto from = juce::Point<float> (hzToX (graph, 500.0f), graph.getCentreY());
            const auto to   = juce::Point<float> (hzToX (graph, 5000.0f), graph.getCentreY());

            display.mouseDown (makeEvent (display, from, commandLeft(), from, false));
            display.mouseDrag (makeEvent (display, to, commandLeft(), from, true));
            expect (display.isDraggingProtect);
            expect (display.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
            expect (! display.isDraggingProtect);
            display.mouseUp (makeEvent (display, to, commandLeft(), from, true));

            expect (proc.getUserProtectedRanges().empty());
            expectEquals (changeNotifications, 0);
        }
    }
};

static SemanticProtectedRangeGestureTest semanticProtectedRangeGestureTest;

#endif
