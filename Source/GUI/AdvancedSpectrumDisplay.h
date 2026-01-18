#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"
#include <array>
#include <cmath>
#include <limits>

//==============================================================================
/**
 * TDR Nova Style Spectrum Display
 * 
 * - Blue spectrum with gradient fill
 * - Subtle grid lines
 * - White/gray EQ curve
 * - Clean frequency/dB labels
 */
class AdvancedSpectrumDisplay : public juce::Component, public juce::Timer
{
public:
    // Callbacks for band interaction
    std::function<void(float)> onFrequencySelected;
    std::function<void(int)> onBandSelected;                          // Single click on band
    std::function<void(int, float, float)> onBandCreatedOrActivated;  // Double click: band index, freq, gain
    std::function<void(int, float, float, float)> onBandDragged;      // Drag: band, freq, gain, q

    explicit AdvancedSpectrumDisplay(AIEqualizerAudioProcessor& p) : processor(p)
    {
        setOpaque(true);
        startTimerHz(60);
        smoothedSpectrum.resize(512, spectrumMinDb);
        frozenSpectrum.resize(512, spectrumMinDb);
        capturedSpectrum.resize(512, spectrumMinDb);
        
        // Initialize band colors (repeat palette for all supported bands)
        constexpr int paletteSize = 8;
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            bandColors[i] = ModernLookAndFeel::Colors::getBandColor(i % paletteSize);
        }
        
