#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../PluginProcessor.h"
#include "AnalyzerContextMenu.h"
#include "BandRadialMenu.h"
#include "ModernLookAndFeel.h"
#include <algorithm>
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
        setWantsKeyboardFocus(true);
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
        
        // Spectrum toolbar — semi-transparent buttons that blend with the display.
        // Wave 4A: active state is amber to match the Liquid Intelligence palette.
        auto setupToolbarBtn = [](juce::TextButton& btn, const juce::String& text) {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, ModernLookAndFeel::Colors::bgLight.withAlpha(0.25f));
            btn.setColour(juce::TextButton::buttonOnColourId, ModernLookAndFeel::Colors::amber.withAlpha(0.55f));
            btn.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary.withAlpha(0.73f));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF181A22));
        };

        setupToolbarBtn(freezeButton, "FREEZE");
        freezeButton.setTooltip("Freeze the live analyzer display.");
        freezeButton.setClickingTogglesState(true);
        freezeButton.onClick = [this]() {
            bool frozen = freezeButton.getToggleState();
            if (frozen)
            {
                // Snapshot the displayed (lerped) frame, then stop live rebuilds.
                if (!displayPre.empty())
                    rebuildLiveSpectrumPaths(true);
                const juce::SpinLock::ScopedLockType lock(spectrumDataLock);
                frozenSpectrum = !displayPre.empty() ? displayPre : smoothedSpectrum;
                isFrozen = true;
            }
            else
            {
                const juce::SpinLock::ScopedLockType lock(spectrumDataLock);
                isFrozen = false;
            }
            refreshPeaks();
        };
        addAndMakeVisible(freezeButton);

        setupToolbarBtn(captureButton, "CAPTURE");
        captureButton.setTooltip("Snapshot the current spectrum for SHOW CAPT. Not AI Capture.");
        captureButton.onClick = [this]() {
            {
                const juce::SpinLock::ScopedLockType lock(spectrumDataLock);
                // Stable hop frame (t=1 / current inject), never an in-between lerp.
                capturedSpectrum = !hopCurrPre.empty() ? hopCurrPre : smoothedSpectrum;
                hasCaptured = true;
                capturedPathDirty = true;
            }
            captureButton.setButtonText("CAPTURED!");
            startTimer(100);
            currentTimerHz = 0; // next tick restores 60 Hz after the flash interval
        };
        addAndMakeVisible(captureButton);

        // 75x20 slot, 12px bold + 0.10 extra kerning: "SHOW CAPTURE" clips.
        showCapturedButton.setButtonText("SHOW CAPT");
        showCapturedButton.setTooltip(
            "Show the last captured spectrum as an orange dashed overlay. Use CAPTURE first.");
        showCapturedButton.setColour(juce::ToggleButton::textColourId, ModernLookAndFeel::Colors::textPrimary.withAlpha(0.73f));
        showCapturedButton.setColour(juce::ToggleButton::tickColourId, ModernLookAndFeel::Colors::accentYellow);
        addAndMakeVisible(showCapturedButton);

        setupToolbarBtn(clearButton, "CLEAR");
        clearButton.setTooltip("Clear the captured overlay and unfreeze.");
        clearButton.setColour(juce::TextButton::textColourOffId, ModernLookAndFeel::Colors::textPrimary.withAlpha(0.53f));
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

        // Hidden until a band node is right-clicked.  It is a child overlay,
        // not a native popup, so its bubble animation stays clipped to and
        // visually integrated with the graph.
        addChildComponent(bandRadialMenu);

        // Normal right-clicks on empty graph space use this animated,
        // graph-local menu. Alt/Option keeps the native JUCE fallback.
        addChildComponent(analyzerContextMenu);
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

    /** When true, the GL pipeline handles pre/post spectrum rendering.
     *  Software path skips pre/post path builds to avoid double work. */
    void setGLSpectrumActive(bool v) noexcept { glSpectrumActive = v; }
    bool isGLSpectrumActive() const noexcept { return glSpectrumActive; }

    /** AI breathing phase (0..2π, 4-second cycle) — fed from editor's timerCallback.
     *  Used to modulate glow alpha on band nodes that have pending AI corrections. */
    void setAIBreathingPhase(float phase) noexcept { aiBreathingPhase = phase; }

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
        else
            injectedPostSpectrum.clear();
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
                ModernLookAndFeel::Colors::bgDark.darker(0.15f), bounds.getX(), bounds.getY(),
                ModernLookAndFeel::Colors::bgDark.darker(0.4f), bounds.getX(), bounds.getBottom(), false);
            bgGrad.addColour(0.5, ModernLookAndFeel::Colors::bgDark.darker(0.25f));
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

        // Phase 7A/7B: AI zones + dashed suggestion curve (behind bands so
        // the draggable nodes stay on top).
        drawAIMarkers(g);
        drawAISuggestedCurve(g);  // Phase 7B: AI suggestion overlay (dashed, amber 0.60)

        drawEQBands(g);

        if (hoverX >= 0) drawHover(g);

        // Phase 7C: contextual tooltip drawn LAST so it sits on top of everything.
        drawTooltip(g);

#if AIEQ_GUI_DEBUG
        tBands = lap() - t0;
