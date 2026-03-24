#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "GUI/ModernLookAndFeel.h"
#include "GUI/AdvancedSpectrumDisplay.h"
#include "GUI/AIControlPanel.h"
#include "GUI/EQBandControl.h"
#include "GUI/AIProblemPanel.h"
#include "GUI/BandControlPanel.h"
#include "GUI/DynamicEQPanel.h"
#include "GUI/PremiumKnob.h"
#include "GUI/BandViewport.h"
#include "GUI/SemanticControlPanel.h"
#include "GUI/LevelMeter.h"
#include <atomic>
#include <vector>

class CaptureWaveformView : public juce::Component
{
public:
    void setData(const std::vector<float>& samples, double sr, bool recording)
    {
        sampleRate = sr;
        isRecording = recording;
        
        display.clear();
        if (!samples.empty())
        {
            const size_t targetPoints = 512;
            const size_t step = std::max<size_t>(1, samples.size() / targetPoints);
            display.reserve(std::min<size_t>(targetPoints, samples.size()));
            
            for (size_t i = 0; i < samples.size(); i += step)
                display.push_back(juce::jlimit(-1.0f, 1.0f, samples[i]));
        }
        
        repaint();
    }
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        
        g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(0.25f));
        g.fillRoundedRectangle(area, 4.0f);
        
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
        
        // Midline
        g.setColour(ModernLookAndFeel::Colors::bgLighter.withAlpha(0.45f));
        g.drawHorizontalLine((int)area.getCentreY(), area.getX() + 4.0f, area.getRight() - 4.0f);
        
        if (display.empty())
        {
            g.setColour(ModernLookAndFeel::Colors::textMuted);
            g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
            g.drawText(isRecording ? "Recording..." : "Waiting for capture",
                       area.toNearestInt(), juce::Justification::centred);
            return;
        }
        
        const float width = area.getWidth();
        const float height = area.getHeight() * 0.8f;
        const float xStep = width / std::max<int>(1, (int)display.size() - 1);
        const float midY = area.getCentreY();
        
        juce::Path p;
        p.startNewSubPath(area.getX(), midY - display[0] * height * 0.5f);
        for (size_t i = 1; i < display.size(); ++i)
        {
            const float x = area.getX() + (float)i * xStep;
            const float y = midY - display[i] * height * 0.5f;
            p.lineTo(x, y);
        }
        
        g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.9f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
private:
    std::vector<float> display;
    double sampleRate = 44100.0;
    bool isRecording = false;
};

//==============================================================================
/**
 * AI Equalizer Pro - TDR Nova Style GUI
 * 
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │  ← → [Preset▼]          [A B] [A>B]     [PRE][POST]   [?][⚙][♥]     │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │                                                                      │
 * │                     SPECTRUM ANALYZER                                │
 * │              ○I    ○II    ○III    ○IV                               │
 * │                                                                      │
 * ├──────────────────────────────────────────────────────────────────────┤
 * │  AI EQ PRO       [Band Selector]     [KNOBS]           [OUT GAIN]   │
 * │  Standard        I II III IV                                        │
 * └──────────────────────────────────────────────────────────────────────┘
 */
class AIEqualizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::Timer
{
public:
    explicit AIEqualizerAudioProcessorEditor(AIEqualizerAudioProcessor&);
    ~AIEqualizerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void createHeader();
    void createControlPanel();
    void createBands();
    void showOptionsMenu();
    void updateBandPositions();
    void onBandChanged(int idx, const EQBandControl::BandParameters& p);
    void selectBand(int bandIndex);
    
    float freqToX(float f);
    float gainToY(float g);
    float xToFreq(float x);
    float yToGain(float y);

    AIEqualizerAudioProcessor& processor;
    ModernLookAndFeel lookAndFeel;
    
    // Layout
    static constexpr int headerH = 32;
    static constexpr int controlH = 55;
    static constexpr int bandPanelH = 140;
    static constexpr int pad = 6;
    bool aiPanelVisible = false;
    
