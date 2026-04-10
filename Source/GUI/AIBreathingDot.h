#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"

/**
 * AIBreathingDot — passive 14x14 component.
 *
 * TRIBUNALE AIEQ+ Amendment #2 (BINDING): NO internal juce::Timer.
 * Phase is driven from the parent's existing timerCallback via setPhase(),
 * ensuring a single UI heartbeat across the entire editor.
 *
 * Visual: amber dot 7px diameter with soft amber glow 4px expanded, both
 * modulated by sin(phase) over a 4-second cycle.
 */
class AIBreathingDot : public juce::Component
{
public:
    AIBreathingDot()
    {
        setInterceptsMouseClicks (false, false);
    }

    void setPhase (float newPhase) noexcept
    {
        if (std::abs (newPhase - phase) > 0.001f)
        {
            phase = newPhase;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const float alpha     = 0.4f + 0.3f * std::sin (phase);
        const auto  bounds    = getLocalBounds().toFloat().withSizeKeepingCentre (7.0f, 7.0f);
        const auto  glowBnds  = bounds.expanded (4.0f);

        // Glow first (z-order: halo behind dot)
        g.setColour (ModernLookAndFeel::Colors::amber.withAlpha (alpha * 0.4f));
        g.fillEllipse (glowBnds);

        // Core dot
        g.setColour (ModernLookAndFeel::Colors::amber.withAlpha (alpha));
        g.fillEllipse (bounds);
    }

private:
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AIBreathingDot)
};
