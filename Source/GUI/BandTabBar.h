#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"

/**
 * BandTabBar — 8 band selector tabs (I..VIII), 28px tall.
 * Drawn below header, above spectrum. Each tab shows a dot whose
 * colour indicates AI pending / normal active / inactive state.
 *
 * Liquid Intelligence: click selects that band for editing.
 * Roman numerals + colored dot + amber underline on selected tab.
 */
class BandTabBar : public juce::Component
{
public:
    std::function<void (int bandIndex)> onBandSelected;

    BandTabBar() = default;

    void setSelectedBand (int idx)
    {
        const int clamped = juce::jlimit (0, 7, idx);
        if (clamped != selectedBand)
        {
            selectedBand = clamped;
            repaint();
        }
    }

    int getSelectedBand() const noexcept { return selectedBand; }

    /** Called by the editor timer (no internal timer) to refresh AI pending flags. */
    void setAIPending (int bandIdx, bool pending)
    {
        if (bandIdx >= 0 && bandIdx < 8 && aiPending[(size_t) bandIdx] != pending)
        {
            aiPending[(size_t) bandIdx] = pending;
            repaint();
        }
    }

    void setBandActive (int bandIdx, bool active)
    {
        if (bandIdx >= 0 && bandIdx < 8 && bandActive[(size_t) bandIdx] != active)
        {
            bandActive[(size_t) bandIdx] = active;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using Colors = ModernLookAndFeel::Colors;

        g.fillAll (Colors::bgMain);

        auto area = getLocalBounds().toFloat();
        const float tabW = area.getWidth() / 8.0f;

        const juce::StringArray labels { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" };
        const juce::Colour bandColours[8] = {
            Colors::band1, Colors::band2, Colors::band3, Colors::band4,
            Colors::band5, Colors::band6, Colors::band7, Colors::band8
        };

        for (int i = 0; i < 8; ++i)
        {
            auto tab = juce::Rectangle<float> (area.getX() + (float) i * tabW,
                                                area.getY(),
                                                tabW,
                                                area.getHeight());

            const bool isSel      = (i == selectedBand);
            const bool hasPending = aiPending[(size_t) i];
            const bool isActive   = bandActive[(size_t) i];

            // Tab label (Roman numeral)
            g.setColour (isSel ? Colors::textPrimary
                               : Colors::textSecondary.withAlpha (isActive ? 0.9f : 0.5f));
            g.setFont (ModernLookAndFeel::makeLabelFont (10.0f, true));
            auto labelRect = tab.reduced (0.0f, 4.0f).removeFromTop (tab.getHeight() - 8.0f);
            g.drawText (labels[i], labelRect.withTrimmedRight (14.0f), juce::Justification::centredRight);

            // Coloured dot (5px)
            auto dotBounds = juce::Rectangle<float> (0.0f, 0.0f, 5.0f, 5.0f)
                                  .withCentre ({ tab.getRight() - 8.0f, tab.getCentreY() });
            juce::Colour dotColour;
            if (hasPending)       dotColour = Colors::amber;
            else if (isActive)    dotColour = bandColours[i];
            else                  dotColour = Colors::textSecondary.withAlpha (0.3f);
            g.setColour (dotColour);
            g.fillEllipse (dotBounds);

            // Active selection underline
            if (isSel)
            {
                g.setColour (Colors::amber);
                auto underline = juce::Rectangle<float> (tab.getX() + 4.0f,
                                                          tab.getBottom() - 2.0f,
                                                          tab.getWidth() - 8.0f,
                                                          2.0f);
                g.fillRect (underline);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (getWidth() <= 0)
            return;

        const float tabW = (float) getWidth() / 8.0f;
        const int idx = juce::jlimit (0, 7, (int) ((float) e.x / tabW));
        setSelectedBand (idx);
        if (onBandSelected)
            onBandSelected (idx);
    }

private:
    int selectedBand = 0;
    std::array<bool, 8> aiPending { false, false, false, false, false, false, false, false };
    std::array<bool, 8> bandActive { true,  true,  true,  true,  false, false, false, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandTabBar)
};
