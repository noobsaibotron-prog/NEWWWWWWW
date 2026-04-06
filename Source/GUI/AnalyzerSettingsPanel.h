#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ModernLookAndFeel.h"

//==============================================================================
/**
 * Analyzer Settings Panel - Pro-Q 4 Style
 * 
 * Compact overlay panel with:
 * - FFT Resolution (1024/2048/4096/8192)
 * - Analyzer Speed (Fast/Medium/Slow)
 * - dB Range (-90 to +12, -60 to +12, etc.)
 * - Tilt Compensation toggle
 * - Channel selector (L/R/L+R/M/S)
 */
class AnalyzerSettingsPanel : public juce::Component
{
public:
    AnalyzerSettingsPanel(juce::AudioProcessorValueTreeState& apvts)
        : parameters(apvts)
    {
        setOpaque(false);
        
        // Title
        titleLabel.setText("ANALYZER SETTINGS", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
        titleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textBright());
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // FFT Resolution
        fftLabel.setText("Resolution", juce::dontSendNotification);
        fftLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        fftLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        addAndMakeVisible(fftLabel);
        
        fftCombo.addItem("Low (1024)", 1);
        fftCombo.addItem("Medium (2048)", 2);
        fftCombo.addItem("High (4096)", 3);
        fftCombo.addItem("Max (8192)", 4);
        fftCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark());
        fftCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary());
        fftCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter());
        addAndMakeVisible(fftCombo);
        fftAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, "analyzerResolution", fftCombo);
        
        // Speed
        speedLabel.setText("Speed", juce::dontSendNotification);
        speedLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        speedLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        addAndMakeVisible(speedLabel);
        
        speedCombo.addItem("Fast", 1);
        speedCombo.addItem("Medium", 2);
        speedCombo.addItem("Slow", 3);
        speedCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark());
        speedCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary());
        speedCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter());
        addAndMakeVisible(speedCombo);
        speedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            parameters, "analyzerSpeed", speedCombo);
        
        // Tilt Compensation
        tiltToggle.setButtonText("Tilt (4.5 dB/oct)");
        tiltToggle.setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textPrimary());
        tiltToggle.setColour(juce::ToggleButton::tickColourId, ModernLookAndFeel::Colors::accentBlue());
        tiltToggle.setTooltip("Apply pink noise slope compensation for more musical display");
        addAndMakeVisible(tiltToggle);
        tiltAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, "analyzerTilt", tiltToggle);
        
        // Piano Roll
        pianoToggle.setButtonText("Piano Keys");
        pianoToggle.setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textPrimary());
        pianoToggle.setColour(juce::ToggleButton::tickColourId, ModernLookAndFeel::Colors::accentBlue());
        pianoToggle.setTooltip("Show piano keyboard with note names under spectrum");
        addAndMakeVisible(pianoToggle);
        pianoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            parameters, "pianoRollOverlay", pianoToggle);
        
        // Channel selector
        channelLabel.setText("Channel", juce::dontSendNotification);
        channelLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        channelLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        addAndMakeVisible(channelLabel);
        
        channelCombo.addItem("L+R (Stereo)", 1);
        channelCombo.addItem("Left", 2);
        channelCombo.addItem("Right", 3);
        channelCombo.addItem("Mid", 4);
        channelCombo.addItem("Side", 5);
        channelCombo.setSelectedId(1, juce::dontSendNotification);
        channelCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark());
        channelCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary());
        channelCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter());
        channelCombo.onChange = [this]() {
            if (onChannelChanged)
                onChannelChanged(channelCombo.getSelectedId() - 1);
        };
        addAndMakeVisible(channelCombo);
        
        // dB Range
        rangeLabel.setText("Range", juce::dontSendNotification);
        rangeLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        rangeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        addAndMakeVisible(rangeLabel);
        
        rangeCombo.addItem("-90 to +12 dB", 1);
        rangeCombo.addItem("-60 to +12 dB", 2);
        rangeCombo.addItem("-48 to +12 dB", 3);
        rangeCombo.setSelectedId(1, juce::dontSendNotification);
        rangeCombo.setColour(juce::ComboBox::backgroundColourId, ModernLookAndFeel::Colors::bgDark());
        rangeCombo.setColour(juce::ComboBox::textColourId, ModernLookAndFeel::Colors::textPrimary());
        rangeCombo.setColour(juce::ComboBox::outlineColourId, ModernLookAndFeel::Colors::bgLighter());
        rangeCombo.onChange = [this]() {
            if (onRangeChanged)
            {
                float minDb = -90.0f;
                switch (rangeCombo.getSelectedId())
                {
                    case 2: minDb = -60.0f; break;
                    case 3: minDb = -48.0f; break;
                    default: minDb = -90.0f; break;
                }
                onRangeChanged(minDb, 12.0f);
            }
        };
        addAndMakeVisible(rangeCombo);
        
        // Close button
        closeButton.setButtonText("×");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textMuted());
        closeButton.onClick = [this]() {
            setVisible(false);
            if (onClose) onClose();
        };
        addAndMakeVisible(closeButton);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Semi-transparent dark background
        g.setColour(ModernLookAndFeel::Colors::bgPanel().withAlpha(0.97f));
        g.fillRoundedRectangle(bounds, 8.0f);
        
        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter());
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.5f);
        
        // Accent line at top
        g.setColour(ModernLookAndFeel::Colors::accentBlue());
        g.fillRoundedRectangle(bounds.removeFromTop(3.0f).reduced(20.0f, 0.0f), 1.5f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);
        
        // Close button (top-right)
        closeButton.setBounds(bounds.removeFromRight(24).removeFromTop(24));
        
        // Title
        titleLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(8);
        
        const int rowH = 28;
        const int labelW = 70;
        const int gap = 6;
        
        // FFT Resolution
        auto fftRow = bounds.removeFromTop(rowH);
        fftLabel.setBounds(fftRow.removeFromLeft(labelW));
        fftCombo.setBounds(fftRow.reduced(0, 2));
        bounds.removeFromTop(gap);
        
        // Speed
        auto speedRow = bounds.removeFromTop(rowH);
        speedLabel.setBounds(speedRow.removeFromLeft(labelW));
        speedCombo.setBounds(speedRow.reduced(0, 2));
        bounds.removeFromTop(gap);
        
        // Channel
        auto channelRow = bounds.removeFromTop(rowH);
        channelLabel.setBounds(channelRow.removeFromLeft(labelW));
        channelCombo.setBounds(channelRow.reduced(0, 2));
        bounds.removeFromTop(gap);
        
        // Range
        auto rangeRow = bounds.removeFromTop(rowH);
        rangeLabel.setBounds(rangeRow.removeFromLeft(labelW));
        rangeCombo.setBounds(rangeRow.reduced(0, 2));
        bounds.removeFromTop(gap + 4);
        
        // Toggles
        tiltToggle.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(4);
        pianoToggle.setBounds(bounds.removeFromTop(24));
    }
    
    // Callbacks
    std::function<void()> onClose;
    std::function<void(int)> onChannelChanged;  // 0=L+R, 1=L, 2=R, 3=M, 4=S
    std::function<void(float, float)> onRangeChanged;  // minDb, maxDb
    
