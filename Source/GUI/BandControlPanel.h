#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModernLookAndFeel.h"
#include "PremiumKnob.h"

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

        // Wave 4B Fix 3 (Tribunale): knob labels bumped 9 → 11 px Bold with
        // extra kerning so they read as proper hardware-style section headers
        // instead of fine-print. Text box below the knob also bumped 14 → 18
        // px for a larger value readout.
        auto makeLabelFont = []() {
            auto f = juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold"));
            f.setExtraKerningFactor(0.12f);
            return f;
        };

        // Frequency knob
        freqLabel.setText("FREQ", juce::dontSendNotification);
        freqLabel.setFont(makeLabelFont());
        freqLabel.setJustificationType(juce::Justification::centred);
        freqLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(freqLabel);

        // PremiumKnob (LargeAmber) already uses rotary drag style from its constructor.
        // Wave 4B Fix 3: larger text box (18 px) + wider (66 px) so the value
        // readout has breathing room and can display 4-char values without
        // clipping ("1.23k", "+12dB", etc.).
        freqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
        addAndMakeVisible(freqKnob);
        freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Freq", freqKnob);

        // Gain knob
        gainLabel.setText("GAIN", juce::dontSendNotification);
        gainLabel.setFont(makeLabelFont());
        gainLabel.setJustificationType(juce::Justification::centred);
        gainLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(gainLabel);

        gainKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
        addAndMakeVisible(gainKnob);
        gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Gain", gainKnob);

        // Q knob
        qLabel.setText("Q", juce::dontSendNotification);
        qLabel.setFont(makeLabelFont());
        qLabel.setJustificationType(juce::Justification::centred);
        qLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(qLabel);

        qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
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

        // DynEQ mode selector
        // CRITICAL: Must match APVTS AudioParameterChoice indices exactly
        // APVTS: 0=Off, 1=Compress, 2=Expand, 3=Gate
        // ComboBox IDs MUST be index+1 because JUCE treats ID 0 as "no selection"
        dynModeCombo.addItem("Off", 1);       // APVTS index 0
        dynModeCombo.addItem("Compress", 2);  // APVTS index 1
        dynModeCombo.addItem("Expand", 3);    // APVTS index 2
        dynModeCombo.addItem("Gate", 4);      // APVTS index 3
        dynModeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgPanel);
        dynModeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        dynModeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        dynModeCombo.onChange = [this] { updateDynEQVisibility(); };
        addAndMakeVisible(dynModeCombo);

        // DynEQ expand button ("..." opens overlay)
        dynExpandBtn.setTooltip("Advanced DynEQ settings (Range, Knee)");
        dynExpandBtn.onClick = [this] { if (onExpandDynEQRequested) onExpandDynEQRequested(); };
        addAndMakeVisible(dynExpandBtn);

        // DynEQ knob labels (10px, uppercase, centered)
        auto makeDynLabel = [](juce::Label& lbl, const juce::String& text) {
            lbl.setText(text, juce::dontSendNotification);
            auto f = juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold"));
            f.setExtraKerningFactor(0.12f);
            lbl.setFont(f);
            lbl.setJustificationType(juce::Justification::centred);
            lbl.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        };
        makeDynLabel(thrLabel, "THR");
        makeDynLabel(ratLabel, "RAT");
        makeDynLabel(atkLabel, "ATK");
        makeDynLabel(relLabel, "REL");
        addAndMakeVisible(thrLabel);
        addAndMakeVisible(ratLabel);
        addAndMakeVisible(atkLabel);
        addAndMakeVisible(relLabel);

        // DynEQ knobs (SmallBlue style, 10px textbox below)
        thresholdKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        ratioKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        attackKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        releaseKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        addAndMakeVisible(thresholdKnob);
        addAndMakeVisible(ratioKnob);
        addAndMakeVisible(attackKnob);
        addAndMakeVisible(releaseKnob);

        // DynEQ APVTS attachments
        dynModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynMode", dynModeCombo);
        thrAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Threshold", thresholdKnob);
        ratAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Ratio", ratioKnob);
        atkAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Attack", attackKnob);
        relAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Release", releaseKnob);

        updateSlopeVisibility();
        updateDynEQVisibility();
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
        dynModeAtt.reset(); thrAtt.reset(); ratAtt.reset(); atkAtt.reset(); relAtt.reset();

        bandIndex = newIndex;
        juce::String prefix = "band" + juce::String(bandIndex);
        bandColor = ModernLookAndFeel::Colors::getBandColor(bandIndex);

        bandLabel.setText("B" + juce::String(bandIndex + 1), juce::dontSendNotification);
        bandLabel.setColour(juce::Label::textColourId, bandColor);
        // PremiumKnob filmstrip is pre-rendered amber; no per-band recolor needed.
        enableBtn.setColour(juce::TextButton::buttonOnColourId, bandColor);

        freqAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Freq",    freqKnob);
        gainAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Gain",    gainKnob);
        qAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Q",       qKnob);
        typeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "Type",  typeCombo);
        slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "Slope", slopeCombo);
        enableAtt= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Enabled", enableBtn);
        soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Solo",    soloBtn);

        // DynEQ APVTS attachments
        dynModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynMode", dynModeCombo);
        thrAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Threshold", thresholdKnob);
        ratAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Ratio", ratioKnob);
        atkAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Attack", attackKnob);
        relAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Release", releaseKnob);

        typeCombo.addListener(this);
        updateSlopeVisibility();
        updateDynEQVisibility();
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

        // Wave 4B Fix 3 (Tribunale): dedicated darker sub-panel behind the
        // FREQ/GAIN/Q knob cluster so the three knobs pop visually against
        // the rest of the band panel. Only drawn in non-compact vertical
        // layout — the compact row doesn't have room for a separate backdrop.
        if (!compact && !knobClusterBounds.isEmpty())
        {
            auto kb = knobClusterBounds.toFloat().expanded(2.0f, 2.0f);
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.55f));
            g.fillRoundedRectangle(kb, 4.0f);
            g.setColour(bandColor.withAlpha(0.18f));
            g.drawRoundedRectangle(kb, 4.0f, 1.0f);
        }

        // DynEQ knob cluster backdrop (blue accent instead of band color)
        if (!compact && dynEQActive && !dynKnobClusterBounds.isEmpty())
        {
            auto dkb = dynKnobClusterBounds.toFloat().expanded(2.0f, 2.0f);
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.55f));
            g.fillRoundedRectangle(dkb, 4.0f);
            g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.15f));
            g.drawRoundedRectangle(dkb, 4.0f, 1.0f);
        }

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
            // Wave 4B Fix 3: clear knobClusterBounds in compact mode so the
            // darker sub-panel backdrop is only drawn in the vertical layout.
            knobClusterBounds = {};

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
            // === VERTICAL LAYOUT (Liquid Intelligence: 3 big LargeAmber knobs + DynEQ) ===
            bounds.removeFromTop(2);
            bandLabel.setBounds(bounds.removeFromTop(16));
            bounds.removeFromTop(2);

            // Type combo + ON/SOLO inline row (22px)
            auto typeRow = bounds.removeFromTop(22);
            typeLabel.setVisible(false);
            typeCombo.setBounds(typeRow.removeFromLeft(typeRow.getWidth() - 80).reduced(2, 0));
            typeRow.removeFromLeft(2);
            enableBtn.setBounds(typeRow.removeFromLeft(38).reduced(1));
            typeRow.removeFromLeft(2);
            soloBtn.setBounds(typeRow.reduced(1));
            bounds.removeFromTop(2);

            // Slope row (only when LowCut/HighCut)
            if (showSlope)
            {
                slopeLabel.setVisible(false);
                slopeCombo.setBounds(bounds.removeFromTop(22).reduced(2, 0));
                bounds.removeFromTop(2);
            }
            else
            {
                slopeLabel.setVisible(false);
            }

            // DynEQ mode selector row (22px) — combo + "..." button
            auto dynModeRow = bounds.removeFromTop(22);
            dynModeCombo.setBounds(dynModeRow.removeFromLeft(dynModeRow.getWidth() - 36).reduced(2, 0));
            dynModeRow.removeFromLeft(2);
            dynExpandBtn.setBounds(dynModeRow.reduced(1));
            bounds.removeFromTop(2);

            // Bottom-up layout: DynEQ knobs (if active) + main knobs
            // Enable/Solo row already placed inline with type combo

            // DynEQ knob row (56px: 10 label + 36 knob + 10 textbox) — from bottom
            if (dynEQActive)
            {
                auto dynRow = bounds.removeFromBottom(56);
                dynKnobClusterBounds = dynRow;
                const int dynKnobW = dynRow.getWidth() / 4;
                const int dynLabelH = 10;

                auto thrArea = dynRow.removeFromLeft(dynKnobW);
                thrLabel.setBounds(thrArea.removeFromTop(dynLabelH));
                thresholdKnob.setBounds(thrArea);

                auto ratArea = dynRow.removeFromLeft(dynKnobW);
                ratLabel.setBounds(ratArea.removeFromTop(dynLabelH));
                ratioKnob.setBounds(ratArea);

                auto atkArea = dynRow.removeFromLeft(dynKnobW);
                atkLabel.setBounds(atkArea.removeFromTop(dynLabelH));
                attackKnob.setBounds(atkArea);

                auto relArea = dynRow;
                relLabel.setBounds(relArea.removeFromTop(dynLabelH));
                releaseKnob.setBounds(relArea);

                bounds.removeFromBottom(2); // gap above DynEQ row
            }
            else
            {
                dynKnobClusterBounds = {};
            }

            // Main FREQ/GAIN/Q knob cluster — takes remaining vertical space
            bounds.removeFromTop(4);
            knobClusterBounds = bounds;

            const int knobW = bounds.getWidth() / 3;
            const int labelH = 14;

            auto freqArea = bounds.removeFromLeft(knobW);
            freqLabel.setVisible(true);
            freqLabel.setBounds(freqArea.removeFromTop(labelH));
            freqArea.removeFromTop(1);
            freqKnob.setBounds(freqArea);

            auto gainArea = bounds.removeFromLeft(knobW);
            gainLabel.setVisible(true);
            gainLabel.setBounds(gainArea.removeFromTop(labelH));
            gainArea.removeFromTop(1);
            gainKnob.setBounds(gainArea);

            auto qArea = bounds;
            qLabel.setVisible(true);
            qLabel.setBounds(qArea.removeFromTop(labelH));
            qArea.removeFromTop(1);
            qKnob.setBounds(qArea);
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

    void updateDynEQVisibility()
    {
        // Note: ComboBox IDs are 1-based (1=Off, 2=Compress, 3=Expand, 4=Gate)
        // APVTS indices are 0-based, handled by ComboBoxAttachment automatically
        dynEQActive = (dynModeCombo.getSelectedId() > 1); // 1 = Off
        thresholdKnob.setVisible(dynEQActive);
        ratioKnob.setVisible(dynEQActive);
        attackKnob.setVisible(dynEQActive);
        releaseKnob.setVisible(dynEQActive);
        thrLabel.setVisible(dynEQActive);
        ratLabel.setVisible(dynEQActive);
        atkLabel.setVisible(dynEQActive);
        relLabel.setVisible(dynEQActive);
        dynExpandBtn.setVisible(dynEQActive);
        resized();
        repaint();
    }

    int bandIndex;
    juce::Colour bandColor;
    juce::AudioProcessorValueTreeState& parameters;

    // Wave 4B Fix 3: rect used by paint() to draw the dedicated darker
    // sub-panel behind the FREQ/GAIN/Q knob cluster. Written by resized()
    // in the non-compact branch, read by paint().
    juce::Rectangle<int> knobClusterBounds;

    juce::Label bandLabel;
    juce::Label freqLabel, gainLabel, qLabel, typeLabel, slopeLabel;
    // Phase 4 (completion): 3 filmstrip LargeAmber knobs for Freq / Gain / Q
    // Empty custom label — we use the external juce::Label next to each knob.
    PremiumKnob freqKnob { juce::String(), PremiumKnob::Style::LargeAmber };
    PremiumKnob gainKnob { juce::String(), PremiumKnob::Style::LargeAmber };
    PremiumKnob qKnob    { juce::String(), PremiumKnob::Style::LargeAmber };
    juce::ComboBox typeCombo, slopeCombo;
    juce::TextButton enableBtn, soloBtn;

    // DynEQ mode selector (always visible)
    juce::ComboBox dynModeCombo;
    juce::TextButton dynExpandBtn { "···" }; // opens overlay for Range/Knee

    // DynEQ knobs (visible only when mode != Off)
    PremiumKnob thresholdKnob { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob ratioKnob     { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob attackKnob    { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob releaseKnob   { juce::String(), PremiumKnob::Style::SmallBlue };
    juce::Label thrLabel, ratLabel, atkLabel, relLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt, soloAtt;

    // DynEQ APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dynModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thrAtt, ratAtt, atkAtt, relAtt;

    // Layout state
    juce::Rectangle<int> dynKnobClusterBounds;
    bool dynEQActive = false;

public:
    // Callback for opening advanced DynEQ overlay (Range/Knee)
    std::function<void()> onExpandDynEQRequested;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlPanel)
};
