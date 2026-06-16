#pragma once
#include <optional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "GUI/ModernLookAndFeel.h"
#include "GUI/AdvancedSpectrumDisplay.h"
#include "GUI/EQBandControl.h"
#include "GUI/AIProblemPanel.h"
#include "GUI/BandControlPanel.h"
#include "GUI/DynamicEQPanel.h"
#include "GUI/AIBreathingDot.h"
#include "GUI/PremiumKnob.h"
#include "GUI/SemanticControlPanel.h"
#include "GUI/LevelMeter.h"
#include "GUI/NewSpectrumPipeline.h"
#include <atomic>
#include <vector>

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
    // Wave 4A: headerH bumped 36 → 44 to give the 20px Bold logo + 20px Bold
    // PRE/POST/DELTA pill toggles enough vertical breathing room.
    static constexpr int headerH = 44;
    static constexpr int footerH = 52;    // footer bar (meter, OUT/MIX amber knobs, bypass)
    static constexpr int controlH = 300;  // bottom panel (band controls + context)
                                          // 300 gives BandControlPanel proper room for
                                          // the 3 LargeAmber filmstrip knobs at ~138px
                                          // and AIProblemPanel ~3 visible problem rows.
    static constexpr int bandPanelH = 140;
    static constexpr int pad = 6;
    bool aiPanelVisible = true;   // always visible in bottom panel
    
    // Header
    juce::TextButton prevBtn{"<"}, nextBtn{">"};
    juce::ComboBox presetBox;
    juce::TextButton savePresetBtn{"SAVE"};
    juce::TextButton optionsBtn{"Options"};
    juce::TextButton aiPanelToggle{"AI"};
    juce::ComboBox phaseModeCombo;
    juce::ToggleButton btnA{"A"}, btnB{"B"}, btnC{"C"}, btnD{"D"};
    juce::TextButton copyBtn{"Copy"};
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
    juce::TextButton oversamplingBtn{"OFF"};  // cycles Off→2x→4x→Auto
    juce::ComboBox oversamplingCombo;         // hidden, keeps APVTS attachment
    juce::ComboBox slopeCombo;
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
    
    // OpenGL context — accelerates all JUCE software rendering via GPU compositing
    // (setComponentPaintingEnabled). No custom GL renderer is attached: the
    // spectrum is drawn by the software path (the old GL spectrum renderer was
    // dead code, removed in GUI-1).
    juce::OpenGLContext openGLContext;

    // Premium matericità: 256×256 tiled procedural noise texture (generated once).
    // Overlaid at 2-3% opacity on backgrounds to simulate brushed aluminium /
    // polycarbonate surface and eliminate gradient banding.
    juce::Image noiseTexture;

    // Metrological 5-layer spectrum pipeline (Parseval-correct, IIR ballistics, log LUT)
    std::unique_ptr<NewSpectrumPipeline> spectrumPipeline;

    // APVTS attachment: forwards "analyzerResolution" param changes (menu,
    // preset load, DAW automation) to spectrumPipeline->setFFTOrder() so both
    // PRE and POST always share the same FFT resolution (industry-standard
    // behaviour; non-negotiable per product requirement 2026-04-23).
    std::unique_ptr<juce::ParameterAttachment> analyzerResolutionAttachment;

    // Wave 5 verdict: BandTabBar (Roman numerals above spectrum) removed.
    // Band identity now conveyed exclusively via the coloured node rings on
    // the EQ curve + the three large filmstrip knobs in the left control panel.
    // BandTabBar.h remains in repo, disconnected.

    // DynEQ controls now integrated directly in BandControlPanel

    // Phase 7D: passive breathing amber dot next to logo, driven by timerCallback
    AIBreathingDot  aiBreathingDot;
    float           breathingPhase = 0.0f;

    // Main
    std::unique_ptr<AdvancedSpectrumDisplay> spectrum;
    std::unique_ptr<AIProblemPanel> aiProblemPanel;
    juce::OwnedArray<EQBandControl> bands;
    
    // Selected band for detail view
    int selectedBand = 0;
    std::unique_ptr<BandControlPanel> selectedBandPanel;
    std::atomic<bool> isAnalyzing { false };
    // Single analysis thread — joined before launching a new one (isAnalyzing serializes).
    std::optional<std::thread> analysisThread;
    
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
        phaseModeAtt, oversamplingAtt, slopeAtt;

    // Output level meter
    LevelMeter outputMeter;
    juce::Label versionLabel;

    // Preset navigation
    std::vector<PresetManager::Preset> cachedPresetList;
    int currentPresetIndex = -1;
    void rebuildPresetMenu();
    void navigatePreset(int direction);
    void showSavePresetDialog();

    uint64_t lastParameterChangeCount = 0;
    uint32_t lastBlockClampEvents = 0;
    uint32_t totalClickEvents = 0;  // cumulative click detector counter
    
    // Timer throttle: spread heavy UI work across multiple ticks to avoid
    // message thread starvation (Ableton freeze). timerTickCount increments
    // each timerCallback() call; heavy work runs only on selected ticks.
    int timerTickCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEqualizerAudioProcessorEditor)
};
