#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModernLookAndFeel.h"

//==============================================================================
class BandControlPanel : public juce::Component,
                         public juce::ComboBox::Listener
{
public:
    BandControlPanel(int bandIdx, juce::AudioProcessorValueTreeState& apvts)
        : bandIndex(bandIdx), parameters(apvts)
    {
        juce::String prefix = "band" + juce::String(bandIndex);
        bandColor = ModernLookAndFeel::Colors::getBandColor(bandIndex);

        // Band label
        bandLabel.setText("B" + juce::String(bandIndex + 1), juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(14.0f));
            font.setBold(true);
            bandLabel.setFont(font);
        }
        bandLabel.setColour(juce::Label::textColourId, bandColor);
        bandLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(bandLabel);

        // Frequency knob
        freqLabel.setText("FREQ", juce::dontSendNotification);
        freqLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        freqLabel.setJustificationType(juce::Justification::centred);
        freqLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(freqLabel);

        freqKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        freqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        freqKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(freqKnob);
        freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Freq", freqKnob);

        // Gain knob
        gainLabel.setText("GAIN", juce::dontSendNotification);
        gainLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        gainLabel.setJustificationType(juce::Justification::centred);
        gainLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(gainLabel);

        gainKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        gainKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(gainKnob);
        gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Gain", gainKnob);

        // Q knob
        qLabel.setText("Q", juce::dontSendNotification);
        qLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        qLabel.setJustificationType(juce::Justification::centred);
        qLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(qLabel);

        qKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        qKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        addAndMakeVisible(qKnob);
        qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Q", qKnob);

        // Filter type selector
        typeLabel.setText("TYPE", juce::dontSendNotification);
        typeLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        typeLabel.setJustificationType(juce::Justification::centred);
        typeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(typeLabel);

        typeCombo.addItem("Low Cut", 1);
        typeCombo.addItem("Low Shelf", 2);
        typeCombo.addItem("Peak", 3);
        typeCombo.addItem("High Shelf", 4);
        typeCombo.addItem("High Cut", 5);
        typeCombo.addItem("Notch", 6);
        typeCombo.addItem("Band Pass", 7);
        typeCombo.addItem("Vintage Low Shelf", 8);
        typeCombo.addItem("Vintage High Shelf", 9);
        typeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark);
        typeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        typeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        addAndMakeVisible(typeCombo);
        typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, prefix + "Type", typeCombo);
        typeCombo.addListener(this);

        // Slope selector (visible only for LowCut / HighCut)
        slopeLabel.setText("SLOPE", juce::dontSendNotification);
        slopeLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        slopeLabel.setJustificationType(juce::Justification::centred);
        slopeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        addAndMakeVisible(slopeLabel);

        slopeCombo.addItem("12 dB/oct", 1);
        slopeCombo.addItem("24 dB/oct", 2);
        slopeCombo.addItem("48 dB/oct", 3);
        slopeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark);
        slopeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        slopeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        addAndMakeVisible(slopeCombo);
        slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, prefix + "Slope", slopeCombo);

        // Enable toggle
        enableBtn.setButtonText("ON");
        enableBtn.setClickingTogglesState(true);
        enableBtn.setColour(juce::TextButton::buttonOnColourId, bandColor);
        addAndMakeVisible(enableBtn);
        enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, prefix + "Enabled", enableBtn);

        // Solo toggle
        soloBtn.setButtonText("SOLO");
        soloBtn.setClickingTogglesState(true);
        soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFFD700));
        soloBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        soloBtn.setTooltip("Solo this band (mutes all others)");
        addAndMakeVisible(soloBtn);
        soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, prefix + "Solo", soloBtn);

        updateSlopeVisibility();
    }

    ~BandControlPanel() override
    {
        typeCombo.removeListener(this);
    }

    void comboBoxChanged(juce::ComboBox* combo) override
    {
        if (combo == &typeCombo)
            updateSlopeVisibility();
    }

    void setBandIndex(int newIndex)
    {
        if (newIndex == bandIndex) return;

        typeCombo.removeListener(this);

        freqAtt.reset(); gainAtt.reset(); qAtt.reset();
        typeAtt.reset(); slopeAtt.reset();
        enableAtt.reset(); soloAtt.reset();

        bandIndex = newIndex;
        juce::String prefix = "band" + juce::String(bandIndex);
        bandColor = ModernLookAndFeel::Colors::getBandColor(bandIndex);

        bandLabel.setText("B" + juce::String(bandIndex + 1), juce::dontSendNotification);
        bandLabel.setColour(juce::Label::textColourId, bandColor);
        freqKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        gainKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        qKnob.setColour(juce::Slider::rotarySliderFillColourId, bandColor);
        enableBtn.setColour(juce::TextButton::buttonOnColourId, bandColor);

        freqAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Freq",    freqKnob);
        gainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Gain",    gainKnob);
        qAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Q",       qKnob);
        typeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "Type",  typeCombo);
        slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "Slope", slopeCombo);
        enableAtt= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Enabled", enableBtn);
        soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Solo",    soloBtn);

        typeCombo.addListener(this);
        updateSlopeVisibility();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const bool compact = bounds.getHeight() < 80;

        g.setColour(ModernLookAndFeel::Colors::bgPanel);
        g.fillRoundedRectangle(bounds, compact ? 3.0f : 4.0f);

        g.setColour(bandColor.withAlpha(0.6f));
        if (compact)
            g.fillRoundedRectangle(bounds.removeFromLeft(3.0f), 2.0f);
        else
            g.fillRoundedRectangle(bounds.removeFromTop(3.0f), 2.0f);

        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), compact ? 3.0f : 4.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(4, 2);
        const bool compact = bounds.getHeight() < 80;
        const bool showSlope = slopeCombo.isVisible();

        if (compact)
        {
            // === HORIZONTAL COMPACT LAYOUT ===
            bandLabel.setBounds(bounds.removeFromLeft(28));
            bounds.removeFromLeft(2);

            enableBtn.setBounds(bounds.removeFromLeft(32).reduced(0, 4));
            bounds.removeFromLeft(2);
            soloBtn.setBounds(bounds.removeFromLeft(38).reduced(0, 4));
            bounds.removeFromLeft(4);

            typeLabel.setVisible(false);
            typeCombo.setBounds(bounds.removeFromLeft(90).reduced(0, 4));
            bounds.removeFromLeft(2);

            // Slope combo (compact: narrow, right after type)
            slopeLabel.setVisible(false);
            if (showSlope)
            {
                slopeCombo.setBounds(bounds.removeFromLeft(80).reduced(0, 4));
                bounds.removeFromLeft(2);
            }

            int knobW = std::min(60, bounds.getWidth() / 3);

            auto freqArea = bounds.removeFromLeft(knobW);
            freqLabel.setVisible(true);
            freqLabel.setBounds(freqArea.removeFromTop(11));
            freqKnob.setBounds(freqArea);
            freqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
            bounds.removeFromLeft(2);

            auto gainArea = bounds.removeFromLeft(knobW);
            gainLabel.setVisible(true);
            gainLabel.setBounds(gainArea.removeFromTop(11));
            gainKnob.setBounds(gainArea);
            gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
            bounds.removeFromLeft(2);

            auto qArea = bounds.removeFromLeft(knobW);
            qLabel.setVisible(true);
            qLabel.setBounds(qArea.removeFromTop(11));
            qKnob.setBounds(qArea);
            qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        }
        else
        {
            // === VERTICAL LAYOUT ===
            bounds.removeFromTop(4);
            bandLabel.setBounds(bounds.removeFromTop(18));
            bounds.removeFromTop(2);

            typeLabel.setVisible(true);
            typeLabel.setBounds(bounds.removeFromTop(12));
            typeCombo.setBounds(bounds.removeFromTop(26).reduced(2, 0));
            bounds.removeFromTop(4);

            // Slope row (only when LowCut/HighCut)
            if (showSlope)
            {
                slopeLabel.setVisible(true);
                slopeLabel.setBounds(bounds.removeFromTop(12));
                slopeCombo.setBounds(bounds.removeFromTop(26).reduced(2, 0));
                bounds.removeFromTop(4);
            }
            else
            {
                slopeLabel.setVisible(false);
            }

            auto btnRow = bounds.removeFromBottom(26);
            enableBtn.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(4, 2));
            soloBtn.setBounds(btnRow.reduced(4, 2));
            bounds.removeFromBottom(2);

            int knobW = bounds.getWidth() / 3;

            auto freqArea = bounds.removeFromLeft(knobW);
            freqLabel.setVisible(true);
            freqLabel.setBounds(freqArea.removeFromTop(12));
            freqKnob.setBounds(freqArea);
            freqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);

            auto gainArea = bounds.removeFromLeft(knobW);
            gainLabel.setVisible(true);
            gainLabel.setBounds(gainArea.removeFromTop(12));
            gainKnob.setBounds(gainArea);
            gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);

            auto qArea = bounds;
            qLabel.setVisible(true);
            qLabel.setBounds(qArea.removeFromTop(12));
            qKnob.setBounds(qArea);
            qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        }
    }

    int getBandIndex() const { return bandIndex; }

private:
    void updateSlopeVisibility()
    {
        // 1=LowCut, 5=HighCut (1-based combo IDs)
        const int t = typeCombo.getSelectedId();
        const bool isCutFilter = (t == 1 || t == 5);
        slopeCombo.setVisible(isCutFilter);
        slopeLabel.setVisible(isCutFilter);
        resized();
    }

    int bandIndex;
    juce::Colour bandColor;
    juce::AudioProcessorValueTreeState& parameters;

    juce::Label bandLabel;
    juce::Label freqLabel, gainLabel, qLabel, typeLabel, slopeLabel;
    juce::Slider freqKnob, gainKnob, qKnob;
    juce::ComboBox typeCombo, slopeCombo;
    juce::TextButton enableBtn, soloBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt, soloAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlPanel)
};
