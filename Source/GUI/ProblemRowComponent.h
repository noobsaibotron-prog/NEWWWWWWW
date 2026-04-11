//==============================================================================
// ProblemRowComponent.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../AI/AIEngine.h"
#include "ModernLookAndFeel.h"

class ProblemRowComponent final : public juce::Component
{
public:
    using Correction = AIEngine::Correction;
    using FixCallback = std::function<void(int)>;

    ProblemRowComponent()
    {
        fixButton.setButtonText("FIX");
        fixButton.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::amber);
        fixButton.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amberBright);
        fixButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF181A22));
        fixButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
        fixButton.onClick = [this]
        {
            if (onFix != nullptr && rowIndex >= 0)
                onFix(rowIndex);
        };

        addAndMakeVisible(fixButton);
        setInterceptsMouseClicks(true, true);
    }

    void configure(const Correction& c, int index, FixCallback cb)
    {
        correction = c;
        rowIndex = index;
        onFix = std::move(cb);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        const bool hovered = isMouseOver(true);
        const auto bg = hovered
            ? ModernLookAndFeel::Colors::bgLighter.withAlpha(0.88f)
            : ModernLookAndFeel::Colors::bgMid.withAlpha(0.86f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds.reduced(0.5f), 8.0f);

        g.setColour(ModernLookAndFeel::Colors::bgLighter.withAlpha(0.95f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        auto content = getLocalBounds().reduced(10, 8);

        auto iconArea = content.removeFromLeft(18);
        auto fixArea  = juce::Rectangle<int>(fixButton.getBounds());
        content.removeFromRight(fixArea.getWidth() + 10);

        auto badgeArea = content.removeFromRight(54);
        auto titleArea = content.removeFromTop(16);
        auto descArea  = content;

        // Warning icon
        g.setColour(getSeverityColour());
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
        g.drawText(juce::String(CharPointer_UTF8(u8"⚠")), iconArea, juce::Justification::centred, false);

        // Title
        g.setColour(ModernLookAndFeel::Colors::textPrimary);
        g.setFont(ModernLookAndFeel::makeTitleFont(12.0f));
        g.drawFittedText(buildTitle(), titleArea.reduced(6, 0), juce::Justification::centredLeft, 1, 0.95f);

        // Description / action
        g.setColour(ModernLookAndFeel::Colors::textSecondary);
        g.setFont(ModernLookAndFeel::makeValueFont(11.0f));
        g.drawFittedText(buildDescription(), descArea.reduced(6, 0), juce::Justification::centredLeft, 2, 0.90f);

        // Severity badge
        auto badge = badgeArea.toFloat().withTrimmedTop(2.0f).withTrimmedBottom(2.0f).reduced(2.0f, 2.0f);
        g.setColour(getSeverityColour().withAlpha(0.18f));
        g.fillRoundedRectangle(badge, 5.0f);
        g.setColour(getSeverityColour().withAlpha(0.85f));
        g.drawRoundedRectangle(badge, 5.0f, 1.0f);

        g.setColour(getSeverityColour().brighter(0.10f));
        g.setFont(ModernLookAndFeel::makeLabelFont(10.0f, true));
        g.drawText(getSeverityText(), badge.toNearestInt(), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 8);
        bounds.removeFromLeft(18); // icon gutter

        auto right = bounds.removeFromRight(82);
        right.removeFromLeft(8); // gutter

        auto buttonArea = right.removeFromRight(52);
        fixButton.setBounds(buttonArea.withTrimmedTop(2).withTrimmedBottom(2));

        // remaining `right` is reserved visually for badge; painted directly
    }

private:
    juce::Colour getSeverityColour() const
    {
        if (correction.severity >= 0.75f)
            return ModernLookAndFeel::Colors::accentRed;

        if (correction.severity >= 0.40f)
            return ModernLookAndFeel::Colors::amber;

        return ModernLookAndFeel::Colors::accentGreen;
    }

    juce::String getSeverityText() const
    {
        if (correction.severity >= 0.75f)
            return "HIGH";

        if (correction.severity >= 0.40f)
            return "MED";

        return "LOW";
    }

    juce::String formatFrequency(float hz) const
    {
        if (hz >= 1000.0f)
            return juce::String(hz / 1000.0f, 1) + " kHz";

        return juce::String(juce::roundToInt(hz)) + " Hz";
    }

    juce::String buildTitle() const
    {
        return AIEngine::getProblemTypeName(correction.type) + " at " + formatFrequency(correction.frequency);
    }

    juce::String buildDescription() const
    {
        juce::String gainText = (correction.suggestedGain >= 0.0f ? "+" : "")
            + juce::String(correction.suggestedGain, 1) + " dB";

        return gainText
             + "  "
             + AIEngine::getFilterTypeName(correction.suggestedFilter).toUpperCase()
             + "  Q "
             + juce::String(correction.suggestedQ, 2);
    }

    Correction correction;
    int rowIndex = -1;
    FixCallback onFix;
    juce::TextButton fixButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProblemRowComponent)
};
