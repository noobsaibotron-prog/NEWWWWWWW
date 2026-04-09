#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "../AI/SemanticEQEngine.h"
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
    
    // Callback to get generated EQ adjustments
    std::function<void(const std::vector<SemanticEQEngine::SemanticEQAdjustment>&)> onEQGenerated;

    // Must be called when the host sample rate changes (e.g. from PluginEditor::prepareToPlay)
    void setSampleRate(double sr) { currentSampleRate = sr; }

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
        intensitySlider.setColour(juce::Slider::thumbColourId, ModernLookAndFeel::Colors::accentYellow);
        intensitySlider.setColour(juce::Slider::trackColourId, ModernLookAndFeel::Colors::bgLighter);
        intensitySlider.onValueChange = [this]() {
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
        
        startTimerHz(30);
    }
    
    ~SemanticControlPanel() override
    {
        stopTimer();
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
        bool needsRepaint = false;

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
        data.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 16);
        data.slider->setColour(juce::Slider::thumbColourId, color);
        data.slider->setColour(juce::Slider::trackColourId, ModernLookAndFeel::Colors::bgLight);
        data.slider->setColour(juce::Slider::backgroundColourId, ModernLookAndFeel::Colors::bgDark);
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

#if AIEQ_GUI_DEBUG
        debugUpdateEQCount++;
#endif
        // Generate EQ adjustments from current semantic state
        auto adjustments = semanticEngine.generateEQFromState({}, currentSampleRate);
        
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
    bool semanticDirty = false;            // Coalesce semantic updates to timer rate

#if AIEQ_GUI_DEBUG
    int debugSliderEventCount = 0;
    int debugUpdateEQCount = 0;
    double debugLastSliderReport = 0.0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SemanticControlPanel)
};

