#include "PluginEditor.h"
#include "GUI/BandViewport.h"
#include "Utils/Logger.h"
#include <thread>

AIEqualizerAudioProcessorEditor::AIEqualizerAudioProcessorEditor(AIEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    
    createHeader();
    createControlPanel();
    createBands();
    
    spectrum = std::make_unique<AdvancedSpectrumDisplay>(processor);
    addAndMakeVisible(*spectrum);
    
    // Setup spectrum callbacks for band interaction
    spectrum->onBandSelected = [this](int bandIndex) {
        selectBand(bandIndex);
    };
    
    spectrum->onBandCreatedOrActivated = [this](int bandIndex, float /*freq*/, float /*gain*/) {
        selectBand(bandIndex);
    };
    
    spectrum->onBandDragged = [this](int bandIndex, float /*freq*/, float /*gain*/, float /*q*/) {
        // Update the band toggles and panels if this band is selected
        if (bandIndex == selectedBand)
        {
            // Panels will auto-update from APVTS
        }
    };
    
    // AI Problem Panel (right side)
    aiProblemPanel = std::make_unique<AIProblemPanel>(processor);
    addAndMakeVisible(*aiProblemPanel);
    
    // Connect AI Problem Panel to Spectrum Display
    // When user clicks a problem, highlight it on the spectrum
    aiProblemPanel->onProblemSelected = [this](float frequency, float q, float severity, AIEngine::ProblemType type) {
        if (spectrum)
        {
            spectrum->highlightProblem(frequency, q, severity, type);
        }
    };
    
    // Semantic Control Panel (alternative right panel)
    semanticPanel = std::make_unique<SemanticControlPanel>(processor.getSemanticEngine());
    addChildComponent(*semanticPanel);  // Hidden by default
    
    // Connect Semantic Panel to apply EQ changes
    semanticPanel->onEQGenerated = [this](const std::vector<SemanticEQEngine::SemanticEQAdjustment>& adjustments) {
        // Apply semantic EQ adjustments without disturbing existing manual bands
        processor.applySemanticAdjustments(adjustments);
    };
    
    // Tab buttons for switching between AI Detect and Semantic panels
    aiTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A9FD9));
    aiTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    aiTabBtn.setTooltip("AI Problem Detection - Automatic issue identification");
    aiTabBtn.onClick = [this]() { switchRightTab(0); };
    addAndMakeVisible(aiTabBtn);
    
    semanticTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
    semanticTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
    semanticTabBtn.setTooltip("Semantic Control - Shape sound with words like 'Air', 'Warmth', 'Punch'");
    semanticTabBtn.onClick = [this]() { switchRightTab(1); };
    addAndMakeVisible(semanticTabBtn);

    optionsBtn.setTooltip("Global options and analyzer settings");
    optionsBtn.onClick = [this]() { showOptionsMenu(); };
    addAndMakeVisible(optionsBtn);
    
    // Band viewport (scrollable panels)
    bandViewport = std::make_unique<BandViewport>(processor.getAPVTS(), AIEqualizerAudioProcessor::maxBands);
    bandViewport->setNumBands(processor.getNumActiveBands());
    addAndMakeVisible(*bandViewport);
    
    // Dynamic EQ Panel (per-band controls)
    dynamicEQPanel = std::make_unique<DynamicEQPanel>(processor.getAPVTS(), 0);
    dynamicEQPanel->setBandMeterProvider([this](int band) {
        return processor.getDynamicBandMeter(band);
    });
    addAndMakeVisible(*dynamicEQPanel);
    
    // Dynamic EQ Master Panel (global controls)
    dynamicEQMasterPanel = std::make_unique<DynamicEQMasterPanel>(processor.getAPVTS());
    dynamicEQMasterPanel->setTotalGRProvider([this]() {
        return processor.getDynamicTotalGainReduction();
    });
    addAndMakeVisible(*dynamicEQMasterPanel);
    
    // FIX 2: persistent selected band panel (avoid recreating on every selection)
    selectedBandPanel = std::make_unique<BandControlPanel>(0, processor.getAPVTS());
    addAndMakeVisible(*selectedBandPanel);
    
    setSize(1200, 750);
    setResizable(true, true);
    setResizeLimits(1100, 680, 1800, 1100);
    
    // Ensure a band is selected so the detail panel shows controls (including filter type)
    selectBand(0);
    
    // FIX 9: 30Hz is sufficient for smooth UI, reduces CPU load
    startTimerHz(30);
}

AIEqualizerAudioProcessorEditor::~AIEqualizerAudioProcessorEditor()
{
    while (isAnalyzing.load(std::memory_order_acquire))
        juce::Thread::sleep(5);
    
    stopTimer();
    setLookAndFeel(nullptr);
}

