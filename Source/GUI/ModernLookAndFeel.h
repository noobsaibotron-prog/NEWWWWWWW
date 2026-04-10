#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

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
        // === BACKGROUNDS (Liquid Intelligence — Manus desaturated palette) ===
        inline static const juce::Colour bgMain        { 0xFF16161A }; // background principale
        inline static const juce::Colour bgDark        { 0xFF16161A }; // alias → bgMain (legacy)
        inline static const juce::Colour bgMid         { 0xFF1E1E22 }; // pannelli/bottoni
        inline static const juce::Colour bgLight       { 0xFF22242E };
        inline static const juce::Colour bgLighter     { 0xFF2A2C38 };
        inline static const juce::Colour bgPanel       { 0xFF1E1E22 }; // pannelli/bottoni (Manus)

        // === ACCENTS (Amber signature + functional states) ===
        inline static const juce::Colour accentBlue    { 0xFF4A9FD9 }; // tab Semantic, DynEQ (Manus)
        inline static const juce::Colour accentCyan    { 0xFF5BA8E0 }; // spectrum line
        inline static const juce::Colour accentYellow  { 0xFFE8A030 }; // AI amber signature
        inline static const juce::Colour accentGreen   { 0xFF4ADE80 }; // active/ok
        inline static const juce::Colour accentOrange  { 0xFFF4B84A }; // amber bright (hover)
        inline static const juce::Colour accentRed     { 0xFFEF4444 }; // alert/clip
        // AI amber convenience aliases
        inline static const juce::Colour amber         { 0xFFE8A030 };
        inline static const juce::Colour amberBright   { 0xFFF4B84A };
        inline static const juce::Colour amberDim      { 0xFF8B6420 };

        // === TEXT (Liquid Intelligence — Manus cool tint) ===
        inline static const juce::Colour textBright    { 0xFFF0EDE8 };
        inline static const juce::Colour textPrimary   { 0xFFE0E0E8 }; // testo primario (Manus)
        inline static const juce::Colour textSecondary { 0xFF8888A0 }; // testo secondario (Manus)
        inline static const juce::Colour textMuted     { 0xFF56544E };
        inline static const juce::Colour textLabel     { 0xFF8888A0 };

        // === SPECTRUM (cool blue — contrast with warm amber UI) ===
        inline static const juce::Colour spectrumFill  { 0xFF1A3A5A };
        inline static const juce::Colour spectrumLine  { 0xFF3A8AC4 };
        inline static const juce::Colour eqCurve       { 0xFFE0E0E8 }; // curva EQ bianca pura
        inline static const juce::Colour grid          { 0xFF1A1C26 };
        inline static const juce::Colour gridMajor     { 0xFF22242E };

        // === KNOBS (flat minimal arc style) ===
        inline static const juce::Colour knobOuter     { 0xFF2A2C38 }; // track arc
        inline static const juce::Colour knobOuterLight{ 0xFF2A2C38 };
        inline static const juce::Colour knobOuterDark { 0xFF22242E };
        inline static const juce::Colour knobInner     { 0xFF1E1E22 }; // body fill (Manus bgPanel)
        inline static const juce::Colour knobPointer   { 0xFFE0E0E8 }; // dot indicator (Manus textPrimary)
        inline static const juce::Colour knobGrip      { 0xFF2A2C38 }; // unused in new style

        // === BAND COLORS (Liquid Intelligence — Manus desaturati, riassegnati) ===
        inline static const juce::Colour band1         { 0xFFFF8C42 }; // arancio
        inline static const juce::Colour band2         { 0xFF4A9FD9 }; // blu
        inline static const juce::Colour band3         { 0xFF5ED4A0 }; // verde
        inline static const juce::Colour band4         { 0xFFA07FE8 }; // viola
        // Tribunale AIEQ+ direttiva #1: bande 5-8 desaturate -30% saturazione e -10% luminosità
        // per renderle visibilmente subordinate alle prime 4 bande "prime" (Liquid Intelligence).
        inline static const juce::Colour band5         { juce::Colour (0xFFE8D44A).withMultipliedSaturation (0.70f).withMultipliedBrightness (0.90f) }; // yellow subordinato
        inline static const juce::Colour band6         { juce::Colour (0xFF4AD4D4).withMultipliedSaturation (0.70f).withMultipliedBrightness (0.90f) }; // teal subordinato
        inline static const juce::Colour band7         { juce::Colour (0xFFD48A5A).withMultipliedSaturation (0.70f).withMultipliedBrightness (0.90f) }; // terracotta subordinato
        inline static const juce::Colour band8         { juce::Colour (0xFF80C0E0).withMultipliedSaturation (0.70f).withMultipliedBrightness (0.90f) }; // sky subordinato
        
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
        // ── Embed Inter font family (4 weights) as default sans-serif ─────────
        // Note: juce_add_binary_data strips dashes (Inter-Regular.ttf → InterRegular_ttf, no underscore)
        interRegular  = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf,  BinaryData::InterRegular_ttfSize);
        interMedium   = juce::Typeface::createSystemTypefaceFor(BinaryData::InterMedium_ttf,   BinaryData::InterMedium_ttfSize);
        interSemiBold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        interBold     = juce::Typeface::createSystemTypefaceFor(BinaryData::InterBold_ttf,     BinaryData::InterBold_ttfSize);
        setDefaultSansSerifTypeface(interRegular);

        setColour(juce::ResizableWindow::backgroundColourId, Colors::bgMid);
        setColour(juce::Label::textColourId, Colors::textPrimary);
        setColour(juce::TextButton::buttonColourId, Colors::bgLighter);
        // Wave 4A: toggle-on default is AMBER, not blue.
        // accentBlue stays reserved for the Semantic tab / DynEQ knobs only,
        // per the Liquid Intelligence mockup (Tribunale direttiva 4A-3).
        setColour(juce::TextButton::buttonOnColourId, Colors::amber);
        setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
        setColour(juce::ComboBox::backgroundColourId, Colors::bgLight);
        setColour(juce::ComboBox::textColourId, Colors::textPrimary);
        setColour(juce::ComboBox::outlineColourId, Colors::bgLighter);
        setColour(juce::PopupMenu::backgroundColourId, Colors::bgLight);
        setColour(juce::PopupMenu::textColourId, Colors::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Colors::amber);
        setColour(juce::Slider::thumbColourId, Colors::amber);
        setColour(juce::Slider::trackColourId, Colors::bgLighter);
    }

    //==========================================================================
    // FONT HELPERS — Liquid Intelligence Inter typography (Tribunal #3)
    //==========================================================================

    /** Label font (UI labels like "FREQ", "GAIN", "Q"). */
    static juce::Font makeLabelFont(float height, bool uppercase = true)
    {
        juce::Font f(juce::FontOptions().withHeight(height));
        if (uppercase)
            f.setExtraKerningFactor(0.12f);  // Tribunal Amendment #3: premium uppercase tracking
        return f;
    }

    /** Value font (numeric readouts). Medium weight. */
    static juce::Font makeValueFont(float height)
    {
        return juce::Font(juce::FontOptions().withHeight(height).withStyle("Medium"));
    }

    /** Section title font. SemiBold + premium uppercase kerning. */
    static juce::Font makeTitleFont(float height)
    {
        juce::Font f(juce::FontOptions().withHeight(height).withStyle("SemiBold"));
        f.setExtraKerningFactor(0.12f);  // Tribunal Amendment #3
        return f;
    }

    /** Heading font. Bold + premium uppercase kerning. */
    static juce::Font makeHeadingFont(float height)
    {
        juce::Font f(juce::FontOptions().withHeight(height).withStyle("Bold"));
        f.setExtraKerningFactor(0.12f);  // Tribunal Amendment #3
        return f;
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
            setColour(juce::TextButton::buttonOnColourId, Colors::amber);
            setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
            setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
            setColour(juce::ComboBox::backgroundColourId, Colors::bgLight);
            setColour(juce::ComboBox::textColourId, Colors::textPrimary);
            setColour(juce::Slider::thumbColourId, Colors::amber);
        }
    }

