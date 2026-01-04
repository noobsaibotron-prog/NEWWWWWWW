#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * Professional Output Level Meter
 * 
 * Features:
 * - Stereo VU-style display with peak hold
 * - Gradient from green to yellow to red
 * - Clip indicator with reset on click
 * - dB scale markings
 * - Smooth ballistics (attack/release)
 */
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    LevelMeter()
    {
        // startTimerHz(30); // ALL TIMERS DISABLED
    }
    
    ~LevelMeter() override { stopTimer(); }
    
    //==========================================================================
    // Level Input (call from timer on message thread)
    //==========================================================================
    void setLevels(float leftDB, float rightDB)
    {
        // Attack (fast)
        if (leftDB > currentLeftDB)
            currentLeftDB = leftDB;
        else
            currentLeftDB = currentLeftDB * 0.85f + leftDB * 0.15f; // Release (slower)
        
        if (rightDB > currentRightDB)
            currentRightDB = rightDB;
        else
            currentRightDB = currentRightDB * 0.85f + rightDB * 0.15f;
        
        // Peak hold
        if (leftDB > peakLeftDB)
        {
            peakLeftDB = leftDB;
            peakHoldCounterL = peakHoldFrames;
        }
        if (rightDB > peakRightDB)
        {
            peakRightDB = rightDB;
            peakHoldCounterR = peakHoldFrames;
        }
        
        // Clip detection
        if (leftDB >= 0.0f || rightDB >= 0.0f)
            clipped = true;
    }
    
    void resetClip() { clipped = false; }
    bool hasClipped() const { return clipped; }
    
    //==========================================================================
    // Drawing
    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
        
        // Inner area
        auto inner = bounds.reduced(3.0f);
        
        // Meter area (leave space for labels)
        auto meterArea = inner.reduced(2.0f);
        meterArea.removeFromTop(16.0f);  // Space for dB readout
        meterArea.removeFromBottom(4.0f);
        
        float meterW = (meterArea.getWidth() - 4.0f) / 2.0f;  // Two channels
        
        // Left channel
        auto leftRect = meterArea.removeFromLeft(meterW);
        drawChannel(g, leftRect, currentLeftDB, peakLeftDB);
        
        meterArea.removeFromLeft(4.0f);  // Gap
        
        // Right channel  
        auto rightRect = meterArea;
        drawChannel(g, rightRect, currentRightDB, peakRightDB);
        
        // dB readout at top
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        float currentMaxDB = std::max(currentLeftDB, currentRightDB);
        juce::String dbText = currentMaxDB > -60.0f 
            ? juce::String(currentMaxDB, 1) + " dB"
            : "-∞";
        
        g.setColour(clipped ? juce::Colours::red : ModernLookAndFeel::Colors::textBright);
        g.drawText(dbText, inner.removeFromTop(14).toNearestInt(), juce::Justification::centred);
        
        // Clip indicator
        if (clipped)
        {
            auto clipRect = bounds.removeFromTop(6.0f).reduced(2.0f, 0.0f);
            g.setColour(juce::Colours::red);
            g.fillRoundedRectangle(clipRect, 2.0f);
        }
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        // Click to reset clip indicator
        resetClip();
        repaint();
    }
    
    void timerCallback() override
    {
        // Peak hold decay
        if (peakHoldCounterL > 0)
            peakHoldCounterL--;
        else
            peakLeftDB = std::max(peakLeftDB - 1.0f, minDB);
        
        if (peakHoldCounterR > 0)
            peakHoldCounterR--;
        else
            peakRightDB = std::max(peakRightDB - 1.0f, minDB);
        
        repaint();
    }
    
private:
    void drawChannel(juce::Graphics& g, juce::Rectangle<float> rect, float levelDB, float peakDB)
    {
        // Background track
        g.setColour(ModernLookAndFeel::Colors::bgLight);
        g.fillRoundedRectangle(rect, 2.0f);
        
        // Calculate fill height
        float normalized = juce::jmap(juce::jlimit(minDB, maxDB, levelDB), minDB, maxDB, 0.0f, 1.0f);
        float fillHeight = rect.getHeight() * normalized;
        
        if (fillHeight > 1.0f)
        {
            auto fillRect = rect.removeFromBottom(fillHeight);
            
            // Gradient: green -> yellow -> red
            juce::ColourGradient gradient(
                juce::Colour(0xFF44BB44), fillRect.getBottomLeft(),  // Green at bottom
                juce::Colour(0xFFFF4444), fillRect.getTopLeft(),     // Red at top
                false);
            gradient.addColour(0.6, juce::Colour(0xFFEECC00));       // Yellow at 60%
            gradient.addColour(0.85, juce::Colour(0xFFFF8800));      // Orange at 85%
            
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(fillRect, 2.0f);
        }
        
        // Peak hold marker
        float peakNorm = juce::jmap(juce::jlimit(minDB, maxDB, peakDB), minDB, maxDB, 0.0f, 1.0f);
        if (peakNorm > 0.01f)
        {
            float peakY = rect.getBottom() - rect.getHeight() * peakNorm;
            
            juce::Colour peakCol = peakDB >= 0.0f ? juce::Colours::red 
                                 : peakDB >= -6.0f ? juce::Colour(0xFFFF8800)
                                 : juce::Colour(0xFF44BB44);
            g.setColour(peakCol);
            g.fillRect(rect.getX(), peakY - 1.0f, rect.getWidth(), 2.0f);
        }
        
        // Scale markers (subtle lines at key dB levels)
        g.setColour(ModernLookAndFeel::Colors::textMuted.withAlpha(0.3f));
        const float dbMarks[] = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        for (float db : dbMarks)
        {
            float y = rect.getY() + rect.getHeight() * (1.0f - juce::jmap(db, minDB, maxDB, 0.0f, 1.0f));
            g.drawHorizontalLine(static_cast<int>(y), rect.getX() + 1, rect.getRight() - 1);
        }
    }
    
    float currentLeftDB = minDB;
    float currentRightDB = minDB;
    float peakLeftDB = minDB;
    float peakRightDB = minDB;
    
    int peakHoldCounterL = 0;
    int peakHoldCounterR = 0;
    static constexpr int peakHoldFrames = 30;  // 1 second at 30fps
    
    bool clipped = false;
    
    static constexpr float minDB = -60.0f;
    static constexpr float maxDB = 6.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

