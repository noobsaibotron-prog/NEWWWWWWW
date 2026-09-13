#pragma once
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"
#include "../../DSP/thermal.h"
#include "EmberTokens.h"
#include "EmberMeters.h"
#include "EmberInspector.h"
#include "EmberLookAndFeel.h"
#include "EmberPhraseParser.h"
#include <vector>

class EmberGraph : public juce::Component
{
public:
    enum class SpecView { Pre, Post, Delta };

    std::function<void(int)> onBandSelected; // -1 = deselect
    std::function<void()> onRequestRepaint;

    EmberGraph(AIEqualizerAudioProcessor& p, EmberLookAndFeel& sharedLaf)
        : processor(p), laf(sharedLaf)
    {
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
        addAndMakeVisible(meters);
        inspector = std::make_unique<EmberInspector>();
        addChildComponent(*inspector);
        inspector->onFreq = [this](float v){ writeGeometry(selected, true, v, false, 0, false, 0, -1); };
        inspector->onGain = [this](float v){ writeGeometry(selected, false, 0, true, v, false, 0, -1); };
        inspector->onQ    = [this](float v){ writeGeometry(selected, false, 0, false, 0, true, v, -1); };
        inspector->onType = [this](int t){ writeGeometry(selected, false, 0, false, 0, false, 0, t); };

        for (auto* b : { &btnPre, &btnPost, &btnDelta })
        {
            b->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b->setColour(juce::TextButton::textColourOffId, EmberTokens::mute);
            b->setColour(juce::TextButton::textColourOnId, EmberTokens::text);
            b->setClickingTogglesState(true);
            b->setRadioGroupId(8801);
            addAndMakeVisible(*b);
        }
        btnPost.setToggleState(true, juce::dontSendNotification);
        btnPre.onClick = [this]{ specView = SpecView::Pre; repaint(); };
        btnPost.onClick = [this]{ specView = SpecView::Post; repaint(); };
        btnDelta.onClick = [this]{ specView = SpecView::Delta; repaint(); };

        refLabel.setText("Ref A", juce::dontSendNotification);
        refLabel.setFont(laf.getMetaFont());
        refLabel.setColour(juce::Label::textColourId, EmberTokens::cyan);
        addChildComponent(refLabel);
    }

    void setUiState(EmberUiState s)
    {
        state = s;
        refLabel.setVisible(s == EmberUiState::Match && hasMatchRef);
        updateInspectorVisibility();
        repaint();
    }

    void setGhosts(std::vector<EmberGhostBand> g, float intensity)
    {
        ghosts = std::move(g);
        ghostIntensity = intensity;
        repaint();
    }

    /** On-screen amber ghosts with intensity already applied to dB (Apply commit source). */
    [[nodiscard]] std::vector<EmberGhostBand> getEffectiveGhosts() const
    {
        auto out = ghosts;
        for (auto& g : out)
            g.db *= ghostIntensity;
        return out;
    }

    void setApplyProgress(float p01) { applyProgress = juce::jlimit(0.0f, 1.0f, p01); repaint(); }

    void setClimate(bool on)
    {
        climateOn = on;
        if (! on)
            thermal.reset();
        resized();
        repaint();
    }

    void setSpectrumPixels(const std::vector<float>& pre,
                           const std::vector<float>& post)
    {
        prePx = pre;
        postPx = post;
        if (climateOn && ! post.empty())
            updateThermalFromSpectrum(post);
        repaint();
    }

    void setMatchOverlay(bool active, const std::vector<float>& curveDb, float amount)
    {
        hasMatchRef = active && ! curveDb.empty();
        matchCurveDb = curveDb;
        matchAmount = amount;
        refLabel.setVisible(state == EmberUiState::Match && hasMatchRef);
        repaint();
    }

    void setLevels(float l, float r) { meters.setLevels(l, r); }

    void selectBand(int idx)
    {
        selected = idx;
        updateInspectorVisibility();
        if (onBandSelected) onBandSelected(idx);
        repaint();
    }