void AIEqualizerAudioProcessorEditor::createHeader()
{
    // Nav buttons with tooltips
    prevBtn.setTooltip("Previous preset");
    nextBtn.setTooltip("Next preset");
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);
    
    // Preset with tooltip
    {
        static const char* profiles[] = { "Generic", "Vocals", "Drums", "Bass", "Synth", "Master", "EDM" };
        presetBox.clear(juce::dontSendNotification);
        for (int i = 0; i < 7; ++i)
            presetBox.addItem(profiles[i], i + 1);

        if (auto* param = processor.getAPVTS().getParameter("sourceProfile"))
        {
            const int idx = static_cast<int>(param->convertFrom0to1(param->getValue()) + 0.5f);
            presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
        }
    }
    presetBox.setTooltip("Select source profile - AI adapts detection thresholds accordingly");
    sourceProfileAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "sourceProfile", presetBox);
    addAndMakeVisible(presetBox);
    
    // A/B with tooltips
    btnA.setRadioGroupId(1001);
    btnA.setToggleState(true, juce::dontSendNotification);
    btnA.setTooltip("Switch to A slot - Compare different EQ settings");
    btnA.onClick = [this]() {
        processor.setABState(AIEqualizerAudioProcessor::ABState::A);
        btnA.setToggleState(true, juce::dontSendNotification);
        btnB.setToggleState(false, juce::dontSendNotification);
    };
    addAndMakeVisible(btnA);
    
    btnB.setRadioGroupId(1001);
    btnB.setTooltip("Switch to B slot - Compare different EQ settings");
    btnB.onClick = [this]() {
        processor.setABState(AIEqualizerAudioProcessor::ABState::B);
        btnA.setToggleState(false, juce::dontSendNotification);
        btnB.setToggleState(true, juce::dontSendNotification);
    };
    addAndMakeVisible(btnB);
    
    copyBtn.setTooltip("Copy current A settings to B slot");
    copyBtn.onClick = [this]() { processor.copyAtoB(); };
    addAndMakeVisible(copyBtn);
    
    // Phase mode
    phaseModeLabel.setText("Processing:", juce::dontSendNotification);
    phaseModeLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    phaseModeLabel.setJustificationType(juce::Justification::centredLeft);
    phaseModeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textPrimary);
    addAndMakeVisible(phaseModeLabel);
    
    phaseModeCombo.setJustificationType(juce::Justification::centredLeft);
    phaseModeCombo.setTextWhenNothingSelected("Select");
    phaseModeCombo.addItem("Zero Latency", 1);
    phaseModeCombo.addItem("Natural Phase", 2);
    phaseModeCombo.addItem("Linear Phase", 3);
    phaseModeCombo.setTooltip("Processing phase mode");
    addAndMakeVisible(phaseModeCombo);
    phaseModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "phaseMode", phaseModeCombo);
    
    // Spectrum toggles with tooltips
    btnPre.setToggleState(true, juce::dontSendNotification);
    btnPre.setTooltip("Show PRE-EQ spectrum (input signal before processing)");
    addAndMakeVisible(btnPre);
    preAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "showPreSpectrum", btnPre);
    
    btnPost.setTooltip("Show POST-EQ spectrum (output signal after processing)");
    addAndMakeVisible(btnPost);
    postAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "showPostSpectrum", btnPost);

    btnDelta.setTooltip("Show DELTA spectrum (Post - Pre)");
    addAndMakeVisible(btnDelta);
    deltaAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "showDeltaSpectrum", btnDelta);
    
    // Bypass with tooltip
    bypassBtn.setTooltip("Bypass all processing - Pass audio through unchanged");
    addAndMakeVisible(bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "bypass", bypassBtn);
}

