#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EmberTokens.h"
#include "EmberLookAndFeel.h"
#include "EmberPhraseParser.h"

class EmberBar : public juce::Component
{
public:
    std::function<void(const juce::String&)> onPhraseChanged;
    std::function<void()> onApply;
    std::function<void()> onEnterFrase;
    std::function<void()> onCancelFrase;
    std::function<void()> onMatchToggle;
    std::function<void(float)> onMatchAmount;
    std::function<void(float)> onIntensity;

    EmberBar(EmberLookAndFeel& sharedLaf) : laf(sharedLaf)
    {
        setLookAndFeel(&laf);
        hint.setText("Type a phrase", juce::dontSendNotification);
        hint.setFont(laf.getMetaFont());
        hint.setColour(juce::Label::textColourId, EmberTokens::mute);
        hint.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(hint);

        editor.setMultiLine(false);
        editor.setReturnKeyStartsNewLine(false);
        editor.setFont(laf.getVoiceFont());
        editor.setTextToShowWhenEmpty("air / warm / harsh / mud / punch / clear", EmberTokens::mute);
        editor.onTextChange = [this]
        {
            if (onPhraseChanged)
                onPhraseChanged(editor.getText());
            updateChips();
        };
        editor.onReturnKey = [this]{ if (onApply) onApply(); };
        editor.onEscapeKey = [this]{ if (onCancelFrase) onCancelFrase(); };
        addChildComponent(editor);

        intensity.setRange(0.0, 1.0, 0.01);
        intensity.setValue(1.0);
        intensity.setSliderStyle(juce::Slider::LinearHorizontal);
        intensity.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        intensity.setColour(juce::Slider::trackColourId, EmberTokens::intent);
        intensity.onValueChange = [this]{ if (onIntensity) onIntensity((float) intensity.getValue()); };
        addChildComponent(intensity);

        apply.setButtonText("Apply");
        apply.setColour(juce::TextButton::buttonColourId, EmberTokens::intent);
        apply.setColour(juce::TextButton::textColourOffId, EmberTokens::bg);
        apply.onClick = [this]{ if (onApply) onApply(); };
        addChildComponent(apply);

        match.setButtonText("MATCH");
        match.setColour(juce::TextButton::buttonColourId, EmberTokens::raised);
        match.setColour(juce::TextButton::textColourOffId, EmberTokens::cyan);
        match.onClick = [this]{ if (onMatchToggle) onMatchToggle(); };
        addChildComponent(match);

        matchAmt.setRange(0.0, 1.0, 0.01);
        matchAmt.setValue(1.0);
        matchAmt.setSliderStyle(juce::Slider::LinearHorizontal);
        matchAmt.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        matchAmt.setColour(juce::Slider::trackColourId, EmberTokens::cyan);
        matchAmt.onValueChange = [this]{ if (onMatchAmount) onMatchAmount((float) matchAmt.getValue()); };
        addChildComponent(matchAmt);

        for (int i = 0; i < 2; ++i)
        {
            chips[i].setJustificationType(juce::Justification::centred);
            chips[i].setFont(laf.getMetaFont());
            chips[i].setColour(juce::Label::textColourId, EmberTokens::intent);
            chips[i].setColour(juce::Label::backgroundColourId, EmberTokens::raised);
            addChildComponent(chips[i]);
        }
    }

    ~EmberBar() override { setLookAndFeel(nullptr); }

    void setUiState(EmberUiState s)
    {
        state = s;
        const bool frase = (s == EmberUiState::Frase || s == EmberUiState::Apply);
        const bool matchMode = (s == EmberUiState::Match);
        editor.setVisible(frase);
        intensity.setVisible(frase);
        apply.setVisible(frase && s != EmberUiState::Apply);
        match.setVisible(frase || matchMode || s == EmberUiState::Riposo || s == EmberUiState::Nodo);
        matchAmt.setVisible(matchMode);
        hint.setVisible(s == EmberUiState::Riposo || s == EmberUiState::Nodo);
        updateChips();
        resized();
        repaint();
    }

    void focusPhrase()
    {
        editor.setVisible(true);
        editor.grabKeyboardFocus();
    }

    juce::String getPhrase() const { return editor.getText(); }
    float getIntensity() const { return (float) intensity.getValue(); }
    float getMatchAmount() const { return (float) matchAmt.getValue(); }

    void clearPhrase()
    {
        editor.clear();
        updateChips();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (state == EmberUiState::Riposo || state == EmberUiState::Nodo)
            if (onEnterFrase) onEnterFrase();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getTextCharacter() == '/' && (state == EmberUiState::Riposo || state == EmberUiState::Nodo))
        {
            if (onEnterFrase) onEnterFrase();
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(EmberTokens::raised);
        g.fillRect(getLocalBounds());
        g.setColour(EmberTokens::hairline);
        g.fillRect(0, 0, getWidth(), 1);

        // filament (intent) only when frase alive
        if (state == EmberUiState::Frase || state == EmberUiState::Apply)
        {
            g.setColour(EmberTokens::intent.withAlpha(0.85f));
            g.fillRect(0, 0, getWidth(), 1);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 4);
        if (state == EmberUiState::Riposo || state == EmberUiState::Nodo)
        {
            hint.setBounds(r);
            return;
        }
        if (state == EmberUiState::Match)
        {
            match.setBounds(r.removeFromLeft(72));
            r.removeFromLeft(8);
            matchAmt.setBounds(r.removeFromLeft(juce::jmin(220, r.getWidth())));
            return;
        }
        // FRASE / APPLY
        editor.setBounds(r.removeFromLeft(juce::jmin(320, r.getWidth() / 2)));
        r.removeFromLeft(8);
        for (int i = 0; i < 2; ++i)
        {
            if (chips[i].isVisible())
            {
                chips[i].setBounds(r.removeFromLeft(64));
                r.removeFromLeft(4);
            }
        }
        intensity.setBounds(r.removeFromLeft(100));
        r.removeFromLeft(8);
        apply.setBounds(r.removeFromLeft(64));
        r.removeFromLeft(8);
        match.setBounds(r.removeFromLeft(64));
    }

private:
    void updateChips()
    {
        auto ghosts = EmberPhrase::parse(editor.getText());
        for (int i = 0; i < 2; ++i)
        {
            if (i < (int) ghosts.size() && (state == EmberUiState::Frase || state == EmberUiState::Apply))
            {
                chips[i].setText(ghosts[(size_t) i].chip, juce::dontSendNotification);
                chips[i].setVisible(true);
            }
            else
            {
                chips[i].setVisible(false);
            }
        }
        resized();
    }

    EmberLookAndFeel& laf;
    EmberUiState state { EmberUiState::Riposo };
    juce::Label hint;
    juce::TextEditor editor;
    juce::Slider intensity, matchAmt;
    juce::TextButton apply, match;
    juce::Label chips[2];
};
