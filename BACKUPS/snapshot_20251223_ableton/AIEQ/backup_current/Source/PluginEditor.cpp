#include "PluginEditor.h"
#include "GUI/BandViewport.h"

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
        // Apply semantic EQ adjustments to processor bands
        // Use first available bands for the adjustments
        int bandIndex = 0;
        for (const auto& adj : adjustments)
        {
            if (bandIndex >= AIEqualizerAudioProcessor::maxBands) break;
            if (std::abs(adj.gain) < 0.1f) continue;  // Skip negligible adjustments
            
            AIEqualizerAudioProcessor::BandState state;
            state.frequency = adj.frequency;
            state.gain = adj.gain;
            state.q = adj.q;
            state.type = adj.filterType;
            state.enabled = true;
            processor.setBandState(bandIndex, state);
            bandIndex++;
        }
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
    
    // Band viewport (scrollable panels)
    bandViewport = std::make_unique<BandViewport>(processor.getAPVTS(), AIEqualizerAudioProcessor::maxBands);
    bandViewport->setNumBands(processor.getNumActiveBands());
    addAndMakeVisible(*bandViewport);
    
    // Dynamic EQ Panel (per-band controls)
    dynamicEQPanel = std::make_unique<DynamicEQPanel>(processor.getAPVTS(), 0);
    dynamicEQPanel->setDynamicProcessor(&processor.getDynamicEQProcessor());
    addAndMakeVisible(*dynamicEQPanel);
    
    // Dynamic EQ Master Panel (global controls)
    dynamicEQMasterPanel = std::make_unique<DynamicEQMasterPanel>(processor.getAPVTS());
    dynamicEQMasterPanel->setDynamicProcessor(&processor.getDynamicEQProcessor());
    addAndMakeVisible(*dynamicEQMasterPanel);
    
    setSize(1200, 750);
    setResizable(true, true);
    setResizeLimits(1100, 680, 1800, 1100);
    
    // Ensure a band is selected so the detail panel shows controls (including filter type)
    selectBand(0);
    
    startTimerHz(60);
}