void AIEqualizerAudioProcessorEditor::createControlPanel()
{
    // Logo
    logoLabel.setText("AI EQ PRO", juce::dontSendNotification);
    logoLabel.setFont(juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold")));
    logoLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textPrimary);
    addAndMakeVisible(logoLabel);
    
    subtitleLabel.setText("Intelligent Equalizer", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentBlue);
    addAndMakeVisible(subtitleLabel);
    
    // Band toggles - click to select, right-click or checkbox to enable/disable
    bandToggles.resize(AIEqualizerAudioProcessor::maxBands);
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        bandToggles[i] = std::make_unique<juce::ToggleButton>();
        auto& toggle = *bandToggles[i];
        toggle.setButtonText(juce::String(i + 1));
        toggle.setToggleState(processor.getBandState(i).enabled, juce::dontSendNotification);
        toggle.setTooltip("Click to select Band " + juce::String(i + 1) + "\nToggle checkbox to enable/disable");
        toggle.onClick = [this, i]() {
            // Toggle enables/disables the band
            auto state = processor.getBandState(i);
            state.enabled = bandToggles[i]->getToggleState();
            processor.setBandState(i, state);
            
            // Also select this band
            selectBand(i);
        };
        addAndMakeVisible(toggle);
    }

    // Band selector (covers all bands up to maxBands)
    bandSelectCombo.setTooltip("Select a band (1-" + juce::String(AIEqualizerAudioProcessor::maxBands) + ") to edit");
    bandSelectCombo.setTextWhenNothingSelected("Band");
    bandSelectCombo.setJustificationType(juce::Justification::centred);
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        bandSelectCombo.addItem("Band " + juce::String(i + 1), i + 1);
    bandSelectCombo.onChange = [this]()
    {
        const int idx = bandSelectCombo.getSelectedId() - 1;
        if (idx >= 0)
            selectBand(idx);
    };
    addAndMakeVisible(bandSelectCombo);
    
    // Sensitivity knob (connected to APVTS for save/restore)
    sensitivityLabel.setText("SENSITIVITY", juce::dontSendNotification);
    sensitivityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    sensitivityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(sensitivityLabel);
    
    sensitivityKnob.setTooltip("AI Sensitivity - Higher values detect more subtle problems\nLower values only flag obvious issues");
    addAndMakeVisible(sensitivityKnob);
    sensitivityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiSensitivity", sensitivityKnob);
    
    // Strength knob (connected to APVTS for save/restore)
    strengthLabel.setText("STRENGTH", juce::dontSendNotification);
    strengthLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    strengthLabel.setJustificationType(juce::Justification::centred);
    strengthLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(strengthLabel);
    
    strengthKnob.setTooltip("AI Strength - Controls how aggressively corrections are applied\n100% = Full suggested correction, 50% = Half correction");
    addAndMakeVisible(strengthKnob);
    strengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiStrength", strengthKnob);
    
    // Auto with tooltip
    autoBtn.setTooltip("Auto Gain - Automatically compensates for volume changes from EQ");
    addAndMakeVisible(autoBtn);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "autoGain", autoBtn);

    // Quality mode toggle (Zero Latency / High Quality)
    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    qualityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    qualityLabel.setJustificationType(juce::Justification::centred);
    qualityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(qualityLabel);

    qualityBtn.setTooltip("Quality Mode: HQ abilita 5ms di lookahead (più latenza), ZL = zero-latency");
    qualityBtn.setClickingTogglesState(true);
    qualityBtn.setComponentID("qualityToggle");
    qualityBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
    qualityBtn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::accentBlue);
    qualityBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary);
    qualityBtn.setColour(juce::TextButton::textColourOnId, ModernLookAndFeel::Colors::textBright);
    qualityBtn.setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    qualityBtn.onClick = [this]() {
        int mode = qualityBtn.getToggleState() ? 1 : 0; // 1 = High Quality, 0 = Zero Latency
        if (auto* param = processor.getAPVTS().getParameter("qualityMode"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(mode)));
    };
    addAndMakeVisible(qualityBtn);

    // Oversampling selector (Off / 2x / 4x / Auto)
    oversamplingLabel.setText("OVERSAMP", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    oversamplingLabel.setJustificationType(juce::Justification::centred);
    oversamplingLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(oversamplingLabel);

    oversamplingCombo.setJustificationType(juce::Justification::centredLeft);
    oversamplingCombo.setTextWhenNothingSelected("Off");
    oversamplingCombo.addItem("Off", 1);
    oversamplingCombo.addItem("2x", 2);
    oversamplingCombo.addItem("4x", 3);
    oversamplingCombo.addItem("Auto", 4);
    oversamplingCombo.setTooltip("Oversampling: Off/2x/4x or Auto (sceglie 2x/4x in base al Q e modalità HQ)");
    addAndMakeVisible(oversamplingCombo);
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "oversamplingFactor", oversamplingCombo);

    // Capture & analyze button (retroactive: records last N seconds and runs AI analysis)
    captureAnalyzeBtn.setButtonText("CAPTURE LAST");
    captureAnalyzeBtn.setTooltip("Capture last N seconds (retroactive) and analyze");
    captureAnalyzeBtn.onClick = [this]() {
        if (isAnalyzing.exchange(true, std::memory_order_acq_rel))
            return;

        processor.captureAudioSnapshotMs(processor.getCaptureLengthMs());
        captureAnalyzeBtn.setEnabled(false);
        captureStatusLabel.setText("Analyzing...", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);

        juce::Component::SafePointer<AIEqualizerAudioProcessorEditor> safeThis(this);
        std::thread([this, safeThis]() {
            const bool ok = processor.analyzeCapturedAudioSnapshot();
            const float sec = processor.getCaptureLengthMs() / 1000.0f;
            isAnalyzing.store(false, std::memory_order_release);

            juce::MessageManager::callAsync([safeThis, ok, sec]() {
                if (auto* editor = safeThis.getComponent())
                {
                    editor->captureAnalyzeBtn.setEnabled(true);
                    editor->captureStatusLabel.setText(
                        ok ? "Retro: last " + juce::String(sec, 1) + "s analyzed"
                           : "No audio captured",
                        juce::dontSendNotification);
                    editor->captureStatusLabel.setColour(
                        juce::Label::textColourId,
                        ok ? juce::Colours::limegreen : juce::Colours::orange);
                }
            });
        }).detach();
    };
    addAndMakeVisible(captureAnalyzeBtn);
    
    // Manual capture START button
    startCaptureBtn.setButtonText("START LIVE");
    startCaptureBtn.setTooltip("Start manual capture - records from now until STOP");
    startCaptureBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
    startCaptureBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    startCaptureBtn.onClick = [this]() {
        if (isAnalyzing.load(std::memory_order_acquire))
            return;

        if (processor.startManualCapture())
        {
            captureStatusLabel.setText("Recording live capture... press STOP to analyze",
                                       juce::dontSendNotification);
            captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
            startCaptureBtn.setEnabled(false);
            stopCaptureBtn.setEnabled(true);
        }
    };
    addAndMakeVisible(startCaptureBtn);
    
    // Manual capture STOP button
    stopCaptureBtn.setButtonText("STOP + ANALYZE");
    stopCaptureBtn.setTooltip("Stop manual capture and analyze");
    stopCaptureBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5A2D2D));
    stopCaptureBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopCaptureBtn.setEnabled(false);
    stopCaptureBtn.onClick = [this]() {
        if (isAnalyzing.exchange(true, std::memory_order_acq_rel))
            return;

        processor.stopManualCapture();
        captureStatusLabel.setText("Analyzing...", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
        startCaptureBtn.setEnabled(false);
        stopCaptureBtn.setEnabled(false);

        juce::Component::SafePointer<AIEqualizerAudioProcessorEditor> safeThis(this);
        std::thread([this, safeThis]() {
            const bool ok = processor.analyzeCapturedAudioSnapshot();
            const auto& mono = processor.getCapturedAudioMono();
            const double sr = processor.getCapturedSampleRate();
            const double secs = (sr > 0.0) ? static_cast<double>(mono.size()) / sr : 0.0;
            isAnalyzing.store(false, std::memory_order_release);

            juce::MessageManager::callAsync([safeThis, ok, secs]() {
                if (auto* editor = safeThis.getComponent())
                {
                    if (ok)
                    {
                        editor->captureStatusLabel.setText(
                            "Live: " + juce::String(secs, 1) + "s analyzed",
                            juce::dontSendNotification);
                        editor->captureStatusLabel.setColour(
                            juce::Label::textColourId, juce::Colours::limegreen);
                    }
                    else
                    {
                        editor->captureStatusLabel.setText("No audio captured", juce::dontSendNotification);
                        editor->captureStatusLabel.setColour(
                            juce::Label::textColourId, juce::Colours::orange);
                    }

                    editor->startCaptureBtn.setEnabled(true);
                    editor->stopCaptureBtn.setEnabled(false);
                }
            });
        }).detach();
    };
    addAndMakeVisible(stopCaptureBtn);
    
    // Auto-capture toggle (triggers on energy peak)
    autoCaptureBtn.setButtonText("AUTO PEAK");
    autoCaptureBtn.setTooltip("Auto-capture: automatically captures on energy peaks (e.g. drop start)");
    autoCaptureBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    autoCaptureBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF4A9FD9));
    autoCaptureBtn.onClick = [this]() {
        processor.setAutoCaptureEnabled(autoCaptureBtn.getToggleState());
    };
    addAndMakeVisible(autoCaptureBtn);
    
    // Capture length slider (1-20s)
    captureLenLabel.setText("CAP LEN", juce::dontSendNotification);
    captureLenLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    captureLenLabel.setJustificationType(juce::Justification::centred);
    captureLenLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(captureLenLabel);

    captureLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    captureLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
    captureLenSlider.setRange(1.0, 20.0, 1.0);
    captureLenSlider.setValue(20.0);
    captureLenSlider.onValueChange = [this]() {
        int ms = static_cast<int>(captureLenSlider.getValue() * 1000.0);
        processor.setCaptureLengthMs(ms);
    };
    addAndMakeVisible(captureLenSlider);
    
    captureStatusLabel.setText("Retro: CAPTURE LAST. Live: START/STOP. Auto: peaks.",
                               juce::dontSendNotification);
    captureStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    captureStatusLabel.setJustificationType(juce::Justification::centredLeft);
    captureStatusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    captureStatusLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(captureStatusLabel);
    
    captureWaveform = std::make_unique<CaptureWaveformView>();
    addAndMakeVisible(*captureWaveform);

    // Number of active bands selector (1-24)
    numBandsLabel.setText("BANDS", juce::dontSendNotification);
    numBandsLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    numBandsLabel.setJustificationType(juce::Justification::centred);
    numBandsLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(numBandsLabel);
    
    for (int i = 1; i <= 24; ++i)
        numBandsCombo.addItem(juce::String(i), i);
    numBandsCombo.setSelectedId(8, juce::dontSendNotification);  // Default 8
    numBandsCombo.setTooltip("Number of active EQ bands (1-24)");
    addAndMakeVisible(numBandsCombo);
    numBandsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "numActiveBands", numBandsCombo);
    
    // Output knob with tooltip
    outLabel.setText("OUT GAIN", juce::dontSendNotification);
    outLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    outLabel.setJustificationType(juce::Justification::centred);
    outLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(outLabel);
    
    outKnob.setTooltip("Output Gain - Adjust overall output level (-24dB to +24dB)");
    addAndMakeVisible(outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outKnob);
    
    // Dry/Wet mix
    mixLabel.setText("DRY/WET", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    mixLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(mixLabel);
    
    mixKnob.setTooltip("Blend between dry (0%) and fully processed (100%) signal");
    addAndMakeVisible(mixKnob);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "dryWet", mixKnob);

    // Analyzer slope (visual)
    slopeLabel.setText("SLOPE", juce::dontSendNotification);
    slopeLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    slopeLabel.setJustificationType(juce::Justification::centred);
    slopeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(slopeLabel);

    slopeCombo.addItem("Flat", 1);
    slopeCombo.addItem("3 dB/oct", 2);
    slopeCombo.addItem("4.5 dB/oct", 3);
    slopeCombo.setJustificationType(juce::Justification::centred);
    slopeCombo.setTooltip("Analyzer visual slope compensation");
    addAndMakeVisible(slopeCombo);
    slopeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "analyzerSlope", slopeCombo);
}

