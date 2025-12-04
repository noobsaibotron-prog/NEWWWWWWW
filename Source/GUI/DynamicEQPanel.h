#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/DynamicEQProcessor.h"

//==============================================================================
/**
 * Dynamic EQ Control Panel - FabFilter Pro-Q / TDR Nova Style
 * 
 * Provides per-band dynamic EQ controls:
 * - Dynamic Mode selector (Off/Compress/Expand/Gate)
 * - Threshold knob with meter
 * - Ratio knob
 * - Attack/Release knobs
 * - Range knob
 * - Knee knob
 * - Gain reduction meter per band
 */
class DynamicEQPanel : public juce::Component,
                       public juce::Timer
{
public:
    DynamicEQPanel(juce::AudioProcessorValueTreeState& apvts, int bandIndex)
        : apvts(apvts), bandIndex(bandIndex)
    {
        setOpaque(false);
        
        juce::String prefix = "band" + juce::String(bandIndex);
        
        // Dynamic Mode ComboBox
        modeCombo.addItem("Off", 1);
        modeCombo.addItem("Compress", 2);
        modeCombo.addItem("Expand", 3);
        modeCombo.addItem("Gate", 4);
        modeCombo.setSelectedId(1);
        modeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF2D2D2D));
        modeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        modeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF4A90D9));
        addAndMakeVisible(modeCombo);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, prefix + "DynMode", modeCombo);
        
        // Setup knobs
        setupKnob(thresholdKnob, "Threshold", -60.0f, 0.0f, -20.0f, " dB");
        setupKnob(ratioKnob, "Ratio", 1.0f, 20.0f, 2.0f, ":1");
        setupKnob(attackKnob, "Attack", 0.1f, 500.0f, 10.0f, " ms");
        setupKnob(releaseKnob, "Release", 1.0f, 2000.0f, 100.0f, " ms");
        setupKnob(rangeKnob, "Range", 0.0f, 48.0f, 24.0f, " dB");
        setupKnob(kneeKnob, "Knee", 0.0f, 24.0f, 6.0f, " dB");
        
        // Attachments
        thresholdAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Threshold", thresholdKnob);
        ratioAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Ratio", ratioKnob);
        attackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Attack", attackKnob);
        releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Release", releaseKnob);
        rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Range", rangeKnob);
        kneeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Knee", kneeKnob);
        
        // Labels
        for (auto* label : {&modeLabel, &thresholdLabel, &ratioLabel, 
                            &attackLabel, &releaseLabel, &rangeLabel, &kneeLabel})
        {
            label->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
            label->setFont(juce::Font(11.0f));
            label->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(label);
        }
        
        modeLabel.setText("MODE", juce::dontSendNotification);
        thresholdLabel.setText("THRESH", juce::dontSendNotification);
        ratioLabel.setText("RATIO", juce::dontSendNotification);
        attackLabel.setText("ATTACK", juce::dontSendNotification);
        releaseLabel.setText("RELEASE", juce::dontSendNotification);
        rangeLabel.setText("RANGE", juce::dontSendNotification);
        kneeLabel.setText("KNEE", juce::dontSendNotification);
        
        // Title label
        titleLabel.setText("DYNAMIC EQ - Band " + juce::String(bandIndex + 1), juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF4A90D9));
        titleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);
        
        // Start timer for gain reduction meter
        startTimerHz(30);
    }
    
    ~DynamicEQPanel() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Border
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
        
        // Draw gain reduction meter
        drawGainReductionMeter(g);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Title
        titleLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);
        
        // Mode selector
        auto modeArea = bounds.removeFromTop(50);
        modeLabel.setBounds(modeArea.removeFromTop(15));
        modeCombo.setBounds(modeArea.reduced(5, 5));
        
        bounds.removeFromTop(5);
        
        // Gain reduction meter area
        auto meterArea = bounds.removeFromTop(20);
        gainReductionBounds = meterArea;
        
        bounds.removeFromTop(10);
        
        // Knobs row 1: Threshold, Ratio, Range
        auto row1 = bounds.removeFromTop(70);
        int knobWidth = row1.getWidth() / 3;
        
        auto threshArea = row1.removeFromLeft(knobWidth);
        thresholdLabel.setBounds(threshArea.removeFromTop(15));
        thresholdKnob.setBounds(threshArea.reduced(5));
        
        auto ratioArea = row1.removeFromLeft(knobWidth);
        ratioLabel.setBounds(ratioArea.removeFromTop(15));
        ratioKnob.setBounds(ratioArea.reduced(5));
        
        auto rangeArea = row1;
        rangeLabel.setBounds(rangeArea.removeFromTop(15));
        rangeKnob.setBounds(rangeArea.reduced(5));
        
        bounds.removeFromTop(5);
        
        // Knobs row 2: Attack, Release, Knee
        auto row2 = bounds.removeFromTop(70);
        
        auto attackArea = row2.removeFromLeft(knobWidth);
        attackLabel.setBounds(attackArea.removeFromTop(15));
        attackKnob.setBounds(attackArea.reduced(5));
        
        auto releaseArea = row2.removeFromLeft(knobWidth);
        releaseLabel.setBounds(releaseArea.removeFromTop(15));
        releaseKnob.setBounds(releaseArea.reduced(5));
        
        auto kneeArea = row2;
        kneeLabel.setBounds(kneeArea.removeFromTop(15));
        kneeKnob.setBounds(kneeArea.reduced(5));
    }
    
    void timerCallback() override
    {
        // Update gain reduction display
        if (dynamicProcessor != nullptr)
        {
            auto meter = dynamicProcessor->getBandMeter(bandIndex);
            currentGainReduction = meter.gainReduction;
            repaint(gainReductionBounds);
        }
    }
    
    void setDynamicProcessor(DynamicEQProcessor* processor)
    {
        dynamicProcessor = processor;
    }
    
    void setBandIndex(int newIndex)
    {
        if (newIndex == bandIndex || newIndex < 0 || newIndex >= 8)
            return;
        
        bandIndex = newIndex;
        
        juce::String prefix = "band" + juce::String(bandIndex);
        
        // Re-attach all parameters
        modeAttachment.reset();
        thresholdAtt.reset();
        ratioAtt.reset();
        attackAtt.reset();
        releaseAtt.reset();
        rangeAtt.reset();
        kneeAtt.reset();
        
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, prefix + "DynMode", modeCombo);
        thresholdAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Threshold", thresholdKnob);
        ratioAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Ratio", ratioKnob);
        attackAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Attack", attackKnob);
        releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Release", releaseKnob);
        rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Range", rangeKnob);
        kneeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, prefix + "Knee", kneeKnob);
        
        titleLabel.setText("DYNAMIC EQ - Band " + juce::String(bandIndex + 1), juce::dontSendNotification);
        repaint();
    }