private:
    juce::AudioProcessorValueTreeState& parameters;
    
    juce::Label titleLabel;
    juce::Label fftLabel, speedLabel, channelLabel, rangeLabel;
    juce::ComboBox fftCombo, speedCombo, channelCombo, rangeCombo;
    juce::ToggleButton tiltToggle, pianoToggle;
    juce::TextButton closeButton;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftAtt, speedAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tiltAtt, pianoAtt;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerSettingsPanel)
};

//==============================================================================
/**
 * Spectrum Zoom Controls - Pro-Q 4 Style
 * 
 * Compact controls for:
 * - Horizontal zoom (frequency range)
 * - Vertical zoom (dB range)
 * - Reset button
 */
class SpectrumZoomControls : public juce::Component
{
public:
    SpectrumZoomControls()
    {
        // Zoom X (horizontal - frequency)
        zoomXLabel.setText("X", juce::dontSendNotification);
        zoomXLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        zoomXLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        zoomXLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(zoomXLabel);
        
        zoomXMinus.setButtonText("−");
        zoomXMinus.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight());
        zoomXMinus.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textBright());
        zoomXMinus.onClick = [this]() { adjustZoomX(-0.25f); };
        addAndMakeVisible(zoomXMinus);
        
        zoomXPlus.setButtonText("+");
        zoomXPlus.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight());
        zoomXPlus.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textBright());
        zoomXPlus.onClick = [this]() { adjustZoomX(0.25f); };
        addAndMakeVisible(zoomXPlus);
        
        // Zoom Y (vertical - dB)
        zoomYLabel.setText("Y", juce::dontSendNotification);
        zoomYLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        zoomYLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted());
        zoomYLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(zoomYLabel);
        
        zoomYMinus.setButtonText("−");
        zoomYMinus.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight());
        zoomYMinus.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textBright());
        zoomYMinus.onClick = [this]() { adjustZoomY(-0.25f); };
        addAndMakeVisible(zoomYMinus);
        
        zoomYPlus.setButtonText("+");
        zoomYPlus.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight());
        zoomYPlus.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textBright());
        zoomYPlus.onClick = [this]() { adjustZoomY(0.25f); };
        addAndMakeVisible(zoomYPlus);
        
        // Reset button
        resetButton.setButtonText("⟲");
        resetButton.setTooltip("Reset zoom to default");
        resetButton.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight());
        resetButton.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::accentBlue());
        resetButton.onClick = [this]() { resetZoom(); };
        addAndMakeVisible(resetButton);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(ModernLookAndFeel::Colors::bgDark().withAlpha(0.8f));
        g.fillRoundedRectangle(bounds, 4.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(4);
        const int btnW = 22;
        const int btnH = 18;
        const int gap = 2;
        
        // X zoom controls
        zoomXLabel.setBounds(bounds.removeFromLeft(14));
        zoomXMinus.setBounds(bounds.removeFromLeft(btnW).reduced(0, (bounds.getHeight() - btnH) / 2));
        bounds.removeFromLeft(gap);
        zoomXPlus.setBounds(bounds.removeFromLeft(btnW).reduced(0, (bounds.getHeight() - btnH) / 2));
        
        bounds.removeFromLeft(8);
        
        // Y zoom controls
        zoomYLabel.setBounds(bounds.removeFromLeft(14));
        zoomYMinus.setBounds(bounds.removeFromLeft(btnW).reduced(0, (bounds.getHeight() - btnH) / 2));
        bounds.removeFromLeft(gap);
        zoomYPlus.setBounds(bounds.removeFromLeft(btnW).reduced(0, (bounds.getHeight() - btnH) / 2));
        
        bounds.removeFromLeft(8);
        
        // Reset
        resetButton.setBounds(bounds.removeFromLeft(btnW).reduced(0, (bounds.getHeight() - btnH) / 2));
    }
    
    // Zoom state
    float getZoomX() const { return zoomX; }
    float getZoomY() const { return zoomY; }
    float getPanX() const { return panX; }  // 0 = centered
    
    void setZoom(float x, float y, float pan = 0.5f)
    {
        zoomX = juce::jlimit(1.0f, 8.0f, x);
        zoomY = juce::jlimit(1.0f, 4.0f, y);
        panX = juce::jlimit(0.0f, 1.0f, pan);
        if (onZoomChanged) onZoomChanged(zoomX, zoomY, panX);
        repaint();
    }
    
    // Callbacks
    std::function<void(float, float, float)> onZoomChanged;  // zoomX, zoomY, panX
    
private:
    void adjustZoomX(float delta)
    {
        setZoom(zoomX + delta, zoomY, panX);
    }
    
    void adjustZoomY(float delta)
    {
        setZoom(zoomX, zoomY + delta, panX);
    }
    
    void resetZoom()
    {
        setZoom(1.0f, 1.0f, 0.5f);
    }
    
    juce::Label zoomXLabel, zoomYLabel;
    juce::TextButton zoomXMinus, zoomXPlus;
    juce::TextButton zoomYMinus, zoomYPlus;
    juce::TextButton resetButton;
    
    float zoomX = 1.0f;
    float zoomY = 1.0f;
    float panX = 0.5f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumZoomControls)
};