        // Freeze button
        freezeButton.setButtonText("FREEZE");
        freezeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        freezeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF4A90D9));
        freezeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        freezeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        freezeButton.setClickingTogglesState(true);
        freezeButton.onClick = [this]() {
            isFrozen = freezeButton.getToggleState();
            if (isFrozen) {
                frozenSpectrum = smoothedSpectrum;
            }
            refreshPeaks();
        };
        addAndMakeVisible(freezeButton);
        
        // Capture button
        captureButton.setButtonText("CAPTURE");
        captureButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A2A));
        captureButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        captureButton.onClick = [this]() {
            capturedSpectrum = smoothedSpectrum;
            hasCaptured = true;
            captureButton.setButtonText("CAPTURED!");
            startTimer(100); // Will reset button text
        };
        addAndMakeVisible(captureButton);
        
        // Show Captured toggle
        showCapturedButton.setButtonText("SHOW CAPT");
        showCapturedButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        showCapturedButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFE6A23C));
        addAndMakeVisible(showCapturedButton);
        
        // Clear button
        clearButton.setButtonText("CLEAR");
        clearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A3A3A));
        clearButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
        clearButton.onClick = [this]() {
            hasCaptured = false;
            isFrozen = false;
            freezeButton.setToggleState(false, juce::dontSendNotification);
            showCapturedButton.setToggleState(false, juce::dontSendNotification);
        };
        addAndMakeVisible(clearButton);
    }
    
    ~AdvancedSpectrumDisplay() override { stopTimer(); }
    
    // Public API for freeze/capture
    void freeze() { freezeButton.setToggleState(true, juce::sendNotification); }
    void unfreeze() { freezeButton.setToggleState(false, juce::sendNotification); }
    bool getIsFrozen() const { return isFrozen; }
    void capture() { captureButton.triggerClick(); }
    bool getHasCaptured() const { return hasCaptured; }
    const std::vector<float>& getCapturedSpectrum() const { return capturedSpectrum; }
    
    // Band selection API
    void setSelectedBand(int band) { selectedBandIndex = band; repaint(); }
    int getSelectedBand() const { return selectedBandIndex; }
    
    //==========================================================================
    // AI Problem Highlight API - Shows detected problem on spectrum
    //==========================================================================
    struct ProblemHighlight
    {
        float frequency = 0.0f;
        float q = 1.0f;
        float severity = 0.0f;
        AIEngine::ProblemType type = AIEngine::ProblemType::None;
        bool active = false;
        int fadeCounter = 0;  // For fade-out animation
    };
    
    void highlightProblem(float frequency, float q, float severity, AIEngine::ProblemType type)
    {
        currentHighlight.frequency = frequency;
        currentHighlight.q = q;
        currentHighlight.severity = severity;
        currentHighlight.type = type;
        currentHighlight.active = true;
        currentHighlight.fadeCounter = 180;  // 3 seconds at 60fps
        repaint();
    }
    
    void clearHighlight()
    {
        currentHighlight.active = false;
        currentHighlight.fadeCounter = 0;
        repaint();
    }
    
    ProblemHighlight currentHighlight;
    
    // Display range for spectrum (helps visual width/height of the curve)
    static constexpr float spectrumMinDb = -90.0f;
    static constexpr float spectrumMaxDb = 12.0f;

    struct SpectrumPeak
    {
        float frequency = 0.0f;
        float magnitudeDb = spectrumMinDb;
        float suggestedGain = 0.0f;
        AIEngine::ProblemType aiType = AIEngine::ProblemType::None;
        bool fromAI = false;
    };

    void paint(juce::Graphics& g) override
    {
        g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::highResamplingQuality);
        g.reduceClipRegion(getLocalBounds()); // ensure crisp edges

        auto bounds = getLocalBounds().toFloat();
        
        // Background
        g.setColour(ModernLookAndFeel::Colors::bgDark);
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Graph area
        graphBounds = bounds.reduced(45, 25);
        graphBounds.removeFromBottom(22);
        graphBounds.removeFromLeft(5);
        
        drawGrid(g);
        drawSpectrum(g);
        drawSpectrumGrab(g);
        drawProblemHighlight(g);  // Draw AI problem highlight BEFORE EQ curve
        drawEQCurve(g);
        drawEQBands(g);      // Draw EQ band controls on spectrum
        drawAIMarkers(g);
        drawLabels(g);
        
        if (hoverX >= 0) drawHover(g);
        
        // Border
        g.setColour(ModernLookAndFeel::Colors::bgLighter);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    }

    void resized() override 
    {
        auto bounds = getLocalBounds();
        
        // Buttons in top-right corner
        int btnW = 65, btnH = 20, gap = 4;
        int startX = bounds.getRight() - (btnW * 4 + gap * 3) - 10;
        int startY = bounds.getY() + 5;
        
        freezeButton.setBounds(startX, startY, btnW, btnH);
        captureButton.setBounds(startX + btnW + gap, startY, btnW, btnH);
        showCapturedButton.setBounds(startX + (btnW + gap) * 2, startY, btnW + 10, btnH);
        clearButton.setBounds(startX + (btnW + gap) * 2 + btnW + 10 + gap, startY, 50, btnH);
    }
    
    void timerCallback() override 
    { 
        // SAFETY: Skip processing if processor not ready
        if (!processor.isProcessorReady())
        {
            repaint();  // Still repaint background
            return;
        }
        
        // Reset capture button text after showing "CAPTURED!"
        if (captureButton.getButtonText() == "CAPTURED!") {
            if (++captureTextTimer > 10) {
                captureButton.setButtonText("CAPTURE");
                captureTextTimer = 0;
            }
        }
        
        if (!isFrozen) {
            updateSmoothedSpectrum();
            refreshPeaks();
        }
        repaint(); 
    }

    void mouseMove(const juce::MouseEvent& e) override 
    { 
        hoverX = e.x; 
        hoverY = e.y;
        
        // Check if hovering over a band
        hoveredBandIndex = getBandAtPosition(e.position);
        hoveredPeakIndex = getPeakAtPosition(e.position);
        
        // Change cursor when over a band
        if (hoveredBandIndex >= 0 || hoveredPeakIndex >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseExit(const juce::MouseEvent&) override 
    { 
        hoverX = -1; 
        hoveredBandIndex = -1;
        hoveredPeakIndex = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseDown(const juce::MouseEvent& e) override 
    {
        if (e.mods.isPopupMenu())
        {
            showContextMenu(e.getPosition());
            return;
        }

        if (!graphBounds.contains(e.position))
            return;
        
        // Check if clicking on a band
        int clickedBand = getBandAtPosition(e.position);
        
        if (clickedBand >= 0)
        {
            // Select this band
            selectedBandIndex = clickedBand;
            isDraggingBand = true;
            dragStartPos = e.position;
            
            auto state = processor.getBandState(clickedBand);
            dragStartFreq = state.frequency;
            dragStartGain = state.gain;
            dragStartQ = state.q;
            
            if (onBandSelected)
                onBandSelected(clickedBand);
            
            repaint();
        }
        else
        {
            if (hoveredPeakIndex >= 0 && hoveredPeakIndex < static_cast<int>(detectedPeaks.size()))
            {
                handlePeakClick(detectedPeaks[hoveredPeakIndex]);
                return;
            }

            // Clicked on empty space - just report frequency
            if (onFrequencySelected)
                onFrequencySelected(quantizeFrequency(xToFreq((float)e.x)));
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isDraggingBand && selectedBandIndex >= 0)
        {
            auto delta = e.position - dragStartPos;
            
            // Calculate new frequency (horizontal, log scale)
            float freqMult = std::pow(2.0f, delta.x / 100.0f);
            float newFreq = juce::jlimit(20.0f, 20000.0f, dragStartFreq * freqMult);
            
            // Calculate new gain (vertical)
            float newGain = juce::jlimit(-24.0f, 24.0f, dragStartGain - delta.y / 8.0f);
            
            // Q with shift modifier
            float newQ = dragStartQ;
            if (e.mods.isShiftDown())
            {
                float qMult = std::pow(2.0f, -delta.y / 150.0f);
                newQ = juce::jlimit(0.1f, 10.0f, dragStartQ * qMult);
            }
            
            // Update processor
            auto state = processor.getBandState(selectedBandIndex);
            state.frequency = newFreq;
            state.gain = newGain;
            state.q = newQ;
            processor.setBandState(selectedBandIndex, state);
            
            // Notify callback
            if (onBandDragged)
                onBandDragged(selectedBandIndex, newFreq, newGain, newQ);
            
            repaint();
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingBand = false;
    }
    
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!graphBounds.contains(e.position))
            return;
        
        float freq = quantizeFrequency(xToFreq((float)e.x));
        float gain = yToGain((float)e.y);
        
        // Check if double-clicking on existing band -> reset its gain
        int clickedBand = getBandAtPosition(e.position);
        if (clickedBand >= 0)
        {
            // Reset gain to 0
            auto state = processor.getBandState(clickedBand);
            state.gain = 0.0f;
            processor.setBandState(clickedBand, state);
            
            selectedBandIndex = clickedBand;
            if (onBandSelected)
                onBandSelected(clickedBand);
        }
        else
        {
            // Double-click on empty space -> activate the best available band
            // Find band with lowest gain (most "unused")
            int targetBand = -1;
            float minGain = std::numeric_limits<float>::max();

            // Use the processor's configured active band count
            int numBands = processor.getNumActiveBands();
            for (int i = 0; i < numBands; ++i)
            {
                auto state = processor.getBandState(i);
                if (std::abs(state.gain) < minGain)
                {
                    minGain = std::abs(state.gain);
                    targetBand = i;
                }
            }
            
            if (targetBand >= 0)
            {
                // Configure this band at the clicked position
                auto state = processor.getBandState(targetBand);
                state.frequency = freq;
                state.gain = gain;
                state.enabled = true;
                processor.setBandState(targetBand, state);
                
                selectedBandIndex = targetBand;
                
                if (onBandCreatedOrActivated)
                    onBandCreatedOrActivated(targetBand, freq, gain);
                
                if (onBandSelected)
                    onBandSelected(targetBand);
            }
        }
        
        repaint();
    }
    
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // If hovering over a band, adjust its Q
        int band = getBandAtPosition(e.position);
        if (band >= 0)
        {
            auto state = processor.getBandState(band);
            state.q = juce::jlimit(0.1f, 10.0f, state.q * (1.0f + wheel.deltaY * 0.3f));
            processor.setBandState(band, state);
            
            if (onBandDragged)
                onBandDragged(band, state.frequency, state.gain, state.q);
            
            repaint();
        }
    }

private:
    float getAnalyzerSlopeDbPerOct() const
    {
        // New: analyzerSlope choice parameter (0=Flat,1=3dB,2=4.5dB)
        if (auto* p = processor.getAPVTS().getRawParameterValue("analyzerSlope"))
        {
            const int v = static_cast<int>(p->load());
            if (v == 1) return 3.0f;
            if (v == 2) return 4.5f;
            return 0.0f;
        }
        // Backward compatibility: analyzerTilt bool toggles 4.5 dB/oct
        if (auto* tilt = processor.getAPVTS().getRawParameterValue("analyzerTilt"))
            return (tilt->load() > 0.5f) ? 4.5f : 0.0f;
        return 4.5f; // default visual
    }

    bool isPianoRollEnabled() const
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue("pianoRollOverlay"))
            return p->load() > 0.5f;
        return false;
    }

    bool isDeltaEnabled() const
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue("showDeltaSpectrum"))
            return p->load() > 0.5f;
        return false;
    }

    float getPeakHoldSeconds() const
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue("analyzerPeakHold"))
            return p->load();
        return 2.0f;
    }

    float getPeakDecayDbPerSec() const
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue("analyzerPeakDecay"))
            return p->load();
        return 20.0f;
    }

    float applyTilt(float db, float freq) const
    {
        float slope = getAnalyzerSlopeDbPerOct();
        if (std::abs(slope) < 0.01f)
            return db;
        float safeFreq = juce::jmax(20.0f, freq);
        float octaves = static_cast<float>(std::log2(static_cast<double>(safeFreq) / 1000.0));
        return static_cast<float>(db + octaves * slope);
    }

    float quantizeFrequency(float freq) const
    {
        if (!isPianoRollEnabled())
            return freq;
        float clamped = juce::jlimit(20.0f, 20000.0f, freq);
        float midi = 69.0f + 12.0f * static_cast<float>(std::log2(static_cast<double>(clamped) / 440.0));
        int rounded = static_cast<int>(std::round(midi));
        rounded = juce::jlimit(0, 127, rounded);
        return static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(rounded));
    }

    juce::String getNoteName(float freq) const
    {
        int midi = static_cast<int>(std::round(69.0f + 12.0f * static_cast<float>(std::log2(static_cast<double>(juce::jlimit(20.0f, 20000.0f, freq)) / 440.0))));
        midi = juce::jlimit(0, 127, midi);
        return juce::MidiMessage::getMidiNoteName(midi, true, true, 3);
    }

    void refreshPeaks()
    {
        const auto& source = (isFrozen && !frozenSpectrum.empty()) ? frozenSpectrum : smoothedSpectrum;
        if (source.size() < 5 || graphBounds.isEmpty())
        {
            detectedPeaks.clear();
            hoveredPeakIndex = -1;
            return;
        }

        std::vector<SpectrumPeak> rawPeaks;
        const size_t size = source.size();
        const int step = 2;
        for (size_t i = 2; i + 2 < size; i += step)
        {
            float v = source[i];
            if (v < spectrumMinDb + 6.0f)
                continue;

            if (v > source[i - 1] + 1.5f && v > source[i + 1] + 1.5f)
            {
                float x = graphBounds.getX() + static_cast<float>(i);
                float freq = xToFreq(x);
                SpectrumPeak peak;
                peak.frequency = freq;
                peak.magnitudeDb = applyTilt(v, freq);
                rawPeaks.push_back(peak);
            }
        }

        std::sort(rawPeaks.begin(), rawPeaks.end(),
                  [](const SpectrumPeak& a, const SpectrumPeak& b) { return a.magnitudeDb > b.magnitudeDb; });

        const size_t maxPeaks = 10;
        if (rawPeaks.size() > maxPeaks)
            rawPeaks.resize(maxPeaks);

        detectedPeaks = rawPeaks;
        if (hoveredPeakIndex >= static_cast<int>(detectedPeaks.size()))
            hoveredPeakIndex = -1;

        auto corrections = processor.getAIEngine().getPendingCorrections();
        for (auto& peak : detectedPeaks)
        {
            for (const auto& c : corrections)
            {
                float ratio = std::abs(std::log2(peak.frequency / juce::jmax(20.0f, c.frequency)));
                if (ratio < 0.08f)
                {
                    peak.fromAI = true;
                    peak.aiType = c.type;
                    peak.suggestedGain = c.suggestedGain;
                    break;
                }
            }
        }
    }

    int getPeakAtPosition(juce::Point<float> pos) const
    {
        if (detectedPeaks.empty())
            return -1;

        for (size_t i = 0; i < detectedPeaks.size(); ++i)
        {
            const auto& peak = detectedPeaks[i];
            float x = freqToX(peak.frequency);
            float y = dbToY(peak.magnitudeDb);
            float dist = std::sqrt((pos.x - x) * (pos.x - x) + (pos.y - y) * (pos.y - y));
            if (dist < 12.0f)
                return static_cast<int>(i);
        }
        return -1;
    }

    void handlePeakClick(const SpectrumPeak& peak)
    {
        float targetFreq = quantizeFrequency(peak.frequency);
        float targetGain = std::abs(peak.suggestedGain) > 0.01f ? peak.suggestedGain : 0.0f;
        float targetQ = peak.fromAI ? 2.5f : 2.0f;

        int targetBand = -1;
        float minGain = std::numeric_limits<float>::max();
        int numBands = processor.getNumActiveBands();
        for (int i = 0; i < numBands; ++i)
        {
            auto state = processor.getBandState(i);
            if (std::abs(state.gain) < minGain)
            {
                minGain = std::abs(state.gain);
                targetBand = i;
            }
        }

        if (targetBand >= 0)
        {
            auto state = processor.getBandState(targetBand);
            state.frequency = targetFreq;
            state.gain = targetGain;
            state.q = targetQ;
            state.enabled = true;
            processor.setBandState(targetBand, state);

            selectedBandIndex = targetBand;

            if (onBandCreatedOrActivated)
                onBandCreatedOrActivated(targetBand, targetFreq, targetGain);
            if (onBandSelected)
                onBandSelected(targetBand);

            repaint();
        }
    }

    void setBoolParameter(const juce::String& paramID, bool value)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    }

    void setChoiceParameter(const juce::String& paramID, int index)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            auto norm = p->convertTo0to1(static_cast<float>(index));
            p->beginChangeGesture();
            p->setValueNotifyingHost(norm);
            p->endChangeGesture();
        }
    }

    void showContextMenu(juce::Point<int> pos)
    {
        juce::PopupMenu menu;

        // Defensive: guard against missing params (should always exist)
        auto* preParam = processor.getAPVTS().getRawParameterValue("showPreSpectrum");
        auto* postParam = processor.getAPVTS().getRawParameterValue("showPostSpectrum");
        bool showPre = preParam ? preParam->load() > 0.5f : false;
        bool showPost = postParam ? postParam->load() > 0.5f : false;
        bool showDelta = isDeltaEnabled();
        const float slope = getAnalyzerSlopeDbPerOct();
        bool pianoRoll = isPianoRollEnabled();

        menu.addItem(1, "Input Spectrum (Pre)", true, showPre);
        menu.addItem(2, "Output Spectrum (Post)", true, showPost);
        menu.addItem(3, "Delta (Post - Pre)", true, showDelta);
        menu.addSeparator();

        juce::PopupMenu fftMenu;
        auto resParam = processor.getAPVTS().getRawParameterValue("analyzerResolution");
        int resIdx = resParam ? static_cast<int>(resParam->load()) : 2;
        fftMenu.addItem(10, "Low (1024)", true, resIdx == 0);
        fftMenu.addItem(11, "Medium (2048)", true, resIdx == 1);
        fftMenu.addItem(12, "High (4096)", true, resIdx == 2);
        fftMenu.addItem(13, "Maximum (8192)", true, resIdx == 3);
        menu.addSubMenu("FFT Resolution", fftMenu);

        juce::PopupMenu speedMenu;
        auto speedParam = processor.getAPVTS().getRawParameterValue("analyzerSpeed");
        int speedIdx = speedParam ? static_cast<int>(speedParam->load()) : 1;
        speedMenu.addItem(20, "Fast", true, speedIdx == 0);
        speedMenu.addItem(21, "Medium", true, speedIdx == 1);
        speedMenu.addItem(22, "Slow", true, speedIdx == 2);
        menu.addSubMenu("Analyzer Speed", speedMenu);

        menu.addSeparator();
        menu.addItem(30, "Slope: Flat", true, std::abs(slope) < 0.01f);
        menu.addItem(31, "Slope: 3 dB/oct", true, std::abs(slope - 3.0f) < 0.01f);
        menu.addItem(32, "Slope: 4.5 dB/oct", true, std::abs(slope - 4.5f) < 0.01f);
        menu.addItem(40, "Piano Roll Overlay", true, pianoRoll);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({pos, {1, 1}}),
            [this, showPre, showPost, showDelta, pianoRoll](int result)
            {
                switch (result)
                {
                    case 1: setBoolParameter("showPreSpectrum", !showPre); break;
                    case 2: setBoolParameter("showPostSpectrum", !showPost); break;
                    case 3: setBoolParameter("showDeltaSpectrum", !showDelta); break;
                    case 10: setChoiceParameter("analyzerResolution", 0); break;
                    case 11: setChoiceParameter("analyzerResolution", 1); break;
                    case 12: setChoiceParameter("analyzerResolution", 2); break;
                    case 13: setChoiceParameter("analyzerResolution", 3); break;
                    case 20: setChoiceParameter("analyzerSpeed", 0); break;
                    case 21: setChoiceParameter("analyzerSpeed", 1); break;
                    case 22: setChoiceParameter("analyzerSpeed", 2); break;
                    case 30: setChoiceParameter("analyzerSlope", 0); break;
                    case 31: setChoiceParameter("analyzerSlope", 1); break;
                    case 32: setChoiceParameter("analyzerSlope", 2); break;
                    case 40: setBoolParameter("pianoRollOverlay", !pianoRoll); break;
                    default: break;
                }
            });
    }

    void drawSpectrumGrab(juce::Graphics& g)
    {
        bool showPre = processor.getAPVTS().getRawParameterValue("showPreSpectrum")->load() > 0.5f;
        if (!showPre || detectedPeaks.empty())
            return;

        for (size_t i = 0; i < detectedPeaks.size(); ++i)
        {
            const auto& peak = detectedPeaks[i];
            float x = freqToX(peak.frequency);
            float y = dbToY(peak.magnitudeDb);

            if (x < graphBounds.getX() || x > graphBounds.getRight())
                continue;

            juce::Colour base = peak.fromAI ? ModernLookAndFeel::Colors::accentOrange
                                            : ModernLookAndFeel::Colors::textSecondary;

            g.setColour(base.withAlpha(0.35f));
            g.drawVerticalLine(static_cast<int>(x), graphBounds.getY(), graphBounds.getBottom());

            g.setColour(base);
            g.fillEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f);

            if (static_cast<int>(i) == hoveredPeakIndex)
            {
                juce::String freqStr = peak.frequency >= 1000.0f
                    ? juce::String(peak.frequency / 1000.0f, 2) + " kHz"
                    : juce::String(peak.frequency, 1) + " Hz";
                auto note = getNoteName(peak.frequency);
                juce::String label = freqStr + "  (" + note + ")";
                if (peak.fromAI)
                    label += " • AI DETECT";

                int w = 170;
                int h = 24;
                int tx = static_cast<int>(juce::jlimit(graphBounds.getX(), graphBounds.getRight() - w, x - w * 0.5f));
                int ty = static_cast<int>(y) - 32;
                if (ty < graphBounds.getY() + 4) ty = static_cast<int>(y) + 12;

                g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(0.95f));
                g.fillRoundedRectangle((float)tx, (float)ty, (float)w, (float)h, 4.0f);
                g.setColour(base);
                g.drawRoundedRectangle((float)tx, (float)ty, (float)w, (float)h, 4.0f, 1.4f);

                g.setColour(ModernLookAndFeel::Colors::textBright);
                g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
                g.drawText(label, tx, ty, w, h, juce::Justification::centred);
            }
        }
    }

    void drawPianoRollOverlay(juce::Graphics& g)
    {
        auto area = juce::Rectangle<float>(graphBounds.getX(), graphBounds.getBottom(), graphBounds.getWidth(), 18.0f);
        g.setColour(ModernLookAndFeel::Colors::bgLight);
        g.fillRect(area);

        auto isWhite = [](int midi)
        {
            int n = midi % 12;
            return n == 0 || n == 2 || n == 4 || n == 5 || n == 7 || n == 9 || n == 11;
        };

        for (int midi = 36; midi <= 96; ++midi)
        {
            float f1 = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midi));
            float f2 = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midi + 1));
            float x1 = freqToX(f1);
            float x2 = freqToX(f2);
            if (x2 < graphBounds.getX() || x1 > graphBounds.getRight())
                continue;

            float w = x2 - x1;
            juce::Rectangle<float> keyRect(x1, area.getY(), w, area.getHeight());
            g.setColour(isWhite(midi) ? ModernLookAndFeel::Colors::bgDark.brighter(0.08f)
                                      : ModernLookAndFeel::Colors::bgDark);
            g.fillRect(keyRect);

            g.setColour(ModernLookAndFeel::Colors::bgLighter);
            g.drawLine(x1, area.getY(), x1, area.getBottom(), 0.5f);

            if (midi % 12 == 0)
            {
                juce::String name = juce::MidiMessage::getMidiNoteName(midi, true, true, 3);
                g.setColour(ModernLookAndFeel::Colors::textMuted);
                g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
                g.drawText(name, (int)x1 - 8, (int)area.getY(), 40, (int)area.getHeight(),
                           juce::Justification::centredLeft, false);
            }
        }
    }

    void updateSmoothedSpectrum()
    {
        const auto& raw = processor.getSpectrumAnalyzer().getSmoothedSpectrum();
        if (raw.empty()) return;
        
        const size_t w = static_cast<size_t>(juce::jmax(100.0f, graphBounds.getWidth()));
        if (smoothedSpectrum.size() < w) smoothedSpectrum.resize(w, spectrumMinDb); // grow only (no per-frame realloc)
        if (peakHold.size() < w) peakHold.resize(w, spectrumMinDb);
        if (peakTimers.size() < w) peakTimers.resize(w, 0.0f);
        
        const float dt = 1.0f / 60.0f; // timer at 60 Hz
        const float holdSec = getPeakHoldSeconds();
        const float decayDbPerSec = getPeakDecayDbPerSec();
        
        for (size_t i = 0; i < w; ++i)
        {
            float freq = xToFreq(graphBounds.getX() + (float)i);
            int bin = processor.getSpectrumAnalyzer().getBinForFrequency(freq);
            float db = (bin >= 0 && bin < (int)raw.size()) ? raw[bin] : spectrumMinDb;
            smoothedSpectrum[i] = smoothedSpectrum[i] * 0.75f + db * 0.25f;

            // Peak-hold tracking
            if (smoothedSpectrum[i] >= peakHold[i])
            {
                peakHold[i] = smoothedSpectrum[i];
                peakTimers[i] = 0.0f;
            }
            else
            {
                peakTimers[i] += dt;
                if (peakTimers[i] > holdSec)
                {
                    peakHold[i] = juce::jmax(peakHold[i] - decayDbPerSec * dt, spectrumMinDb);
                }
            }
        }
    }

    void drawGrid(juce::Graphics& g)
    {
        // Vertical frequency lines
        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (float f : freqs)
        {
            float x = freqToX(f);
            if (x < graphBounds.getX() || x > graphBounds.getRight()) continue;
            
            bool major = (f == 100 || f == 1000 || f == 10000);
            g.setColour(major ? ModernLookAndFeel::Colors::gridMajor 
                              : ModernLookAndFeel::Colors::grid);
            g.drawVerticalLine((int)x, graphBounds.getY(), graphBounds.getBottom());
        }
        
        // Horizontal dB lines
        for (float db = spectrumMinDb; db <= spectrumMaxDb; db += 12.0f)
        {
            float y = dbToY(db);
            bool isZero = std::abs(db) < 0.01f;
            g.setColour(isZero ? ModernLookAndFeel::Colors::textMuted.withAlpha(0.4f)
                               : ModernLookAndFeel::Colors::grid);
            g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
        }
    }

    void drawSpectrum(juce::Graphics& g)
    {
        bool showPre = processor.getAPVTS().getRawParameterValue("showPreSpectrum")->load() > 0.5f;
        bool showPost = processor.getAPVTS().getRawParameterValue("showPostSpectrum")->load() > 0.5f;
        bool showDelta = isDeltaEnabled();
        bool showCaptured = showCapturedButton.getToggleState() && hasCaptured;

        const auto& preBuffer = (isFrozen && !frozenSpectrum.empty()) ? frozenSpectrum : smoothedSpectrum;
        if (preBuffer.empty() && !showCaptured && !(showPost && !isFrozen))
            return;

        //----------------------------------------------------------------------
        // Captured spectrum (orange dashed)
        //----------------------------------------------------------------------
        if (showCaptured && !capturedSpectrum.empty())
        {
            juce::Path capturedPath;
            bool started = false;

            for (size_t i = 0; i < capturedSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float freq = xToFreq(x);
                float y = dbToY(applyTilt(capturedSpectrum[i], freq));
                if (!started) { capturedPath.startNewSubPath(x, y); started = true; }
                else capturedPath.lineTo(x, y);
            }

            g.setColour(juce::Colour(0xFFE6A23C).withAlpha(0.6f));
            float dashLengths[] = { 4.0f, 4.0f };
            juce::PathStrokeType stroke(1.5f);
            stroke.createDashedStroke(capturedPath, capturedPath, dashLengths, 2);
            g.strokePath(capturedPath, stroke);
        }

        //----------------------------------------------------------------------
        // Frozen spectrum (blue indicator)
        //----------------------------------------------------------------------
        if (isFrozen && !frozenSpectrum.empty())
        {
            juce::Path frozenFillPath;
            frozenFillPath.startNewSubPath(graphBounds.getX(), graphBounds.getBottom());

            for (size_t i = 0; i < frozenSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float freq = xToFreq(x);
                float y = dbToY(applyTilt(frozenSpectrum[i], freq));
                frozenFillPath.lineTo(x, y);
            }
            frozenFillPath.lineTo(graphBounds.getRight(), graphBounds.getBottom());
            frozenFillPath.closeSubPath();

            juce::ColourGradient frozenGrad(
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.35f), 0, graphBounds.getY(),
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.03f), 0, graphBounds.getBottom(), false);
            g.setGradientFill(frozenGrad);
            g.fillPath(frozenFillPath);

            juce::Path frozenLinePath;
            bool started = false;
            for (size_t i = 0; i < frozenSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float freq = xToFreq(x);
                float y = dbToY(applyTilt(frozenSpectrum[i], freq));
                if (!started) { frozenLinePath.startNewSubPath(x, y); started = true; }
                else frozenLinePath.lineTo(x, y);
            }

            g.setColour(ModernLookAndFeel::Colors::accentBlue);
            g.strokePath(frozenLinePath, juce::PathStrokeType(2.0f));

            g.setColour(ModernLookAndFeel::Colors::accentBlue);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
            g.drawText("FROZEN",
                      static_cast<int>(graphBounds.getX() + 10.0f),
                      static_cast<int>(graphBounds.getY() + 30.0f),
                      80, 20,
                      juce::Justification::centredLeft);

            g.fillEllipse(graphBounds.getRight() - 16.0f, graphBounds.getY() + 10.0f, 8.0f, 8.0f);
        }

        const size_t usable = std::min(preBuffer.size(), static_cast<size_t>(std::max(0, (int)graphBounds.getWidth())));

        //----------------------------------------------------------------------
        // Pre (input) - grey behind
        //----------------------------------------------------------------------
        if (showPre && usable > 4)
        {
            juce::Path fillPath;
            fillPath.startNewSubPath(graphBounds.getX(), graphBounds.getBottom());

            juce::Path linePath;
            bool started = false;

            for (size_t i = 0; i < usable; ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float freq = xToFreq(x);
                float y = dbToY(applyTilt(preBuffer[i], freq));
                fillPath.lineTo(x, y);

                if (!started) { linePath.startNewSubPath(x, y); started = true; }
                else linePath.lineTo(x, y);
            }
            fillPath.lineTo(graphBounds.getRight(), graphBounds.getBottom());
            fillPath.closeSubPath();

            juce::ColourGradient fillGrad(
                ModernLookAndFeel::Colors::textSecondary.withAlpha(0.28f), 0, graphBounds.getY(),
                ModernLookAndFeel::Colors::textSecondary.withAlpha(0.05f), 0, graphBounds.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);

            g.setColour(ModernLookAndFeel::Colors::textSecondary.withAlpha(0.55f));
            g.strokePath(linePath, juce::PathStrokeType(1.4f));
        }

        //----------------------------------------------------------------------
        // Post (output) - coloured front line
        //----------------------------------------------------------------------
        if (showPost && !isFrozen)
        {
            const auto& postRaw = processor.getPostEQAnalyzer().getSmoothedSpectrum();
            if (!postRaw.empty())
            {
                juce::Path postPath;
                bool started = false;

                for (size_t i = 0; i < usable; ++i)
                {
                    float x = graphBounds.getX() + (float)i;
                    float freq = xToFreq(x);
                    int bin = processor.getPostEQAnalyzer().getBinForFrequency(freq);
                    float db = (bin >= 0 && bin < (int)postRaw.size()) ? postRaw[bin] : spectrumMinDb;
                    float y = dbToY(applyTilt(db, freq));

                    if (!started) { postPath.startNewSubPath(x, y); started = true; }
                    else postPath.lineTo(x, y);
                }

                juce::Colour front = ModernLookAndFeel::Colors::accentGreen.brighter(0.05f);
                juce::ColourGradient postGrad(
                    ModernLookAndFeel::Colors::accentYellow.withAlpha(0.85f), 0, graphBounds.getY(),
                    front.withAlpha(0.85f), 0, graphBounds.getBottom(), false);
                g.setGradientFill(postGrad);
                g.strokePath(postPath, juce::PathStrokeType(1.8f));
            }
        }

        //----------------------------------------------------------------------
        // Delta (post - pre) overlay
        //----------------------------------------------------------------------
        if (showDelta && showPost && !isFrozen)
        {
            const auto& postRaw = processor.getPostEQAnalyzer().getSmoothedSpectrum();
            if (!postRaw.empty() && usable > 4)
            {
                juce::Path deltaPath;
                bool started = false;
                float zeroY = dbToY(0.0f);

                for (size_t i = 0; i < usable; ++i)
                {
                    float x = graphBounds.getX() + (float)i;
                    float freq = xToFreq(x);
                    int bin = processor.getPostEQAnalyzer().getBinForFrequency(freq);
                    float postDb = (bin >= 0 && bin < (int)postRaw.size()) ? postRaw[bin] : spectrumMinDb;
                    float preDb = preBuffer[i];
                    float delta = applyTilt(postDb, freq) - applyTilt(preDb, freq);
                    delta = juce::jlimit(-24.0f, 24.0f, delta);
                    float y = zeroY - (delta / 24.0f) * (graphBounds.getHeight() * 0.45f);

                    if (!started) { deltaPath.startNewSubPath(x, y); started = true; }
                    else deltaPath.lineTo(x, y);
                }

                g.setColour(ModernLookAndFeel::Colors::accentCyan.withAlpha(0.8f));
                g.strokePath(deltaPath, juce::PathStrokeType(1.6f));
            }
        }

        //----------------------------------------------------------------------
        // Peak-hold overlay (visual only)
        //----------------------------------------------------------------------
        if (!peakHold.empty() && usable > 4)
        {
            juce::Path peakPath;
            bool started = false;
            const size_t limit = std::min(usable, peakHold.size());
            for (size_t i = 0; i < limit; ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float freq = xToFreq(x);
                float y = dbToY(applyTilt(peakHold[i], freq));
                if (!started) { peakPath.startNewSubPath(x, y); started = true; }
                else peakPath.lineTo(x, y);
            }
            g.setColour(ModernLookAndFeel::Colors::accentOrange.withAlpha(0.9f));
            g.strokePath(peakPath, juce::PathStrokeType(1.2f));
        }
    }

    void drawEQCurve(juce::Graphics& g)
    {
        // SAFETY: Skip if processor not ready
        if (!processor.isProcessorReady())
            return;

        rebuildEQCurvePath();

        // EQ curve (white/gray like TDR Nova)
        g.setColour(ModernLookAndFeel::Colors::eqCurve.withAlpha(0.15f));
        g.strokePath(cachedEQCurve, juce::PathStrokeType(4.0f));
        
        g.setColour(ModernLookAndFeel::Colors::eqCurve);
        g.strokePath(cachedEQCurve, juce::PathStrokeType(2.0f));
    }

    void drawAIMarkers(juce::Graphics& g)
    {
        // SAFETY: Skip if processor not ready
        if (!processor.isProcessorReady())
            return;
            
        const auto corrections = processor.getAIEngine().getPendingCorrections();
        
        for (const auto& c : corrections)
        {
            float x = freqToX(c.frequency);
            if (x < graphBounds.getX() || x > graphBounds.getRight()) continue;
            
            juce::Colour col = c.approved ? ModernLookAndFeel::Colors::accentGreen 
                                          : ModernLookAndFeel::Colors::accentOrange;
            
            // Vertical highlight line
            g.setColour(col.withAlpha(0.15f));
            g.fillRect(x - 2, graphBounds.getY(), 4.0f, graphBounds.getHeight());
            
            // Marker circle
            float my = graphBounds.getY() + 12;
            
            // Outer ring
            g.setColour(col.withAlpha(0.3f));
            g.drawEllipse(x - 10, my - 10, 20, 20, 2.0f);
            
            // Inner filled circle
            g.setColour(col);
            g.fillEllipse(x - 6, my - 6, 12, 12);
            
            // White center
            g.setColour(ModernLookAndFeel::Colors::textBright);
            g.fillEllipse(x - 2, my - 2, 4, 4);
        }
    }
    
    void drawEQBands(juce::Graphics& g)
    {
        const int active = processor.getNumActiveBands();
        const int maxBands = AIEqualizerAudioProcessor::maxBands;
        for (int i = 0; i < std::min(active, maxBands); ++i)
        {
            auto state = processor.getBandState(i);
            
            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);
            
            // Skip if outside bounds
            if (x < graphBounds.getX() - 20 || x > graphBounds.getRight() + 20)
                continue;
            
            juce::Colour col = bandColors[i];
            bool isSelected = (i == selectedBandIndex);
            bool isHovered = (i == hoveredBandIndex);
            bool isDragging = (isDraggingBand && i == selectedBandIndex);
            
            float baseRadius = state.enabled ? 13.0f : 6.0f;
            float radius = baseRadius;
            
            if (isSelected || isHovered || isDragging)
                radius = baseRadius + 3.0f;
            
            //------------------------------------------------------------------
            // Draw Q indicator (width of the band)
            //------------------------------------------------------------------
            if (state.enabled && std::abs(state.gain) > 0.5f)
            {
                float bandwidth = state.frequency / state.q;
                float x1 = freqToX(state.frequency - bandwidth * 0.5f);
                float x2 = freqToX(state.frequency + bandwidth * 0.5f);
                
                // Draw bandwidth area
                g.setColour(col.withAlpha(0.08f));
                g.fillRect(x1, graphBounds.getY(), x2 - x1, graphBounds.getHeight());
                
                // Draw vertical line at center
                g.setColour(col.withAlpha(0.3f));
                g.drawVerticalLine(static_cast<int>(x), graphBounds.getY(), graphBounds.getBottom());
            }
            
            //------------------------------------------------------------------
            // Draw connection line from 0dB to current gain
            //------------------------------------------------------------------
            if (state.enabled && std::abs(state.gain) > 0.5f)
            {
                float zeroY = gainToY(0.0f);
                g.setColour(col.withAlpha(0.4f));
                g.drawLine(x, zeroY, x, y, 2.0f);
            }
            
            //------------------------------------------------------------------
            // Draw selection ring
            //------------------------------------------------------------------
            if (isSelected || isDragging)
            {
                g.setColour(col.withAlpha(0.3f));
                g.fillEllipse(x - radius - 6, y - radius - 6, (radius + 6) * 2, (radius + 6) * 2);
                g.setColour(col.withAlpha(0.7f));
                g.drawEllipse(x - radius - 4, y - radius - 4, (radius + 4) * 2, (radius + 4) * 2, 2.0f);
            }

            //------------------------------------------------------------------
            // SOLO indicator (badge + outline)
            //------------------------------------------------------------------
            if (state.solo)
            {
                juce::Rectangle<float> badge(x - radius - 10.0f, y - radius - 18.0f, 20.0f, 14.0f);
                g.setColour(ModernLookAndFeel::Colors::accentYellow.withAlpha(0.9f));
                g.fillRoundedRectangle(badge, 3.0f);
                g.setColour(ModernLookAndFeel::Colors::bgDark);
                {
                    juce::Font font(9.0f);
                    font.setBold(true);
                    g.setFont(font);
                }
                g.drawText("S", badge, juce::Justification::centred);

                g.setColour(ModernLookAndFeel::Colors::accentYellow);
                g.drawEllipse(x - radius - 3, y - radius - 3, (radius + 3) * 2, (radius + 3) * 2, 2.0f);
            }
            else if (isHovered)
            {
                g.setColour(col.withAlpha(0.2f));
                g.fillEllipse(x - radius - 4, y - radius - 4, (radius + 4) * 2, (radius + 4) * 2);
            }
            
            //------------------------------------------------------------------
            // Draw main circle
            //------------------------------------------------------------------
            if (state.enabled)
            {
                // Filled circle
                g.setColour(col.withAlpha(0.9f));
                g.fillEllipse(x - radius, y - radius, radius * 2, radius * 2);
                
                // Border
                g.setColour(col.brighter(isDragging ? 0.4f : 0.1f));
                g.drawEllipse(x - radius, y - radius, radius * 2, radius * 2, 2.0f);
                
                // Inner highlight
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.fillEllipse(x - radius * 0.4f, y - radius * 0.7f, radius * 0.6f, radius * 0.3f);
                
                // Band number (Roman numerals for 1-4)
                juce::String label;
                switch (i)
                {
                    case 0: label = "I"; break;
                    case 1: label = "II"; break;
                    case 2: label = "III"; break;
                    case 3: label = "IV"; break;
                    case 4: label = "V"; break;
                    case 5: label = "VI"; break;
                    case 6: label = "VII"; break;
                    case 7: label = "VIII"; break;
                    default: label = juce::String(i + 1); break;
                }
                
                g.setColour(ModernLookAndFeel::Colors::bgDark);
                const float fontHeight = (i < 4 ? 10.0f : 9.0f);
                g.setFont(juce::Font(juce::FontOptions().withHeight(fontHeight)
                                                          .withStyle("Bold")));
                g.drawText(label, static_cast<int>(x - radius), static_cast<int>(y - radius),
                          static_cast<int>(radius * 2), static_cast<int>(radius * 2),
                          juce::Justification::centred);
            }
            else
            {
                // Disabled: small dim circle
                g.setColour(col.withAlpha(0.3f));
                g.fillEllipse(x - 5, y - 5, 10, 10);
                g.setColour(col.withAlpha(0.5f));
                g.drawEllipse(x - 5, y - 5, 10, 10, 1.0f);
            }
        }
        
        //----------------------------------------------------------------------
        // Draw info tooltip for selected band
        //----------------------------------------------------------------------
        // BOUNDS CHECK: Ensure selectedBandIndex is valid before accessing bandColors array
        if (selectedBandIndex >= 0 && selectedBandIndex < AIEqualizerAudioProcessor::maxBands)
        {
            auto state = processor.getBandState(selectedBandIndex);
            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);
            
            // Format frequency
            juce::String freqStr = state.frequency >= 1000 
                ? juce::String(state.frequency / 1000.0f, 1) + " kHz"
                : juce::String(static_cast<int>(state.frequency)) + " Hz";
            
            // Format info
            juce::String info = "Band " + juce::String(selectedBandIndex + 1) + ": " + freqStr;
            info += "  " + juce::String(state.gain, 1) + " dB";
            info += "  Q:" + juce::String(state.q, 1);
            
            // Draw tooltip background
            int tw = 180, th = 18;
            int tx = static_cast<int>(juce::jlimit(graphBounds.getX(), graphBounds.getRight() - tw, x - tw / 2));
            int ty = static_cast<int>(y) - 35;
            if (ty < graphBounds.getY() + 30) ty = static_cast<int>(y) + 25;
            
            g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(0.95f));
            g.fillRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty), 
                                   static_cast<float>(tw), static_cast<float>(th), 4.0f);
            g.setColour(bandColors[static_cast<size_t>(selectedBandIndex)]);
            g.drawRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty), 
                                   static_cast<float>(tw), static_cast<float>(th), 4.0f, 1.5f);
            
            g.setColour(ModernLookAndFeel::Colors::textBright);
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
            g.drawText(info, tx, ty, tw, th, juce::Justification::centred);
        }
    }
    
