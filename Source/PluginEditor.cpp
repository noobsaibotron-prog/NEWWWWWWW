#include "PluginEditor.h"
#include "GUI/BandViewport.h"
#include "Utils/Logger.h"
#include <thread>

AIEqualizerAudioProcessorEditor::AIEqualizerAudioProcessorEditor(AIEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);

    // Attach OpenGL context to this top-level component.
    // All child component paint() calls are composited via GPU automatically.
    // setContinuousRepainting(false): we drive repaints via our own Timer.
    // setRenderer(this): enables renderOpenGL() for the metrological spectrum pipeline.
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.setContinuousRepainting(false);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);

    // Metrological 5-layer spectrum pipeline
    glSpectrumHelper = std::make_unique<GLSpectrumHelper>();
    spectrumPipeline = std::make_unique<NewSpectrumPipeline>(
        processor.getPreEqFifo(),
        processor.getPostEqFifo(),
        12,    // fftOrder: 2^12 = 4096 samples
        processor.getSampleRate(),
        60.0);

    createHeader();
    createControlPanel();
    createBands();
    
    spectrum = std::make_unique<AdvancedSpectrumDisplay>(processor);
    addAndMakeVisible(*spectrum);

    // Sync display speed with saved analyzer speed parameter
    {
        auto* speedParam = processor.getAPVTS().getRawParameterValue("analyzerSpeed");
        if (speedParam)
        {
            int spd = static_cast<int>(std::round(speedParam->load()));
            if (spd == 0) spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Fast);
            else if (spd == 2) spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Slow);
            else spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Medium);
        }
    }

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
    dynamicEQPanel->setDynamicParamsChangedCallback([this](int band) {
        // Follow the same code path as clicking the band node on the graph.
        // The reported bug is that touching threshold/mode does not fully wake the
        // selected band's dynamic visuals/meter until the user clicks/drags the node.
        // Reusing selectBand() keeps panel, graph, selected-band state and overlays in sync.
        selectBand(band);
        if (spectrum)
            spectrum->repaint();
        repaint();
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

    // Output level meter (stereo VU with peak hold)
    addAndMakeVisible(outputMeter);

    // Version label (bottom-right branding)
    versionLabel.setText("v2.1.1", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    versionLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    setSize(1200, 750);
    setResizable(true, true);
    setResizeLimits(1100, 680, 1800, 1100);
    
    // Ensure a band is selected so the detail panel shows controls (including filter type)
    selectBand(0);
    
    // Keep the editor timer responsive for meters, capture state and general UI.
    // Spectrum/FFT work is throttled separately inside timerCallback().
    startTimerHz(60);
}

AIEqualizerAudioProcessorEditor::~AIEqualizerAudioProcessorEditor()
{
    // Stop timer FIRST — prevents callbacks from accessing half-destroyed components
    stopTimer();

    // Join analysis thread before teardown to avoid use-after-free.
    if (analysisThread && analysisThread->joinable())
        analysisThread->join();

    // Detach GL context BEFORE destroying pipeline objects (cleanupGL is called during detach)
    openGLContext.setRenderer(nullptr);
    openGLContext.detach();

    // Destroy pipeline after GL context is gone — safe to release heap now
    glSpectrumHelper.reset();
    spectrumPipeline.reset();

    setLookAndFeel(nullptr);
}

//==============================================================================
// juce::OpenGLRenderer callbacks — called on the GL thread
//==============================================================================

void AIEqualizerAudioProcessorEditor::newOpenGLContextCreated()
{
    if (glSpectrumHelper)
        glSpectrumHelper->initGL(openGLContext);
}

void AIEqualizerAudioProcessorEditor::openGLContextClosing()
{
    if (glSpectrumHelper)
        glSpectrumHelper->cleanupGL(openGLContext);
}

void AIEqualizerAudioProcessorEditor::renderOpenGL()
{
    if (!glSpectrumHelper)
        return;

    const float scale = static_cast<float>(openGLContext.getRenderingScale());
    const int compW = getWidth();
    const int compH = getHeight();

    if (compW <= 0 || compH <= 0 || !spectrum)
        return;

    // Graph bounds in component-local coords → physical pixels (GL Y-up)
    const auto gb = spectrum->getGraphBoundsF();
    const int vpX = static_cast<int>(gb.getX()      * scale);
    const int vpH = static_cast<int>(gb.getHeight() * scale);
    const int vpW = static_cast<int>(gb.getWidth()  * scale);
    const int vpY = static_cast<int>((static_cast<float>(compH) - gb.getBottom()) * scale);

    glSpectrumHelper->renderShader(openGLContext, vpX, vpY, vpW, vpH, compW, compH);
}

//==============================================================================

void AIEqualizerAudioProcessorEditor::createHeader()
{
    // Nav buttons with tooltips
    prevBtn.setTooltip("Previous preset");
    nextBtn.setTooltip("Next preset");
    prevBtn.onClick = [this]() { navigatePreset(-1); };
    nextBtn.onClick = [this]() { navigatePreset(1); };
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);

    // Preset selector — shows factory + user presets
    rebuildPresetMenu();
    presetBox.setTooltip("Select EQ preset");
    presetBox.onChange = [this]()
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        const int idx = id - 1;
        if (idx >= 0 && idx < static_cast<int>(cachedPresetList.size()))
        {
            if (processor.hasPresetManager())
                processor.getPresetManager().loadPreset(cachedPresetList[static_cast<size_t>(idx)]);
            currentPresetIndex = idx;
        }
    };
    addAndMakeVisible(presetBox);

    // Save button for user presets
    savePresetBtn.setTooltip("Save current settings as user preset");
    savePresetBtn.onClick = [this]() { showSavePresetDialog(); };
    addAndMakeVisible(savePresetBtn);
    
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
    phaseModeLabel.setVisible(false);
    
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

    // AI Panel toggle
    aiPanelToggle.setButtonText("AI");
    aiPanelToggle.setClickingTogglesState(true);
    aiPanelToggle.setTooltip("Toggle AI panel");
    aiPanelToggle.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLighter);
    aiPanelToggle.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::accentBlue);
    aiPanelToggle.onClick = [this]() {
        aiPanelVisible = aiPanelToggle.getToggleState();
        resized();
    };
    addAndMakeVisible(aiPanelToggle);
}

