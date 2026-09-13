#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EmberTokens.h"
#include "EmberLookAndFeel.h"

/** Floating inspector anchored under the selected node (168px). */
class EmberInspector : public juce::Component
{
public:
    std::function<void(float)> onFreq, onGain, onQ;
    std::function<void(int)> onType;

    EmberInspector()
    {
        setLookAndFeel(&laf);
        for (auto* s : { &freq, &gain, &q })
        {
            s->setSliderStyle(juce::Slider::LinearVertical);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
            s->setColour(juce::Slider::trackColourId, EmberTokens::text);
            addAndMakeVisible(*s);
        }
        freq.setRange(20.0, 20000.0, 1.0);
        freq.setSkewFactorFromMidPoint(1000.0);
        gain.setRange(-24.0, 24.0, 0.1);
        q.setRange(0.2, 10.0, 0.01);
        q.setSkewFactorFromMidPoint(1.0);

        type.addItemList(juce::StringArray{ "Low Cut", "Low Shelf", "Peak", "High Shelf",
                                            "High Cut", "Notch", "Band Pass" }, 1);
        addAndMakeVisible(type);

        freq.onValueChange = [this]{ if (onFreq) onFreq((float) freq.getValue()); };
        gain.onValueChange = [this]{ if (onGain) onGain((float) gain.getValue()); };
        q.onValueChange    = [this]{ if (onQ) onQ((float) q.getValue()); };
        type.onChange      = [this]{ if (onType) onType(type.getSelectedItemIndex()); };

        freqLabel.setText("Hz", juce::dontSendNotification);
        gainLabel.setText("dB", juce::dontSendNotification);
        qLabel.setText("Q", juce::dontSendNotification);
        for (auto* l : { &freqLabel, &gainLabel, &qLabel })
        {
            l->setFont(laf.getMetaFont());
            l->setColour(juce::Label::textColourId, EmberTokens::dim);
            l->setJustificationType(juce::Justification::centred);
            addAndMakeVisible(*l);
        }
    }

    ~EmberInspector() override { setLookAndFeel(nullptr); }

    void setValues(float hz, float db, float qq, int typeIdx)
    {
        freq.setValue(hz, juce::dontSendNotification);
        gain.setValue(db, juce::dontSendNotification);
        q.setValue(qq, juce::dontSendNotification);
        type.setSelectedItemIndex(juce::jlimit(0, 6, typeIdx), juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(EmberTokens::glass);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), EmberTokens::radiusInspector);
        g.setColour(EmberTokens::hairline);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), EmberTokens::radiusInspector, EmberTokens::strokeHairline);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6);
        type.setBounds(r.removeFromTop(22));
        r.removeFromTop(4);
        auto row = r.removeFromTop(14);
        const int w = row.getWidth() / 3;
        freqLabel.setBounds(row.removeFromLeft(w));
        gainLabel.setBounds(row.removeFromLeft(w));
        qLabel.setBounds(row);
        auto body = r;
        freq.setBounds(body.removeFromLeft(w));
        gain.setBounds(body.removeFromLeft(w));
        q.setBounds(body);
    }

private:
    EmberLookAndFeel laf;
    juce::Slider freq, gain, q;
    juce::ComboBox type;
    juce::Label freqLabel, gainLabel, qLabel;
};