void AIEqualizerAudioProcessorEditor::createBands()
{
    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        auto* b = new EQBandControl(i);
        auto state = processor.getBandState(i);
        EQBandControl::BandParameters p;
        p.frequency = state.frequency;
        p.gain = state.gain;
        p.q = state.q;
        p.filterType = state.type;
        p.enabled = state.enabled;
        b->setParameters(p);
        b->onParametersChanged = [this](int idx, const EQBandControl::BandParameters& params) {
            onBandChanged(idx, params);
        };
        addAndMakeVisible(b);
        bands.add(b);
    }
}

void AIEqualizerAudioProcessorEditor::showOptionsMenu()
{
    juce::PopupMenu menu;

    auto setChoice = [this](const juce::String& paramID, int choiceIndex)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(choiceIndex)));
            p->endChangeGesture();
        }
    };

    auto setBool = [this](const juce::String& paramID, bool value)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    };

    auto setFloat = [this](const juce::String& paramID, float value)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(value));
            p->endChangeGesture();
        }
    };

    auto getChoice = [this](const juce::String& paramID) -> int
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
            return static_cast<int>(std::round(p->getValue() * (p->getNumSteps() > 1 ? (p->getNumSteps() - 1) : 1)));
        return -1;
    };

    auto getBool = [this](const juce::String& paramID) -> bool
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
            return p->getValue() > 0.5f;
        return false;
    };

    auto getFloat = [this](const juce::String& paramID) -> float
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
            return p->convertFrom0to1(p->getValue());
        return 0.0f;
    };

    auto makeItem = [](int id, const juce::String& label, bool tick, std::function<void()> action)
    {
        juce::PopupMenu::Item it { label };
        it.itemID = id;
        it.setEnabled(true);
        it.setTicked(tick);
        it.action = std::move(action);
        return it;
    };

    // MS Mode
    {
        juce::PopupMenu msMenu;
        const int current = getChoice("msMode");
        msMenu.addItem(makeItem(1, "Stereo", current == 0, [=]() { setChoice("msMode", 0); }));
        msMenu.addItem(makeItem(2, "Mid Only", current == 1, [=]() { setChoice("msMode", 1); }));
        msMenu.addItem(makeItem(3, "Side Only", current == 2, [=]() { setChoice("msMode", 2); }));
        msMenu.addItem(makeItem(4, "M/S Linked", current == 3, [=]() { setChoice("msMode", 3); }));
        menu.addSubMenu("M/S Mode", msMenu);
    }

    // AI toggle
    {
        const bool enabled = getBool("aiEnabled");
        menu.addItem(makeItem(10, "AI Enabled", enabled, [=]() { setBool("aiEnabled", !enabled); }));
    }

    // Source Profile
    {
        juce::PopupMenu profile;
        static const juce::StringArray profiles { "Generic", "Vocals", "Drums", "Bass", "Synth", "Master", "EDM" };
        const int current = getChoice("sourceProfile");
        for (int i = 0; i < profiles.size(); ++i)
            profile.addItem(makeItem(20 + i, profiles[i], current == i, [=]() { setChoice("sourceProfile", i); }));
        menu.addSubMenu("Source Profile", profile);
    }

    // Analyzer settings
    {
        juce::PopupMenu analyzer;
        // Resolution
        {
            juce::PopupMenu res;
            const int cur = getChoice("analyzerResolution");
            res.addItem(makeItem(40, "Low (1024)", cur == 0, [=]() { setChoice("analyzerResolution", 0); }));
            res.addItem(makeItem(41, "Medium (2048)", cur == 1, [=]() { setChoice("analyzerResolution", 1); }));
            res.addItem(makeItem(42, "High (4096)", cur == 2, [=]() { setChoice("analyzerResolution", 2); }));
            res.addItem(makeItem(43, "Max (8192)", cur == 3, [=]() { setChoice("analyzerResolution", 3); }));
            analyzer.addSubMenu("Resolution", res);
        }
        // Speed
        {
            juce::PopupMenu spd;
            const int cur = getChoice("analyzerSpeed");
            spd.addItem(makeItem(50, "Fast", cur == 0, [=]() { setChoice("analyzerSpeed", 0); }));
            spd.addItem(makeItem(51, "Medium", cur == 1, [=]() { setChoice("analyzerSpeed", 1); }));
            spd.addItem(makeItem(52, "Slow", cur == 2, [=]() { setChoice("analyzerSpeed", 2); }));
            analyzer.addSubMenu("Speed", spd);
        }
        // Peak hold
        {
            juce::PopupMenu hold;
            const float cur = getFloat("analyzerPeakHold");
            auto addHold = [&](int id, const juce::String& label, float v)
            {
                hold.addItem(makeItem(id, label, std::abs(cur - v) < 0.05f, [=]() { setFloat("analyzerPeakHold", v); }));
            };
            addHold(60, "Off", 0.0f);
            addHold(61, "1.0 s", 1.0f);
            addHold(62, "2.0 s", 2.0f);
            addHold(63, "5.0 s", 5.0f);
            analyzer.addSubMenu("Peak Hold", hold);
        }
        // Peak decay
        {
            juce::PopupMenu dec;
            const float cur = getFloat("analyzerPeakDecay");
            auto addDec = [&](int id, const juce::String& label, float v)
            {
                dec.addItem(makeItem(id, label, std::abs(cur - v) < 0.05f, [=]() { setFloat("analyzerPeakDecay", v); }));
            };
            addDec(70, "10 dB/s", 10.0f);
            addDec(71, "20 dB/s", 20.0f);
            addDec(72, "40 dB/s", 40.0f);
            addDec(73, "60 dB/s", 60.0f);
            analyzer.addSubMenu("Peak Decay", dec);
        }
        menu.addSubMenu("Analyzer", analyzer);
    }

    // UI toggles
    {
        const bool pianoRoll = getBool("pianoRollOverlay");
        menu.addItem(makeItem(80, "Piano Roll Overlay", pianoRoll, [=]() { setBool("pianoRollOverlay", !pianoRoll); }));

        const bool hc = getBool("highContrastMode");
        menu.addItem(makeItem(81, "High Contrast Mode", hc, [=]() { setBool("highContrastMode", !hc); }));
    }

    // Learning
    {
        const bool learning = getBool("learningEnabled");
        menu.addItem(makeItem(90, "User Learning Enabled", learning, [=]() { setBool("learningEnabled", !learning); }));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&optionsBtn));
}

void AIEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(ModernLookAndFeel::Colors::bgMid);
    
    // Header
    g.setColour(ModernLookAndFeel::Colors::bgLight);
    g.fillRect(0, 0, getWidth(), headerH);
    g.setColour(ModernLookAndFeel::Colors::bgLighter);
    g.drawHorizontalLine(headerH - 1, 0, (float)getWidth());
    
    // Control panel
    int cpY = getHeight() - controlH;
    g.setColour(ModernLookAndFeel::Colors::bgPanel);
    g.fillRect(0, cpY, getWidth(), controlH);
    g.setColour(ModernLookAndFeel::Colors::bgLighter);
    g.drawHorizontalLine(cpY, 0, (float)getWidth());

    // Dividers (gradient fade)
    for (int x : dividerPositions)
    {
        juce::ColourGradient grad(
            juce::Colours::transparentBlack, (float)x, (float)(cpY + 12),
            juce::Colour(0xFF3a3a42),         (float)x, (float)(cpY + controlH / 2),
            false);
        grad.addColour(0.5, juce::Colour(0xFF3a3a42));
        grad.addColour(1.0, juce::Colours::transparentBlack);
        g.setGradientFill(grad);
        g.fillRect(x, cpY + 12, 1, controlH - 24);
    }
}

void AIEqualizerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // === HEADER ===
    auto header = bounds.removeFromTop(headerH).reduced(pad, 5);
    
    prevBtn.setBounds(header.removeFromLeft(28).reduced(2));
    nextBtn.setBounds(header.removeFromLeft(28).reduced(2));
    header.removeFromLeft(8);
    presetBox.setBounds(header.removeFromLeft(140).reduced(0, 2));
    header.removeFromLeft(10);
    optionsBtn.setBounds(header.removeFromLeft(80).reduced(2));
    header.removeFromLeft(10);
    
    // A/B
    btnA.setBounds(header.removeFromLeft(28).reduced(2));
    btnB.setBounds(header.removeFromLeft(28).reduced(2));
    header.removeFromLeft(4);
    copyBtn.setBounds(header.removeFromLeft(40).reduced(2));
    header.removeFromLeft(14);
    
    auto phaseLabelArea = header.removeFromLeft(88);
    phaseModeLabel.setBounds(phaseLabelArea.reduced(2, 6));
    
    auto phaseComboArea = header.removeFromLeft(150);
    phaseModeCombo.setBounds(phaseComboArea.reduced(2));
    
    header.removeFromLeft(10);
    
    // Pre/Post
    btnPre.setBounds(header.removeFromLeft(40).reduced(2));
    header.removeFromLeft(4);
    btnPost.setBounds(header.removeFromLeft(45).reduced(2));
    header.removeFromLeft(4);
    btnDelta.setBounds(header.removeFromLeft(55).reduced(2));
    
    // Bypass (right)
    bypassBtn.setBounds(header.removeFromRight(70).reduced(2));
    
    // === RIGHT SIDE PANELS ===
    auto rightSide = bounds.removeFromRight(aiPanelW).reduced(pad);
    
    // Tab buttons at top of right side
    auto tabRow = rightSide.removeFromTop(28);
    int tabW = tabRow.getWidth() / 2;
    aiTabBtn.setBounds(tabRow.removeFromLeft(tabW).reduced(2));
    semanticTabBtn.setBounds(tabRow.reduced(2));
    rightSide.removeFromTop(4);
    
    // Main panel area (for AI Detect or Semantic panel)
    int mainPanelHeight = static_cast<int>(rightSide.getHeight() * 0.65f);
    auto mainPanel = rightSide.removeFromTop(mainPanelHeight);
    aiProblemPanel->setBounds(mainPanel);
    semanticPanel->setBounds(mainPanel);  // Same bounds, visibility controlled by tabs
    
    rightSide.removeFromTop(pad);
    
    // Dynamic EQ Panel (smaller portion - 35%)
    dynamicEQPanel->setBounds(rightSide);
    
    // === CONTROL PANEL (bottom) — premium, 140px high ===
    auto cpanel = bounds.removeFromBottom(controlH).reduced(pad, 4);
    dividerPositions.clear();

    // Section widths
    constexpr int logoW = 65;
    constexpr int captureW = 200;   // with waveform
    constexpr int aiW = 110;
    constexpr int qualityW = 130;   // includes num bands
    constexpr int bandsW = 150;
    constexpr int outputW = 110;
    constexpr int dynMasterW = 85;
    constexpr int gapW = 6;

    // LOGO
    auto logoSection = cpanel.removeFromLeft(logoW);
    logoLabel.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
    logoLabel.setBounds(logoSection.removeFromTop(20).reduced(2));
    subtitleLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    subtitleLabel.setBounds(logoSection.removeFromTop(14).reduced(2, 0));

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromLeft(gapW);

    // CAPTURE with waveform
    auto captureSection = cpanel.removeFromLeft(captureW);
    {
        auto leftCol = captureSection.removeFromLeft(110);

        auto row1 = leftCol.removeFromTop(22);
        captureLenLabel.setBounds(row1.removeFromLeft(48));
        captureLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
        captureLenSlider.setBounds(row1);

        leftCol.removeFromTop(2);
        auto row2 = leftCol.removeFromTop(24);
        int btnW = row2.getWidth() / 2;
        captureAnalyzeBtn.setBounds(row2.removeFromLeft(btnW).reduced(1));
        autoCaptureBtn.setBounds(row2.reduced(1));

        leftCol.removeFromTop(2);
        auto row3 = leftCol.removeFromTop(24);
        btnW = row3.getWidth() / 2;
        startCaptureBtn.setBounds(row3.removeFromLeft(btnW).reduced(1));
        stopCaptureBtn.setBounds(row3.reduced(1));

        leftCol.removeFromTop(2);
        captureStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        captureStatusLabel.setBounds(leftCol.removeFromTop(16));

        captureSection.removeFromLeft(6);
        if (captureWaveform)
        {
            captureWaveform->setVisible(true);
            captureWaveform->setBounds(captureSection.reduced(2));
        }
    }

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromLeft(gapW);

    // AI (premium knobs)
    auto aiSection = cpanel.removeFromLeft(aiW);
    {
        const int knobW = 46;
        auto row = aiSection.withTrimmedTop(6).withTrimmedBottom(6);
        int startX = (row.getWidth() - knobW * 2 - 6) / 2;
        row.removeFromLeft(startX);
        auto sensArea = row.removeFromLeft(knobW);
        sensitivityKnob.setBounds(sensArea);
        row.removeFromLeft(6);
        auto strArea = row.removeFromLeft(knobW);
        strengthKnob.setBounds(strArea);
    }

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromLeft(gapW);

    // QUALITY (HQ, oversampling, bands, auto gain, slope)
    auto qualitySection = cpanel.removeFromLeft(qualityW);
    {
        auto row1 = qualitySection.removeFromTop(26);
        qualityLabel.setBounds(row1.removeFromLeft(55));
        qualityBtn.setBounds(row1.reduced(2));

        qualitySection.removeFromTop(2);
        auto row2 = qualitySection.removeFromTop(24);
        oversamplingLabel.setBounds(row2.removeFromLeft(55));
        oversamplingCombo.setBounds(row2.reduced(2));

        qualitySection.removeFromTop(2);
        auto row3 = qualitySection.removeFromTop(24);
        numBandsLabel.setText("BANDS", juce::dontSendNotification);
        numBandsLabel.setBounds(row3.removeFromLeft(55));
        numBandsCombo.setVisible(true);
        numBandsCombo.setBounds(row3.reduced(2));

        qualitySection.removeFromTop(2);
        auto row4 = qualitySection.removeFromTop(22);
        autoBtn.setBounds(row4.reduced(4, 0));

        qualitySection.removeFromTop(2);
        auto row5 = qualitySection.removeFromTop(24);
        slopeLabel.setBounds(row5.removeFromLeft(55));
        slopeCombo.setBounds(row5.reduced(2));
    }

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromLeft(gapW);

    // BANDS toggles (grid for all bands)
    auto bandsSection = cpanel.removeFromLeft(bandsW);
    int btnW = 32;
    const int btnH = 26;
    const int comboH = 24;
    const int cols = 8;
    const int rows = (AIEqualizerAudioProcessor::maxBands + cols - 1) / cols;
    int spacing = 2;
    int totalGridH = rows * btnH + (rows - 1) * 4;
    int topPad = (bandsSection.getHeight() - (totalGridH + comboH + 4)) / 2;
    topPad = std::max(0, topPad);
    bandsSection.removeFromTop(topPad);
    bandSelectCombo.setBounds(bandsSection.removeFromTop(comboH).reduced(4, 0));
    bandsSection.removeFromTop(4);

    // If space is tight, scale buttons and spacing to avoid overlap.
    {
        const int availableW = bandsSection.getWidth();
        int totalW = cols * btnW + (cols - 1) * spacing;
        if (totalW > availableW)
        {
            const float scale = static_cast<float>(availableW) / static_cast<float>(totalW);
            btnW = std::max(22, static_cast<int>(std::floor(btnW * scale)));
            spacing = std::max(1, static_cast<int>(std::floor(spacing * scale)));
            totalW = cols * btnW + (cols - 1) * spacing;
        }
    }

    int startX = (bandsSection.getWidth() - (cols * btnW + (cols - 1) * spacing)) / 2;
    startX = std::max(0, startX);
    for (int r = 0; r < rows; ++r)
    {
        auto rowArea = bandsSection.removeFromTop(btnH);
        rowArea.removeFromLeft(startX);
        for (int c = 0; c < cols; ++c)
        {
            int idx = r * cols + c;
            if (idx >= static_cast<int>(bandToggles.size()))
                break;
            if (bandToggles[(size_t)idx])
                bandToggles[(size_t)idx]->setBounds(rowArea.removeFromLeft(btnW).reduced(1));
            rowArea.removeFromLeft(spacing);
        }
        if (r < rows - 1)
            bandsSection.removeFromTop(4);
    }

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromLeft(gapW);

    // OUTPUT knobs (premium)
    auto outputSection = cpanel.removeFromRight(outputW);
    {
        const int knobW = 46;
        auto row = outputSection.withTrimmedTop(6).withTrimmedBottom(6);
        mixKnob.setBounds(row.removeFromLeft(knobW));
        row.removeFromLeft(6);
        outKnob.setBounds(row.removeFromLeft(knobW));
    }

    dividerPositions.push_back(cpanel.getX());
    cpanel.removeFromRight(gapW);

    // Dyn master at right of band params
    auto dynArea = cpanel.removeFromRight(dynMasterW);
    dynamicEQMasterPanel->setBounds(dynArea.reduced(2));

    // Band detail fills remaining
    auto bandDetailArea = cpanel;
    bandViewport->setBounds(bandDetailArea);
    if (selectedBandPanel)
    {
        selectedBandPanel->setBounds(bandDetailArea);
        selectedBandPanel->toFront(false);
    }
    
    // === SPECTRUM (main area) ===
    bounds.reduce(pad, pad);
    spectrumBounds = bounds;
    spectrum->setBounds(spectrumBounds);
    
    graphBounds = spectrumBounds.reduced(45, 25);
    graphBounds.removeFromBottom(22);
    graphBounds.removeFromLeft(5);
    
    updateBandPositions();
}