public:
    void drawLabels(juce::Graphics& g)
    {
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.setColour(ModernLookAndFeel::Colors::textMuted);
        
        // dB labels
        for (float db = spectrumMinDb; db <= spectrumMaxDb; db += 12.0f)
        {
            float y = dbToY(db);
            juce::String txt = juce::String((int)db);
            g.drawText(txt, 5, (int)y - 7, 32, 14, juce::Justification::centredRight);
        }

        if (isPianoRollEnabled())
        {
            drawPianoRollOverlay(g);
            return;
        }

        // Frequency labels
        const std::pair<float, const char*> freqLabels[] = {
            {20.0f,"20"}, {50.0f,"50"}, {100.0f,"100"}, {200.0f,"200"}, {500.0f,"500"},
            {1000.0f,"1k"}, {2000.0f,"2k"}, {5000.0f,"5k"}, {10000.0f,"10k"}, {20000.0f,"20k"}
        };
        for (auto& [f, lbl] : freqLabels)
        {
            float x = freqToX(f);
            if (x >= graphBounds.getX() && x <= graphBounds.getRight())
                g.drawText(lbl, (int)x - 15, (int)graphBounds.getBottom() + 3, 30, 14, 
                          juce::Justification::centred);
        }
    }

    void drawProblemHighlight(juce::Graphics& g)
    {
        if (!currentHighlight.active || currentHighlight.fadeCounter <= 0)
            return;
        
        // Calculate fade alpha
        float alpha = juce::jmin(1.0f, currentHighlight.fadeCounter / 60.0f);
        
        float centerX = freqToX(currentHighlight.frequency);
        
        // Calculate bandwidth based on Q (bandwidth = freq / Q)
        float bandwidth = currentHighlight.frequency / juce::jmax(0.5f, currentHighlight.q);
        float lowFreq = currentHighlight.frequency - bandwidth * 0.5f;
        float highFreq = currentHighlight.frequency + bandwidth * 0.5f;
        float leftX = freqToX(juce::jmax(20.0f, lowFreq));
        float rightX = freqToX(juce::jmin(20000.0f, highFreq));
        
        // Get color based on problem type
        juce::Colour highlightCol;
        switch (currentHighlight.type)
        {
            case AIEngine::ProblemType::Resonance:
                highlightCol = juce::Colour(0xFFFF4444);  // Red
                break;
            case AIEngine::ProblemType::Harshness:
            case AIEngine::ProblemType::Sibilance:
                highlightCol = juce::Colour(0xFFFF8800);  // Orange
                break;
            case AIEngine::ProblemType::Muddiness:
            case AIEngine::ProblemType::Boxyness:
            case AIEngine::ProblemType::LowEndBoom:
                highlightCol = juce::Colour(0xFFBB6600);  // Brown/Orange
                break;
            case AIEngine::ProblemType::ThinSound:
            case AIEngine::ProblemType::DullSound:
                highlightCol = juce::Colour(0xFF4488FF);  // Blue (boost)
                break;
            default:
                highlightCol = juce::Colour(0xFFFFFF00);  // Yellow
                break;
        }
        
        // Draw gradient highlight zone
        juce::ColourGradient gradient(highlightCol.withAlpha(alpha * 0.4f * currentHighlight.severity),
                                       centerX, graphBounds.getY(),
                                       highlightCol.withAlpha(0.0f),
                                       rightX, graphBounds.getY(), true);
        gradient.addColour(0.5, highlightCol.withAlpha(alpha * 0.3f * currentHighlight.severity));
        g.setGradientFill(gradient);
        g.fillRect(leftX, graphBounds.getY(), rightX - leftX, graphBounds.getHeight());
        
        // Draw center line (pulsing)
        float pulse = 0.7f + 0.3f * std::sin(currentHighlight.fadeCounter * 0.15f);
        g.setColour(highlightCol.withAlpha(alpha * pulse));
        g.drawVerticalLine(static_cast<int>(centerX), graphBounds.getY(), graphBounds.getBottom());
        g.drawVerticalLine(static_cast<int>(centerX) + 1, graphBounds.getY(), graphBounds.getBottom());
        
        // Draw frequency marker at top
        juce::String freqStr = currentHighlight.frequency >= 1000 
            ? juce::String(currentHighlight.frequency / 1000.0f, 1) + " kHz"
            : juce::String(static_cast<int>(currentHighlight.frequency)) + " Hz";
        
        int markerW = 80, markerH = 28;
        float markerX = juce::jlimit(graphBounds.getX(), graphBounds.getRight() - markerW, centerX - markerW * 0.5f);
        float markerY = graphBounds.getY() + 5;
        
        // Marker background
        g.setColour(highlightCol.darker(0.3f).withAlpha(alpha * 0.95f));
        g.fillRoundedRectangle(markerX, markerY, static_cast<float>(markerW), static_cast<float>(markerH), 6.0f);
        g.setColour(highlightCol.withAlpha(alpha));
        g.drawRoundedRectangle(markerX, markerY, static_cast<float>(markerW), static_cast<float>(markerH), 6.0f, 2.0f);
        
        // Problem type icon
        juce::String icon = "⚠";
        if (currentHighlight.type == AIEngine::ProblemType::ThinSound || 
            currentHighlight.type == AIEngine::ProblemType::DullSound)
            icon = "📈";
        
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText(icon + " " + freqStr, static_cast<int>(markerX), static_cast<int>(markerY), markerW, markerH,
                   juce::Justification::centred);
        
        // Decrement fade counter
        currentHighlight.fadeCounter--;
        if (currentHighlight.fadeCounter <= 0)
            currentHighlight.active = false;
    }

    void drawHover(juce::Graphics& g)
    {
        if (hoverX < graphBounds.getX() || hoverX > graphBounds.getRight()) return;
        
        float freq = xToFreq((float)hoverX);
        
        // Vertical line
        g.setColour(ModernLookAndFeel::Colors::textSecondary.withAlpha(0.3f));
        g.drawVerticalLine(hoverX, graphBounds.getY(), graphBounds.getBottom());
        
        // Tooltip
        juce::String txt = freq >= 1000 ? juce::String(freq/1000.0f, 1) + " kHz"
                                        : juce::String((int)freq) + " Hz";
        if (isPianoRollEnabled())
        {
            txt += "  (" + getNoteName(freq) + ")";
        }
        
        int tw = 60, th = 18;
        int tx = juce::jlimit((int)graphBounds.getX(), (int)graphBounds.getRight() - tw, hoverX - tw/2);
        int ty = (int)graphBounds.getY() - th - 4;
        
        g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(0.95f));
        g.fillRoundedRectangle((float)tx, (float)ty, (float)tw, (float)th, 3.0f);
        g.setColour(ModernLookAndFeel::Colors::accentBlue);
        g.drawRoundedRectangle((float)tx, (float)ty, (float)tw, (float)th, 3.0f, 1.0f);
        
        g.setColour(ModernLookAndFeel::Colors::textBright);
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.drawText(txt, tx, ty, tw, th, juce::Justification::centred);
    }

