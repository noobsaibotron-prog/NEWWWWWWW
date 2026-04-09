#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

//==============================================================================
/**
 * TDR Nova Style Look and Feel
 * 
 * - Industrial metallic knobs with grip texture
 * - Clean blue-gray color scheme
 * - Professional control panel layout
 */
class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // High-contrast mode support
    void setHighContrastMode(bool enabled) { highContrastMode = enabled; }
    bool isHighContrastMode() const { return highContrastMode; }
    
    struct Colors
    {
        // === BACKGROUNDS (Liquid Intelligence: warm-tinted deep darks) ===
        inline static const juce::Colour bgDark        { 0xFF0E0F14 };
        inline static const juce::Colour bgMid         { 0xFF181A22 };
        inline static const juce::Colour bgLight       { 0xFF22242E };
        inline static const juce::Colour bgLighter     { 0xFF2A2C38 };
        inline static const juce::Colour bgPanel       { 0xFF1C1E28 };

        // === ACCENTS (Amber signature + functional states) ===
        inline static const juce::Colour accentBlue    { 0xFF60A5FA }; // info/selection
        inline static const juce::Colour accentCyan    { 0xFF5BA8E0 }; // spectrum line
        inline static const juce::Colour accentYellow  { 0xFFE8A030 }; // AI amber signature
        inline static const juce::Colour accentGreen   { 0xFF4ADE80 }; // active/ok
        inline static const juce::Colour accentOrange  { 0xFFF4B84A }; // amber bright (hover)
        inline static const juce::Colour accentRed     { 0xFFEF4444 }; // alert/clip
        // AI amber convenience aliases
        inline static const juce::Colour amber         { 0xFFE8A030 };
        inline static const juce::Colour amberBright   { 0xFFF4B84A };
        inline static const juce::Colour amberDim      { 0xFF8B6420 };

        // === TEXT (warm tint, not pure white) ===
        inline static const juce::Colour textBright    { 0xFFF0EDE8 };
        inline static const juce::Colour textPrimary   { 0xFFE8E6E2 };
        inline static const juce::Colour textSecondary { 0xFF8A8880 };
        inline static const juce::Colour textMuted     { 0xFF56544E };
        inline static const juce::Colour textLabel     { 0xFF8A8880 };

        // === SPECTRUM (cool blue — contrast with warm amber UI) ===
        inline static const juce::Colour spectrumFill  { 0xFF1A3A5A };
        inline static const juce::Colour spectrumLine  { 0xFF3A8AC4 };
        inline static const juce::Colour eqCurve       { 0xFFE0E0E8 };
        inline static const juce::Colour grid          { 0xFF1A1C26 };
        inline static const juce::Colour gridMajor     { 0xFF22242E };

        // === KNOBS (flat minimal arc style) ===
        inline static const juce::Colour knobOuter     { 0xFF2A2C38 }; // track arc
        inline static const juce::Colour knobOuterLight{ 0xFF2A2C38 };
        inline static const juce::Colour knobOuterDark { 0xFF22242E };
        inline static const juce::Colour knobInner     { 0xFF181A22 }; // body fill
        inline static const juce::Colour knobPointer   { 0xFFE8E6E2 }; // dot indicator
        inline static const juce::Colour knobGrip      { 0xFF2A2C38 }; // unused in new style

        // === BAND COLORS (distinct, no amber — amber is reserved for AI) ===
        inline static const juce::Colour band1         { 0xFF4AA8D4 }; // azure
        inline static const juce::Colour band2         { 0xFF5ED4A0 }; // mint
        inline static const juce::Colour band3         { 0xFFD46A8A }; // rose
        inline static const juce::Colour band4         { 0xFFA480E0 }; // lavender
        inline static const juce::Colour band5         { 0xFFE8D44A }; // yellow
        inline static const juce::Colour band6         { 0xFF4AD4D4 }; // teal
        inline static const juce::Colour band7         { 0xFFD48A5A }; // terracotta
        inline static const juce::Colour band8         { 0xFF80C0E0 }; // sky
        
        static juce::Colour getBandColor(int idx) {
            const juce::Colour cols[] = { band1, band2, band3, band4, band5, band6, band7, band8 };
            return cols[idx % 8];
        }
        
        static juce::Colour getSeverity(float s) {
            if (s < 0.33f) return accentGreen;
            if (s < 0.66f) return accentOrange;
            return accentRed;
        }
        
        // High-contrast colors
        inline static const juce::Colour hcBgDark      { 0xFF000000 };
        inline static const juce::Colour hcBgMid       { 0xFF1A1A1A };
        inline static const juce::Colour hcBgLight     { 0xFF2A2A2A };
        inline static const juce::Colour hcTextBright   { 0xFFFFFFFF };
        inline static const juce::Colour hcTextPrimary { 0xFFFFFFFF };
        inline static const juce::Colour hcAccent      { 0xFFFFD700 }; // Gold for high visibility
    };
    
    // Get current color based on high-contrast mode
    juce::Colour getCurrentBgDark() const { return highContrastMode ? Colors::hcBgDark : Colors::bgDark; }
    juce::Colour getCurrentBgMid() const { return highContrastMode ? Colors::hcBgMid : Colors::bgMid; }
    juce::Colour getCurrentBgLight() const { return highContrastMode ? Colors::hcBgLight : Colors::bgLight; }
    juce::Colour getCurrentTextPrimary() const { return highContrastMode ? Colors::hcTextPrimary : Colors::textPrimary; }
    juce::Colour getCurrentTextBright() const { return highContrastMode ? Colors::hcTextBright : Colors::textBright; }
    juce::Colour getCurrentAccent() const { return highContrastMode ? Colors::hcAccent : Colors::accentBlue; }

    ModernLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Colors::bgMid);
        setColour(juce::Label::textColourId, Colors::textPrimary);
        setColour(juce::TextButton::buttonColourId, Colors::bgLighter);
        setColour(juce::TextButton::buttonOnColourId, Colors::accentBlue);
        setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
        setColour(juce::TextButton::textColourOnId, Colors::textBright);
        setColour(juce::ComboBox::backgroundColourId, Colors::bgLight);
        setColour(juce::ComboBox::textColourId, Colors::textPrimary);
        setColour(juce::ComboBox::outlineColourId, Colors::bgLighter);
        setColour(juce::PopupMenu::backgroundColourId, Colors::bgLight);
        setColour(juce::PopupMenu::textColourId, Colors::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Colors::accentBlue);
        setColour(juce::Slider::thumbColourId, Colors::accentBlue);
        setColour(juce::Slider::trackColourId, Colors::bgLighter);
    }
    
    void updateHighContrastColors()
    {
        if (highContrastMode)
        {
            setColour(juce::ResizableWindow::backgroundColourId, Colors::hcBgMid);
            setColour(juce::Label::textColourId, Colors::hcTextPrimary);
            setColour(juce::TextButton::buttonColourId, Colors::hcBgLight);
            setColour(juce::TextButton::buttonOnColourId, Colors::hcAccent);
            setColour(juce::TextButton::textColourOffId, Colors::hcTextPrimary);
            setColour(juce::TextButton::textColourOnId, Colors::hcTextBright);
            setColour(juce::ComboBox::backgroundColourId, Colors::hcBgLight);
            setColour(juce::ComboBox::textColourId, Colors::hcTextPrimary);
            setColour(juce::Slider::thumbColourId, Colors::hcAccent);
        }
        else
        {
            setColour(juce::ResizableWindow::backgroundColourId, Colors::bgMid);
            setColour(juce::Label::textColourId, Colors::textPrimary);
            setColour(juce::TextButton::buttonColourId, Colors::bgLighter);
            setColour(juce::TextButton::buttonOnColourId, Colors::accentBlue);
            setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
            setColour(juce::TextButton::textColourOnId, Colors::textBright);
            setColour(juce::ComboBox::backgroundColourId, Colors::bgLight);
            setColour(juce::ComboBox::textColourId, Colors::textPrimary);
            setColour(juce::Slider::thumbColourId, Colors::accentBlue);
        }
    }