    int getSelectedBand() const { return selected; }
    juce::Rectangle<float> getPlotBounds() const { return plotBounds; }

    void paint(juce::Graphics& g) override
    {
        g.setColour(EmberTokens::sunken);
        g.fillRect(getLocalBounds());

        rebuildEQCurve();

        // 2. grid
        paintGrid(g);

        // 3. spectrum
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(plotBounds.toNearestInt());
            paintSpectrum(g);
        }

        // 4. MATCH overlay
        if (state == EmberUiState::Match && hasMatchRef)
            paintMatchOverlay(g);

        // 5. F white
        paintFactCurve(g);

        // 6. ghost dashed gold
        if (state == EmberUiState::Frase || state == EmberUiState::Apply)
            paintGhost(g);

        // 7. nodes
        paintNodes(g);

        // 8. climate
        if (climateOn)
            paintClimate(g);

        // 9. labels
        paintAxisLabels(g);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto scope = r.removeFromTop(18).removeFromRight(160);
        btnDelta.setBounds(scope.removeFromRight(50));
        btnPost.setBounds(scope.removeFromRight(50));
        btnPre.setBounds(scope.removeFromRight(50));
        refLabel.setBounds(r.removeFromTop(14).removeFromRight(48));

        const int climateExtra = climateOn ? EmberTokens::climateH : 0;
        plotBounds = juce::Rectangle<float>((float) EmberTokens::padL,
                                            (float) EmberTokens::padT + 18.0f,
                                            (float) juce::jmax(1, getWidth() - EmberTokens::padL - EmberTokens::padR - EmberTokens::meterW - 8),
                                            (float) juce::jmax(1, getHeight() - EmberTokens::padT - EmberTokens::padB - 18 - climateExtra));
        climateBounds = { plotBounds.getX(), plotBounds.getBottom() + 2.0f,
                          plotBounds.getWidth(), (float) EmberTokens::climateH };

        meters.setBounds((int) plotBounds.getRight() + 8, (int) plotBounds.getY(),
                         EmberTokens::meterW, (int) plotBounds.getHeight());
        positionInspector();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! plotBounds.contains(e.position))
            return;

        const int hit = hitTestNode(e.position);
        if (hit >= 0)
        {
            selectBand(hit);
            dragging = true;
            dragBand = hit;
            altLockAxis = 0;
            firstDragDelta = {};
            auto st = processor.getBandState(hit);
            dragStartFreq = st.frequency;
            dragStartGain = st.gain;
            return;
        }

        if (selected >= 0)
        {
            // click empty with selection → deselect (not add)
            selectBand(-1);
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragging || dragBand < 0)
            return;

        auto delta = e.position - e.mouseDownPosition;
        if (e.mods.isAltDown())
        {
            if (altLockAxis == 0)
            {
                if (std::abs(delta.x) > 2.0f || std::abs(delta.y) > 2.0f)
                    altLockAxis = (std::abs(delta.x) >= std::abs(delta.y)) ? 1 : 2;
            }
            if (altLockAxis == 1) delta.y = 0;
            if (altLockAxis == 2) delta.x = 0;
        }

        float freq = xToFreq(e.mouseDownPosition.x + delta.x);
        float gain = yToGain(e.mouseDownPosition.y + delta.y);
        freq = juce::jlimit(EmberTokens::minHz, EmberTokens::maxHz, freq);
        gain = juce::jlimit(EmberTokens::curveMinDb, EmberTokens::curveMaxDb, gain);
        writeGeometry(dragBand, true, freq, true, gain, false, 0, -1);
        positionInspector();
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragging = false;
        dragBand = -1;
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (! plotBounds.contains(e.position))
            return;
        if (hitTestNode(e.position) >= 0)
            return;
        addPeakAt(xToFreq(e.position.x), yToGain(e.position.y));
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const int hit = selected >= 0 ? selected : hitTestNode(e.position);
        if (hit < 0) return;
        auto st = processor.getBandState(hit);
        const float step = e.mods.isShiftDown() ? 0.02f : 0.08f;
        st.q = juce::jlimit(0.2f, 10.0f, st.q + wheel.deltaY * step * 10.0f);
        writeGeometry(hit, false, 0, false, 0, true, st.q, -1);
        if (inspector->isVisible())
            inspector->setValues(st.frequency, st.gain, st.q, st.type);
        repaint();
    }

