#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"
#include "../GUI/AdvancedSpectrumDisplay.h"

/**
 * Regression: mouseUp must always end a band-drag session, including the
 * chord where a radial marking gesture consumes the release.
 *
 * mouseDown returns early while isDraggingBand is already true, so a stuck
 * flag makes the original band keep following mouseDrag after the gesture
 * that should have ended it.
 */
class BandDragMouseUpTest final : public juce::UnitTest
{
public:
    BandDragMouseUpTest()
        : juce::UnitTest ("Band drag mouseUp always clears drag", "Integration") {}

    void runTest() override
    {
        juce::ignoreUnused (juce::MessageManager::getInstance());

        testPlainReleaseStopsZombieDrag();
        testRadialChordReleaseStopsZombieDrag();
        testFreshDragWorksAfterRadialChord();
    }

private:
    static constexpr float kStartHz = 80.0f;
    static constexpr float kStartGainDb = 6.0f;

    static juce::MouseEvent makeEvent (juce::Component& component,
                                       juce::Point<float> position,
                                       juce::ModifierKeys modifiers,
                                       bool mouseWasDragged = false)
    {
        const auto now = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(),
                 position,
                 modifiers,
                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                 &component,
                 &component,
                 now,
                 position,
                 now,
                 1,
                 mouseWasDragged };
    }

    static void paintOnce (AdvancedSpectrumDisplay& display)
    {
        juce::Image image (juce::Image::ARGB, display.getWidth(), display.getHeight(), true);
        juce::Graphics g (image);
        display.paint (g);
    }

    static juce::Point<float> findBandNode (AdvancedSpectrumDisplay& display, int band)
    {
        const auto bounds = display.getGraphBoundsF();
        for (float y = bounds.getY(); y <= bounds.getBottom(); y += 2.0f)
            for (float x = bounds.getX(); x <= bounds.getRight(); x += 2.0f)
                if (display.getBandAtPosition ({ x, y }) == band)
                    return { x, y };
        return {};
    }

    struct Fixture
    {
        AIEqualizerAudioProcessor processor;
        AdvancedSpectrumDisplay display;
        juce::Point<float> node;

        Fixture()
            : display (processor)
        {
            processor.prepareToPlay (48000.0, 512);
            processor.setNumActiveBands (1);
            AIEqualizerAudioProcessor::BandState band;
            band.frequency = kStartHz;
            band.gain = kStartGainDb;
            band.q = 1.0f;
            band.type = 2;
            band.enabled = true;
            processor.setBandState (0, band);

            display.setBounds (0, 0, 1200, 500);
            paintOnce (display);
            node = findBandNode (display, 0);
        }

        ~Fixture()
        {
            processor.releaseResources();
        }
    };

    void testPlainReleaseStopsZombieDrag()
    {
        beginTest ("plain mouseUp stops further mouseDrag from moving the band");

        Fixture fx;
        expect (fx.node != juce::Point<float>{}, "node must be hittable after paint");
        expectEquals (fx.display.getBandAtPosition (fx.node), 0);

        fx.display.mouseDown (makeEvent (fx.display, fx.node,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        fx.display.mouseUp (makeEvent (fx.display, fx.node,
                                       juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));

        const float before = fx.processor.getBandState (0).frequency;
        const auto dragged = fx.node + juce::Point<float> (240.0f, 0.0f);
        fx.display.mouseDrag (makeEvent (fx.display, dragged,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         true));
        expectWithinAbsoluteError (fx.processor.getBandState (0).frequency, before, 0.01f,
                                   "released drag must not keep following the pointer");
    }

    void testRadialChordReleaseStopsZombieDrag()
    {
        beginTest ("radial marking mouseUp stops zombie drag of the original band");

        Fixture fx;
        expect (fx.node != juce::Point<float>{});

        fx.display.mouseDown (makeEvent (fx.display, fx.node,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        fx.display.mouseDown (makeEvent (fx.display, fx.node,
                                         juce::ModifierKeys (juce::ModifierKeys::popupMenuClickModifier)));
        fx.display.mouseUp (makeEvent (fx.display, fx.node,
                                       juce::ModifierKeys (juce::ModifierKeys::popupMenuClickModifier)));

        const float before = fx.processor.getBandState (0).frequency;
        const auto dragged = fx.node + juce::Point<float> (240.0f, 0.0f);
        fx.display.mouseDrag (makeEvent (fx.display, dragged,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         true));
        expectWithinAbsoluteError (fx.processor.getBandState (0).frequency, before, 0.01f,
                                   "chord with radial menu must still end the band-drag session");
    }

    void testFreshDragWorksAfterRadialChord()
    {
        beginTest ("after the radial chord, a new drag moves the clicked band not the old one");

        Fixture fx;
        fx.processor.setNumActiveBands (2);
        AIEqualizerAudioProcessor::BandState high;
        high.frequency = 8000.0f;
        high.gain = kStartGainDb;
        high.q = 1.0f;
        high.type = 2;
        high.enabled = true;
        fx.processor.setBandState (1, high);
        paintOnce (fx.display);

        const auto node0 = findBandNode (fx.display, 0);
        const auto node1 = findBandNode (fx.display, 1);
        expect (node0 != juce::Point<float>{});
        expect (node1 != juce::Point<float>{});

        fx.display.mouseDown (makeEvent (fx.display, node0,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        fx.display.mouseDown (makeEvent (fx.display, node0,
                                         juce::ModifierKeys (juce::ModifierKeys::popupMenuClickModifier)));
        fx.display.mouseUp (makeEvent (fx.display, node0,
                                       juce::ModifierKeys (juce::ModifierKeys::popupMenuClickModifier)));

        const float lowBefore = fx.processor.getBandState (0).frequency;
        const float highBefore = fx.processor.getBandState (1).frequency;
        fx.display.mouseDown (makeEvent (fx.display, node1,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)));
        const auto dragged = node1 + juce::Point<float> (-240.0f, 0.0f);
        fx.display.mouseDrag (makeEvent (fx.display, dragged,
                                         juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         true));

        expectWithinAbsoluteError (fx.processor.getBandState (0).frequency, lowBefore, 0.01f,
                                   "the original band must not keep the zombie drag");
        expect (std::abs (fx.processor.getBandState (1).frequency - highBefore) > 10.0f,
                "the newly clicked band must be the one that moves");
    }
};

static BandDragMouseUpTest bandDragMouseUpTest;