void AIEqualizerAudioProcessorEditor::timerCallback()
{
    // Drain RT-safe logger queue on message thread
    AIEQLogger::getInstance().flushRTLogs();

    // Spectrum - process only when audio flagged new data
    if (processor.consumeSpectrumDataReady())
    {
        processor.getSpectrumAnalyzer().processFFT();
        if (processor.getPostEQAnalyzer().hasNewData())
            processor.getPostEQAnalyzer().processFFT();
        if (spectrum)
            spectrum->repaint();
    }

    // AI problems update
    if (processor.consumeAIProblemsChanged())
    {
        if (aiProblemPanel)
            aiProblemPanel->refreshFromProcessor();
    }

    // Diagnostics: log block clamp events when they change (message thread safe)
    auto clampEvents = processor.getBlockClampEvents();
    if (clampEvents != lastBlockClampEvents)
    {
        lastBlockClampEvents = clampEvents;
        juce::Logger::outputDebugString("AI Equalizer Pro - block clamp events: " + juce::String((int)clampEvents));
    }

    // Parameter-driven UI updates (bands, toggles, A/B, quality)
    uint64_t currentChange = processor.getParameterChangeCounter();
    if (currentChange != lastParameterChangeCount)
    {
        lastParameterChangeCount = currentChange;

        if (bandViewport)
            bandViewport->setNumBands(processor.getNumActiveBands());

        int activeBands = processor.getNumActiveBands();
        if (activeBands > 0 && selectedBand >= activeBands)
            selectBand(activeBands - 1);
        const int toggleCount = static_cast<int>(bandToggles.size());
        for (int i = 0; i < toggleCount; ++i)
        {
            bool shouldShow = (i < activeBands);
            if (bandToggles[i])
            {
                bandToggles[i]->setEnabled(shouldShow);
                bandToggles[i]->setAlpha(shouldShow ? 1.0f : 0.4f);
            }
        }

        const int bandsToSync = std::min(static_cast<int>(bands.size()), processor.getNumActiveBands());
        for (int i = 0; i < bandsToSync; ++i)
        {
            auto state = processor.getBandState(i);
            EQBandControl::BandParameters p;
            p.frequency = state.frequency;
            p.gain = state.gain;
            p.q = state.q;
            p.filterType = state.type;
            p.enabled = state.enabled;
            bands[i]->setParameters(p);
            
            if (i < toggleCount && bandToggles[i])
                bandToggles[i]->setToggleState(state.enabled, juce::dontSendNotification);
        }
        updateBandPositions();

        bool isA = processor.getCurrentABState() == AIEqualizerAudioProcessor::ABState::A;
        btnA.setToggleState(isA, juce::dontSendNotification);
        btnB.setToggleState(!isA, juce::dontSendNotification);

        // Sync quality toggle (0 = Zero Latency, 1 = High Quality)
        if (auto* param = processor.getAPVTS().getParameter("qualityMode"))
        {
            float v = param->getValue();
            int mode = static_cast<int>(param->convertFrom0to1(v) + 0.5f);
            qualityBtn.setToggleState(mode == 1, juce::dontSendNotification);
            qualityBtn.setButtonText(mode == 1 ? "HQ" : "ZL");
            qualityBtn.setColour(juce::TextButton::buttonColourId,
                                 mode == 1 ? ModernLookAndFeel::Colors::accentBlue.withAlpha(0.18f)
                                           : ModernLookAndFeel::Colors::bgLight);
            qualityBtn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::accentBlue);
            qualityBtn.setColour(juce::TextButton::textColourOffId,
                                 mode == 1 ? ModernLookAndFeel::Colors::textBright
                                           : ModernLookAndFeel::Colors::textPrimary);
            qualityBtn.setColour(juce::TextButton::textColourOnId, ModernLookAndFeel::Colors::textBright);
        }

        // Sync source profile combo with parameter (covers preset load/automation)
        if (auto* param = processor.getAPVTS().getParameter("sourceProfile"))
        {
            const int idx = static_cast<int>(param->convertFrom0to1(param->getValue()) + 0.5f);
            const int desiredId = idx + 1;
            if (presetBox.getSelectedId() != desiredId)
                presetBox.setSelectedId(desiredId, juce::dontSendNotification);
        }
    }

    // Sync capture state
    if (processor.isCapturing())
    {
        if (stopCaptureBtn.isEnabled() == false)
        {
            stopCaptureBtn.setEnabled(true);
            startCaptureBtn.setEnabled(false);
        }
        captureStatusLabel.setText("Recording...", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    }
    else
    {
        if (stopCaptureBtn.isEnabled() == true)
        {
            stopCaptureBtn.setEnabled(false);
            startCaptureBtn.setEnabled(true);
        }
    }
    
    // Update capture waveform preview
    {
        const bool recording = processor.isCapturing();
        const bool captureSafe = processor.isCaptureBufferSafeToRead();
        const auto& lastCapture = processor.getCapturedAudioMono();
        std::vector<float> preview;
        if (recording)
            processor.getManualCapturePreview(preview, 512);
        static const std::vector<float> emptyCapture;
        const std::vector<float>* source = nullptr;
        if (recording)
            source = &preview;
        else if (captureSafe)
            source = &lastCapture;
        else
            source = &emptyCapture;
        
        if (captureWaveform)
            captureWaveform->setData(*source,
                                     processor.getCapturedSampleRate(),
                                     recording);
    }
    
    // Sync auto-capture toggle
    autoCaptureBtn.setToggleState(processor.isAutoCaptureEnabled(), juce::dontSendNotification);
    
    // Sync high-contrast mode
    bool hcMode = processor.getAPVTS().getRawParameterValue("highContrastMode")->load() > 0.5f;
    if (lookAndFeel.isHighContrastMode() != hcMode)
    {
        lookAndFeel.setHighContrastMode(hcMode);
        lookAndFeel.updateHighContrastColors();
        repaint();
    }
}

