#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"
#include <array>

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
        smoothedSpectrum.resize(512, -90.0f);
        frozenSpectrum.resize(512, -90.0f);
        capturedSpectrum.resize(512, -90.0f);
        
        // Initialize band colors
        for (int i = 0; i < 8; ++i)
        {
            bandColors[i] = ModernLookAndFeel::Colors::getBandColor(i);
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

    void paint(juce::Graphics& g) override
    {
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
        // Reset capture button text after showing "CAPTURED!"
        static int captureTextTimer = 0;
        if (captureButton.getButtonText() == "CAPTURED!") {
            if (++captureTextTimer > 10) {
                captureButton.setButtonText("CAPTURE");
                captureTextTimer = 0;
            }
        }
        
        if (!isFrozen) {
            updateSmoothedSpectrum();
        }
        repaint(); 
    }

    void mouseMove(const juce::MouseEvent& e) override 
    { 
        hoverX = e.x; 
        hoverY = e.y;
        
        // Check if hovering over a band
        hoveredBandIndex = getBandAtPosition(e.position);
        
        // Change cursor when over a band
        if (hoveredBandIndex >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseExit(const juce::MouseEvent&) override 
    { 
        hoverX = -1; 
        hoveredBandIndex = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    
    void mouseDown(const juce::MouseEvent& e) override 
    {
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
            // Clicked on empty space - just report frequency
            if (onFrequencySelected)
                onFrequencySelected(xToFreq((float)e.x));
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
        
        float freq = xToFreq((float)e.x);
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
            float minGain = 1000.0f;
            
            for (int i = 0; i < AIEqualizerAudioProcessor::numBands; ++i)
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
    void updateSmoothedSpectrum()
    {
        const auto& raw = processor.getSpectrumAnalyzer().getSmoothedSpectrum();
        if (raw.empty()) return;
        
        size_t w = static_cast<size_t>(juce::jmax(100.0f, graphBounds.getWidth()));
        if (smoothedSpectrum.size() != w) smoothedSpectrum.resize(w, -90.0f);
        
        for (size_t i = 0; i < w; ++i)
        {
            float freq = xToFreq(graphBounds.getX() + (float)i);
            int bin = processor.getSpectrumAnalyzer().getBinForFrequency(freq);
            float db = (bin >= 0 && bin < (int)raw.size()) ? raw[bin] : -90.0f;
            smoothedSpectrum[i] = smoothedSpectrum[i] * 0.75f + db * 0.25f;
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
        for (float db = -48; db <= 12; db += 12)
        {
            float y = dbToY(db);
            bool isZero = (db == 0);
            g.setColour(isZero ? ModernLookAndFeel::Colors::textMuted.withAlpha(0.4f)
                               : ModernLookAndFeel::Colors::grid);
            g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
        }
    }

    void drawSpectrum(juce::Graphics& g)
    {
        if (smoothedSpectrum.empty()) return;
        
        bool showPre = processor.getAPVTS().getRawParameterValue("showPreSpectrum")->load() > 0.5f;
        bool showPost = processor.getAPVTS().getRawParameterValue("showPostSpectrum")->load() > 0.5f;
        bool showCaptured = showCapturedButton.getToggleState() && hasCaptured;
        
        //----------------------------------------------------------------------
        // Draw CAPTURED spectrum (orange, dashed) - behind live spectrum
        //----------------------------------------------------------------------
        if (showCaptured && !capturedSpectrum.empty())
        {
            juce::Path capturedPath;
            bool started = false;
            
            for (size_t i = 0; i < capturedSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float y = dbToY(capturedSpectrum[i]);
                if (!started) { capturedPath.startNewSubPath(x, y); started = true; }
                else capturedPath.lineTo(x, y);
            }
            
            // Orange color for captured
            g.setColour(juce::Colour(0xFFE6A23C).withAlpha(0.6f));
            float dashLengths[] = { 4.0f, 4.0f };
            juce::PathStrokeType stroke(1.5f);
            stroke.createDashedStroke(capturedPath, capturedPath, dashLengths, 2);
            g.strokePath(capturedPath, stroke);
        }
        
        //----------------------------------------------------------------------
        // Draw FROZEN spectrum (cyan) when frozen
        //----------------------------------------------------------------------
        if (isFrozen && !frozenSpectrum.empty())
        {
            juce::Path frozenFillPath;
            frozenFillPath.startNewSubPath(graphBounds.getX(), graphBounds.getBottom());
            
            for (size_t i = 0; i < frozenSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float y = dbToY(frozenSpectrum[i]);
                frozenFillPath.lineTo(x, y);
            }
            frozenFillPath.lineTo(graphBounds.getRight(), graphBounds.getBottom());
            frozenFillPath.closeSubPath();
            
            // Cyan gradient fill for frozen
            juce::ColourGradient frozenGrad(
                juce::Colour(0xFF00BCD4).withAlpha(0.4f), 0, graphBounds.getY(),
                juce::Colour(0xFF00BCD4).withAlpha(0.02f), 0, graphBounds.getBottom(), false);
            g.setGradientFill(frozenGrad);
            g.fillPath(frozenFillPath);
            
            // Frozen line (cyan)
            juce::Path frozenLinePath;
            bool started = false;
            for (size_t i = 0; i < frozenSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float y = dbToY(frozenSpectrum[i]);
                if (!started) { frozenLinePath.startNewSubPath(x, y); started = true; }
                else frozenLinePath.lineTo(x, y);
            }
            
            g.setColour(juce::Colour(0xFF00BCD4));
            g.strokePath(frozenLinePath, juce::PathStrokeType(2.0f));
            
            // Draw "FROZEN" label
            g.setColour(juce::Colour(0xFF00BCD4));
            g.setFont(12.0f);
            g.drawText("FROZEN", graphBounds.getX() + 10, graphBounds.getY() + 30, 80, 20, 
                      juce::Justification::centredLeft);
        }
        
        //----------------------------------------------------------------------
        // Draw LIVE spectrum (blue) - only when NOT frozen
        //----------------------------------------------------------------------
        if (showPre && !isFrozen)
        {
            // Build filled path
            juce::Path fillPath;
            fillPath.startNewSubPath(graphBounds.getX(), graphBounds.getBottom());
            
            for (size_t i = 0; i < smoothedSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float y = dbToY(smoothedSpectrum[i]);
                fillPath.lineTo(x, y);
            }
            fillPath.lineTo(graphBounds.getRight(), graphBounds.getBottom());
            fillPath.closeSubPath();
            
            // Gradient fill (TDR Nova blue style)
            juce::ColourGradient fillGrad(
                ModernLookAndFeel::Colors::spectrumFill.withAlpha(0.5f), 0, graphBounds.getY(),
                ModernLookAndFeel::Colors::spectrumFill.withAlpha(0.05f), 0, graphBounds.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);
            
            // Top line
            juce::Path linePath;
            bool started = false;
            for (size_t i = 0; i < smoothedSpectrum.size(); ++i)
            {
                float x = graphBounds.getX() + (float)i;
                float y = dbToY(smoothedSpectrum[i]);
                if (!started) { linePath.startNewSubPath(x, y); started = true; }
                else linePath.lineTo(x, y);
            }
            
            g.setColour(ModernLookAndFeel::Colors::spectrumLine);
            g.strokePath(linePath, juce::PathStrokeType(1.5f));
        }
        
        //----------------------------------------------------------------------
        // Post EQ (green, thinner)
        //----------------------------------------------------------------------
        if (showPost && !isFrozen)
        {
            const auto& postRaw = processor.getPostEQAnalyzer().getSmoothedSpectrum();
            if (!postRaw.empty())
            {
                juce::Path postPath;
                bool started = false;
                
                for (size_t i = 0; i < smoothedSpectrum.size(); ++i)
                {
                    float freq = xToFreq(graphBounds.getX() + (float)i);
                    int bin = processor.getPostEQAnalyzer().getBinForFrequency(freq);
                    float db = (bin >= 0 && bin < (int)postRaw.size()) ? postRaw[bin] : -90.0f;
                    float x = graphBounds.getX() + (float)i;
                    float y = dbToY(db);
                    
                    if (!started) { postPath.startNewSubPath(x, y); started = true; }
                    else postPath.lineTo(x, y);
                }
                
                g.setColour(ModernLookAndFeel::Colors::accentGreen.withAlpha(0.7f));
                g.strokePath(postPath, juce::PathStrokeType(1.2f));
            }
        }
    }

    void drawEQCurve(juce::Graphics& g)
    {
        auto& eq = processor.getEQProcessor();
        double sr = processor.getSampleRate();
        if (sr <= 0) sr = 44100;
        
        juce::Path curvePath;
        bool started = false;
        float zeroY = dbToY(0);
        
        for (float x = graphBounds.getX(); x <= graphBounds.getRight(); x += 1.0f)
        {
            float freq = xToFreq(x);
            float mag = eq.getMagnitudeForFrequency(freq, sr);
            float db = juce::Decibels::gainToDecibels(mag, -48.0f);
            db = juce::jlimit(-24.0f, 24.0f, db);
            
            // Map to Y centered at 0dB line
            float y = zeroY - (db / 24.0f) * (graphBounds.getHeight() * 0.45f);
            
            if (!started) { curvePath.startNewSubPath(x, y); started = true; }
            else curvePath.lineTo(x, y);
        }
        
        // EQ curve (white/gray like TDR Nova)
        g.setColour(ModernLookAndFeel::Colors::eqCurve.withAlpha(0.15f));
        g.strokePath(curvePath, juce::PathStrokeType(4.0f));
        
        g.setColour(ModernLookAndFeel::Colors::eqCurve);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));
    }

    void drawAIMarkers(juce::Graphics& g)
    {
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
        // Draw all 8 EQ bands
        for (int i = 0; i < AIEqualizerAudioProcessor::numBands; ++i)
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
                g.setFont(juce::Font(i < 4 ? 10.0f : 9.0f, juce::Font::bold));
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
        if (selectedBandIndex >= 0)
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
            g.setColour(bandColors[selectedBandIndex]);
            g.drawRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty), 
                                   static_cast<float>(tw), static_cast<float>(th), 4.0f, 1.5f);
            
            g.setColour(ModernLookAndFeel::Colors::textBright);
            g.setFont(10.0f);
            g.drawText(info, tx, ty, tw, th, juce::Justification::centred);
        }
    }
    
    int getBandAtPosition(juce::Point<float> pos) const
    {
        const float hitRadius = 18.0f;
        
        for (int i = 0; i < AIEqualizerAudioProcessor::numBands; ++i)
        {
            auto state = processor.getBandState(i);
            float bx = freqToX(state.frequency);
            float by = gainToY(state.gain);
            
            float dist = std::sqrt((pos.x - bx) * (pos.x - bx) + (pos.y - by) * (pos.y - by));
            
            if (dist < hitRadius)
                return i;
        }
        
        return -1; // No band at this position
    }

    void drawLabels(juce::Graphics& g)
    {
        g.setFont(10.0f);
        g.setColour(ModernLookAndFeel::Colors::textMuted);
        
        // Frequency labels
        const std::pair<float, const char*> freqLabels[] = {
            {20,"20"}, {50,"50"}, {100,"100"}, {200,"200"}, {500,"500"},
            {1000,"1k"}, {2000,"2k"}, {5000,"5k"}, {10000,"10k"}, {20000,"20k"}
        };
        for (auto& [f, lbl] : freqLabels)
        {
            float x = freqToX(f);
            if (x >= graphBounds.getX() && x <= graphBounds.getRight())
                g.drawText(lbl, (int)x - 15, (int)graphBounds.getBottom() + 3, 30, 14, 
                          juce::Justification::centred);
        }
        
        // dB labels
        for (float db = -48; db <= 12; db += 12)
        {
            float y = dbToY(db);
            juce::String txt = juce::String((int)db);
            g.drawText(txt, 5, (int)y - 7, 32, 14, juce::Justification::centredRight);
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
        g.setFont(11.0f);
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
        
        int tw = 60, th = 18;
        int tx = juce::jlimit((int)graphBounds.getX(), (int)graphBounds.getRight() - tw, hoverX - tw/2);
        int ty = (int)graphBounds.getY() - th - 4;
        
        g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(0.95f));
        g.fillRoundedRectangle((float)tx, (float)ty, (float)tw, (float)th, 3.0f);
        g.setColour(ModernLookAndFeel::Colors::accentBlue);
        g.drawRoundedRectangle((float)tx, (float)ty, (float)tw, (float)th, 3.0f, 1.0f);
        
        g.setColour(ModernLookAndFeel::Colors::textBright);
        g.setFont(10.0f);
        g.drawText(txt, tx, ty, tw, th, juce::Justification::centred);
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
        float p = juce::jlimit(0.0f, 1.0f, (db - (-60.0f)) / (12.0f - (-60.0f)));
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

    AIEqualizerAudioProcessor& processor;
    juce::Rectangle<float> graphBounds;
    std::vector<float> smoothedSpectrum;
    int hoverX = -1, hoverY = -1;
    
    // Freeze/Capture functionality
    juce::TextButton freezeButton, captureButton, clearButton;
    juce::ToggleButton showCapturedButton;
    std::vector<float> frozenSpectrum;
    std::vector<float> capturedSpectrum;
    bool isFrozen = false;
    bool hasCaptured = false;
    
    // Band interaction
    int selectedBandIndex = 0;      // Currently selected band
    int hoveredBandIndex = -1;      // Band under mouse cursor
    bool isDraggingBand = false;    // Currently dragging a band
    juce::Point<float> dragStartPos;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;
    float dragStartQ = 0.0f;
    std::array<juce::Colour, 8> bandColors;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedSpectrumDisplay)
};
