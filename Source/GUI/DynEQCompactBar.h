#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModernLookAndFeel.h"

/**
 * DynEQCompactBar — 40px horizontal strip with OFF/COMP/EXP/GATE radio buttons
 * + value row showing Threshold, Ratio, Attack, Release for the currently
 * selected band. Sits under the band knobs in the left column.
 *
 * Click the "…" area to request opening the full DynamicEQPanel as an overlay
 * (callback onExpandRequested).
 */
class DynEQCompactBar : public juce::Component
{
public:
    enum class Mode { Off, Compress, Expand, Gate };

    std::function<void (Mode)> onModeChanged;
    std::function<void()>      onExpandRequested;

    DynEQCompactBar()
    {
        const char* names[] = { "OFF", "COMP", "EXP", "GATE" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = modeButtons.add (new juce::TextButton (names[i]));
            b->setRadioGroupId (9991);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xFF1E1E22));
            b->setColour (juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amber);
            b->setColour (juce::TextButton::textColourOnId,   juce::Colour (0xFF181A22));
            b->setColour (juce::TextButton::textColourOffId,  ModernLookAndFeel::Colors::textSecondary);
            b->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
            b->setButtonText (names[i]);
            b->onClick = [this, i]
            {
                currentMode = static_cast<Mode> (i);
                if (onModeChanged)
                    onModeChanged (currentMode);
                repaint();
            };
            addAndMakeVisible (b);
        }
        modeButtons[0]->setToggleState (true, juce::dontSendNotification); // OFF default
    }

    void setValues (float thresholdDb, float ratio, float attackMs, float releaseMs)
    {
        if (std::abs (thresholdDb - threshold) > 0.01f
            || std::abs (ratio - ratioValue) > 0.001f
            || std::abs (attackMs - attackValue) > 0.01f
            || std::abs (releaseMs - releaseValue) > 0.01f)
        {
            threshold    = thresholdDb;
            ratioValue   = ratio;
            attackValue  = attackMs;
            releaseValue = releaseMs;
            repaint();
        }
    }

    Mode getCurrentMode() const noexcept { return currentMode; }

    void setCurrentMode (Mode m)
    {
        currentMode = m;
        const int idx = static_cast<int> (m);
        if (idx >= 0 && idx < modeButtons.size())
            modeButtons[idx]->setToggleState (true, juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1A1A1F));

        // 1 px top divider
        g.setColour (ModernLookAndFeel::Colors::textSecondary.withAlpha (0.15f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());

        // Value row (bottom half)
        auto area   = getLocalBounds().toFloat();
        auto valRow = area.removeFromBottom (18.0f).reduced (4.0f, 1.0f);

        g.setFont (ModernLookAndFeel::makeLabelFont (9.0f, true));
        const float cellW = valRow.getWidth() / 4.0f;

        const juce::String labels[4] = { "THR", "RAT", "ATK", "REL" };
        const juce::String values[4] = {
            juce::String (threshold, 1) + " dB",
            juce::String (ratioValue, 2) + ":1",
            juce::String (attackValue, 1) + " ms",
            juce::String (releaseValue, 1) + " ms"
        };

        for (int i = 0; i < 4; ++i)
        {
            auto cell = juce::Rectangle<float> (valRow.getX() + (float) i * cellW,
                                                 valRow.getY(),
                                                 cellW,
                                                 valRow.getHeight());
            g.setColour (ModernLookAndFeel::Colors::textSecondary.withAlpha (0.6f));
            g.drawText (labels[i], cell.removeFromLeft (22.0f),
                        juce::Justification::centredLeft, false);
            g.setColour (ModernLookAndFeel::Colors::textPrimary);
            g.drawText (values[i], cell, juce::Justification::centredLeft, false);
        }
    }

    void resized() override
    {
        auto area   = getLocalBounds();
        auto btnRow = area.removeFromTop (22);

        const int w = btnRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            modeButtons[i]->setBounds (btnRow.removeFromLeft (w).reduced (1));
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (onExpandRequested)
            onExpandRequested();
    }

private:
    juce::OwnedArray<juce::TextButton> modeButtons;
    Mode  currentMode   = Mode::Off;
    float threshold     = -18.0f;
    float ratioValue    = 2.0f;
    float attackValue   = 10.0f;
    float releaseValue  = 100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynEQCompactBar)
};
