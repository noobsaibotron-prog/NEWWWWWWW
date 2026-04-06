#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "DesignTokens.h"
#include <cmath>

/**
 * PremiumKnob — Filament Style
 * Themed rotary knob that reads colors from ThemeManager at paint time.
 * Matches the Filament Knob design in ModernLookAndFeel::drawRotarySlider.
 * - Brushed copper/chrome outer ring (knobRing)
 * - Dark glass body (knobBody)
 * - Crimson filament indicator with glow (knobIndicator / knobIndicatorGlow)
 * - 60 FPS repaint for glow pulse on hover
 */
class PremiumKnob : public juce::Slider, private juce::Timer
{
public:
    PremiumKnob() { init(); }
    explicit PremiumKnob(const juce::String& /*labelText*/) { init(); }

    ~PremiumKnob() override = default;

    void paint(juce::Graphics& g) override
    {
        const auto& thm = ThemeManager::getInstance().getTheme();

        const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        const float size   = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float radius = size * 0.5f;
        const juce::Point<float> centre = bounds.getCentre();

        g.fillAll(juce::Colours::transparentBlack);

        // 1. DROP SHADOW
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(centre.x - radius * 0.96f + 1.5f,
                      centre.y - radius * 0.96f + 2.0f,
                      radius * 1.92f, radius * 1.92f);

        // 2. OUTER METAL RING — brushed copper gradient (knobRing / knobBody)
        juce::ColourGradient ringGrad(
            thm.knobRing.brighter(0.15f), centre.x - radius, centre.y - radius,
            thm.knobBody,                 centre.x + radius, centre.y + radius, false);
        ringGrad.addColour(0.45, thm.knobRing);
        g.setGradientFill(ringGrad);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // Subtle rim highlight
        g.setColour(thm.knobRing.withAlpha(0.18f));
        g.drawEllipse(centre.x - radius + 0.5f, centre.y - radius + 0.5f,
                      radius * 2.0f - 1.0f, radius * 2.0f - 1.0f, 1.0f);

        // 3. GLASS BODY — smoked glass effect (tooltipBg / knobBody)
        const float bodyR = radius * 0.78f;
        juce::ColourGradient glassGrad(
            thm.tooltipBg.withAlpha(0.82f), centre.x, centre.y - bodyR,
            thm.knobBody.withAlpha(0.95f),  centre.x, centre.y + bodyR, false);
        g.setGradientFill(glassGrad);
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        // Glass inner shadow
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.drawEllipse(centre.x - bodyR + 1.0f, centre.y - bodyR + 1.0f,
                      bodyR * 2.0f - 2.0f, bodyR * 2.0f - 2.0f, 1.5f);

        // 4. SPECULAR HIGHLIGHT (top-left glass reflection)
        juce::ColourGradient spec(
            juce::Colours::white.withAlpha(0.18f), centre.x - bodyR * 0.3f, centre.y - bodyR * 0.7f,
            juce::Colours::transparentWhite,        centre.x,                centre.y, true);
        g.setGradientFill(spec);
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 0.85f);

        // 5. VALUE ARC (track + filled)
        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
        const float angle      = startAngle + proportion * (endAngle - startAngle);
        const float arcR       = radius * 0.92f;
        const float arcThick   = 2.2f;

        // Background track
        {
            juce::Path arcBg;
            arcBg.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour(thm.bgPrimary.withAlpha(0.8f));
            g.strokePath(arcBg, juce::PathStrokeType(arcThick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        // Filled arc — accent color
        if (proportion > 0.005f)
        {
            juce::Path arcVal;
            arcVal.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour(thm.accent.withAlpha(0.88f));
            g.strokePath(arcVal, juce::PathStrokeType(arcThick, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // 6. FILAMENT INDICATOR — crimson line with glow pulse
        const float t    = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.002);
        const float glow = 0.35f + 0.25f * std::sin(t);
        const float filR = bodyR * 0.88f;

        const float tipX = centre.x + std::cos(angle) * filR;
        const float tipY = centre.y + std::sin(angle) * filR;

        // Shadow line
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(centre.x, centre.y, tipX + 0.5f, tipY + 0.5f, 3.5f);

        // Glow halo
        g.setColour(thm.knobIndicatorGlow.withAlpha(0.25f + glow * 0.15f));
        g.drawLine(centre.x, centre.y, tipX, tipY, 5.5f);

        // Main filament line
        g.setColour(thm.knobIndicator.withMultipliedAlpha(0.88f + glow * 0.12f));
        g.drawLine(centre.x, centre.y, tipX, tipY, 2.2f);

        // Glowing tip dot
        const float dotR = 3.2f;
        g.setColour(thm.knobIndicatorGlow.withAlpha(0.7f + glow * 0.3f));
        g.fillEllipse(tipX - dotR, tipY - dotR, dotR * 2.0f, dotR * 2.0f);

        // 7. CENTER CAP
        juce::ColourGradient capGrad(
            thm.bgSecondary.brighter(0.05f), centre.x, centre.y - radius * 0.25f,
            thm.bgPrimary,                   centre.x, centre.y + radius * 0.25f, false);
        g.setGradientFill(capGrad);
        const float capR = bodyR * 0.28f;
        g.fillEllipse(centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
    }

private:
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
