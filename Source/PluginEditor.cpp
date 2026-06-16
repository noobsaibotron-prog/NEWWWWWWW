#include "PluginEditor.h"
#include "Utils/Logger.h"
#include <thread>

AIEqualizerAudioProcessorEditor::AIEqualizerAudioProcessorEditor(AIEqualizerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);

    // Attach OpenGL context to this top-level component for GPU compositing of
    // all child component paint() calls (setComponentPaintingEnabled). No custom
    // GL renderer: the spectrum is drawn by the software path. setContinuousRepainting
    // (false): we drive repaints via our own Timer.
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.setContinuousRepainting(false);
    // 4x MSAA for smooth spectrum lines and anti-aliased fills
    juce::OpenGLPixelFormat fmt;
    fmt.multisamplingLevel = 4;
    openGLContext.setPixelFormat(fmt);
    openGLContext.attachTo(*this);

    // Premium matericità: 256×256 tiled ANISOTROPIC noise texture (generated once).
    // Simulates brushed aluminium with horizontal grain direction.
    // Each row gets a coherent base luminance; individual pixels deviate ±20
    // from that base. This creates subtle horizontal striations instead of
    // uniform sandblast noise. Alpha 6/255 ≈ 2.3% opacity.
    {
        const int noiseSz = 256;
        noiseTexture = juce::Image(juce::Image::ARGB, noiseSz, noiseSz, true);
        juce::Image::BitmapData bmp(noiseTexture, juce::Image::BitmapData::writeOnly);
        juce::Random rng(42); // deterministic seed for reproducible look
        for (int y = 0; y < noiseSz; ++y)
        {
            // Per-row base grey → horizontal coherence (brushed grain)
            const int baseGrey = rng.nextInt(256);
            for (int x = 0; x < noiseSz; ++x)
            {
                // ±20 per-pixel deviation from row base for micro-variation
                const int deviation = rng.nextInt(41) - 20;
                const uint8_t grey = static_cast<uint8_t>(juce::jlimit(0, 255, baseGrey + deviation));
                bmp.setPixelColour(x, y, juce::Colour(grey, grey, grey, static_cast<uint8_t>(6)));
            }
        }
    }

    // Metrological 5-layer spectrum pipeline.
    // FFT size is driven by the "analyzerResolution" APVTS parameter:
    //   choice 0 → 1024 samples  (fftOrder 10)
    //   choice 1 → 2048 samples  (fftOrder 11)
    //   choice 2 → 4096 samples  (fftOrder 12, default "High")
    //   choice 3 → 8192 samples  (fftOrder 13)
    // Both PRE and POST must ALWAYS use the same FFT (non-negotiable).
    int resChoice = 2;
    if (auto* resParam = processor.getAPVTS().getRawParameterValue("analyzerResolution"))
        resChoice = juce::jlimit(0, 3, static_cast<int>(std::round(resParam->load())));
    const size_t initialFFTOrder = static_cast<size_t>(10 + resChoice);

    spectrumPipeline = std::make_unique<NewSpectrumPipeline>(
        processor.getPreEqFifo(),
        processor.getPostEqFifo(),
        initialFFTOrder,
        processor.getSampleRate(),
        60.0);

    // Live wiring: menu click, preset load, and DAW automation on
    // "analyzerResolution" all route through this ParameterAttachment to
    // NewSpectrumPipeline::setFFTOrder (which holds the pipeline's SpinLock
    // so process() is never mid-flight when preCore/postCore are rebuilt).
    if (auto* resParam = processor.getAPVTS().getParameter("analyzerResolution"))
    {
        analyzerResolutionAttachment = std::make_unique<juce::ParameterAttachment>(
            *resParam,
            [this](float newNormalisedOrChoice)
            {
                // RangedAudioParameter::ParameterAttachment callback receives the
                // already-denormalised value for choice/int params.
                const int choice = juce::jlimit(0, 3,
                    static_cast<int>(std::round(newNormalisedOrChoice)));
                const size_t newOrder = static_cast<size_t>(10 + choice);
                if (spectrumPipeline)
                    spectrumPipeline->setFFTOrder(newOrder, processor.getSampleRate());
            });
        // No sendInitialUpdate(): the pipeline was just constructed with the
        // current value, so we'd only waste a rebuild here.
    }

    createHeader();
    createControlPanel();
    createBands();
    
    spectrum = std::make_unique<AdvancedSpectrumDisplay>(processor);
    // GL renders under software paint() which draws an opaque background on top,
    // so GL spectrum is never visible. Keep software path active for now.
    // spectrum->setGLSpectrumActive(true);
    addAndMakeVisible(*spectrum);

    // Wave 5 verdict: BandTabBar removed — band identity via coloured node
    // rings on the curve + the three big filmstrip knobs in the left panel.

    // DynEQ integration removed — controls now integrated directly in BandControlPanel
    // The "..." expand button in BandControlPanel opens the advanced overlay (Range/Knee)

    // Phase 7D: passive breathing amber dot (driven by timerCallback)
    addAndMakeVisible(aiBreathingDot);

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
    // [Gemma Phase 3, Point 2]: "inset" property → CNC-milled recessed look
    aiTabBtn.getProperties().set("inset", true);
    aiTabBtn.setColour(juce::TextButton::buttonColourId,
        juce::Colour(0xFF232028).interpolatedWith(ModernLookAndFeel::Colors::amber, 0.08f));
    aiTabBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::amber);
    aiTabBtn.setTooltip("AI Problem Detection - Automatic issue identification");
    aiTabBtn.onClick = [this]() { switchRightTab(0); };
    addAndMakeVisible(aiTabBtn);

    semanticTabBtn.getProperties().set("inset", true);
    semanticTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF222228));
    semanticTabBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF56544E));
    semanticTabBtn.setTooltip("Semantic Control - Shape sound with words like 'Air', 'Warmth', 'Punch'");
    semanticTabBtn.onClick = [this]() { switchRightTab(1); };
    addAndMakeVisible(semanticTabBtn);

    optionsBtn.setButtonText("...");  // Inter font lacks U+2699 gear glyph — ASCII fallback
    optionsBtn.setTooltip("Global options and analyzer settings");
    optionsBtn.onClick = [this]() { showOptionsMenu(); };
    addAndMakeVisible(optionsBtn);
    
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
    // Phase 5: full DynamicEQPanel is now an on-demand overlay, hidden by
    // default. Toggled visible via DynEQCompactBar::onExpandRequested.
    addChildComponent(*dynamicEQPanel);
    
    // Dynamic EQ Master Panel (global controls)
    dynamicEQMasterPanel = std::make_unique<DynamicEQMasterPanel>(processor.getAPVTS());
    dynamicEQMasterPanel->setTotalGRProvider([this]() {
        return processor.getDynamicTotalGainReduction();
    });
    addAndMakeVisible(*dynamicEQMasterPanel);
    
    // FIX 2: persistent selected band panel (avoid recreating on every selection)
    selectedBandPanel = std::make_unique<BandControlPanel>(0, processor.getAPVTS());
    addAndMakeVisible(*selectedBandPanel);

    // Connect BandControlPanel "..." button to DynEQ advanced overlay (Range/Knee)
    selectedBandPanel->onExpandDynEQRequested = [this] {
        if (dynamicEQPanel)
        {
            const bool nowVisible = !dynamicEQPanel->isVisible();
            dynamicEQPanel->setVisible(nowVisible);
            if (nowVisible)
            {
                dynamicEQPanel->toFront(true);
                const int w = 400, h = 300;
                dynamicEQPanel->setBounds(getWidth()/2 - w/2, getHeight()/2 - h/2, w, h);
            }
        }
    };

    // Output level meter (stereo VU with peak hold)
    addAndMakeVisible(outputMeter);

    // Version label (bottom-right branding)
    versionLabel.setText("v2.1.1", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    versionLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    setSize(1200, 810);
    setResizable(true, true);
    setResizeLimits(1100, 740, 1800, 1200);
    
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

    // Detach GL context BEFORE destroying pipeline objects.
    openGLContext.detach();

    // Destroy pipeline after GL context is gone — safe to release heap now
    spectrumPipeline.reset();

    setLookAndFeel(nullptr);
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
    
    // A/B/C/D slot switching
    auto selectSlot = [this](AIEqualizerAudioProcessor::ABState state) {
        processor.setABState(state);
        btnA.setToggleState(state == AIEqualizerAudioProcessor::ABState::A, juce::dontSendNotification);
        btnB.setToggleState(state == AIEqualizerAudioProcessor::ABState::B, juce::dontSendNotification);
        btnC.setToggleState(state == AIEqualizerAudioProcessor::ABState::C, juce::dontSendNotification);
        btnD.setToggleState(state == AIEqualizerAudioProcessor::ABState::D, juce::dontSendNotification);
    };

    btnA.setRadioGroupId(1001);
    btnA.setToggleState(true, juce::dontSendNotification);
    btnA.setTooltip("Slot A");
    btnA.onClick = [this, selectSlot]() { selectSlot(AIEqualizerAudioProcessor::ABState::A); };
    addAndMakeVisible(btnA);

    btnB.setRadioGroupId(1001);
    btnB.setTooltip("Slot B");
    btnB.onClick = [this, selectSlot]() { selectSlot(AIEqualizerAudioProcessor::ABState::B); };
    addAndMakeVisible(btnB);

    btnC.setRadioGroupId(1001);
    btnC.setTooltip("Slot C");
    btnC.onClick = [this, selectSlot]() { selectSlot(AIEqualizerAudioProcessor::ABState::C); };
    addAndMakeVisible(btnC);

    btnD.setRadioGroupId(1001);
    btnD.setTooltip("Slot D");
    btnD.onClick = [this, selectSlot]() { selectSlot(AIEqualizerAudioProcessor::ABState::D); };
    addAndMakeVisible(btnD);

    copyBtn.setTooltip("Copy active slot to next slot");
    copyBtn.onClick = [this]() {
        auto state = processor.getCurrentABState();
        switch (state) {
            case AIEqualizerAudioProcessor::ABState::A: processor.copyAtoB(); break;
            case AIEqualizerAudioProcessor::ABState::B: processor.copyBtoC(); break;
            case AIEqualizerAudioProcessor::ABState::C: processor.copyCtoD(); break;
            case AIEqualizerAudioProcessor::ABState::D: processor.copyAtoB(); break;
        }
    };
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
    aiPanelToggle.setToggleState(true, juce::dontSendNotification);
    aiPanelToggle.setTooltip("AI Engine Active");
    aiPanelToggle.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLighter);
    aiPanelToggle.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amber);
    aiPanelToggle.onClick = [this]() {
        // AI toggle now indicates AI engine state (panel is always visible)
        repaint();
    };
    addAndMakeVisible(aiPanelToggle);
}