void AIEqualizerAudioProcessorEditor::updateBandPositions()
{
    if (graphBounds.isEmpty()) return;
    
    const int bandsToPlace = std::min(static_cast<int>(bands.size()), processor.getNumActiveBands());
    for (int i = 0; i < bandsToPlace; ++i)
    {
        const auto& p = bands[i]->getParameters();
        float x = freqToX(p.frequency);
        float y = gainToY(p.gain);
        
        x = juce::jlimit((float)graphBounds.getX(), (float)graphBounds.getRight(), x);
        y = juce::jlimit((float)graphBounds.getY(), (float)graphBounds.getBottom(), y);
        
        int sz = p.enabled ? 26 : 14;
        bands[i]->setBounds((int)x - sz/2, (int)y - sz/2, sz, sz);
    }
}

void AIEqualizerAudioProcessorEditor::onBandChanged(int idx, const EQBandControl::BandParameters& p)
{
    AIEqualizerAudioProcessor::BandState s;
    s.frequency = p.frequency;
    s.gain = p.gain;
    s.q = p.q;
    s.type = p.filterType;
    s.enabled = p.enabled;
    processor.setBandState(idx, s);
    
    // Update selected band if this is a different band
    if (idx != selectedBand)
    {
        selectBand(idx);
    }
    
    updateBandPositions();
}

