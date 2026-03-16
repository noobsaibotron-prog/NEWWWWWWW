#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
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

        g.fillAll(juce::Colours::transparentBlack);

        // Shadow
        g.setColour(juce::Colour(0x33000000));
        g.fillEllipse(centre.x - radius * 0.52f, centre.y - radius * 0.48f, radius * 1.04f, radius * 1.04f);

        // Outer ring (metallic)
        juce::ColourGradient ringGrad(juce::Colour(0xFF5c6070), centre.x, centre.y - radius,
                                      juce::Colour(0xFF282b34), centre.x, centre.y + radius, false);
        ringGrad.addColour(0.5, juce::Colour(0xFF8a90a0));
        g.setGradientFill(ringGrad);
        g.fillEllipse(centre.x - radius * 0.95f, centre.y - radius * 0.95f, radius * 1.9f, radius * 1.9f);

        // Inner body
        juce::ColourGradient bodyGrad(juce::Colour(0xFF2a2a30), centre.x, centre.y - radius * 0.5f,
                                      juce::Colour(0xFF17181f), centre.x, centre.y + radius * 0.6f, false);
        bodyGrad.addColour(0.4, juce::Colour(0xFF3d424b));
        g.setGradientFill(bodyGrad);
        g.fillEllipse(centre.x - radius * 0.78f, centre.y - radius * 0.78f, radius * 1.56f, radius * 1.56f);

        // Notches
        g.setColour(juce::Colour(0x33ffffff));
        for (int i = 0; i < 12; ++i)
        {
            const float a = juce::degreesToRadians(30.0f * i - 90.0f);
            auto p1 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * radius * 0.80f;
            auto p2 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * radius * 0.88f;
            g.drawLine({ p1, p2 }, 1.1f);
        }

        // Specular highlight
        juce::ColourGradient spec(juce::Colours::white.withAlpha(0.25f), centre.x, centre.y - radius * 0.6f,
                                  juce::Colours::transparentWhite, centre.x, centre.y, true);
        g.setGradientFill(spec);
        g.fillEllipse(centre.x - radius * 0.78f, centre.y - radius * 0.78f, radius * 1.56f, radius * 0.8f);

        // Angle mapping
        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle   = juce::MathConstants<float>::pi * 2.75f;
        const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
        const float angle = startAngle + proportion * (endAngle - startAngle);

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

        // Center cap
        juce::ColourGradient capGrad(juce::Colour(0xFF1b1d22), centre.x, centre.y - radius * 0.3f,
                                     juce::Colour(0xFF292d35), centre.x, centre.y + radius * 0.3f, false);
        capGrad.addColour(0.6, juce::Colour(0xFF0f1015));
        g.setGradientFill(capGrad);
        g.fillEllipse(centre.x - radius * 0.25f, centre.y - radius * 0.25f, radius * 0.5f, radius * 0.5f);
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