public:
    int getBandAtPosition(juce::Point<float> pos) const
    {
        const float hitRadius = 10.0f;
        const float stickyRadius = hitRadius * 2.0f;
        
        const int active = processor.getNumActiveBands();
        const int maxBands = AIEqualizerAudioProcessor::maxBands;

        int bestIndex = -1;
        float bestDist = hitRadius;

        for (int i = 0; i < std::min(active, maxBands); ++i)
        {
            auto state = processor.getBandState(i);
            float bx = freqToX(state.frequency);
            float by = gainToY(state.gain);
            
            float dist = std::sqrt((pos.x - bx) * (pos.x - bx) + (pos.y - by) * (pos.y - by));
            
            // Keep current selection sticky when bands overlap
            if (i == selectedBandIndex && dist < stickyRadius)
                return i;

            if (dist < bestDist)
            {
                bestDist = dist;
                bestIndex = i;
            }
        }
        
        return bestIndex; // -1 if nothing within radius
    }

    // Conversions
    float freqToX(float f) const {
        float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
        float p = (std::log10(juce::jlimit(20.0f, 20000.0f, f)) - logMin) / (logMax - logMin);
        return graphBounds.getX() + p * graphBounds.getWidth();
    }
    
    float xToFreq(float x) const {
        float p = juce::jlimit(0.0f, 1.0f, (x - graphBounds.getX()) / graphBounds.getWidth());
        float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
        return std::pow(10.0f, logMin + p * (logMax - logMin));
    }
    
    float dbToY(float db) const {
        float p = juce::jlimit(0.0f, 1.0f, (db - spectrumMinDb) / (spectrumMaxDb - spectrumMinDb));
        return graphBounds.getBottom() - p * graphBounds.getHeight();
    }
    
    // Gain to Y (for band positions, centered at 0dB)
    float gainToY(float gain) const {
        // Map -24 to +24 dB to graph bounds
        float p = juce::jmap(gain, -24.0f, 24.0f, 1.0f, 0.0f);
        return graphBounds.getY() + p * graphBounds.getHeight();
    }
    
    float yToGain(float y) const {
        float p = (y - graphBounds.getY()) / graphBounds.getHeight();
        return juce::jmap(p, 0.0f, 1.0f, 24.0f, -24.0f);
    }