void AIEqualizerAudioProcessorEditor::selectBand(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= AIEqualizerAudioProcessor::maxBands)
        return;
    
    selectedBand = bandIndex;
    
    // Update spectrum display
    if (spectrum)
        spectrum->setSelectedBand(bandIndex);
    
    // FIX 2: reuse band control panel instead of recreating it
    if (selectedBandPanel)
        selectedBandPanel->setBandIndex(bandIndex);
    
    // Update band selector combo
    if (bandSelectCombo.getSelectedId() != bandIndex + 1)
        bandSelectCombo.setSelectedId(bandIndex + 1, juce::dontSendNotification);
    
    // Update Dynamic EQ panel for this band
    if (dynamicEQPanel)
        dynamicEQPanel->setBandIndex(bandIndex);
    
    // Update band toggle highlight
    for (size_t i = 0; i < bandToggles.size(); ++i)
    {
        if (!bandToggles[i])
            continue;
        if (static_cast<int>(i) == bandIndex)
            bandToggles[i]->setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::accentBlue);
        else
            bandToggles[i]->setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textPrimary);
    }
    
    resized();
    repaint();
}

void AIEqualizerAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    // Band creation/selection is now handled directly by AdvancedSpectrumDisplay
    // This method is kept for potential future use outside the spectrum area
}

float AIEqualizerAudioProcessorEditor::freqToX(float f)
{
    float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
    float p = (std::log10(juce::jlimit(20.0f, 20000.0f, f)) - logMin) / (logMax - logMin);
    return graphBounds.getX() + p * graphBounds.getWidth();
}

float AIEqualizerAudioProcessorEditor::gainToY(float g)
{
    float p = juce::jmap(g, -24.0f, 24.0f, 1.0f, 0.0f);
    return graphBounds.getY() + p * graphBounds.getHeight();
}

float AIEqualizerAudioProcessorEditor::xToFreq(float x)
{
    float p = juce::jlimit(0.0f, 1.0f, (x - graphBounds.getX()) / graphBounds.getWidth());
    float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
    return std::pow(10.0f, logMin + p * (logMax - logMin));
}

float AIEqualizerAudioProcessorEditor::yToGain(float y)
{
    float p = (y - graphBounds.getY()) / graphBounds.getHeight();
    return juce::jmap(p, 0.0f, 1.0f, 24.0f, -24.0f);
}

void AIEqualizerAudioProcessorEditor::switchRightTab(int tab)
{
    activeRightTab = tab;
    
    // Update visibility
    aiProblemPanel->setVisible(tab == 0);
    semanticPanel->setVisible(tab == 1);
    
    // Update button styles
    if (tab == 0)
    {
        aiTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4A9FD9));
        aiTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        semanticTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        semanticTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
    }
    else
    {
        semanticTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE6A23C));
        semanticTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        aiTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF333333));
        aiTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFAAAAAA));
    }
    
    repaint();
}
