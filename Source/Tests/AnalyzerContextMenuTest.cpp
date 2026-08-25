#include <juce_gui_basics/juce_gui_basics.h>

#include "../GUI/AnalyzerContextMenu.h"

#include <cstdint>
#include <cstring>
#include <vector>

class AnalyzerContextMenuTest final : public juce::UnitTest
{
public:
    AnalyzerContextMenuTest()
        : juce::UnitTest("Analyzer Context Menu", "Integration") {}

    void runTest() override
    {
        juce::ignoreUnused(juce::MessageManager::getInstance());
        testLayoutIsDeterministicAndContained();
        testNativePopupScreenAreaIsOnePixelAtClick();
        testOpenSettlesInShortWindow();
        testOpenCloseCallbackOnce();
        testRenderedPanelAndBackdrop();
        testEscapeClosesRoot();
        testRootCommandViaKeyboard();
        testSubmenuCommandViaKeyboard();
    }

private:
    static juce::Rectangle<float> graphBounds()
    {
        return { 0.0f, 0.0f, 1200.0f, 480.0f };
    }

    static void advanceUntilActive(AnalyzerContextMenu& menu)
    {
        int guard = 0;
        while (menu.isAnimating() && guard++ < 64)
            menu.advanceAnimation();
    }

    static void advanceUntilClosed(AnalyzerContextMenu& menu)
    {
        int guard = 0;
        while (menu.isOpen() && guard++ < 64)
            menu.advanceAnimation();
    }

    static void openMenu(AnalyzerContextMenu& menu,
                         juce::Point<float> anchor,
                         std::function<void(AnalyzerContextMenu::Command)> onCommand,
                         std::function<void()> onClosed)
    {
        AnalyzerContextMenu::State state;
        state.showPre = true;
        state.showPost = true;
        state.resolution = 2;
        state.speed = 1;
        state.tiltDbPerOctave = 4.5f;
        menu.open(anchor, graphBounds(), state, std::move(onCommand), std::move(onClosed));
    }

    void testLayoutIsDeterministicAndContained()
    {
        beginTest("Panel is deterministic and contained at centre, edges and corners");

        const auto bounds = graphBounds();
        const juce::Point<float> anchors[] {
            bounds.getCentre(),
            { 2.0f, 2.0f },
            { bounds.getRight() - 2.0f, 2.0f },
            { bounds.getRight() - 2.0f, bounds.getBottom() - 2.0f },
            { 2.0f, bounds.getBottom() - 2.0f }
        };

        for (const auto anchor : anchors)
        {
            for (const int rows : { 3, 4, 8 })
            {
                const auto first = AnalyzerContextMenu::calculatePanelBounds(anchor, bounds, rows);
                const auto second = AnalyzerContextMenu::calculatePanelBounds(anchor, bounds, rows);
                expect(first == second, "Identical inputs must produce identical bounds");
                expect(bounds.reduced(10.0f).contains(first),
                       "Panel escaped the graph-safe bounds at " + anchor.toString()
                           + " with " + juce::String(rows) + " rows");
            }
        }
    }

    void testNativePopupScreenAreaIsOnePixelAtClick()
    {
        beginTest("Native popup target is a 1x1 screen pixel at the click, not a span to (1,1)");

        const juce::Point<int> click { 640, 240 };
        const auto area = AnalyzerContextMenu::nativePopupScreenArea(click);
        expectEquals(area.getX(), 640);
        expectEquals(area.getY(), 240);
        expectEquals(area.getWidth(), 1);
        expectEquals(area.getHeight(), 1);

        const juce::Rectangle<int> braceTrap { click, { 1, 1 } };
        expect(braceTrap.getX() == 1 && braceTrap.getWidth() > 1,
               "JUCE Rectangle(Point, Point) from the click to (1,1) is the screen-left trap");
    }

    void testOpenSettlesInShortWindow()
    {
        beginTest("Opening settles in nine 60 Hz frames (~150 ms) with no leftover motion");

        AnalyzerContextMenu menu;
        openMenu(menu, graphBounds().getCentre(), {}, {});

        int frames = 0;
        while (menu.isAnimating() && frames++ < 64)
            menu.advanceAnimation();

        expect(frames >= 8 && frames <= 10, "Open duration must stay in the 120-180 ms window");
        expect(menu.isOpen());
        expect(! menu.isAnimating());
    }