    // Header
    juce::TextButton prevBtn{"<"}, nextBtn{">"};
    juce::ComboBox presetBox;
    juce::TextButton optionsBtn{"Options"};
    juce::TextButton aiPanelToggle{"AI"};
    juce::ComboBox phaseModeCombo;
    juce::ToggleButton btnA{"A"}, btnB{"B"};
    juce::TextButton copyBtn{"A>B"};
    juce::ToggleButton btnPre{"PRE"}, btnPost{"POST"}, btnDelta{"DELTA"};
    juce::ToggleButton bypassBtn{"BYPASS"};
    
    // Control Panel
    juce::Label logoLabel, subtitleLabel;
    std::vector<std::unique_ptr<juce::ToggleButton>> bandToggles;
    juce::ComboBox bandSelectCombo;
    juce::Label gainLabel, sensitivityLabel, strengthLabel, outLabel, mixLabel, slopeLabel, qualityLabel, phaseModeLabel;
    juce::Label oversamplingLabel;
    juce::Slider gainKnob;
    PremiumKnob sensitivityKnob { "SENS" }, strengthKnob { "STR" }, outKnob { "OUT" }, mixKnob { "MIX" };
    juce::Label gainValue, outValue, mixValue;
    juce::ToggleButton autoBtn{"AUTO"};
    juce::TextButton qualityBtn{"ZL"};
    juce::ComboBox oversamplingCombo;
    juce::ComboBox slopeCombo;
    juce::TextButton captureAnalyzeBtn{"CAPTURE ANALYZE"};
    juce::TextButton startCaptureBtn{"START CAPTURE"};
    juce::TextButton stopCaptureBtn{"STOP CAPTURE"};
    juce::ToggleButton autoCaptureBtn{"AUTO CAPTURE"};
    juce::Slider captureLenSlider;
    juce::Label captureLenLabel;
    juce::Label captureStatusLabel;
    std::unique_ptr<CaptureWaveformView> captureWaveform;

    // Number of bands control
    juce::Label numBandsLabel;
    juce::ComboBox numBandsCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> numBandsAtt;
    
    // OpenGL context — accelerates all JUCE software rendering via GPU compositing.
    // Attach to the top-level editor so every child component benefits automatically.
    juce::OpenGLContext openGLContext;

    // Main
    std::unique_ptr<AdvancedSpectrumDisplay> spectrum;
    std::unique_ptr<AIProblemPanel> aiProblemPanel;
    juce::OwnedArray<EQBandControl> bands;
    std::unique_ptr<class BandViewport> bandViewport;
    
    // Selected band for detail view
    int selectedBand = 0;
    std::unique_ptr<BandControlPanel> selectedBandPanel;
    std::atomic<bool> isAnalyzing { false };
    // Analysis threads: stored so destructor can join them before teardown,
    // avoiding detached threads that outlive the editor (use-after-free risk).
    std::vector<std::thread> analysisThreads;
    
    // Dynamic EQ controls
    std::unique_ptr<DynamicEQPanel> dynamicEQPanel;
    std::unique_ptr<DynamicEQMasterPanel> dynamicEQMasterPanel;
    
    // Semantic Control
    std::unique_ptr<SemanticControlPanel> semanticPanel;
    
    // Tab buttons for right panel switching
    juce::TextButton aiTabBtn{"AI DETECT"};
    juce::TextButton semanticTabBtn{"SEMANTIC"};
    int activeRightTab = 0;  // 0 = AI Detect, 1 = Semantic
    void switchRightTab(int tab);
    
    juce::Rectangle<int> spectrumBounds;
    juce::Rectangle<int> graphBounds;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> 
        bypassAtt, preAtt, postAtt, deltaAtt, autoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> 
        outAtt, mixAtt, sensitivityAtt, strengthAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> 
        phaseModeAtt, oversamplingAtt, slopeAtt, sourceProfileAtt;

    // Output level meter
    LevelMeter outputMeter;
    juce::Label versionLabel;

    uint64_t lastParameterChangeCount = 0;
    uint32_t lastBlockClampEvents = 0;
    
    // Timer throttle: spread heavy UI work across multiple ticks to avoid
    // message thread starvation (Ableton freeze). timerTickCount increments
    // each timerCallback() call; heavy work runs only on selected ticks.
    int timerTickCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessorEditor)
};
