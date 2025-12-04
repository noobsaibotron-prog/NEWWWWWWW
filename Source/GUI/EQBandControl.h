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
            // Disabled: small dim circle
            g.setColour(color.withAlpha(0.2f));
            g.fillEllipse(cx - 4, cy - 4, 8, 8);
            g.setColour(color.withAlpha(0.4f));
            g.drawEllipse(cx - 4, cy - 4, 8, 8, 1.0f);
            return;
        }

        // === SELECTION RING (when hovered or dragging) ===
        if (isMouseOver() || dragging)
        {
            g.setColour(color.withAlpha(0.25f));
            g.fillEllipse(cx - r - 6, cy - r - 6, (r + 6) * 2, (r + 6) * 2);
            
            g.setColour(color.withAlpha(0.5f));
            g.drawEllipse(cx - r - 4, cy - r - 4, (r + 4) * 2, (r + 4) * 2, 2.0f);
        }

        // === MAIN CIRCLE ===
        // Filled background
        g.setColour(color.withAlpha(0.9f));
        g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
        
        // Border
        g.setColour(dragging ? color.brighter(0.3f) : color.brighter(0.1f));
        g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 2.0f);
        
        // Inner highlight (top)
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.fillEllipse(cx - r * 0.4f, cy - r * 0.6f, r * 0.6f, r * 0.35f);

        // === BAND NUMBER ===
        // Use Roman numerals for first 4, then numbers
        juce::String label;
        switch (bandIndex)
        {
            case 0: label = "I"; break;
            case 1: label = "II"; break;
            case 2: label = "III"; break;
            case 3: label = "IV"; break;
            default: label = juce::String(bandIndex + 1); break;
        }
        
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.setFont(juce::Font(bandIndex < 4 ? 11.0f : 12.0f, juce::Font::bold));
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
