#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * TDR Nova Style EQ Band Node
 * 
 * - Simple circle with band number
 * - Ring highlight when selected/hovered
 * - Color-coded per band
 */
class EQBandControl : public juce::Component
{
public:
    struct BandParameters
    {
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int filterType = 2;
        bool enabled = true;
    };
    
    std::function<void(int, const BandParameters&)> onParametersChanged;
    std::function<void(int)> onDragStart;
    std::function<void(int)> onDragEnd;

    explicit EQBandControl(int idx) : bandIndex(idx)
    {
        color = ModernLookAndFeel::Colors::getBandColor(idx);
        setRepaintsOnMouseActivity(true);
    }

    void setParameters(const BandParameters& p) { params = p; repaint(); }
    const BandParameters& getParameters() const { return params; }
    int getBandIndex() const { return bandIndex; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 3.0f;

        if (!params.enabled)
        {
            // Disabled: small dim circle with subtle border
            g.setColour(color.withAlpha(0.15f));
            g.fillEllipse(cx - 4, cy - 4, 8, 8);
            g.setColour(color.withAlpha(0.3f));
            g.drawEllipse(cx - 4, cy - 4, 8, 8, 1.0f);
            return;
        }

        // === DROP SHADOW ===
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillEllipse(cx - r + 1.5f, cy - r + 2.0f, r * 2, r * 2);

        // === SELECTION RING (when hovered or dragging) ===
        if (isMouseOver() || dragging)
        {
            // Outer glow
            g.setColour(color.withAlpha(0.15f));
            g.fillEllipse(cx - r - 8, cy - r - 8, (r + 8) * 2, (r + 8) * 2);
            g.setColour(color.withAlpha(0.25f));
            g.fillEllipse(cx - r - 5, cy - r - 5, (r + 5) * 2, (r + 5) * 2);

            g.setColour(color.withAlpha(0.6f));
            g.drawEllipse(cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2, 1.5f);
        }

        // === MAIN CIRCLE — gradient for 3D depth ===
        {
            juce::ColourGradient bodyGrad(
                color.brighter(0.2f), cx, cy - r,
                color.darker(0.2f), cx, cy + r, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
        }

        // Border
        g.setColour(dragging ? color.brighter(0.4f) : color.brighter(0.15f));
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.8f);

        // Inner specular highlight (top-left)
        {
            juce::ColourGradient spec(
                juce::Colours::white.withAlpha(0.28f), cx - r * 0.3f, cy - r * 0.5f,
                juce::Colours::transparentWhite, cx, cy, true);
            g.setGradientFill(spec);
            g.fillEllipse(cx - r * 0.7f, cy - r * 0.75f, r * 1.0f, r * 0.7f);
        }

        // === BAND NUMBER ===
        juce::String label;
        switch (bandIndex)
        {
            case 0: label = "I"; break;
            case 1: label = "II"; break;
            case 2: label = "III"; break;
            case 3: label = "IV"; break;
            default: label = juce::String(bandIndex + 1); break;
        }

        // Text shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        auto font = juce::Font(juce::FontOptions().withHeight(bandIndex < 4 ? 11.0f : 12.0f));
        font.setBold(true);
        g.setFont(font);
        g.drawText(label, bounds.translated(0.5f, 0.5f), juce::Justification::centred);

        // Text
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.drawText(label, bounds, juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            params.enabled = !params.enabled;
            if (onParametersChanged) onParametersChanged(bandIndex, params);
            repaint();
            return;
        }
        
        dragging = true;
        dragStart = e.position;
        startFreq = params.frequency;
        startGain = params.gain;
        startQ = params.q;
        
        if (onDragStart) onDragStart(bandIndex);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging || !params.enabled) return;
        
        auto delta = e.position - dragStart;
        
        // Frequency (horizontal, log scale)
        float freqMult = std::pow(2.0f, delta.x / 100.0f);
        params.frequency = juce::jlimit(20.0f, 20000.0f, startFreq * freqMult);
        
        // Gain (vertical, linear)
        params.gain = juce::jlimit(-24.0f, 24.0f, startGain - delta.y / 10.0f);
        
        // Q with shift
        if (e.mods.isShiftDown())
        {
            float qMult = std::pow(2.0f, -delta.y / 150.0f);
            params.q = juce::jlimit(0.1f, 10.0f, startQ * qMult);
        }
        
        if (onParametersChanged) onParametersChanged(bandIndex, params);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging)
        {
            dragging = false;
            if (onDragEnd) onDragEnd(bandIndex);
            repaint();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        params.gain = 0.0f;
        params.filterType = 2; // Reset to Peak — LowCut/HighCut with gain=0 still filters!
        if (onParametersChanged) onParametersChanged(bandIndex, params);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (!params.enabled) return;
        params.q = juce::jlimit(0.1f, 10.0f, params.q * (1.0f + w.deltaY * 0.2f));
        if (onParametersChanged) onParametersChanged(bandIndex, params);
    }

private:
    int bandIndex;
    BandParameters params;
    juce::Colour color;
    
    bool dragging = false;
    juce::Point<float> dragStart;
    float startFreq = 0, startGain = 0, startQ = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQBandControl)
};
