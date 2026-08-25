#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <functional>
#include <limits>

/**
 * In-graph radial menu for band actions.
 *
 * This component owns presentation and pointer/keyboard interaction only.  It
 * never touches processor state: every command is returned to the graph through
 * onCommand, so the classic PopupMenu and this menu share one command path.
 */
class BandRadialMenu final : public juce::Component
{
public:
    enum class CommandType
    {
        setFilterType,
        toggleEnabled,
        toggleSolo,
        resetGain,
        resetBand,
        deleteBand
    };

    struct Command
    {
        CommandType type = CommandType::resetGain;
        int value = 0;
    };

    struct LayoutResult
    {
        std::array<juce::Point<float>, 7> centres {};
        int count = 0;
        float centreAngleRadians = 0.0f;
    };

    BandRadialMenu()
    {
        setOpaque(false);
        setVisible(false);
        setWantsKeyboardFocus(true);
        setTitle("Band actions");
        setDescription("Context actions for the selected EQ band");
    }

    void open(juce::Point<float> nodeAnchor,
              juce::Rectangle<float> allowedGraphBounds,
              juce::Colour colour,
              bool bandEnabled,
              bool bandSolo,
              std::function<void(Command)> commandCallback,
              std::function<void()> closedCallback)
    {
        anchor = nodeAnchor;
        graphBounds = allowedGraphBounds;
        bandColour = colour;
        onCommand = std::move(commandCallback);
        onClosed = std::move(closedCallback);
        enabled = bandEnabled;
        solo = bandSolo;
        submenu = false;
        configureRootItems();
        animationProgress = 0.0f;
        phase = Phase::opening;
        hoveredItem = -1;
        keyboardItem = -1;
        markingActive = false;
        setVisible(true);
        toFront(false);
        grabKeyboardFocus();
        repaint();
    }

    void close()
    {
        if (phase != Phase::closed && phase != Phase::closing)
        {
            phase = Phase::closing;
            repaint();
        }
    }

    bool isOpen() const noexcept       { return phase != Phase::closed; }
    bool isAnimating() const noexcept  { return phase == Phase::opening || phase == Phase::closing; }
    bool isMarkingGestureActive() const noexcept { return markingActive; }

    /** Called by AdvancedSpectrumDisplay's existing adaptive GUI timer. */
    bool advanceAnimation() noexcept
    {
        if (phase == Phase::opening)
        {
            animationProgress = juce::jmin(1.0f, animationProgress + 0.105f);
            if (animationProgress >= 1.0f)
                phase = Phase::active;
            repaint();
            return true;
        }

        if (phase == Phase::closing)
        {
            animationProgress = juce::jmax(0.0f, animationProgress - 0.14f);
            repaint();
            if (animationProgress <= 0.0f)
            {
                phase = Phase::closed;
                setVisible(false);
                markingActive = false;
                if (onClosed)
                    onClosed();
            }
            return true;
        }

        return false;
    }

    void beginMarkingGesture(juce::Point<float> position) noexcept
    {
        markingOrigin = position;
        markingActive = true;
        markingDistanceReached = false;
    }

    void updateMarkingGesture(juce::Point<float> position)
    {
        if (!markingActive || !isOpen())
            return;

        const auto delta = position - markingOrigin;
        if (delta.getDistanceFromOrigin() < kMarkingThreshold)
        {
            hoveredItem = -1;
            repaint();
            return;
        }

        markingDistanceReached = true;
        const int candidate = closestItemInDirection(position);
        if (candidate != hoveredItem)
        {
            hoveredItem = candidate;
            repaint();
        }
    }

    /**
     * Completes the initial right-drag marking gesture.  A stationary release
     * leaves the menu open.  Delete is intentionally never executable by the
     * initial gesture; it requires an explicit second click or keyboard action.
     */
    bool endMarkingGesture(juce::Point<float> position)
    {
        if (!markingActive)
            return false;

        updateMarkingGesture(position);
        markingActive = false;

        if (!markingDistanceReached || hoveredItem < 0 || hoveredItem >= itemCount)
            return false;

        if (items[static_cast<size_t>(hoveredItem)].command.type == CommandType::deleteBand)
        {
            hoveredItem = -1;
            repaint();
            return false;
        }

        activateItem(hoveredItem);
        return true;
    }

