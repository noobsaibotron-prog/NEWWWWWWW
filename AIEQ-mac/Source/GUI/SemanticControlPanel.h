#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "../AI/SemanticEQEngine.h"
#include "ModernLookAndFeel.h"

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
    
    // Callback to get generated EQ adjustments
    std::function<void(const std::vector<SemanticEQEngine::SemanticEQAdjustment>&)> onEQGenerated;

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
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE6A23C));
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Subtitle
        subtitleLabel.setText("Shape your sound with words", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(10.0f));
            font.setItalic(true);
            subtitleLabel.setFont(font);
        }
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        subtitleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(subtitleLabel);
        
        // Natural language input
        commandInput.setMultiLine(false);
        commandInput.setReturnKeyStartsNewLine(false);
        commandInput.setTextToShowWhenEmpty("Type: \"more air\", \"warmer\", \"add punch\"...", 
                                            juce::Colour(0xFF555555));
        commandInput.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
        commandInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A1A));
        commandInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        commandInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF333333));
        commandInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFFE6A23C));
        commandInput.addListener(this);
        addAndMakeVisible(commandInput);
        
        // Apply button for text input
        applyButton.setButtonText("APPLY");
        applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
        applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        applyButton.onClick = [this]() { applyTextCommand(); };
        addAndMakeVisible(applyButton);
        
        // Initialize quality sliders - Main qualities
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Air, "AIR", "Aria", 
                          juce::Colour(0xFF4A9FD9), "High frequency sparkle and openness");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Warmth, "WARMTH", "Calore",
                          juce::Colour(0xFFE67E22), "Low-mid fullness, analog feel");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Punch, "PUNCH", "Punch",
                          juce::Colour(0xFFE74C3C), "Attack and percussive impact");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Clarity, "CLARITY", "Chiarezza",
                          juce::Colour(0xFF2ECC71), "Definition, reduced muddiness");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Body, "BODY", "Corpo",
                          juce::Colour(0xFF9B59B6), "Weight and substance");
        setupQualitySlider(SemanticEQEngine::SemanticQuality::Brilliance, "BRILLIANCE", "Brillantezza",
                          juce::Colour(0xFF00BCD4), "Crystal clear highs");
        
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
        intensitySlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE6A23C));
        intensitySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF333333));
        intensitySlider.onValueChange = [this]() {
            semanticEngine.setIntensity(static_cast<float>(intensitySlider.getValue()));
            updateEQFromState();
        };
        addAndMakeVisible(intensitySlider);
        
        intensityLabel.setText("INTENSITY", juce::dontSendNotification);
        {
            auto font = juce::Font(juce::FontOptions().withHeight(9.0f));
            font.setBold(true);
            intensityLabel.setFont(font);
        }
        intensityLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        addAndMakeVisible(intensityLabel);
        
        // Preset buttons
        setupPresetButtons();
        
        // Reset button
        resetButton.setButtonText("RESET");
        resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        resetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
        resetButton.onClick = [this]() { resetAllSliders(); };
        addAndMakeVisible(resetButton);
        
        // Morph button
        morphButton.setButtonText("MORPH");
        morphButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        morphButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE6A23C));
        morphButton.setClickingTogglesState(true);
        morphButton.setTooltip("Enable smooth transitions between states");
        addAndMakeVisible(morphButton);
        
        // Status label
        statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF666666));
        statusLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusLabel);
        
        startTimerHz(30);
    }
    
    ~SemanticControlPanel() override
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Background
        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        
        // Top accent
        g.setColour(juce::Colour(0xFFE6A23C));
        g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), 3.0f);
        
        // Border
        g.setColour(juce::Colour(0xFF333333));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
        
        // Section dividers
        int y = 85;  // After title and input
        g.setColour(juce::Colour(0xFF2A2A2A));
        g.drawHorizontalLine(y, 10.0f, static_cast<float>(getWidth() - 10));
        
        // Draw active quality visualizer
        drawQualityVisualizer(g);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);
        
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
            
            // Position label to the left
            auto labelBounds = slider.bounds.removeFromLeft(70);
            slider.label->setBounds(labelBounds);
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
    
    void timerCallback() override
    {
        // Update morph progress
        if (semanticEngine.isMorphing())
        {
            semanticEngine.updateMorph(33.0f);  // ~30fps
            syncSlidersFromEngine();
            updateEQFromState();
        }
        
        repaint();  // For visualizer animation
    }
    
    //==========================================================================
    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override
    {
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
                    semanticEngine.setQuality(qs.quality, value);
                    updateEQFromState();
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
    
    void textEditorTextChanged(juce::TextEditor&) override {}
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
        data.slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        data.slider->setColour(juce::Slider::thumbColourId, color);
        data.slider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF2A2A2A));
        data.slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF1A1A1A));
        data.slider->setTooltip(tooltip);
        data.slider->addListener(this);
        data.slider->setDoubleClickReturnValue(true, 0.0);
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
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
            
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
        juce::String text = commandInput.getText().trim();
        if (text.isEmpty()) return;
        
        auto parsed = semanticEngine.parseNaturalLanguage(text);
        
        if (parsed.empty())
        {
            statusLabel.setText("Couldn't understand command", juce::dontSendNotification);
            return;
        }
        
        // Apply parsed qualities
        for (const auto& [quality, amount] : parsed)
        {
            float current = semanticEngine.getQuality(quality);
            float newValue = juce::jlimit(-1.0f, 1.0f, current + amount);
            semanticEngine.setQuality(quality, newValue);
        }
        
        syncSlidersFromEngine();
        updateEQFromState();
        
        commandInput.clear();
        statusLabel.setText("Applied: " + text, juce::dontSendNotification);
    }
    
    void resetAllSliders()
    {
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
            
        // Generate EQ adjustments from current semantic state
        auto adjustments = semanticEngine.generateEQFromState({}, 44100.0);
        
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
        
        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillEllipse(static_cast<float>(vizX), static_cast<float>(vizY), 
                     static_cast<float>(vizSize), static_cast<float>(vizSize));
        
        g.setColour(juce::Colour(0xFF333333));
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
        g.setColour(juce::Colour(0xFFE6A23C));
        g.fillEllipse(centerX - 3, centerY - 3, 6, 6);
    }
    
    //==========================================================================
    SemanticEQEngine& semanticEngine;
    
    juce::Label titleLabel, subtitleLabel, intensityLabel, statusLabel;
    juce::TextEditor commandInput;
    juce::TextButton applyButton, resetButton, morphButton;
    juce::Slider intensitySlider;
    
    std::vector<QualitySliderData> qualitySliders;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SemanticControlPanel)
};

