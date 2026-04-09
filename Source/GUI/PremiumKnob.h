#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"
#include <cmath>

/**
 * PremiumKnob
 * A compact rotary knob with a metallic 3D look, animated glow and smooth repaint.
 * - Self contained (no external LookAndFeel)
 * - 60 FPS repaint for glow pulse
 * - Designed for small footprints (32–48 px)
 */
class PremiumKnob : public juce::Slider, private juce::Timer
{
public:
    PremiumKnob() { init(); }
    explicit PremiumKnob(const juce::String& /*labelText*/) { init(); }

    ~PremiumKnob() override = default;

    /** Set the accent color for this knob's value arc. */
    void setAccentColour(juce::Colour c) { accent = c; bgCache = {}; repaint(); }

    void paint(juce::Graphics& g) override
    {
        using C = ModernLookAndFeel::Colors;
        const auto bounds = getLocalBounds().toFloat();
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float radius = size * 0.5f;
        const juce::Point<float> centre = bounds.getCentre();

        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
        const float angle = startAngle + proportion * (endAngle - startAngle);

        const juce::Colour col = accent.isTransparent() ? C::amber : accent;

        // ── Flat circle body ──
        g.setColour(C::knobInner);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0x10FFFFFF));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // ── Track arc (270 deg) ──
        float arcR = radius * 0.82f;
        float arcThk = juce::jmax(2.0f, radius * 0.1f);
        {
            juce::Path arcBg;
            arcBg.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour(C::knobOuter);
            g.strokePath(arcBg, juce::PathStrokeType(arcThk, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // ── Value arc + glow ──
        if (proportion > 0.005f)
        {
            juce::Path arcVal;
            arcVal.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
            // Glow (wider, transparent)
            g.setColour(col.withAlpha(0.2f));
            g.strokePath(arcVal, juce::PathStrokeType(arcThk + 3.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            g.setColour(col);
            g.strokePath(arcVal, juce::PathStrokeType(arcThk, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        // ── Dot indicator (white) ──
        float dotR = juce::jmax(2.5f, radius * 0.1f);
        float dotX = centre.x + std::sin(angle) * arcR;
        float dotY = centre.y - std::cos(angle) * arcR;
        g.setColour(C::knobPointer);
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

        // ── Center value text (only if knob >= 48px) ──
        if (size >= 48.0f)
        {
            auto val = getValue();
            juce::String text;
            if (val >= 1000.0) text = juce::String(val / 1000.0, 1) + "k";
            else if (val >= 100.0) text = juce::String(static_cast<int>(val));
            else text = juce::String(val, 1);

            g.setColour(C::textPrimary);
            g.setFont(juce::Font(juce::FontOptions().withHeight(juce::jmax(9.0f, size * 0.2f))).withStyle(juce::Font::bold));
            g.drawText(text, bounds, juce::Justification::centred);
        }
    }

private:
    juce::Image bgCache;   // unused — kept for ABI compat
    juce::Colour accent;   // per-knob accent color (default: amber)

    void init()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(300);
    }

    void timerCallback() override { repaint(); }

    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseEnter(e);
        startTimerHz(60);
    }

    void mouseExit(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseExit(e);
        stopTimer();
        repaint();
    }
};