AIEqualizerAudioProcessorEditor::~AIEqualizerAudioProcessorEditor()
{
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
    presetBox.addItem("Default", 1);
    presetBox.addItem("Vocals", 2);
    presetBox.addItem("Drums", 3);
    presetBox.addItem("Bass", 4);
    presetBox.addItem("Master", 5);
    presetBox.addItem("EDM", 6);
    presetBox.addItem("Techno", 7);
    presetBox.setSelectedId(1);
    presetBox.setTooltip("Select source profile - AI adapts detection thresholds accordingly");
    presetBox.onChange = [this]() {
        int id = presetBox.getSelectedId();
        if (id > 0) processor.setSourceProfile(static_cast<AIEngine::SourceProfile>(id - 1));
    };
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
    const char* labels[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII"};
    for (int i = 0; i < 8; ++i)
    {
        bandToggles[i].setButtonText(labels[i]);
        bandToggles[i].setToggleState(true, juce::dontSendNotification);
        bandToggles[i].setTooltip("Click to select Band " + juce::String(i + 1) + "\nToggle checkbox to enable/disable");
        bandToggles[i].onClick = [this, i]() {
            // Toggle enables/disables the band
            auto state = processor.getBandState(i);
            state.enabled = bandToggles[i].getToggleState();
            processor.setBandState(i, state);
            
            // Also select this band
            selectBand(i);
        };
        addAndMakeVisible(bandToggles[i]);
    }
    
    // Sensitivity knob (connected to APVTS for save/restore)
    sensitivityLabel.setText("SENSITIVITY", juce::dontSendNotification);
    sensitivityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    sensitivityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(sensitivityLabel);
    
    sensitivityKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sensitivityKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
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
    
    strengthKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    strengthKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
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

    // Capture & analyze button (retroactive: records last N seconds and runs AI analysis)
    captureAnalyzeBtn.setTooltip("Capture last N seconds (retroactive) and analyze");
    captureAnalyzeBtn.onClick = [this]() {
        processor.captureAudioSnapshotMs(processor.getCaptureLengthMs());
        const bool ok = processor.analyzeCapturedAudioSnapshot();
        captureStatusLabel.setText(ok ? "Captured/Analyzed" : "No audio captured", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId,
                                     ok ? juce::Colours::limegreen : juce::Colours::orange);
    };
    addAndMakeVisible(captureAnalyzeBtn);
    
    // Manual capture START button
    startCaptureBtn.setTooltip("Start manual capture - records from now until STOP");
    startCaptureBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D5A27));
    startCaptureBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    startCaptureBtn.onClick = [this]() {
        if (processor.startManualCapture())
        {
            captureStatusLabel.setText("Recording...", juce::dontSendNotification);
            captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
            startCaptureBtn.setEnabled(false);
            stopCaptureBtn.setEnabled(true);
        }
    };
    addAndMakeVisible(startCaptureBtn);
    
    // Manual capture STOP button
    stopCaptureBtn.setTooltip("Stop manual capture and analyze");
    stopCaptureBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5A2D2D));
    stopCaptureBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    stopCaptureBtn.setEnabled(false);
    stopCaptureBtn.onClick = [this]() {
        processor.stopManualCapture();
        const bool ok = processor.analyzeCapturedAudioSnapshot();
        captureStatusLabel.setText(ok ? "Captured/Analyzed" : "No audio captured", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId,
                                     ok ? juce::Colours::limegreen : juce::Colours::orange);
        startCaptureBtn.setEnabled(true);
        stopCaptureBtn.setEnabled(false);
    };
    addAndMakeVisible(stopCaptureBtn);
    
    // Auto-capture toggle (triggers on energy peak)
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
    
    captureStatusLabel.setText("", juce::dontSendNotification);
    captureStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    captureStatusLabel.setJustificationType(juce::Justification::centredLeft);
    captureStatusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(captureStatusLabel);

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
    
    outKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    outKnob.setTooltip("Output Gain - Adjust overall output level (-24dB to +24dB)");
    addAndMakeVisible(outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outKnob);
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
    header.removeFromLeft(20);
    
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
    
    // === CONTROL PANEL (bottom) ===
    auto cpanel = bounds.removeFromBottom(controlH).reduced(pad);

    // Reserve rightmost areas first to avoid being squeezed away
    const int knobW = 65;
    const int knobH = 80;
    const int outW = 90;
    const int dynMasterW = 90;
    const int bandDetailW = 360;
    const int captureW = 240;
    const int togglesW = 220;
    const int numBandsW = 55;
    const int logoW = 100;

    // Output (right side)
    auto outArea = cpanel.removeFromRight(outW);
    outLabel.setBounds(outArea.removeFromTop(14));
    outKnob.setBounds(outArea.removeFromTop(knobH));

    // Dynamic EQ Master Panel
    auto dynMasterArea = cpanel.removeFromRight(dynMasterW);
    dynamicEQMasterPanel->setBounds(dynMasterArea);

    // Selected band panel
    auto bandDetailArea = cpanel.removeFromRight(bandDetailW);
    bandViewport->setBounds(bandDetailArea);
    if (selectedBandPanel)
    {
        selectedBandPanel->setBounds(bandDetailArea);
        selectedBandPanel->toFront(false);
    }

    // Capture controls (expanded area)
    auto captureArea = cpanel.removeFromLeft(240);
    auto capLabelArea = captureArea.removeFromTop(14);
    captureLenLabel.setBounds(capLabelArea);
    auto capSliderArea = captureArea.removeFromTop(20);
    captureLenSlider.setBounds(capSliderArea.reduced(2));
    
    // Button row: Retroactive capture + Auto-capture toggle
    auto btnRow1 = captureArea.removeFromTop(26);
    captureAnalyzeBtn.setBounds(btnRow1.removeFromLeft(140).reduced(2));
    autoCaptureBtn.setBounds(btnRow1.reduced(2));
    
    // Button row: Manual START/STOP
    auto btnRow2 = captureArea.removeFromTop(26);
    startCaptureBtn.setBounds(btnRow2.removeFromLeft(110).reduced(2));
    stopCaptureBtn.setBounds(btnRow2.reduced(2));
    
    captureStatusLabel.setBounds(captureArea);

    cpanel.removeFromLeft(8);

    // AI Knobs
    auto sensArea = cpanel.removeFromLeft(knobW);
    sensitivityLabel.setBounds(sensArea.removeFromTop(14));
    sensitivityKnob.setBounds(sensArea.removeFromTop(knobH));

    auto strArea = cpanel.removeFromLeft(knobW);
    strengthLabel.setBounds(strArea.removeFromTop(14));
    strengthKnob.setBounds(strArea.removeFromTop(knobH));

    cpanel.removeFromLeft(8);
    autoBtn.setBounds(cpanel.removeFromLeft(45).removeFromTop(26).reduced(2));
    auto qualityArea = cpanel.removeFromLeft(70);
    qualityLabel.setBounds(qualityArea.removeFromTop(14));
    qualityBtn.setBounds(qualityArea.removeFromTop(26).reduced(2));

    cpanel.removeFromLeft(10);

    // Band toggles
    auto toggleArea = cpanel.removeFromLeft(togglesW);
    auto toggleRow = toggleArea.removeFromTop(26);
    for (int i = 0; i < 4; ++i)
        bandToggles[i].setBounds(toggleRow.removeFromLeft(52).reduced(2));
    toggleRow = toggleArea.removeFromTop(26);
    for (int i = 4; i < 8; ++i)
        bandToggles[i].setBounds(toggleRow.removeFromLeft(52).reduced(2));

    cpanel.removeFromLeft(8);

    // Number of bands selector
    auto numBandsArea = cpanel.removeFromLeft(numBandsW);
    numBandsLabel.setBounds(numBandsArea.removeFromTop(14));
    numBandsCombo.setBounds(numBandsArea.removeFromTop(24).reduced(2));

    cpanel.removeFromLeft(10);

    // Logo section
    auto logoArea = cpanel.removeFromLeft(logoW);
    logoLabel.setBounds(logoArea.removeFromTop(28));
    subtitleLabel.setBounds(logoArea.removeFromTop(16));
    
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
    // Sync active bands for viewport
    if (bandViewport)
        bandViewport->setNumBands(processor.getNumActiveBands());

    // Sync band toggles visibility based on active bands
    int activeBands = processor.getNumActiveBands();
    if (activeBands > 0 && selectedBand >= activeBands)
        selectBand(activeBands - 1);
    for (int i = 0; i < 8; ++i)
    {
        bool shouldShow = (i < activeBands);
        bandToggles[i].setEnabled(shouldShow);
        bandToggles[i].setAlpha(shouldShow ? 1.0f : 0.4f);
    }
    
    // Update nodes positions and states
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
    }
    updateBandPositions();
    
    // A/B state
    bool isA = processor.getCurrentABState() == AIEqualizerAudioProcessor::ABState::A;
    btnA.setToggleState(isA, juce::dontSendNotification);
    btnB.setToggleState(!isA, juce::dontSendNotification);

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
    
    // Sync auto-capture toggle
    autoCaptureBtn.setToggleState(processor.isAutoCaptureEnabled(), juce::dontSendNotification);
    
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
    
    // Recreate band control panel for new selection
    selectedBandPanel = std::make_unique<BandControlPanel>(bandIndex, processor.getAPVTS());
    addAndMakeVisible(*selectedBandPanel);
    
    // Update Dynamic EQ panel for this band
    if (dynamicEQPanel)
        dynamicEQPanel->setBandIndex(bandIndex);
    
    // Update band toggle highlight
    for (int i = 0; i < 8; ++i)
    {
        if (i == bandIndex)
        {
            bandToggles[i].setColour(juce::ToggleButton::textColourId, 
                                     ModernLookAndFeel::Colors::accentBlue);
        }
        else
        {
            bandToggles[i].setColour(juce::ToggleButton::textColourId, 
                                     ModernLookAndFeel::Colors::textPrimary);
        }
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
