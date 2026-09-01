#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <optional>
#include "../AI/SemanticEQEngine.h"
#include "../AI/SemanticPlanningService.h"
#include "../Integration/EmberProposalProtocol.h"
#include "ModernLookAndFeel.h"
#include "SemanticIntentMap.h"
#include <vector>
#if AIEQ_GUI_DEBUG
#include "../Utils/DebugLog.h"
#endif

//==============================================================================
/**
 * Semantic Control Panel - Natural Language EQ Interface
 * 
 * This panel allows users to control EQ through timbral descriptors
 * rather than technical parameters. Users can:
 * - Adjust sliders for qualities like "Air", "Warmth", "Punch"
 * - Type natural language commands like "more air" or "warmer sound"
 * - See real-time visual feedback of EQ changes
 * 
 * Design Philosophy:
 * - Musician-friendly terminology
 * - Visual feedback showing EQ impact
 * - Smooth morphing between states
 * - Learning from user adjustments
 */
class SemanticControlPanel : public juce::Component,
                             public juce::Timer,
                             public juce::Slider::Listener,
                             public juce::TextEditor::Listener
{
public:
    //==========================================================================
    // Callback when semantic state changes
    std::function<void(const SemanticEQEngine::SemanticState&)> onStateChanged;
    
    // Callback to get generated EQ adjustments (legacy slider/preset path).
    std::function<void(const std::vector<SemanticEQEngine::SemanticEQAdjustment>&)> onEQGenerated;

    struct TextApplyFeedback
    {
        int requestedBands = 0;
        int appliedBands = 0;
        int rejectedBands = 0;
        bool atomicRejected = false;
        bool deferred = false;
        std::vector<int> appliedBandSlots;

        [[nodiscard]] bool complete() const noexcept
        {
            return !atomicRejected && !deferred
                && rejectedBands == 0 && appliedBands == requestedBands;
        }
    };

    // Typed PLAN/APPLY path: must use all-or-nothing processor policy and return
    // structured feedback instead of silently dropping bands.
    std::function<TextApplyFeedback(
        const std::vector<SemanticEQEngine::SemanticEQAdjustment>&)> onTextPlanApply;

    std::function<void(const EmberUI::SemanticIntentMapState&)> onIntentMapChanged;
    std::function<void(bool)> onIntentMapHover;

    /** T5.3 - the source context to plan against, taken on the message thread
        at the instant PLAN is pressed and then frozen into the request. Left
        unset the planner degrades to text-only, which is exactly what an
        invalid context already means downstream. */
    std::function<std::optional<AIEQPerceptual::SpectralContext>()> onRequestSpectralContext;

    /** Message-thread snapshot of user Hz fences at PLAN. Unset means none.
        The panel must not read the processor or the graph. */
    std::function<std::vector<AIEQPerceptual::SemanticProtectedRange>()> onRequestProtectedRanges;

    std::function<void(bool)> onEmberLinkToggled;
    std::function<void()> onEmberPairClicked;
    std::function<void(bool /*alreadyStaged*/)> onExternalProposalInvalidated;
    std::function<void(EmberProposal::ReasonCode, std::string, std::string)> onExternalPlanResult;
    std::function<void(std::string)> onExternalApplied;

    void setEmberLinkUi(const EmberProposal::LinkUiState& ui)
    {
        emberLinkToggle.setToggleState(ui.linkEnabled, juce::dontSendNotification);
        emberLinkToggle.setButtonText(ui.linkEnabled ? "LINK ON" : "LINK OFF");
        emberPairButton.setEnabled(ui.linkEnabled && !ui.paired);
        emberPairButton.setButtonText("PAIR");
        emberLinkStatus.setText(ui.statusText, juce::dontSendNotification);
        if (!ui.pendingSource.empty())
            pendingProposalSource = juce::String::fromUTF8(ui.pendingSource.c_str());
        else if (!isPendingExternal())
            pendingProposalSource.clear();
    }

    void stageExternalCommand(const juce::String& phrase, float intensity01)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        suppressExternalInvalidate = true;
        invalidatePendingTextPlan();
        suppressExternalInvalidate = false;
        pendingProposalSource = EmberProposal::kPendingSourceLabel;
        intensitySlider.setValue(static_cast<double>(intensity01), juce::dontSendNotification);
        semanticEngine.setIntensity(intensity01);
        commandInput.setText(phrase, juce::dontSendNotification);
        submitPlanForText(phrase);
    }

    [[nodiscard]] bool isPendingExternal() const
    {
        return pendingProposalSource.isNotEmpty();
    }

    // Supplies the current smoothed dB spectrum (analyzer format: numBins,
    // dB values) for the engine's context-aware mapping. Wired by the editor;
    // when unset the engine falls back to context-neutral behaviour (A3 fix —
    // previously this path ALWAYS received an empty spectrum).
    std::function<std::vector<float>()> spectrumProvider;

    // Must be called when the host sample rate changes (e.g. from PluginEditor::prepareToPlay)
    void setSampleRate(double sr)
    {
        if (currentSampleRate != sr)
        {
            currentSampleRate = sr;
            invalidatePendingTextPlan();
            clearResponseStrip();
        }
    }

    //==========================================================================
    explicit SemanticControlPanel(SemanticEQEngine& engine)
        : semanticEngine(engine)
    {
        setOpaque(true);
        
        // Title
        titleLabel.setText("SEMANTIC CONTROL", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(14.0f));
            font.setBold(true);
            titleLabel.setFont(font);
        }
        titleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentYellow);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Subtitle
        subtitleLabel.setText("Shape your sound with words", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(10.0f));
            font.setItalic(true);
            subtitleLabel.setFont(font);
        }
        subtitleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        subtitleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(subtitleLabel);
        
        // Natural language input
        commandInput.setMultiLine(false);
        commandInput.setReturnKeyStartsNewLine(false);
        commandInput.setTextToShowWhenEmpty("Type: \"more air\", \"warmer\", \"add punch\"...",
                                            ModernLookAndFeel::Colors::textMuted);
        commandInput.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        commandInput.setColour(juce::TextEditor::backgroundColourId, ModernLookAndFeel::Colors::bgDark);
        commandInput.setColour(juce::TextEditor::textColourId, ModernLookAndFeel::Colors::textBright);
        commandInput.setColour(juce::TextEditor::outlineColourId, ModernLookAndFeel::Colors::bgLighter);
        commandInput.setColour(juce::TextEditor::focusedOutlineColourId, ModernLookAndFeel::Colors::accentYellow);
        commandInput.addListener(this);
        commandInput.setComponentID("semanticCommandInput");
        addAndMakeVisible(commandInput);
        
        // Apply button for text input
        applyButton.setButtonText("PLAN");
        applyButton.setComponentID("semanticPlanButton");
        applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
        applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        applyButton.onClick = [this]() { applyTextCommand(); };
        addAndMakeVisible(applyButton);

        emberLinkToggle.setButtonText("LINK OFF");
        emberLinkToggle.setClickingTogglesState(true);
        emberLinkToggle.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLighter);
        emberLinkToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF2D5A27));
        emberLinkToggle.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textLabel);
        emberLinkToggle.setTooltip("Ember Link: optional Ableton Copilot proposals. Off by default. APPLY stays local.");
        emberLinkToggle.setComponentID("emberLinkToggle");
        emberLinkToggle.onClick = [this]() {
            const bool on = emberLinkToggle.getToggleState();
            emberLinkToggle.setButtonText(on ? "LINK ON" : "LINK OFF");
            if (onEmberLinkToggled)
                onEmberLinkToggled(on);
        };
        addAndMakeVisible(emberLinkToggle);

        emberPairButton.setButtonText("PAIR");
        emberPairButton.setEnabled(false);
        emberPairButton.setComponentID("emberPairButton");
        emberPairButton.setTooltip("Send a one-time pair offer to Ableton Copilot Bridge");
        emberPairButton.onClick = [this]() {
            if (onEmberPairClicked)
                onEmberPairClicked();
        };
        addAndMakeVisible(emberPairButton);

        emberLinkStatus.setText("Link off", juce::dontSendNotification);
        emberLinkStatus.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        emberLinkStatus.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        emberLinkStatus.setJustificationType(juce::Justification::centredLeft);
        emberLinkStatus.setInterceptsMouseClicks(false, false);
        emberLinkStatus.setComponentID("emberLinkStatus");
        addAndMakeVisible(emberLinkStatus);
        
        // Initialize quality sliders - Main qualities
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Air, "AIR", "Aria",
                          juce::Colour(0xFF60D4E8), "High frequency sparkle and openness");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Warmth, "WARMTH", "Calore",
                          juce::Colour(0xFFE8A030), "Low-mid fullness, analog feel");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Punch, "PUNCH", "Punch",
                          juce::Colour(0xFFE86060), "Attack and percussive impact");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Clarity, "CLARITY", "Chiarezza",
                          juce::Colour(0xFF5ED4A0), "Definition, reduced muddiness");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Body, "BODY", "Corpo",
                          juce::Colour(0xFF4AA8D4), "Weight and substance");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Brilliance, "BRILLIANCE", "Brillantezza",
                          juce::Colour(0xFF60D4E8), "Crystal clear highs");
        
        // Secondary qualities (collapsible)
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Smoothness, "SMOOTH", "Morbido",
                          juce::Colour(0xFF95A5A6), "Reduce harshness");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Weight, "WEIGHT", "Peso",
                          juce::Colour(0xFF34495E), "Sub-bass presence");
        
        // Intensity control
        intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        intensitySlider.setRange(0.0, 2.0, 0.01);
        intensitySlider.setValue(1.0);
        intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
        intensitySlider.setColour(juce::Slider::thumbColourId, ModernLookAndFeel::Colors::accentYellow);
        intensitySlider.setColour(juce::Slider::trackColourId, ModernLookAndFeel::Colors::bgLighter);
        intensitySlider.onValueChange = [this]() {
            invalidatePendingTextPlan();
            clearResponseStrip();
            semanticEngine.setIntensity(static_cast<float>(intensitySlider.getValue()));
            semanticDirty = true;
        };
        addAndMakeVisible(intensitySlider);
        
        intensityLabel.setText("INTENSITY", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(9.0f));
            font.setBold(true);
            intensityLabel.setFont(font);
        }
        intensityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
        addAndMakeVisible(intensityLabel);
        
        // Preset buttons
        setupPresetButtons();
        
        // Reset button
        resetButton.setButtonText("RESET");
        resetButton.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLighter);
        resetButton.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textLabel);
        resetButton.onClick = [this]() { resetAllSliders(); };
        addAndMakeVisible(resetButton);
        
        // Morph button
        morphButton.setButtonText("MORPH");
        morphButton.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
        morphButton.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::accentYellow);
        morphButton.setClickingTogglesState(true);
        morphButton.setTooltip("Enable smooth transitions between states");
        addAndMakeVisible(morphButton);
        
        // UI-A1: PLAN response lives beside the request, not in the footer.
        // Chip copy is a 1:1 map of planner/apply outcomes; detail is the
        // existing planner/apply string. Idle has no chip and no text.
        //
        // MOV 09:05 freeze (Ableton SEMANTIC): UI-A locus PASS — CAN'T PLAN
        // + "Couldn't understand command" under the request is truthful
        // planner output, not a layout fail. Do not relocate the strip.
        // UI-A.1 PASS — tracks must not run through AIR/WARMTH/PUNCH.
        // APPLY unproven this take. Out of tranche: planner synonyms,
        // chrome compression, GRAPH-GRID-G2/A3.
        {
            auto chipFont = juce::Font(juce::FontOptions().withHeight(11.5f));
            chipFont.setBold(true);
            responseChip.setFont(chipFont);
            responseChip.setJustificationType(juce::Justification::centredLeft);
            responseChip.setMinimumHorizontalScale(1.0f);
            responseChip.setBorderSize({});
            responseChip.setInterceptsMouseClicks(false, false);
            addChildComponent(responseChip);

            responseDetail.setFont(juce::Font(juce::FontOptions().withHeight(10.5f)));
            responseDetail.setColour(juce::Label::textColourId,
                                     ModernLookAndFeel::Colors::textSecondary);
            responseDetail.setJustificationType(juce::Justification::centredLeft);
            responseDetail.setMinimumHorizontalScale(1.0f);
            responseDetail.setBorderSize({});
            addChildComponent(responseDetail);
        }
        
        planningService.start();
        startTimerHz(30);
    }
    
    ~SemanticControlPanel() override
    {
        stopTimer();
        // Join before any member the worker could still be publishing into is
        // destroyed. stop() is idempotent and waits; it must not be a timeout.
        planningService.stop();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgMid);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

        // Top accent
        g.setColour(ModernLookAndFeel::Colors::accentYellow);
        g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), 3.0f);

        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

        // Divider sits under the reserved response strip so PLAN feedback
        // stays in the request locus instead of being painted over the axes.
        if (responseDividerY > 0)
        {
            g.setColour(ModernLookAndFeel::Colors::bgLight);
            g.drawHorizontalLine(responseDividerY, 10.0f,
                                 static_cast<float>(getWidth() - 10));
        }
        
        // Draw center notch marks on quality sliders
        for (const auto& qs : qualitySliders)
        {
            if (qs.slider && qs.slider->isVisible())
            {
                auto sb = qs.slider->getBounds().toFloat();
                // Notch the bipolar 0 of the TRACK, not the label or value box.
                const float trackW = juce::jmax(1.0f,
                    sb.getWidth() - (float) qs.slider->getTextBoxWidth());
                float centerX = sb.getX() + trackW * 0.5f;
                float topY = sb.getY() + 4.0f;
                float botY = sb.getBottom() - 4.0f;
                g.setColour(ModernLookAndFeel::Colors::textMuted);
                g.drawLine(centerX, topY, centerX, botY, 1.0f);
                // Small triangle marker at bottom
                juce::Path tri;
                tri.addTriangle(centerX - 3.0f, botY + 1.0f, centerX + 3.0f, botY + 1.0f, centerX, botY - 2.0f);
                g.setColour(ModernLookAndFeel::Colors::textMuted);
                g.fillPath(tri);
            }
        }

        // Draw active quality visualizer (only in full mode, not compact)
        if (getHeight() >= 200)
            drawQualityVisualizer(g);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        const bool compact = bounds.getHeight() < 200;
        responseDividerY = 0;
        responseStripBounds = {};

        if (compact)
        {
            // Compact <200 px: response strip is not guaranteed. This is the
            // pre-UI-A behaviour (status was already hidden here). Do not treat
            // a missing strip in compact as a regression of this tranche.
            bounds.reduce(6, 4);

            titleLabel.setVisible(false);
            subtitleLabel.setVisible(false);
            intensitySlider.setVisible(false);
            intensityLabel.setVisible(false);
            resetButton.setVisible(false);
            morphButton.setVisible(false);
            emberLinkToggle.setVisible(false);
            emberPairButton.setVisible(false);
            emberLinkStatus.setVisible(false);
            responseChip.setVisible(false);
            responseDetail.setVisible(false);
            for (auto& btn : presetButtons) btn->setVisible(false);

            auto inputArea = bounds.removeFromBottom(24);
            bounds.removeFromBottom(3);

            int sliderH = 16;
            for (auto& qs : qualitySliders)
            {
                if (bounds.getHeight() < sliderH)
                    break;

                auto row = bounds.removeFromTop(sliderH);
                qs.label->setBounds(row.removeFromLeft(58));
                qs.label->setVisible(true);
                qs.slider->setBounds(row);
                qs.slider->setVisible(true);
                qs.slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                qs.bounds = row;
                bounds.removeFromTop(1);
            }

            applyButton.setBounds(inputArea.removeFromRight(52).reduced(1));
            commandInput.setBounds(inputArea.reduced(0, 1));
        }
        else
        {
            bounds.reduce(12, 0);
            titleLabel.setVisible(true);
            subtitleLabel.setVisible(true);
            intensitySlider.setVisible(true);
            intensityLabel.setVisible(true);
            resetButton.setVisible(true);
            morphButton.setVisible(true);
            emberLinkToggle.setVisible(true);
            emberPairButton.setVisible(true);
            emberLinkStatus.setVisible(true);
            for (auto& btn : presetButtons) btn->setVisible(true);

            titleLabel.setBounds(bounds.removeFromTop(20));
            subtitleLabel.setBounds(bounds.removeFromTop(16));
            bounds.removeFromTop(4);

            auto linkRow = bounds.removeFromTop(22);
            emberLinkToggle.setBounds(linkRow.removeFromLeft(72).reduced(1));
            emberPairButton.setBounds(linkRow.removeFromLeft(48).reduced(1));
            emberLinkStatus.setBounds(linkRow.reduced(4, 0));
            bounds.removeFromTop(4);

            auto inputRow = bounds.removeFromTop(28);
            applyButton.setBounds(inputRow.removeFromRight(60).reduced(2));
            commandInput.setBounds(inputRow.reduced(0, 2));
            bounds.removeFromTop(4);

            // Fixed-height response strip directly under request+PLAN so the
            // axes never shift when PLAN returns a long or short string.
            responseStripBounds = bounds.removeFromTop(kResponseStripHeight);
            layoutResponseStrip();
            responseDividerY = responseStripBounds.getBottom();
            bounds.removeFromTop(4);

            // Bottom chrome is reserved BEFORE the sliders so a short panel
            // starves the axes, not RESET/MORPH (and not the response strip).
            // The old status label used to share this row and could receive a
            // zero-height rectangle; it no longer lives here.
            auto bottomRow = bounds.removeFromBottom(28);
            resetButton.setBounds(bottomRow.removeFromLeft(60).reduced(2));
            morphButton.setBounds(bottomRow.removeFromLeft(60).reduced(2));
            bounds.removeFromBottom(6);

            auto presetRow = bounds.removeFromBottom(26);
            int presetW = presetRow.getWidth() / juce::jmax(1, (int) presetButtons.size());
            for (auto& btn : presetButtons)
                btn->setBounds(presetRow.removeFromLeft(presetW).reduced(2));
            bounds.removeFromBottom(6);

            auto intensityRow = bounds.removeFromBottom(24);
            intensityLabel.setBounds(intensityRow.removeFromLeft(60));
            intensitySlider.setBounds(intensityRow);
            bounds.removeFromBottom(6);

            const int sliderCount = juce::jmax(1, (int) qualitySliders.size());
            const int columnCount = bounds.getWidth() >= 520 && sliderCount > 4 ? 2 : 1;
            const int rowCount = (sliderCount + columnCount - 1) / columnCount;
            const int sliderHeight =
                juce::jlimit(18, 40, bounds.getHeight() / juce::jmax(1, rowCount));
            const int columnGap = columnCount == 2 ? 10 : 0;
            const int columnWidth =
                (bounds.getWidth() - columnGap * (columnCount - 1)) / columnCount;

            // UI-A.1: the gate is visual non-intersection of label vs track,
            // not row≥18. Horizontal geometry first — do not reclaim chrome
            // unless the track is too short at 1100×740.
            // Each half: [label gutter][slider track = remainder][value].
            constexpr int kAxisLabelW = 72;
            constexpr int kAxisValueW = 42;

            for (int index = 0; index < sliderCount; ++index)
            {
                auto& slider = qualitySliders[static_cast<size_t>(index)];
                const int column = index / rowCount;
                const int row = index % rowCount;
                auto cell = juce::Rectangle<int>(
                    bounds.getX() + column * (columnWidth + columnGap),
                    bounds.getY() + row * sliderHeight,
                    columnWidth,
                    sliderHeight);

                if (cell.getBottom() > bounds.getBottom())
                {
                    slider.slider->setVisible(false);
                    slider.label->setVisible(false);
                    continue;
                }
                slider.slider->setVisible(true);
                slider.label->setVisible(true);

                auto labelBounds = cell.removeFromLeft(kAxisLabelW);
                slider.label->setBounds(labelBounds);
                slider.bounds = cell;
                slider.slider->setBounds(cell);
                slider.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, kAxisValueW, 18);
            }
        }
    }
    
    void timerCallback() override
    {
        bool needsRepaint = false;

        consumePlanningResult();
        pollIntentMapHover();

        // Update morph progress
        if (semanticEngine.isMorphing())
        {
            semanticEngine.updateMorph(33.0f);  // ~30fps
            syncSlidersFromEngine();
            semanticDirty = true;
            needsRepaint = true; // Animation active
        }

        // FIX 1: Coalesce — apply semantic EQ updates at timer rate (30Hz max),
        // not on every slider drag event. This eliminates the parameter storm
        // while keeping the UI responsive.
        if (semanticDirty)
        {
            updateEQFromState();
            semanticDirty = false;
            needsRepaint = true; // State changed
        }

        // CRITICAL FIX: Only repaint if something actually changed (morphing or dirty)
        // This eliminates idle overhead (30Hz repaint even when doing nothing).
        if (needsRepaint)
            repaint();
    }
    
    //==========================================================================
    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override
    {
        invalidatePendingTextPlan();
        clearResponseStrip();
#if AIEQ_GUI_DEBUG
        debugSliderEventCount++;
        double now = juce::Time::getMillisecondCounterHiRes();
        if (now - debugLastSliderReport > 2000.0)
        {
            aieqDebugLog( "[SEMANTIC] sliderEvents/sec=%.1f updateEQ/sec=%.1f\n",
                debugSliderEventCount * 1000.0 / (now - debugLastSliderReport),
                debugUpdateEQCount * 1000.0 / (now - debugLastSliderReport));
            debugSliderEventCount = 0;
            debugUpdateEQCount = 0;
            debugLastSliderReport = now;
        }
#endif
        for (auto& qs : qualitySliders)
        {
            if (qs.slider.get() == slider)
            {
                float value = static_cast<float>(slider->getValue());

                if (morphButton.getToggleState() && std::abs(value - semanticEngine.getQuality(qs.quality)) > 0.1f)
                {
                    // Morph to new state
                    SemanticEQEngine::SemanticState target = semanticEngine.getSemanticState();
                    target.setQuality(qs.quality, value);
                    semanticEngine.morphToState(target, 500.0f);  // 500ms morph
                }
                else
                {
                    // Strict coalescing: during drag, update the semantic engine immediately
                    // for local UI state, but defer EQ application to the 30Hz timer.
                    // This avoids bursting APVTS/host writes at ~60Hz while audio is running.
                    semanticEngine.setQuality(qs.quality, value);
                    semanticDirty = true;
                }

                break;
            }
        }
    }
    
    //==========================================================================
    // TextEditor::Listener
    void textEditorReturnKeyPressed(juce::TextEditor&) override
    {
        applyTextCommand();
    }
    
    void textEditorTextChanged(juce::TextEditor&) override
    {
        if (pendingTextPlan.has_value() && commandInput.getText().trim() != pendingTextCommand)
        {
            invalidatePendingTextPlan();
            clearResponseStrip();
        }
    }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override {}
    void textEditorFocusLost(juce::TextEditor&) override {}

    /** A user safety fence changed after a PLAN snapshot. The reviewed or
        in-flight plan is no longer authoritative and must not remain APPLYable. */
    void userProtectedRangesChanged()
    {
        invalidatePendingTextPlan();
        clearResponseStrip();
    }