void AIEqualizerAudioProcessorEditor::createControlPanel()
{
    // Wave 4A: Logo "AI EQ PRO" — big Bold amber (20px) matching the Liquid
    // Intelligence rendering. Also apply a slight positive kerning so the
    // letters read as a premium signature, not a form label.
    logoLabel.setText("AI EQ PRO", juce::dontSendNotification);
    {
        auto logoFont = juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold"));
        logoFont.setExtraKerningFactor(0.08f);
        logoLabel.setFont(logoFont);
    }
    logoLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::amber);
    logoLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(logoLabel);
    
    subtitleLabel.setText("Intelligent Equalizer", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textSecondary);
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setVisible(false);
    

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
    
    // Sensitivity knob (connected to APVTS, visible in AI panel)
    sensitivityLabel.setText("SENSITIVITY", juce::dontSendNotification);
    sensitivityLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    sensitivityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(sensitivityLabel);

    sensitivityKnob.setTooltip("AI Sensitivity - Higher values detect more subtle problems\nLower values only flag obvious issues");
    addAndMakeVisible(sensitivityKnob);
    sensitivityAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiSensitivity", sensitivityKnob);

    // Strength knob (connected to APVTS, visible in AI panel)
    strengthLabel.setText("CORRECTION", juce::dontSendNotification);
    strengthLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    strengthLabel.setJustificationType(juce::Justification::centred);
    strengthLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(strengthLabel);

    strengthKnob.setTooltip("AI Correction Amount - Controls how aggressively corrections are applied\n100% = Full suggested correction, 50% = Half correction");
    addAndMakeVisible(strengthKnob);
    strengthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "aiStrength", strengthKnob);
    
    // Auto with tooltip
    autoBtn.setTooltip("Auto Gain - Automatically compensates for volume changes from EQ");
    addAndMakeVisible(autoBtn);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "autoGain", autoBtn);

    // Quality mode toggle (Zero Latency / High Quality, hidden in new layout)
    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    qualityLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    qualityLabel.setJustificationType(juce::Justification::centred);
    qualityLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(qualityLabel);
    qualityLabel.setVisible(false);

    qualityBtn.setTooltip("Quality Mode: HQ abilita 5ms di lookahead (piÃ¹ latenza), ZL = zero-latency");
    qualityBtn.setClickingTogglesState(true);
    qualityBtn.setComponentID("qualityToggle");
    qualityBtn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight);
    // Wave 4A: amber toggle-on, dark text on amber for readability.
    qualityBtn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amber);
    qualityBtn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary);
    qualityBtn.setVisible(false);
    qualityBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
    qualityBtn.setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    qualityBtn.onClick = [this]() {
        int mode = qualityBtn.getToggleState() ? 1 : 0; // 1 = High Quality, 0 = Zero Latency
        if (auto* param = processor.getAPVTS().getParameter("qualityMode"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(mode)));
    };
    addAndMakeVisible(qualityBtn);

    // Oversampling selector (Off / 2x / 4x / Auto, hidden in new layout)
    oversamplingLabel.setText("OVERSAMP", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
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
    captureLenLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
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
    captureStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    captureStatusLabel.setJustificationType(juce::Justification::centredLeft);
    captureStatusLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    captureStatusLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(captureStatusLabel);
    captureStatusLabel.setVisible(false);

    // Number of active bands selector (1-24)
    numBandsLabel.setText("BANDS", juce::dontSendNotification);
    numBandsLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
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
    outLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    outLabel.setJustificationType(juce::Justification::centred);
    outLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(outLabel);
    
    outKnob.setTooltip("Output Gain - Adjust overall output level (-24dB to +24dB)");
    addAndMakeVisible(outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outKnob);
    
    // Dry/Wet mix
    mixLabel.setText("DRY/WET", juce::dontSendNotification);
    mixLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    mixLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textMuted);
    addAndMakeVisible(mixLabel);
    
    mixKnob.setTooltip("Blend between dry (0%) and fully processed (100%) signal");
    addAndMakeVisible(mixKnob);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "dryWet", mixKnob);

    // Analyzer slope (visual, hidden in new layout)
    slopeLabel.setText("SLOPE", juce::dontSendNotification);
    slopeLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
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

    // Wave 4A: ensure the logo font is reapplied AFTER every other setFont in
    // createControlPanel() so any future LookAndFeel restyle can't silently
    // override the 20 px Bold Amber signature. This also pins the canonical
    // logo font to the end of the constructor for deterministic behaviour.
    {
        auto logoFontFinal = juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold"));
        logoFontFinal.setExtraKerningFactor(0.08f);
        logoLabel.setFont(logoFontFinal);
    }
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
    // Wave 4A: Liquid Intelligence background — deep blue-black gradient (135°)
    // matching the mockup. The top-left is nearly black, the bottom-right has a
    // subtle navy tint so the spectrum's azure dust reads as if it floats above
    // a very dark ocean. Previous values (0xFF0A0A0E → 0xFF141420) were too
    // neutral grey and made the plugin look "grey plastic".
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    juce::ColourGradient bgGrad(
        juce::Colour(0xFF05070E), 0.0f, 0.0f,        // top-left: near black with faint blue
        juce::Colour(0xFF0F1A2E), w, h,              // bottom-right: deep navy
        false);
    // Intermediate stop to stretch the navy glow through the middle band
    bgGrad.addColour(0.45, juce::Colour(0xFF0A1220));
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Premium matericità: tiled noise overlay breaks gradient banding and
    // simulates brushed aluminium / polycarbonate surface texture.
    // CRITICAL: apply ONLY to header + bottom panel + footer — NOT the spectrum
    // area. The spectrum child is non-opaque, so any parent repaint in its
    // region forces a full re-tile at 60fps. Restrict noise to static zones.
    if (noiseTexture.isValid())
    {
        g.setTiledImageFill(noiseTexture, 0, 0, 1.0f);
        // Header only
        g.fillRect(0.0f, 0.0f, w, static_cast<float>(headerH));
        // Bottom panel + footer (below spectrum)
        float panelY = static_cast<float>(getHeight() - footerH - controlH);
        g.fillRect(0.0f, panelY, w, static_cast<float>(controlH + footerH));
    }

    // === HEADER BAR (gradient + subtle inner shadow + dividers) ===
    {
        auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, w, static_cast<float>(headerH));
        juce::ColourGradient headerGrad(
            ModernLookAndFeel::Colors::bgLight.brighter(0.06f), 0.0f, 0.0f,
            ModernLookAndFeel::Colors::bgLight.darker(0.04f), 0.0f, static_cast<float>(headerH),
            false);
        g.setGradientFill(headerGrad);
        g.fillRect(headerRect);

        // Bottom edge highlight
        g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.12f));
        g.fillRect(0.0f, static_cast<float>(headerH - 1), w, 1.0f);

        // Separator line
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.fillRect(0.0f, static_cast<float>(headerH), w, 1.0f);

        // Thin vertical dividers between header groups (mockup: 1px, 18px tall, centered)
        auto divCol = juce::Colour(0xFFFFFFFF).withAlpha(0.12f);
        float divH = 18.0f;
        float divY = (static_cast<float>(headerH) - divH) / 2.0f;
        g.setColour(divCol);

        // Divider 1: after logo, before preset
        float d1x = static_cast<float>(logoLabel.getRight()) + 2.0f;
        g.fillRect(d1x, divY, 1.0f, divH);

        // Divider 2: after preset nav, before A/B slots
        float d2x = static_cast<float>(nextBtn.getRight()) + 4.0f;
        g.fillRect(d2x, divY, 1.0f, divH);

        // Divider 3: between comparison group (PRE/POST/DELTA/BYPASS) and processing mode (ZL/Phase/OS)
        float d3x = static_cast<float>(qualityBtn.getX()) - 6.0f;
        g.fillRect(d3x, divY, 1.0f, divH);

        // Divider 4: between processing mode group and AI/Options
        float d4x = static_cast<float>(optionsBtn.getX()) - 6.0f;
        g.fillRect(d4x, divY, 1.0f, divH);
    }

    // === FOOTER BAR — same anodized aluminium chassis material ===
    {
        float ftY = static_cast<float>(getHeight() - footerH);

        // Anodized aluminium gradient (matches bottom panel)
        {
            juce::ColourGradient footerGrad(
                juce::Colour(0xFF181A24), 0.0f, ftY,
                juce::Colour(0xFF101218), 0.0f, ftY + static_cast<float>(footerH),
                false);
            g.setGradientFill(footerGrad);
            g.fillRect(0.0f, ftY, w, static_cast<float>(footerH));
        }

        // (Noise already applied in the unified block above)

        // Three-line top edge — rim light at 5%
        g.setColour(juce::Colour(0xFF0A0A10));
        g.fillRect(0.0f, ftY - 1.0f, w, 2.0f);
        g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.22f));
        g.fillRect(0.0f, ftY + 1.0f, w, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));  // rim light enhanced
        g.fillRect(0.0f, ftY + 2.0f, w, 1.0f);

        // Bottom bevel (thickness illusion)
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, w);
    }

    // === BOTTOM PANEL — "Anodized Aluminium Chassis" (Gemma Phase 3) ===
    {
        float cpY = static_cast<float>(getHeight() - footerH - controlH);
        auto bottomRect = juce::Rectangle<float>(0.0f, cpY, w, static_cast<float>(controlH));

        // [Point 3] AMBIENT OCCLUSION — soft shadow cast UPWARD from chassis
        // into spectrum zone. Separates the bar from the analyzer background.
        // Drawn BEFORE the chassis fill so it appears above the bar edge.
        {
            const float aoHeight = 6.0f;
            juce::ColourGradient aoGrad(
                juce::Colours::black.withAlpha(0.12f), 0.0f, cpY,        // bar edge: darkest
                juce::Colours::transparentBlack,       0.0f, cpY - aoHeight, // fades upward
                false);
            g.setGradientFill(aoGrad);
            g.fillRect(0.0f, cpY - aoHeight, w, aoHeight);
        }

        // [Point 1] ANODIZED ALUMINIUM GRADIENT — subtle top-to-bottom
        // lighter anthracite → darker anthracite (not flat solid)
        {
            juce::ColourGradient chassisGrad(
                juce::Colour(0xFF181A24), 0.0f, cpY,                        // top: slightly lighter
                juce::Colour(0xFF101218), 0.0f, cpY + static_cast<float>(controlH), // bottom: darker
                false);
            g.setGradientFill(chassisGrad);
            g.fillRect(bottomRect);
        }

        // (Noise texture already applied in the unified block above)

        // Top edge — three-line stack for material depth
        // [Point 4] RIM LIGHT — 1px high-contrast white at 5% (was 3%)
        g.setColour(juce::Colour(0xFF0A0A10));             // dark hairline
        g.fillRect(0.0f, cpY - 1.0f, w, 2.0f);
        g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.22f));  // amber accent
        g.fillRect(0.0f, cpY + 1.0f, w, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));  // rim light (enhanced 3%→5%)
        g.fillRect(0.0f, cpY + 2.0f, w, 1.0f);

        // [Point 5] BOTTOM BEVEL — inner shadow at bar's bottom edge
        // Gives the chassis physical thickness / Z-axis depth
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.drawHorizontalLine(static_cast<int>(cpY + static_cast<float>(controlH) - 1),
                             0.0f, w);

        // Vertical divider between band controls (380px) and context panel
        float divX = static_cast<float>(juce::jmin(380, getWidth() / 2));
        g.setColour(juce::Colour(0xFF0A0A10));
        g.fillRect(divX, cpY + 6.0f, 1.0f, static_cast<float>(controlH - 12));
        g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.08f));
        g.fillRect(divX + 1.0f, cpY + 6.0f, 1.0f, static_cast<float>(controlH - 12));

        // [Point 6] AI PANEL "RECESSED GLASS DISPLAY" — inner glow on right panel
        // Simulates an OLED screen embedded in metal (matte chassis vs glossy display)
        {
            float rpX = divX + 2.0f;
            float rpW = w - rpX;
            float rpY = cpY + 4.0f;
            float rpH = static_cast<float>(controlH) - 8.0f;
            auto rpRect = juce::Rectangle<float>(rpX, rpY, rpW, rpH);

            // Inner glow: top edge (accentBlue at low opacity)
            {
                juce::ColourGradient glowTop(
                    ModernLookAndFeel::Colors::accentBlue.withAlpha(0.07f), rpX, rpY,
                    juce::Colours::transparentBlack, rpX, rpY + 4.0f,
                    false);
                g.setGradientFill(glowTop);
                g.fillRect(rpRect.withHeight(4.0f));
            }
            // Inner glow: bottom edge
            {
                juce::ColourGradient glowBot(
                    ModernLookAndFeel::Colors::accentBlue.withAlpha(0.05f), rpX, rpY + rpH,
                    juce::Colours::transparentBlack, rpX, rpY + rpH - 4.0f,
                    false);
                g.setGradientFill(glowBot);
                g.fillRect(rpRect.removeFromBottom(4.0f));
            }
            // Inner glow: left edge
            {
                juce::ColourGradient glowLeft(
                    ModernLookAndFeel::Colors::accentBlue.withAlpha(0.06f), rpX, rpY,
                    juce::Colours::transparentBlack, rpX + 4.0f, rpY,
                    false);
                g.setGradientFill(glowLeft);
                g.fillRect(rpX, rpY, 4.0f, rpH);
            }
            // Inset border: dark top/left, light bottom/right
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawHorizontalLine(static_cast<int>(rpY), rpX, rpX + rpW);
            g.drawVerticalLine(static_cast<int>(rpX), rpY, rpY + rpH);
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            g.drawHorizontalLine(static_cast<int>(rpY + rpH), rpX, rpX + rpW);
            g.drawVerticalLine(static_cast<int>(rpX + rpW - 1.0f), rpY, rpY + rpH);
        }
    }
}

void AIEqualizerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === HEADER (44 px — mockup: logo | div | ‹ Preset › | div | A B | spacer | PRE POST DELTA | div | LINEAR PHASE | div | AI dot) ===
    auto header = bounds.removeFromTop(headerH).reduced(14, 0);

    // Wave 4A: Logo "AI EQ PRO" — widened 72 → 130 to fit the 20px Bold amber
    // signature. The label is centred vertically inside the 44px header row.
    logoLabel.setVisible(true);
    logoLabel.setBounds(header.removeFromLeft(130));
    header.removeFromLeft(4);

    // Phase 7D: breathing amber dot next to the logo (Tribunale directive #3)
    {
        auto dotSlot = header.removeFromLeft(18);
        aiBreathingDot.setBounds(dotSlot.withSizeKeepingCentre(14, 14));
    }

    header.removeFromLeft(10);  // divider gap after logo group

    // Preset navigation (mockup: ‹ [Vocal Presence] ›)
    prevBtn.setBounds(header.removeFromLeft(24).reduced(0, 8));
    presetBox.setBounds(header.removeFromLeft(140).reduced(0, 8));
    nextBtn.setBounds(header.removeFromLeft(24).reduced(0, 8));
    header.removeFromLeft(12);  // divider gap before A/B

    // A/B slots only (mockup shows only A and B, C/D hidden)
    btnA.setBounds(header.removeFromLeft(28).reduced(1, 8));
    header.removeFromLeft(4);
    btnB.setBounds(header.removeFromLeft(28).reduced(1, 8));
    btnC.setVisible(false);
    btnD.setVisible(false);

    // Right group — three logical clusters separated by divider gaps:
    //   [AI] [...]  |  [OS] [Phase] [ZL]  |  [BYPASS] [DELTA] [POST] [PRE]

    // --- Cluster 3: AI + Options (far right) ---
    aiPanelToggle.setBounds(header.removeFromRight(32).reduced(0, 8));
    header.removeFromRight(8);
    optionsBtn.setVisible(true);
    optionsBtn.setBounds(header.removeFromRight(32).reduced(0, 8));
    header.removeFromRight(12);  // divider gap

    // --- Cluster 2: Processing Mode (OS, Phase, ZL/HQ) ---
    oversamplingBtn.setVisible(true);
    oversamplingBtn.setBounds(header.removeFromRight(38).reduced(0, 8));
    header.removeFromRight(4);
    phaseModeCombo.setBounds(header.removeFromRight(110).reduced(0, 8));
    header.removeFromRight(4);
    qualityBtn.setVisible(true);
    qualityBtn.setBounds(header.removeFromRight(28).reduced(0, 8));
    header.removeFromRight(12);  // divider gap

    // --- Cluster 1: Comparison (BYPASS, DELTA, POST, PRE) ---
    bypassBtn.setVisible(true);
    bypassBtn.setBounds(header.removeFromRight(56).reduced(1, 8));
    header.removeFromRight(6);
    btnDelta.setVisible(true);
    btnPost.setVisible(true);
    btnDelta.setBounds(header.removeFromRight(50).reduced(1, 8));
    header.removeFromRight(6);
    btnPost .setBounds(header.removeFromRight(44).reduced(1, 8));
    header.removeFromRight(6);
    btnPre  .setBounds(header.removeFromRight(40).reduced(1, 8));

    // Hide non-essential items (accessible via Options menu)
    savePresetBtn.setVisible(false);
    copyBtn.setVisible(false);

    // Wave 5 verdict: BandTabBar (Roman numerals above spectrum) removed —
    // spectrum now starts immediately below the header, giving the curve and
    // node rings the full canvas for readability.

    // === FOOTER BAR (52px — signal flow only: meter | OUT | MIX | AUTO | version) ===
    // BYPASS, ZL/HQ, OS moved to header. Footer = monitoring + output controls.
    auto footer = bounds.removeFromBottom(footerH).reduced(12, 0);
    {
        // Stereo meter (left side)
        outputMeter.setBounds(footer.removeFromLeft(juce::jmax(160, footer.getWidth() * 3 / 10)).reduced(0, 4));

        // Version label (far right)
        versionLabel.setVisible(true);
        versionLabel.setBounds(footer.removeFromRight(30));

        // Hidden combo keeps APVTS attachment alive
        oversamplingCombo.setVisible(false);
    }

    // Output Gain — LargeAmber knob, same Manus filmstrip as band knobs
    footer.removeFromLeft(10);  // gap after meter
    outLabel.setVisible(true);
    outLabel.setBounds(footer.removeFromLeft(16).reduced(0, 16));
    outKnob.setVisible(true);
    outKnob.setBounds(footer.removeFromLeft(46).reduced(0, 2));

    footer.removeFromLeft(6);  // gap

    // Dry/Wet — LargeAmber knob, same material as band knobs
    mixLabel.setVisible(true);
    mixLabel.setBounds(footer.removeFromLeft(16).reduced(0, 16));
    mixKnob.setVisible(true);
    mixKnob.setBounds(footer.removeFromLeft(46).reduced(0, 2));

    // Auto Gain stays visible — placed in footer before BYPASS
    autoBtn.setVisible(true);
    autoBtn.setBounds(footer.removeFromRight(52).reduced(0, 5));
    footer.removeFromRight(6);

    // === BOTTOM PANEL — split: left=band controls (380px), right=context (flex) ===
    auto bottom = bounds.removeFromBottom(controlH).reduced(0, 0);

    // --- Left column: band controls (mockup: 380px fixed, border-right) ---
    int bandColW = juce::jmin(380, bottom.getWidth() / 2);
    auto bandCol = bottom.removeFromLeft(bandColW);
    bottom.removeFromLeft(1);  // 1px border-right space

    // --- Right column: context panel (remaining space) ---
    auto contextCol = bottom;
    {
        // Tab row
        auto tabRow = contextCol.removeFromTop(24);
        int tabW = tabRow.getWidth() / 2;
        aiTabBtn.setBounds(tabRow.removeFromLeft(tabW).reduced(1));
        semanticTabBtn.setBounds(tabRow.reduced(1));
        contextCol.removeFromTop(2);

        // AI tab: sensitivity/correction knobs at top.
        // Fix: give the knob row enough height (50px) and constrain knob bounds
        // to a SQUARE centered region so the circular filmstrip doesn't stretch
        // into an ellipse.
        if (activeRightTab == 0)
        {
            auto knobRow = contextCol.removeFromTop(50);
            auto sensArea = knobRow.removeFromLeft(knobRow.getWidth() / 2);
            auto strArea  = knobRow;

            sensitivityLabel.setBounds(sensArea.removeFromTop(10));
            strengthLabel.setBounds(strArea.removeFromTop(10));

            const int sensSize = juce::jmin(sensArea.getWidth(), sensArea.getHeight());
            sensitivityKnob.setBounds(sensArea.withSizeKeepingCentre(sensSize, sensSize));

            const int strSize = juce::jmin(strArea.getWidth(), strArea.getHeight());
            strengthKnob.setBounds(strArea.withSizeKeepingCentre(strSize, strSize));

            contextCol.removeFromTop(2);
        }

        aiProblemPanel->setBounds(contextCol);
        semanticPanel->setBounds(contextCol);

        // Phase 5: dynamicEQPanel is now an on-demand overlay. Only reposition
        // when the user has toggled it visible via DynEQCompactBar; otherwise
        // it stays hidden and its bounds are irrelevant.
        if (dynamicEQPanel && dynamicEQPanel->isVisible())
        {
            const int w = 400;
            const int h = 300;
            dynamicEQPanel->setBounds(getWidth() / 2 - w / 2,
                                       getHeight() / 2 - h / 2,
                                       w, h);
            dynamicEQPanel->toFront(true);
        }
    }

    // Context panel always visible
    aiTabBtn.setVisible(true);
    semanticTabBtn.setVisible(true);
    sensitivityLabel.setVisible(activeRightTab == 0);
    sensitivityKnob.setVisible(activeRightTab == 0);
    strengthLabel.setVisible(activeRightTab == 0);
    strengthKnob.setVisible(activeRightTab == 0);
    if (activeRightTab != 0)
    {
        sensitivityKnob.setBounds(0, 0, 0, 0);
        strengthKnob.setBounds(0, 0, 0, 0);
    }
    aiProblemPanel->setVisible(activeRightTab == 0);
    semanticPanel->setVisible(activeRightTab == 1);
    // Phase 5: dynamicEQPanel visibility is now driven by DynEQCompactBar
    // (default hidden at construction; toggled via onExpandRequested).

    // --- Left column: band controls (mockup: 12px 16px padding) ---
    bandCol.reduce(16, 12);

    // Row 1: band selector
    auto selectorRow = bandCol.removeFromTop(22);
    {
        auto numArea = selectorRow.removeFromLeft(90);
        numBandsLabel.setBounds(numArea.removeFromLeft(42));
        numBandsCombo.setBounds(numArea.reduced(0, 1));
        selectorRow.removeFromLeft(6);
        bandSelectCombo.setBounds(selectorRow.removeFromLeft(110).reduced(0, 1));
    }
    bandCol.removeFromTop(4);

    // DynEQ controls now integrated in selectedBandPanel — no separate compact bar
    // Full vertical space available for band detail panel
    if (selectedBandPanel)
    {
        selectedBandPanel->setBounds(bandCol);
        selectedBandPanel->setVisible(true);
    }

    // Hide unused items
    dynamicEQMasterPanel->setVisible(false);

    // === SPECTRUM (everything remaining — FULL WIDTH) ===
    bounds.reduce(4, 4);
    spectrumBounds = bounds;
    spectrum->setBounds(spectrumBounds);
    // Wave 4D Fix 2 (Marco from video): symmetric vertical inset so 0 dB
    // sits at the visual centre of the graph. Previous layout was
    // reduced(45, 25) + removeFromBottom(22), giving 25 px top padding vs
    // 47 px bottom padding — the 0 dB line floated ~11 px above centre,
    // making the whole graph look pushed up. New layout: a clean
    // reduced(45, 30) symmetric inset (total 60 px vertical padding, was
    // 72 px asymmetric), which both centres the graph and gives 12 px
    // more breathing room to the curves.
    graphBounds = spectrumBounds.reduced(45, 30);
    graphBounds.removeFromLeft(5);

    updateBandPositions();
}
void AIEqualizerAudioProcessorEditor::timerCallback()
{
    ++timerTickCount;

    // Bug K fix: guard against timer firing during processor teardown
    if (!processor.isProcessorReady())
        return;

    // Phase 7D: single-heartbeat breathing phase for AIBreathingDot.
    // Tribunale Amendment #2 (BINDING): NO scattered timers — phase driven here
    // from the editor's existing 60 Hz timer. 4-second full sine cycle.
    {
        constexpr float kBreathingCycleSeconds = 4.0f;
        constexpr float kTimerHz               = 60.0f; // matches startTimerHz(60) above
        breathingPhase += juce::MathConstants<float>::twoPi
                          / (kBreathingCycleSeconds * kTimerHz);
        if (breathingPhase > juce::MathConstants<float>::twoPi)
            breathingPhase -= juce::MathConstants<float>::twoPi;
        aiBreathingDot.setPhase(breathingPhase);
        if (spectrum)
            spectrum->setAIBreathingPhase(breathingPhase);
    }

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

            // Spectrum is drawn by the software path; the dead GL renderer was
            // removed in GUI-1. The GL context stays attached for GPU compositing.

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

    // Click detector GUI overlay removed — threshold (0.25) was too low for
    // real audio, causing false positives on normal musical transients.
    // The underlying checkClicks() infrastructure in processBlock is retained
    // for automated tests (AntiPopRegressionTest) which use controlled signals.

    // Parameter-driven UI updates (bands, toggles, A/B, quality)
    // Throttled to every 2nd tick (~10Hz) - reduces message thread load in Ableton.
    // Parameter changes are still detected via counter so nothing is lost, just
    // applied slightly later (50ms max delay is imperceptible to the user).
    uint64_t currentChange = processor.getParameterChangeCounter();
    if (currentChange != lastParameterChangeCount && (timerTickCount & 1) == 0)
    {
        lastParameterChangeCount = currentChange;

        int activeBands = processor.getNumActiveBands();
        if (activeBands > 0 && selectedBand >= activeBands)
            selectBand(activeBands - 1);

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

        {
            auto abState = processor.getCurrentABState();
            btnA.setToggleState(abState == AIEqualizerAudioProcessor::ABState::A, juce::dontSendNotification);
            btnB.setToggleState(abState == AIEqualizerAudioProcessor::ABState::B, juce::dontSendNotification);
            btnC.setToggleState(abState == AIEqualizerAudioProcessor::ABState::C, juce::dontSendNotification);
            btnD.setToggleState(abState == AIEqualizerAudioProcessor::ABState::D, juce::dontSendNotification);
        }

        // Sync quality toggle (0 = Zero Latency, 1 = High Quality)
        if (auto* param = processor.getAPVTS().getParameter("qualityMode"))
        {
            float v = param->getValue();
            int mode = static_cast<int>(param->convertFrom0to1(v) + 0.5f);
            qualityBtn.setToggleState(mode == 1, juce::dontSendNotification);
            qualityBtn.setButtonText(mode == 1 ? "HQ" : "ZL");
            // Wave 4A: amber-family visuals when HQ active.
            qualityBtn.setColour(juce::TextButton::buttonColourId,
                                 mode == 1 ? ModernLookAndFeel::Colors::amber.withAlpha(0.18f)
                                           : ModernLookAndFeel::Colors::bgLight);
            qualityBtn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amber);
            qualityBtn.setColour(juce::TextButton::textColourOffId,
                                 mode == 1 ? ModernLookAndFeel::Colors::textBright
                                           : ModernLookAndFeel::Colors::textPrimary);
            qualityBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
        }

        // Sync oversampling cycling button with APVTS
        if (auto* param = processor.getAPVTS().getParameter("oversamplingFactor"))
        {
            int os = static_cast<int>(param->convertFrom0to1(param->getValue()) + 0.5f);
            static const char* osLabels[] = { "OFF", "2x", "4x", "AUTO" };
            oversamplingBtn.setButtonText(osLabels[juce::jlimit(0, 3, os)]);
            bool active = os > 0;
            oversamplingBtn.setColour(juce::TextButton::buttonColourId,
                                      active ? ModernLookAndFeel::Colors::accentBlue.withAlpha(0.18f)
                                             : ModernLookAndFeel::Colors::bgLight);
            oversamplingBtn.setColour(juce::TextButton::textColourOffId,
                                      active ? ModernLookAndFeel::Colors::textBright
                                             : ModernLookAndFeel::Colors::textPrimary);
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

    // Context panel is always visible in bottom layout
    aiProblemPanel->setVisible(tab == 0);
    semanticPanel->setVisible(tab == 1);

    // Update button styles — subtle amber glow when active (matches mockup context-tab.active)
    auto activeCol  = juce::Colour(0xFF232028).interpolatedWith(ModernLookAndFeel::Colors::amber, 0.08f);
    auto inactiveCol = juce::Colour(0xFF222228);
    auto activeText = ModernLookAndFeel::Colors::amber;
    auto inactiveText = juce::Colour(0xFF56544E);

    aiTabBtn.setColour(juce::TextButton::buttonColourId, tab == 0 ? activeCol : inactiveCol);
    aiTabBtn.setColour(juce::TextButton::textColourOffId, tab == 0 ? activeText : inactiveText);
    semanticTabBtn.setColour(juce::TextButton::buttonColourId, tab == 1 ? activeCol : inactiveCol);
    semanticTabBtn.setColour(juce::TextButton::textColourOffId, tab == 1 ? activeText : inactiveText);

    // Re-layout: sensitivity knobs visibility depends on active tab
    resized();
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
