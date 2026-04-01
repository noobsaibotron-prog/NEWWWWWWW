#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../PluginProcessor.h"
#include "ModernLookAndFeel.h"
#include <array>
#include <cmath>
#include <limits>

#if AIEQ_GUI_DEBUG
#include "../Utils/DebugLog.h"
#endif

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
    friend class EQGraphFluidityTest; // Performance test access
public:
    // Callbacks for band interaction
    std::function<void(float)> onFrequencySelected;
    std::function<void(int)> onBandSelected;                          // Single click on band
    std::function<void(int, float, float)> onBandCreatedOrActivated;  // Double click: band index, freq, gain
    std::function<void(int, float, float, float)> onBandDragged;      // Drag: band, freq, gain, q

    explicit AdvancedSpectrumDisplay(AIEqualizerAudioProcessor& p) : processor(p)
    {
        setOpaque(false);
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
        
        // Spectrum toolbar — semi-transparent buttons that blend with the display
        auto setupToolbarBtn = [](juce::TextButton& btn, const juce::String& text) {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x40202030));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0x804A90D9));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xBBD0D0D8));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        };

        setupToolbarBtn(freezeButton, "FREEZE");
        freezeButton.setClickingTogglesState(true);
        freezeButton.onClick = [this]() {
            bool frozen = freezeButton.getToggleState();
            const juce::SpinLock::ScopedLockType lock(spectrumDataLock);
            if (frozen) {
                frozenSpectrum = smoothedSpectrum;
                isFrozen = true;
            } else {
                isFrozen = false;
            }
            refreshPeaks();
        };
        addAndMakeVisible(freezeButton);

        setupToolbarBtn(captureButton, "CAPTURE");
        captureButton.onClick = [this]() {
            {
                const juce::SpinLock::ScopedLockType lock(spectrumDataLock);
                capturedSpectrum = smoothedSpectrum;
                hasCaptured = true;
                capturedPathDirty = true;
            }
            captureButton.setButtonText("CAPTURED!");
            startTimer(100);
        };
        addAndMakeVisible(captureButton);

        showCapturedButton.setButtonText("SHOW CAPT");
        showCapturedButton.setColour(juce::ToggleButton::textColourId, juce::Colour(0xBBD0D0D8));
        showCapturedButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFFE6A23C));
        addAndMakeVisible(showCapturedButton);

        setupToolbarBtn(clearButton, "CLEAR");
        clearButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0x88D0D0D8));
        clearButton.onClick = [this]() {
            {
                const juce::SpinLock::ScopedLockType lk(spectrumDataLock);
                hasCaptured = false;
                isFrozen = false;
            }
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
    int getCurrentRefreshHz() const { return currentTimerHz; }
    
    // Band selection API
    void setSelectedBand(int band) { selectedBandIndex = band; repaint(); }
    int getSelectedBand() const { return selectedBandIndex; }

    /** Graph bounds in component-local float coordinates (set during paint/resized). */
    juce::Rectangle<float> getGraphBoundsF() const noexcept { return graphBounds; }

    /** Inject per-pixel dB arrays from the metrological spectrum pipeline.
     *  When set, updateSmoothedSpectrum() uses this data instead of computing from SpectrumAnalyzer.
     *  preDB: pre-EQ spectrum, one value per pixel column of the graph area.
     *  postDB: post-EQ spectrum (may be empty if showPost is false). */
    void injectPrecomputedSpectrum (const std::vector<float>& preDB,
                                    const std::vector<float>& postDB)
    {
        if (preDB.empty()) return;
        const juce::SpinLock::ScopedLockType lk (spectrumDataLock);
        smoothedSpectrum.assign (preDB.begin(), preDB.end());
        if (!postDB.empty())
            injectedPostSpectrum.assign (postDB.begin(), postDB.end());
        ++injectedSpectrumVersion;
    }

    /** Click detector overlay — shows glitch count + last checkpoint in top-left corner.
     *  Pass count=0 to clear. Thread-safe (message thread only). */
    void setClickOverlay (uint32_t count, const char* checkpointName)
    {
        clickOverlayCount = count;
        clickOverlayCP    = checkpointName ? juce::String (checkpointName) : juce::String{};
        repaint();
    }

    // Spectrum display speed (smoothing)
    enum class SpectrumSpeed { Fast, Medium, Slow };

    void setSpectrumSpeed(SpectrumSpeed speed)
    {
        spectrumSpeed = speed;
        switch (speed)
        {
            case SpectrumSpeed::Fast:   displayReleaseCoeff = 0.40f; break;
            case SpectrumSpeed::Medium: displayReleaseCoeff = 0.70f; break;
            case SpectrumSpeed::Slow:   displayReleaseCoeff = 0.85f; break;
        }
    }
    SpectrumSpeed getSpectrumSpeed() const { return spectrumSpeed; }
    
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
#if AIEQ_GUI_DEBUG
        auto paintStartMs = juce::Time::getMillisecondCounterHiRes();
        double tBg = 0, tGrid = 0, tSpec = 0, tEQ = 0, tBands = 0, tOther = 0;
        auto lap = [&]() { return juce::Time::getMillisecondCounterHiRes(); };
        double t0 = lap();
#endif
        g.reduceClipRegion(getLocalBounds());

        auto bounds = getLocalBounds().toFloat();

        // === PREMIUM BACKGROUND — subtle vertical gradient ===
        {
            juce::ColourGradient bgGrad(
                juce::Colour(0xFF101018), bounds.getX(), bounds.getY(),
                juce::Colour(0xFF0A0A12), bounds.getX(), bounds.getBottom(), false);
            bgGrad.addColour(0.5, juce::Colour(0xFF0E0E16));
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(bounds, 4.0f);
        }

        // Graph area
        graphBounds = bounds.reduced(45, 25);
        graphBounds.removeFromBottom(22);
        graphBounds.removeFromLeft(5);

        // Subtle inner shadow at top of graph area
        {
            juce::ColourGradient shadowGrad(
                juce::Colour(0x18000000), graphBounds.getX(), graphBounds.getY(),
                juce::Colours::transparentBlack, graphBounds.getX(), graphBounds.getY() + 30, false);
            g.setGradientFill(shadowGrad);
            g.fillRect(graphBounds.getX(), graphBounds.getY(), graphBounds.getWidth(), 30.0f);
        }

        // FIX 4: Grid + labels cached as off-screen image (static per resize)
        if (gridCacheDirty || gridCache.isNull()
            || gridCache.getWidth() != getWidth() || gridCache.getHeight() != getHeight())
        {
            gridCache = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
            juce::Graphics gc(gridCache);
            drawGrid(gc);
            drawLabels(gc);
            gridCacheDirty = false;
        }
        g.drawImageAt(gridCache, 0, 0);

#if AIEQ_GUI_DEBUG
        tBg = lap() - t0; t0 = lap();
#endif

        // FIX 2: Spectrum image is pre-rendered in timerCallback → rebuildLiveSpectrumPaths() → renderSpectrumToImage().
        // Here we just blit the cached image (~0.3ms) instead of strokePath/fillPath (~17ms).
        // Captured/frozen spectra are drawn directly (they change rarely).
        drawSpectrum(g);

#if AIEQ_GUI_DEBUG
        tSpec = lap() - t0; t0 = lap();
#endif

        drawSpectrumGrab(g);
        drawProblemHighlight(g);

        // Draw EQ curve directly from cached paths (no image buffer).
        // rebuildEQCurvePath() is version-gated — only rebuilds when curve params change.
        // Direct path draw costs ~2-3ms but avoids 3.7MB image clear that trashes L2 cache.
        drawEQCurveFill(g);
        drawEQCurve(g);
        drawDynamicGROverlay(g);

#if AIEQ_GUI_DEBUG
        tEQ = lap() - t0; t0 = lap();
#endif

        drawEQBands(g);
        drawAIMarkers(g);

        if (hoverX >= 0) drawHover(g);

#if AIEQ_GUI_DEBUG
        tBands = lap() - t0;
#endif

        // Subtle border
        g.setColour(juce::Colour(0xFF222230));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

#if AIEQ_GUI_DEBUG
        {
            double paintMs = juce::Time::getMillisecondCounterHiRes() - paintStartMs;
            debugPaintTimeAccum += paintMs;
            debugMaxPaintTime = std::max(debugMaxPaintTime, paintMs);
            debugPaintCount++;
            debugBgAccum += tBg;
            debugSpecAccum += tSpec;
            debugEQAccum += tEQ;
            debugBandsAccum += tBands;
            double now = juce::Time::getMillisecondCounterHiRes();
            if (now - debugLastReportTime > 2000.0)
            {
                int n = std::max(1, debugPaintCount);
                aieqDebugLog( "[SPECTRUM-DISPLAY] paints/sec=%.1f avgMs=%.2f maxMs=%.2f eqRebuilds=%d  bg=%.1f spec=%.1f eq=%.1f bands=%.1f\n",
                    debugPaintCount * 1000.0 / (now - debugLastReportTime),
                    debugPaintTimeAccum / n,
                    debugMaxPaintTime,
                    debugEQCurveRebuildCount,
                    debugBgAccum / n, debugSpecAccum / n, debugEQAccum / n, debugBandsAccum / n);
                debugPaintCount = 0;
                debugPaintTimeAccum = 0.0;
                debugMaxPaintTime = 0.0;
                debugEQCurveRebuildCount = 0;
                debugBgAccum = 0; debugSpecAccum = 0; debugEQAccum = 0; debugBandsAccum = 0;
                debugLastReportTime = now;
            }
        }
#endif

        // ── Click detector overlay ────────────────────────────────────────
        if (clickOverlayCount > 0)
        {
            juce::String txt = juce::String ("CLICKS: ") + juce::String (clickOverlayCount)
                             + juce::String ("  last@") + clickOverlayCP;
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            const int tw = g.getCurrentFont().getStringWidth (txt) + 10;
            g.setColour (juce::Colours::red.withAlpha (0.85f));
            g.fillRoundedRectangle (6.0f, 6.0f, static_cast<float>(tw), 18.0f, 3.0f);
            g.setColour (juce::Colours::white);
            g.drawText (txt, 6, 6, tw, 18, juce::Justification::centred);
        }
    }

    void resized() override
    {
        capturedPathDirty = true;
        gridCacheDirty = true;        // FIX 4: invalidate grid cache on resize
        lastPreSpectrumVersion = 0;   // FIX 2: invalidate spectrum path cache on resize
        lastPostSpectrumVersion = 0;
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

        // ── Adaptive timer rate ──
        // Multiple plugin instances share ONE message thread in JUCE.
        // If this window isn't visible, drop to 5Hz to free budget for the active instance.
        // If visible but idle, 30Hz is enough. 60Hz only during active interaction.
        {
            const bool windowVisible = isShowing();
            const bool interacting = isDraggingBand || hoverX >= 0;
            const int desiredHz = !windowVisible ? 5
                                : interacting    ? 60
                                                 : 30;
            if (desiredHz != currentTimerHz)
            {
                currentTimerHz = desiredHz;
                startTimerHz(desiredHz);
            }
        }

        // Reset capture button text after showing "CAPTURED!"
        if (captureButton.getButtonText() == "CAPTURED!") {
            if (++captureTextTimer > 10) {
                captureButton.setButtonText("CAPTURE");
                captureTextTimer = 0;
            }
        }

        bool needsRepaint = false;

        if (!isFrozen) {
            updateSmoothedSpectrum();
            rebuildLiveSpectrumPaths();
            updateDynamicGRSmoothing();
            // Repaint when new FFT data arrived (paths rebuilt) or during interaction
            if (lastPreSpectrumVersion != prevRepaintPreVer || lastPostSpectrumVersion != prevRepaintPostVer
                || isDraggingBand || hoverX >= 0)
            {
                prevRepaintPreVer = lastPreSpectrumVersion;
                prevRepaintPostVer = lastPostSpectrumVersion;
                needsRepaint = true;
            }
            // Throttle peak detection to ~10Hz (every 6th frame at 30Hz base)
            if (++peakRefreshCounter >= 3)
            {
                peakRefreshCounter = 0;
                refreshPeaks();
            }
        }

        // EQ curve: no image cache. rebuildEQCurvePath() (called from drawEQCurveFill/drawEQCurve)
        // is version-gated and only rebuilds the juce::Path when curve params actually change.
        // Direct path rendering in paint (~2-3ms) avoids the 3.7MB image clear that was
        // thrashing L2 cache and slowing down ALL paint components.

        // FIX 3: Always repaint if mouse is interacting, frozen, or dynamic bands pulsing
        if (hoverX >= 0 || isDraggingBand || isFrozen || anyDynamicBandActive)
            needsRepaint = true;

        if (needsRepaint)
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
        
        // Prevent re-entrancy if a drag is already active
        if (isDraggingBand)
            return;

        // Check if clicking on a band
        const int clickedBand = getBandAtPosition(e.position);
        
        if (clickedBand >= 0)
        {
            selectedBandIndex = clickedBand;          // UI selection
            draggedBandIndex = clickedBand;           // locked drag target
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
        const int targetBand = draggedBandIndex;

        if (isDraggingBand && targetBand >= 0)
        {
            auto delta = e.position - dragStartPos;
            
            // Calculate new frequency (horizontal, log scale)
            const float freqSens  = juce::jmax(50.0f, graphBounds.getWidth()  * 0.1f);
            const float gainSens  = juce::jmax(4.0f,  graphBounds.getHeight() * 0.04f);
            const float qSens     = juce::jmax(75.0f, graphBounds.getWidth()  * 0.15f);

            float freqMult = std::pow(2.0f, delta.x / freqSens);
            float newFreq = juce::jlimit(20.0f, 20000.0f, dragStartFreq * freqMult);

            // Calculate new gain (vertical)
            float newGain = juce::jlimit(-24.0f, 24.0f, dragStartGain - delta.y / gainSens);

            // Q with shift modifier
            float newQ = dragStartQ;
            if (e.mods.isShiftDown())
            {
                float qMult = std::pow(2.0f, -delta.y / qSens);
                newQ = juce::jlimit(0.1f, 10.0f, dragStartQ * qMult);
            }
            
            // Update processor
            auto state = processor.getBandState(targetBand);
            state.frequency = newFreq;
            state.gain = newGain;
            state.q = newQ;
            processor.setBandState(targetBand, state);
            
            // Notify callback
            if (onBandDragged)
                onBandDragged(targetBand, newFreq, newGain, newQ);
            
            repaint();
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingBand = false;
        draggedBandIndex = -1;
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
        // Bug L fix: null-check before deref (parameter may not exist during teardown)
        auto* preP = processor.getAPVTS().getRawParameterValue("showPreSpectrum");
        bool showPre = preP ? preP->load() > 0.5f : false;
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
        // If metrological pipeline has injected newer data, use it directly.
        // smoothedSpectrum is already populated by injectPrecomputedSpectrum().
        if (injectedSpectrumVersion != lastInjectedVersion)
        {
            lastInjectedVersion = injectedSpectrumVersion;
            return;  // data already in smoothedSpectrum via injection
        }

        const auto& raw = processor.getSpectrumAnalyzer().getSmoothedSpectrum();
        if (raw.empty()) return;
        
        const size_t w = static_cast<size_t>(juce::jmax(100.0f, graphBounds.getWidth()));
        if (smoothedSpectrum.size() < w) smoothedSpectrum.resize(w, spectrumMinDb); // grow only (no per-frame realloc)
        if (peakHold.size() < w) peakHold.resize(w, spectrumMinDb);
        if (peakTimers.size() < w) peakTimers.resize(w, 0.0f);
        
        const float dt = 1.0f / 60.0f; // timer at 60 Hz
        const float holdSec = getPeakHoldSeconds();
        const float decayDbPerSec = getPeakDecayDbPerSec();
        
        // Asymmetric display smoothing: instant attack, gentle release
        // The DSP analyzer already does dual-time-constant smoothing,
        // so we only add a light release-only filter here to avoid jitter.

        const auto& analyzer = processor.getSpectrumAnalyzer();
        const int fftSize = analyzer.getFFTSize();
        const double sr = analyzer.getSampleRate();
        const int numBins = static_cast<int>(raw.size());

        for (size_t i = 0; i < w; ++i)
        {
            float freq = xToFreq(graphBounds.getX() + (float)i);
            // Interpolate between adjacent FFT bins for smooth spectrum
            float binF = freq * static_cast<float>(fftSize) / static_cast<float>(sr);
            int bin0 = static_cast<int>(binF);
            int bin1 = bin0 + 1;
            float frac = binF - static_cast<float>(bin0);
            bin0 = juce::jlimit(0, numBins - 1, bin0);
            bin1 = juce::jlimit(0, numBins - 1, bin1);
            float db = raw[bin0] * (1.0f - frac) + raw[bin1] * frac;

            // Instant attack (new value is higher) — smooth release only
            if (db >= smoothedSpectrum[i])
                smoothedSpectrum[i] = db;
            else
                smoothedSpectrum[i] = smoothedSpectrum[i] * displayReleaseCoeff + db * (1.0f - displayReleaseCoeff);

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

    // Rebuild live spectrum paths only when new FFT data arrives.
    // Display-side smoothing creates <0.1dB changes between FFT frames — visually imperceptible.
    // Path building (4× pathBuilder.build + renderSpectrumToImage) costs ~6ms, so skipping
    // unchanged frames saves significant message thread time during band drag.
    void rebuildLiveSpectrumPaths()
    {
        if (graphBounds.isEmpty()) return;

        const uint64_t preVer  = (injectedSpectrumVersion != 0)
                                 ? injectedSpectrumVersion
                                 : processor.getSpectrumAnalyzer().getSpectrumVersion();
        const uint64_t postVer = (injectedSpectrumVersion != 0)
                                 ? injectedSpectrumVersion
                                 : processor.getPostEQAnalyzer().getSpectrumVersion();
        const bool boundsChanged = (lastSpectrumBounds != graphBounds);

        if (preVer == lastPreSpectrumVersion && postVer == lastPostSpectrumVersion && !boundsChanged)
            return;

        lastPreSpectrumVersion = preVer;
        lastPostSpectrumVersion = postVer;
        lastSpectrumBounds = graphBounds;

        const auto& preBuffer = smoothedSpectrum; // alias used in paint
        const size_t usable = std::min(preBuffer.size(),
            static_cast<size_t>(std::max(0, static_cast<int>(graphBounds.getWidth()))));

        // --- Pre (input) path ---
        auto* preP = processor.getAPVTS().getRawParameterValue("showPreSpectrum");
        bool showPre = preP ? preP->load() > 0.5f : false;
        if (showPre && usable > 4)
        {
            smoothYBuffer.resize(usable);
            for (size_t i = 0; i < usable; ++i)
            {
                float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                smoothYBuffer[i] = dbToY(applyTilt(preBuffer[i], freq));
            }
            cachedPreLine.clear();
            cachedPreFill.clear();
            pathBuilder.build(smoothYBuffer.data(), usable,
                              graphBounds.getX(), graphBounds.getBottom(),
                              cachedPreLine, &cachedPreFill, 3);
        }
        else
        {
            cachedPreLine.clear();
            cachedPreFill.clear();
        }

        // --- Post (output) path ---
        auto* postP = processor.getAPVTS().getRawParameterValue("showPostSpectrum");
        bool showPost = postP ? postP->load() > 0.5f : false;
        if (showPost && !isFrozen)
        {
            const auto& postRaw = processor.getPostEQAnalyzer().getSmoothedSpectrum();
            if (!postRaw.empty() && usable > 4)
            {
                smoothYBuffer.resize(usable);
                const int postFFTSize = processor.getPostEQAnalyzer().getFFTSize();
                const double postSR = processor.getPostEQAnalyzer().getSampleRate();
                const int postNumBins = static_cast<int>(postRaw.size());
                for (size_t i = 0; i < usable; ++i)
                {
                    float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                    float binF = freq * static_cast<float>(postFFTSize) / static_cast<float>(postSR);
                    int b0 = juce::jlimit(0, postNumBins - 1, static_cast<int>(binF));
                    int b1 = juce::jlimit(0, postNumBins - 1, b0 + 1);
                    float frac = binF - static_cast<float>(static_cast<int>(binF));
                    float db = postRaw[b0] * (1.0f - frac) + postRaw[b1] * frac;
                    smoothYBuffer[i] = dbToY(applyTilt(db, freq));
                }
                cachedPostLine.clear();
                pathBuilder.build(smoothYBuffer.data(), usable,
                                  graphBounds.getX(), graphBounds.getBottom(),
                                  cachedPostLine, nullptr, 3);
            }
            else { cachedPostLine.clear(); }
        }
        else { cachedPostLine.clear(); }

        // --- Delta (post - pre) path ---
        bool showDelta = isDeltaEnabled();
        if (showDelta && showPost && !isFrozen)
        {
            const auto& postRaw = processor.getPostEQAnalyzer().getSmoothedSpectrum();
            if (!postRaw.empty() && usable > 4)
            {
                float zeroY = dbToY(0.0f);
                smoothYBuffer.resize(usable);
                const int dFFTSize = processor.getPostEQAnalyzer().getFFTSize();
                const double dSR = processor.getPostEQAnalyzer().getSampleRate();
                const int dNumBins = static_cast<int>(postRaw.size());
                for (size_t i = 0; i < usable; ++i)
                {
                    float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                    float binF = freq * static_cast<float>(dFFTSize) / static_cast<float>(dSR);
                    int b0 = juce::jlimit(0, dNumBins - 1, static_cast<int>(binF));
                    int b1 = juce::jlimit(0, dNumBins - 1, b0 + 1);
                    float frac = binF - static_cast<float>(static_cast<int>(binF));
                    float postDb = postRaw[b0] * (1.0f - frac) + postRaw[b1] * frac;
                    float preDb = preBuffer[i];
                    float delta = applyTilt(postDb, freq) - applyTilt(preDb, freq);
                    delta = juce::jlimit(-24.0f, 24.0f, delta);
                    smoothYBuffer[i] = zeroY - (delta / 24.0f) * (graphBounds.getHeight() * 0.45f);
                }
                cachedDeltaLine.clear();
                pathBuilder.build(smoothYBuffer.data(), usable,
                                  graphBounds.getX(), graphBounds.getBottom(),
                                  cachedDeltaLine, nullptr, 3);
            }
            else { cachedDeltaLine.clear(); }
        }
        else { cachedDeltaLine.clear(); }

        // --- Peak-hold path ---
        if (!peakHold.empty() && usable > 4)
        {
            const size_t limit = std::min(usable, peakHold.size());
            smoothYBuffer.resize(limit);
            for (size_t i = 0; i < limit; ++i)
            {
                float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                smoothYBuffer[i] = dbToY(applyTilt(peakHold[i], freq));
            }
            cachedPeakLine.clear();
            pathBuilder.build(smoothYBuffer.data(), limit,
                              graphBounds.getX(), graphBounds.getBottom(),
                              cachedPeakLine, nullptr, 3);
        }
        else { cachedPeakLine.clear(); }

        // --- Render all live spectrum paths into offscreen image ---
        renderSpectrumToImage();

    }

    // Render cached spectrum paths (pre, post, delta, peak) into an offscreen image.
    // Called only when paths are rebuilt (~12-47Hz), not every paint (~60Hz).
    // paint() then does a single drawImageAt() instead of 4× strokePath/fillPath.
    void renderSpectrumToImage()
    {
        int w = static_cast<int>(std::ceil(graphBounds.getWidth()));
        int h = static_cast<int>(std::ceil(graphBounds.getHeight()));
        if (w <= 0 || h <= 0) return;

        // Reallocate image only if size changed
        if (spectrumImageCache.isNull() || spectrumImageCache.getWidth() != w || spectrumImageCache.getHeight() != h)
            spectrumImageCache = juce::Image(juce::Image::ARGB, w, h, true);
        else
            spectrumImageCache.clear(juce::Rectangle<int>(0, 0, w, h));

        juce::Graphics ig(spectrumImageCache);
        // Offset so paths (which use graphBounds coordinates) render at image origin
        ig.addTransform(juce::AffineTransform::translation(-graphBounds.getX(), -graphBounds.getY()));

        // Pre fill + line
        if (!cachedPreFill.isEmpty())
        {
            juce::ColourGradient fillGrad(
                ModernLookAndFeel::Colors::textSecondary.withAlpha(0.28f), 0, graphBounds.getY(),
                ModernLookAndFeel::Colors::textSecondary.withAlpha(0.05f), 0, graphBounds.getBottom(), false);
            ig.setGradientFill(fillGrad);
            ig.fillPath(cachedPreFill);
        }
        if (!cachedPreLine.isEmpty())
        {
            ig.setColour(ModernLookAndFeel::Colors::textSecondary.withAlpha(0.55f));
            ig.strokePath(cachedPreLine, juce::PathStrokeType(1.4f));
        }

        // Post line
        if (!cachedPostLine.isEmpty())
        {
            juce::Colour front = ModernLookAndFeel::Colors::accentGreen.brighter(0.05f);
            juce::ColourGradient postGrad(
                ModernLookAndFeel::Colors::accentYellow.withAlpha(0.85f), 0, graphBounds.getY(),
                front.withAlpha(0.85f), 0, graphBounds.getBottom(), false);
            ig.setGradientFill(postGrad);
            ig.strokePath(cachedPostLine, juce::PathStrokeType(1.8f));
        }

        // Delta line
        if (!cachedDeltaLine.isEmpty())
        {
            ig.setColour(ModernLookAndFeel::Colors::accentCyan.withAlpha(0.8f));
            ig.strokePath(cachedDeltaLine, juce::PathStrokeType(1.6f));
        }

        // Peak-hold line
        if (!cachedPeakLine.isEmpty())
        {
            ig.setColour(ModernLookAndFeel::Colors::accentOrange.withAlpha(0.9f));
            ig.strokePath(cachedPeakLine, juce::PathStrokeType(1.2f));
        }
    }

    void drawGrid(juce::Graphics& g)
    {
        // === Ultra-subtle frequency grid lines ===
        const float freqs[] = { 20, 30, 40, 50, 60, 80, 100, 200, 300, 400, 500, 600, 800,
                                1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 20000 };
        for (float f : freqs)
        {
            float x = freqToX(f);
            if (x < graphBounds.getX() || x > graphBounds.getRight()) continue;
            
            bool major = (f == 100 || f == 1000 || f == 10000);
            bool decade = (f == 20 || f == 200 || f == 2000 || f == 20000);
            
            if (major || decade)
                g.setColour(juce::Colour(0xFF1E1E2A));  // Slightly visible
            else
                g.setColour(juce::Colour(0xFF151520));  // Nearly invisible
            
            g.drawVerticalLine((int)x, graphBounds.getY(), graphBounds.getBottom());
        }
        
        // === Horizontal dB lines — finer grid ===
        for (float db = spectrumMinDb; db <= spectrumMaxDb; db += 6.0f)
        {
            float y = dbToY(db);
            bool isZero = std::abs(db) < 0.01f;
            bool isMajor = (std::fmod(std::abs(db), 12.0f) < 0.01f);
            
            if (isZero)
            {
                // 0 dB line — slightly brighter, dashed feel
                g.setColour(juce::Colour(0xFF2A2A3A));
                g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
            }
            else if (isMajor)
            {
                g.setColour(juce::Colour(0xFF1A1A28));
                g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
            }
            else
            {
                g.setColour(juce::Colour(0xFF131320));
                g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
            }
        }
    }

    void drawSpectrum(juce::Graphics& g)
    {
        // Bug L fix: null-check before deref (parameter may not exist during teardown)
        auto* preP  = processor.getAPVTS().getRawParameterValue("showPreSpectrum");
        auto* postP = processor.getAPVTS().getRawParameterValue("showPostSpectrum");
        bool showPre  = preP  ? preP->load()  > 0.5f : false;
        bool showPost = postP ? postP->load() > 0.5f : false;
        bool showDelta = isDeltaEnabled();

        // Lock protects frozenSpectrum, capturedSpectrum, isFrozen, hasCaptured, capturedPathDirty
        // against concurrent writes from message thread button callbacks.
        // SpinLock is non-blocking — the render thread will spin briefly if the message
        // thread is mid-copy (< 1μs for a vector assignment).
        const juce::SpinLock::ScopedTryLockType lock(spectrumDataLock);
        if (!lock.isLocked())
            return;  // Skip this frame rather than block — next frame will catch up

        bool showCaptured = showCapturedButton.getToggleState() && hasCaptured;

        const auto& preBuffer = (isFrozen && !frozenSpectrum.empty()) ? frozenSpectrum : smoothedSpectrum;
        if (preBuffer.empty() && !showCaptured && !(showPost && !isFrozen))
            return;

        //----------------------------------------------------------------------
        // Captured spectrum (orange dashed)
        //----------------------------------------------------------------------
        if (showCaptured && !capturedSpectrum.empty())
        {
            // Use cached dashed path to avoid expensive createDashedStroke every frame
            if (capturedPathDirty || cachedCapturedDash.isEmpty())
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

                cachedCapturedDash.clear();
                float dashLengths[] = { 4.0f, 4.0f };
                juce::PathStrokeType stroke(1.5f);
                stroke.createDashedStroke(cachedCapturedDash, capturedPath, dashLengths, 2);
                capturedPathDirty = false;
            }

            g.setColour(juce::Colour(0xFFE6A23C).withAlpha(0.6f));
            g.strokePath(cachedCapturedDash, juce::PathStrokeType(1.5f));
        }

        //----------------------------------------------------------------------
        // Frozen spectrum (blue indicator)
        //----------------------------------------------------------------------

        if (isFrozen && !frozenSpectrum.empty())
        {
            const size_t frozenUsable = std::min(frozenSpectrum.size(),
                static_cast<size_t>(std::max(0, (int)graphBounds.getWidth())));
            smoothYBuffer.resize(frozenUsable);
            for (size_t i = 0; i < frozenUsable; ++i)
            {
                float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                smoothYBuffer[i] = dbToY(applyTilt(frozenSpectrum[i], freq));
            }

            juce::Path frozenLinePath, frozenFillPath;
            pathBuilder.build(smoothYBuffer.data(), frozenUsable,
                                     graphBounds.getX(), graphBounds.getBottom(),
                                     frozenLinePath, &frozenFillPath, 3);

            juce::ColourGradient frozenGrad(
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.35f), 0, graphBounds.getY(),
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.03f), 0, graphBounds.getBottom(), false);
            g.setGradientFill(frozenGrad);
            g.fillPath(frozenFillPath);

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

        //----------------------------------------------------------------------
        // Live spectrum (pre, post, delta, peak) — single image blit
        // [FIX 2b: render-to-image, ~0.5ms instead of ~17ms]
        //----------------------------------------------------------------------
        if (!spectrumImageCache.isNull())
        {
            g.drawImageAt(spectrumImageCache,
                          static_cast<int>(graphBounds.getX()),
                          static_cast<int>(graphBounds.getY()));
        }
    }

    /** Per-band colored gradient fill under the EQ curve */
    void drawEQCurveFill(juce::Graphics& g)
    {
        if (!processor.isProcessorReady() || graphBounds.isEmpty())
            return;

        // Skip during band drag — the fill is a visual nicety, not essential for interaction
        if (isDraggingBand)
            return;

        rebuildEQCurvePath();

        if (cachedEQCurve.isEmpty())
            return;

        // Single unified fill under the entire EQ curve instead of per-band fills.
        // Per-band fills with individual getMagnitudeForFrequency loops cost ~20ms with 12+ bands.
        // A single fillPath on the cached curve + gradient costs ~1ms.
        juce::Path fillPath(cachedEQCurve);
        // Close the path along the 0 dB center line to create a filled area
        float zeroY = gainToY(0.0f);
        fillPath.lineTo(graphBounds.getRight(), zeroY);
        fillPath.lineTo(graphBounds.getX(), zeroY);
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad(
            juce::Colour(0xFFB0C8E8).withAlpha(0.12f), 0, graphBounds.getY(),
            juce::Colour(0xFFB0C8E8).withAlpha(0.02f), 0, zeroY, false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);
    }

    void drawEQCurve(juce::Graphics& g)
    {
        if (!processor.isProcessorReady())
            return;

        rebuildEQCurvePath();

        if (cachedEQCurve.isEmpty())
            return;

        if (!isDraggingBand)
        {
            // === GLOW LAYER 1: Wide soft outer glow ===
            g.setColour(juce::Colour(0xFFB0C8E8).withAlpha(0.08f));
            g.strokePath(cachedEQCurve, juce::PathStrokeType(10.0f, juce::PathStrokeType::mitered,
                                                               juce::PathStrokeType::rounded));

            // === GLOW LAYER 2: Medium glow ===
            g.setColour(juce::Colour(0xFFB0C8E8).withAlpha(0.16f));
            g.strokePath(cachedEQCurve, juce::PathStrokeType(5.0f, juce::PathStrokeType::mitered,
                                                               juce::PathStrokeType::rounded));
        }

        // === MAIN CURVE: Bright crisp line ===
        g.setColour(juce::Colour(0xFFE8F0FA));
        g.strokePath(cachedEQCurve, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered,
                                                           juce::PathStrokeType::rounded));
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
        const int limit = std::min(active, maxBands);

        auto drawOne = [&](int i)
        {
            auto state = processor.getBandState(i);
            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);

            if (x < graphBounds.getX() - 20 || x > graphBounds.getRight() + 20)
                return;

            juce::Colour col = bandColors[i];
            const bool isSelected = (i == selectedBandIndex);
            const bool isHovered = (i == hoveredBandIndex);
            const bool isDragging = (isDraggingBand && i == draggedBandIndex);

            // === FabFilter-style band node ===
            float baseRadius = state.enabled ? 11.0f : 5.0f;
            float radius = baseRadius;
            if (isDragging) radius = baseRadius + 4.0f;
            else if (isSelected) radius = baseRadius + 2.0f;
            else if (isHovered) radius = baseRadius + 2.0f;

            // Vertical guide line from 0dB to node (subtle)
            if (state.enabled && std::abs(state.gain) > 0.3f)
            {
                float zeroY = gainToY(0.0f);
                g.setColour(col.withAlpha(0.15f));
                g.drawLine(x, zeroY, x, y, 1.0f);
            }

            // === GLOW (selected/hovered/dragging) — skip non-dragged glow during drag for perf ===
            if (isDragging)
            {
                g.setColour(col.withAlpha(0.10f));
                g.fillEllipse(x - radius - 10, y - radius - 10, (radius + 10) * 2, (radius + 10) * 2);
                g.setColour(col.withAlpha(0.18f));
                g.fillEllipse(x - radius - 5, y - radius - 5, (radius + 5) * 2, (radius + 5) * 2);
            }
            else if (!isDraggingBand && (isSelected || isHovered))
            {
                g.setColour(col.withAlpha(0.12f));
                g.fillEllipse(x - radius - 6, y - radius - 6, (radius + 6) * 2, (radius + 6) * 2);
            }

            // Solo badge — skip during drag (font creation is expensive)
            if (state.solo && !isDraggingBand)
            {
                juce::Rectangle<float> badge(x + radius, y - radius - 4, 16.0f, 12.0f);
                g.setColour(ModernLookAndFeel::Colors::accentYellow.withAlpha(0.9f));
                g.fillRoundedRectangle(badge, 3.0f);
                g.setColour(ModernLookAndFeel::Colors::bgDark);
                g.setFont(soloBadgeFont);
                g.drawText("S", badge, juce::Justification::centred);
            }

            if (state.enabled)
            {
                // === SEMI-TRANSPARENT FILL ===
                g.setColour(col.withAlpha(isDragging ? 0.65f : (isSelected ? 0.55f : 0.40f)));
                g.fillEllipse(x - radius, y - radius, radius * 2, radius * 2);

                // Luminous border ring
                g.setColour(col.withAlpha(isDragging ? 1.0f : (isSelected ? 0.9f : 0.7f)));
                g.drawEllipse(x - radius, y - radius, radius * 2, radius * 2,
                             isDragging ? 2.5f : 1.8f);

                // Band number — skip during drag (setFont + drawText per band is expensive)
                if (!isDraggingBand)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.9f));
                    g.setFont(bandNumberFont);
                    g.drawText(juce::String(i + 1),
                              static_cast<int>(x - radius), static_cast<int>(y - radius),
                              static_cast<int>(radius * 2), static_cast<int>(radius * 2),
                              juce::Justification::centred);
                }
            }
            else
            {
                // Disabled: tiny ring only
                g.setColour(col.withAlpha(0.20f));
                g.fillEllipse(x - 4, y - 4, 8, 8);
                g.setColour(col.withAlpha(0.40f));
                g.drawEllipse(x - 4, y - 4, 8, 8, 1.0f);
            }
        };

        for (int i = 0; i < limit; ++i)
        {
            if (i == selectedBandIndex)
                continue;
            drawOne(i);
        }
        if (selectedBandIndex >= 0 && selectedBandIndex < limit)
            drawOne(selectedBandIndex);

        // Tooltip for selected band — FabFilter-style floating info panel (skip during drag for perf)
        if (!isDraggingBand && selectedBandIndex >= 0 && selectedBandIndex < AIEqualizerAudioProcessor::maxBands)
        {
            auto state = processor.getBandState(selectedBandIndex);
            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);

            juce::String freqStr = state.frequency >= 1000
                ? juce::String(state.frequency / 1000.0f, 2) + " kHz"
                : juce::String(static_cast<int>(state.frequency)) + " Hz";

            // Filter type name
            const char* typeNames[] = { "Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass" };
            juce::String typeName = (state.type >= 0 && state.type < 7)
                ? typeNames[state.type] : "Peak";

            int tw = 150, th = 52;
            int tx = static_cast<int>(juce::jlimit(graphBounds.getX(), graphBounds.getRight() - tw, x - tw / 2));
            int ty = static_cast<int>(y) - th - 14;
            if (ty < graphBounds.getY() + 5) ty = static_cast<int>(y) + 20;

            // Drop shadow
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRoundedRectangle(static_cast<float>(tx + 2), static_cast<float>(ty + 2),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f);

            // Background
            g.setColour(juce::Colour(0xF0181822));
            g.fillRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f);

            // Band color accent bar on left
            auto bandCol = bandColors[static_cast<size_t>(selectedBandIndex)];
            g.setColour(bandCol);
            g.fillRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   3.0f, static_cast<float>(th), 6.0f);

            // Border
            g.setColour(bandCol.withAlpha(0.4f));
            g.drawRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f, 1.0f);

            // Line 1: Band number + type
            g.setColour(bandCol);
            auto boldFont = juce::Font(juce::FontOptions().withHeight(11.0f));
            boldFont.setBold(true);
            g.setFont(boldFont);
            g.drawText("Band " + juce::String(selectedBandIndex + 1) + "  " + typeName,
                       tx + 8, ty + 4, tw - 14, 14, juce::Justification::centredLeft);

            // Line 2: Freq | Gain | Q — values prominent
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
            g.drawText(freqStr, tx + 8, ty + 20, 60, 14, juce::Justification::centredLeft);

            juce::String gainStr = (state.gain >= 0 ? "+" : "") + juce::String(state.gain, 1) + " dB";
            g.drawText(gainStr, tx + 60, ty + 20, 50, 14, juce::Justification::centred);

            // Line 3: Q value
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
            g.drawText("Q: " + juce::String(state.q, 2), tx + 8, ty + 36, 60, 12, juce::Justification::centredLeft);
        }
    }
    
