#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "EmberTokens.h"

/** Slim L&F for Ember intensity / match sliders and type combo. */
class EmberLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EmberLookAndFeel()
    {
        interRegular  = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_ttf,  BinaryData::InterRegular_ttfSize);
        interMedium   = juce::Typeface::createSystemTypefaceFor(BinaryData::InterMedium_ttf,   BinaryData::InterMedium_ttfSize);
        interSemiBold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        setDefaultSansSerifTypeface(interRegular);

        setColour(juce::Slider::backgroundColourId, EmberTokens::sunken);
        setColour(juce::Slider::trackColourId, EmberTokens::intent);
        setColour(juce::Slider::thumbColourId, EmberTokens::text);
        setColour(juce::ComboBox::backgroundColourId, EmberTokens::raised);
        setColour(juce::ComboBox::outlineColourId, EmberTokens::hairline);
        setColour(juce::ComboBox::textColourId, EmberTokens::text);
        setColour(juce::PopupMenu::backgroundColourId, EmberTokens::raised);
        setColour(juce::PopupMenu::textColourId, EmberTokens::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, EmberTokens::hairline);
        setColour(juce::TextEditor::backgroundColourId, EmberTokens::sunken);
        setColour(juce::TextEditor::textColourId, EmberTokens::text);
        setColour(juce::TextEditor::outlineColourId, EmberTokens::hairline);
        setColour(juce::TextEditor::focusedOutlineColourId, EmberTokens::intent);
        setColour(juce::TextEditor::highlightColourId, EmberTokens::intent.withAlpha(EmberTokens::alphaTextSelection));
        setColour(juce::CaretComponent::caretColourId, EmberTokens::intent);
        setColour(juce::Label::textColourId, EmberTokens::dim);
    }

    juce::Font getWordmarkFont() const
    {
        juce::Font f(interMedium);
        f.setHeight(12.0f);
        f.setExtraKerningFactor(0.22f);
        return f;
    }

    juce::Font getVoiceFont() const
    {
        juce::Font f(interRegular);
        f.setHeight(14.0f);
        return f;
    }
    juce::Font getCmdFont() const
    {
        juce::Font f(interMedium);
        f.setHeight(11.0f);
        f.setExtraKerningFactor(0.04f);
        return f;
    }
    juce::Font getMetaFont() const
    {
        juce::Font f(interRegular);
        f.setHeight(10.0f);
        return f;
    }
    juce::Font getNumFont() const
    {
        juce::Font f(interRegular);
        f.setHeight(12.0f);
        return f;
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);
        auto track = juce::Rectangle<float>((float) x, (float) y + (float) height * 0.45f,
                                            (float) width, 2.0f);
        g.setColour(EmberTokens::hairline);
        g.fillRect(track);

        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
        {
            auto fill = track.withWidth(juce::jmax(0.0f, sliderPos - (float) x));
            g.setColour(slider.findColour(juce::Slider::trackColourId));
            g.fillRect(fill);
            g.setColour(EmberTokens::text);
            g.fillEllipse(sliderPos - 4.0f, track.getCentreY() - 4.0f, 8.0f, 8.0f);
        }
    }

private:
    juce::Typeface::Ptr interRegular, interMedium, interSemiBold;
};