private:
    void setupKnob(juce::Slider& knob, const juce::String& name,
                   float min, float max, float defaultVal, const juce::String& suffix)
    {
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        knob.setRange(min, max);
        knob.setValue(defaultVal);
        knob.setTextValueSuffix(suffix);
        knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF4A90D9));
        knob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF3A3A3A));
        knob.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        knob.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(knob);
    }
    
    void drawGainReductionMeter(juce::Graphics& g)
    {
        auto bounds = gainReductionBounds.toFloat().reduced(2);
        
        // Background
        g.setColour(juce::Colour(0xFF2D2D2D));
        g.fillRoundedRectangle(bounds, 3.0f);
        
        // GR meter (shows negative values as reduction)
        if (std::abs(currentGainReduction) > 0.1f)
        {
            float normalizedGR = juce::jlimit(0.0f, 1.0f, std::abs(currentGainReduction) / 24.0f);
            float meterWidth = bounds.getWidth() * normalizedGR;
            
            // Color based on reduction amount
            juce::Colour grColor;
            if (normalizedGR < 0.3f)
                grColor = juce::Colour(0xFF4A90D9); // Blue for light reduction
            else if (normalizedGR < 0.6f)
                grColor = juce::Colour(0xFFE6A23C); // Orange for medium
            else
                grColor = juce::Colour(0xFFF56C6C); // Red for heavy
            
            g.setColour(grColor);
            g.fillRoundedRectangle(bounds.getX(), bounds.getY(), 
                                   meterWidth, bounds.getHeight(), 3.0f);
        }
        
        // Text
        juce::String grText = "GR: " + juce::String(currentGainReduction, 1) + " dB";
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(grText, gainReductionBounds, juce::Justification::centred);
    }
    
    //==========================================================================
    juce::AudioProcessorValueTreeState& apvts;
    int bandIndex;
    DynamicEQProcessor* dynamicProcessor = nullptr;
    
    // UI Components
    juce::ComboBox modeCombo;
    juce::Slider thresholdKnob, ratioKnob, attackKnob, releaseKnob, rangeKnob, kneeKnob;
    juce::Label titleLabel, modeLabel, thresholdLabel, ratioLabel;
    juce::Label attackLabel, releaseLabel, rangeLabel, kneeLabel;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAtt;
    
    // Metering
    float currentGainReduction = 0.0f;
    juce::Rectangle<int> gainReductionBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQPanel)
};

