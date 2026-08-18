#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <optional>
#include "../AI/SemanticEQEngine.h"
#include "../AI/SemanticPlanningService.h"
#include "ModernLookAndFeel.h"
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

    /** T5.3 - the source context to plan against, taken on the message thread
        at the instant PLAN is pressed and then frozen into the request. Left
        unset the planner degrades to text-only, which is exactly what an
        invalid context already means downstream. */
    std::function<std::optional<AIEQPerceptual::SpectralContext>()> onRequestSpectralContext;

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
        addAndMakeVisible(commandInput);
        
        // Apply button for text input
        applyButton.setButtonText("PLAN");
        applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
        applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        applyButton.onClick = [this]() { applyTextCommand(); };
        addAndMakeVisible(applyButton);
        
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
        
        // Status label
        statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        statusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
        statusLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusLabel);
        
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

        // Section dividers
        int y = 85;  // After title and input
        g.setColour(ModernLookAndFeel::Colors::bgLight);
        g.drawHorizontalLine(y, 10.0f, static_cast<float>(getWidth() - 10));
        
        // Draw center notch marks on quality sliders
        for (const auto& qs : qualitySliders)
        {
            if (qs.slider && qs.slider->isVisible())
            {
                auto sb = qs.slider->getBounds().toFloat();
                // The slider track center (value 0 is at center of range -1..1)
                float centerX = sb.getX() + sb.getWidth() * 0.5f;
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

        if (compact)
        {
            // ── COMPACT MODE (bottom panel) ──
            bounds.reduce(6, 4);

            // Hide non-essential elements
            titleLabel.setVisible(false);
            subtitleLabel.setVisible(false);
            intensitySlider.setVisible(false);
            intensityLabel.setVisible(false);
            resetButton.setVisible(false);
            morphButton.setVisible(false);
            statusLabel.setVisible(false);
            for (auto& btn : presetButtons) btn->setVisible(false);

            // Liquid Intelligence: input row moved to bottom (24px tall)
            auto inputArea = bounds.removeFromBottom(24);
            bounds.removeFromBottom(3);  // spacer above input row

            // Quality sliders — compact rows (16px each), occupying remaining bounds
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

            // Position input row children inside the bottom area
            applyButton.setBounds(inputArea.removeFromRight(52).reduced(1));
            commandInput.setBounds(inputArea.reduced(0, 1));
        }
        else
        {
            // ── FULL MODE (tall panel) ──
            bounds.reduce(12, 0);
            titleLabel.setVisible(true);
            subtitleLabel.setVisible(true);
            intensitySlider.setVisible(true);
            intensityLabel.setVisible(true);
            resetButton.setVisible(true);
            morphButton.setVisible(true);
            statusLabel.setVisible(true);
            for (auto& btn : presetButtons) btn->setVisible(true);

            // Title area
            titleLabel.setBounds(bounds.removeFromTop(20));
            subtitleLabel.setBounds(bounds.removeFromTop(16));
            bounds.removeFromTop(8);

            // Command input area
            auto inputRow = bounds.removeFromTop(28);
            applyButton.setBounds(inputRow.removeFromRight(60).reduced(2));
            commandInput.setBounds(inputRow.reduced(0, 2));
            bounds.removeFromTop(10);

            // Quality sliders
            int sliderHeight = 40;
            for (auto& slider : qualitySliders)
            {
                slider.bounds = bounds.removeFromTop(sliderHeight);
                slider.slider->setBounds(slider.bounds.reduced(4));
                slider.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
                slider.slider->setVisible(true);
                auto labelBounds = slider.bounds.removeFromLeft(70);
                slider.label->setBounds(labelBounds);
                slider.label->setVisible(true);
            }

            bounds.removeFromTop(8);

            // Intensity slider
            auto intensityRow = bounds.removeFromTop(24);
            intensityLabel.setBounds(intensityRow.removeFromLeft(60));
            intensitySlider.setBounds(intensityRow);
            bounds.removeFromTop(8);

            // Preset buttons
            auto presetRow = bounds.removeFromTop(26);
            int presetW = (presetRow.getWidth() - 8) / 4;
            for (auto& btn : presetButtons)
            {
                btn->setBounds(presetRow.removeFromLeft(presetW).reduced(2));
            }
            bounds.removeFromTop(8);

            // Bottom buttons
            auto bottomRow = bounds.removeFromTop(28);
            resetButton.setBounds(bottomRow.removeFromLeft(60).reduced(2));
            morphButton.setBounds(bottomRow.removeFromLeft(60).reduced(2));
            statusLabel.setBounds(bottomRow);
        }
    }
    
    void timerCallback() override
    {
        bool needsRepaint = false;

        consumePlanningResult();

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

                updateStatusLabel(qs.name, value);
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
            invalidatePendingTextPlan();
    }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override {}
    void textEditorFocusLost(juce::TextEditor&) override {}

private:
    //==========================================================================
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
        
        statusLabel.setText("Preset applied", juce::dontSendNotification);
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
                statusLabel.setText("No safe EQ move to apply", juce::dontSendNotification);
                return;
            }

            const juce::String interpretation = juce::String::fromUTF8(
                pendingTextPlan->interpretation.c_str());

            if (onTextPlanApply)
            {
                const auto feedback = onTextPlanApply(adjustments);
                if (feedback.atomicRejected)
                {
                    statusLabel.setText(
                        "Can't apply safely: "
                        + juce::String(feedback.rejectedBands)
                        + " plan band(s) have no free EQ slot",
                        juce::dontSendNotification);
                    return; // keep the reviewed plan pending
                }
                if (feedback.deferred)
                {
                    statusLabel.setText("Apply deferred — retry from the UI thread",
                                        juce::dontSendNotification);
                    return;
                }
                if (!feedback.complete())
                {
                    statusLabel.setText(
                        "Plan not fully applied ("
                        + juce::String(feedback.appliedBands) + "/"
                        + juce::String(feedback.requestedBands) + ")",
                        juce::dontSendNotification);
                    return;
                }
            }
            else
            {
                // Typed text plans are reliability-first and therefore never fall
                // back to the legacy best-effort slider callback. Without an
                // authoritative atomic apply endpoint, keep the reviewed plan.
                statusLabel.setText("Apply unavailable — atomic Semantic endpoint not connected",
                                    juce::dontSendNotification);
                return;
            }

            invalidatePendingTextPlan();
            commandInput.clear();
            statusLabel.setText("Applied: " + interpretation, juce::dontSendNotification);
            return;
        }

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

        pendingGeneration = planningService.submit(text.toStdString(),
                                                   semanticEngine.getIntensity(),
                                                   currentSampleRate,
                                                   contextSnapshot);
        planningText = text;
        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Planning;
        applyButton.setButtonText("PLAN");
        statusLabel.setText("Planning...", juce::dontSendNotification);
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
                statusLabel.setText("Couldn't understand command", juce::dontSendNotification);
                invalidatePendingTextPlan();
                return;

            case Status::ContradictoryIntent:
                statusLabel.setText(
                    plan.intent.goalConstraintConflict
                        ? "Contradictory request - goal conflicts with requested protection"
                        : "Ambiguous command - clarify the direction",
                    juce::dontSendNotification);
                invalidatePendingTextPlan();
                return;

            case Status::InternalError:
                statusLabel.setText("Couldn't build a safe semantic plan",
                                    juce::dontSendNotification);
                invalidatePendingTextPlan();
                return;

            case Status::NoSafeMove:
                if (!plan.outcomeSummary.empty())
                    statusLabel.setText(
                        "No safe move - "
                        + juce::String::fromUTF8(plan.outcomeSummary.c_str()),
                        juce::dontSendNotification);
                else
                    statusLabel.setText("No meaningful EQ move required",
                                        juce::dontSendNotification);
                invalidatePendingTextPlan();
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
        if (!plan.outcomeSummary.empty())
            planStatus += " | " + juce::String::fromUTF8(plan.outcomeSummary.c_str());
        statusLabel.setText(planStatus, juce::dontSendNotification);
    }

    void invalidatePendingTextPlan()
    {
        pendingTextPlan.reset();
        pendingTextCommand.clear();
        // Bump the epoch, do not merely clear the slot: an in-flight fit is only
        // made unpublishable by a newer generation.
        planningService.invalidate();
        planningUiState = AIEQPerceptual::SemanticPlanningUiState::Idle;
        pendingGeneration = 0;
        planningText.clear();
        applyButton.setButtonText("PLAN");
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
        statusLabel.setText("Reset to neutral", juce::dontSendNotification);
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
    
    void updateStatusLabel(const juce::String& name, float value)
    {
        juce::String direction = value > 0 ? "+" : "";
        statusLabel.setText(name + ": " + direction + juce::String(value * 100, 0) + "%",
                           juce::dontSendNotification);
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
    
    juce::Label titleLabel, subtitleLabel, intensityLabel, statusLabel;
    juce::TextEditor commandInput;
    juce::TextButton applyButton, resetButton, morphButton;
    juce::Slider intensitySlider;
    
    std::vector<QualitySliderData> qualitySliders;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    std::optional<AIEQPerceptual::SemanticPlan> pendingTextPlan;
    juce::String pendingTextCommand;

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