    static LayoutResult calculateLayout(juce::Point<float> menuAnchor,
                                        juce::Rectangle<float> allowedBounds,
                                        int numberOfItems,
                                        float itemWidth = kItemWidth,
                                        float itemHeight = kItemHeight)
    {
        LayoutResult result;
        result.count = juce::jlimit(1, 7, numberOfItems);

        constexpr std::array<float, 8> candidateAngles {
            -juce::MathConstants<float>::halfPi,
             juce::MathConstants<float>::halfPi,
             0.0f,
             juce::MathConstants<float>::pi,
            -juce::MathConstants<float>::pi / 4.0f,
            -3.0f * juce::MathConstants<float>::pi / 4.0f,
             juce::MathConstants<float>::pi / 4.0f,
             3.0f * juce::MathConstants<float>::pi / 4.0f
        };

        const auto safeBounds = allowedBounds.reduced(kGraphMargin);
        const float spread = result.count >= 7
            ? 1.5f * juce::MathConstants<float>::pi
            : 4.0f * juce::MathConstants<float>::pi / 3.0f;
        const float radius = result.count >= 7 ? 106.0f : kMenuRadius;

        float bestCost = std::numeric_limits<float>::max();
        std::array<juce::Point<float>, 7> bestCentres {};

        for (const float centreAngle : candidateAngles)
        {
            float cost = 0.0f;
            std::array<juce::Point<float>, 7> candidateCentres {};

            for (int i = 0; i < result.count; ++i)
            {
                const float unit = result.count == 1
                    ? 0.5f
                    : static_cast<float>(i) / static_cast<float>(result.count - 1);
                const float angle = centreAngle - spread * 0.5f + spread * unit;
                const auto centre = menuAnchor + juce::Point<float>(std::cos(angle), std::sin(angle)) * radius;
                candidateCentres[static_cast<size_t>(i)] = centre;

                const juce::Rectangle<float> itemRect(centre.x - itemWidth * 0.5f,
                                                      centre.y - itemHeight * 0.5f,
                                                      itemWidth,
                                                      itemHeight);
                const float overflowLeft   = juce::jmax(0.0f, safeBounds.getX() - itemRect.getX());
                const float overflowRight  = juce::jmax(0.0f, itemRect.getRight() - safeBounds.getRight());
                const float overflowTop    = juce::jmax(0.0f, safeBounds.getY() - itemRect.getY());
                const float overflowBottom = juce::jmax(0.0f, itemRect.getBottom() - safeBounds.getBottom());
                cost += 1000.0f * (overflowLeft + overflowRight + overflowTop + overflowBottom);
            }

            // Preserve candidate order as the deterministic tie break: the
            // preferred orientation is upward, then downward, right and left.
            if (cost < bestCost)
            {
                bestCost = cost;
                bestCentres = candidateCentres;
                result.centreAngleRadians = centreAngle;
            }
        }

        for (int i = 0; i < result.count; ++i)
        {
            auto centre = bestCentres[static_cast<size_t>(i)];
            centre.x = juce::jlimit(safeBounds.getX() + itemWidth * 0.5f,
                                    safeBounds.getRight() - itemWidth * 0.5f,
                                    centre.x);
            centre.y = juce::jlimit(safeBounds.getY() + itemHeight * 0.5f,
                                    safeBounds.getBottom() - itemHeight * 0.5f,
                                    centre.y);
            result.centres[static_cast<size_t>(i)] = centre;
        }

        return result;
    }

