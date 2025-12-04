#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

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
    struct Colors
    {
        // === BACKGROUNDS ===
        inline static const juce::Colour bgDark        { 0xFF16161E };
        inline static const juce::Colour bgMid         { 0xFF1E1E28 };
        inline static const juce::Colour bgLight       { 0xFF282834 };
        inline static const juce::Colour bgLighter     { 0xFF32323E };
        inline static const juce::Colour bgPanel       { 0xFF24242E };
        
        // === ACCENTS ===
        inline static const juce::Colour accentBlue    { 0xFF4A90D9 };
        inline static const juce::Colour accentCyan    { 0xFF5BA8E0 };
        inline static const juce::Colour accentYellow  { 0xFFD4A843 };
        inline static const juce::Colour accentGreen   { 0xFF4CAF50 };
        inline static const juce::Colour accentOrange  { 0xFFFF9800 };
        inline static const juce::Colour accentRed     { 0xFFE53935 };
        
        // === TEXT ===
        inline static const juce::Colour textBright    { 0xFFFFFFFF };
        inline static const juce::Colour textPrimary   { 0xFFD0D0D8 };
        inline static const juce::Colour textSecondary { 0xFF808090 };
        inline static const juce::Colour textMuted     { 0xFF505060 };
        inline static const juce::Colour textLabel     { 0xFF909098 };
        
        // === SPECTRUM ===
        inline static const juce::Colour spectrumFill  { 0xFF4A7DB8 };
        inline static const juce::Colour spectrumLine  { 0xFF6AAAE8 };
        inline static const juce::Colour eqCurve       { 0xFFD0D0D8 };
        inline static const juce::Colour grid          { 0xFF252530 };
        inline static const juce::Colour gridMajor     { 0xFF2A2A38 };
        
        // === KNOBS ===
        inline static const juce::Colour knobOuter     { 0xFF585868 };
        inline static const juce::Colour knobOuterLight{ 0xFF6A6A7A };
        inline static const juce::Colour knobOuterDark { 0xFF404050 };
        inline static const juce::Colour knobInner     { 0xFF2A2A34 };
        inline static const juce::Colour knobPointer   { 0xFFE0E0E8 };
        inline static const juce::Colour knobGrip      { 0xFF404048 };
        
        // === BAND COLORS ===
        inline static const juce::Colour band1         { 0xFFD4A843 };
        inline static const juce::Colour band2         { 0xFF4A90D9 };
        inline static const juce::Colour band3         { 0xFF5BA8E0 };
        inline static const juce::Colour band4         { 0xFF4CAF50 };
        inline static const juce::Colour band5         { 0xFF9C27B0 };
        inline static const juce::Colour band6         { 0xFFE91E63 };
        inline static const juce::Colour band7         { 0xFFFF5722 };
        inline static const juce::Colour band8         { 0xFFD4A843 };
        
        static juce::Colour getBandColor(int idx) {
            const juce::Colour cols[] = { band1, band2, band3, band4, band5, band6, band7, band8 };
            return cols[idx % 8];
        }
        
        static juce::Colour getSeverity(float s) {
            if (s < 0.33f) return accentGreen;
            if (s < 0.66f) return accentOrange;
            return accentRed;
        }
    };

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

    //==========================================================================
    // METALLIC KNOB - TDR Nova Style
    //==========================================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto angle = startAngle + sliderPos * (endAngle - startAngle);

        // === OUTER METALLIC RING ===
        float outerR = radius;
        float innerR = radius * 0.70f;
        
        // 3D metallic gradient (top-left light, bottom-right dark)
        juce::ColourGradient metalGrad(
            Colors::knobOuterLight, cx - outerR * 0.7f, cy - outerR * 0.7f,
            Colors::knobOuterDark, cx + outerR * 0.7f, cy + outerR * 0.7f, false);
        g.setGradientFill(metalGrad);
        g.fillEllipse(cx - outerR, cy - outerR, outerR * 2, outerR * 2);
        
        // Outer highlight ring
        g.setColour(Colors::knobOuterLight.withAlpha(0.4f));
        g.drawEllipse(cx - outerR + 0.5f, cy - outerR + 0.5f, (outerR - 0.5f) * 2, (outerR - 0.5f) * 2, 1.0f);
        
        // Inner shadow
        g.setColour(Colors::knobOuterDark.darker(0.3f));
        g.drawEllipse(cx - innerR - 2, cy - innerR - 2, (innerR + 2) * 2, (innerR + 2) * 2, 2.0f);

        // === GRIP LINES (radial texture) ===
        g.setColour(Colors::knobGrip);
        int numGrips = 32;
        float gripR1 = radius * 0.76f;
        float gripR2 = radius * 0.94f;
        for (int i = 0; i < numGrips; ++i)
        {
            float a = juce::MathConstants<float>::twoPi * i / numGrips;
            float x1 = cx + std::cos(a) * gripR1;
            float y1 = cy + std::sin(a) * gripR1;
            float x2 = cx + std::cos(a) * gripR2;
            float y2 = cy + std::sin(a) * gripR2;
            g.drawLine(x1, y1, x2, y2, 1.2f);
        }

        // === INNER DARK CENTER ===
        juce::ColourGradient innerGrad(
            Colors::knobInner.brighter(0.15f), cx, cy - innerR,
            Colors::knobInner.darker(0.15f), cx, cy + innerR, false);
        g.setGradientFill(innerGrad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2);
        
        // Inner ring highlight
        g.setColour(Colors::bgDark.brighter(0.1f));
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2, 1.5f);

        // === VALUE ARC ===
        float arcR = radius * 0.52f;
        juce::Path arcBg;
        arcBg.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, endAngle, true);
        g.setColour(Colors::bgDark);
        g.strokePath(arcBg, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        
        // Value arc (filled portion)
        if (sliderPos > 0.005f)
        {
            juce::Path arcVal;
            arcVal.addCentredArc(cx, cy, arcR, arcR, 0, startAngle, angle, true);
            g.setColour(Colors::accentBlue);
            g.strokePath(arcVal, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // === POINTER LINE ===
        float ptrInner = innerR * 0.25f;
        float ptrOuter = innerR * 0.75f;
        float px1 = cx + std::sin(angle) * ptrInner;
        float py1 = cy - std::cos(angle) * ptrInner;
        float px2 = cx + std::sin(angle) * ptrOuter;
        float py2 = cy - std::cos(angle) * ptrOuter;
        
        g.setColour(Colors::knobPointer);
        g.drawLine(px1, py1, px2, py2, 2.5f);
        
        // Pointer end dot
        g.fillEllipse(px2 - 3.0f, py2 - 3.0f, 6.0f, 6.0f);
    }

    //==========================================================================
    // LINEAR SLIDER
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            float trackH = 4.0f;
            float trackY = y + (h - trackH) / 2.0f;

            g.setColour(Colors::bgDark);
            g.fillRoundedRectangle((float)x, trackY, (float)w, trackH, 2.0f);

            float fillW = sliderPos - x;
            if (fillW > 0)
            {
                g.setColour(Colors::accentBlue);
                g.fillRoundedRectangle((float)x, trackY, fillW, trackH, 2.0f);
            }

            // Metallic thumb
            float thumbR = 7.0f;
            juce::ColourGradient thumbGrad(
                Colors::knobOuterLight, sliderPos - thumbR, trackY,
                Colors::knobOuterDark, sliderPos + thumbR, trackY + thumbR * 2, false);
            g.setGradientFill(thumbGrad);
            g.fillEllipse(sliderPos - thumbR, trackY + trackH/2 - thumbR, thumbR*2, thumbR*2);
            
            g.setColour(Colors::textBright);
            g.fillEllipse(sliderPos - 2.0f, trackY + trackH/2 - 2.0f, 4.0f, 4.0f);
        }
    }

    //==========================================================================
    // BUTTON
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool hover, bool down) override
    {
        auto bounds = btn.getLocalBounds().toFloat().reduced(1);
        
        juce::Colour base = btn.getToggleState() ? Colors::accentBlue : Colors::bgLighter;
        if (down) base = base.darker(0.15f);
        else if (hover) base = base.brighter(0.08f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        g.setColour(btn.getToggleState() ? Colors::accentCyan.withAlpha(0.4f) : Colors::bgLighter.brighter(0.15f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool hover, bool down) override
    {
        drawButtonBackground(g, btn, {}, hover, down);
        
        auto bounds = btn.getLocalBounds().toFloat();
        g.setColour(btn.getToggleState() ? Colors::textBright : Colors::textSecondary);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
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

    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(12.0f); }
    juce::Font getLabelFont(juce::Label&) override { return juce::Font(11.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(11.0f, juce::Font::bold); }
};
