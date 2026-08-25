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
        qKnob.setComponentID("bandQControl");
        addAndMakeVisible(qKnob);
        qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            parameters, prefix + "Q", qKnob);

        // Surgical shelves deliberately use the authority-fixed Butterworth
        // damping Q = 1/sqrt(2).  Keep the stored/automatable Q untouched so a
        // later Peak or Legacy selection recovers it, but never present that
        // inactive value as though it affected the shelf response.
        qFixedValueLabel.setText("FIXED 0.707", juce::dontSendNotification);
        qFixedValueLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
        qFixedValueLabel.setJustificationType(juce::Justification::centred);
        qFixedValueLabel.setColour(juce::Label::textColourId,
                                   ModernLookAndFeel::Colors::textSecondary);
        qFixedValueLabel.setColour(juce::Label::backgroundColourId,
                                   ModernLookAndFeel::Colors::bgDark.withAlpha(0.96f));
        qFixedValueLabel.setTooltip(
            "Surgical shelves use fixed Q = 1/sqrt(2) (0.707). The stored Q is preserved.");
        qFixedValueLabel.setComponentID("surgicalShelfFixedQ");
        qFixedValueLabel.setInterceptsMouseClicks(false, false);
        addChildComponent(qFixedValueLabel);

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
        typeCombo.setComponentID("filterTypeSelector");
        addAndMakeVisible(typeCombo);
        typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, prefix + "Type", typeCombo);
        typeCombo.addListener(this);

        // Curve topology selector. CurveMode is an append-only APVTS parameter
        // already present in the host surface; this control only exposes that
        // existing authority and therefore does not alter parameter ordering.
        curveModeCombo.addItem("Legacy", 1);
        curveModeCombo.addItem("Surgical", 2);
        curveModeCombo.setColour(juce::ComboBox::backgroundColourId,
                                 ModernLookAndFeel::Colors::bgDark);
        curveModeCombo.setColour(juce::ComboBox::textColourId,
                                 ModernLookAndFeel::Colors::textPrimary);
        curveModeCombo.setColour(juce::ComboBox::outlineColourId,
                                 ModernLookAndFeel::Colors::bgLighter);
        curveModeCombo.setTooltip("Filter topology: compatible Legacy curves or premium Surgical TPT-SVF");
        curveModeCombo.setComponentID("curveModeSelector");
        addAndMakeVisible(curveModeCombo);
        curveModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, prefix + "CurveMode", curveModeCombo);
        curveModeCombo.addListener(this);

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
        enableBtn.getProperties().set("inset", true);
        addAndMakeVisible(enableBtn);
        enableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, prefix + "Enabled", enableBtn);

        // Solo toggle — inset style (CNC-milled into chassis)
        soloBtn.setButtonText("SOLO");
        soloBtn.setClickingTogglesState(true);
        soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFFD700));
        soloBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        soloBtn.setTooltip("Solo this band (mutes all others)");
        soloBtn.getProperties().set("inset", true);
        addAndMakeVisible(soloBtn);
        soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, prefix + "Solo", soloBtn);

        // DynEQ mode selector
        // CRITICAL: Must match APVTS AudioParameterChoice indices exactly
        // APVTS: 0=Off, 1=Compress, 2=Expand, 3=legacy Gate.
        // Value 3 remains readable for host ABI compatibility but the new UI
        // cannot emit it; Expand+Below is the canonical gate configuration.
        // ComboBox IDs MUST be index+1 because JUCE treats ID 0 as "no selection"
        dynModeCombo.addItem("Off", 1);       // APVTS index 0
        dynModeCombo.addItem("Compress", 2);  // APVTS index 1
        dynModeCombo.addItem("Expand", 3);    // APVTS index 2
        dynModeCombo.addItem("Gate (Legacy)", 4); // APVTS index 3
        dynModeCombo.setItemEnabled(4, false);
        dynModeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgPanel);
        dynModeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        dynModeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        dynModeCombo.onChange = [this] { updateDynEQVisibility(); };
        dynModeCombo.setComponentID("dynamicActionSelector");
        addAndMakeVisible(dynModeCombo);

        dynTriggerCombo.addItem("Above", 1);
        dynTriggerCombo.addItem("Below", 2);
        dynTriggerCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgPanel);
        dynTriggerCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        dynTriggerCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        dynTriggerCombo.setComponentID("dynamicTriggerSelector");
        addAndMakeVisible(dynTriggerCombo);

        // Detector controls are contextual: hidden while DynEQ is Off.  The
        // source names mirror the append-only APVTS choice ordering exactly.
        detectionModeCombo.addItem("Peak", 1);
        detectionModeCombo.addItem("RMS", 2);
        detectionModeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgPanel);
        detectionModeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        detectionModeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        detectionModeCombo.setTooltip("Detector envelope: Peak amplitude or RMS power");
        detectionModeCombo.setComponentID("detectionModeSelector");
        addAndMakeVisible(detectionModeCombo);

        detectorSourceCombo.addItem("Internal Wideband", 1);
        detectorSourceCombo.addItem("Internal Filtered", 2);
        detectorSourceCombo.addItem("External Wideband", 3);
        detectorSourceCombo.addItem("External Filtered", 4);
        detectorSourceCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgPanel);
        detectorSourceCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary);
        detectorSourceCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        detectorSourceCombo.setTooltip("Signal that drives this band's dynamic detector");
        detectorSourceCombo.setComponentID("detectorSourceSelector");
        addAndMakeVisible(detectorSourceCombo);
        detectorSourceCombo.addListener(this);

        detectorAvailabilityLabel.setFont(
            juce::Font(juce::FontOptions().withHeight(8.5f).withStyle("Bold")));
        detectorAvailabilityLabel.setJustificationType(juce::Justification::centred);
        detectorAvailabilityLabel.setComponentID("detectorAvailabilityStatus");
        detectorAvailabilityLabel.setInterceptsMouseClicks(false, false);
        addChildComponent(detectorAvailabilityLabel);

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
        makeDynLabel(rngLabel, "RNG");
        makeDynLabel(kneLabel, "KNE");
        makeDynLabel(scFreqLabel, "SC FREQ");
        makeDynLabel(scQLabel, "SC Q");
        addAndMakeVisible(thrLabel);
        addAndMakeVisible(ratLabel);
        addAndMakeVisible(atkLabel);
        addAndMakeVisible(relLabel);
        addAndMakeVisible(rngLabel);
        addAndMakeVisible(kneLabel);
        addAndMakeVisible(scFreqLabel);
        addAndMakeVisible(scQLabel);

        // DynEQ knobs (SmallBlue style, 10px textbox below) — all in inspector (no graph overlay)
        thresholdKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        ratioKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        attackKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        releaseKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        rangeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        kneeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        sidechainFreqKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 10);
        sidechainQKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 10);
        addAndMakeVisible(thresholdKnob);
        addAndMakeVisible(ratioKnob);
        addAndMakeVisible(attackKnob);
        addAndMakeVisible(releaseKnob);
        addAndMakeVisible(rangeKnob);
        addAndMakeVisible(kneeKnob);
        sidechainFreqKnob.setComponentID("sidechainFrequencyControl");
        sidechainQKnob.setComponentID("sidechainQControl");
        addAndMakeVisible(sidechainFreqKnob);
        addAndMakeVisible(sidechainQKnob);

        // DynEQ APVTS attachments
        dynModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynMode", dynModeCombo);
        dynTriggerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynTrigger", dynTriggerCombo);
        detectionModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DetectionMode", detectionModeCombo);
        detectorSourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DetectorSource", detectorSourceCombo);
        thrAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Threshold", thresholdKnob);
        ratAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Ratio", ratioKnob);
        atkAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Attack", attackKnob);
        relAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Release", releaseKnob);
        rngAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Range", rangeKnob);
        kneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Knee", kneeKnob);
        sidechainFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "SidechainFreq", sidechainFreqKnob);
        sidechainQAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "SidechainQ", sidechainQKnob);

        updateSlopeVisibility();
        updateDynEQVisibility();
        refreshRuntimeSemantics();

        // Premium caching: buffer the entire panel as a GPU-backed image.
        // Child components (knobs) repaint independently without triggering
        // a full parent redraw. Huge win when only a knob is rotating.
        setBufferedToImage(true);
    }

    ~BandControlPanel() override
    {
        typeCombo.removeListener(this);
        curveModeCombo.removeListener(this);
        detectorSourceCombo.removeListener(this);
    }

    void comboBoxChanged(juce::ComboBox* combo) override
    {
        if (combo == &typeCombo)
            updateSlopeVisibility();

        if (combo == &typeCombo || combo == &curveModeCombo)
            updateQSemanticState();

        if (combo == &detectorSourceCombo)
        {
            updateDetectorVisibility();
            resized();
            repaint();
        }
    }

    void setBandIndex(int newIndex)
    {
        if (newIndex == bandIndex || newIndex < 0) return;

        typeCombo.removeListener(this);
        curveModeCombo.removeListener(this);
        detectorSourceCombo.removeListener(this);

        freqAtt.reset(); gainAtt.reset(); qAtt.reset();
        typeAtt.reset(); slopeAtt.reset(); curveModeAtt.reset();
        enableAtt.reset(); soloAtt.reset();
        dynModeAtt.reset(); dynTriggerAtt.reset(); detectionModeAtt.reset(); detectorSourceAtt.reset();
        thrAtt.reset(); ratAtt.reset(); atkAtt.reset(); relAtt.reset(); rngAtt.reset(); kneAtt.reset();
        sidechainFreqAtt.reset(); sidechainQAtt.reset();

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
        curveModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "CurveMode", curveModeCombo);
        enableAtt= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Enabled", enableBtn);
        soloAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(parameters, prefix + "Solo",    soloBtn);

        // DynEQ APVTS attachments
        dynModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynMode", dynModeCombo);
        dynTriggerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DynTrigger", dynTriggerCombo);
        detectionModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DetectionMode", detectionModeCombo);
        detectorSourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(parameters, prefix + "DetectorSource", detectorSourceCombo);
        thrAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Threshold", thresholdKnob);
        ratAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Ratio", ratioKnob);
        atkAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Attack", attackKnob);
        relAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Release", releaseKnob);
        rngAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Range", rangeKnob);
        kneAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "Knee", kneeKnob);
        sidechainFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "SidechainFreq", sidechainFreqKnob);
        sidechainQAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(parameters, prefix + "SidechainQ", sidechainQKnob);

        typeCombo.addListener(this);
        curveModeCombo.addListener(this);
        detectorSourceCombo.addListener(this);
        updateSlopeVisibility();
        updateDynEQVisibility();
        runtimeEligibilityInitialized = false;
        refreshRuntimeSemantics();
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
            // Neumorphic recessed panel: darker fill + inner shadow gradient
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.55f));
            g.fillRoundedRectangle(kb, 4.0f);

            // Inner shadow: 4px top gradient (dark → transparent) for depth
            {
                juce::ColourGradient innerShadow(
                    juce::Colours::black.withAlpha(0.25f), kb.getX(), kb.getY(),
                    juce::Colours::transparentBlack,       kb.getX(), kb.getY() + 4.0f,
                    false);
                g.setGradientFill(innerShadow);
                g.fillRoundedRectangle(kb.withHeight(4.0f), 4.0f);
            }

            // Bottom highlight: thin light line simulating light hitting the recess edge
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine(static_cast<int>(kb.getBottom() - 1.0f),
                                 kb.getX() + 4.0f, kb.getRight() - 4.0f);

            // Accent border (band color)
            g.setColour(bandColor.withAlpha(0.18f));
            g.drawRoundedRectangle(kb, 4.0f, 1.0f);
        }

        // DynEQ knob cluster backdrop (blue accent, neumorphic recess)
        if (!compact && dynEQActive && !dynKnobClusterBounds.isEmpty())
        {
            auto dkb = dynKnobClusterBounds.toFloat().expanded(2.0f, 2.0f);
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.55f));
            g.fillRoundedRectangle(dkb, 4.0f);

            // Inner shadow (same depth treatment as main cluster)
            {
                juce::ColourGradient innerShadow(
                    juce::Colours::black.withAlpha(0.25f), dkb.getX(), dkb.getY(),
                    juce::Colours::transparentBlack,       dkb.getX(), dkb.getY() + 4.0f,
                    false);
                g.setGradientFill(innerShadow);
                g.fillRoundedRectangle(dkb.withHeight(4.0f), 4.0f);
            }

            // Bottom highlight
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine(static_cast<int>(dkb.getBottom() - 1.0f),
                                 dkb.getX() + 4.0f, dkb.getRight() - 4.0f);

            g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.15f));
            g.drawRoundedRectangle(dkb, 4.0f, 1.0f);
        }

        // 360° inner bevel — simulates milled aluminium panel edge.
        // Top+Left: highlight (light hitting the top-left at "10 o'clock").
        // Bottom+Right: shadow (depth on the opposite corner).
        // Drawn INSTEAD of the old flat bgLighter border for a 3D feel.
        if (!compact)
        {
            auto bevelRect = getLocalBounds().toFloat().reduced(1.5f);
            const float cr = 4.0f;

            // Top highlight
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawHorizontalLine(static_cast<int>(bevelRect.getY()),
                                 bevelRect.getX() + cr, bevelRect.getRight() - cr);
            // Left highlight
            g.drawVerticalLine(static_cast<int>(bevelRect.getX()),
                               bevelRect.getY() + cr, bevelRect.getBottom() - cr);

            // Bottom shadow
            g.setColour(juce::Colours::black.withAlpha(0.14f));
            g.drawHorizontalLine(static_cast<int>(bevelRect.getBottom()),
                                 bevelRect.getX() + cr, bevelRect.getRight() - cr);
            // Right shadow
            g.drawVerticalLine(static_cast<int>(bevelRect.getRight()),
                               bevelRect.getY() + cr, bevelRect.getBottom() - cr);
        }
        else
        {
            // Compact mode: keep simple border
            g.setColour(ModernLookAndFeel::Colors::bgLighter);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
        }
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

            // Detector detail is an inspector-only surface. Compact rows keep
            // the Curve selector, but must not retain stale bounds from a prior
            // vertical layout when the editor is resized.
            detectionModeCombo.setBounds({});
            detectorSourceCombo.setBounds({});
            detectorAvailabilityLabel.setBounds({});
            sidechainFreqKnob.setBounds({});
            sidechainQKnob.setBounds({});
            scFreqLabel.setBounds({});
            scQLabel.setBounds({});

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

            curveModeCombo.setBounds(bounds.removeFromLeft(78).reduced(0, 4));
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
            qFixedValueLabel.setBounds(qArea.removeFromBottom(14));
            qKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        }
        else
        {
            // === VERTICAL LAYOUT (Liquid Intelligence: 3 big LargeAmber knobs + DynEQ) ===
            bounds.removeFromTop(2);
            bandLabel.setBounds(bounds.removeFromTop(16));
            bounds.removeFromTop(2);

            // Type + Curve topology + ON/SOLO inline row (22px). Keeping Curve
            // on this row preserves vertical room for the six DynEQ controls.
            auto typeRow = bounds.removeFromTop(22);
            typeLabel.setVisible(false);
            auto selectorArea = typeRow.removeFromLeft(typeRow.getWidth() - 80);
            const int curveWidth = juce::jmin(100, selectorArea.getWidth() * 2 / 5);
            typeCombo.setBounds(selectorArea.removeFromLeft(selectorArea.getWidth() - curveWidth).reduced(2, 0));
            curveModeCombo.setBounds(selectorArea.reduced(2, 0));
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

            // DynEQ action/trigger row (22px) — inspector only, no expand overlay
            auto dynModeRow = bounds.removeFromTop(22);
            const int actionWidth = dynModeRow.getWidth() * 3 / 5;
            dynModeCombo.setBounds(dynModeRow.removeFromLeft(actionWidth).reduced(2, 0));
            dynTriggerCombo.setBounds(dynModeRow.reduced(2, 0));
            bounds.removeFromTop(2);

            if (dynEQActive)
            {
                auto detectorRow = bounds.removeFromTop(22);
                detectionModeCombo.setBounds(detectorRow.removeFromLeft(76).reduced(2, 0));
                if (detectorAvailabilityLabel.isVisible())
                    detectorAvailabilityLabel.setBounds(
                        detectorRow.removeFromRight(62).reduced(2, 2));
                detectorSourceCombo.setBounds(detectorRow.reduced(2, 0));
                bounds.removeFromTop(2);
            }

            // Bottom-up layout: DynEQ knobs (if active) + main knobs
            // Enable/Solo row already placed inline with type combo

            // DynEQ knob rows — all six params live here (no center modal)
            if (dynEQActive)
            {
                auto placeDynKnob = [](juce::Rectangle<int> cell, juce::Label& label, juce::Component& knob)
                {
                    const int dynLabelH = 10;
                    label.setBounds(cell.removeFromTop(dynLabelH));
                    knob.setBounds(cell);
                };

                auto dynRow2 = bounds.removeFromBottom(46);
                auto dynRow1 = bounds.removeFromBottom(46);
                dynKnobClusterBounds = dynRow1.getUnion(dynRow2);

                const int dynKnobW1 = dynRow1.getWidth() / 4;
                placeDynKnob(dynRow1.removeFromLeft(dynKnobW1), thrLabel, thresholdKnob);
                placeDynKnob(dynRow1.removeFromLeft(dynKnobW1), ratLabel, ratioKnob);
                placeDynKnob(dynRow1.removeFromLeft(dynKnobW1), atkLabel, attackKnob);
                placeDynKnob(dynRow1, relLabel, releaseKnob);

                if (sidechainFilterVisible)
                {
                    const int dynKnobW2 = dynRow2.getWidth() / 4;
                    placeDynKnob(dynRow2.removeFromLeft(dynKnobW2), rngLabel, rangeKnob);
                    placeDynKnob(dynRow2.removeFromLeft(dynKnobW2), kneLabel, kneeKnob);
                    placeDynKnob(dynRow2.removeFromLeft(dynKnobW2), scFreqLabel, sidechainFreqKnob);
                    placeDynKnob(dynRow2, scQLabel, sidechainQKnob);
                }
                else
                {
                    const int dynKnobW2 = dynRow2.getWidth() / 2;
                    placeDynKnob(dynRow2.removeFromLeft(dynKnobW2), rngLabel, rangeKnob);
                    placeDynKnob(dynRow2, kneLabel, kneeKnob);
                }

                bounds.removeFromBottom(2); // gap above DynEQ rows
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
            qFixedValueLabel.setBounds(qArea.removeFromBottom(20));
        }
    }

    int getBandIndex() const { return bandIndex; }

    // Message-thread bridge for runtime bus telemetry.  Only the selected
    // source is stored in APVTS; DAW bus availability must never be serialized.
    void setExternalDetectorAvailable(bool available)
    {
        if (externalDetectorAvailable == available)
            return;

        externalDetectorAvailable = available;
        updateDetectorAvailabilityDisplay();
    }

    // Called by the editor heartbeat so host automation of the global DynEQ
    // switch or per-band Enabled flag is reflected without adding GUI work to
    // the audio-thread parameter listener.
    void refreshRuntimeSemantics()
    {
        const auto* global = parameters.getRawParameterValue("dynEqEnabled");
        const auto* enabled = parameters.getRawParameterValue(
            "band" + juce::String(bandIndex) + "Enabled");
        const bool eligible = global != nullptr && global->load() > 0.5f
                           && enabled != nullptr && enabled->load() > 0.5f;
        if (runtimeEligibilityInitialized && eligible == runtimeBandEnabled)
            return;

        runtimeEligibilityInitialized = true;
        runtimeBandEnabled = eligible;
        updateQSemanticState();
    }

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
        // Note: ComboBox IDs are 1-based (1=Off, 2=Compress, 3=Expand,
        // 4=legacy Gate read-only).
        // APVTS indices are 0-based, handled by ComboBoxAttachment automatically
        dynEQActive = (dynModeCombo.getSelectedId() > 1); // 1 = Off
        thresholdKnob.setVisible(dynEQActive);
        ratioKnob.setVisible(dynEQActive);
        attackKnob.setVisible(dynEQActive);
        releaseKnob.setVisible(dynEQActive);
        rangeKnob.setVisible(dynEQActive);
        kneeKnob.setVisible(dynEQActive);
        thrLabel.setVisible(dynEQActive);
        ratLabel.setVisible(dynEQActive);
        atkLabel.setVisible(dynEQActive);
        relLabel.setVisible(dynEQActive);
        rngLabel.setVisible(dynEQActive);
        kneLabel.setVisible(dynEQActive);
        updateDetectorVisibility();
        updateQSemanticState();
        resized();
        repaint();
    }

    void updateDetectorVisibility()
    {
        detectionModeCombo.setVisible(dynEQActive);
        detectorSourceCombo.setVisible(dynEQActive);

        // Combo IDs are source index+1. Only Internal Filtered (2) and
        // External Filtered (4) consume the sidechain filter controls.
        const int sourceId = detectorSourceCombo.getSelectedId();
        sidechainFilterVisible = dynEQActive && (sourceId == 2 || sourceId == 4);
        sidechainFreqKnob.setVisible(sidechainFilterVisible);
        sidechainQKnob.setVisible(sidechainFilterVisible);
        scFreqLabel.setVisible(sidechainFilterVisible);
        scQLabel.setVisible(sidechainFilterVisible);
        updateDetectorAvailabilityDisplay();
    }

    void updateDetectorAvailabilityDisplay()
    {
        const int sourceId = detectorSourceCombo.getSelectedId();
        const bool externalSelected = sourceId == 3 || sourceId == 4;
        const bool show = dynEQActive && externalSelected;

        detectorAvailabilityLabel.setVisible(show);
        if (! show)
            return;

        detectorAvailabilityLabel.setText(
            externalDetectorAvailable ? "SC READY" : "SC MISSING",
            juce::dontSendNotification);
        detectorAvailabilityLabel.setColour(
            juce::Label::textColourId,
            externalDetectorAvailable ? juce::Colour(0xff70d69a)
                                      : juce::Colour(0xffff756f));
        detectorAvailabilityLabel.setTooltip(
            externalDetectorAvailable
                ? "The DAW external sidechain bus is connected and driving this detector."
                : "External detector selected, but the DAW sidechain bus is unavailable.");
        resized();
        repaint();
    }

    void updateQSemanticState()
    {
        // Combo IDs are 1-based: Low Shelf=2, High Shelf=4 and
        // CurveMode Surgical=2. Peak Surgical intentionally retains user Q.
        const int typeId = typeCombo.getSelectedId();
        const bool isShelf = typeId == 2 || typeId == 4;
        const bool dynamicOwnsBand = runtimeBandEnabled && dynEQActive
            && (typeId == 2 || typeId == 3 || typeId == 4);
        const bool isSurgical = curveModeCombo.getSelectedId() == 2;
        const bool fixedShelfQ = isShelf && isSurgical && ! dynamicOwnsBand;

        qKnob.setEnabled(!fixedShelfQ);
        qKnob.setAlpha(fixedShelfQ ? 0.32f : 1.0f);
        qLabel.setText(fixedShelfQ ? "Q FIXED"
                                  : (dynamicOwnsBand && isShelf ? "Q DYN" : "Q"),
                       juce::dontSendNotification);
        qLabel.setColour(juce::Label::textColourId,
                         fixedShelfQ ? ModernLookAndFeel::Colors::textMuted
                                     : ModernLookAndFeel::Colors::textSecondary);
        qFixedValueLabel.setVisible(fixedShelfQ);
        curveModeCombo.setEnabled(! dynamicOwnsBand);
        curveModeCombo.setAlpha(dynamicOwnsBand ? 0.42f : 1.0f);
        curveModeCombo.setTooltip(
            dynamicOwnsBand
                ? "CurveMode controls the static EQ path. This band is currently owned by the dynamic EQ path."
                : "Filter topology: compatible Legacy curves or premium Surgical TPT-SVF");
        if (fixedShelfQ)
            qFixedValueLabel.toFront(false);
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
    juce::Label freqLabel, gainLabel, qLabel, qFixedValueLabel, typeLabel, slopeLabel;
    // Phase 4 (completion): 3 filmstrip LargeAmber knobs for Freq / Gain / Q
    // Empty custom label — we use the external juce::Label next to each knob.
    PremiumKnob freqKnob { juce::String(), PremiumKnob::Style::LargeAmber };
    PremiumKnob gainKnob { juce::String(), PremiumKnob::Style::LargeAmber };
    PremiumKnob qKnob    { juce::String(), PremiumKnob::Style::LargeAmber };
    juce::ComboBox typeCombo, slopeCombo, curveModeCombo;
    juce::TextButton enableBtn, soloBtn;

    // DynEQ mode selector (always visible)
    juce::ComboBox dynModeCombo, dynTriggerCombo, detectionModeCombo, detectorSourceCombo;

    // DynEQ knobs (visible only when mode != Off) — Range/Knee included (no overlay)
    PremiumKnob thresholdKnob { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob ratioKnob     { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob attackKnob    { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob releaseKnob   { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob rangeKnob     { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob kneeKnob      { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob sidechainFreqKnob { juce::String(), PremiumKnob::Style::SmallBlue };
    PremiumKnob sidechainQKnob    { juce::String(), PremiumKnob::Style::SmallBlue };
    juce::Label thrLabel, ratLabel, atkLabel, relLabel, rngLabel, kneLabel, scFreqLabel, scQLabel;
    juce::Label detectorAvailabilityLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, slopeAtt, curveModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtt, soloAtt;

    // DynEQ APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dynModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dynTriggerAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectionModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectorSourceAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thrAtt, ratAtt, atkAtt, relAtt, rngAtt, kneAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sidechainFreqAtt, sidechainQAtt;

    // Layout state
    juce::Rectangle<int> dynKnobClusterBounds;
    bool dynEQActive = false;
    bool sidechainFilterVisible = false;
    bool externalDetectorAvailable = false;
    bool runtimeBandEnabled = false;
    bool runtimeEligibilityInitialized = false;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControlPanel)
};