    void testOpenCloseCallbackOnce()
    {
        beginTest("Opening and closing animates and calls onClosed exactly once");

        AnalyzerContextMenu menu;
        int closedCount = 0;
        openMenu(menu, graphBounds().getCentre(), {}, [&] { ++closedCount; });

        expect(menu.isOpen());
        expect(menu.isAnimating());
        advanceUntilActive(menu);
        expect(menu.isOpen());
        expect(! menu.isAnimating());

        menu.close();
        advanceUntilClosed(menu);
        expect(! menu.isOpen());
        expectEquals(closedCount, 1);
        expect(! menu.advanceAnimation());
        expectEquals(closedCount, 1);
    }

    void testRenderedPanelAndBackdrop()
    {
        beginTest("Finished animation renders an opaque anchored panel over a restrained backdrop");

        AnalyzerContextMenu menu;
        const auto anchor = graphBounds().getCentre();
        openMenu(menu, anchor, {}, {});
        advanceUntilActive(menu);

        juce::Image image(juce::Image::ARGB,
                          static_cast<int>(graphBounds().getWidth()),
                          static_cast<int>(graphBounds().getHeight()),
                          true);
        juce::Graphics graphics(image);
        menu.paint(graphics);

        const auto panel = AnalyzerContextMenu::calculatePanelBounds(anchor, graphBounds(), 8);
        const auto panelPixel = image.getPixelAt(static_cast<int>(panel.getCentreX()),
                                                  static_cast<int>(panel.getCentreY()));
        const auto backdropPixel = image.getPixelAt(20, 20);

        expect(panelPixel.getAlpha() > 220,
               "The finished panel must be visibly opaque at its anchored centre");
        expect(backdropPixel.getAlpha() > 0 && backdropPixel.getAlpha() < 40,
               "The graph backdrop must be present but visually restrained");
    }

    void testEscapeClosesRoot()
    {
        beginTest("Escape closes the root menu");

        AnalyzerContextMenu menu;
        int closedCount = 0;
        openMenu(menu, graphBounds().getCentre(), {}, [&] { ++closedCount; });
        advanceUntilActive(menu);

        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
        advanceUntilClosed(menu);
        expect(! menu.isOpen());
        expectEquals(closedCount, 1);
    }

    void testRootCommandViaKeyboard()
    {
        beginTest("First root item emits togglePre and closes");

        AnalyzerContextMenu menu;
        std::vector<AnalyzerContextMenu::Command> commands;
        openMenu(menu, graphBounds().getCentre(),
                 [&](AnalyzerContextMenu::Command command) { commands.push_back(command); }, {});
        advanceUntilActive(menu);

        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::downKey)));
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        expectEquals(static_cast<int>(commands.size()), 1);
        if (! commands.empty())
            expect(commands.front().type == AnalyzerContextMenu::CommandType::togglePre);
        expect(menu.isAnimating());
        advanceUntilClosed(menu);
        expect(! menu.isOpen());
    }

    void testSubmenuCommandViaKeyboard()
    {
        beginTest("FFT submenu emits exact resolution choice");

        AnalyzerContextMenu menu;
        std::vector<AnalyzerContextMenu::Command> commands;
        openMenu(menu, graphBounds().getCentre(),
                 [&](AnalyzerContextMenu::Command command) { commands.push_back(command); }, {});
        advanceUntilActive(menu);

        // Root item 3 is FFT Resolution.
        for (int i = 0; i < 4; ++i)
            expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::downKey)));
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        expect(menu.isOpen());
        expect(commands.empty());
        advanceUntilActive(menu);

        // First submenu item is Low (1024).
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::downKey)));
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        expectEquals(static_cast<int>(commands.size()), 1);
        if (! commands.empty())
        {
            expect(commands.front().type == AnalyzerContextMenu::CommandType::setResolution);
            expectEquals(commands.front().value, 0);
        }
        advanceUntilClosed(menu);
        expect(! menu.isOpen());
    }
};

static AnalyzerContextMenuTest analyzerContextMenuTest;
