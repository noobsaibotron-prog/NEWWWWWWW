//==============================================================================
// PillComboBox.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"

class PillComboBox : public juce::ComboBox
{
public:
    PillComboBox()
    {
        setLookAndFeel(&pillLookAndFeel);
        setEditableText(false);
        setJustificationType(juce::Justification::centred);
        setTextWhenNothingSelected({});
        setWantsKeyboardFocus(true);

        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF181A22));
        setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        setColour(juce::ComboBox::arrowColourId, ModernLookAndFeel::Colors::textSecondary);
        setColour(juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);
    }

    ~PillComboBox() override
    {
        setLookAndFeel(nullptr);
    }

    void setAccentBorderEnabled(bool enabled) noexcept
    {
        pillLookAndFeel.setAccentBorderEnabled(enabled);
        repaint();
    }

    void setBorderColour(juce::Colour c) noexcept
    {
        pillLookAndFeel.setBorderColour(c);
        repaint();
    }

    void setBackgroundColour(juce::Colour c) noexcept
    {
        setColour(juce::ComboBox::backgroundColourId, c);
        repaint();
    }

private:
    class PillLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void setAccentBorderEnabled(bool enabled) noexcept { accentBorderEnabled = enabled; }
        void setBorderColour(juce::Colour c) noexcept { explicitBorder = c; }

        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int, int, int, int, juce::ComboBox& box) override
        {
            const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
            const float radius = bounds.getHeight() * 0.5f;

            auto bg = box.findColour(juce::ComboBox::backgroundColourId);
            auto border = explicitBorder.isTransparent()
                ? (accentBorderEnabled ? ModernLookAndFeel::Colors::amber.withAlpha(0.60f)
                                       : ModernLookAndFeel::Colors::bgLighter.withAlpha(0.95f))
                : explicitBorder;

            if (box.isMouseOver(true))
                border = border.brighter(0.12f);
            if (box.hasKeyboardFocus(false))
                border = ModernLookAndFeel::Colors::amber.withAlpha(0.90f);
            if (isButtonDown)
                bg = bg.brighter(0.04f);

            g.setColour(bg);
            g.fillRoundedRectangle(bounds, radius);

            g.setColour(border);
            g.drawRoundedRectangle(bounds, radius, 1.0f);

            const float arrowAreaW = juce::jmax(18.0f, bounds.getHeight());
            auto arrowArea = bounds.removeFromRight(arrowAreaW);

            juce::Path arrow;
            const float cx = arrowArea.getCentreX();
            const float cy = arrowArea.getCentreY() + 0.5f;
            arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
            arrow.lineTo(cx + 4.0f, cy - 2.0f);
            arrow.lineTo(cx,        cy + 2.5f);
            arrow.closeSubPath();

            g.setColour(box.findColour(juce::ComboBox::arrowColourId));
            g.fillPath(arrow);
        }

        juce::Font getComboBoxFont(juce::ComboBox&) override
        {
            return ModernLookAndFeel::makeValueFont(12.0f);
        }

        void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
        {
            const int h = box.getHeight();
            const int arrowW = juce::roundToInt(juce::jmax(18.0f, (float) h));
            label.setBounds(12, 0, box.getWidth() - 24 - arrowW, box.getHeight());
            label.setFont(ModernLookAndFeel::makeValueFont(12.0f));
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, box.findColour(juce::ComboBox::textColourId));
        }

        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                               bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                               bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                               const juce::Drawable* icon, const juce::Colour* textColour) override
        {
            juce::LookAndFeel_V4::drawPopupMenuItem(g, area, isSeparator, isActive, isHighlighted, isTicked,
                                                    hasSubMenu, text, shortcutKeyText, icon, textColour);
        }

    private:
        bool accentBorderEnabled = false;
        juce::Colour explicitBorder;
    };

    PillLookAndFeel pillLookAndFeel;
};
