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

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float radius = size * 0.5f;
        const juce::Point<float> centre = bounds.getCentre();
        const int w = getWidth();
        const int h = getHeight();

        // ── Static background cache ──────────────────────────────────────
        // Shadow, metallic ring, inner body, 12 notches (sin/cos), specular
        // highlight and center cap are all value-independent. Render once
        // into an image; redraw only when the component resizes.
        if (bgCache.isNull() || bgCache.getWidth() != w || bgCache.getHeight() != h)
        {
            bgCache = juce::Image(juce::Image::ARGB, juce::jmax(1, w), juce::jmax(1, h), true);
            juce::Graphics bg(bgCache);

            using C = ModernLookAndFeel::Colors;

            // Shadow
            bg.setColour(juce::Colour(0x33000000));
            bg.fillEllipse(centre.x - radius * 0.52f, centre.y - radius * 0.48f, radius * 1.04f, radius * 1.04f);

            // Outer ring (metallic)
            juce::ColourGradient ringGrad(C::knobOuterLight, centre.x, centre.y - radius,
                                          C::knobOuterDark, centre.x, centre.y + radius, false);
            ringGrad.addColour(0.5, C::knobOuter.brighter(0.3f));
            bg.setGradientFill(ringGrad);
            bg.fillEllipse(centre.x - radius * 0.95f, centre.y - radius * 0.95f, radius * 1.9f, radius * 1.9f);

            // Inner body
            juce::ColourGradient bodyGrad(C::knobInner, centre.x, centre.y - radius * 0.5f,
                                          C::bgDark, centre.x, centre.y + radius * 0.6f, false);
            bodyGrad.addColour(0.4, C::knobGrip);
            bg.setGradientFill(bodyGrad);
            bg.fillEllipse(centre.x - radius * 0.78f, centre.y - radius * 0.78f, radius * 1.56f, radius * 1.56f);

            // Notches (12× sin/cos — computed once, not 60× per second)
            bg.setColour(C::textBright.withAlpha(0.2f));
            for (int i = 0; i < 12; ++i)
            {
                const float a = juce::degreesToRadians(30.0f * i - 90.0f);
                auto p1 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * radius * 0.80f;
                auto p2 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * radius * 0.88f;
                bg.drawLine({ p1, p2 }, 1.1f);
            }

            // Specular highlight
            juce::ColourGradient spec(juce::Colours::white.withAlpha(0.25f), centre.x, centre.y - radius * 0.6f,
                                      juce::Colours::transparentWhite, centre.x, centre.y, true);
            bg.setGradientFill(spec);
            bg.fillEllipse(centre.x - radius * 0.78f, centre.y - radius * 0.78f, radius * 1.56f, radius * 0.8f);

            // Center cap (static)
            juce::ColourGradient capGrad(C::bgDark.brighter(0.05f), centre.x, centre.y - radius * 0.3f,
                                         C::bgLight.darker(0.2f), centre.x, centre.y + radius * 0.3f, false);
            capGrad.addColour(0.6, C::bgDark.darker(0.3f));
            bg.setGradientFill(capGrad);
            bg.fillEllipse(centre.x - radius * 0.25f, centre.y - radius * 0.25f, radius * 0.5f, radius * 0.5f);
        }

        // Blit cached background
        g.drawImageAt(bgCache, 0, 0);

        // ── Dynamic parts (change with value / time) ─────────────────────
        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
        const float angle = startAngle + proportion * (endAngle - startAngle);

        // Value arc (track + filled portion)
        {
            const float arcR = radius * 0.95f;
            const float arcThickness = 2.5f;

            // Background track
            juce::Path arcBg;
            arcBg.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour(ModernLookAndFeel::Colors::bgDark);
            g.strokePath(arcBg, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

            // Filled arc (accent blue)
            if (proportion > 0.005f)
            {
                juce::Path arcVal;
                arcVal.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
                g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.85f));
                g.strokePath(arcVal, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            }
        }

        // Indicator glow pulse
        const float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.002);
        const float glow = 0.35f + 0.25f * std::sin(t);

        // Indicator shadow
        g.setColour(juce::Colour(0x22000000));
        g.drawLine(centre.x, centre.y,
                   centre.x + std::cos(angle) * radius * 0.9f,
                   centre.y + std::sin(angle) * radius * 0.9f, 4.5f);

        // Indicator line
        juce::Colour indicator = juce::Colour(0xFF5fb4ff).withMultipliedAlpha(0.85f + glow * 0.3f);
        g.setColour(indicator);
        g.drawLine(centre.x, centre.y,
                   centre.x + std::cos(angle) * radius * 0.88f,
                   centre.y + std::sin(angle) * radius * 0.88f, 3.0f);

        // Glow tip
        g.setColour(indicator.withMultipliedAlpha(0.65f + glow * 0.35f));
        g.fillEllipse(centre.x + std::cos(angle) * radius * 0.88f - 3.5f,
                      centre.y + std::sin(angle) * radius * 0.88f - 3.5f,
                      7.0f, 7.0f);
    }

private:
    juce::Image bgCache;  // Cached static background (shadow, ring, body, notches, cap)

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

