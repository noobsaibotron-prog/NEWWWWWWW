#include <juce_gui_basics/juce_gui_basics.h>

#include "../GUI/BandRadialMenu.h"

#include <cstdint>
#include <cstring>
#include <vector>

/**
 * Integration coverage for BandRadialMenu against its public API.
 *
 * The graph owns command execution; this suite drives the same surface
 * AdvancedSpectrumDisplay uses: calculateLayout, open/close, advanceAnimation,
 * marking gestures, and the root/submenu item ids encoded in Command.
 *
 * Item size (66x29) and the 10 px graph inset are the published layout
 * contract from BandRadialMenu::calculateLayout — not test hooks.
 */
class BandRadialMenuTest final : public juce::UnitTest
{
public:
    BandRadialMenuTest()
        : juce::UnitTest("Band Radial Menu", "Integration") {}

    void runTest() override
    {
        juce::ignoreUnused(juce::MessageManager::getInstance());

        testLayoutDeterminism();
        testRootAndSubmenuItemCounts();
        testPillsStayInsideReducedGraphBounds();
        testOpenCloseOnClosedOnce();
        testStationaryReleaseLeavesMenuOpen();
        testMarkingNonDestructiveAction();
        testInitialMarkingTowardDeleteIsBlocked();
    }

private:
    static constexpr float kItemWidth = 66.0f;
    static constexpr float kItemHeight = 29.0f;
    static constexpr float kGraphMargin = 10.0f;
    static constexpr int kRootItemCount = 6;
    static constexpr int kSubmenuItemCount = 7;
    static constexpr int kDeleteItemIndex = 5;
    static constexpr int kShapeItemIndex = 0;
    static constexpr int kResetGainItemIndex = 3;

    static juce::Rectangle<float> graphBounds()
    {
        return { 0.0f, 0.0f, 1200.0f, 480.0f };
    }

    static juce::Point<float> graphCentre()
    {
        return graphBounds().getCentre();
    }

    static bool layoutsEqual(const BandRadialMenu::LayoutResult& a,
                             const BandRadialMenu::LayoutResult& b)
    {
        if (a.count != b.count)
            return false;
        {
            std::uint32_t bitsA = 0, bitsB = 0;
            std::memcpy(&bitsA, &a.centreAngleRadians, sizeof(float));
            std::memcpy(&bitsB, &b.centreAngleRadians, sizeof(float));
            if (bitsA != bitsB)
                return false;
        }

        for (int i = 0; i < static_cast<int>(a.centres.size()); ++i)
            if (a.centres[static_cast<size_t>(i)] != b.centres[static_cast<size_t>(i)])
                return false;

        return true;
    }

    static void advanceUntilActive(BandRadialMenu& menu)
    {
        int guard = 0;
        while (menu.isAnimating() && guard++ < 64)
            menu.advanceAnimation();
    }

    static void advanceUntilClosed(BandRadialMenu& menu)
    {
        int guard = 0;
        while (menu.isOpen() && guard++ < 64)
            menu.advanceAnimation();
    }

    static void openAt(BandRadialMenu& menu,
                       juce::Point<float> anchor,
                       juce::Rectangle<float> bounds,
                       std::function<void(BandRadialMenu::Command)> onCommand,
                       std::function<void()> onClosed)
    {
        menu.open(anchor,
                  bounds,
                  juce::Colour(0xfff2a72d),
                  true,
                  false,
                  std::move(onCommand),
                  std::move(onClosed));
    }

    static bool markToward(BandRadialMenu& menu,
                           juce::Point<float> origin,
                           juce::Point<float> target)
    {
        menu.beginMarkingGesture(origin);
        return menu.endMarkingGesture(target);
    }