public:
    void drawLabels(juce::Graphics& g)
    {
        // === dB labels (left side, small and subtle) ===
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        
        for (float db = spectrumMinDb; db <= spectrumMaxDb; db += 12.0f)
        {
            float y = dbToY(db);
            bool isZero = std::abs(db) < 0.01f;
            g.setColour(isZero ? juce::Colour(0xFF606078) : juce::Colour(0xFF3A3A4A));
            juce::String txt = juce::String((int)db);
            g.drawText(txt, 4, (int)y - 6, 34, 12, juce::Justification::centredRight);
        }

        if (isPianoRollEnabled())
        {
            drawPianoRollOverlay(g);
            return;
        }

        // === Frequency labels (bottom, refined) ===
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        
        const std::pair<float, const char*> freqLabels[] = {
            {20.0f,"20"}, {50.0f,"50"}, {100.0f,"100"}, {200.0f,"200"}, {500.0f,"500"},
            {1000.0f,"1k"}, {2000.0f,"2k"}, {5000.0f,"5k"}, {10000.0f,"10k"}, {20000.0f,"20k"}
        };
        for (auto& [f, lbl] : freqLabels)
        {
            float x = freqToX(f);
            if (x >= graphBounds.getX() && x <= graphBounds.getRight())
            {
                bool isMajor = (f == 100 || f == 1000 || f == 10000);
                g.setColour(isMajor ? juce::Colour(0xFF505068) : juce::Colour(0xFF3A3A4A));
                g.drawText(lbl, (int)x - 15, (int)graphBounds.getBottom() + 3, 30, 12, 
                          juce::Justification::centred);
            }
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
        // Map spectrum range to graph: 0 dB at center, -90 dB in lower half, +12 dB in upper half
        const float centerY = graphBounds.getY() + graphBounds.getHeight() * 0.5f;
        
        if (db >= 0.0f) {
            // Above 0 dB: map 0 → +12 to upper half
            float p = juce::jlimit(0.0f, 1.0f, db / spectrumMaxDb);
            return centerY - p * (graphBounds.getHeight() * 0.5f);
        } else {
            // Below 0 dB: map -90 → 0 to lower half
            float p = juce::jlimit(0.0f, 1.0f, db / spectrumMinDb);
            return centerY + p * (graphBounds.getHeight() * 0.5f);
        }
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

    //==========================================================================
    // Smooth spectrum path builder — Catmull-Rom spline subsampled
    // Instead of 1 lineTo per pixel, subsample every `step` pixels and
    // connect with quadratic beziers for a smooth, premium look.
    //==========================================================================
    struct SmoothPathBuilder
    {
        // Persistent scratch buffers — eliminates heap allocations per call
        std::vector<float> blurTemp;
        struct Pt { float x, y; };
        std::vector<Pt> pts;

        // In-place box blur (variable radius based on position — wider at high freq)
        void boxBlur(float* data, size_t count, int baseRadius = 2)
        {
            if (count < 8 || baseRadius < 1) return;
            if (blurTemp.size() < count) blurTemp.resize(count);
            for (size_t i = 0; i < count; ++i)
            {
                // Wider blur at right side (high freq, sparser bins)
                float t = static_cast<float>(i) / static_cast<float>(count);
                int r = baseRadius + static_cast<int>(t * t * 6.0f); // 2..8px radius
                int lo = static_cast<int>(i) - r;
                int hi = static_cast<int>(i) + r;
                if (lo < 0) lo = 0;
                if (hi >= static_cast<int>(count)) hi = static_cast<int>(count) - 1;
                float sum = 0.0f;
                for (int j = lo; j <= hi; ++j)
                    sum += data[j];
                blurTemp[i] = sum / static_cast<float>(hi - lo + 1);
            }
            std::memcpy(data, blurTemp.data(), count * sizeof(float));
        }

        // Builds a smooth line path + optional fill path from raw Y values.
        // yValues[i] = Y coordinate at pixel offset i from graphBounds.getX()
        // step = pixels between control points (3-6 is good)
        void build(float* yValues, size_t count,
                   float startX, float bottomY,
                   juce::Path& linePath, juce::Path* fillPath,
                   int step = 4, bool applyBlur = true)
        {
            if (count < 4) return;

            // Apply adaptive box blur to smooth staircase artifacts from FFT bins
            if (applyBlur)
                boxBlur(yValues, count, 2);

            // Collect control points by subsampling — reuse pts vector
            pts.clear();
            const size_t needed = count / static_cast<size_t>(step) + 2;
            if (pts.capacity() < needed) pts.reserve(needed);

            for (size_t i = 0; i < count; i += static_cast<size_t>(step))
                pts.push_back({ startX + static_cast<float>(i), yValues[i] });

            // Always include the last point
            if (pts.back().x < startX + static_cast<float>(count - 1))
                pts.push_back({ startX + static_cast<float>(count - 1), yValues[count - 1] });

            if (pts.size() < 2) return;

            // Pre-allocate path storage (avoids incremental realloc inside JUCE Path)
            linePath.preallocateSpace(static_cast<int>(pts.size()) * 3 + 4);
            if (fillPath)
                fillPath->preallocateSpace(static_cast<int>(pts.size()) * 3 + 8);

            // Start paths
            linePath.startNewSubPath(pts[0].x, pts[0].y);
            if (fillPath)
            {
                fillPath->startNewSubPath(startX, bottomY);
                fillPath->lineTo(pts[0].x, pts[0].y);
            }

            // Catmull-Rom → cubic bezier conversion
            const size_t n = pts.size();
            for (size_t i = 0; i + 1 < n; ++i)
            {
                const auto& p0 = pts[i == 0 ? 0 : i - 1];
                const auto& p1 = pts[i];
                const auto& p2 = pts[i + 1];
                const auto& p3 = pts[i + 1 < n - 1 ? i + 2 : i + 1];

                // Convert Catmull-Rom to cubic bezier control points
                float cp1x = p1.x + (p2.x - p0.x) / 6.0f;
                float cp1y = p1.y + (p2.y - p0.y) / 6.0f;
                float cp2x = p2.x - (p3.x - p1.x) / 6.0f;
                float cp2y = p2.y - (p3.y - p1.y) / 6.0f;

                linePath.cubicTo(cp1x, cp1y, cp2x, cp2y, p2.x, p2.y);
                if (fillPath)
                    fillPath->cubicTo(cp1x, cp1y, cp2x, cp2y, p2.x, p2.y);
            }

            if (fillPath)
            {
                fillPath->lineTo(startX + static_cast<float>(count - 1), bottomY);
                fillPath->closeSubPath();
            }
        }
    };

private:
    // ── Dynamic GR smoothing — called from timerCallback ─────────────────────
    // Reads instantaneous GR per band from the DSP meter cache (lock-free),
    // applies 1-pole smoothing (fast attack ~3 frames, slow release ~10 frames),
    // and marks the dynamic path dirty if values changed enough to be worth redrawing.
    void updateDynamicGRSmoothing()
    {
        if (!processor.isProcessorReady()) return;

        const int numActive = processor.getNumActiveBands();
        const int limit     = std::min(numActive, AIEqualizerAudioProcessor::maxBands);
        const auto& dynProc = processor.getDynamicEQProcessor();
        bool hasAny = false;

        for (int i = 0; i < limit; ++i)
        {
            const auto dynParams = dynProc.getBandParams(i);
            if (dynParams.dynamicMode == DynamicEQProcessor::DynamicMode_Off || !dynParams.enabled)
            {
                dynGRSmoothed[static_cast<size_t>(i)] *= 0.85f; // decay to zero when deactivated
                continue;
            }

            const float gr = processor.getDynamicBandMeter(i).gainReduction; // negative = compressing
            float& s = dynGRSmoothed[static_cast<size_t>(i)];
            const float coeff = (std::abs(gr) > std::abs(s)) ? 0.55f : 0.12f; // fast attack, slow release
            s = s * (1.0f - coeff) + gr * coeff;

            if (std::abs(s) > 0.05f)
                hasAny = true;
        }

        anyDynamicBandActive = hasAny;
    }

    // Rebuilds the dynamic EQ curve, storing per-point X/Y coordinates for
    // both static and dynamic curves. These are used to build the fill path
    // between the two curves (the pulsing GR overlay).
    void rebuildDynamicEQCurvePath()
    {
        if (graphBounds.isEmpty()) return;

        const int numActive = processor.getNumActiveBands();
        const int limit     = std::min(numActive, AIEqualizerAudioProcessor::maxBands);
        const auto& dynProc = processor.getDynamicEQProcessor();

        // Build per-band gain offsets from smoothed GR
        std::array<float, AIEqualizerAudioProcessor::maxBands> offsets {};
        for (int i = 0; i < limit; ++i)
        {
            const auto p = dynProc.getBandParams(i);
            if (p.dynamicMode != DynamicEQProcessor::DynamicMode_Off && p.enabled)
                offsets[static_cast<size_t>(i)] = dynGRSmoothed[static_cast<size_t>(i)];
        }

        ensureEQCurveFrequencies();
        dynCurveMagnitudes.resize(eqCurveFrequencies.size(), 1.0f);

        auto& eq  = processor.getEQProcessor();
        double sr = processor.getSampleRate();
        if (sr <= 0) sr = 44100.0;

        eq.getMagnitudeForFrequencyArrayWithGainOffsets(
            eqCurveFrequencies.data(), dynCurveMagnitudes.data(),
            eqCurveFrequencies.size(), sr,
            offsets.data(), static_cast<int>(offsets.size()));

        // Store per-point X and Y for both curves
        const size_t n = eqCurveFrequencies.size();
        dynCurveXPoints.resize(n);
        dynCurveYPoints.resize(n);
        staticCurveYPoints.resize(n);

        cachedDynamicEQCurve.clear();
        bool started = false;

        for (size_t i = 0; i < n; ++i)
        {
            const float x = freqToX(eqCurveFrequencies[i]);
            dynCurveXPoints[i] = x;

            // Dynamic curve Y
            float dynDb = juce::Decibels::gainToDecibels(dynCurveMagnitudes[i], -48.0f);
            dynDb = juce::jlimit(-24.0f, 24.0f, dynDb);
            dynCurveYPoints[i] = gainToY(dynDb);  // Use gainToY for consistent centering

            // Static curve Y (from already-computed magnitudes)
            float statDb = (i < eqCurveMagnitudes.size())
                ? juce::Decibels::gainToDecibels(eqCurveMagnitudes[i], -48.0f) : 0.0f;
            statDb = juce::jlimit(-24.0f, 24.0f, statDb);
            staticCurveYPoints[i] = gainToY(statDb);  // Use gainToY for consistent centering

            if (!started) { cachedDynamicEQCurve.startNewSubPath(x, dynCurveYPoints[i]); started = true; }
            else cachedDynamicEQCurve.lineTo(x, dynCurveYPoints[i]);
        }

        dynGRAtLastRebuild = dynGRSmoothed;
    }

    // ── Draw dynamic GR overlay (TDR Nova style) ─────────────────────────────
    //
    // 1. Fills the area BETWEEN the static and dynamic curves with warm amber
    // 2. Draws the dynamic (live) curve as a white line
    //
    // The fill is built manually: trace static curve left→right along the top,
    // then dynamic curve right→left along the bottom, close the path.
    // This creates the correct enclosed area regardless of which curve is above.
    void drawDynamicGROverlay(juce::Graphics& g)
    {
        if (!processor.isProcessorReady() || graphBounds.isEmpty()) return;
        if (!anyDynamicBandActive) return;

        // Ensure static curve is built
        rebuildEQCurvePath();

        // Check if GR values changed enough to warrant a dynamic path rebuild
        bool needsRebuild = cachedDynamicEQCurve.isEmpty();
        if (!needsRebuild)
        {
            const int limit = std::min(processor.getNumActiveBands(), AIEqualizerAudioProcessor::maxBands);
            for (int i = 0; i < limit; ++i)
            {
                if (std::abs(dynGRSmoothed[static_cast<size_t>(i)] - dynGRAtLastRebuild[static_cast<size_t>(i)]) > 0.05f)
                { needsRebuild = true; break; }
            }
        }
        if (needsRebuild)
            rebuildDynamicEQCurvePath();

        const size_t n = dynCurveXPoints.size();
        if (n < 2) return;

        // ── Build fill path between the two curves ──
        // Trace: static curve left→right, then dynamic curve right→left, close
        juce::Path fillArea;
        fillArea.startNewSubPath(dynCurveXPoints[0], staticCurveYPoints[0]);
        for (size_t i = 1; i < n; ++i)
            fillArea.lineTo(dynCurveXPoints[i], staticCurveYPoints[i]);
        // Now go back right→left along the dynamic curve
        for (int i = static_cast<int>(n) - 1; i >= 0; --i)
            fillArea.lineTo(dynCurveXPoints[static_cast<size_t>(i)], dynCurveYPoints[static_cast<size_t>(i)]);
        fillArea.closeSubPath();

        // Warm amber fill, ~20% alpha
        const juce::Colour overlayColour { 0xFFC89040u };
        g.setColour(overlayColour.withAlpha(0.20f));
        g.fillPath(fillArea);

        // Dynamic (live) curve — bright white line
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.strokePath(cachedDynamicEQCurve,
                     juce::PathStrokeType(1.8f, juce::PathStrokeType::mitered,
                                          juce::PathStrokeType::rounded));
    }

    void rebuildEQCurvePath()
    {
        const uint64_t version = processor.getEQCurveChangeCounter();
        const bool boundsChanged = (lastCurveBounds != graphBounds);

        if (!eqCurveDirty && !boundsChanged && version == lastEQVersion && !cachedEQCurve.isEmpty())
            return;

        // Throttle rebuilds to ~30Hz during drag. getMagnitudeForFrequencyArray on 600 freqs
        // costs ~3-5ms. At 60fps that's 50% of the budget just for EQ curve math.
        if (isDraggingBand && !boundsChanged)
        {
            double now = juce::Time::getMillisecondCounterHiRes();
            if (now - lastEQCurveRebuildTime < 33.0) // 30Hz
                return;
            lastEQCurveRebuildTime = now;
        }

#if AIEQ_GUI_DEBUG
        debugEQCurveRebuildCount++;
#endif
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

        bool started = false;

        for (size_t i = 0; i < eqCurveFrequencies.size(); ++i)
        {
            float db = juce::Decibels::gainToDecibels(eqCurveMagnitudes[i], -48.0f);
            db = juce::jlimit(-24.0f, 24.0f, db);

            const float x = freqToX(eqCurveFrequencies[i]);
            const float y = gainToY(db);  // Use gainToY for consistent centering

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
        if (eqCurveFrequencies.size() == eqCurvePointCount)
            return;
        eqCurveFrequencies.clear();

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

    // Metrological pipeline injection (set via injectPrecomputedSpectrum)
    std::vector<float> injectedPostSpectrum;
    uint64_t injectedSpectrumVersion = 0;
    uint64_t lastInjectedVersion = 0;

    // Click detector overlay
    uint32_t     clickOverlayCount = 0;
    juce::String clickOverlayCP;
    int hoverX = -1, hoverY = -1;
    
    // Freeze/Capture functionality
    juce::TextButton freezeButton, captureButton, clearButton;
    juce::ToggleButton showCapturedButton;
    juce::SpinLock spectrumDataLock;  // protects frozen/captured spectrum data vs OpenGL render thread
    std::vector<float> frozenSpectrum;
    std::vector<float> capturedSpectrum;
    bool isFrozen = false;
    bool hasCaptured = false;

    // Spectrum smoothing
    SpectrumSpeed spectrumSpeed = SpectrumSpeed::Medium;
    float displayReleaseCoeff = 0.70f; // Default: Medium (premium smooth)
    int captureTextTimer = 0;
    int peakRefreshCounter = 0;
    juce::Path cachedCapturedDash;
    bool capturedPathDirty = true;
    std::vector<float> smoothYBuffer; // Reusable Y-coord buffer for smooth path builder
    mutable SmoothPathBuilder pathBuilder; // Persistent instance — zero heap alloc per frame
    // Spectrum buffers are grown on demand; no shrinking to avoid per-frame realloc
    int draggedBandIndex = -1;
    
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

    // FIX 4: Off-screen grid + labels cache (static between resizes)
    juce::Image gridCache;
    bool gridCacheDirty = true;

    // FIX 2: Cached spectrum paths — rebuilt only when spectrum version changes
    juce::Path cachedPreLine, cachedPreFill;
    juce::Path cachedPostLine;
    juce::Path cachedDeltaLine;
    juce::Path cachedPeakLine;
    juce::Path cachedFrozenLine, cachedFrozenFill;
    uint64_t lastPreSpectrumVersion = 0;
    uint64_t lastPostSpectrumVersion = 0;
    uint64_t prevRepaintPreVer = 0;
    uint64_t prevRepaintPostVer = 0;
    juce::Rectangle<float> lastSpectrumBounds;

    // FIX SPEC-IMAGE: Off-screen image cache for spectrum rendering
    // Paths are cheap to cache, but strokePath+fillPath with gradients is expensive (~17ms).
    // Render all spectrum visuals into an image, rebuild only when spectrum version changes.
    juce::Image spectrumImageCache;
    // (removed: lastSpectrumImage tracking — now handled by rebuildLiveSpectrumPaths)

    // Cached EQ curve to avoid per-pixel recomputation
    juce::Path cachedEQCurve;
    std::vector<float> eqCurveFrequencies;
    std::vector<float> eqCurveMagnitudes;
    juce::Rectangle<float> lastCurveBounds;
    uint64_t lastEQVersion = std::numeric_limits<uint64_t>::max();
    bool eqCurveDirty = true;
    double lastEQCurveRebuildTime = 0.0;
    static constexpr size_t eqCurvePointCount = 128;

    // Adaptive timer — tracks current rate to avoid redundant startTimerHz calls
    int currentTimerHz = 60;

    // ── Dynamic EQ GR overlay (TDR Nova style) ──────────────────────────────
    // Per-band smoothed gain reduction values (1-pole IIR)
    std::array<float, AIEqualizerAudioProcessor::maxBands> dynGRSmoothed {};
    // Cached dynamic EQ curve path + per-point coordinate arrays for fill construction
    juce::Path cachedDynamicEQCurve;
    std::vector<float> dynCurveMagnitudes;   // magnitude buffer (reused)
    std::vector<float> dynCurveXPoints;       // X pixel positions (shared between static & dynamic)
    std::vector<float> dynCurveYPoints;       // Y pixel positions (dynamic curve)
    std::vector<float> staticCurveYPoints;    // Y pixel positions (static curve, for fill)
    // Last smoothed GR values used to build the cached dynamic path
    std::array<float, AIEqualizerAudioProcessor::maxBands> dynGRAtLastRebuild {};
    // Whether any band has dynamic mode active
    bool anyDynamicBandActive = false;
    // ────────────────────────────────────────────────────────────────────────


    // Pre-cached fonts for band drawing — avoid Font construction per band per frame
    juce::Font bandNumberFont { juce::FontOptions().withHeight(10.0f).withStyle("Bold") };
    juce::Font soloBadgeFont  { juce::FontOptions().withHeight(8.0f).withStyle("Bold") };

#if AIEQ_GUI_DEBUG
    int debugPaintCount = 0;
    double debugPaintTimeAccum = 0.0;
    double debugMaxPaintTime = 0.0;
    double debugLastReportTime = 0.0;
    int debugEQCurveRebuildCount = 0;
    double debugBgAccum = 0, debugSpecAccum = 0, debugEQAccum = 0, debugBandsAccum = 0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedSpectrumDisplay)
};