private:
    //==========================================================================
    // UI-A1 chips are labels for existing planner/apply outcomes, not new copy.
    // Ambiguous is ContradictoryIntent without a goal/protection fight — there
    // is no separate planner enum value.
    enum class ResponseChip
    {
        None,
        Planning,
        CantPlan,
        Conflict,
        Ambiguous,
        NoSafeMove,
        Ready,
        Applied
    };

    static constexpr int kResponseStripHeight = 28;

    struct QualitySliderData
    {
        SemanticEQEngine::SemanticQuality quality{};
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        juce::String name;
        juce::Colour color;
        juce::Rectangle<int> bounds;

        QualitySliderData() = default;
        QualitySliderData(QualitySliderData&&) noexcept = default;
        QualitySliderData& operator=(QualitySliderData&&) noexcept = default;
        QualitySliderData(const QualitySliderData&) = delete;
        QualitySliderData& operator=(const QualitySliderData&) = delete;
    };
    
    void setupQualitySlider(SemanticEQEngine::SemanticQuality quality,
                           const juce::String& name,
                           const juce::String& nameIT,
                           juce::Colour color,
                           const juce::String& tooltip)
    {
        juce::ignoreUnused(nameIT);
        QualitySliderData data;
        data.quality = quality;
        data.name = name;
        data.color = color;
        data.slider = std::make_unique<juce::Slider>();
        data.label = std::make_unique<juce::Label>();
        
        data.slider->setSliderStyle(juce::Slider::LinearHorizontal);
        data.slider->setRange(-1.0, 1.0, 0.01);
        data.slider->setValue(0.0);
        data.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 16);
        // Liquid Intelligence: lighter slider weight (3px track, 5px thumb) —
        // read by ModernLookAndFeel::drawLinearSlider via client properties.
        data.slider->getProperties().set("trackThickness", 3.0f);
        data.slider->getProperties().set("thumbRadius",    5.0f);
        data.slider->setColour(juce::Slider::thumbColourId, color);
        data.slider->setColour(juce::Slider::trackColourId, color.withAlpha(0.6f));  // desaturated track per quality
        data.slider->setColour(juce::Slider::backgroundColourId, color.withAlpha(0.15f));
        data.slider->setColour(juce::Slider::textBoxTextColourId, ModernLookAndFeel::Colors::textLabel);
        data.slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        data.slider->setTooltip(tooltip);
        data.slider->addListener(this);
        data.slider->setDoubleClickReturnValue(true, 0.0);
        data.slider->textFromValueFunction = [](double v) {
            return (v >= 0 ? "+" : "") + juce::String(static_cast<int>(v * 100)) + "%";
        };
        data.slider->valueFromTextFunction = [](const juce::String& text) {
            return text.getDoubleValue() / 100.0;
        };
        // P0-C: at 0 the control reads near-grey (neutral), not full accent
        data.slider->onValueChange = [slider = data.slider.get(), accent = color]()
        {
            const float a = static_cast<float>(std::abs(slider->getValue()));
            const float chroma = juce::jlimit(0.12f, 1.0f, 0.12f + a * 0.88f);
            const auto grey = juce::Colour(0xFF8A8A96);
            const auto live = accent.interpolatedWith(grey, 1.0f - chroma);
            slider->setColour(juce::Slider::thumbColourId, live);
            slider->setColour(juce::Slider::trackColourId, live.withAlpha(0.35f + 0.35f * a));
            slider->setColour(juce::Slider::backgroundColourId, live.withAlpha(0.10f + 0.10f * a));
        };
        data.slider->onValueChange(); // apply neutral look at default 0
        addAndMakeVisible(*data.slider);
        
        data.label->setText(name, juce::dontSendNotification);
        auto font = juce::Font(juce::FontOptions().withHeight(10.0f));
        font.setBold(true);
        data.label->setFont(font);
        data.label->setColour(juce::Label::textColourId, color);
        data.label->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*data.label);
        
        qualitySliders.push_back(std::move(data));
    }
    
    void setupPresetButtons()
    {
        struct PresetDef {
            juce::String name;
            std::vector<std::pair<SemanticEQEngine::SemanticQuality, float>> settings;
        };
        
        std::vector<PresetDef> presets = {
            { "VOCAL", {
                { SemanticEQEngine::SemanticQuality::Air, 0.4f },
                { SemanticEQEngine::SemanticQuality::Presence, 0.5f },
                { SemanticEQEngine::SemanticQuality::Clarity, 0.3f },
                { SemanticEQEngine::SemanticQuality::Warmth, 0.2f }
            }},
            { "DRUMS", {
                { SemanticEQEngine::SemanticQuality::Punch, 0.6f },
                { SemanticEQEngine::SemanticQuality::Weight, 0.4f },
                { SemanticEQEngine::SemanticQuality::Snap, 0.3f }
            }},
            { "BASS", {
                { SemanticEQEngine::SemanticQuality::Weight, 0.5f },
                { SemanticEQEngine::SemanticQuality::Body, 0.4f },
                { SemanticEQEngine::SemanticQuality::Punch, 0.3f },
                { SemanticEQEngine::SemanticQuality::Clarity, 0.2f }
            }},
            { "MASTER", {
                { SemanticEQEngine::SemanticQuality::Air, 0.2f },
                { SemanticEQEngine::SemanticQuality::Warmth, 0.15f },
                { SemanticEQEngine::SemanticQuality::Clarity, 0.2f },
                { SemanticEQEngine::SemanticQuality::Body, 0.1f }
            }}
        };
        
        for (const auto& preset : presets)
        {
            auto btn = std::make_unique<juce::TextButton>(preset.name);
            btn->setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
            btn->setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textLabel);
            
            auto settings = preset.settings;  // Copy for lambda
            btn->onClick = [this, settings]() {
                applyPreset(settings);
            };
            
            addAndMakeVisible(*btn);
            presetButtons.push_back(std::move(btn));
        }
    }
    
    void applyPreset(const std::vector<std::pair<SemanticEQEngine::SemanticQuality, float>>& settings)
    {
        invalidatePendingTextPlan();
        SemanticEQEngine::SemanticState target;
        target.reset();
        
        for (const auto& [quality, value] : settings)
        {
            target.setQuality(quality, value);
        }
        
        if (morphButton.getToggleState())
        {
            semanticEngine.morphToState(target, 800.0f);
        }
        else
        {
            semanticEngine.setSemanticState(target);
            syncSlidersFromEngine();
            updateEQFromState();
        }
        
        clearResponseStrip();
    }
    
    void applyTextCommand()
    {
        const juce::String text = commandInput.getText().trim();
        if (text.isEmpty())
            return;

        // Second activation applies the exact plan that the user just reviewed.
        // We do not silently recompile between PLAN and APPLY. Editing the text
        // invalidates pendingTextPlan through textEditorTextChanged().
        if (pendingTextPlan.has_value() && pendingTextCommand == text)
        {
            const auto adjustments = semanticEngine.adjustmentsFromPlan(*pendingTextPlan);
            if (adjustments.empty())
            {
                setResponseStrip(ResponseChip::NoSafeMove, "No safe EQ move to apply");
                publishIntentMap(EmberUI::buildSemanticIntentMapState(
                    *pendingTextPlan, EmberUI::SemanticIntentMapPhase::NoSafeMove));
                return;
            }

            const juce::String interpretation = juce::String::fromUTF8(
                pendingTextPlan->interpretation.c_str());

            if (onTextPlanApply)
            {
                const auto feedback = onTextPlanApply(adjustments);
                if (feedback.atomicRejected)
                {
                    setResponseStrip(ResponseChip::CantPlan,
                        "Can't apply safely: "
                        + juce::String(feedback.rejectedBands)
                        + " plan band(s) have no free EQ slot");
                    return; // keep the reviewed plan pending
                }
                if (feedback.deferred)
                {
                    setResponseStrip(ResponseChip::CantPlan,
                                    "Apply deferred — retry from the UI thread");
                    return;
                }
                if (!feedback.complete())
                {
                    setResponseStrip(ResponseChip::CantPlan,
                        "Plan not fully applied ("
                        + juce::String(feedback.appliedBands) + "/"
                        + juce::String(feedback.requestedBands) + ")");
                    return;
                }

                auto map = EmberUI::buildSemanticIntentMapState(
                    *pendingTextPlan, EmberUI::SemanticIntentMapPhase::Applied);
                map.appliedBandSlots = feedback.appliedBandSlots;
                const auto appliedDetail = "Applied: " + interpretation
                    + intentMapSummarySuffix(map);
                publishIntentMap(std::move(map));
                if (isPendingExternal() && onExternalApplied)
                {
                    const auto hash = EmberProposal::makeAudit(
                        pendingTextPlan->interpretation).sha256Hex;
                    onExternalApplied(hash);
                }
                invalidatePendingTextPlan(false);
                pendingProposalSource.clear();
                commandInput.clear();
                setResponseStrip(ResponseChip::Applied, appliedDetail);
                return;
            }
            else
            {
                // Typed text plans are reliability-first and therefore never fall
                // back to the legacy best-effort slider callback. Without an
                // authoritative atomic apply endpoint, keep the reviewed plan.
                setResponseStrip(ResponseChip::CantPlan,
                                "Apply unavailable — atomic Semantic endpoint not connected");
                return;
            }
        }

        submitPlanForText(text);
    }

    void submitPlanForText(const juce::String& text)
    {
        if (text.isEmpty())
            return;

        // PLAN. The fit costs 32-42 ms on this machine (measured 44.1/48/96 kHz),
        // so it runs on a worker: doing it here dropped 2-3 GUI frames per press.
        // Everything the worker needs is captured now, by value.
        if (planningUiState == AIEQPerceptual::SemanticPlanningUiState::Planning
            && planningText == text)
            return; // already planning exactly this

        // Snapshot now, not in the worker: planning must reason about the
        // source as it was when the user asked, and the worker must never read
        // analysis state that keeps moving while it computes.
        AIEQPerceptual::SpectralContext contextSnapshot;
        if (onRequestSpectralContext)
            if (auto snapshot = onRequestSpectralContext())
                contextSnapshot = *snapshot;

        std::vector<AIEQPerceptual::SemanticProtectedRange> protectedSnapshot;
        if (onRequestProtectedRanges)
            protectedSnapshot = onRequestProtectedRanges();

        pendingGeneration = planningService.submit(text.toStdString(),
                                                   semanticEngine.getIntensity(),
                                                   currentSampleRate,
                                                   contextSnapshot,
                                                   std::move(protectedSnapshot));
        planningText = text;
        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Planning;
        applyButton.setButtonText("PLAN");
        {
            EmberUI::SemanticIntentMapState planning;
            planning.phase = EmberUI::SemanticIntentMapPhase::Planning;
            publishIntentMap(std::move(planning));
        }
        setResponseStrip(ResponseChip::Planning, "Planning...");
    }

    /** Message thread. Consumes a current-generation planning result, if one is
        ready. The status wording is deliberately identical to what the previous
        synchronous path produced, so moving the work off-thread cannot silently
        change what the user is told. */
    void consumePlanningResult()
    {
        if (planningUiState != AIEQPerceptual::SemanticPlanningUiState::Planning)
            return;

        auto result = planningService.takeCurrentResult();
        if (!result.has_value())
            return;

        // takeCurrentResult() already dropped stale generations; this is a second
        // guard for the panel's own epoch and costs nothing.
        if (result->generation != pendingGeneration)
            return;

        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Idle;

        using Status = AIEQPerceptual::SemanticPlanningStatus;
        const auto& plan = result->plan;

        switch (result->status)
        {
            case Status::UnknownIntent:
                setResponseStrip(ResponseChip::CantPlan, "Couldn't understand command");
                notifyExternalPlannerOutcome(EmberProposal::ReasonCode::unknown_intent, plan);
                suppressExternalInvalidate = true;
                invalidatePendingTextPlan();
                suppressExternalInvalidate = false;
                // P0-C: do not leave a zombie prompt for 90s after a failed parse
                commandInput.clear();
                return;

            case Status::ContradictoryIntent:
                notifyExternalPlannerOutcome(EmberProposal::ReasonCode::contradictory_intent, plan);
                setResponseStrip(plan.intent.goalConstraintConflict
                                     ? ResponseChip::Conflict
                                     : ResponseChip::Ambiguous,
                                 plan.intent.goalConstraintConflict
                                     ? juce::String("Contradictory request - goal conflicts with requested protection")
                                     : juce::String("Ambiguous command - clarify the direction"));
                suppressExternalInvalidate = true;
                invalidatePendingTextPlan();
                suppressExternalInvalidate = false;
                commandInput.clear();
                return;

            case Status::InternalError:
                notifyExternalPlannerOutcome(EmberProposal::ReasonCode::internal_error, plan);
                setResponseStrip(ResponseChip::CantPlan, "Couldn't build a safe semantic plan");
                suppressExternalInvalidate = true;
                invalidatePendingTextPlan();
                suppressExternalInvalidate = false;
                return;

            case Status::NoSafeMove:
                notifyExternalPlannerOutcome(EmberProposal::ReasonCode::no_safe_move, plan);
                if (!plan.outcomeSummary.empty())
                    setResponseStrip(ResponseChip::NoSafeMove,
                        "No safe move - "
                        + juce::String::fromUTF8(plan.outcomeSummary.c_str()));
                else
                    setResponseStrip(ResponseChip::NoSafeMove, "No meaningful EQ move required");
                suppressExternalInvalidate = true;
                invalidatePendingTextPlan();
                suppressExternalInvalidate = false;
                publishIntentMap(EmberUI::buildSemanticIntentMapState(
                    plan, EmberUI::SemanticIntentMapPhase::NoSafeMove));
                return;

            case Status::Cancelled:
                return; // superseded; a newer plan is already on its way

            case Status::Ready:
                break;
        }

        pendingTextCommand = planningText;
        pendingTextPlan = plan;
        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Ready;
        applyButton.setButtonText("APPLY");

        const juce::String interpretation = juce::String::fromUTF8(
            plan.interpretation.c_str());
        juce::String planStatus = "Plan: " + interpretation + " | "
            + juce::String(static_cast<int>(plan.fit.bands.size())) + " band(s)";
        planStatus += describeSourceContext(plan);
        if (!plan.outcomeSummary.empty())
            planStatus += " | " + juce::String::fromUTF8(plan.outcomeSummary.c_str());
        auto map = EmberUI::buildSemanticIntentMapState(
            plan, EmberUI::SemanticIntentMapPhase::Ready);
        planStatus += intentMapSummarySuffix(map);
        publishIntentMap(std::move(map));
        setResponseStrip(ResponseChip::Ready, planStatus);
        notifyExternalPlannerOutcome(EmberProposal::ReasonCode::plan_staged, plan);
    }

    void notifyExternalPlannerOutcome(EmberProposal::ReasonCode reason,
                                      const AIEQPerceptual::SemanticPlan& plan)
    {
        if (!isPendingExternal() || !onExternalPlanResult)
            return;
        std::string summary = plan.interpretation.empty() ? "Staged in Semantic"
                                                          : plan.interpretation;
        if (summary.size() > 128)
            summary.resize(128);
        const auto hash = EmberProposal::makeAudit(plan.interpretation).sha256Hex;
        onExternalPlanResult(reason, summary, hash);
    }


    /** What the SOURCE did to the request, in the few characters the status row
        has room for.

        Without this the panel shows only what was planned, so a request that
        came back at 44% of what the user typed looked identical to one that
        came back whole - the plug-in appeared to under-deliver for no visible
        reason. The plan already carries all of it; it simply was not surfaced.

        Confidence is printed only when it is NOT high, because a confidence of
        1.00 explains nothing: it is the low values that are the reason for an
        unexpected result. */
    [[nodiscard]] juce::String describeSourceContext(
        const AIEQPerceptual::SemanticPlan& plan) const
    {
        if (plan.intent.goals.empty())
            return {};

        const auto dimension = plan.intent.goals.front().dimension;
        const AIEQPerceptual::SemanticContextAdjustment* adjustment = nullptr;
        for (const auto& a : plan.contextAdjustments)
            if (a.dimension == dimension) { adjustment = &a; break; }

        juce::String out = " | src ";

        if (adjustment == nullptr)
            return out + "n/a";

        if (adjustment->contextualized)
        {
            // Held back: the source already leans this way. Report BOTH the
            // amount kept and how strongly the source reads that way, because
            // "held to 44%" without the reading is a number with no cause.
            out += juce::String(juce::roundToInt(100.0f * adjustment->scale)) + "%";
            out += " (has " + juce::String(adjustment->axisPosition, 2) + ")";
        }
        else if (std::abs(adjustment->axisPosition) < 1.0e-6f)
        {
            // Axis exactly neutral means no usable evidence in the region this
            // goal depends on - a bass asked for air - rather than a source
            // that happens to sit at zero.
            out += "full, no evidence";
        }
        else
        {
            out += "full (has " + juce::String(adjustment->axisPosition, 2) + ")";
        }

        if (plan.contextConfidence < 0.95f)
            out += " conf " + juce::String(plan.contextConfidence, 2);

        return out;
    }

    static EmberUI::SemanticUiAxis uiAxisFromQuality (
        SemanticEQEngine::SemanticQuality quality) noexcept
    {
        using Q = SemanticEQEngine::SemanticQuality;
        using A = EmberUI::SemanticUiAxis;
        switch (quality)
        {
            case Q::Air:        return A::Air;
            case Q::Warmth:     return A::Warmth;
            case Q::Punch:      return A::Punch;
            case Q::Clarity:    return A::Clarity;
            case Q::Body:       return A::Body;
            case Q::Brilliance: return A::Brilliance;
            case Q::Smoothness: return A::Smooth;
            case Q::Weight:     return A::Weight;
            default:            return A::Count;
        }
    }

    juce::String intentMapSummarySuffix (const EmberUI::SemanticIntentMapState& map) const
    {
        juce::String out;
        if (!map.focusRegions.empty())
        {
            const auto& focus = map.focusRegions.front();
            out += " | Focus: "
                + juce::String (EmberUI::semanticFocusDisplayName (focus.dimension, focus.focus));
        }
        if (!map.protectRegions.empty() && !map.protectRegions.front().sourcePhrase.empty())
            out += " | Protect: "
                + juce::String::fromUTF8 (map.protectRegions.front().sourcePhrase.c_str());
        return out;
    }

    void publishIntentMap (EmberUI::SemanticIntentMapState state)
    {
        intentMapState = std::move (state);
        refreshAxisPresentation();
        if (onIntentMapChanged)
            onIntentMapChanged (intentMapState);
    }

    void hideIntentMap()
    {
        if (intentMapState.phase == EmberUI::SemanticIntentMapPhase::Hidden
            && !intentMapState.hasFocus()
            && !intentMapState.hasProtection())
            return;
        publishIntentMap ({});
    }

    void refreshAxisPresentation()
    {
        const bool mapVisible = intentMapState.visible();
        for (auto& qs : qualitySliders)
        {
            const auto axis = uiAxisFromQuality (qs.quality);
            auto colour = qs.color;
            if (mapVisible && axis != EmberUI::SemanticUiAxis::Count)
            {
                const auto role = intentMapState.axisRoles[static_cast<std::size_t> (axis)];
                if (role == EmberUI::SemanticAxisRole::Primary)
                    colour = qs.color.brighter (0.18f);
                else if (role == EmberUI::SemanticAxisRole::Involved)
                    colour = qs.color;
                else
                    colour = ModernLookAndFeel::Colors::textMuted;
            }
            qs.label->setColour (juce::Label::textColourId, colour);
        }
    }

    void pollIntentMapHover()
    {
        bool hover = false;
        if (intentMapState.visible())
        {
            const auto pos = getMouseXYRelative();
            if (responseStripBounds.contains (pos))
                hover = true;
            for (const auto& qs : qualitySliders)
            {
                const auto axis = uiAxisFromQuality (qs.quality);
                if (axis == EmberUI::SemanticUiAxis::Count)
                    continue;
                if (intentMapState.axisRoles[static_cast<std::size_t> (axis)]
                    == EmberUI::SemanticAxisRole::Neutral)
                    continue;
                if ((qs.label != nullptr && qs.label->getBounds().contains (pos))
                    || (qs.slider != nullptr && qs.slider->getBounds().contains (pos)))
                    hover = true;
            }
        }
        if (hover == intentMapHover)
            return;
        intentMapHover = hover;
        if (onIntentMapHover)
            onIntentMapHover (hover);
    }

    void invalidatePendingTextPlan(bool clearIntentMap = true)
    {
        const bool wasExternal = isPendingExternal();
        const bool wasStaged = pendingTextPlan.has_value();
        pendingTextPlan.reset();
        pendingTextCommand.clear();
        // Bump the epoch, do not merely clear the slot: an in-flight fit is only
        // made unpublishable by a newer generation.
        planningService.invalidate();
        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Idle;
        pendingGeneration = 0;
        planningText.clear();
        applyButton.setButtonText("PLAN");
        if (clearIntentMap)
            hideIntentMap();
        if (wasExternal && !suppressExternalInvalidate)
        {
            pendingProposalSource.clear();
            if (onExternalProposalInvalidated)
                onExternalProposalInvalidated(wasStaged);
        }
        else if (!wasExternal)
            pendingProposalSource.clear();
    }
    
    void resetAllSliders()
    {
        invalidatePendingTextPlan();
        semanticEngine.resetState();
        
        for (auto& qs : qualitySliders)
        {
            qs.slider->setValue(0.0, juce::dontSendNotification);
        }
        
        updateEQFromState();
        clearResponseStrip();
    }
    
    void syncSlidersFromEngine()
    {
        for (auto& qs : qualitySliders)
        {
            float value = semanticEngine.getQuality(qs.quality);
            qs.slider->setValue(value, juce::dontSendNotification);
        }
    }
    
    void updateEQFromState()
    {
        // SAFETY: Only update if panel is visible and ready
        if (!isVisible())
            return;

#if AIEQ_GUI_DEBUG
        debugUpdateEQCount++;
#endif
        // Generate EQ adjustments from current semantic state, feeding the
        // live analyzer spectrum so adjustForContext() sees real content
        // (bass/brightness proportions) instead of the neutral 0.5 fallback.
        auto adjustments = semanticEngine.generateEQFromState(
            spectrumProvider ? spectrumProvider() : std::vector<float>{},
            currentSampleRate);
        
        if (onEQGenerated)
            onEQGenerated(adjustments);
        
        if (onStateChanged)
            onStateChanged(semanticEngine.getSemanticState());
    }
    
    void setResponseStrip(ResponseChip chip, juce::String detail)
    {
        currentResponseChip = chip;
        responseDetailFull = std::move(detail);
        layoutResponseStrip();
    }

    void clearResponseStrip()
    {
        setResponseStrip(ResponseChip::None, {});
    }

    [[nodiscard]] static juce::String chipTextFor(ResponseChip chip)
    {
        switch (chip)
        {
            case ResponseChip::None:       return {};
            case ResponseChip::Planning:   return "PLANNING";
            case ResponseChip::CantPlan:   return "CAN'T PLAN";
            case ResponseChip::Conflict:   return "CONFLICT";
            case ResponseChip::Ambiguous:  return "AMBIGUOUS";
            case ResponseChip::NoSafeMove: return "NO SAFE MOVE";
            case ResponseChip::Ready:      return "READY";
            case ResponseChip::Applied:    return "APPLIED";
        }
        return {};
    }

    [[nodiscard]] static juce::Colour chipColourFor(ResponseChip chip)
    {
        using C = ModernLookAndFeel::Colors;
        switch (chip)
        {
            case ResponseChip::Ready:
            case ResponseChip::Applied:
                return C::accentYellow;
            case ResponseChip::Planning:
            case ResponseChip::None:
                return C::textSecondary;
            case ResponseChip::CantPlan:
            case ResponseChip::Conflict:
            case ResponseChip::Ambiguous:
            case ResponseChip::NoSafeMove:
                return C::accentRed;
        }
        return C::textSecondary;
    }

    [[nodiscard]] static juce::String elideToWidth(const juce::String& text,
                                                   const juce::Font& font,
                                                   int width)
    {
        if (width <= 0)
            return {};
        if (font.getStringWidth(text) <= width)
            return text;

        const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));
        int lo = 0, hi = text.length();
        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            if (font.getStringWidth(text.substring(0, mid) + ellipsis) <= width)
                lo = mid;
            else
                hi = mid - 1;
        }
        return lo <= 0 ? ellipsis : text.substring(0, lo) + ellipsis;
    }

    void layoutResponseStrip()
    {
        if (responseStripBounds.isEmpty() || currentResponseChip == ResponseChip::None)
        {
            responseChip.setVisible(false);
            responseDetail.setVisible(false);
            responseChip.setTooltip({});
            responseDetail.setTooltip({});
            return;
        }

        auto strip = responseStripBounds;
        const auto chipText = chipTextFor(currentResponseChip);
        const int chipW = juce::jlimit(0, strip.getWidth(),
                                       responseChip.getFont().getStringWidth(chipText) + 10);

        responseChip.setText(chipText, juce::dontSendNotification);
        responseChip.setColour(juce::Label::textColourId, chipColourFor(currentResponseChip));
        responseChip.setBounds(strip.removeFromLeft(chipW));
        responseChip.setVisible(true);

        if (strip.getWidth() > 8)
            strip.removeFromLeft(8);

        const auto shown = elideToWidth(responseDetailFull, responseDetail.getFont(),
                                        strip.getWidth());
        responseDetail.setText(shown, juce::dontSendNotification);
        responseDetail.setTooltip(shown != responseDetailFull ? responseDetailFull
                                                              : juce::String{});
        responseDetail.setBounds(strip);
        responseDetail.setVisible(true);
    }

    void drawQualityVisualizer(juce::Graphics& g)
    {
        // Draw a circular visualizer showing active qualities
        int vizSize = 80;
        int vizX = getWidth() - vizSize - 15;
        int vizY = 45;
        
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.fillEllipse(static_cast<float>(vizX), static_cast<float>(vizY),
                     static_cast<float>(vizSize), static_cast<float>(vizSize));

        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawEllipse(static_cast<float>(vizX), static_cast<float>(vizY),
                     static_cast<float>(vizSize), static_cast<float>(vizSize), 1.0f);
        
        // Draw quality arcs
        float centerX = vizX + vizSize / 2.0f;
        float centerY = vizY + vizSize / 2.0f;
        float radius = vizSize / 2.0f - 5.0f;
        
        int qualityIndex = 0;
        for (const auto& qs : qualitySliders)
        {
            float value = std::abs(static_cast<float>(qs.slider->getValue()));
            if (value > 0.05f)
            {
                float startAngle = qualityIndex * (juce::MathConstants<float>::twoPi / qualitySliders.size()) - 
                                  juce::MathConstants<float>::halfPi;
                float endAngle = startAngle + (juce::MathConstants<float>::twoPi / qualitySliders.size()) * 0.8f;
                
                juce::Path arc;
                arc.addCentredArc(centerX, centerY, radius * value, radius * value,
                                 0.0f, startAngle, endAngle, true);
                
                g.setColour(qs.color.withAlpha(0.8f));
                g.strokePath(arc, juce::PathStrokeType(3.0f + value * 4.0f));
            }
            qualityIndex++;
        }
        
        // Center dot
        g.setColour(ModernLookAndFeel::Colors::accentYellow);
        g.fillEllipse(centerX - 3, centerY - 3, 6, 6);
    }
    
    //==========================================================================
    SemanticEQEngine& semanticEngine;
    double currentSampleRate = 44100.0;
    
    juce::Label titleLabel, subtitleLabel, intensityLabel;
    juce::Label responseChip, responseDetail;
    ResponseChip currentResponseChip = ResponseChip::None;
    juce::String responseDetailFull;
    juce::Rectangle<int> responseStripBounds;
    int responseDividerY = 0;
    juce::TextEditor commandInput;
    juce::TextButton applyButton, resetButton, morphButton;
    juce::TextButton emberLinkToggle, emberPairButton;
    juce::Label emberLinkStatus;
    juce::Slider intensitySlider;
    juce::String pendingProposalSource;
    bool suppressExternalInvalidate = false;
    
    std::vector<QualitySliderData> qualitySliders;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    std::optional<AIEQPerceptual::SemanticPlan> pendingTextPlan;
    juce::String pendingTextCommand;
    EmberUI::SemanticIntentMapState intentMapState;
    bool intentMapHover = false;

    // T3.2 async planning. The panel owns the UX state; the worker owns only
    // the computation. pendingGeneration is the epoch this panel is waiting for.
    AIEQPerceptual::SemanticPlanningService planningService;
    AIEQPerceptual::SemanticPlanningUiState planningUiState
        = AIEQPerceptual::SemanticPlanningUiState::Idle;
    std::uint64_t pendingGeneration = 0;
    juce::String planningText;
    bool semanticDirty = false;            // Coalesce semantic updates to timer rate

#if AIEQ_GUI_DEBUG
    int debugSliderEventCount = 0;
    int debugUpdateEQCount = 0;
    double debugLastSliderReport = 0.0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SemanticControlPanel)
};
