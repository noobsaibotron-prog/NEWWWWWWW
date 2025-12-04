#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * Band Control Panel - Detailed controls for a single EQ band
 * 
 * Provides knobs for:
 * - Frequency
 * - Gain  
 * - Q (Resonance/Bandwidth)
 * - Filter Type selector
 * - Enable toggle
 */
class BandControlPanel : public juce::Component
{
public:
    BandControlPanel(int bandIdx, juce::AudioProcessorValueTreeState& apvts)
        : bandIndex(bandIdx), parameters(apvts)
    {
        juce::String prefix = "band" + juce::String(bandIndex);
        bandColor = ModernLookAndFeel::Colors::getBandColor(bandIndex);
        
        // Band label
        juce::String labels[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
        bandLabel.setText(labels[bandIndex], juce::dontSendNotification);
        bandLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        bandLabel.setColour(juce::Label::textColourId, bandColor);
        bandLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(bandLabel);
        
        // Frequency knob
        freqLabel.setText("FREQ", juce::dontSendNotification);
        freqLabel.setFont(juce::Font(8.0f));
        freqLabel.setJustificationType(juce::Justification::centred);
        freqLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(freqLabel);
        
        freqKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        freqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        freqKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(freqKnob);
        freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Freq", freqKnob);
        
        // Gain knob
        gainLabel.setText("GAIN", juce::dontSendNotification);
        gainLabel.setFont(juce::Font(8.0f));
        gainLabel.setJustificationType(juce::Justification::centred);
        gainLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(gainLabel);
        
        gainKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        gainKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(gainKnob);
        gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Gain", gainKnob);
        
        // Q knob
        qLabel.setText("Q", juce::dontSendNotification);
        qLabel.setFont(juce::Font(8.0f));
        qLabel.setJustificationType(juce::Justification::centred);
        qLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(qLabel);
        
        qKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        qKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(qKnob);
        qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Q", qKnob);
        
        // Enable toggle
        enableBtn.setButtonText("ON");
        enableBtn.setClickingTogglesState(true);
        enableBtn.setColour(juce::TextButton::buttonOnColourId, bandColor);
        addAndMakeVisible(enableBtn);
        enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, prefix + "Enabled", enableBtn);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgPanel);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Top accent bar
        g.setColour(bandColor.withAlpha(0.6f));
        g.fillRoundedRectangle(bounds.removeFromTop(3.0f), 2.0f);
        
        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(4);
        bounds.removeFromTop(4); // Account for accent bar
        
        // Band label at top
        bandLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(2);
        
        // Enable button
        enableBtn.setBounds(bounds.removeFromBottom(22).reduced(6, 2));
        bounds.removeFromBottom(2);
        
        // Three knobs in a row
        int knobW = bounds.getWidth() / 3;
        int knobH = bounds.getHeight();
        
        auto freqArea = bounds.removeFromLeft(knobW);
        freqLabel.setBounds(freqArea.removeFromTop(12));
        freqKnob.setBounds(freqArea);
        
        auto gainArea = bounds.removeFromLeft(knobW);
        gainLabel.setBounds(gainArea.removeFromTop(12));
        gainKnob.setBounds(gainArea);
        
        auto qArea = bounds;
        qLabel.setBounds(qArea.removeFromTop(12));
        qKnob.setBounds(qArea);
    }
    
    int getBandIndex() const { return bandIndex; }
    
private:
    int bandIndex;
    juce::Colour bandColor;
    juce::AudioProcessorValueTreeState& parameters;
    
    juce::Label bandLabel;
    juce::Label freqLabel, gainLabel, qLabel;
    juce::Slider freqKnob, gainKnob, qKnob;
    juce::TextButton enableBtn;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlPanel)
};

