#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "EmberTokens.h"
#include "EmberLookAndFeel.h"

class EmberOverflow : public juce::Component
{
public:
    std::function<void(bool)> onClimateChanged;
    std::function<void()> onClose;

    EmberOverflow(juce::AudioProcessorValueTreeState& apvtsIn, EmberLookAndFeel& sharedLaf)
        : apvts(apvtsIn), laf(sharedLaf)
    {
        setLookAndFeel(&laf);
        title.setText("Overflow", juce::dontSendNotification);
        title.setFont(laf.getCmdFont());
        title.setColour(juce::Label::textColourId, EmberTokens::dim);
        addAndMakeVisible(title);

        phase.addItemList(juce::StringArray{ "ZL", "natural", "linear" }, 1);
        addAndMakeVisible(phase);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("phaseMode")))
            phase.setSelectedItemIndex(p->getIndex(), juce::dontSendNotification);
        phase.onChange = [this]
        {
            if (auto* p = apvts.getParameter("phaseMode"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1((float) phase.getSelectedItemIndex()));
                p->endChangeGesture();
            }
        };

        climate.setButtonText("Climate off");
        climate.setToggleState(false, juce::dontSendNotification);
        climate.setColour(juce::ToggleButton::textColourId, EmberTokens::dim);
        climate.onClick = [this]
        {
            const bool on = climate.getToggleState();
            climate.setButtonText(on ? "Climate on" : "Climate off");
            if (onClimateChanged) onClimateChanged(on);
        };
        addAndMakeVisible(climate);

        close.setButtonText("Close");
        close.onClick = [this]{ setVisible(false); if (onClose) onClose(); };
        addAndMakeVisible(close);
        setVisible(false);
    }

    ~EmberOverflow() override { setLookAndFeel(nullptr); }

    bool isClimateOn() const { return climate.getToggleState(); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(EmberTokens::raised);
        g.fillRect(getLocalBounds());
        g.setColour(EmberTokens::hairline);
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        title.setBounds(r.removeFromTop(20));
        r.removeFromTop(12);
        phase.setBounds(r.removeFromTop(28));
        r.removeFromTop(12);
        climate.setBounds(r.removeFromTop(24));
        close.setBounds(r.removeFromBottom(28));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    EmberLookAndFeel& laf;
    juce::Label title;
    juce::ComboBox phase;
    juce::ToggleButton climate;
    juce::TextButton close;
};
