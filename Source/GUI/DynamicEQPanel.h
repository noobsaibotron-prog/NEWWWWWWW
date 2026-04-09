#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/DynamicEQProcessor.h"
#include "ModernLookAndFeel.h"
#include <atomic>
#include <functional>

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
        modeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgLighter);
        modeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textBright);
        modeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::accentBlue);
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
            label->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
            {
                auto lf = juce::Font(juce::FontOptions().withHeight(13.0f));
                lf.setBold(true);
                label->setFont(lf);
            }
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
        titleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentBlue);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(13.0f));
            font.setBold(true);
            titleLabel.setFont(font);
        }
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);
        
        // Repaint/refresh hook for graph overlays and selected-band visuals.
        auto notifyDynamicParamChanged = [this, &apvts]()
        {
            // If the user is editing Dynamic EQ for this band, make sure the band is active.
            // This matches the graph interaction path where touching/dragging a node can
            // implicitly activate it; without this, the Dynamic EQ DSP/meter may stay inert
            // until the user moves the node on the spectrum.
            if (auto* enabledParam = apvts.getParameter("band" + juce::String(this->bandIndex) + "Enabled"))
            {
                if (auto* raw = apvts.getRawParameterValue("band" + juce::String(this->bandIndex) + "Enabled");
                    raw != nullptr && raw->load() < 0.5f)
                {
                    enabledParam->beginChangeGesture();
                    enabledParam->setValueNotifyingHost(enabledParam->convertTo0to1(1.0f));
                    enabledParam->endChangeGesture();
                }
            }

            if (onDynamicParamsChanged)
                onDynamicParamsChanged(this->bandIndex);
            repaint();
        };

        modeCombo.onChange = notifyDynamicParamChanged;
        thresholdKnob.onValueChange = notifyDynamicParamChanged;
        ratioKnob.onValueChange = notifyDynamicParamChanged;
        attackKnob.onValueChange = notifyDynamicParamChanged;
        releaseKnob.onValueChange = notifyDynamicParamChanged;
        rangeKnob.onValueChange = notifyDynamicParamChanged;
        kneeKnob.onValueChange = notifyDynamicParamChanged;

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
        g.setColour(ModernLookAndFeel::Colors::bgMid);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
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
        if (!bandMeterProvider)
            return;

        auto meter = bandMeterProvider(bandIndex);

        // Compute GR directly from current knob values + audio-thread input level.
        // This guarantees instant meter response when threshold/ratio/mode are tweaked,
        // instead of waiting for the audio thread to pick up the param change and
        // recalculate via smoothedEnv (which doesn't move when signal is static).
        //
        // meter.inputLevel = smoothedEnv in dB from audio thread (updates while audio flows).
        // knob values        = always current (read directly here).
        const int modeId = modeCombo.getSelectedId(); // 1=Off, 2=Compress, 3=Expand, 4=Gate
        float newGR = 0.0f;

        if (modeId > 1 && meter.inputLevel > -99.0f)
        {
            const float thresh = static_cast<float>(thresholdKnob.getValue());
            const float ratio  = static_cast<float>(ratioKnob.getValue());
            const float range  = static_cast<float>(rangeKnob.getValue());
            const float knee   = static_cast<float>(kneeKnob.getValue());
            newGR = computeExpectedGR(meter.inputLevel, modeId - 1, thresh, ratio, knee, range);
        }

        // Smooth for display: instant attack, fast release (~80ms at 30Hz)
        constexpr float releaseCoeff = 0.6f;
        if (std::abs(newGR) > std::abs(currentGainReduction))
            currentGainReduction = newGR;
        else
            currentGainReduction = currentGainReduction * releaseCoeff + newGR * (1.0f - releaseCoeff);

        if (std::abs(currentGainReduction) < 0.05f)
            currentGainReduction = 0.0f;

        repaint(gainReductionBounds);
    }
    
    void setBandMeterProvider(std::function<DynamicEQProcessor::BandMeter(int)> provider)
    {
        bandMeterProvider = std::move(provider);
    }

    void setDynamicParamsChangedCallback(std::function<void(int)> callback)
    {
        onDynamicParamsChanged = std::move(callback);
    }
    
    void setBandIndex(int newIndex)
    {
        if (newIndex == bandIndex || newIndex < 0 || newIndex >= DynamicEQProcessor::maxBands)
            return;

        // CRITICAL: Destroy ALL old attachments FIRST, before creating new ones.
        // If a new attachment updates the knob value while the old attachment is still
        // alive, the old attachment's listener fires and writes the new band's value
        // into the OLD band's parameter — corrupting the previous band's settings.
        modeAttachment.reset();
        thresholdAtt.reset();
        ratioAtt.reset();
        attackAtt.reset();
        releaseAtt.reset();
        rangeAtt.reset();
        kneeAtt.reset();

        bandIndex = newIndex;
        juce::String prefix = "band" + juce::String(bandIndex);

        // Now safe to create new attachments — no old listener can intercept
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
    // Mirror of DynamicEQProcessor::calculateDynamicGain — called on GUI thread
    // so the meter reflects current knob values instantly without waiting for audio thread.
    static float computeExpectedGR(float inputDb, int dynMode,
                                   float threshold, float ratio, float knee, float range)
    {
        // dynMode: 1=Compress, 2=Expand, 3=Gate  (matches DynamicMode_* constants)
        if (dynMode == 1) // Compress
        {
            if (inputDb <= threshold - knee * 0.5f)
                return 0.0f;
            if (knee > 0.0f && inputDb < threshold + knee * 0.5f)
            {
                float x = inputDb - (threshold - knee * 0.5f);
                float gr = (x * x) / (2.0f * knee) * (1.0f / ratio - 1.0f);
                return juce::jlimit(-range, 0.0f, gr);
            }
            return juce::jlimit(-range, 0.0f, (threshold - inputDb) * (1.0f - 1.0f / ratio));
        }
        if (dynMode == 2) // Expand
        {
            if (inputDb <= threshold - knee * 0.5f)
                return 0.0f;
            float excess = inputDb - threshold;
            if (knee > 0.0f && excess < knee * 0.5f)
            {
                float kneeRatio = (excess + knee * 0.5f) / knee;
                return juce::jlimit(0.0f, range, kneeRatio * excess * (1.0f - 1.0f / ratio));
            }
            return juce::jlimit(0.0f, range, excess * (1.0f - 1.0f / ratio));
        }
        if (dynMode == 3) // Gate
        {
            if (inputDb >= threshold)
                return 0.0f;
            float below = threshold - inputDb;
            return juce::jlimit(-range, 0.0f, -below * ratio);
        }
        return 0.0f;
    }

    void setupKnob(juce::Slider& knob, const juce::String& name,
                   float min, float max, float defaultVal, const juce::String& suffix)
    {
        juce::ignoreUnused(name);
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        knob.setRange(min, max);
        knob.setValue(defaultVal);
        knob.setTextValueSuffix(suffix);
        knob.setColour(juce::Slider::rotarySliderFillColourId, ModernLookAndFeel::Colors::accentBlue);
        knob.setColour(juce::Slider::rotarySliderOutlineColourId, ModernLookAndFeel::Colors::bgLighter);
        knob.setColour(juce::Slider::thumbColourId, ModernLookAndFeel::Colors::textBright);
        knob.setColour(juce::Slider::textBoxTextColourId, ModernLookAndFeel::Colors::textBright);
        knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(knob);
    }
    
    void drawGainReductionMeter(juce::Graphics& g)
    {
        auto bounds = gainReductionBounds.toFloat().reduced(2);
        
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.fillRoundedRectangle(bounds, 3.0f);

        // GR meter (shows negative values as reduction) - always draw even for small GR
        float absGR = std::abs(currentGainReduction);
        float normalizedGR = juce::jlimit(0.0f, 1.0f, absGR / 24.0f);
        float meterWidth = bounds.getWidth() * normalizedGR;

        // Color based on reduction amount
        juce::Colour grColor;
        if (normalizedGR < 0.3f)
            grColor = ModernLookAndFeel::Colors::accentBlue;   // Light reduction
        else if (normalizedGR < 0.6f)
            grColor = ModernLookAndFeel::Colors::accentYellow;  // Medium
        else
            grColor = ModernLookAndFeel::Colors::accentRed;     // Heavy
        
        g.setColour(grColor.withAlpha(normalizedGR > 0.0f ? 1.0f : 0.2f));
        g.fillRoundedRectangle(bounds.getX(), bounds.getY(), 
                               meterWidth, bounds.getHeight(), 3.0f);
        
        // Text
        juce::String grText = "GR: " + juce::String(currentGainReduction, 1) + " dB";
        g.setColour(ModernLookAndFeel::Colors::textBright);
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText(grText, gainReductionBounds, juce::Justification::centred);
    }
    
    //==========================================================================
    juce::AudioProcessorValueTreeState& apvts;
    int bandIndex;
    std::function<DynamicEQProcessor::BandMeter(int)> bandMeterProvider;
    std::function<void(int)> onDynamicParamsChanged;
    
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
        enableButton.setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textBright);
        enableButton.setColour(juce::ToggleButton::tickColourId, ModernLookAndFeel::Colors::accentBlue);
        addAndMakeVisible(enableButton);
        enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "dynEqEnabled", enableButton);
        
        // Mix knob
        mixKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        mixKnob.setRange(0.0, 100.0);
        mixKnob.setValue(100.0);
        mixKnob.setTextValueSuffix("%");
        mixKnob.setColour(juce::Slider::rotarySliderFillColourId, ModernLookAndFeel::Colors::accentBlue);
        mixKnob.setColour(juce::Slider::rotarySliderOutlineColourId, ModernLookAndFeel::Colors::bgLighter);
        mixKnob.setColour(juce::Slider::thumbColourId, ModernLookAndFeel::Colors::textBright);
        mixKnob.setColour(juce::Slider::textBoxTextColourId, ModernLookAndFeel::Colors::textBright);
        mixKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(mixKnob);
        mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, "dynEqMix", mixKnob);
        
        // Auto Makeup toggle
        autoMakeupButton.setButtonText("AUTO MU");
        autoMakeupButton.setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textBright);
        autoMakeupButton.setColour(juce::ToggleButton::tickColourId, ModernLookAndFeel::Colors::accentBlue);
        addAndMakeVisible(autoMakeupButton);
        autoMakeupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "dynAutoMakeup", autoMakeupButton);
        
        // Labels
        mixLabel.setText("MIX", juce::dontSendNotification);
        mixLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textPrimary);
        mixLabel.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
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
        g.setColour(ModernLookAndFeel::Colors::bgPanel);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

        // Border
        g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.5f));
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
        if (totalGRProvider)
        {
            totalGainReduction = totalGRProvider();
            repaint(totalGRBounds);
        }
    }
    
    void setTotalGRProvider(std::function<float()> provider)
    {
        totalGRProvider = std::move(provider);
    }