    void paint(juce::Graphics& g) override
    {
        if (!isOpen())
            return;

        const float menuAlpha = smoothStep(animationProgress);
        g.setColour(juce::Colours::black.withAlpha(0.10f * menuAlpha));
        g.fillRect(graphBounds);

        // The selected node remains the visual source of every option.
        const float pulse = 1.0f + 0.10f * std::sin(animationProgress * juce::MathConstants<float>::pi);
        g.setColour(bandColour.withAlpha(0.12f * menuAlpha));
        g.fillEllipse(juce::Rectangle<float>(34.0f, 34.0f).withCentre(anchor).expanded(5.0f * pulse));
        g.setColour(bandColour.withAlpha(0.95f * menuAlpha));
        g.drawEllipse(juce::Rectangle<float>(27.0f, 27.0f).withCentre(anchor), 2.0f);

        for (int i = 0; i < itemCount; ++i)
        {
            const float staggerStart = static_cast<float>(i) * 0.035f;
            const float local = juce::jlimit(0.0f, 1.0f,
                (animationProgress - staggerStart) / juce::jmax(0.01f, 1.0f - staggerStart));
            const float eased = easeOutBack(local);
            const auto centre = anchor + (layout.centres[static_cast<size_t>(i)] - anchor) * eased;
            currentCentres[static_cast<size_t>(i)] = centre;

            const bool hot = i == hoveredItem || i == keyboardItem;
            const bool destructive = items[static_cast<size_t>(i)].command.type == CommandType::deleteBand;
            const auto accent = destructive && hot ? juce::Colour(0xffe35d62) : bandColour;
            auto bounds = juce::Rectangle<float>(kItemWidth, kItemHeight).withCentre(centre);
            if (hot)
                bounds = bounds.expanded(3.0f, 2.0f);

            g.setColour(juce::Colours::black.withAlpha(0.42f * local));
            g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), bounds.getHeight() * 0.5f);
            g.setColour(juce::Colour(0xff171922).withAlpha(0.97f * local));
            g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
            g.setColour(accent.withAlpha((hot ? 0.95f : 0.50f) * local));
            g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, hot ? 1.8f : 1.1f);

            g.setColour((hot ? juce::Colours::white : juce::Colour(0xffc3c6cf)).withAlpha(local));
            g.setFont(itemFont);
            g.drawFittedText(items[static_cast<size_t>(i)].shortLabel,
                             bounds.toNearestInt().reduced(5, 1),
                             juce::Justification::centred,
                             1,
                             0.85f);
        }

        if (submenu)
        {
            g.setColour(juce::Colour(0xff171922).withAlpha(0.96f * menuAlpha));
            g.fillEllipse(juce::Rectangle<float>(26.0f, 26.0f).withCentre(anchor));
            g.setColour(bandColour.withAlpha(0.9f * menuAlpha));
            g.drawEllipse(juce::Rectangle<float>(26.0f, 26.0f).withCentre(anchor), 1.5f);
            g.setColour(juce::Colours::white.withAlpha(0.85f * menuAlpha));
            g.setFont(backFont);
            g.drawText("<", juce::Rectangle<float>(26.0f, 26.0f).withCentre(anchor).toNearestInt(),
                       juce::Justification::centred);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (markingActive)
            return;

        const int hit = hitTestItem(e.position);
        if (hit != hoveredItem)
        {
            hoveredItem = hit;
            keyboardItem = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int hit = hitTestItem(e.position);
        if (hit >= 0)
        {
            activateItem(hit);
            return;
        }

        if (submenu && e.position.getDistanceFrom(anchor) <= 18.0f)
        {
            submenu = false;
            configureRootItems();
            animationProgress = 0.0f;
            phase = Phase::opening;
            repaint();
            return;
        }

        close();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (submenu)
            {
                submenu = false;
                configureRootItems();
                animationProgress = 0.0f;
                phase = Phase::opening;
            }
            else
            {
                close();
            }
            return true;
        }

        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
        {
            keyboardItem = keyboardItem < 0 ? itemCount - 1 : (keyboardItem + itemCount - 1) % itemCount;
            hoveredItem = -1;
            repaint();
            return true;
        }

        if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
        {
            keyboardItem = keyboardItem < 0 ? 0 : (keyboardItem + 1) % itemCount;
            hoveredItem = -1;
            repaint();
            return true;
        }

        if (key == juce::KeyPress::returnKey && keyboardItem >= 0)
        {
            activateItem(keyboardItem);
            return true;
        }

        return false;
    }