#endif

        // ── Tilt drag widget (bottom-left overlay) ─────────────────────
        {
            const float tiltVal = getAnalyzerSlopeDbPerOct();
            juce::String tiltText;
            if (tiltVal < 0.05f)
                tiltText = juce::String(juce::CharPointer_UTF8("TONAL TILT \xc2\xb7 FLAT"));
            else
                tiltText = juce::String(juce::CharPointer_UTF8("TONAL TILT \xc2\xb7 "))
                           + juce::String(tiltVal, 1) + " dB/oct";

            const auto font = juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold"));
            g.setFont(font);
            const float tw = static_cast<float>(font.getStringWidth(tiltText)) + 24.0f;
            const float th = 18.0f;
            const float tx = graphBounds.getX() + 6.0f;
            const float ty = graphBounds.getBottom() - th - 4.0f;
            tiltWidgetBounds = { tx, ty, tw, th };

            // Background pill
            const float bgAlpha = isDraggingTilt ? 0.55f : 0.30f;
            g.setColour(ModernLookAndFeel::Colors::bgLight.withAlpha(bgAlpha));
            g.fillRoundedRectangle(tiltWidgetBounds, 4.0f);
            g.setColour(ModernLookAndFeel::Colors::textPrimary.withAlpha(0.20f));
            g.drawRoundedRectangle(tiltWidgetBounds, 4.0f, 0.5f);

            // Up/down arrows icon (left side)
            const float arrowX = tx + 6.0f;
            const float arrowCY = ty + th * 0.5f;
            g.setColour(ModernLookAndFeel::Colors::textPrimary.withAlpha(isDraggingTilt ? 0.9f : 0.6f));
            // Up triangle
            juce::Path upArrow;
            upArrow.addTriangle(arrowX, arrowCY - 2.0f,
                                arrowX - 3.0f, arrowCY - 6.0f,
                                arrowX + 3.0f, arrowCY - 6.0f);
            g.fillPath(upArrow);
            // Down triangle
            juce::Path downArrow;
            downArrow.addTriangle(arrowX, arrowCY + 2.0f,
                                  arrowX - 3.0f, arrowCY + 6.0f,
                                  arrowX + 3.0f, arrowCY + 6.0f);
            g.fillPath(downArrow);

            // Text
            g.setColour(ModernLookAndFeel::Colors::textPrimary.withAlpha(isDraggingTilt ? 0.95f : 0.73f));
            g.drawText(tiltText, tiltWidgetBounds.withLeft(tx + 14.0f), juce::Justification::centredLeft);
        }

        // Subtle border
        g.setColour(ModernLookAndFeel::Colors::grid);
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
        bandRadialMenu.setBounds(getLocalBounds());
        analyzerContextMenu.setBounds(getLocalBounds());
    }
    
    void timerCallback() override
    {
        bandRadialMenu.advanceAnimation();
        analyzerContextMenu.advanceAnimation();

        // SAFETY: Skip processing if processor not ready
        if (!processor.isProcessorReady())
        {
            repaint();  // Still repaint background
            return;
        }

        // ── Adaptive timer rate ──
        // P1-5 motion experiment: visible + unfrozen display interpolates toward 60 Hz
        // between FFT hops. Hidden windows stay at 5 Hz. Frozen overlay does not
        // need hop interpolation (30 Hz is enough for node fades).
        {
            const bool windowVisible = isShowing();
            const int desiredHz = !windowVisible ? 5
                                : isFrozen       ? 30
                                                 : 60;
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
            updateDynamicGRSmoothing();

            bool rebuiltLive = false;
            if (injectedSpectrumVersion != 0)
            {
                const bool hopChanged = ingestAndAdvanceSpectrumLerp();
                const bool tMoved = std::abs(spectrumLerpT - lastRebuiltLerpT) > kLerpRebuildEpsilon
                                 || ((spectrumLerpT >= 0.999f) != (lastRebuiltLerpT >= 0.999f));
                const bool boundsChanged = (lastSpectrumBounds != graphBounds);
                // Rebuild in-between hop frames from lerped columns even if FFT version is unchanged.
                // Skip the ~6 ms path+image cost when t is holding at 1 waiting for the next hop.
                if (isShowing() && (hopChanged || tMoved || boundsChanged || lastRebuiltLerpT < 0.0f))
                {
                    rebuildLiveSpectrumPaths(true);
                    lastRebuiltLerpT = spectrumLerpT;
                    rebuiltLive = true;
                }
            }
            else
            {
                rebuildLiveSpectrumPaths(false);
                rebuiltLive = (lastPreSpectrumVersion != prevRepaintPreVer
                               || lastPostSpectrumVersion != prevRepaintPostVer);
            }

            if (rebuiltLive || isDraggingBand || hoverX >= 0)
            {
                prevRepaintPreVer = lastPreSpectrumVersion;
                prevRepaintPostVer = lastPostSpectrumVersion;
                needsRepaint = true;
            }
            // Throttle peak detection to ~10 Hz (every 6th frame at 60 Hz)
            if (++peakRefreshCounter >= 6)
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

        // FabFilter-style node fade: smoothly animate nodesOpacity toward target.
        // Fade-in ~200ms (6 frames @60Hz), fade-out ~400ms (12 frames).
        // While animating, force repaint so the transition is visible.
        {
            const float fadeInStep  = 1.0f / 6.0f;   // ~166ms @ 60Hz
            const float fadeOutStep = 1.0f / 12.0f;   // ~200ms @ 60Hz — brisk, not sluggish

            if (nodesOpacity < nodesTargetOpacity)
            {
                nodesOpacity = juce::jmin(nodesTargetOpacity, nodesOpacity + fadeInStep);
                needsRepaint = true;
            }
            else if (nodesOpacity > nodesTargetOpacity)
            {
                nodesOpacity = juce::jmax(nodesTargetOpacity, nodesOpacity - fadeOutStep);
                needsRepaint = true;
            }
            // While dragging, nodes MUST be fully visible (override fade-out)
            if (isDraggingBand)
                nodesOpacity = 1.0f;
        }

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

        // Change cursor when over a band or tilt widget
        if (tiltWidgetBounds.contains(e.position))
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        else if (hoveredBandIndex >= 0 || hoveredPeakIndex >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);

        // Phase 7C: show tooltip when hovering an AI zone.
        // Only considered when the cursor is inside the graph and not already
        // grabbing a band.
        if (processor.isProcessorReady() && !isDraggingBand && graphBounds.contains(e.position))
        {
            const auto corrections = processor.getAIEngine().getPendingCorrections();
            int hitIdx = -1;

            const float graphTop    = graphBounds.getY();
            const float graphHeight = graphBounds.getHeight();
            const double sr = (processor.getSampleRate() > 0.0) ? processor.getSampleRate() : 44100.0;
            const float nyquist = static_cast<float>(sr * 0.5);

            for (int i = 0; i < static_cast<int>(corrections.size()); ++i)
            {
                const auto& corr = corrections[(size_t) i];
                const float freq = corr.frequency;
                const float Q    = juce::jmax(0.01f, corr.suggestedQ);
                if (freq <= 0.0f || freq >= nyquist)
                    continue;

                const float ratio = std::pow(2.0f, 1.0f / (2.0f * Q));
                float xL = freqToX(freq / ratio);
                float xH = freqToX(freq * ratio);
                const float minWidth = 20.0f;
                if (xH - xL < minWidth)
                {
                    const float xC = freqToX(freq);
                    xL = xC - minWidth * 0.5f;
                    xH = xC + minWidth * 0.5f;
                }
                xL = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), xL);
                xH = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), xH);
                if (xH - xL < 2.0f)
                    continue;

                juce::Rectangle<float> zone(xL, graphTop, xH - xL, graphHeight);
                if (zone.contains(e.position))
                {
                    hitIdx = i;

                    // Hero Graph Polish v1.3 — hover identity must use the
                    // SAME criterion the FIX click resolver uses, otherwise
                    // in dense scenes two same-type corrections sitting close
                    // together can decouple three things: the active overlay
                    // highlight (which follows the live correctionIdx), the
                    // frozen tooltip text (snapshot-based), and the FIX
                    // re-match target (also snapshot-based). The user could
                    // then click FIX on what looks like the active zone and
                    // approve a different correction.
                    //
                    // Tolerances live in kTooltip*Max constants so this site
                    // and the FIX resolver below stay in lock-step. Auxiliary
                    // geometric guard is ANDed in to defend against the rare
                    // case of two distinct corrections sharing a near-identical
                    // payload but living far apart on the graph.
                    const float zoneCx = zone.getCentreX();
                    bool isSameCorrection = false;
                    if (aiTooltip.visible
                        && aiTooltip.snapshotType == corr.type
                        && aiTooltip.snapshotFrequency > 0.0f)
                    {
                        const float fRatio = std::abs(std::log2(freq
                                                                / juce::jmax(1.0f, aiTooltip.snapshotFrequency)));
                        const float gDiff  = std::abs(corr.suggestedGain - aiTooltip.snapshotSuggestedGain);
                        const float qDiff  = std::abs(Q                  - aiTooltip.snapshotSuggestedQ);
                        const float xDiff  = std::abs(zoneCx - aiTooltip.anchor.getCentreX());

                        isSameCorrection = fRatio <= kTooltipFreqRatioMax
                                        && gDiff  <= kTooltipGainDiffMaxDb
                                        && qDiff  <= kTooltipQDiffMax
                                        && xDiff  <= kTooltipPixelDriftMax;
                    }

                    if (!isSameCorrection)
                    {
                        // First show OR genuinely different correction:
                        // refresh anchor, payload and snapshot in one shot.
                        aiTooltip.anchor        = zone;
                        aiTooltip.title         = juce::String("AI Suggestion");
                        aiTooltip.description   = corr.description.isNotEmpty()
                            ? corr.description
                            : juce::String("Detected issue at ") + juce::String(freq, 0) + " Hz";
                        aiTooltip.suggestion    = juce::String("FIX: ")
                                                + juce::String(corr.suggestedGain, 1) + " dB @ Q "
                                                + juce::String(Q, 2);

                        aiTooltip.snapshotFrequency     = freq;
                        aiTooltip.snapshotSuggestedGain = corr.suggestedGain;
                        aiTooltip.snapshotSuggestedQ    = Q;
                        aiTooltip.snapshotType          = corr.type;

                        aiTooltip.correctionIdx = i;
                        if (!aiTooltip.visible)
                        {
                            aiTooltip.visible = true;
                            setMouseCursor(juce::MouseCursor::PointingHandCursor);
                        }
                        repaint();
                    }
                    else if (aiTooltip.correctionIdx != i)
                    {
                        // Same correction conceptually, but the live index
                        // moved (AI reshuffled the array). Keep the frozen
                        // anchor / payload, just update the index used by
                        // the overlay's Subtle Idle / Active highlight.
                        aiTooltip.correctionIdx = i;
                        repaint();
                    }
                    break;
                }
            }

            // Wave 4A: keep the tooltip visible if the cursor has moved INSIDE
            // the tooltip itself (user reaching for the FIX button). Previously
            // the tooltip disappeared the moment the mouse left the ambra zone,
            // making the FIX button unclickable.
            const bool mouseInsideTooltip = aiTooltip.visible
                && aiTooltip.tooltipBounds.contains(e.position);

            if (hitIdx < 0 && aiTooltip.visible && !mouseInsideTooltip)
            {
                aiTooltip.visible = false;
                aiTooltip.correctionIdx = -1;
                setMouseCursor(juce::MouseCursor::NormalCursor);
                repaint();
            }
            else if (mouseInsideTooltip)
            {
                // Show pointer cursor while hovering the FIX button region.
                if (aiTooltip.fixButtonBounds.contains(e.position))
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                else
                    setMouseCursor(juce::MouseCursor::NormalCursor);
            }
        }
        else if (aiTooltip.visible)
        {
            // Graph left, or dragging started — clear the tooltip unless the
            // cursor is still inside the tooltip rect (edge case: tooltip drawn
            // partially outside graphBounds).
            if (!aiTooltip.tooltipBounds.contains(e.position))
            {
                aiTooltip.visible = false;
                aiTooltip.correctionIdx = -1;
                repaint();
            }
        }
    }
    
    void mouseEnter(const juce::MouseEvent&) override
    {
        mouseInsideSpectrum = true;
        nodesTargetOpacity = 1.0f;
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoverX = -1;
        hoveredBandIndex = -1;
        hoveredPeakIndex = -1;
        mouseInsideSpectrum = false;
        // Don't fade out nodes if a context menu is open — the user
        // is still interacting. The menu callback clears the flag.
        // Fade to a reduced-but-visible opacity (FabFilter-style): nodes
        // dim when the cursor leaves the spectrum but never fully disappear.
        if (!bandContextMenuOpen)
            nodesTargetOpacity = kNodesIdleOpacity;
        setMouseCursor(juce::MouseCursor::NormalCursor);

        if (aiTooltip.visible)
        {
            aiTooltip.visible = false;
            aiTooltip.correctionIdx = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Tilt widget: click to start drag (left-click only)
        if (!e.mods.isPopupMenu() && tiltWidgetBounds.contains(e.position))
        {
            isDraggingTilt = true;
            tiltDragStartY = e.position.y;
            tiltDragStartValue = getAnalyzerSlopeDbPerOct();
            repaint();
            return;
        }

        // Right-click: show per-band context menu if clicking on a band,
        // otherwise show the global analyzer settings menu.
        if (e.mods.isPopupMenu())
        {
            const int clickedBand = getBandAtPosition(e.position);
            if (clickedBand >= 0)
            {
                selectedBandIndex = clickedBand;
                if (onBandSelected)
                    onBandSelected(clickedBand);
                // Alt/Option preserves the native JUCE menu as a discoverable
                // fallback.  The normal path is the in-graph radial menu.
                if (e.mods.isAltDown())
                    showClassicBandContextMenu(e.getPosition(), clickedBand);
                else
                    showBandRadialMenu(e.position, clickedBand);
            }
            else
            {
                if (e.mods.isAltDown())
                    showClassicAnalyzerContextMenu(e.getPosition());
                else
                    showAnimatedAnalyzerContextMenu(e.position);
            }
            return;
        }

        // Phase 7C: click on the FIX button inside the tooltip → approve
        // the correction. Check this BEFORE the band hit test so the click
        // never falls through to drag-start.
        //
        // Hero Graph Polish v1.1: do NOT trust aiTooltip.correctionIdx as the
        // final identity — between the moment the tooltip was shown and the
        // moment the user clicks FIX, the AI may have re-analysed and either
        // reordered, removed, or mutated the corrections vector. Re-resolve
        // the click against a fresh getPendingCorrections() snapshot using
        // the payload we captured when the tooltip first appeared (type +
        // log-frequency + gain/Q tolerance, score-ranked).
        if (aiTooltip.visible
            && aiTooltip.fixButtonBounds.contains(e.position))
        {
            if (processor.isProcessorReady()
                && aiTooltip.snapshotType != AIEngine::ProblemType::None
                && aiTooltip.snapshotFrequency > 0.0f)
            {
                const auto current = processor.getAIEngine().getPendingCorrections();
                int   bestIdx   = -1;
                float bestScore = std::numeric_limits<float>::max();

                for (int j = 0; j < (int) current.size(); ++j)
                {
                    const auto& c = current[(size_t) j];
                    if (c.type != aiTooltip.snapshotType)
                        continue;

                    const float fRatio = std::abs(std::log2(c.frequency
                                                            / juce::jmax(1.0f, aiTooltip.snapshotFrequency)));
                    const float gDiff  = std::abs(c.suggestedGain - aiTooltip.snapshotSuggestedGain);
                    const float qDiff  = std::abs(c.suggestedQ    - aiTooltip.snapshotSuggestedQ);

                    // Hard filters: same problem type plus payload tolerances
                    // shared with the mouseMove hover-identity test (single
                    // source of truth in kTooltip*Max). Anything outside is
                    // treated as a different correction.
                    if (fRatio > kTooltipFreqRatioMax
                        || gDiff > kTooltipGainDiffMaxDb
                        || qDiff > kTooltipQDiffMax)
                        continue;

                    const float score = fRatio * 8.0f + gDiff * 0.5f + qDiff * 0.25f;
                    if (score < bestScore) { bestScore = score; bestIdx = j; }
                }

                if (bestIdx >= 0)
                    processor.getAIEngine().approveCorrection(bestIdx);
                // else: snapshot no longer matches anything — silently dismiss.
            }

            aiTooltip.visible = false;
            aiTooltip.correctionIdx = -1;
            repaint();
            return;  // don't propagate to band-node dragging
        }

        if (!graphBounds.contains(e.position))
            return;

        // Prevent re-entrancy if a drag is already active
        if (isDraggingBand)
            return;

        // Check if clicking on a band (left-click only at this point)
        const int clickedBand = getBandAtPosition(e.position);

        // Alt+click (Option+click on macOS) on a band → instant delete
        // This is the FabFilter Pro-Q shortcut for fast workflow.
        if (clickedBand >= 0 && e.mods.isAltDown())
        {
            deleteBand(clickedBand);
            return;
        }

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
        if (bandRadialMenu.isOpen() && bandRadialMenu.isMarkingGestureActive())
        {
            bandRadialMenu.updateMarkingGesture(e.position);
            return;
        }

        // Tilt drag: vertical movement adjusts dB/oct continuously
        if (isDraggingTilt)
        {
            const float deltaY = tiltDragStartY - e.position.y; // up = positive
            const float sensitivity = 0.04f; // dB/oct per pixel
            float newTilt = juce::jlimit(0.0f, 8.0f, tiltDragStartValue + deltaY * sensitivity);
            setFloatParameter("spectrumTilt", newTilt);
            repaint();
            return;
        }

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
    
    void mouseUp(const juce::MouseEvent& e) override
    {
        // On macOS JUCE no longer reports rightButtonDown on the release event.
        // The menu therefore owns the gesture explicitly instead of inferring
        // it again from mouse-up modifiers.
        if (bandRadialMenu.isOpen() && bandRadialMenu.isMarkingGestureActive())
        {
            bandRadialMenu.endMarkingGesture(e.position);
            return;
        }

        if (isDraggingTilt)
        {
            isDraggingTilt = false;
            repaint();
            return;
        }
        isDraggingBand = false;
        draggedBandIndex = -1;
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Keyboard users retain the platform-native menu, whose semantics and
        // accessibility are provided by JUCE.  The radial menu remains a fast
        // pointer/marking-menu surface rather than replacing that fallback.
        if (key.getKeyCode() == juce::KeyPress::F10Key && key.getModifiers().isShiftDown())
        {
            if (selectedBandIndex >= 0 && selectedBandIndex < processor.getNumActiveBands())
            {
                const auto state = processor.getBandState(selectedBandIndex);
                showClassicBandContextMenu({ static_cast<int>(freqToX(state.frequency)),
                                             static_cast<int>(gainToY(state.gain)) },
                                           selectedBandIndex);
                return true;
            }
        }

        return false;
    }
    
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (!graphBounds.contains(e.position))
            return;

        // Right double-click: unreliable (context menu steals focus on first
        // right-click). Delete via Alt+click or context menu instead.
        if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
            return;

        const int clickedBand = getBandAtPosition(e.position);

        // ── LEFT DOUBLE-CLICK on existing band → reset gain to 0 dB ────────
        if (clickedBand >= 0)
        {
            auto state = processor.getBandState(clickedBand);
            state.gain = 0.0f;
            processor.setBandState(clickedBand, state);

            selectedBandIndex = clickedBand;
            if (onBandSelected)
                onBandSelected(clickedBand);
        }
        else
        {
            // Double-click on empty space → create / activate a new band
            float freq = quantizeFrequency(xToFreq((float)e.x));
            float gain = yToGain((float)e.y);

            // Prefer an already-disabled band slot; else pick lowest-gain
            int targetBand = -1;
            float minGain = std::numeric_limits<float>::max();
            const int numBands = processor.getNumActiveBands();

            // First pass: find a disabled slot (clean reuse)
            for (int i = 0; i < numBands; ++i)
            {
                auto s = processor.getBandState(i);
                if (!s.enabled)
                {
                    targetBand = i;
                    break;
                }
            }

            // Second pass: if none disabled, pick the one with smallest |gain|
            if (targetBand < 0)
            {
                for (int i = 0; i < numBands; ++i)
                {
                    auto s = processor.getBandState(i);
                    if (std::abs(s.gain) < minGain)
                    {
                        minGain = std::abs(s.gain);
                        targetBand = i;
                    }
                }
            }

            if (targetBand >= 0)
            {
                auto state = processor.getBandState(targetBand);
                state.frequency = freq;
                state.gain = gain;
                state.enabled = true;
                state.type = 2;   // Peak (sensible default for new bands)
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
        // Primary: continuous spectrumTilt float param (0-8 dB/oct)
        if (auto* p = processor.getAPVTS().getRawParameterValue("spectrumTilt"))
            return p->load();
        // Fallback: legacy analyzerSlope choice param
        if (auto* p = processor.getAPVTS().getRawParameterValue("analyzerSlope"))
        {
            const int v = static_cast<int>(p->load());
            if (v == 1) return 3.0f;
            if (v == 2) return 4.5f;
            if (v == 3) return 6.0f;
            return 0.0f;
        }
        return 4.5f; // default visual
    }

    bool isPeakHoldEnabled() const
    {
        if (auto* p = processor.getAPVTS().getRawParameterValue("showPeakHold"))
            return p->load() > 0.5f;
        return false;
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
        const auto& source = (isFrozen && !frozenSpectrum.empty()) ? frozenSpectrum
                           : (!displayPre.empty() ? displayPre : smoothedSpectrum);
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

    void setFloatParameter(const juce::String& paramID, float value)
    {
        if (auto* p = processor.getAPVTS().getParameter(paramID))
        {
            auto norm = p->convertTo0to1(value);
            p->beginChangeGesture();
            p->setValueNotifyingHost(norm);
            p->endChangeGesture();
        }
    }

    // ── FabFilter-style band deletion: disable + reset gain/Q to defaults ──
    void deleteBand(int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= processor.getNumActiveBands())
            return;

        auto state = processor.getBandState(bandIndex);
        state.enabled = false;
        state.gain = 0.0f;
        state.q = 1.0f;        // neutral Q
        processor.setBandState(bandIndex, state);

        // If the deleted band was selected, deselect
        if (selectedBandIndex == bandIndex)
            selectedBandIndex = -1;

        if (onBandSelected)
            onBandSelected(selectedBandIndex);

        repaint();
    }

    void executeBandContextAction(int bandIndex, BandRadialMenu::Command command)
    {
        if (bandIndex < 0 || bandIndex >= processor.getNumActiveBands())
            return;

        auto state = processor.getBandState(bandIndex);
        switch (command.type)
        {
            case BandRadialMenu::CommandType::setFilterType:
                if (command.value >= 0 && command.value < 7)
                {
                    state.type = command.value;
                    processor.setBandState(bandIndex, state);
                }
                break;

            case BandRadialMenu::CommandType::toggleEnabled:
                state.enabled = !state.enabled;
                processor.setBandState(bandIndex, state);
                break;

            case BandRadialMenu::CommandType::toggleSolo:
                state.solo = !state.solo;
                processor.setBandState(bandIndex, state);
                break;

            case BandRadialMenu::CommandType::resetGain:
                state.gain = 0.0f;
                processor.setBandState(bandIndex, state);
                break;

            case BandRadialMenu::CommandType::resetBand:
                state.gain = 0.0f;
                state.q = 1.0f;
                state.type = 2; // Peak
                state.solo = false;
                processor.setBandState(bandIndex, state);
                break;

            case BandRadialMenu::CommandType::deleteBand:
                deleteBand(bandIndex);
                break;
        }

        repaint();
    }

    void showBandRadialMenu(juce::Point<float> pointerPosition, int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= processor.getNumActiveBands())
            return;

        bandContextMenuOpen = true;
        nodesTargetOpacity = 1.0f;

        const auto state = processor.getBandState(bandIndex);
        const juce::Point<float> nodeAnchor(freqToX(state.frequency), gainToY(state.gain));
        const auto colour = bandColors[static_cast<size_t>(bandIndex)];

        bandRadialMenu.open(
            nodeAnchor,
            graphBounds,
            colour,
            state.enabled,
            state.solo,
            [this, bandIndex](BandRadialMenu::Command command)
            {
                executeBandContextAction(bandIndex, command);
            },
            [this]
            {
                bandContextMenuOpen = false;
                if (!mouseInsideSpectrum)
                    nodesTargetOpacity = kNodesIdleOpacity;
                repaint();
            });
        bandRadialMenu.beginMarkingGesture(pointerPosition);
    }

    // ── Platform-native fallback (Alt/Option-right-click or Shift+F10) ──────
    void showClassicBandContextMenu(juce::Point<int> pos, int bandIndex)
    {
        if (bandIndex < 0 || bandIndex >= processor.getNumActiveBands())
            return;

        bandContextMenuOpen = true;
        nodesTargetOpacity = 1.0f;

        auto state = processor.getBandState(bandIndex);

        juce::PopupMenu menu;

        // Band header (non-clickable label)
        menu.addSectionHeader("Band " + juce::String(bandIndex + 1));

        // Filter type submenu
        juce::PopupMenu typeMenu;
        const char* typeNames[] = { "Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass" };
        for (int t = 0; t < 7; ++t)
            typeMenu.addItem(100 + t, typeNames[t], true, state.type == t);
        menu.addSubMenu("Filter Type", typeMenu);

        menu.addSeparator();

        // Enable / Solo toggles
        menu.addItem(10, state.enabled ? "Disable Band" : "Enable Band");
        menu.addItem(11, state.solo ? "Unsolo" : "Solo Band");

        menu.addSeparator();

        // Reset & Delete
        menu.addItem(20, "Reset Gain to 0 dB");
        menu.addItem(21, "Reset Band (Defaults)");
        menu.addSeparator();
        menu.addItem(30, "Delete Band");

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withTargetComponent(this)
                .withTargetScreenArea(AnalyzerContextMenu::nativePopupScreenArea(localPointToGlobal(pos))),
            [safeThis = juce::Component::SafePointer<AdvancedSpectrumDisplay>(this), bandIndex](int result)
            {
                if (safeThis == nullptr)
                    return;

                auto& self = *safeThis.getComponent();
                // Menu closed (dismissed or selected) — release node visibility guard.
                // If mouse is still inside spectrum, keep nodes visible; otherwise fade out.
                self.bandContextMenuOpen = false;
                if (!self.mouseInsideSpectrum)
                    self.nodesTargetOpacity = kNodesIdleOpacity;

                if (result == 0) return; // dismissed without selection

                if (result >= 100 && result < 107)
                {
                    self.executeBandContextAction(bandIndex,
                        { BandRadialMenu::CommandType::setFilterType, result - 100 });
                }
                else switch (result)
                {
                    case 10: self.executeBandContextAction(bandIndex, { BandRadialMenu::CommandType::toggleEnabled, 0 }); break;
                    case 11: self.executeBandContextAction(bandIndex, { BandRadialMenu::CommandType::toggleSolo, 0 }); break;
                    case 20: self.executeBandContextAction(bandIndex, { BandRadialMenu::CommandType::resetGain, 0 }); break;
                    case 21: self.executeBandContextAction(bandIndex, { BandRadialMenu::CommandType::resetBand, 0 }); break;
                    case 30: self.executeBandContextAction(bandIndex, { BandRadialMenu::CommandType::deleteBand, 0 }); break;
                    default: break;
                }
            });
    }

    void executeAnalyzerContextAction(AnalyzerContextMenu::Command command)
    {
        switch (command.type)
        {
            case AnalyzerContextMenu::CommandType::togglePre:
            {
                auto* value = processor.getAPVTS().getRawParameterValue("showPreSpectrum");
                setBoolParameter("showPreSpectrum", ! (value && value->load() > 0.5f));
                break;
            }
            case AnalyzerContextMenu::CommandType::togglePost:
            {
                auto* value = processor.getAPVTS().getRawParameterValue("showPostSpectrum");
                setBoolParameter("showPostSpectrum", ! (value && value->load() > 0.5f));
                break;
            }
            case AnalyzerContextMenu::CommandType::toggleDelta:
                setBoolParameter("showDeltaSpectrum", ! isDeltaEnabled());
                break;
            case AnalyzerContextMenu::CommandType::setResolution:
                setChoiceParameter("analyzerResolution", command.value);
                break;
            case AnalyzerContextMenu::CommandType::setSpeed:
                setChoiceParameter("analyzerSpeed", command.value);
                break;
            case AnalyzerContextMenu::CommandType::setTilt:
                setFloatParameter("spectrumTilt", command.floatValue);
                break;
            case AnalyzerContextMenu::CommandType::togglePeakHold:
                setBoolParameter("showPeakHold", ! isPeakHoldEnabled());
                break;
            case AnalyzerContextMenu::CommandType::togglePianoRoll:
                setBoolParameter("pianoRollOverlay", ! isPianoRollEnabled());
                break;
        }
    }

    void showAnimatedAnalyzerContextMenu(juce::Point<float> pos)
    {
        AnalyzerContextMenu::State menuState;
        if (auto* value = processor.getAPVTS().getRawParameterValue("showPreSpectrum"))
            menuState.showPre = value->load() > 0.5f;
        if (auto* value = processor.getAPVTS().getRawParameterValue("showPostSpectrum"))
            menuState.showPost = value->load() > 0.5f;
        menuState.showDelta = isDeltaEnabled();
        menuState.peakHold = isPeakHoldEnabled();
        menuState.pianoRoll = isPianoRollEnabled();
        if (auto* value = processor.getAPVTS().getRawParameterValue("analyzerResolution"))
            menuState.resolution = static_cast<int>(value->load());
        if (auto* value = processor.getAPVTS().getRawParameterValue("analyzerSpeed"))
            menuState.speed = static_cast<int>(value->load());
        menuState.tiltDbPerOctave = getAnalyzerSlopeDbPerOct();

        bandContextMenuOpen = true;
        nodesTargetOpacity = 1.0f;
        if (currentTimerHz != 60)
        {
            currentTimerHz = 60;
            startTimerHz(60);
        }
        analyzerContextMenu.open(
            pos,
            graphBounds,
            menuState,
            [this](AnalyzerContextMenu::Command command)
            {
                executeAnalyzerContextAction(command);
            },
            [this]
            {
                bandContextMenuOpen = false;
                if (! mouseInsideSpectrum)
                    nodesTargetOpacity = kNodesIdleOpacity;
                repaint();
            });
    }

    // Platform-native accessibility fallback (Alt/Option-right-click).
    void showClassicAnalyzerContextMenu(juce::Point<int> pos)
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

        juce::PopupMenu slopeMenu;
        slopeMenu.addItem(30, "Flat (0 dB/oct)", true, std::abs(slope) < 0.01f);
        slopeMenu.addItem(31, "3 dB/oct", true, std::abs(slope - 3.0f) < 0.3f);
        slopeMenu.addItem(32, "4.5 dB/oct", true, std::abs(slope - 4.5f) < 0.3f);
        slopeMenu.addItem(33, "6 dB/oct", true, std::abs(slope - 6.0f) < 0.3f);
        menu.addSubMenu("Spectrum Tilt", slopeMenu);

        bool peakHoldOn = isPeakHoldEnabled();
        menu.addItem(50, "Peak Hold", true, peakHoldOn);
        menu.addItem(40, "Piano Roll Overlay", true, pianoRoll);

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(this)
                               .withTargetScreenArea(AnalyzerContextMenu::nativePopupScreenArea(localPointToGlobal(pos))),
            [safeThis = juce::Component::SafePointer<AdvancedSpectrumDisplay>(this),
             showPre, showPost, showDelta, pianoRoll, peakHoldOn](int result)
            {
                if (safeThis == nullptr)
                    return;

                auto& self = *safeThis.getComponent();
                switch (result)
                {
                    case 1: self.setBoolParameter("showPreSpectrum", !showPre); break;
                    case 2: self.setBoolParameter("showPostSpectrum", !showPost); break;
                    case 3: self.setBoolParameter("showDeltaSpectrum", !showDelta); break;
                    case 10: self.setChoiceParameter("analyzerResolution", 0); break;
                    case 11: self.setChoiceParameter("analyzerResolution", 1); break;
                    case 12: self.setChoiceParameter("analyzerResolution", 2); break;
                    case 13: self.setChoiceParameter("analyzerResolution", 3); break;
                    case 20: self.setChoiceParameter("analyzerSpeed", 0); break;
                    case 21: self.setChoiceParameter("analyzerSpeed", 1); break;
                    case 22: self.setChoiceParameter("analyzerSpeed", 2); break;
                    case 30: self.setFloatParameter("spectrumTilt", 0.0f); break;
                    case 31: self.setFloatParameter("spectrumTilt", 3.0f); break;
                    case 32: self.setFloatParameter("spectrumTilt", 4.5f); break;
                    case 33: self.setFloatParameter("spectrumTilt", 6.0f); break;
                    case 40: self.setBoolParameter("pianoRollOverlay", !pianoRoll); break;
                    case 50: self.setBoolParameter("showPeakHold", !peakHoldOn); break;
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

    static void lerpPixelDb (const std::vector<float>& prev,
                             const std::vector<float>& curr,
                             float t,
                             std::vector<float>& out)
    {
        const size_t n = curr.size();
        if (n == 0)
        {
            out.clear();
            return;
        }
        out.resize(n);
        if (prev.size() != n || t >= 1.0f)
        {
            std::copy(curr.begin(), curr.end(), out.begin());
            return;
        }
        if (t <= 0.0f)
        {
            std::copy(prev.begin(), prev.end(), out.begin());
            return;
        }
        const float u = 1.0f - t;
        for (size_t i = 0; i < n; ++i)
            out[i] = prev[i] * u + curr[i] * t;
    }

    // P1-5: paint-thread hop interpolation. New inject → snap prev=curr, store curr, t=0.
    // Otherwise advance t toward 1 over ~1 hop interval (last hop dt, clamped 16–50 ms).
    bool ingestAndAdvanceSpectrumLerp()
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        bool hopChanged = false;

        {
            const juce::SpinLock::ScopedLockType lk(spectrumDataLock);
            if (injectedSpectrumVersion != 0 && injectedSpectrumVersion != lastLerpHopVersion)
            {
                hopChanged = true;
                lastLerpHopVersion = injectedSpectrumVersion;

                const bool firstHop = hopCurrPre.empty();
                hopPrevPre.assign(hopCurrPre.begin(), hopCurrPre.end());
                hopCurrPre.assign(smoothedSpectrum.begin(), smoothedSpectrum.end());
                hopPrevPost.assign(hopCurrPost.begin(), hopCurrPost.end());
                hopCurrPost.assign(injectedPostSpectrum.begin(), injectedPostSpectrum.end());

                const bool sizeMismatch = hopPrevPre.size() != hopCurrPre.size();
                if (firstHop || hopPrevPre.empty() || sizeMismatch)
                    hopPrevPre.assign(hopCurrPre.begin(), hopCurrPre.end());
                if (hopPrevPost.empty() || hopPrevPost.size() != hopCurrPost.size())
                    hopPrevPost.assign(hopCurrPost.begin(), hopCurrPost.end());

                const double gapMs = (lastHopTimeMs > 0.0) ? (nowMs - lastHopTimeMs) : 0.0;
                // Fresh hop: lerp from previous. First hop / resize / freeze-resume (>100 ms): snap.
                spectrumLerpT = (firstHop || sizeMismatch || gapMs > 100.0) ? 1.0f : 0.0f;

                if (lastHopTimeMs > 0.0)
                    hopIntervalMs = juce::jlimit(kHopIntervalMinMs, kHopIntervalMaxMs, nowMs - lastHopTimeMs);
                lastHopTimeMs = nowMs;
            }
        }

        if (!hopChanged && lastDisplayTickMs > 0.0)
        {
            const double dtMs = nowMs - lastDisplayTickMs;
            if (dtMs > 0.0)
                spectrumLerpT = juce::jmin(1.0f, spectrumLerpT + static_cast<float>(dtMs / hopIntervalMs));
        }
        lastDisplayTickMs = nowMs;

        lerpPixelDb(hopPrevPre, hopCurrPre, spectrumLerpT, displayPre);
        lerpPixelDb(hopPrevPost, hopCurrPost, spectrumLerpT, displayPost);
        return hopChanged;
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

            // Peak-hold tracking (only when enabled — OFF by default)
            if (isPeakHoldEnabled())
            {
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
    }

    // Rebuild live spectrum paths from the displayed (lerped) columns.
    // Path building + renderSpectrumToImage costs ~6 ms, so the timer skips this
    // when lerp t is holding and the FFT hop version is unchanged.
    void rebuildLiveSpectrumPaths(bool forceRebuild = false)
    {
        if (graphBounds.isEmpty()) return;

        const uint64_t preVer  = (injectedSpectrumVersion != 0)
                                 ? injectedSpectrumVersion
                                 : processor.getSpectrumAnalyzer().getSpectrumVersion();
        const uint64_t postVer = (injectedSpectrumVersion != 0)
                                 ? injectedSpectrumVersion
                                 : processor.getPostEQAnalyzer().getSpectrumVersion();
        const bool boundsChanged = (lastSpectrumBounds != graphBounds);

        if (!forceRebuild && preVer == lastPreSpectrumVersion && postVer == lastPostSpectrumVersion && !boundsChanged)
            return;

        lastPreSpectrumVersion = preVer;
        lastPostSpectrumVersion = postVer;
        lastSpectrumBounds = graphBounds;

        const auto& preBuffer = !displayPre.empty() ? displayPre : smoothedSpectrum;
        const auto& postSrc   = !displayPost.empty() ? displayPost : injectedPostSpectrum;
        const size_t usable = std::min(preBuffer.size(),
            static_cast<size_t>(std::max(0, static_cast<int>(graphBounds.getWidth()))));

        // --- Pre (input) path ---
        // When GL is active, the shader already renders the pre spectrum underneath.
        // Skip the expensive software path build (~3ms saved per rebuild).
        if (!glSpectrumActive)
        {
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
        }
        else
        {
            cachedPreLine.clear();
            cachedPreFill.clear();
        }

        // --- Post (output) path ---
        auto* postP = processor.getAPVTS().getRawParameterValue("showPostSpectrum");
        bool showPost = postP ? postP->load() > 0.5f : false;
        // When GL is active, the shader already renders the post spectrum underneath.
        if (!glSpectrumActive)
        {
            if (showPost && !isFrozen)
            {
                const bool useInjectedPost = !postSrc.empty()
                    && (injectedSpectrumVersion != 0 || !displayPost.empty());
                if (useInjectedPost && usable > 4)
                {
                    const size_t limit = std::min(usable, postSrc.size());
                    smoothYBuffer.resize(limit);
                    for (size_t i = 0; i < limit; ++i)
                    {
                        float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                        smoothYBuffer[i] = dbToY(applyTilt(postSrc[i], freq));
                    }
                    cachedPostLine.clear();
                    cachedPostFill.clear();
                    pathBuilder.build(smoothYBuffer.data(), limit,
                                      graphBounds.getX(), graphBounds.getBottom(),
                                      cachedPostLine, &cachedPostFill, 3);
                }
                else
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
                        cachedPostFill.clear();
                        pathBuilder.build(smoothYBuffer.data(), usable,
                                          graphBounds.getX(), graphBounds.getBottom(),
                                          cachedPostLine, &cachedPostFill, 3);
                    }
                    else { cachedPostLine.clear(); cachedPostFill.clear(); }
                }
            }
            else { cachedPostLine.clear(); cachedPostFill.clear(); }
        }
        else { cachedPostLine.clear(); cachedPostFill.clear(); }

        // --- Delta (post - pre) path ---
        bool showDelta = isDeltaEnabled();
        if (showDelta && showPost && !isFrozen)
        {
            const bool useInjectedPost = !postSrc.empty()
                && (injectedSpectrumVersion != 0 || !displayPost.empty());
            if (useInjectedPost && usable > 4)
            {
                float zeroY = dbToY(0.0f);
                const size_t limit = std::min(usable, std::min(postSrc.size(), preBuffer.size()));
                smoothYBuffer.resize(limit);
                for (size_t i = 0; i < limit; ++i)
                {
                    float freq = xToFreq(graphBounds.getX() + static_cast<float>(i));
                    float postDb = postSrc[i];
                    float preDb = preBuffer[i];
                    float delta = applyTilt(postDb, freq) - applyTilt(preDb, freq);
                    delta = juce::jlimit(-24.0f, 24.0f, delta);
                    smoothYBuffer[i] = zeroY - (delta / 24.0f) * (graphBounds.getHeight() * 0.45f);
                }
                cachedDeltaLine.clear();
                pathBuilder.build(smoothYBuffer.data(), limit,
                                  graphBounds.getX(), graphBounds.getBottom(),
                                  cachedDeltaLine, nullptr, 3);
            }
            else
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
        }
        else { cachedDeltaLine.clear(); }

        // --- Peak-hold path (only when enabled) ---
        if (isPeakHoldEnabled() && !peakHold.empty() && usable > 4)
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
    // Called when a hop arrives or lerp t moved; skipped while t holds at 1.
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

        // =============================================================
        // Wave 5 Premium Spectrum — software fallback.
        // Mirrors the 3-pass GL architecture (fill + glow + line) using
        // plain alpha strokes. juce::Graphics has no additive blend mode,
        // so the "glow" layer is approximated with a single wide soft
        // stroke; pixel-perfect parity with the GL path is not possible
        // here, but the visual hierarchy (pre secondary, post hero)
        // stays coherent when users toggle OpenGL off. Pro-Q and other
        // premium analyzers are the percepual reference — not a claim
        // that this code matches any specific product's internals.
        // =============================================================

        // ---------- Pre-EQ (3 layers) ------------------------------
        if (!cachedPreFill.isEmpty())
        {
            // Pass 1: SOLID dusty-azure fill — filled body, NOT transparent at bottom
            juce::ColourGradient fillGrad(
                juce::Colour(0x554A9FD9), 0, graphBounds.getY(),    // ~33 % alpha top
                juce::Colour(0x284A9FD9), 0, graphBounds.getBottom(), false); // ~16% bottom — still visible
            ig.setGradientFill(fillGrad);
            ig.fillPath(cachedPreFill);
        }
        if (!cachedPreLine.isEmpty())
        {
            // Pass 2: wide soft halo (software glow substitute)
            ig.setColour(juce::Colour(0x554A9FD9));   // ~33 % alpha
            ig.strokePath(cachedPreLine,
                          juce::PathStrokeType(6.0f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

            // Pass 3: crisp main stroke
            ig.setColour(juce::Colour(0xD04A9FD9));   // ~82 % alpha
            ig.strokePath(cachedPreLine,
                          juce::PathStrokeType(1.5f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        }

        // ---------- Post-EQ (3 layers) -----------------------------
        if (!cachedPostFill.isEmpty())
        {
            // Pass 1: SOLID luminous cyan fill — filled body, distinct from pre-EQ
            juce::ColourGradient postGrad(
                juce::Colour(0x4400E5FF), 0, graphBounds.getY(),    // ~27 % alpha top
                juce::Colour(0x2000E5FF), 0, graphBounds.getBottom(), false); // ~12% bottom — still filled
            ig.setGradientFill(postGrad);
            ig.fillPath(cachedPostFill);
        }
        if (!cachedPostLine.isEmpty())
        {
            // Pass 2: wide cyan halo (software glow substitute)
            ig.setColour(juce::Colour(0x6600E5FF));   // ~40 % alpha
            ig.strokePath(cachedPostLine,
                          juce::PathStrokeType(6.0f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

            // Pass 3: crisp vivid cyan main stroke
            ig.setColour(juce::Colour(0xE600E5FF));   // ~90 % alpha
            ig.strokePath(cachedPostLine,
                          juce::PathStrokeType(1.5f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        }

        // Delta line
        if (!cachedDeltaLine.isEmpty())
        {
            ig.setColour(ModernLookAndFeel::Colors::accentCyan.withAlpha(0.5f));
            ig.strokePath(cachedDeltaLine, juce::PathStrokeType(1.0f));
        }

        // Peak-hold line
        if (!cachedPeakLine.isEmpty())
        {
            ig.setColour(ModernLookAndFeel::Colors::accentOrange.withAlpha(0.6f));
            ig.strokePath(cachedPeakLine, juce::PathStrokeType(0.8f));
        }
    }

    void drawGrid(juce::Graphics& g)
    {
        // Liquid Intelligence: full frequency grid stays gone. GRAPH-GRID-A1
        // restores only a decade backbone (100 / 1k / 10k) as peripheral
        // orientation. Salience: hover/selected-frequency guide (α≈0.30) >
        // these verticals (α≈0.10) > background. X from freqToX — the same
        // log map used by nodes and Hz labels.
        //
        // Horizontal dB lines remain the EQ-grid set (+12, +6, 0, -6, -12).
        // Do not add analyzer-depth horizontals (GRAPH-GRID-A3) here.
        //
        // MOV 09:05 freeze: GRAPH-GRID-A PASS (100/1k/10k faint verticals,
        // 0 dB analyzer label, hover stronger than decades). Do not add
        // G2 minor verticals or A3 analyzer horizontals.
        //
        // Wave 5 verdict fix: previous alpha (0.3/0.5 on 0xFF242836) was too
        // faint against the dark spectrum background — grid lines above 0 dB
        // were effectively invisible. Bumped to 0.55/0.80 and switched ink to
        // a lighter slate (0xFF3A4050) so every mark is readable on all
        // backgrounds without competing with the EQ curve.

        const float dbMarks[] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
        for (float db : dbMarks)
        {
            if (db < spectrumMinDb || db > spectrumMaxDb) continue;
            float y = dbToY(db);
            bool isZero = std::abs(db) < 0.01f;

            if (isZero)
                g.setColour(juce::Colour(0xFF3A4050).withAlpha(0.80f));
            else
                g.setColour(juce::Colour(0xFF3A4050).withAlpha(0.55f));

            g.drawHorizontalLine((int)y, graphBounds.getX(), graphBounds.getRight());
        }

        g.setColour(juce::Colour(0xFF3A4050).withAlpha(0.10f));
        const float decadeHz[] = { 100.0f, 1000.0f, 10000.0f };
        for (float hz : decadeHz)
        {
            const float x = freqToX(hz);
            if (x > graphBounds.getX() && x < graphBounds.getRight())
                g.drawVerticalLine((int)x, graphBounds.getY(), graphBounds.getBottom());
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

            // Wave 4B Fix 1 (Tribunale): captured spectrum was too invasive at
            // 0.6 alpha — the orange dashed line competed with the main EQ
            // curve for visual attention. Reduced to 0.25 so it reads as a
            // subtle historical reference, not a primary channel.
            g.setColour(ModernLookAndFeel::Colors::accentYellow.withAlpha(0.25f));
            g.strokePath(cachedCapturedDash, juce::PathStrokeType(0.8f));
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

            // spectrum frozen capture overlay — legitimate accentBlue use
            juce::ColourGradient frozenGrad(
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.35f), 0, graphBounds.getY(), // spectrum frozen gradient top
                ModernLookAndFeel::Colors::accentBlue.withAlpha(0.03f), 0, graphBounds.getBottom(), false); // spectrum frozen gradient bottom
            g.setGradientFill(frozenGrad);
            g.fillPath(frozenFillPath);

            g.setColour(ModernLookAndFeel::Colors::accentBlue); // spectrum frozen line stroke
            g.strokePath(frozenLinePath, juce::PathStrokeType(1.0f));

            g.setColour(ModernLookAndFeel::Colors::accentBlue); // spectrum frozen label text
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

        // Wave 4A: stronger curve fill gradient (0.09 → 0.00) to anchor the
        // curve visually above the blue spectrum dust.
        juce::ColourGradient fillGrad(
            ModernLookAndFeel::Colors::eqCurve.withAlpha(0.09f), 0, graphBounds.getY(),
            ModernLookAndFeel::Colors::eqCurve.withAlpha(0.00f), 0, zeroY, false);
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
            // P0-A: one soft glow only — drop the 10px + 5px double bloom so
            // the composite curve stays below selected nodes in the hierarchy.
            g.setColour(ModernLookAndFeel::Colors::eqCurve.withAlpha(0.10f));
            g.strokePath(cachedEQCurve, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                               juce::PathStrokeType::rounded));
        }

        // P0-A: thinner main stroke (was 2.5) — curve is level-1, nodes lead.
        g.setColour(ModernLookAndFeel::Colors::eqCurve.withAlpha(0.95f));
        g.strokePath(cachedEQCurve, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
    }

    void drawAIMarkers(juce::Graphics& g)
    {
        // Hero Graph Polish v1 — Subtle Idle:
        // idle zones render as a faint wash + thin centre spine so they stay
        // discoverable without competing with the EQ curve. Only the zone
        // currently hovered (aiTooltip.correctionIdx, already hover-driven by
        // mouseMove) intensifies and shows its label. Geometry, Q-derived
        // width, minWidth and the approved/pending colour mapping are
        // preserved verbatim from the previous implementation.
        if (!processor.isProcessorReady())
            return;

        const auto corrections = processor.getAIEngine().getPendingCorrections();
        if (corrections.empty())
            return;

        const float graphLeft   = graphBounds.getX();
        const float graphRight  = graphBounds.getRight();
        const float graphTop    = graphBounds.getY();
        const float graphBottom = graphBounds.getBottom();
        const float graphHeight = graphBounds.getHeight();
        if (graphHeight <= 0.0f)
            return;

        const double sr = (processor.getSampleRate() > 0.0) ? processor.getSampleRate() : 44100.0;
        const float nyquist = static_cast<float>(sr * 0.5);

        for (size_t i = 0; i < corrections.size(); ++i)
        {
            const auto& corr = corrections[i];
            const float freq = corr.frequency;
            const float Q    = juce::jmax(0.01f, corr.suggestedQ);
            if (freq <= 0.0f || freq >= nyquist)
                continue;

            // Musical octave half-width per side (Tribunale directive)
            const float ratio    = std::pow(2.0f, 1.0f / (2.0f * Q));
            const float freqLow  = freq / ratio;
            const float freqHigh = freq * ratio;

            float xLow  = freqToX(freqLow);
            float xHigh = freqToX(freqHigh);

            // Minimum 20 px so narrow-Q suggestions remain visible
            const float minWidth = 20.0f;
            if (xHigh - xLow < minWidth)
            {
                const float xC = freqToX(freq);
                xLow  = xC - minWidth * 0.5f;
                xHigh = xC + minWidth * 0.5f;
            }

            // Clamp to graph bounds
            const float xL = juce::jlimit(graphLeft, graphRight, xLow);
            const float xH = juce::jlimit(graphLeft, graphRight, xHigh);
            if (xH - xL < 2.0f)
                continue;

            auto zoneRect = juce::Rectangle<float>(xL, graphTop, xH - xL, graphHeight);

            // Zone color: green if approved, amber if pending
            const juce::Colour base = corr.approved
                ? ModernLookAndFeel::Colors::accentGreen
                : ModernLookAndFeel::Colors::amber;

            // Subtle Idle vs Active:
            // - active = hovered zone (drives the tooltip already), near full peso
            // - idle   = barely-there wash so overlapping zones don't "wash" the graph
            const bool isActive = aiTooltip.visible
                               && aiTooltip.correctionIdx == static_cast<int>(i);

            const float topA    = isActive ? 0.20f  : 0.08f;
            const float bottomA = isActive ? 0.06f  : 0.02f;

            const auto topColour    = base.withAlpha(topA);
            const auto bottomColour = base.withAlpha(bottomA);

            juce::ColourGradient grad(topColour,    zoneRect.getX(), graphTop,
                                      bottomColour, zoneRect.getX(), graphBottom,
                                      false);
            g.setGradientFill(grad);
            g.fillRect(zoneRect);

            // Idle-only centre spine: keeps zones discoverable when their
            // wash is intentionally faint. Active zones already read clearly
            // through the stronger gradient + label.
            if (!isActive)
            {
                const float xC = juce::jlimit(graphLeft, graphRight, freqToX(freq));
                g.setColour(base.withAlpha(0.14f));
                g.drawLine(xC, graphTop, xC, graphBottom, 1.0f);
            }

            // Active-only label (was always-on; produced background noise on
            // crowded scenes). Width threshold (≥18 px) preserved.
            if (isActive && zoneRect.getWidth() >= 18.0f)
            {
                auto problemLabel = [](AIEngine::ProblemType pt) -> const char*
                {
                    switch (pt)
                    {
                        case AIEngine::ProblemType::Resonance:  return "RESO";
                        case AIEngine::ProblemType::Harshness:  return "HARSH";
                        case AIEngine::ProblemType::Muddiness:  return "MUDDY";
                        case AIEngine::ProblemType::Boxyness:   return "BOXY";
                        case AIEngine::ProblemType::Sibilance:  return "SIBIL";
                        case AIEngine::ProblemType::LowEndBoom: return "RUMBLE";
                        case AIEngine::ProblemType::ThinSound:  return "THIN";
                        case AIEngine::ProblemType::DullSound:  return "DULL";
                        case AIEngine::ProblemType::None:
                        default:                                return "AI";
                    }
                };

                juce::String freqStr = (freq >= 1000.0f)
                    ? juce::String(freq / 1000.0f, 1) + "kHz"
                    : juce::String((int) std::round(freq)) + "Hz";

                juce::String labelStr = juce::String(problemLabel(corr.type))
                                        + " " + freqStr;

                auto labelRect = juce::Rectangle<int>(
                    (int) zoneRect.getX(),
                    (int) (graphTop + 4.0f),
                    (int) zoneRect.getWidth(),
                    12);

                g.setColour(base.withAlpha(0.92f));
                g.setFont(aiMarkerFont);
                g.drawFittedText(labelStr, labelRect,
                                 juce::Justification::centredTop, 1, 0.85f);
            }
        }
    }

    //==========================================================================
    // Phase 7B — AI suggested curve (dashed amber overlay).
    // Draws the bell-shape of every pending correction using an analytic
    // biquad magnitude response. Rebuilt every paint — cheap because the
    // method short-circuits when there are no pending corrections (rare).
    //==========================================================================
    void drawAISuggestedCurve(juce::Graphics& g)
    {
        if (!processor.isProcessorReady() || graphBounds.isEmpty())
            return;

        const auto corrections = processor.getAIEngine().getPendingCorrections();
        if (corrections.empty())
            return;

        ensureEQCurveFrequencies();
        if (eqCurveFrequencies.empty())
            return;

        const double sr = (processor.getSampleRate() > 0.0) ? processor.getSampleRate() : 44100.0;
        const float  nyquist = static_cast<float>(sr * 0.5);

        // Wave 4D (Marco from video): the dashed AI suggestion line was
        // reading as "toy-like" — too thick, too solid, too warm. Three
        // tweaks to make it feel technical/engineering-grade:
        //   1. alpha 0.60 → 0.30 (subtler, less dominating)
        //   2. dash {4,4} → {2,4} (short dash, long gap — telemetry look)
        //   3. stroke 1.6 → 0.8 px (finer line, below)
        const juce::Colour amberLine = ModernLookAndFeel::Colors::amber.withAlpha(0.30f);
        const float dashes[] = { 2.0f, 4.0f };

        // Draw one dashed curve per pending correction (individual bell shape).
        for (const auto& corr : corrections)
        {
            if (corr.approved)
                continue;  // approved corrections are already folded into the main EQ curve

            const float freq = corr.frequency;
            const float Q    = juce::jmax(0.01f, corr.suggestedQ);
            const float gainDb = corr.suggestedGain;
            if (freq <= 0.0f || freq >= nyquist)
                continue;

            // Precompute biquad-peak constants (RBJ cookbook, peaking EQ).
            const double A     = std::pow(10.0, gainDb / 40.0);
            const double w0    = 2.0 * juce::MathConstants<double>::pi * static_cast<double>(freq) / sr;
            const double sinW0 = std::sin(w0);
            const double cosW0 = std::cos(w0);
            const double alpha = sinW0 / (2.0 * static_cast<double>(Q));

            const double b0 = 1.0 + alpha * A;
            const double b1 = -2.0 * cosW0;
            const double b2 = 1.0 - alpha * A;
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * cosW0;
            const double a2 = 1.0 - alpha / A;

            juce::Path suggestionPath;
            bool started = false;

            for (size_t i = 0; i < eqCurveFrequencies.size(); ++i)
            {
                const float f = eqCurveFrequencies[i];
                if (f >= nyquist) break;

                const double w  = 2.0 * juce::MathConstants<double>::pi * static_cast<double>(f) / sr;
                const double cw = std::cos(w);
                const double sw = std::sin(w);

                // |H(e^jw)|^2 = (|num|^2) / (|den|^2)
                // num = b0 + b1*e^-jw + b2*e^-2jw
                const double numRe = b0 + b1 * cw + b2 * std::cos(2.0 * w);
                const double numIm = -b1 * sw - b2 * std::sin(2.0 * w);
                const double denRe = a0 + a1 * cw + a2 * std::cos(2.0 * w);
                const double denIm = -a1 * sw - a2 * std::sin(2.0 * w);

                const double num2 = numRe * numRe + numIm * numIm;
                const double den2 = denRe * denRe + denIm * denIm;
                const double mag2 = (den2 > 1.0e-20) ? (num2 / den2) : 1.0;

                float db = static_cast<float>(10.0 * std::log10(juce::jmax(1.0e-20, mag2)));
                db = juce::jlimit(-24.0f, 24.0f, db);

                const float x = freqToX(f);
                const float y = gainToY(db);

                if (!started) { suggestionPath.startNewSubPath(x, y); started = true; }
                else           { suggestionPath.lineTo(x, y); }
            }

            if (suggestionPath.isEmpty())
                continue;

            // Dashed amber stroke (Liquid Intelligence) — Wave 4D: stroke
            // width dropped 1.6 → 0.8 px for a finer, engineering look.
            juce::Path dashed;
            juce::PathStrokeType stroke(0.8f, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded);
            stroke.createDashedStroke(dashed, suggestionPath, dashes, 2);

            g.setColour(amberLine);
            g.strokePath(dashed, juce::PathStrokeType(0.8f,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }

    //==========================================================================
    // Phase 7C — Contextual tooltip drawn on top of everything.
    // Tribunale #1 (BINDING): OPAQUE 0xEE181A22 backdrop + amber border.
    // NO StackBlur, NO createComponentSnapshot — flat fill only.
    //==========================================================================
    void drawTooltip(juce::Graphics& g)
    {
        if (!aiTooltip.visible)
            return;

        constexpr float tooltipW = 220.0f;
        constexpr float tooltipH = 110.0f;

        // Anchor above the zone by default; fall back to below if too close to top.
        float tx = aiTooltip.anchor.getCentreX() - tooltipW * 0.5f;
        float ty = aiTooltip.anchor.getY() - tooltipH - 10.0f;
        if (ty < graphBounds.getY() + 4.0f)
            ty = aiTooltip.anchor.getY() + 10.0f;

        // Clamp horizontally inside the component bounds.
        const float maxX = static_cast<float>(getWidth()) - tooltipW - 4.0f;
        tx = juce::jlimit(4.0f, juce::jmax(4.0f, maxX), tx);

        juce::Rectangle<float> tooltipRect(tx, ty, tooltipW, tooltipH);

        // Wave 4A: save bounds for mouseMove hit-testing (keep tooltip visible
        // while the cursor is hovering over the tooltip itself).
        aiTooltip.tooltipBounds = tooltipRect;

        // ── CSS-like drop shadow (Tribunale Wave 4A) ──
        // Opzione A: Emendamento #1 resta vincolante (no StackBlur / snapshot).
        // Per simulare la profondità: due rounded rect neri sfalsati verso il
        // basso con alpha decrescente, PRIMA del backdrop opaco. Costo: 2 fill
        // ops aggiuntivi, ~0 overhead misurabile.
        g.setColour(juce::Colour(0x4D000000));                              // 30% black
        g.fillRoundedRectangle(tooltipRect.translated(0.0f, 2.0f), 7.0f);
        g.setColour(juce::Colour(0x26000000));                              // 15% black
        g.fillRoundedRectangle(tooltipRect.translated(0.0f, 4.0f), 8.0f);

        // ── OPAQUE backdrop (Tribunale #1, BINDING) ──
        // NO StackBlur, NO createComponentSnapshot — flat fill only.
        g.setColour(juce::Colour(0xEE181A22));           // 93 % opacity
        g.fillRoundedRectangle(tooltipRect, 6.0f);

        g.setColour(ModernLookAndFeel::Colors::amber);
        g.drawRoundedRectangle(tooltipRect, 6.0f, 1.0f);

        // Text area reserves the bottom 22 px for the FIX button.
        auto textArea = tooltipRect.reduced(8.0f, 6.0f);
        textArea.removeFromBottom(22.0f);

        // Wave 4B Fix 5 (Tribunale): running Y cursor to prevent rows from
        // overlapping when the description wraps onto 2 lines. Previously
        // titleRect/descRect/suggRect were fixed Y offsets at {0, lineH, 2*lineH}
        // and a separate descWrapRect was built with height lineH*2 — which
        // collided with suggRect at Y=28 whenever the description actually
        // wrapped. The cursor-based layout advances past the full wrapped
        // description height before placing the suggestion row.
        const int titleH   = 14;
        const int descLineH = 14;
        const int descMaxLines = 2;
        const int suggH    = 14;
        const int rowGap   = 2;

        int cursorY = (int) textArea.getY();

        // Title (bold amber)
        auto titleRect = juce::Rectangle<int>(
            (int) textArea.getX(), cursorY,
            (int) textArea.getWidth(), titleH);
        g.setColour(ModernLookAndFeel::Colors::amber);
        g.setFont(tooltipTitleFont);
        g.drawFittedText(aiTooltip.title, titleRect,
                         juce::Justification::topLeft, 1, 0.9f);
        cursorY += titleH + rowGap;

        // Description (regular, secondary text) — up to 2 lines, guaranteed
        // dedicated vertical space (descLineH * descMaxLines) so the following
        // suggestion row can never overlap the second wrapped line.
        auto descWrapRect = juce::Rectangle<int>(
            (int) textArea.getX(), cursorY,
            (int) textArea.getWidth(), descLineH * descMaxLines);
        g.setColour(ModernLookAndFeel::Colors::textSecondary);
        g.setFont(tooltipDescFont);
        g.drawFittedText(aiTooltip.description, descWrapRect,
                         juce::Justification::topLeft, descMaxLines, 0.85f);
        cursorY += descLineH * descMaxLines + rowGap;

        // Suggestion (italic, primary text) — now placed AFTER the full
        // description block, never on top of it.
        auto suggRect = juce::Rectangle<int>(
            (int) textArea.getX(), cursorY,
            (int) textArea.getWidth(), suggH);
        g.setColour(ModernLookAndFeel::Colors::textPrimary);
        g.setFont(tooltipSuggFont);
        g.drawFittedText(aiTooltip.suggestion, suggRect,
                         juce::Justification::topLeft, 1, 0.9f);

        // ── FIX button (amber) at bottom-right ──
        const float btnW = 40.0f;
        const float btnH = 18.0f;
        juce::Rectangle<float> btnBounds(
            tooltipRect.getRight() - btnW - 6.0f,
            tooltipRect.getBottom() - btnH - 6.0f,
            btnW, btnH);
        aiTooltip.fixButtonBounds = btnBounds;

        g.setColour(ModernLookAndFeel::Colors::amber);
        g.fillRoundedRectangle(btnBounds, 3.0f);
        g.setColour(juce::Colour(0xFF181A22));
        g.setFont(tooltipFixFont);
        g.drawFittedText("FIX", btnBounds.toNearestInt(),
                         juce::Justification::centred, 1, 1.0f);
    }
    
    void drawEQBands(juce::Graphics& g)
    {
        const int active = processor.getNumActiveBands();
        const int maxBands = AIEqualizerAudioProcessor::maxBands;
        const int limit = std::min(active, maxBands);

        // FabFilter-style: skip all node rendering when fully transparent
        // (mouse outside spectrum and fade complete). EQ curve stays visible.
        const float nOp = nodesOpacity;
        if (nOp <= 0.001f)
            return;

        // AI Pulse: cache pending corrections once per frame (cheap vector copy)
        // so each node can check if it has a suggestion.
        const auto aiCorrections = processor.isProcessorReady()
            ? processor.getAIEngine().getPendingCorrections()
            : decltype(processor.getAIEngine().getPendingCorrections()){};
        // Breathing modulator: 0.0 → 1.0 sine wave (4-second cycle)
        const float breathMod = 0.5f + 0.5f * std::sin(aiBreathingPhase);

        // Hero Graph Polish v1 — Pass 1: detect whether any enabled band is
        // currently the user's primary focus (dragged/hovered/selected). The
        // result drives an emphasis multiplier in Pass 2 so non-focused
        // nodes step back when one node is leading the scene. No interaction
        // with hit testing / selection / drag — purely a render-time signal.
        bool hasPrimaryFocus = false;
        for (int i = 0; i < limit; ++i)
        {
            auto s = processor.getBandState(i);
            if (!s.enabled)
                continue;
            const bool dragThis = isDraggingBand && i == draggedBandIndex;
            const bool hovThis  = (i == hoveredBandIndex);
            const bool selThis  = (i == selectedBandIndex);
            if (dragThis || hovThis || selThis)
            {
                hasPrimaryFocus = true;
                break;
            }
        }

        auto drawOne = [&](int i)
        {
            auto state = processor.getBandState(i);

            // Skip disabled bands entirely — they have no visual presence
            // until the user creates them (FabFilter/Sonible: clean canvas)
            if (!state.enabled)
                return;

            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);

            if (x < graphBounds.getX() - 20 || x > graphBounds.getRight() + 20)
                return;

            juce::Colour col = bandColors[i];
            const bool isSelected = (i == selectedBandIndex);
            const bool isHovered  = (i == hoveredBandIndex);
            const bool isDragging = (isDraggingBand && i == draggedBandIndex);
            const bool isPrimary  = isDragging || isSelected || isHovered;

            // Salience scale, not four independent alphas:
            //   kNodesPrimaryEmphasis > kNodesCalmEmphasis > kNodesUnfocusedEmphasis > kNodesIdleOpacity
            // P0-A: calm ships at 0.70 (was 0.82) so dense 18–24 band scenes read.
            const float emphasis = isPrimary
                ? kNodesPrimaryEmphasis
                : (hasPrimaryFocus ? kNodesUnfocusedEmphasis : kNodesCalmEmphasis);

            // AI Pulse: check if this band has a pending AI correction
            // (frequency within ±1 semitone ≈ ratio < 0.06 in log2 domain)
            bool hasAICorrection = false;
            for (const auto& corr : aiCorrections)
            {
                float logRatio = std::abs(std::log2(state.frequency / juce::jmax(20.0f, corr.frequency)));
                if (logRatio < 0.06f)
                {
                    hasAICorrection = true;
                    break;
                }
            }

            // Precise Premium radii — smaller idle, modest selection bump,
            // dragged still distinct without becoming an "arcade puck".
            float baseRadius = 11.0f;
            float radius = baseRadius;
            if (isDragging)       radius = 15.5f;
            else if (isSelected)  radius = 12.5f;
            else if (isHovered)   radius = 12.5f;

            // Vertical guide line from 0dB to node — calmer alpha + emphasis
            if (std::abs(state.gain) > 0.3f)
            {
                float zeroY = gainToY(0.0f);
                g.setColour(col.withAlpha(0.10f * nOp * emphasis));
                g.drawLine(x, zeroY, x, y, 1.0f);
            }

            // === GLOW: dragged > selected/hovered > AI pulse > nothing ===
            // No "always-on" amber rim. Halos appear ONLY for nodes that
            // earned focus or that carry a pending AI suggestion.
            if (isDragging)
            {
                g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.08f * nOp));
                g.fillEllipse(x - radius - 8, y - radius - 8, (radius + 8) * 2, (radius + 8) * 2);
                g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.14f * nOp));
                g.fillEllipse(x - radius - 4, y - radius - 4, (radius + 4) * 2, (radius + 4) * 2);
            }
            else if (isSelected)
            {
                // Hero Graph Polish v1.2 — selected node gets a stronger,
                // larger halo than a plain hover so the active band is
                // clearly distinguishable from idle ones (Scena 3 feedback).
                g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.12f * nOp));
                g.fillEllipse(x - radius - 6, y - radius - 6, (radius + 6) * 2, (radius + 6) * 2);
            }
            else if (isHovered)
            {
                g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(0.09f * nOp));
                g.fillEllipse(x - radius - 5, y - radius - 5, (radius + 5) * 2, (radius + 5) * 2);
            }
            else if (hasAICorrection)
            {
                // AI pending pulse — single halo, gentler breath (0.035 → 0.075)
                float pulseAlpha = 0.035f + 0.040f * breathMod;
                g.setColour(ModernLookAndFeel::Colors::amber.withAlpha(pulseAlpha * nOp));
                g.fillEllipse(x - radius - 6, y - radius - 6, (radius + 6) * 2, (radius + 6) * 2);
            }

            // Solo badge — skip during drag (font creation is expensive)
            if (state.solo && !isDraggingBand)
            {
                juce::Rectangle<float> badge(x + radius, y - radius - 4, 16.0f, 12.0f);
                g.setColour(ModernLookAndFeel::Colors::accentYellow.withAlpha(0.9f * nOp));
                g.fillRoundedRectangle(badge, 3.0f);
                g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(nOp));
                g.setFont(soloBadgeFont);
                g.drawText("S", badge, juce::Justification::centred);
            }

            // === Node disc — Precise Premium (no permanent amber double-halo) ===
            juce::Rectangle<float> nodeBounds (x - radius, y - radius, radius * 2, radius * 2);

            // Fill — P0-A unselected ~0.22–0.32 after emphasis
            g.setColour(col.withAlpha(0.28f * nOp * emphasis));
            g.fillEllipse(nodeBounds);

            // Inner highlight — gentler specular
            g.setColour(col.brighter(0.20f).withAlpha(0.12f * nOp * emphasis));
            g.fillEllipse(nodeBounds.reduced(3.0f));

            // Border ring — selected=1.0, hover~0.75, unselected 0.35–0.45
            float ringAlpha;
            float ringThickness;
            if (isDragging)                  { ringAlpha = 1.00f * nOp;            ringThickness = 2.5f; }
            else if (isSelected)             { ringAlpha = 1.00f * nOp;            ringThickness = 2.0f; }
            else if (isHovered)              { ringAlpha = 0.75f * nOp;            ringThickness = 1.8f; }
            else                             { ringAlpha = 0.40f * nOp * emphasis; ringThickness = 1.5f; }
            g.setColour(col.withAlpha(ringAlpha));
            g.drawEllipse(nodeBounds, ringThickness);
        };

        // Z-order: non-selected first, selected last (on top)
        for (int i = 0; i < limit; ++i)
        {
            if (i == selectedBandIndex) continue;
            drawOne(i);
        }
        if (selectedBandIndex >= 0 && selectedBandIndex < limit)
            drawOne(selectedBandIndex);

        // Tooltip for selected band — FabFilter-style floating info panel
        // Visible only when nodes are visible AND not dragging
        if (!isDraggingBand && !bandRadialMenu.isOpen() && nOp > 0.3f
            && selectedBandIndex >= 0 && selectedBandIndex < maxBands)
        {
            auto state = processor.getBandState(selectedBandIndex);
            if (!state.enabled)
                return;  // no tooltip for disabled bands

            float x = freqToX(state.frequency);
            float y = gainToY(state.gain);

            juce::String freqStr = state.frequency >= 1000
                ? juce::String(state.frequency / 1000.0f, 2) + " kHz"
                : juce::String(static_cast<int>(state.frequency)) + " Hz";

            const char* typeNames[] = { "Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass" };
            juce::String typeName = (state.type >= 0 && state.type < 7)
                ? typeNames[state.type] : "Peak";

            int tw = 150, th = 52;
            int tx = static_cast<int>(juce::jlimit(graphBounds.getX(), graphBounds.getRight() - tw, x - tw / 2));
            int ty = static_cast<int>(y) - th - 14;
            if (ty < graphBounds.getY() + 5) ty = static_cast<int>(y) + 20;

            const float tipAlpha = juce::jmin(1.0f, nOp * 1.2f); // tooltip slightly faster fade

            // Drop shadow
            g.setColour(juce::Colours::black.withAlpha(0.4f * tipAlpha));
            g.fillRoundedRectangle(static_cast<float>(tx + 2), static_cast<float>(ty + 2),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f);

            // Background
            g.setColour(ModernLookAndFeel::Colors::bgDark.withAlpha(0.94f * tipAlpha));
            g.fillRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f);

            // Band color accent bar
            auto bandCol = bandColors[static_cast<size_t>(selectedBandIndex)];
            g.setColour(bandCol.withAlpha(tipAlpha));
            g.fillRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   3.0f, static_cast<float>(th), 6.0f);

            // Border
            g.setColour(bandCol.withAlpha(0.4f * tipAlpha));
            g.drawRoundedRectangle(static_cast<float>(tx), static_cast<float>(ty),
                                   static_cast<float>(tw), static_cast<float>(th), 6.0f, 1.0f);

            // Line 1: Band number + type
            g.setColour(bandCol.withAlpha(tipAlpha));
            g.setFont(bandTooltipBold);
            g.drawText("Band " + juce::String(selectedBandIndex + 1) + "  " + typeName,
                       tx + 8, ty + 4, tw - 14, 14, juce::Justification::centredLeft);

            // Line 2: Freq | Gain
            g.setColour(juce::Colours::white.withAlpha(0.95f * tipAlpha));
            g.setFont(bandTooltipVal);
            g.drawText(freqStr, tx + 8, ty + 20, 60, 14, juce::Justification::centredLeft);

            juce::String gainStr = (state.gain >= 0 ? "+" : "") + juce::String(state.gain, 1) + " dB";
            g.drawText(gainStr, tx + 60, ty + 20, 50, 14, juce::Justification::centred);

            // Line 3: Q value
            g.setColour(juce::Colours::white.withAlpha(0.6f * tipAlpha));
            g.setFont(bandTooltipQ);
            g.drawText("Q: " + juce::String(state.q, 2), tx + 8, ty + 36, 60, 12, juce::Justification::centredLeft);
        }
    }
    
    void drawAnalyzerDbLabel(juce::Graphics& g, float db) const
    {
        const float y = dbToY(db);
        const bool isZero = std::abs(db) < 0.01f;
        g.setColour(isZero ? ModernLookAndFeel::Colors::textSecondary.brighter(0.1f)
                           : ModernLookAndFeel::Colors::bgLighter.brighter(0.1f));
        g.drawText(juce::String((int) db), 4, (int) y - 6, 34, 12,
                   juce::Justification::centredRight);
    }