void AIEqualizerAudioProcessorEditor::createControlPanel()
{
    // Logo (hidden in new layout)
    logoLabel.setText("AI EQ PRO", juce::dontSendNotification);
    logoLabel.setFont(juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold")));
    logoLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textPrimary);
    addAndMakeVisible(logoLabel);
    logoLabel.setVisible(false);
    
    subtitleLabel.setText("Intelligent Equalizer", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::accentBlue);
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setVisible(false);
    
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
    
    // Sensitivity knob (connected to APVTS for save/restore, hidden in new layout)
    sensitivityLabel.setText("SENSITIVITY", juce::dontSendNotification);
    sensitivityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    sensitivityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(sensitivityLabel);
    sensitivityLabel.setVisible(false);
    
    sensitivityKnob.setTooltip("AI Sensitivity - Higher values detect more subtle problems\nLower values only flag obvious issues");
    addAndMakeVisible(sensitivityKnob);
    sensitivityKnob.setVisible(false);
    sensitivityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiSensitivity", sensitivityKnob);
    
    // Strength knob (connected to APVTS for save/restore, hidden in new layout)
    strengthLabel.setText("STRENGTH", juce::dontSendNotification);
    strengthLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    strengthLabel.setJustificationType(juce::Justification::centred);
    strengthLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(strengthLabel);
    strengthLabel.setVisible(false);
    
    strengthKnob.setTooltip("AI Strength - Controls how aggressively corrections are applied\n100% = Full suggested correction, 50% = Half correction");
    addAndMakeVisible(strengthKnob);
    strengthKnob.setVisible(false);
    strengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiStrength", strengthKnob);
    
    // Auto with tooltip
    autoBtn.setTooltip("Auto Gain - Automatically compensates for volume changes from EQ");
    addAndMakeVisible(autoBtn);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "autoGain", autoBtn);

    // Quality mode toggle (Zero Latency / High Quality, hidden in new layout)
    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    qualityLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    qualityLabel.setJustificationType(juce::Justification::centred);
    qualityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(qualityLabel);
    qualityLabel.setVisible(false);

    qualityBtn.setTooltip("Quality Mode: HQ abilita 5ms di lookahead (piÃ¹ latenza), ZL = zero-latency");
    qualityBtn.setClickingTogglesState(true);
    qualityBtn.setComponentID("qualityToggle");
    qualityBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
    qualityBtn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::accentBlue);
    qualityBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary);
    qualityBtn.setVisible(false);
    qualityBtn.setColour(juce::TextButton::textColourOnId, ModernLookAndFeel::Colors::textBright);
    qualityBtn.setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    qualityBtn.onClick = [this]() {
        int mode = qualityBtn.getToggleState() ? 1 : 0; // 1 = High Quality, 0 = Zero Latency
        if (auto* param = processor.getAPVTS().getParameter("qualityMode"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(mode)));
    };
    addAndMakeVisible(qualityBtn);

    // Oversampling selector (Off / 2x / 4x / Auto, hidden in new layout)
    oversamplingLabel.setText("OVERSAMP", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    oversamplingLabel.setJustificationType(juce::Justification::centred);
    oversamplingLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(oversamplingLabel);
    oversamplingLabel.setVisible(false);

    oversamplingCombo.setJustificationType(juce::Justification::centredLeft);
    oversamplingCombo.setTextWhenNothingSelected("Off");
    oversamplingCombo.addItem("Off", 1);
    oversamplingCombo.addItem("2x", 2);
    oversamplingCombo.addItem("4x", 3);
    oversamplingCombo.addItem("Auto", 4);
    oversamplingCombo.setTooltip("Oversampling: Off/2x/4x or Auto (sceglie 2x/4x in base al Q e modalitÃ  HQ)");
    addAndMakeVisible(oversamplingCombo);
    oversamplingCombo.setVisible(false);
    oversamplingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "oversamplingFactor", oversamplingCombo);

    // Capture & analyze button (retroactive, hidden in new layout)
    captureAnalyzeBtn.setButtonText("CAPTURE LAST");
    captureAnalyzeBtn.setTooltip("Capture last N seconds (retroactive) and analyze");
    captureAnalyzeBtn.onClick = [this]() {
        if (isAnalyzing.exchange(true, std::memory_order_acq_rel))
            return;

        processor.captureAudioSnapshotMs(processor.getCaptureLengthMs());
        captureAnalyzeBtn.setEnabled(false);
        captureStatusLabel.setText("Analyzing...", juce::dontSendNotification);
        captureStatusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);

        // Join previous analysis thread if still around (isAnalyzing serializes, so it's done)
        if (analysisThread && analysisThread->joinable())
            analysisThread->join();

        juce::Component::SafePointer<AIEqualizerAudioProcessorEditor> safeThis(this);
        analysisThread.emplace([safeThis]() {
            auto* ed = safeThis.getComponent();
            if (ed == nullptr) return;

            const bool ok = ed->processor.analyzeCapturedAudioSnapshot();
            const float sec = ed->processor.getCaptureLengthMs() / 1000.0f;
            ed->isAnalyzing.store(false, std::memory_order_release);

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
        });
    };
    addAndMakeVisible(captureAnalyzeBtn);
    captureAnalyzeBtn.setVisible(false);
    
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
    startCaptureBtn.setVisible(false);
    
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

        // Join previous analysis thread if still around
        if (analysisThread && analysisThread->joinable())
            analysisThread->join();

        juce::Component::SafePointer<AIEqualizerAudioProcessorEditor> safeThis(this);
        analysisThread.emplace([safeThis]() {
            auto* ed = safeThis.getComponent();
            if (ed == nullptr) return;

            const bool ok = ed->processor.analyzeCapturedAudioSnapshot();
            const auto& mono = ed->processor.getCapturedAudioMono();
            const double sr = ed->processor.getCapturedSampleRate();
            const double secs = (sr > 0.0) ? static_cast<double>(mono.size()) / sr : 0.0;
            ed->isAnalyzing.store(false, std::memory_order_release);

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
        });
    };
    addAndMakeVisible(stopCaptureBtn);
    stopCaptureBtn.setVisible(false);
    
    // Auto-capture toggle (triggers on energy peak)
    autoCaptureBtn.setButtonText("AUTO PEAK");
    autoCaptureBtn.setTooltip("Auto-capture: automatically captures on energy peaks (e.g. drop start)");
    autoCaptureBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    autoCaptureBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF4A9FD9));
    autoCaptureBtn.onClick = [this]() {
        processor.setAutoCaptureEnabled(autoCaptureBtn.getToggleState());
    };
    addAndMakeVisible(autoCaptureBtn);
    autoCaptureBtn.setVisible(false);
    
    // Capture length slider (1-20s)
    captureLenLabel.setText("CAP LEN", juce::dontSendNotification);
    captureLenLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    captureLenLabel.setJustificationType(juce::Justification::centred);
    captureLenLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(captureLenLabel);
    captureLenLabel.setVisible(false);

    captureLenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    captureLenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 16);
    captureLenSlider.setRange(1.0, 20.0, 1.0);
    captureLenSlider.setValue(20.0);
    captureLenSlider.onValueChange = [this]() {
        int ms = static_cast<int>(captureLenSlider.getValue() * 1000.0);
        processor.setCaptureLengthMs(ms);
    };
    addAndMakeVisible(captureLenSlider);
    captureLenSlider.setVisible(false);
    
    captureStatusLabel.setText("Retro: CAPTURE LAST. Live: START/STOP. Auto: peaks.",
                               juce::dontSendNotification);
    captureStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    captureStatusLabel.setJustificationType(juce::Justification::centredLeft);
    captureStatusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    captureStatusLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(captureStatusLabel);
    captureStatusLabel.setVisible(false);
    
    captureWaveform = std::make_unique<CaptureWaveformView>();
    addAndMakeVisible(*captureWaveform);
    captureWaveform->setVisible(false);

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

    // Analyzer slope (visual, hidden in new layout)
    slopeLabel.setText("SLOPE", juce::dontSendNotification);
    slopeLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
    slopeLabel.setJustificationType(juce::Justification::centred);
    slopeLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(slopeLabel);
    slopeLabel.setVisible(false);

    slopeCombo.addItem("Flat", 1);
    slopeCombo.addItem("3 dB/oct", 2);
    slopeCombo.addItem("4.5 dB/oct", 3);
    slopeCombo.setJustificationType(juce::Justification::centred);
    slopeCombo.setTooltip("Analyzer visual slope compensation");
    addAndMakeVisible(slopeCombo);
    slopeCombo.setVisible(false);
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
            spd.addItem(makeItem(50, "Fast", cur == 0, [=]() {
                setChoice("analyzerSpeed", 0);
                spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Fast);
            }));
            spd.addItem(makeItem(51, "Medium", cur == 1, [=]() {
                setChoice("analyzerSpeed", 1);
                spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Medium);
            }));
            spd.addItem(makeItem(52, "Slow", cur == 2, [=]() {
                setChoice("analyzerSpeed", 2);
                spectrum->setSpectrumSpeed(AdvancedSpectrumDisplay::SpectrumSpeed::Slow);
            }));
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

    menu.addSeparator();

    // Quality & Oversampling
    {
        juce::PopupMenu qualityMenu;
        const int curQuality = getChoice("qualityMode");
        qualityMenu.addItem(makeItem(100, "Zero Latency", curQuality == 0, [=]() { setChoice("qualityMode", 0); }));
        qualityMenu.addItem(makeItem(101, "High Quality", curQuality == 1, [=]() { setChoice("qualityMode", 1); }));
        menu.addSubMenu("Quality Mode", qualityMenu);

        juce::PopupMenu osMenu;
        const int curOS = getChoice("oversamplingFactor");
        osMenu.addItem(makeItem(110, "Off", curOS == 0, [=]() { setChoice("oversamplingFactor", 0); }));
        osMenu.addItem(makeItem(111, "2x", curOS == 1, [=]() { setChoice("oversamplingFactor", 1); }));
        osMenu.addItem(makeItem(112, "4x", curOS == 2, [=]() { setChoice("oversamplingFactor", 2); }));
        osMenu.addItem(makeItem(113, "Auto", curOS == 3, [=]() { setChoice("oversamplingFactor", 3); }));
        menu.addSubMenu("Oversampling", osMenu);
    }

    // Analyzer Slope
    {
        juce::PopupMenu slopeMenu;
        const int curSlope = getChoice("analyzerSlope");
        slopeMenu.addItem(makeItem(120, "Flat", curSlope == 0, [=]() { setChoice("analyzerSlope", 0); }));
        slopeMenu.addItem(makeItem(121, "3 dB/oct", curSlope == 1, [=]() { setChoice("analyzerSlope", 1); }));
        slopeMenu.addItem(makeItem(122, "4.5 dB/oct", curSlope == 2, [=]() { setChoice("analyzerSlope", 2); }));
        menu.addSubMenu("Analyzer Slope", slopeMenu);
    }

    menu.addSeparator();

    // AI Sensitivity & Strength (show current values, click to reset)
    {
        const float sens = getFloat("aiSensitivity");
        const float str = getFloat("aiStrength");
        menu.addItem(makeItem(130, "AI Sensitivity: " + juce::String(sens, 0) + "% (click to reset)", false,
                              [=]() { setFloat("aiSensitivity", 50.0f); }));
        menu.addItem(makeItem(131, "AI Strength: " + juce::String(str, 0) + "% (click to reset)", false,
                              [=]() { setFloat("aiStrength", 100.0f); }));
    }

    menu.addSeparator();

    // Capture controls
    {
        juce::PopupMenu captureMenu;
        captureMenu.addItem(makeItem(140, "Capture Last (Retroactive)", false, [this]() {
            captureAnalyzeBtn.triggerClick();
        }));
        captureMenu.addItem(makeItem(141, "Start Live Capture", false, [this]() {
            startCaptureBtn.triggerClick();
        }));
        captureMenu.addItem(makeItem(142, "Stop + Analyze", false, [this]() {
            stopCaptureBtn.triggerClick();
        }));
        const bool autoCapture = autoCaptureBtn.getToggleState();
        captureMenu.addItem(makeItem(143, "Auto Peak Capture", autoCapture, [this]() {
            autoCaptureBtn.triggerClick();
        }));
        menu.addSubMenu("Capture", captureMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&optionsBtn));
}

void AIEqualizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Main background — subtle vertical gradient for depth
    {
        juce::ColourGradient bg(
            ModernLookAndFeel::Colors::bgDark, 0.0f, 0.0f,
            ModernLookAndFeel::Colors::bgDark.darker(0.15f), 0.0f, static_cast<float>(getHeight()),
            false);
        g.setGradientFill(bg);
        g.fillRect(getLocalBounds());
    }

    const float w = static_cast<float>(getWidth());

    // === HEADER BAR (gradient + subtle inner shadow) ===
    {
        auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, w, static_cast<float>(headerH));
        juce::ColourGradient headerGrad(
            ModernLookAndFeel::Colors::bgLight.brighter(0.06f), 0.0f, 0.0f,
            ModernLookAndFeel::Colors::bgLight.darker(0.04f), 0.0f, static_cast<float>(headerH),
            false);
        g.setGradientFill(headerGrad);
        g.fillRect(headerRect);

        // Bottom edge highlight
        g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.12f));
        g.fillRect(0.0f, static_cast<float>(headerH - 1), w, 1.0f);

        // Separator line
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.fillRect(0.0f, static_cast<float>(headerH), w, 1.0f);

    }

    // === BOTTOM CONTROL BAR (gradient + top edge) ===
    {
        float cpY = static_cast<float>(getHeight() - controlH);
        auto bottomRect = juce::Rectangle<float>(0.0f, cpY, w, static_cast<float>(controlH));

        juce::ColourGradient bottomGrad(
            ModernLookAndFeel::Colors::bgPanel.brighter(0.03f), 0.0f, cpY,
            ModernLookAndFeel::Colors::bgPanel.darker(0.05f), 0.0f, cpY + controlH,
            false);
        g.setGradientFill(bottomGrad);
        g.fillRect(bottomRect);

        // Top edge — prominent accent separator (FabFilter-style divider)
        g.setColour(juce::Colour(0xFF0A0A10));
        g.fillRect(0.0f, cpY - 1.0f, w, 2.0f);
        g.setColour(ModernLookAndFeel::Colors::accentBlue.withAlpha(0.18f));
        g.fillRect(0.0f, cpY + 1.0f, w, 1.0f);

        // Version text (bottom-right corner)
        g.setColour(ModernLookAndFeel::Colors::textMuted.withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        g.drawText("v2.1.1", getWidth() - 44, getHeight() - 14, 40, 12,
                   juce::Justification::centredRight);
    }
}

void AIEqualizerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === HEADER (32px) ===
    auto header = bounds.removeFromTop(headerH).reduced(4, 2);
    // Left group: nav + preset
    prevBtn.setBounds(header.removeFromLeft(24).reduced(1));
    nextBtn.setBounds(header.removeFromLeft(24).reduced(1));
    header.removeFromLeft(4);
    presetBox.setBounds(header.removeFromLeft(160).reduced(0, 2));
    header.removeFromLeft(2);
    savePresetBtn.setBounds(header.removeFromLeft(40).reduced(1));
    header.removeFromLeft(6);
    // Center group: A/B + phase
    btnA.setBounds(header.removeFromLeft(24).reduced(1));
    btnB.setBounds(header.removeFromLeft(24).reduced(1));
    header.removeFromLeft(4);
    copyBtn.setBounds(header.removeFromLeft(36).reduced(1));
    header.removeFromLeft(8);
    phaseModeCombo.setBounds(header.removeFromLeft(120).reduced(0, 2));
    header.removeFromLeft(8);
    // Spectrum toggles
    btnPre.setBounds(header.removeFromLeft(36).reduced(1));
    btnPost.setBounds(header.removeFromLeft(40).reduced(1));
    btnDelta.setBounds(header.removeFromLeft(48).reduced(1));
    // Right group
    bypassBtn.setBounds(header.removeFromRight(60).reduced(1));
    header.removeFromRight(4);
    aiPanelToggle.setBounds(header.removeFromRight(32).reduced(1));
    optionsBtn.setBounds(header.removeFromRight(60).reduced(1));

    // === RIGHT PANEL (collapsible) ===
    int rightW = aiPanelVisible ? 300 : 0;
    if (aiPanelVisible)
    {
        auto rightSide = bounds.removeFromRight(rightW).reduced(4);
        auto tabRow = rightSide.removeFromTop(26);
        int tabW = tabRow.getWidth() / 2;
        aiTabBtn.setBounds(tabRow.removeFromLeft(tabW).reduced(2));
        semanticTabBtn.setBounds(tabRow.reduced(2));
        rightSide.removeFromTop(4);
        int mainH = static_cast<int>(rightSide.getHeight() * 0.6f);
        aiProblemPanel->setBounds(rightSide.removeFromTop(mainH));
        semanticPanel->setBounds(aiProblemPanel->getBounds());
        rightSide.removeFromTop(4);
        dynamicEQPanel->setBounds(rightSide);
    }
    aiProblemPanel->setVisible(aiPanelVisible);
    semanticPanel->setVisible(aiPanelVisible && activeRightTab == 1);
    dynamicEQPanel->setVisible(aiPanelVisible);
    aiTabBtn.setVisible(aiPanelVisible);
    semanticTabBtn.setVisible(aiPanelVisible);

    // === BOTTOM BAR (55px) ===
    auto bottom = bounds.removeFromBottom(controlH).reduced(4, 4);

    // === LEFT: band selector ===
    auto leftCtrl = bottom.removeFromLeft(200);
    {
        auto row1 = leftCtrl.removeFromTop(leftCtrl.getHeight() / 2);
        auto numArea = row1.removeFromLeft(90);
        numBandsLabel.setBounds(numArea.removeFromLeft(42));
        numBandsCombo.setBounds(numArea.reduced(0, 4));
        row1.removeFromLeft(6);
        bandSelectCombo.setBounds(row1.reduced(0, 4));
    }

    // === RIGHT: output section (meter + knobs) ===
    auto rightControls = bottom.removeFromRight(240);
    {
        // Level meter — tall, visible
        outputMeter.setBounds(rightControls.removeFromRight(36).reduced(0, 2));
        rightControls.removeFromRight(8);

        // Version label below meter area
        versionLabel.setBounds(rightControls.removeFromRight(0));  // hidden from bar, shown in paint
        versionLabel.setVisible(false);

        // Output knobs (bigger)
        auto mixArea = rightControls.removeFromLeft(56);
        mixLabel.setBounds(mixArea.removeFromTop(13));
        mixKnob.setBounds(mixArea);
        rightControls.removeFromLeft(6);

        auto outArea = rightControls.removeFromLeft(56);
        outLabel.setBounds(outArea.removeFromTop(13));
        outKnob.setBounds(outArea);
        rightControls.removeFromLeft(6);

        autoBtn.setBounds(rightControls.removeFromLeft(44).reduced(0, 12));
    }

    // === CENTER: band detail panel ===
    bottom.removeFromLeft(8);
    bottom.removeFromRight(8);
    if (selectedBandPanel)
    {
        selectedBandPanel->setBounds(bottom);
        selectedBandPanel->setVisible(true);
    }

    // Hide moved items
    bandViewport->setVisible(false);
    dynamicEQMasterPanel->setVisible(false);
    for (auto& t : bandToggles) if (t) t->setVisible(false);
    captureWaveform->setVisible(false);

    // === SPECTRUM (everything remaining) ===
    bounds.reduce(4, 4);
    spectrumBounds = bounds;
    spectrum->setBounds(spectrumBounds);
    graphBounds = spectrumBounds.reduced(45, 25);
    graphBounds.removeFromBottom(22);
    graphBounds.removeFromLeft(5);

    updateBandPositions();
}
void AIEqualizerAudioProcessorEditor::timerCallback()
{
    ++timerTickCount;

    // Bug K fix: guard against timer firing during processor teardown
    if (!processor.isProcessorReady())
        return;

    // Drain RT-safe logger queue on message thread (every tick, cheap)
    AIEQLogger::getInstance().flushRTLogs();

    // Output level meter — feed every tick (20 Hz, smooth ballistics in LevelMeter)
    {
        float peakL = processor.getOutputPeakLeft();
        float peakR = processor.getOutputPeakRight();
        float dbL = peakL > 1e-10f ? juce::Decibels::gainToDecibels(peakL) : -100.0f;
        float dbR = peakR > 1e-10f ? juce::Decibels::gainToDecibels(peakR) : -100.0f;
        outputMeter.setLevels(dbL, dbR);
    }

    // Metrological spectrum pipeline — drain FIFOs every tick regardless of legacy path.
    // process() returns true only when at least one new FFT hop was completed.
    if (spectrumPipeline && spectrum)
    {
        const size_t dispW = static_cast<size_t>(
            juce::jmax(64.0f, spectrum->getGraphBoundsF().getWidth()));

        if (spectrumPipeline->process(dispW))
        {
            // Feed pixel data to AdvancedSpectrumDisplay (replaces old SpectrumAnalyzer path)
            spectrum->injectPrecomputedSpectrum(
                spectrumPipeline->getPrePixelDB(),
                spectrumPipeline->getPostPixelDB());

            // Push same data to GLSpectrumHelper for GPU rendering
            if (glSpectrumHelper)
            {
                glSpectrumHelper->updateSpectrumData(
                    spectrumPipeline->getPrePixelDB(),
                    spectrumPipeline->getPostPixelDB(),
                    spectrum->getGraphBoundsF(),
                    -90.0f, 12.0f);
            }

            // R[k] = M[k] ∨ D[k]: repaint when new data arrives
            spectrum->repaint();
        }
    }

    // Legacy FFT handoff — kept for fallback (pipeline not yet warmed up at startup)
    if (processor.consumeSpectrumDataReady())
    {
        processor.getSpectrumAnalyzer().processFFT();
        if (processor.getPostEQAnalyzer().hasNewData())
            processor.getPostEQAnalyzer().processFFT();
    }

    // AI problems update - every other tick (10Hz effective) to reduce message thread load
    if ((timerTickCount & 1) == 0)
    {
        if (processor.consumeAIProblemsChanged())
        {
            if (aiProblemPanel)
                aiProblemPanel->refreshFromProcessor();
        }
    }

    // Diagnostics: log block clamp events when they change (message thread safe)
    auto clampEvents = processor.getBlockClampEvents();
    if (clampEvents != lastBlockClampEvents)
    {
        lastBlockClampEvents = clampEvents;
        juce::Logger::outputDebugString("AI Equalizer Pro - block clamp events: " + juce::String((int)clampEvents));
    }

    // Parameter-driven UI updates (bands, toggles, A/B, quality)
    // Throttled to every 2nd tick (~10Hz) - reduces message thread load in Ableton.
    // Parameter changes are still detected via counter so nothing is lost, just
    // applied slightly later (50ms max delay is imperceptible to the user).
    uint64_t currentChange = processor.getParameterChangeCounter();
    if (currentChange != lastParameterChangeCount && (timerTickCount & 1) == 0)
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
    
    // Update EQBandControl selected state (glow on selected node)
    for (int i = 0; i < bands.size(); ++i)
        bands[i]->setSelected(i == bandIndex);

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
    
    // Update visibility (only show if panel is visible)
    aiProblemPanel->setVisible(aiPanelVisible && tab == 0);
    semanticPanel->setVisible(aiPanelVisible && tab == 1);
    
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

//==============================================================================
// Preset Navigation
//==============================================================================
void AIEqualizerAudioProcessorEditor::rebuildPresetMenu()
{
    presetBox.clear(juce::dontSendNotification);
    cachedPresetList.clear();

    if (!processor.hasPresetManager())
        return;

    auto& pm = processor.getPresetManager();
    auto all = pm.getAllPresets();

    int id = 1;
    juce::String lastCategory;

    for (const auto& preset : all)
    {
        // Add category separator if category changed
        if (preset.category != lastCategory)
        {
            if (!lastCategory.isEmpty())
                presetBox.addSeparator();
            presetBox.addSectionHeading(preset.category.toUpperCase());
            lastCategory = preset.category;
        }
        presetBox.addItem(preset.name, id);
        cachedPresetList.push_back(preset);
        ++id;
    }
}

void AIEqualizerAudioProcessorEditor::navigatePreset(int direction)
{
    if (cachedPresetList.empty())
        return;

    currentPresetIndex += direction;

    if (currentPresetIndex < 0)
        currentPresetIndex = static_cast<int>(cachedPresetList.size()) - 1;
    else if (currentPresetIndex >= static_cast<int>(cachedPresetList.size()))
        currentPresetIndex = 0;

    presetBox.setSelectedId(currentPresetIndex + 1, juce::sendNotification);
}

void AIEqualizerAudioProcessorEditor::showSavePresetDialog()
{
    auto* aw = new juce::AlertWindow("Save Preset",
                                      "Enter a name for your preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "My Preset", "Name:");
    aw->addComboBox("category", { "User", "Vocals", "Drums", "Bass", "Guitar", "Keys", "Master", "EDM", "Creative", "Utility" }, "Category:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, aw](int result)
        {
            if (result == 1)
            {
                auto name = aw->getTextEditorContents("name").trim();
                auto* catBox = aw->getComboBoxComponent("category");
                auto category = catBox ? catBox->getText() : "User";

                if (name.isNotEmpty() && processor.hasPresetManager())
                {
                    processor.getPresetManager().saveUserPreset(name, category);
                    rebuildPresetMenu();
                }
            }
            delete aw;
        }),
        true);
}