private:
    bool highContrastMode = false;

    // ── Inter font family (embedded via BinaryData) ──────────────────────────
    juce::Typeface::Ptr interRegular, interMedium, interSemiBold, interBold;

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
        if (accentCol == juce::Colours::transparentBlack || accentCol == Colors::accentBlue) // Semantic default accent fallback
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

            // Client-property overrides (Liquid Intelligence: lighter, less toyish)
            float trackH = (float) slider.getProperties().getWithDefault ("trackThickness", 4.0f);
            float thumbR = (float) slider.getProperties().getWithDefault ("thumbRadius",    6.0f);

            float trackY = y + (h - trackH) / 2.0f;

            g.setColour(bgColor);
            g.fillRoundedRectangle((float)x, trackY, (float)w, trackH, trackH * 0.5f);

            float fillW = sliderPos - x;
            if (fillW > 0)
            {
                g.setColour(trackColor);
                g.fillRoundedRectangle((float)x, trackY, fillW, trackH, trackH * 0.5f);
            }

            // Flat thumb with border
            g.setColour(Colors::bgMid);
            g.fillEllipse(sliderPos - thumbR, trackY + trackH/2 - thumbR, thumbR*2, thumbR*2);
            g.setColour(thumbColor);
            g.drawEllipse(sliderPos - thumbR, trackY + trackH/2 - thumbR, thumbR*2, thumbR*2, 1.5f);
        }
    }

    //==========================================================================
    // BUTTON — Wave 4A: Liquid Intelligence amber toggles
    //
    // Every toggle in the header (A/B, PRE/POST/DELTA, AI) now renders with an
    // AMBER fill when active, not the old azure blue. A buttonColourId override
    // on a specific button still takes precedence via the `overrideOn` lookup
    // below (e.g. SOLO stays yellow, BYPASS stays amber-with-glow).
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool hover, bool down) override
    {
        auto bounds = btn.getLocalBounds().toFloat().reduced(1);
        const bool isQualityToggle = (btn.getComponentID() == "qualityToggle");
        // Liquid Intelligence pill-style for all header toggles:
        // rounded corners match the mockup's capsule aesthetic.
        const float corner = isQualityToggle ? 12.0f : 8.0f;

        // Per-button override for the "on" state (set via
        // setColour(TextButton::buttonOnColourId, ...) at construction time).
        // Falls back to amber if nothing specific was set.
        const auto overrideOn = btn.findColour(juce::TextButton::buttonOnColourId);
        juce::Colour onCol = (overrideOn == juce::Colours::transparentBlack)
                              ? Colors::amber
                              : overrideOn;

        juce::Colour base = btn.getToggleState() ? onCol : Colors::bgLighter;
        if (isQualityToggle && !btn.getToggleState())
            base = base.withAlpha(0.85f);
        if (down) base = base.darker(0.18f);
        else if (hover) base = base.brighter(0.15f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, corner);

        // Active state gets an amber glow halo (except when the override color
        // itself is not amber-family — e.g. SOLO's yellow).
        if (btn.getToggleState() && onCol == Colors::amber)
        {
            g.setColour(Colors::amber.withAlpha(hover ? 0.35f : 0.22f));
            g.drawRoundedRectangle(bounds.expanded(1.5f), corner + 1.0f, 1.8f);
        }

        // Outline — amber when active, subtle when off
        auto outline = btn.getToggleState()
            ? onCol.brighter(0.15f).withAlpha(hover ? 0.9f : 0.75f)
            : Colors::bgLighter.brighter(hover ? 0.35f : 0.15f);
        g.setColour(outline);
        g.drawRoundedRectangle(bounds, corner, hover ? 1.5f : 1.2f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool hover, bool down) override
    {
        drawButtonBackground(g, btn, {}, hover, down);

        auto bounds = btn.getLocalBounds().toFloat();
        // Wave 4A: dark text on amber (readable) when toggled on, muted grey when off.
        g.setColour(btn.getToggleState()
                      ? juce::Colour(0xFF181A22)
                      : Colors::textSecondary);
        auto font = juce::Font(juce::FontOptions().withHeight(12.0f));
        font.setBold(true);
        font.setExtraKerningFactor(0.10f);  // small uppercase tracking for pills
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
    juce::Font getLabelFont(juce::Label&) override { return juce::Font(juce::FontOptions().withHeight(12.0f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        auto font = juce::Font(juce::FontOptions().withHeight(12.0f));
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