private:
    void drawTotalGRMeter(juce::Graphics& g)
    {
        auto bounds = totalGRBounds.toFloat().reduced(2);
        
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgMid);
        g.fillRoundedRectangle(bounds, 3.0f);

        // GR meter
        if (std::abs(totalGainReduction) > 0.1f)
        {
            float normalizedGR = juce::jlimit(0.0f, 1.0f, std::abs(totalGainReduction) / 24.0f);
            float meterWidth = bounds.getWidth() * normalizedGR;

            juce::Colour grColor;
            if (normalizedGR < 0.3f)
                grColor = ModernLookAndFeel::Colors::accentGreen;  // Light
            else if (normalizedGR < 0.6f)
                grColor = ModernLookAndFeel::Colors::accentYellow;  // Medium
            else
                grColor = ModernLookAndFeel::Colors::accentRed;     // Heavy
            
            g.setColour(grColor);
            g.fillRoundedRectangle(bounds.getX(), bounds.getY(), 
                                   meterWidth, bounds.getHeight(), 3.0f);
        }
        
        // Text
        juce::String grText = "TOTAL GR: " + juce::String(totalGainReduction, 1) + " dB";
        g.setColour(ModernLookAndFeel::Colors::textBright);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText(grText, totalGRBounds, juce::Justification::centred);
    }
    
    //==========================================================================
    juce::AudioProcessorValueTreeState& apvts;
    std::function<float()> totalGRProvider;
    
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