    void openSubmenuViaKeyboard(BandRadialMenu& menu)
    {
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),
               "Right arrow selects the first root item (SHAPE)");
        expect(menu.keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),
               "Return opens the filter submenu");
    }

    void expectPillsInsideSafeBounds(const BandRadialMenu::LayoutResult& layout,
                                     juce::Rectangle<float> allowedBounds,
                                     const juce::String& label)
    {
        const auto safe = allowedBounds.reduced(kGraphMargin);
        expect(layout.count > 0, label + ": layout produced no items");

        for (int i = 0; i < layout.count; ++i)
        {
            const auto pill = juce::Rectangle<float>(kItemWidth, kItemHeight)
                                  .withCentre(layout.centres[static_cast<size_t>(i)]);
            expect(pill.getX() >= safe.getX() - 1.0e-4f,
                   label + " item " + juce::String(i) + " overflowed left");
            expect(pill.getY() >= safe.getY() - 1.0e-4f,
                   label + " item " + juce::String(i) + " overflowed top");
            expect(pill.getRight() <= safe.getRight() + 1.0e-4f,
                   label + " item " + juce::String(i) + " overflowed right");
            expect(pill.getBottom() <= safe.getBottom() + 1.0e-4f,
                   label + " item " + juce::String(i) + " overflowed bottom");
        }
    }

    void testLayoutDeterminism()
    {
        beginTest("calculateLayout is deterministic for identical inputs");

        const auto bounds = graphBounds();
        const auto anchor = juce::Point<float>(118.0f, 36.0f);

        const auto a = BandRadialMenu::calculateLayout(anchor, bounds, kRootItemCount,
                                                       kItemWidth, kItemHeight);
        const auto b = BandRadialMenu::calculateLayout(anchor, bounds, kRootItemCount,
                                                       kItemWidth, kItemHeight);
        expect(layoutsEqual(a, b));

        const auto c = BandRadialMenu::calculateLayout(anchor, bounds, kSubmenuItemCount,
                                                       kItemWidth, kItemHeight);
        const auto d = BandRadialMenu::calculateLayout(anchor, bounds, kSubmenuItemCount,
                                                       kItemWidth, kItemHeight);
        expect(layoutsEqual(c, d));
        expect(a.count != c.count);
    }

    void testRootAndSubmenuItemCounts()
    {
        beginTest("Root menu has 6 items and the filter submenu has 7");

        const auto bounds = graphBounds();
        const auto anchor = graphCentre();

        const auto rootLayout = BandRadialMenu::calculateLayout(anchor, bounds, kRootItemCount,
                                                                kItemWidth, kItemHeight);
        const auto subLayout = BandRadialMenu::calculateLayout(anchor, bounds, kSubmenuItemCount,
                                                               kItemWidth, kItemHeight);
        expectEquals(rootLayout.count, kRootItemCount);
        expectEquals(subLayout.count, kSubmenuItemCount);

        // Live root: six marking targets match the public Command ids
        // (SHAPE opens the submenu, DELETE is blocked on the initial gesture).
        {
            std::vector<BandRadialMenu::Command> commands;
            BandRadialMenu menu;
            openAt(menu, anchor, bounds,
                   [&](BandRadialMenu::Command c) { commands.push_back(c); },
                   {});
            advanceUntilActive(menu);

            expect(markToward(menu, anchor, rootLayout.centres[static_cast<size_t>(kShapeItemIndex)]));
            expect(commands.empty(), "SHAPE must open the submenu without emitting a command");
            expect(menu.isOpen());
        }

        const BandRadialMenu::CommandType rootCommands[] = {
            BandRadialMenu::CommandType::toggleEnabled,
            BandRadialMenu::CommandType::toggleSolo,
            BandRadialMenu::CommandType::resetGain,
            BandRadialMenu::CommandType::resetBand
        };

        for (int i = 0; i < 4; ++i)
        {
            std::vector<BandRadialMenu::Command> commands;
            BandRadialMenu menu;
            openAt(menu, anchor, bounds,
                   [&](BandRadialMenu::Command c) { commands.push_back(c); },
                   {});
            advanceUntilActive(menu);

            const int itemIndex = i + 1;
            expect(markToward(menu, anchor, rootLayout.centres[static_cast<size_t>(itemIndex)]));
            expectEquals(static_cast<int>(commands.size()), 1,
                         "Root item " + juce::String(itemIndex) + " must fire exactly one command");
            if (! commands.empty())
                expect(commands.front().type == rootCommands[i],
                       "Root item " + juce::String(itemIndex) + " command type mismatch");
        }

        for (int filter = 0; filter < kSubmenuItemCount; ++filter)
        {
            std::vector<BandRadialMenu::Command> commands;
            BandRadialMenu menu;
            openAt(menu, anchor, bounds,
                   [&](BandRadialMenu::Command c) { commands.push_back(c); },
                   {});
            advanceUntilActive(menu);
            openSubmenuViaKeyboard(menu);
            advanceUntilActive(menu);

            const auto liveSub = BandRadialMenu::calculateLayout(anchor, bounds, kSubmenuItemCount,
                                                                 kItemWidth, kItemHeight);
            expectEquals(liveSub.count, kSubmenuItemCount);
            expect(markToward(menu, anchor, liveSub.centres[static_cast<size_t>(filter)]));
            expectEquals(static_cast<int>(commands.size()), 1,
                         "Submenu item " + juce::String(filter) + " must fire exactly one command");
            if (! commands.empty())
            {
                expect(commands.front().type == BandRadialMenu::CommandType::setFilterType);
                expectEquals(commands.front().value, filter);
            }
        }
    }

    void testPillsStayInsideReducedGraphBounds()
    {
        beginTest("Pills stay inside a 1200x480 graph reduced by 10 px at centre, edges and corners");

        const auto bounds = graphBounds();
        const float x0 = bounds.getX();
        const float y0 = bounds.getY();
        const float x1 = bounds.getRight();
        const float y1 = bounds.getBottom();
        const float xm = bounds.getCentreX();
        const float ym = bounds.getCentreY();
        const float inset = 12.0f;

        const juce::Point<float> anchors[] = {
            { xm, ym },
            { xm, y0 + inset },
            { x1 - inset, ym },
            { xm, y1 - inset },
            { x0 + inset, ym },
            { x0 + inset, y0 + inset },
            { x1 - inset, y0 + inset },
            { x1 - inset, y1 - inset },
            { x0 + inset, y1 - inset }
        };

        const char* labels[] = {
            "centre", "top edge", "right edge", "bottom edge", "left edge",
            "top-left", "top-right", "bottom-right", "bottom-left"
        };

        for (int a = 0; a < static_cast<int>(sizeof(anchors) / sizeof(anchors[0])); ++a)
        {
            const auto root = BandRadialMenu::calculateLayout(anchors[a], bounds, kRootItemCount,
                                                              kItemWidth, kItemHeight);
            const auto sub = BandRadialMenu::calculateLayout(anchors[a], bounds, kSubmenuItemCount,
                                                             kItemWidth, kItemHeight);
            expectEquals(root.count, kRootItemCount);
            expectEquals(sub.count, kSubmenuItemCount);
            expectPillsInsideSafeBounds(root, bounds, juce::String(labels[a]) + " root");
            expectPillsInsideSafeBounds(sub, bounds, juce::String(labels[a]) + " submenu");
        }
    }

    void testOpenCloseOnClosedOnce()
    {
        beginTest("open, advance until active, close, advance until closed, onClosed once");

        int closedCount = 0;
        BandRadialMenu menu;
        openAt(menu, graphCentre(), graphBounds(), {}, [&] { ++closedCount; });

        expect(menu.isOpen());
        expect(menu.isAnimating());

        advanceUntilActive(menu);
        expect(menu.isOpen());
        expect(! menu.isAnimating());
        expectEquals(closedCount, 0);

        menu.close();
        expect(menu.isOpen());
        expect(menu.isAnimating());

        advanceUntilClosed(menu);
        expect(! menu.isOpen());
        expect(! menu.isAnimating());
        expectEquals(closedCount, 1);

        expect(! menu.advanceAnimation());
        expect(! menu.advanceAnimation());
        expectEquals(closedCount, 1);
    }

    void testStationaryReleaseLeavesMenuOpen()
    {
        beginTest("Stationary right-release fires no command and leaves the menu open");

        std::vector<BandRadialMenu::Command> commands;
        int closedCount = 0;
        BandRadialMenu menu;
        const auto anchor = graphCentre();
        openAt(menu, anchor, graphBounds(),
               [&](BandRadialMenu::Command c) { commands.push_back(c); },
               [&] { ++closedCount; });
        advanceUntilActive(menu);

        expect(menu.isMarkingGestureActive() == false);
        menu.beginMarkingGesture(anchor);
        expect(menu.isMarkingGestureActive());
        expect(menu.endMarkingGesture(anchor) == false);
        expect(commands.empty());
        expect(menu.isOpen());
        expect(! menu.isMarkingGestureActive());
        expectEquals(closedCount, 0);
    }

    void testMarkingNonDestructiveAction()
    {
        beginTest("Marking toward a non-destructive action fires one command and closes");

        std::vector<BandRadialMenu::Command> commands;
        int closedCount = 0;
        BandRadialMenu menu;
        const auto bounds = graphBounds();
        const auto anchor = graphCentre();
        const auto layout = BandRadialMenu::calculateLayout(anchor, bounds, kRootItemCount,
                                                            kItemWidth, kItemHeight);

        openAt(menu, anchor, bounds,
               [&](BandRadialMenu::Command c) { commands.push_back(c); },
               [&] { ++closedCount; });
        advanceUntilActive(menu);

        expect(markToward(menu, anchor, layout.centres[static_cast<size_t>(kResetGainItemIndex)]));
        expectEquals(static_cast<int>(commands.size()), 1);
        if (! commands.empty())
            expect(commands.front().type == BandRadialMenu::CommandType::resetGain);

        expect(menu.isOpen());
        expect(menu.isAnimating());
        advanceUntilClosed(menu);
        expect(! menu.isOpen());
        expectEquals(closedCount, 1);
    }

    void testInitialMarkingTowardDeleteIsBlocked()
    {
        beginTest("Initial marking toward DELETE neither executes nor closes");

        std::vector<BandRadialMenu::Command> commands;
        int closedCount = 0;
        BandRadialMenu menu;
        const auto bounds = graphBounds();
        const auto anchor = graphCentre();
        const auto layout = BandRadialMenu::calculateLayout(anchor, bounds, kRootItemCount,
                                                            kItemWidth, kItemHeight);

        openAt(menu, anchor, bounds,
               [&](BandRadialMenu::Command c) { commands.push_back(c); },
               [&] { ++closedCount; });
        advanceUntilActive(menu);

        expect(markToward(menu, anchor, layout.centres[static_cast<size_t>(kDeleteItemIndex)]) == false);
        expect(commands.empty());
        expect(menu.isOpen());
        expect(! menu.isAnimating());
        expectEquals(closedCount, 0);
    }
};

static BandRadialMenuTest bandRadialMenuTest;