public:
    void drawLabels(juce::Graphics& g)
    {
        // === dB labels (left side, small and subtle) ===
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.0f)));
        
        for (float db = spectrumMinDb; db <= spectrumMaxDb; db += 12.0f)
            drawAnalyzerDbLabel(g, db);

        // GRAPH-GRID-A2: -90 + 12n never lands on 0. Emit 0 in this same
        // analyzer label system at dbToY(0) — the analyzer-scale position,
        // not a second coordinate glued onto the EQ response zero line.
        // Do not add a matching analyzer 0 grid line (existing EQ 0 stays).
        drawAnalyzerDbLabel(g, 0.0f);

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
                g.setColour(isMajor ? ModernLookAndFeel::Colors::textMuted.brighter(0.1f)
                                   : ModernLookAndFeel::Colors::bgLighter.brighter(0.1f));
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
        g.setColour(ModernLookAndFeel::Colors::accentBlue); // spectrum hover tooltip border
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
            // FabFilter-style: disabled bands are invisible → not hittable
            if (!state.enabled)
                continue;

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
    // ── Dynamic overlay activity tracking — called from timerCallback ────────
    // The live DynEQ curve shape now comes from the DSP replacement-delta
    // evaluator, so the GUI no longer needs per-band GR smoothing here.
    // We only track whether any band is currently owned by the dynamic stage
    // to keep the live overlay active when needed.
    void updateDynamicGRSmoothing()
    {
        if (!processor.isProcessorReady()) return;

        const int numActive = processor.getNumActiveBands();
        const int limit     = std::min(numActive, AIEqualizerAudioProcessor::maxBands);
        // The instance that is actually running: see the note on the accessor.
        const auto& dynProc = processor.getActiveDynamicEQProcessorForDisplay();
        bool hasAny = false;

        for (int i = 0; i < limit; ++i)
        {
            const auto dynParams = dynProc.getBandParams(i);
            if (!dynParams.enabled)
                continue;

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

        // The instance that is actually running: see the note on the accessor.
        const auto& dynProc = processor.getActiveDynamicEQProcessorForDisplay();
        rebuildEQCurvePath();

        if (eqCurveFrequencies.empty() || eqCurveMagnitudes.size() != eqCurveFrequencies.size())
            return;

        double sr = processor.getSampleRate();
        if (sr <= 0) sr = 44100.0;

        dynCurveDeltaDb.resize(eqCurveFrequencies.size(), 0.0f);
        dynProc.evaluateDynamicReplacementDeltaDbForFrequencyArray(
            eqCurveFrequencies.data(),
            dynCurveDeltaDb.data(),
            eqCurveFrequencies.size(),
            sr);

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

            // Static curve Y (from already-computed magnitudes)
            float statDb = (i < eqCurveMagnitudes.size())
                ? juce::Decibels::gainToDecibels(eqCurveMagnitudes[i], -48.0f) : 0.0f;
            statDb = juce::jlimit(-24.0f, 24.0f, statDb);
            staticCurveYPoints[i] = gainToY(statDb);  // Use gainToY for consistent centering

            // Dynamic curve Y: static white curve plus DSP-provided replacement delta
            float liveDb = statDb + dynCurveDeltaDb[i];
            liveDb = juce::jlimit(-24.0f, 24.0f, liveDb);
            dynCurveYPoints[i] = gainToY(liveDb);

            if (!started) { cachedDynamicEQCurve.startNewSubPath(x, dynCurveYPoints[i]); started = true; }
            else cachedDynamicEQCurve.lineTo(x, dynCurveYPoints[i]);
        }

        lastDynCurveEQVersion = lastEQVersion;
        lastDynCurveBounds = graphBounds;
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

        const bool staticCurveChanged = lastDynCurveEQVersion != lastEQVersion
            || lastDynCurveBounds != graphBounds
            || dynCurveXPoints.size() != eqCurveFrequencies.size();
        const double now = juce::Time::getMillisecondCounterHiRes();

        // Rebuild at most ~30 Hz while any band is owned by the dynamic stage.
        // This keeps the live curve honest even when GR is near zero but mix < 100%.
        if (cachedDynamicEQCurve.isEmpty()
            || staticCurveChanged
            || now - lastDynCurveRebuildMs > 33.0)
        {
            rebuildDynamicEQCurvePath();
            lastDynCurveRebuildMs = now;
        }

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
        const juce::Colour overlayColour = ModernLookAndFeel::Colors::accentYellow.brighter(0.15f);
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
        // Width-driven per-pixel sampling: one frequency per visible pixel
        // of graphBounds, derived via xToFreq(x). Rendering-resolution grid
        // eliminates high-Q polygonal spikes without smoothing the response.
        const int targetCount = juce::jmax(2, (int) std::ceil(graphBounds.getWidth()) + 1);

        if ((int) eqCurveFrequencies.size() == targetCount)
            return;

        eqCurveFrequencies.resize((size_t) targetCount);
        eqCurveMagnitudes.resize((size_t) targetCount);

        for (int i = 0; i < targetCount; ++i)
        {
            const float x = graphBounds.getX() + (float) i;
            eqCurveFrequencies[(size_t) i] = xToFreq(x);
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

    // P1-5: previous/current hop columns + displayed lerp (same t for pre and post).
    std::vector<float> hopPrevPre, hopCurrPre;
    std::vector<float> hopPrevPost, hopCurrPost;
    std::vector<float> displayPre, displayPost;
    uint64_t lastLerpHopVersion = 0;
    float spectrumLerpT = 1.0f;
    float lastRebuiltLerpT = -1.0f;
    double lastHopTimeMs = 0.0;
    double lastDisplayTickMs = 0.0;
    double hopIntervalMs = 21.0; // ~47 Hz default hop at 4096
    static constexpr double kHopIntervalMinMs = 16.0;
    static constexpr double kHopIntervalMaxMs = 50.0;
    static constexpr float kLerpRebuildEpsilon = 0.05f;

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

    // GL spectrum active — when true, pre/post spectrum is rendered by GL shader;
    // software path skips pre/post path builds to avoid double rendering (~6ms saved).
    bool glSpectrumActive = false;

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

    // FabFilter-style node visibility: nodes dim (but stay visible) when
    // the cursor leaves the spectrum, and come back to full opacity when
    // it re-enters. Smooth transition driven by timerCallback.
    // These four values are one salience ladder. Do not edit one in isolation.
    //   1.00 > kNodesCalmEmphasis > 0.60 > 0.35
    // P0-A: ship calm=0.70 so 18–24 idle nodes don't compete with selection
    static constexpr float kNodesPrimaryEmphasis = 1.00f;
    static constexpr float kNodesCalmEmphasis = 0.70f;
    static constexpr float kNodesUnfocusedEmphasis = 0.50f;
    static constexpr float kNodesIdleOpacity = 0.35f;
    static_assert(kNodesPrimaryEmphasis > kNodesCalmEmphasis);
    static_assert(kNodesCalmEmphasis > kNodesUnfocusedEmphasis);
    static_assert(kNodesUnfocusedEmphasis > kNodesIdleOpacity);
    float nodesOpacity = kNodesIdleOpacity;         // current opacity [0..1]
    float nodesTargetOpacity = kNodesIdleOpacity;   // target: 1 when mouse in, idle when out
    bool  mouseInsideSpectrum = false;              // raw tracking flag

    // AI breathing phase (0..2π) — drives subtle glow pulsation on nodes
    // that have pending AI corrections. Fed from editor's breathingPhase.
    float aiBreathingPhase = 0.0f;

    // Guard: keep nodes visible while a per-band context menu is open.
    // Without this, mouseExit fires when the popup appears, fading out
    // the nodes while the user is still looking at the menu.
    bool bandContextMenuOpen = false;
    BandRadialMenu bandRadialMenu;
    AnalyzerContextMenu analyzerContextMenu;

    // ── Tilt drag widget (bottom-left of spectrum) ──────────────────────
    // Click + drag up/down to continuously adjust spectrum tilt (dB/oct).
    juce::Rectangle<float> tiltWidgetBounds;   // set in paint()
    bool isDraggingTilt = false;
    float tiltDragStartY = 0.0f;
    float tiltDragStartValue = 0.0f;

    // ── Phase 7C: AI contextual tooltip state ───────────────────────────────
    struct AITooltipState
    {
        bool visible = false;
        juce::Rectangle<float> anchor;           // where the tooltip is anchored (near the zone)
        juce::String title;
        juce::String description;
        juce::String suggestion;
        int correctionIdx = -1;                  // live index for the AI overlay's "active" highlight
        juce::Rectangle<float> fixButtonBounds;  // for hit testing the FIX button
        juce::Rectangle<float> tooltipBounds;    // Wave 4A: for mouseMove hit-testing — keep
                                                 // tooltip visible while cursor is INSIDE the
                                                 // tooltip rect (even if it has left the ambra zone)

        // Hero Graph Polish v1.1 — payload snapshot taken when the tooltip first
        // shows for a correction. Used to (a) decide whether a subsequent hover
        // is the "same" correction (so anchor + payload don't jitter while the
        // AI re-analyses), and (b) re-resolve the FIX click against a fresh
        // pendingCorrections vector so we never approve the wrong correction
        // because the index shifted under us.
        float                 snapshotFrequency     = 0.0f;
        float                 snapshotSuggestedGain = 0.0f;
        float                 snapshotSuggestedQ    = 0.0f;
        AIEngine::ProblemType snapshotType          = AIEngine::ProblemType::None;
    };
    AITooltipState aiTooltip;

    // Hero Graph Polish v1.4 — single source of truth for the AI tooltip
    // identity criterion. Both the mouseMove "same correction" test and the
    // mouseDown FIX click resolver MUST use the same tolerances, otherwise
    // the active overlay highlight, the tooltip text and the FIX target can
    // disagree in dense scenes (Codex P1, fixed in 029743db). Tweaking any
    // value here updates both call sites in lock-step.
    static constexpr float kTooltipFreqRatioMax  = 0.10f;  // |log2(f / fSnap)|, ~±7.2 %
    static constexpr float kTooltipGainDiffMaxDb = 3.0f;   // |Δ suggestedGain|, dB
    static constexpr float kTooltipQDiffMax      = 1.5f;   // |Δ suggestedQ|
    static constexpr float kTooltipPixelDriftMax = 24.0f;  // auxiliary geometric guard, px

    // FIX 4: Off-screen grid + labels cache (static between resizes)
    juce::Image gridCache;
    bool gridCacheDirty = true;

    // FIX 2: Cached spectrum paths — rebuilt only when spectrum version changes
    // Wave 5: post now has a fill path too (for the luminous cyan base layer
    // under the 2-layer stroke). Matches the GL pipeline's pre/post symmetry.
    juce::Path cachedPreLine, cachedPreFill;
    juce::Path cachedPostLine, cachedPostFill;
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

    // Adaptive timer — tracks current rate to avoid redundant startTimerHz calls
    int currentTimerHz = 60;

    // ── Dynamic EQ live overlay ──────────────────────────────────────────────
    // Cached dynamic EQ curve path + per-point coordinate arrays for fill construction
    juce::Path cachedDynamicEQCurve;
    std::vector<float> dynCurveDeltaDb;      // replacement delta returned by DSP evaluator
    std::vector<float> dynCurveXPoints;       // X pixel positions (shared between static & dynamic)
    std::vector<float> dynCurveYPoints;       // Y pixel positions (dynamic curve)
    std::vector<float> staticCurveYPoints;    // Y pixel positions (static curve, for fill)
    // Whether any band is currently owned by the dynamic stage
    bool anyDynamicBandActive = false;
    double lastDynCurveRebuildMs = 0.0;  // throttle DynEQ curve rebuilds to ~30 Hz
    uint64_t lastDynCurveEQVersion = std::numeric_limits<uint64_t>::max();
    juce::Rectangle<float> lastDynCurveBounds;
    // ────────────────────────────────────────────────────────────────────────


    // Pre-cached fonts — avoid Font construction in paint() (0.3-0.5ms per frame saved)
    juce::Font bandNumberFont  { juce::FontOptions().withHeight(10.0f).withStyle("Bold") };
    juce::Font soloBadgeFont   { juce::FontOptions().withHeight(8.0f).withStyle("Bold") };
    juce::Font aiMarkerFont    { juce::FontOptions().withHeight(10.0f).withStyle("Bold") };
    juce::Font tooltipTitleFont{ juce::FontOptions().withHeight(12.0f).withStyle("Bold") };
    juce::Font tooltipDescFont { juce::FontOptions().withHeight(11.0f) };
    juce::Font tooltipSuggFont { juce::FontOptions().withHeight(11.0f).withStyle("Italic") };
    juce::Font tooltipFixFont  { juce::FontOptions().withHeight(10.0f).withStyle("Bold") };
    juce::Font bandTooltipBold { juce::FontOptions().withHeight(11.0f).withStyle("Bold") };
    juce::Font bandTooltipVal  { juce::FontOptions().withHeight(13.0f) };
    juce::Font bandTooltipQ    { juce::FontOptions().withHeight(10.0f) };

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