private:
    bool highContrastMode = false;

    //==========================================================================
    // FLAT ARC KNOB — Liquid Intelligence Style
    //==========================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto angle = startAngle + sliderPos * (endAngle - startAngle);

        // Determine accent color: use slider's trackColour if set, else amber
        auto accentCol = slider.findColour(juce::Slider::thumbColourId);
        if (accentCol == juce::Colours::transparentBlack || accentCol == Colors::accentBlue)
            accentCol = Colors::amber;

        // === FLAT CIRCLE BODY ===
        g.setColour(Colors::knobInner);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Subtle border
        g.setColour(juce::Colour(0x10FFFFFF));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // === TRACK ARC (270 deg, background) ===
        float arcR = radius * 0.82f;
        float arcThickness = juce::jmax(2.0f, radius * 0.1f);
        {
            juce::Path arcBg;
            arcBg.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, endAngle, true);
            g.setColour(Colors::knobOuter);
            g.strokePath(arcBg, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // === VALUE ARC (filled portion with glow) ===
        if (sliderPos > 0.005f)
        {
            juce::Path arcVal;
            arcVal.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, angle, true);

            // Glow layer (wider, semi-transparent)
            g.setColour(accentCol.withAlpha(0.2f));
            g.strokePath(arcVal, juce::PathStrokeType(arcThickness + 3.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            // Main arc
            g.setColour(accentCol);
            g.strokePath(arcVal, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        // === DOT INDICATOR (white, on the arc position) ===
        float dotR = juce::jmax(2.5f, radius * 0.1f);
        float dotX = cx + std::sin(angle) * arcR;
        float dotY = cy - std::cos(angle) * arcR;
        g.setColour(Colors::knobPointer);
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    //==========================================================================
    // LINEAR SLIDER
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            // Use per-slider colors if set, otherwise fall back to defaults
            auto trackColor = slider.findColour(juce::Slider::trackColourId);
            auto bgColor    = slider.findColour(juce::Slider::backgroundColourId);
            auto thumbColor = slider.findColour(juce::Slider::thumbColourId);

            // If background wasn't explicitly set, use bgDark
            if (bgColor == juce::Slider().findColour(juce::Slider::backgroundColourId))
                bgColor = Colors::bgDark;
            // If track wasn't explicitly set, use amber
            if (trackColor == juce::Slider().findColour(juce::Slider::trackColourId))
                trackColor = Colors::amber;
            // If thumb wasn't explicitly set, use track color
            if (thumbColor == juce::Slider().findColour(juce::Slider::thumbColourId))
                thumbColor = trackColor;

            float trackH = 4.0f;
            float trackY = y + (h - trackH) / 2.0f;

            g.setColour(bgColor);
            g.fillRoundedRectangle((float)x, trackY, (float)w, trackH, 2.0f);

            float fillW = sliderPos - x;
            if (fillW > 0)
            {
                g.setColour(trackColor);
                g.fillRoundedRectangle((float)x, trackY, fillW, trackH, 2.0f);
            }

            // Flat thumb with border
            float thumbR = 6.0f;
            g.setColour(Colors::bgMid);
            g.fillEllipse(sliderPos - thumbR, trackY + trackH/2 - thumbR, thumbR*2, thumbR*2);
            g.setColour(thumbColor);
            g.drawEllipse(sliderPos - thumbR, trackY + trackH/2 - thumbR, thumbR*2, thumbR*2, 1.5f);
        }
    }

    //==========================================================================
    // BUTTON
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool hover, bool down) override
    {
        auto bounds = btn.getLocalBounds().toFloat().reduced(1);
        const bool isQualityToggle = (btn.getComponentID() == "qualityToggle");
        const float corner = isQualityToggle ? 12.0f : 4.0f;
        
        juce::Colour base = btn.getToggleState() ? Colors::accentBlue : Colors::bgLighter;
        if (isQualityToggle && !btn.getToggleState())
            base = base.withAlpha(0.85f);
        if (down) base = base.darker(0.18f);
        else if (hover) base = base.brighter(0.15f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, corner);

        // Hover glow effect — subtle luminous border
        auto outline = btn.getToggleState()
            ? Colors::accentCyan.withAlpha(hover ? 0.7f : 0.45f)
            : Colors::bgLighter.brighter(hover ? 0.35f : 0.15f);
        g.setColour(outline);
        g.drawRoundedRectangle(bounds, corner, hover ? 1.5f : 1.2f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool hover, bool down) override
    {
        drawButtonBackground(g, btn, {}, hover, down);
        
        auto bounds = btn.getLocalBounds().toFloat();
        g.setColour(btn.getToggleState() ? Colors::textBright : Colors::textSecondary);
        auto font = juce::Font(juce::FontOptions().withHeight(11.0f));
        font.setBold(true);
        g.setFont(font);
        g.drawText(btn.getButtonText(), bounds, juce::Justification::centred);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, w, h).toFloat().reduced(1);
        
        g.setColour(Colors::bgLight);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(Colors::bgLighter);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        juce::Path arrow;
        float ax = w - 14.0f, ay = h / 2.0f;
        arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour(Colors::textSecondary);
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions().withHeight(12.0f)); }
    juce::Font getLabelFont(juce::Label&) override { return juce::Font(juce::FontOptions().withHeight(11.0f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        auto font = juce::Font(juce::FontOptions().withHeight(11.0f));
        font.setBold(true);
        return font;
    }

    //==========================================================================
    // CUSTOM TOOLTIP — premium styled, semi-transparent rounded box
    //==========================================================================
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                           juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        auto font = juce::Font(juce::FontOptions().withHeight(12.0f));
        const int maxWidth = 280;
        juce::AttributedString s;
        s.setJustification(juce::Justification::centredLeft);
        s.append(tipText, font, Colors::textPrimary);

        juce::TextLayout tl;
        tl.createLayout(s, static_cast<float>(maxWidth));

        int w = static_cast<int>(tl.getWidth()) + 18;
        int h = static_cast<int>(tl.getHeight()) + 12;

        int x = screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 8 : screenPos.x + 12;
        int y = screenPos.y + 20;

        return { x, y, w, h };
    }

    void drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, w, h).toFloat();

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.translated(1.5f, 1.5f), 6.0f);

        // Background — semi-transparent dark panel
        g.setColour(juce::Colour(0xF0202030));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Subtle border (amber accent)
        g.setColour(Colors::amber.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        // Text
        g.setColour(Colors::textPrimary);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        g.drawFittedText(text, 9, 5, w - 18, h - 10,
                         juce::Justification::centredLeft, 4);
    }
};
