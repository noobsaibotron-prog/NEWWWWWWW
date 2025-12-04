#include "PluginEditor.h"

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
            if (bandIndex >= AIEqualizerAudioProcessor::numBands) break;
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
    
    // Selected band detail panel
    selectedBandPanel = std::make_unique<BandControlPanel>(0, processor.getAPVTS());
    addAndMakeVisible(*selectedBandPanel);
    
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
    logoLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    logoLabel.setColour(juce::Label::textColourId, ModernLookAndFeel::Colors::textPrimary);
    addAndMakeVisible(logoLabel);
    
    subtitleLabel.setText("Intelligent Equalizer", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(10.0f));
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
    sensitivityLabel.setFont(juce::Font(9.0f));
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
    strengthLabel.setFont(juce::Font(9.0f));
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
    
    // Output knob with tooltip
    outLabel.setText("OUT GAIN", juce::dontSendNotification);
    outLabel.setFont(juce::Font(9.0f));
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
    for (int i = 0; i < AIEqualizerAudioProcessor::numBands; ++i)
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
    header.removeFromLeft(20);
    
    // Pre/Post
    btnPre.setBounds(header.removeFromLeft(40).reduced(2));
    header.removeFromLeft(4);
    btnPost.setBounds(header.removeFromLeft(45).reduced(2));
    
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
    
    // Logo section
    auto logoArea = cpanel.removeFromLeft(100);
    logoLabel.setBounds(logoArea.removeFromTop(28));
    subtitleLabel.setBounds(logoArea.removeFromTop(16));
    cpanel.removeFromLeft(10);
    
    // Band toggles
    auto toggleArea = cpanel.removeFromLeft(220);
    auto toggleRow = toggleArea.removeFromTop(26);
    for (int i = 0; i < 4; ++i)
        bandToggles[i].setBounds(toggleRow.removeFromLeft(52).reduced(2));
    toggleRow = toggleArea.removeFromTop(26);
    for (int i = 4; i < 8; ++i)
        bandToggles[i].setBounds(toggleRow.removeFromLeft(52).reduced(2));
    cpanel.removeFromLeft(15);
    
    // AI Knobs
    int knobW = 65;
    int knobH = 80;
    
    auto sensArea = cpanel.removeFromLeft(knobW);
    sensitivityLabel.setBounds(sensArea.removeFromTop(14));
    sensitivityKnob.setBounds(sensArea.removeFromTop(knobH));
    
    auto strArea = cpanel.removeFromLeft(knobW);
    strengthLabel.setBounds(strArea.removeFromTop(14));
    strengthKnob.setBounds(strArea.removeFromTop(knobH));
    
    cpanel.removeFromLeft(8);
    autoBtn.setBounds(cpanel.removeFromLeft(45).removeFromTop(26).reduced(2));
    
    // Selected band panel (center-right of control panel)
    cpanel.removeFromLeft(15);
    auto bandDetailArea = cpanel.removeFromLeft(160);
    selectedBandPanel->setBounds(bandDetailArea);
    
    // Dynamic EQ Master Panel (next to band panel)
    cpanel.removeFromLeft(10);
    auto dynMasterArea = cpanel.removeFromLeft(90);
    dynamicEQMasterPanel->setBounds(dynMasterArea);
    
    // Output (right side)
    auto outArea = cpanel.removeFromRight(70);
    outLabel.setBounds(outArea.removeFromTop(14));
    outKnob.setBounds(outArea.removeFromTop(knobH));
    
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
    for (int i = 0; i < bands.size(); ++i)
    {
        auto state = processor.getBandState(i);
        EQBandControl::BandParameters p;
        p.frequency = state.frequency;
        p.gain = state.gain;
        p.q = state.q;
        p.filterType = state.type;
        p.enabled = state.enabled;
        bands[i]->setParameters(p);
        bandToggles[i].setToggleState(state.enabled, juce::dontSendNotification);
    }
    updateBandPositions();
    
    // A/B state
    bool isA = processor.getCurrentABState() == AIEqualizerAudioProcessor::ABState::A;
    btnA.setToggleState(isA, juce::dontSendNotification);
    btnB.setToggleState(!isA, juce::dontSendNotification);
}

void AIEqualizerAudioProcessorEditor::updateBandPositions()
{
    if (graphBounds.isEmpty()) return;
    
    for (int i = 0; i < bands.size(); ++i)
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
    if (bandIndex < 0 || bandIndex >= AIEqualizerAudioProcessor::numBands)
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
