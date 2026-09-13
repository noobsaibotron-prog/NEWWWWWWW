#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "EmberTokens.h"
#include "EmberLookAndFeel.h"

class EmberHeader : public juce::Component
{
public:
    std::function<void()> onMenu;
    std::function<void(bool)> onSlotA; // true=A, false=B

    EmberHeader(juce::AudioProcessorValueTreeState& apvtsIn, EmberLookAndFeel& sharedLaf)
        : apvts(apvtsIn), laf(sharedLaf)
    {
        setLookAndFeel(&laf);
        wordmark.setText("EMBER CORE", juce::dontSendNotification);
        wordmark.setFont(laf.getWordmarkFont());
        wordmark.setColour(juce::Label::textColourId, EmberTokens::text);
        wordmark.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(wordmark);

        preset.setColour(juce::Label::textColourId, EmberTokens::dim);
        preset.setFont(laf.getMetaFont());
        preset.setText("Init", juce::dontSendNotification);
        addAndMakeVisible(preset);

        for (auto* b : { &btnA, &btnB })
        {
            b->setClickingTogglesState(true);
            b->setColour(juce::TextButton::buttonColourId, EmberTokens::raised);
            b->setColour(juce::TextButton::buttonOnColourId, EmberTokens::hairline);
            b->setColour(juce::TextButton::textColourOffId, EmberTokens::dim);
            b->setColour(juce::TextButton::textColourOnId, EmberTokens::text);
            addAndMakeVisible(*b);
        }
        btnA.setRadioGroupId(7701);
        btnB.setRadioGroupId(7701);
        btnA.setToggleState(true, juce::dontSendNotification);
        btnA.onClick = [this]{ if (onSlotA) onSlotA(true); };
        btnB.onClick = [this]{ if (onSlotA) onSlotA(false); };

        bypass.setButtonText("Bypass");
        bypass.setColour(juce::ToggleButton::textColourId, EmberTokens::dim);
        addAndMakeVisible(bypass);
        bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "bypass", bypass);

        badge.setFont(laf.getMetaFont());
        badge.setColour(juce::Label::textColourId, EmberTokens::cyan);
        badge.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(badge);
        updateBadge();

        menu.setButtonText(juce::String::fromUTF8("\xE2\x98\xB0")); // ☰
        menu.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        menu.setColour(juce::TextButton::textColourOffId, EmberTokens::dim);
        menu.onClick = [this]{ if (onMenu) onMenu(); };
        addAndMakeVisible(menu);
    }

    ~EmberHeader() override { setLookAndFeel(nullptr); }

    void setPresetName(const juce::String& name) { preset.setText(name, juce::dontSendNotification); }

    void updateBadge()
    {
        // ZL default → no badge. NAT / LP / HQ only when ≠ ZL.
        auto* phase = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("phaseMode"));
        auto* hq = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("qualityMode"));
        juce::String text;
        if (phase != nullptr && phase->getIndex() == 1)
            text = "NAT";
        else if (phase != nullptr && phase->getIndex() == 2)
            text = "LP";
        if (hq != nullptr && hq->getIndex() == 1)
            text = text.isEmpty() ? "HQ +5" : (text + " HQ");
        badge.setText(text, juce::dontSendNotification);
        badge.setVisible(text.isNotEmpty());
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(EmberTokens::chrome);
        g.fillRect(getLocalBounds());
        // Brand mark: the one deliberate amber exception to 'amber means pending intent'.
        g.setColour(EmberTokens::intent);
        g.fillEllipse(10.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
        g.setColour(EmberTokens::hairline);
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(8, 4);
        r.removeFromLeft(18); // logo mark
        wordmark.setBounds(r.removeFromLeft(120));
        preset.setBounds(r.removeFromLeft(100));
        menu.setBounds(r.removeFromRight(28));
        badge.setBounds(r.removeFromRight(64));
        bypass.setBounds(r.removeFromRight(72));
        btnB.setBounds(r.removeFromRight(28));
        btnA.setBounds(r.removeFromRight(28));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    EmberLookAndFeel& laf;
    juce::Label wordmark, preset, badge;
    juce::TextButton btnA { "A" }, btnB { "B" }, menu;
    juce::ToggleButton bypass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
};