private:
    void writeGeometry(int band, bool setF, float freq, bool setG, float gain, bool setQ, float q, int type)
    {
        if (band < 0 || band >= AIEqualizerAudioProcessor::maxBands)
            return;
        auto st = processor.getBandState(band);
        if (setF) st.frequency = freq;
        if (setG) st.gain = gain;
        if (setQ) st.q = juce::jlimit(0.2f, 10.0f, q);
        if (type >= 0) st.type = type;
        processor.setBandGeometry(band, st.frequency, st.gain, st.q, st.type, true);
        if (inspector->isVisible() && band == selected)
            inspector->setValues(st.frequency, st.gain, st.q, st.type);
    }

    void addPeakAt(float hz, float db)
    {
        const int n = processor.getNumActiveBands();
        int slot = -1;
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            auto st = processor.getBandState(i);
            if (! st.enabled || std::abs(st.gain) < 0.05f)
            {
                slot = i;
                break;
            }
        }
        if (slot < 0)
            slot = juce::jmin(n, AIEqualizerAudioProcessor::maxBands - 1);
        if (slot >= n)
            processor.setNumActiveBands(slot + 1);
        processor.setBandGeometry(slot, hz, db, 1.0f, 2, true);
        selectBand(slot);
    }

    int hitTestNode(juce::Point<float> p) const
    {
        const int n = processor.getNumActiveBands();
        const float hitR = (selected >= 0 ? 10.0f : 8.0f);
        int best = -1;
        float bestD = hitR * hitR;
        for (int i = 0; i < n; ++i)
        {
            auto st = processor.getBandState(i);
            if (! st.enabled) continue;
            const float x = freqToX(st.frequency);
            const float y = gainToY(st.gain);
            const float d = (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y);
            if (d <= bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void updateInspectorVisibility()
    {
        // Brief: inspector only when node selected
        const bool really = selected >= 0;
        inspector->setVisible(really);
        if (really)
        {
            auto st = processor.getBandState(selected);
            inspector->setValues(st.frequency, st.gain, st.q, st.type);
            positionInspector();
        }
    }

    void positionInspector()
    {
        if (selected < 0 || ! inspector->isVisible())
            return;
        auto st = processor.getBandState(selected);
        const float x = freqToX(st.frequency);
        const float y = gainToY(st.gain);
        const int w = EmberTokens::inspectorW;
        const int h = 120;
        int ix = (int) std::round(x - w * 0.5f);
        int iy = (int) std::round(y + 10.0f);
        ix = juce::jlimit(0, juce::jmax(0, getWidth() - w), ix);
        iy = juce::jlimit(0, juce::jmax(0, getHeight() - h), iy);
        inspector->setBounds(ix, iy, w, h);
        inspector->toFront(false);
    }

    float freqToX(float hz) const
    {
        const float n = std::log(juce::jlimit(EmberTokens::minHz, EmberTokens::maxHz, hz) / EmberTokens::minHz)
                      / std::log(EmberTokens::maxHz / EmberTokens::minHz);
        return plotBounds.getX() + n * plotBounds.getWidth();
    }
    float xToFreq(float x) const
    {
        const float n = juce::jlimit(0.0f, 1.0f, (x - plotBounds.getX()) / juce::jmax(1.0f, plotBounds.getWidth()));
        return EmberTokens::minHz * std::pow(EmberTokens::maxHz / EmberTokens::minHz, n);
    }
    float gainToY(float db) const
    {
        const float n = (db - EmberTokens::curveMaxDb) / (EmberTokens::curveMinDb - EmberTokens::curveMaxDb);
        return plotBounds.getY() + juce::jlimit(0.0f, 1.0f, n) * plotBounds.getHeight();
    }
    float yToGain(float y) const
    {
        const float n = juce::jlimit(0.0f, 1.0f, (y - plotBounds.getY()) / juce::jmax(1.0f, plotBounds.getHeight()));
        return EmberTokens::curveMaxDb + n * (EmberTokens::curveMinDb - EmberTokens::curveMaxDb);
    }
    float specDbToY(float db) const
    {
        const float n = (db - EmberTokens::specMaxDb) / (EmberTokens::specMinDb - EmberTokens::specMaxDb);
        return plotBounds.getY() + juce::jlimit(0.0f, 1.0f, n) * plotBounds.getHeight();
    }

    void rebuildEQCurve()
    {
        const int N = juce::jmax(64, (int) plotBounds.getWidth());
        if ((int) curveHz.size() != N)
        {
            curveHz.resize((size_t) N);
            curveMag.resize((size_t) N);
            for (int i = 0; i < N; ++i)
            {
                const float x = plotBounds.getX() + (float) i;
                curveHz[(size_t) i] = xToFreq(x);
            }
        }
        processor.getEQProcessor().getMagnitudeForFrequencyArray(
            curveHz.data(), curveMag.data(), (size_t) N, processor.getSampleRate());
    }

    void paintGrid(juce::Graphics& g)
    {
        g.setColour(EmberTokens::grid);
        for (float hz : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            const float x = freqToX(hz);
            g.drawVerticalLine((int) x, plotBounds.getY(), plotBounds.getBottom());
        }
        for (float db : { -6.f, 0.f, 6.f })
        {
            const float y = gainToY(db);
            g.drawHorizontalLine((int) y, plotBounds.getX(), plotBounds.getRight());
        }
    }

    void paintSpectrum(juce::Graphics& g)
    {
        const std::vector<float>* src = &postPx;
        if (specView == SpecView::Pre) src = &prePx;
        if (src->empty()) return;

        juce::Path fill, stroke, peak;
        const int n = (int) src->size();
        const float bottom = plotBounds.getBottom();
        bool started = false;
        for (int i = 0; i < n; ++i)
        {
            float db = (*src)[(size_t) i];
            if (specView == SpecView::Delta)
            {
                // DELTA = energy of curve F, not a third FFT
                const float x = plotBounds.getX() + (float) i * plotBounds.getWidth() / (float) juce::jmax(1, n - 1);
                const float hz = xToFreq(x);
                // sample F at this x
                float fdb = 0.0f;
                if (! curveHz.empty())
                {
                    // nearest
                    size_t idx = (size_t) juce::jlimit(0, (int) curveHz.size() - 1,
                        (int) std::round((x - plotBounds.getX()) / juce::jmax(1.0f, plotBounds.getWidth()) * (float) (curveHz.size() - 1)));
                    fdb = juce::Decibels::gainToDecibels(juce::jmax(1.0e-6f, curveMag[idx]), -48.0f);
                }
                db = fdb; // draw |H(f)| in spectrum space (scaled)
                juce::ignoreUnused(hz);
            }

            const float x = plotBounds.getX() + (float) i * plotBounds.getWidth() / (float) juce::jmax(1, n - 1);
            const float y = specDbToY(db);
            if (! started)
            {
                fill.startNewSubPath(x, bottom);
                fill.lineTo(x, y);
                stroke.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                fill.lineTo(x, y);
                stroke.lineTo(x, y);
            }
        }
        if (started)
        {
            fill.lineTo(plotBounds.getRight(), bottom);
            fill.closeSubPath();
            juce::ColourGradient grad(EmberTokens::cyan.withAlpha(EmberTokens::alphaSpectrumFillTop), 0, plotBounds.getY(),
                                      EmberTokens::cyan.withAlpha(EmberTokens::alphaSpectrumFillBottom), 0, bottom, false);
            g.setGradientFill(grad);
            g.fillPath(fill);
            g.setColour(EmberTokens::cyan.withAlpha(EmberTokens::alphaSpectrumLine));
            g.strokePath(stroke, juce::PathStrokeType(EmberTokens::strokeSpectrum));
        }
    }

    void paintFactCurve(juce::Graphics& g)
    {
        if (curveHz.empty()) return;
        juce::Path p;
        for (size_t i = 0; i < curveHz.size(); ++i)
        {
            const float db = juce::Decibels::gainToDecibels(juce::jmax(1.0e-6f, curveMag[i]), -48.0f);
            const float x = freqToX(curveHz[i]);
            const float y = gainToY(db);
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        g.setColour(EmberTokens::text);
        g.strokePath(p, juce::PathStrokeType(EmberTokens::strokeFactCurve, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void paintGhost(juce::Graphics& g)
    {
        if (ghosts.empty()) return;
        // Draw dashed gold response of ghost bands only (intent, not audio).
        const int N = (int) curveHz.size();
        if (N <= 1) return;
        juce::Path p;
        const float fade = (state == EmberUiState::Apply) ? (1.0f - applyProgress) : 1.0f;
        for (int i = 0; i < N; ++i)
        {
            const float hz = curveHz[(size_t) i];
            float db = 0.0f;
            for (const auto& gb : ghosts)
            {
                const float g0 = gb.db * ghostIntensity * fade;
                // simple peaking / shelf approx for ghost display
                const float ratio = hz / juce::jmax(1.0f, gb.hz);
                const float w = std::log2(juce::jmax(1.0e-6f, ratio));
                const float shape = std::exp(-0.5f * (w * w) * (gb.q * gb.q) * 4.0f);
                db += g0 * shape;
            }
            const float x = freqToX(hz);
            const float y = gainToY(db);
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        float dashes[] = { 4.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType(EmberTokens::strokeGhostCurve).createDashedStroke(dashed, p, dashes, 2);
        g.setColour(EmberTokens::gold.withAlpha(EmberTokens::alphaGhostCurve * fade));
        g.strokePath(dashed, juce::PathStrokeType(EmberTokens::strokeGhostCurve));

        // Amber readouts = the exact vector Apply will commit (intensity baked).
        g.setFont(laf.getMetaFont());
        for (const auto& gb : ghosts)
        {
            const float g0 = gb.db * ghostIntensity;
            const float x = freqToX(gb.hz);
            const float y = gainToY(g0 * fade);
            const float r = 4.0f;
            g.setColour(EmberTokens::gold.withAlpha(EmberTokens::alphaGhostNode * fade));
            g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour(EmberTokens::intent.withAlpha(EmberTokens::alphaGhostReadout * fade));
            const juce::String line = EmberPhrase::formatHzChip(gb.hz) + "  "
                + EmberPhrase::formatDbChip(g0) + "  Q "
                + juce::String(gb.q, 2);
            g.drawText(line, (int) x - 54, (int) y - 18, 110, 12, juce::Justification::centred);
        }
    }

    void paintMatchOverlay(juce::Graphics& g)
    {
        if (matchCurveDb.empty()) return;
        juce::Path p;
        const int n = (int) matchCurveDb.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = plotBounds.getX() + (float) i * plotBounds.getWidth() / (float) juce::jmax(1, n - 1);
            const float y = gainToY(matchCurveDb[(size_t) i] * matchAmount);
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        g.setColour(EmberTokens::cyan.withAlpha(EmberTokens::alphaMatchCurve));
        g.strokePath(p, juce::PathStrokeType(EmberTokens::strokeMatchCurve));
    }

    void paintNodes(juce::Graphics& g)
    {
        const int n = processor.getNumActiveBands();
        for (int i = 0; i < n; ++i)
        {
            auto st = processor.getBandState(i);
            if (! st.enabled) continue;
            const float x = freqToX(st.frequency);
            const float y = gainToY(st.gain);
            const float r = (i == selected) ? 6.0f : 3.5f;
            g.setColour(EmberTokens::text);
            g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
            if (i == selected)
            {
                g.setColour(EmberTokens::text.withAlpha(EmberTokens::alphaSelectedRing));
                g.drawEllipse(x - r - 3.0f, y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f, EmberTokens::strokeSelectedRing);
            }
        }
    }

    void paintClimate(juce::Graphics& g)
    {
        const float cellW = climateBounds.getWidth() / (float) ember::ThermalState::kNumCells;
        for (int k = 0; k < ember::ThermalState::kNumCells; ++k)
        {
            const float t = thermal.T[(size_t) k];
            const juce::Colour c = EmberTokens::cyan.interpolatedWith(EmberTokens::intent, t).withAlpha(t * EmberTokens::alphaClimate);
            g.setColour(c);
            g.fillRect(climateBounds.getX() + (float) k * cellW, climateBounds.getY(), cellW + 0.5f, climateBounds.getHeight());
        }
    }

    void paintAxisLabels(juce::Graphics& g)
    {
        g.setFont(laf.getMetaFont());
        g.setColour(EmberTokens::mute);
        const float labelHz[] = { 100.f, 1000.f, 10000.f };
        const char* labelTxt[] = { "100", "1k", "10k" };
        for (int li = 0; li < 3; ++li)
        {
            g.drawText(labelTxt[li], (int) freqToX(labelHz[li]) - 12, (int) plotBounds.getBottom() + 2, 24, 12,
                       juce::Justification::centred);
        }
        g.drawText("+6", 4, (int) gainToY(6.0f) - 6, 30, 12, juce::Justification::centredLeft);
        g.drawText("0", 4, (int) gainToY(0.0f) - 6, 30, 12, juce::Justification::centredLeft);
        g.drawText("-6", 4, (int) gainToY(-6.0f) - 6, 30, 12, juce::Justification::centredLeft);
    }

    void updateThermalFromSpectrum(const std::vector<float>& post)
    {
        std::array<float, ember::ThermalState::kNumCells> cells {};
        const int n = (int) post.size();
        for (int k = 0; k < ember::ThermalState::kNumCells; ++k)
        {
            const int i0 = k * n / ember::ThermalState::kNumCells;
            const int i1 = (k + 1) * n / ember::ThermalState::kNumCells;
            float sum = 0.0f;
            int c = 0;
            for (int i = i0; i < i1 && i < n; ++i) { sum += post[(size_t) i]; ++c; }
            cells[(size_t) k] = c > 0 ? sum / (float) c : EmberTokens::specMinDb;
        }
        const double now = juce::Time::getMillisecondCounterHiRes();
        float dt = 1.0f / 45.0f;
        if (lastThermalMs > 0.0)
            dt = (float) juce::jlimit(0.001, 0.1, (now - lastThermalMs) * 0.001);
        lastThermalMs = now;
        thermal.process(cells.data(), dt);
    }

    AIEqualizerAudioProcessor& processor;
    EmberLookAndFeel& laf;
    EmberMeters meters;
    std::unique_ptr<EmberInspector> inspector;
    juce::TextButton btnPre { "PRE" }, btnPost { "POST" }, btnDelta { "DELTA" };
    juce::Label refLabel;

    EmberUiState state { EmberUiState::Riposo };
    SpecView specView { SpecView::Post };
    int selected = -1;
    bool dragging = false;
    int dragBand = -1;
    int altLockAxis = 0;
    juce::Point<float> firstDragDelta;
    float dragStartFreq = 1000.0f, dragStartGain = 0.0f;

    std::vector<EmberGhostBand> ghosts;
    float ghostIntensity = 1.0f;
    float applyProgress = 0.0f;

    bool climateOn = false;
    ember::ThermalState thermal;
    double lastThermalMs = 0.0;

    bool hasMatchRef = false;
    std::vector<float> matchCurveDb;
    float matchAmount = 1.0f;

    std::vector<float> prePx, postPx;
    std::vector<float> curveHz, curveMag;
    juce::Rectangle<float> plotBounds, climateBounds;
};