//==============================================================================
/**
 * Global Dynamic EQ Controls Panel
 * 
 * Master controls for the Dynamic EQ:
 * - Enable/Disable toggle
 * - Mix (dry/wet)
 * - Auto Makeup gain toggle
 * - Total Gain Reduction meter
 */
class DynamicEQMasterPanel : public juce::Component,
                              public juce::Timer
{
public:
    DynamicEQMasterPanel(juce::AudioProcessorValueTreeState& apvts)
        : apvts(apvts)
    {
        setOpaque(false);
        
        // Enable toggle
        enableButton.setButtonText("DYN EQ");
        enableButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        enableButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF4A90D9));
        addAndMakeVisible(enableButton);
        enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "dynEqEnabled", enableButton);
        
        // Mix knob
        mixKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        mixKnob.setRange(0.0, 100.0);
        mixKnob.setValue(100.0);
        mixKnob.setTextValueSuffix("%");
        mixKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF4A90D9));
        mixKnob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF3A3A3A));
        mixKnob.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        mixKnob.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        mixKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(mixKnob);
        mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "dynEqMix", mixKnob);
        
        // Auto Makeup toggle
        autoMakeupButton.setButtonText("AUTO MU");
        autoMakeupButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        autoMakeupButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF4A90D9));
        addAndMakeVisible(autoMakeupButton);
        autoMakeupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "dynAutoMakeup", autoMakeupButton);
        
        // Labels
        mixLabel.setText("MIX", juce::dontSendNotification);
        mixLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
        mixLabel.setFont(juce::Font(11.0f));
        mixLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(mixLabel);
        
        startTimerHz(30);
    }
    
    ~DynamicEQMasterPanel() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(juce::Colour(0xFF252525));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        
        // Border
        g.setColour(juce::Colour(0xFF4A90D9).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
        
        // Total GR meter
        drawTotalGRMeter(g);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        
        // Enable button
        enableButton.setBounds(bounds.removeFromTop(25));
        bounds.removeFromTop(5);
        
        // Mix knob
        auto mixArea = bounds.removeFromTop(70);
        mixLabel.setBounds(mixArea.removeFromTop(15));
        mixKnob.setBounds(mixArea.reduced(5));
        
        bounds.removeFromTop(5);
        
        // Auto Makeup button
        autoMakeupButton.setBounds(bounds.removeFromTop(25));
        
        bounds.removeFromTop(5);
        
        // GR meter
        totalGRBounds = bounds.removeFromTop(20);
    }
    
    void timerCallback() override
    {
        if (dynamicProcessor != nullptr)
        {
            totalGainReduction = dynamicProcessor->getTotalGainReduction();
            repaint(totalGRBounds);
        }
    }
    
    void setDynamicProcessor(DynamicEQProcessor* processor)
    {
        dynamicProcessor = processor;
    }

private:
    void drawTotalGRMeter(juce::Graphics& g)
    {
        auto bounds = totalGRBounds.toFloat().reduced(2);
        
        // Background
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(bounds, 3.0f);
        
        // GR meter
        if (std::abs(totalGainReduction) > 0.1f)
        {
            float normalizedGR = juce::jlimit(0.0f, 1.0f, std::abs(totalGainReduction) / 24.0f);
            float meterWidth = bounds.getWidth() * normalizedGR;
            
            juce::Colour grColor;
            if (normalizedGR < 0.3f)
                grColor = juce::Colour(0xFF67C23A); // Green
            else if (normalizedGR < 0.6f)
                grColor = juce::Colour(0xFFE6A23C); // Orange
            else
                grColor = juce::Colour(0xFFF56C6C); // Red
            
            g.setColour(grColor);
            g.fillRoundedRectangle(bounds.getX(), bounds.getY(), 
                                   meterWidth, bounds.getHeight(), 3.0f);
        }
        
        // Text
        juce::String grText = "TOTAL GR: " + juce::String(totalGainReduction, 1) + " dB";
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(grText, totalGRBounds, juce::Justification::centred);
    }
    
    //==========================================================================
    juce::AudioProcessorValueTreeState& apvts;
    DynamicEQProcessor* dynamicProcessor = nullptr;
    
    juce::ToggleButton enableButton, autoMakeupButton;
    juce::Slider mixKnob;
    juce::Label mixLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoMakeupAtt;
    
    float totalGainReduction = 0.0f;
    juce::Rectangle<int> totalGRBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicEQMasterPanel)
};