private:
    void rebuildEQCurvePath()
    {
        const uint64_t version = processor.getParameterChangeCounter();
        const bool boundsChanged = (lastCurveBounds != graphBounds);

        if (!eqCurveDirty && !boundsChanged && version == lastEQVersion && !cachedEQCurve.isEmpty())
            return;

        lastEQVersion = version;
        lastCurveBounds = graphBounds;
        eqCurveDirty = false;
        cachedEQCurve.clear();

        if (graphBounds.isEmpty())
            return;

        auto& eq = processor.getEQProcessor();
        double sr = processor.getSampleRate();
        if (sr <= 0) sr = 44100.0;

        ensureEQCurveFrequencies();
        eqCurveMagnitudes.resize(eqCurveFrequencies.size(), 1.0f);
        eq.getMagnitudeForFrequencyArray(eqCurveFrequencies.data(),
                                         eqCurveMagnitudes.data(),
                                         eqCurveFrequencies.size(),
                                         sr);

        const float zeroY = dbToY(0.0f);
        const float yScale = graphBounds.getHeight() * 0.45f;
        bool started = false;

        for (size_t i = 0; i < eqCurveFrequencies.size(); ++i)
        {
            float db = juce::Decibels::gainToDecibels(eqCurveMagnitudes[i], -48.0f);
            db = juce::jlimit(-24.0f, 24.0f, db);

            const float x = freqToX(eqCurveFrequencies[i]);
            const float y = zeroY - (db / 24.0f) * yScale;

            if (!started)
            {
                cachedEQCurve.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                cachedEQCurve.lineTo(x, y);
            }
        }
    }

    void ensureEQCurveFrequencies()
    {
        if (!eqCurveFrequencies.empty())
            return;

        eqCurveFrequencies.resize(eqCurvePointCount);
        const float logMin = std::log10(20.0f);
        const float logMax = std::log10(20000.0f);
        for (size_t i = 0; i < eqCurveFrequencies.size(); ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(eqCurveFrequencies.size() - 1);
            eqCurveFrequencies[i] = std::pow(10.0f, logMin + t * (logMax - logMin));
        }
    }

    AIEqualizerAudioProcessor& processor;
    juce::Rectangle<float> graphBounds;
    std::vector<float> smoothedSpectrum;
    std::vector<float> peakHold;
    std::vector<float> peakTimers;
    int hoverX = -1, hoverY = -1;
    
    // Freeze/Capture functionality
    juce::TextButton freezeButton, captureButton, clearButton;
    juce::ToggleButton showCapturedButton;
    std::vector<float> frozenSpectrum;
    std::vector<float> capturedSpectrum;
    bool isFrozen = false;
    bool hasCaptured = false;
    int captureTextTimer = 0;
    // Spectrum buffers are grown on demand; no shrinking to avoid per-frame realloc
    
    // Band interaction
    int selectedBandIndex = 0;      // Currently selected band
    int hoveredBandIndex = -1;      // Band under mouse cursor
    bool isDraggingBand = false;    // Currently dragging a band
    juce::Point<float> dragStartPos;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;
    float dragStartQ = 0.0f;
    std::array<juce::Colour, AIEqualizerAudioProcessor::maxBands> bandColors;
    std::vector<SpectrumPeak> detectedPeaks;
    int hoveredPeakIndex = -1;

    // Cached EQ curve to avoid per-pixel recomputation
    juce::Path cachedEQCurve;
    std::vector<float> eqCurveFrequencies;
    std::vector<float> eqCurveMagnitudes;
    juce::Rectangle<float> lastCurveBounds;
    uint64_t lastEQVersion = std::numeric_limits<uint64_t>::max();
    bool eqCurveDirty = true;
    static constexpr size_t eqCurvePointCount = 256;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedSpectrumDisplay)
};