private:
    enum class Phase { closed, opening, active, closing };

    struct Item
    {
        juce::String shortLabel;
        juce::String accessibleLabel;
        Command command;
        bool opensSubmenu = false;
    };

    static constexpr float kMenuRadius = 94.0f;
    static constexpr float kItemWidth = 66.0f;
    static constexpr float kItemHeight = 29.0f;
    static constexpr float kGraphMargin = 10.0f;
    static constexpr float kMarkingThreshold = 18.0f;

    static float smoothStep(float value) noexcept
    {
        const float t = juce::jlimit(0.0f, 1.0f, value);
        return t * t * (3.0f - 2.0f * t);
    }

    static float easeOutBack(float value) noexcept
    {
        const float t = juce::jlimit(0.0f, 1.0f, value) - 1.0f;
        constexpr float c1 = 1.12f; // deliberately restrained overshoot
        return 1.0f + (c1 + 1.0f) * t * t * t + c1 * t * t;
    }

    void configureRootItems()
    {
        itemCount = 6;
        items[0] = { "SHAPE", "Filter shape", {}, true };
        items[1] = { enabled ? "OFF" : "ON", enabled ? "Disable band" : "Enable band",
                     { CommandType::toggleEnabled, 0 }, false };
        items[2] = { solo ? "UNSOLO" : "SOLO", solo ? "Unsolo band" : "Solo band",
                     { CommandType::toggleSolo, 0 }, false };
        items[3] = { "0 dB", "Reset gain", { CommandType::resetGain, 0 }, false };
        items[4] = { "RESET", "Reset band", { CommandType::resetBand, 0 }, false };
        items[5] = { "DELETE", "Delete band", { CommandType::deleteBand, 0 }, false };
        layout = calculateLayout(anchor, graphBounds, itemCount);
    }

    void configureFilterItems()
    {
        static constexpr std::array<const char*, 7> labels { "LOW CUT", "LOW SHELF", "PEAK", "HIGH SHELF", "HIGH CUT", "NOTCH", "BAND PASS" };
        static constexpr std::array<const char*, 7> shortLabels { "LC", "LS", "PEAK", "HS", "HC", "NOTCH", "BP" };
        itemCount = 7;
        for (int i = 0; i < itemCount; ++i)
            items[static_cast<size_t>(i)] = { shortLabels[static_cast<size_t>(i)],
                                              labels[static_cast<size_t>(i)],
                                              { CommandType::setFilterType, i },
                                              false };
        layout = calculateLayout(anchor, graphBounds, itemCount);
    }

    void activateItem(int index)
    {
        if (index < 0 || index >= itemCount)
            return;

        const auto item = items[static_cast<size_t>(index)];
        if (item.opensSubmenu)
        {
            submenu = true;
            configureFilterItems();
            hoveredItem = -1;
            keyboardItem = -1;
            animationProgress = 0.0f;
            phase = Phase::opening;
            repaint();
            return;
        }

        if (onCommand)
            onCommand(item.command);
        close();
    }

    int hitTestItem(juce::Point<float> position) const noexcept
    {
        if (animationProgress < 0.55f)
            return -1;

        for (int i = itemCount - 1; i >= 0; --i)
        {
            if (juce::Rectangle<float>(kItemWidth + 8.0f, kItemHeight + 8.0f)
                    .withCentre(currentCentres[static_cast<size_t>(i)])
                    .contains(position))
                return i;
        }
        return -1;
    }

    int closestItemInDirection(juce::Point<float> position) const noexcept
    {
        const auto direction = position - anchor;
        const float length = direction.getDistanceFromOrigin();
        if (length < kMarkingThreshold)
            return -1;

        int best = -1;
        float bestDot = 0.45f;
        for (int i = 0; i < itemCount; ++i)
        {
            auto itemDirection = layout.centres[static_cast<size_t>(i)] - anchor;
            const float itemLength = itemDirection.getDistanceFromOrigin();
            if (itemLength <= 0.0f)
                continue;
            const float dot = (direction.x * itemDirection.x + direction.y * itemDirection.y) / (length * itemLength);
            if (dot > bestDot)
            {
                bestDot = dot;
                best = i;
            }
        }
        return best;
    }

    Phase phase = Phase::closed;
    bool submenu = false;
    bool enabled = true;
    bool solo = false;
    bool markingActive = false;
    bool markingDistanceReached = false;
    float animationProgress = 0.0f;
    int hoveredItem = -1;
    int keyboardItem = -1;
    int itemCount = 0;
    juce::Point<float> anchor;
    juce::Point<float> markingOrigin;
    juce::Rectangle<float> graphBounds;
    juce::Colour bandColour { 0xfff2a72d };
    LayoutResult layout;
    std::array<Item, 7> items {};
    std::array<juce::Point<float>, 7> currentCentres {};
    std::function<void(Command)> onCommand;
    std::function<void()> onClosed;
    juce::Font itemFont { juce::FontOptions().withHeight(9.5f).withStyle("Bold") };
    juce::Font backFont { juce::FontOptions().withHeight(15.0f).withStyle("Bold") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandRadialMenu)
};
