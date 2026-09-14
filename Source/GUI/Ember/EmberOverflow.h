#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Integration/EmberProposalProtocol.h"
#include "EmberTokens.h"
#include "EmberLookAndFeel.h"

class EmberOverflow : public juce::Component
{
public:
    std::function<void(bool)> onClimateChanged;
    std::function<void(bool)> onCopilotLinkChanged;
    std::function<void()> onCopilotPair;
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

        copilotTitle.setText("COPILOT", juce::dontSendNotification);
        copilotTitle.setFont(laf.getMetaFont());
        copilotTitle.setColour(juce::Label::textColourId, EmberTokens::dim);
        addAndMakeVisible(copilotTitle);

        copilotLink.setButtonText("Link off");
        copilotLink.setClickingTogglesState(true);
        copilotLink.setColour(juce::ToggleButton::textColourId, EmberTokens::dim);
        copilotLink.setTooltip("Allow this Ember Core instance to receive proposals from Ableton Copilot");
        copilotLink.setComponentID("emberV2CopilotLink");
        copilotLink.onClick = [this]
        {
            const bool on = copilotLink.getToggleState();
            copilotLink.setButtonText(on ? "Link on" : "Link off");
            if (onCopilotLinkChanged) onCopilotLinkChanged(on);
        };
        addAndMakeVisible(copilotLink);

        copilotPair.setButtonText("Pair this instance");
        copilotPair.setEnabled(false);
        copilotPair.setTooltip("Pair Copilot with this exact Ember Core instance");
        copilotPair.setComponentID("emberV2CopilotPair");
        copilotPair.onClick = [this]
        {
            if (onCopilotPair) onCopilotPair();
        };
        addAndMakeVisible(copilotPair);

        copilotStatus.setText("Link off", juce::dontSendNotification);
        copilotStatus.setFont(laf.getMetaFont());
        copilotStatus.setColour(juce::Label::textColourId, EmberTokens::mute);
        copilotStatus.setJustificationType(juce::Justification::centredLeft);
        copilotStatus.setMinimumHorizontalScale(0.75f);
        copilotStatus.setInterceptsMouseClicks(false, false);
        copilotStatus.setComponentID("emberV2CopilotStatus");
        addAndMakeVisible(copilotStatus);

        close.setButtonText("Close");
        close.onClick = [this]{ setVisible(false); if (onClose) onClose(); };
        addAndMakeVisible(close);
        setVisible(false);
    }

    ~EmberOverflow() override { setLookAndFeel(nullptr); }

    bool isClimateOn() const { return climate.getToggleState(); }

    void setCopilotUi(const EmberProposal::LinkUiState& ui)
    {
        copilotLink.setToggleState(ui.linkEnabled, juce::dontSendNotification);
        copilotLink.setButtonText(ui.linkEnabled ? "Link on" : "Link off");

        const bool canPair = ui.linkEnabled && ui.authenticated
                          && ! ui.paired && ! ui.pairOfferPending;
        copilotPair.setEnabled(canPair);
        copilotPair.setButtonText(ui.paired ? "Paired"
                                  : (ui.pairOfferPending ? "Waiting for Copilot"
                                                         : "Pair this instance"));
        copilotStatus.setText(juce::String::fromUTF8(ui.statusText.c_str()),
                              juce::dontSendNotification);
        copilotStatus.setColour(juce::Label::textColourId,
                                ! ui.pendingSource.empty() ? EmberTokens::intent
                                : ui.paired ? EmberTokens::signal
                                : ui.linkEnabled ? EmberTokens::dim
                                                 : EmberTokens::mute);
    }

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
        r.removeFromTop(22);
        copilotTitle.setBounds(r.removeFromTop(18));
        r.removeFromTop(6);
        copilotLink.setBounds(r.removeFromTop(24));
        r.removeFromTop(6);
        copilotPair.setBounds(r.removeFromTop(28));
        r.removeFromTop(5);
        copilotStatus.setBounds(r.removeFromTop(20));
        close.setBounds(r.removeFromBottom(28));
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    EmberLookAndFeel& laf;
    juce::Label title, copilotTitle, copilotStatus;
    juce::ComboBox phase;
    juce::ToggleButton climate, copilotLink;
    juce::TextButton copilotPair, close;
};
