#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "DesignTokens.h"

//==============================================================================
/**
 * Premium Rebranding Look and Feel (T5 Iconic)
 * 
 * - Filament Knobs with glass cover and internal glow
 * - Dynamic theme switching (Deep Crimson / Emerald Night)
 * - Professional high-density control panel layout
 */
class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        updateThemeColors();
        
        // Listen for theme changes to update internal LookAndFeel colors
        ThemeManager::getInstance().addListener([this]() {
            updateThemeColors();
        });
    }

    void updateThemeColors()
    {
        auto& thm = ThemeManager::getInstance().getTheme();
        
        setColour(juce::ResizableWindow::backgroundColourId, thm.bgMid);
        setColour(juce::Label::textColourId, thm.textPrimary);
        
        // Buttons
        setColour(juce::TextButton::buttonColourId, thm.bgLighter);
        setColour(juce::TextButton::buttonOnColourId, thm.accent);
        setColour(juce::TextButton::textColourOffId, thm.textPrimary);
        setColour(juce::TextButton::textColourOnId, thm.textBright);
        
        // ComboBox
        setColour(juce::ComboBox::backgroundColourId, thm.bgLight);
        setColour(juce::ComboBox::textColourId, thm.textPrimary);
        setColour(juce::ComboBox::outlineColourId, thm.bgLighter);
        
        // Popup
        setColour(juce::PopupMenu::backgroundColourId, thm.bgLight);
        setColour(juce::PopupMenu::textColourId, thm.textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, thm.accent);
        
        // Slider
        setColour(juce::Slider::thumbColourId, thm.accent);
        setColour(juce::Slider::trackColourId, thm.bgLighter);
        setColour(juce::Slider::rotarySliderFillColourId, thm.accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, thm.bgDark);
    }

    //==========================================================================
    // FILAMENT KNOB - T5 Iconic Style
    //==========================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto angle = startAngle + sliderPos * (endAngle - startAngle);
        
        auto& thm = ThemeManager::getInstance().getTheme();

        // 1. OUTER METAL RING (Brushed Copper/Chrome)
        g.setGradientFill(juce::ColourGradient(
            thm.knobOuterLight, cx - radius, cy - radius,
            thm.knobOuterDark, cx + radius, cy + radius, false));
        g.fillEllipse(bounds);
        
        // Subtle outer rim highlight
        g.setColour(thm.knobOuterLight.withAlpha(0.2f));
        g.drawEllipse(bounds.reduced(0.5f), 1.0f);

        // 2. GLASS COVER (Smoked Glass Effect)
        auto glassBounds = bounds.reduced(radius * 0.15f);
        auto glassRadius = glassBounds.getWidth() / 2.0f;
        
        juce::ColourGradient glassGrad(
            thm.tooltipBg.withAlpha(0.8f), cx, cy - glassRadius,
            thm.tooltipBg.withAlpha(0.95f), cx, cy + glassRadius, false);
        g.setGradientFill(glassGrad);
        g.fillEllipse(glassBounds);
        
        // Glass inner shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawEllipse(glassBounds.reduced(1.0f), 2.0f);

        // 3. INTERNAL FILAMENT (The "Glow" indicator)
        // A thin curved line that looks like a glowing wire inside the glass
        float filamentR = glassRadius * 0.75f;
        juce::Path filament;
        
        // Pointer line (filament)
        float px1 = cx + std::sin(angle) * (filamentR * 0.2f);
        float py1 = cy - std::cos(angle) * (filamentR * 0.2f);
        float px2 = cx + std::sin(angle) * filamentR;
        float py2 = cy - std::cos(angle) * filamentR;
        
        // Outer glow for the filament
        g.setColour(thm.accent.withAlpha(0.4f));
        g.drawLine(px1, py1, px2, py2, 4.0f);
        
        // Core filament (bright)
        g.setColour(thm.textBright);
        g.drawLine(px1, py1, px2, py2, 1.5f);
        
        // Filament tip "hot spot"
        g.setColour(thm.accent);
        g.fillEllipse(px2 - 2.5f, py2 - 2.5f, 5.0f, 5.0f);
        g.setColour(thm.textBright);
        g.fillEllipse(px2 - 1.0f, py2 - 1.0f, 2.0f, 2.0f);

        // 4. VALUE ARC (Internal to glass)
        if (sliderPos > 0.01f)
        {
            float arcR = glassRadius * 0.88f;
            juce::Path arc;
            arc.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, angle, true);
            
            // Glow under the arc
            g.setColour(thm.accent.withAlpha(0.2f));
            g.strokePath(arc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Main arc
            g.setColour(thm.accent.withAlpha(0.6f));
            g.strokePath(arc, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 5. GLASS REFLECTIONS (Top-down lighting)
        juce::Path reflections;
        reflections.addCentredArc(cx, cy, glassRadius * 0.85f, glassRadius * 0.85f, 0, 
                                  juce::MathConstants<float>::pi * 1.1f, 
                                  juce::MathConstants<float>::pi * 1.4f, true);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.strokePath(reflections, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Helper for legacy code that still uses ModernLookAndFeel::Colors
    struct Colors
    {
        static juce::Colour getBandColor(int idx) { return ThemeManager::getInstance().getBandColor(idx); }
    };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModernLookAndFeel)
};
