#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <functional>

/**
 * Animated in-graph context menu for analyzer controls.
 *
 * The component owns presentation and interaction only. It never reads or
 * writes processor state after open(): a snapshot configures the check marks
 * and every action is returned to AdvancedSpectrumDisplay through onCommand.
 */
class AnalyzerContextMenu final : public juce::Component
{
public:
    enum class CommandType
    {
        togglePre,
        togglePost,
        toggleDelta,
        setResolution,
        setSpeed,
        setTilt,
        togglePeakHold,
        togglePianoRoll
    };

    struct Command
    {
        CommandType type = CommandType::togglePre;
        int value = 0;
        float floatValue = 0.0f;
    };

    struct State
    {
        bool showPre = false;
        bool showPost = false;
        bool showDelta = false;
        bool peakHold = false;
        bool pianoRoll = false;
        int resolution = 2;
        int speed = 1;
        float tiltDbPerOctave = 4.5f;
    };

    AnalyzerContextMenu()
    {
        setOpaque(false);
        setVisible(false);
        setWantsKeyboardFocus(true);
        setTitle("Analyzer actions");
        setDescription("Context actions for the spectrum analyzer");
    }

    void open(juce::Point<float> pointerAnchor,
              juce::Rectangle<float> allowedGraphBounds,
              State stateSnapshot,
              std::function<void(Command)> commandCallback,
              std::function<void()> closedCallback)
    {
        anchor = pointerAnchor;
        graphBounds = allowedGraphBounds;
        state = stateSnapshot;
        onCommand = std::move(commandCallback);
        onClosed = std::move(closedCallback);
        page = Page::root;
        configurePage();
        animationProgress = 0.0f;
        phase = Phase::opening;
        hoveredItem = -1;
        keyboardItem = -1;
        closeCallbackDelivered = false;
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

    bool isOpen() const noexcept      { return phase != Phase::closed; }
    bool isAnimating() const noexcept { return phase == Phase::opening || phase == Phase::closing; }

    bool advanceAnimation() noexcept
    {
        // Driven by AdvancedSpectrumDisplay's 60 Hz GUI timer while open:
        // 9 opening frames ≈ 150 ms, 8 closing frames ≈ 133 ms.
        if (phase == Phase::opening)
        {
            animationProgress = juce::jmin(1.0f, animationProgress + kOpenStep);
            if (animationProgress >= 1.0f)
                phase = Phase::active;
            repaint();
            return true;
        }

        if (phase == Phase::closing)
        {
            animationProgress = juce::jmax(0.0f, animationProgress - kCloseStep);
            repaint();
            if (animationProgress <= 0.0f)
            {
                phase = Phase::closed;
                setVisible(false);
                deliverClosedCallback();
            }
            return true;
        }

        return false;
    }

    /** 1x1 screen pixel at the click. Do not write `{ screenPos, { 1, 1 } }`:
        JUCE treats that as Rectangle(Point, Point) from the click to (1, 1). */
    static juce::Rectangle<int> nativePopupScreenArea(juce::Point<int> screenPosition)
    {
        return { screenPosition.x, screenPosition.y, 1, 1 };
    }

    static juce::Rectangle<float> calculatePanelBounds(juce::Point<float> menuAnchor,
                                                        juce::Rectangle<float> allowedBounds,
                                                        int numberOfRows,
                                                        float panelWidth = 228.0f,
                                                        float rowHeight = 29.0f)
    {
        const auto safe = allowedBounds.reduced(kGraphMargin);
        const float height = kPanelPadding * 2.0f + kHeaderHeight
                           + rowHeight * static_cast<float>(juce::jmax(1, numberOfRows));

        float x = menuAnchor.x + kAnchorGap;
        if (x + panelWidth > safe.getRight())
            x = menuAnchor.x - kAnchorGap - panelWidth;

        float y = menuAnchor.y - kHeaderHeight * 0.5f;
        x = juce::jlimit(safe.getX(), safe.getRight() - panelWidth, x);
        y = juce::jlimit(safe.getY(), safe.getBottom() - height, y);
        return { x, y, panelWidth, height };
    }

    void paint(juce::Graphics& g) override
    {
        if (! isOpen())
            return;

        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(graphBounds.toNearestInt());

        const float alpha = smoothStep(animationProgress);
        const float eased = easeOutCubic(animationProgress);
        const float scale = 0.96f + 0.04f * eased;
        const auto pivot = panelBounds.getConstrainedPoint(anchor);

        g.setColour(juce::Colours::black.withAlpha(0.055f * alpha));
        g.fillRect(graphBounds);

        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(-pivot.x, -pivot.y)
                           .scaled(scale)
                           .translated(pivot.x, pivot.y));

        g.setColour(juce::Colours::black.withAlpha(0.28f * alpha));
        g.fillRoundedRectangle(panelBounds.translated(0.0f, 2.0f), 9.0f);
        g.setColour(juce::Colour(0xff141720).withAlpha(0.985f * alpha));
        g.fillRoundedRectangle(panelBounds, 9.0f);
        g.setColour(juce::Colour(0xff3b414e).withAlpha(0.85f * alpha));
        g.drawRoundedRectangle(panelBounds, 9.0f, 1.0f);

        const auto accent = juce::Colour(0xfff2a72d);
        g.setColour(accent.withAlpha(0.92f * alpha));
        g.fillRoundedRectangle(panelBounds.getX() + 10.0f,
                               panelBounds.getY(),
                               panelBounds.getWidth() - 20.0f,
                               2.0f,
                               1.0f);

        auto header = panelBounds.reduced(kPanelPadding, kPanelPadding);
        header.setHeight(kHeaderHeight);
        auto headerText = header;
        if (page != Page::root)
        {
            g.setColour(accent.withAlpha(0.82f * alpha));
            g.setFont(backFont);
            g.drawText("<", headerText.removeFromLeft(15.0f).toNearestInt(), juce::Justification::centredLeft);
            headerText.removeFromLeft(3.0f);
        }

        g.setFont(headerFont);
        g.setColour(juce::Colour(0xffaeb4c1).withAlpha(alpha));
        g.drawText(pageTitle(), headerText.toNearestInt(), juce::Justification::centredLeft);

        for (int i = 0; i < itemCount; ++i)
        {
            const float staggerStart = static_cast<float>(i) * kItemStagger;
            const float local = juce::jlimit(0.0f, 1.0f,
                (animationProgress - staggerStart) / juce::jmax(0.01f, 1.0f - staggerStart));
            const float rowAlpha = smoothStep(local);
            const auto row = rowBounds(i);
            const bool hot = i == hoveredItem || i == keyboardItem;

            if (hot)
            {
                g.setColour(accent.withAlpha(0.13f * rowAlpha));
                g.fillRoundedRectangle(row, 5.0f);
                g.setColour(accent.withAlpha(0.50f * rowAlpha));
                g.drawRoundedRectangle(row, 5.0f, 1.0f);
            }

            auto textArea = row.reduced(10.0f, 0.0f);
            g.setFont(itemFont);
            g.setColour((hot ? juce::Colours::white : juce::Colour(0xffd0d4dc)).withAlpha(rowAlpha));
            g.drawFittedText(items[static_cast<size_t>(i)].label,
                             textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.88f);

            if (items[static_cast<size_t>(i)].checked)
            {
                const auto dot = juce::Rectangle<float>(7.0f, 7.0f)
                                     .withCentre({ row.getRight() - 13.0f, row.getCentreY() });
                g.setColour(accent.withAlpha(0.95f * rowAlpha));
                g.fillEllipse(dot);
                g.setColour(accent.withAlpha(0.24f * rowAlpha));
                g.drawEllipse(dot.expanded(3.0f), 1.0f);
            }
            else if (items[static_cast<size_t>(i)].opensSubmenu)
            {
                g.setColour(juce::Colour(0xff8f96a6).withAlpha(rowAlpha));
                g.setFont(chevronFont);
                g.drawText(">", row.withTrimmedLeft(row.getWidth() - 23.0f).toNearestInt(),
                           juce::Justification::centred);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (animationProgress < 0.72f)
            return;

        const int hit = hitTestRow(e.position);
        if (hit != hoveredItem)
        {
            hoveredItem = hit;
            keyboardItem = -1;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredItem != -1)
        {
            hoveredItem = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (animationProgress < 0.72f)
            return;

        const int hit = hitTestRow(e.position);
        if (hit >= 0)
        {
            activateItem(hit);
            return;
        }

        if (page != Page::root && headerBounds().contains(e.position))
        {
            showPage(Page::root);
            return;
        }

        if (! panelBounds.contains(e.position))
            close();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::leftKey)
        {
            if (page != Page::root)
                showPage(Page::root);
            else
                close();
            return true;
        }

        if (key == juce::KeyPress::upKey)
        {
            keyboardItem = keyboardItem < 0 ? itemCount - 1
                                             : (keyboardItem + itemCount - 1) % itemCount;
            hoveredItem = -1;
            repaint();
            return true;
        }

        if (key == juce::KeyPress::downKey)
        {
            keyboardItem = keyboardItem < 0 ? 0 : (keyboardItem + 1) % itemCount;
            hoveredItem = -1;
            repaint();
            return true;
        }

        if (key == juce::KeyPress::rightKey)
        {
            if (keyboardItem >= 0 && keyboardItem < itemCount
                && items[static_cast<size_t>(keyboardItem)].opensSubmenu)
            {
                activateItem(keyboardItem);
                return true;
            }

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
    enum class Page { root, resolution, speed, tilt };

    struct Item
    {
        juce::String label;
        Command command;
        bool checked = false;
        bool opensSubmenu = false;
        Page submenuPage = Page::root;
    };

    static constexpr float kPanelWidth = 228.0f;
    static constexpr float kRowHeight = 29.0f;
    static constexpr float kHeaderHeight = 28.0f;
    static constexpr float kPanelPadding = 7.0f;
    static constexpr float kGraphMargin = 10.0f;
    static constexpr float kAnchorGap = 12.0f;
    static constexpr float kOpenStep = 1.0f / 9.0f;   // 9 frames @ 60 Hz ≈ 150 ms
    static constexpr float kCloseStep = 1.0f / 8.0f;  // 8 frames @ 60 Hz ≈ 133 ms
    static constexpr float kItemStagger = 12.0f / 150.0f; // 12 ms between rows

    static float smoothStep(float value) noexcept
    {
        const float t = juce::jlimit(0.0f, 1.0f, value);
        return t * t * (3.0f - 2.0f * t);
    }

    static float easeOutCubic(float value) noexcept
    {
        const float t = 1.0f - juce::jlimit(0.0f, 1.0f, value);
        return 1.0f - t * t * t;
    }

    const char* pageTitle() const noexcept
    {
        switch (page)
        {
            case Page::root:       return "ANALYZER";
            case Page::resolution: return "FFT RESOLUTION";
            case Page::speed:      return "ANALYZER SPEED";
            case Page::tilt:       return "SPECTRUM TILT";
        }
        return "";
    }

    void configurePage()
    {
        switch (page)
        {
            case Page::root:
                itemCount = 8;
                items[0] = { "Input Spectrum (Pre)",  { CommandType::togglePre }, state.showPre };
                items[1] = { "Output Spectrum (Post)", { CommandType::togglePost }, state.showPost };
                items[2] = { "Delta (Post - Pre)",     { CommandType::toggleDelta }, state.showDelta };
                items[3] = { "FFT Resolution", {}, false, true, Page::resolution };
                items[4] = { "Analyzer Speed", {}, false, true, Page::speed };
                items[5] = { "Spectrum Tilt", {}, false, true, Page::tilt };
                items[6] = { "Peak Hold", { CommandType::togglePeakHold }, state.peakHold };
                items[7] = { "Piano Roll Overlay", { CommandType::togglePianoRoll }, state.pianoRoll };
                break;

            case Page::resolution:
                itemCount = 4;
                items[0] = { "Low (1024)",     { CommandType::setResolution, 0 }, state.resolution == 0 };
                items[1] = { "Medium (2048)",  { CommandType::setResolution, 1 }, state.resolution == 1 };
                items[2] = { "High (4096)",    { CommandType::setResolution, 2 }, state.resolution == 2 };
                items[3] = { "Maximum (8192)", { CommandType::setResolution, 3 }, state.resolution == 3 };
                break;

            case Page::speed:
                itemCount = 3;
                items[0] = { "Fast",   { CommandType::setSpeed, 0 }, state.speed == 0 };
                items[1] = { "Medium", { CommandType::setSpeed, 1 }, state.speed == 1 };
                items[2] = { "Slow",   { CommandType::setSpeed, 2 }, state.speed == 2 };
                break;

            case Page::tilt:
                itemCount = 4;
                items[0] = { "Flat (0 dB/oct)", { CommandType::setTilt, 0, 0.0f },
                             std::abs(state.tiltDbPerOctave) < 0.01f };
                items[1] = { "3 dB/oct", { CommandType::setTilt, 0, 3.0f },
                             std::abs(state.tiltDbPerOctave - 3.0f) < 0.3f };
                items[2] = { "4.5 dB/oct", { CommandType::setTilt, 0, 4.5f },
                             std::abs(state.tiltDbPerOctave - 4.5f) < 0.3f };
                items[3] = { "6 dB/oct", { CommandType::setTilt, 0, 6.0f },
                             std::abs(state.tiltDbPerOctave - 6.0f) < 0.3f };
                break;
        }

        panelBounds = calculatePanelBounds(anchor, graphBounds, itemCount);
    }

    void showPage(Page newPage)
    {
        page = newPage;
        configurePage();
        hoveredItem = -1;
        keyboardItem = -1;
        if (phase != Phase::opening && phase != Phase::closing)
        {
            animationProgress = 1.0f;
            phase = Phase::active;
        }
        repaint();
    }

    void activateItem(int index)
    {
        if (index < 0 || index >= itemCount)
            return;

        const auto item = items[static_cast<size_t>(index)];
        if (item.opensSubmenu)
        {
            showPage(item.submenuPage);
            return;
        }

        if (onCommand)
            onCommand(item.command);
        close();
    }

    juce::Rectangle<float> headerBounds() const
    {
        auto bounds = panelBounds.reduced(kPanelPadding, kPanelPadding);
        bounds.setHeight(kHeaderHeight);
        return bounds;
    }

    juce::Rectangle<float> rowBounds(int index) const
    {
        auto bounds = panelBounds.reduced(kPanelPadding, kPanelPadding);
        bounds.removeFromTop(kHeaderHeight);
        bounds.setY(bounds.getY() + static_cast<float>(index) * kRowHeight);
        bounds.setHeight(kRowHeight);
        return bounds;
    }

    int hitTestRow(juce::Point<float> position) const noexcept
    {
        for (int i = 0; i < itemCount; ++i)
            if (rowBounds(i).contains(position))
                return i;
        return -1;
    }

    void deliverClosedCallback()
    {
        if (! closeCallbackDelivered)
        {
            closeCallbackDelivered = true;
            if (onClosed)
                onClosed();
        }
    }

    Phase phase = Phase::closed;
    Page page = Page::root;
    State state;
    float animationProgress = 0.0f;
    int hoveredItem = -1;
    int keyboardItem = -1;
    int itemCount = 0;
    bool closeCallbackDelivered = false;
    juce::Point<float> anchor;
    juce::Rectangle<float> graphBounds;
    juce::Rectangle<float> panelBounds;
    std::array<Item, 8> items {};
    std::function<void(Command)> onCommand;
    std::function<void()> onClosed;
    juce::Font headerFont { juce::FontOptions().withHeight(10.0f).withStyle("Bold") };
    juce::Font itemFont { juce::FontOptions().withHeight(11.0f) };
    juce::Font backFont { juce::FontOptions().withHeight(14.0f).withStyle("Bold") };
    juce::Font chevronFont { juce::FontOptions().withHeight(13.0f).withStyle("Bold") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerContextMenu)
};
