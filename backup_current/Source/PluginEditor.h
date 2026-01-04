#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GUI/ModernLookAndFeel.h"
#include "GUI/AdvancedSpectrumDisplay.h"
#include "GUI/AIControlPanel.h"
#include "GUI/EQBandControl.h"
#include "GUI/AIProblemPanel.h"
#include "GUI/BandControlPanel.h"
#include "GUI/DynamicEQPanel.h"
#include "GUI/BandViewport.h"
#include "GUI/SemanticControlPanel.h"

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
    static constexpr int headerH = 38;
    static constexpr int controlH = 130;
    static constexpr int aiPanelW = 440;   // Larger panel for detailed AI problem display
    static constexpr int bandPanelH = 140;
    static constexpr int pad = 6;
    
    // Header
    juce::TextButton prevBtn{"<"}, nextBtn{">"};
    juce::ComboBox presetBox;
    juce::ComboBox phaseModeCombo;
    juce::ToggleButton btnA{"A"}, btnB{"B"};
    juce::TextButton copyBtn{"A>B"};
    juce::ToggleButton btnPre{"PRE"}, btnPost{"POST"}, btnDelta{"DELTA"};
    juce::ToggleButton bypassBtn{"BYPASS"};
    
    // Control Panel
    juce::Label logoLabel, subtitleLabel;
    std::array<juce::ToggleButton, 8> bandToggles;
    juce::Label gainLabel, sensitivityLabel, strengthLabel, outLabel, qualityLabel, phaseModeLabel;
    juce::Slider gainKnob, sensitivityKnob, strengthKnob, outKnob;
    juce::Label gainValue, outValue;
    juce::ToggleButton autoBtn{"AUTO"};
    juce::TextButton qualityBtn{"ZL"};
    juce::TextButton captureAnalyzeBtn{"CAPTURE ANALYZE"};
    juce::TextButton startCaptureBtn{"START CAPTURE"};
    juce::TextButton stopCaptureBtn{"STOP CAPTURE"};
    juce::ToggleButton autoCaptureBtn{"AUTO CAPTURE"};
    juce::Slider captureLenSlider;
    juce::Label captureLenLabel;
    juce::Label captureStatusLabel;

    // Number of bands control
    juce::Label numBandsLabel;
    juce::ComboBox numBandsCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> numBandsAtt;
    
    // Main
    std::unique_ptr<AdvancedSpectrumDisplay> spectrum;
    std::unique_ptr<AIProblemPanel> aiProblemPanel;
    juce::OwnedArray<EQBandControl> bands;
    std::unique_ptr<class BandViewport> bandViewport;
    
    // Selected band for detail view
    int selectedBand = 0;
    std::unique_ptr<BandControlPanel> selectedBandPanel;
    
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
        outAtt, sensitivityAtt, strengthAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> 
        phaseModeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessorEditor)
};
