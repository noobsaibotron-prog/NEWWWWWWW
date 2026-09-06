#pragma once
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"
#include "../NewSpectrumPipeline.h"
#include "EmberLookAndFeel.h"
#include "EmberHeader.h"
#include "EmberGraph.h"
#include "EmberBar.h"
#include "EmberOverflow.h"
#include "EmberPhraseParser.h"

/** Top-level Ember Core UI v2 host (SPECCHIO). */
class EmberV2Shell : public juce::Component,
                     private juce::Timer
{
public:
    EmberV2Shell(AIEqualizerAudioProcessor& p, NewSpectrumPipeline* pipeline)
        : processor(p), spectrumPipeline(pipeline)
    {
        setLookAndFeel(&laf);
        header = std::make_unique<EmberHeader>(processor.getAPVTS(), laf);
        graph  = std::make_unique<EmberGraph>(processor, laf);
        bar    = std::make_unique<EmberBar>(laf);
        overflow = std::make_unique<EmberOverflow>(processor.getAPVTS(), laf);

        addAndMakeVisible(*header);
        addAndMakeVisible(*graph);
        addAndMakeVisible(*bar);
        addChildComponent(*overflow);

        header->onMenu = [this]
        {
            overflow->setVisible(! overflow->isVisible());
            resized();
        };

        overflow->onClimateChanged = [this](bool on)
        {
            graph->setClimate(on);
            resized();
        };

        graph->onBandSelected = [this](int idx)
        {
            if (idx >= 0)
                setState(EmberUiState::Nodo);
            else if (state == EmberUiState::Nodo)
                setState(phraseAlive ? EmberUiState::Frase : EmberUiState::Riposo);
        };

        bar->onEnterFrase = [this]
        {
            phraseAlive = true;
            setState(EmberUiState::Frase);
            bar->focusPhrase();
        };
        bar->onCancelFrase = [this]
        {
            phraseAlive = false;
            bar->clearPhrase();
            graph->setGhosts({}, 1.0f);
            setState(graph->getSelectedBand() >= 0 ? EmberUiState::Nodo : EmberUiState::Riposo);
        };
        bar->onPhraseChanged = [this](const juce::String& text)
        {
            auto ghosts = EmberPhrase::parse(text);
            for (auto& g : ghosts)
                g.db *= bar->getIntensity();
            graph->setGhosts(ghosts, 1.0f);
        };
        bar->onIntensity = [this](float)
        {
            auto ghosts = EmberPhrase::parse(bar->getPhrase());
            graph->setGhosts(ghosts, bar->getIntensity());
        };
        bar->onApply = [this]{ beginApply(); };
        bar->onMatchToggle = [this]
        {
            if (state == EmberUiState::Match)
            {
                graph->setMatchOverlay(false, {}, 0.0f);
                setState(phraseAlive ? EmberUiState::Frase : EmberUiState::Riposo);
            }
            else
            {
                captureMatchRef();
                setState(EmberUiState::Match);
            }
        };
        bar->onMatchAmount = [this](float a)
        {
            // amount scales overlay only — never writes APVTS bands
            graph->setMatchOverlay(true, matchRefDb, a);
        };

        setWantsKeyboardFocus(true);
        setState(EmberUiState::Riposo);
        startTimerHz(45);
        barHeightAnim = (float) EmberTokens::barRiposoH;
    }

    ~EmberV2Shell() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(EmberTokens::bg);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        header->setBounds(r.removeFromTop(EmberTokens::headerH));
        const int barH = (int) std::round(barHeightAnim);
        bar->setBounds(r.removeFromBottom(barH));
        if (overflow->isVisible())
            overflow->setBounds(r.removeFromRight(EmberTokens::overflowW));
        graph->setBounds(r);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (overflow->isVisible()) { overflow->setVisible(false); resized(); return true; }
            if (state == EmberUiState::Match)
            {
                graph->setMatchOverlay(false, {}, 0.0f);
                setState(EmberUiState::Riposo);
                return true;
            }
            if (state == EmberUiState::Frase)
            {
                bar->onCancelFrase();
                return true;
            }
            if (state == EmberUiState::Nodo)
            {
                graph->selectBand(-1);
                setState(EmberUiState::Riposo);
                return true;
            }
        }
        if (key.getTextCharacter() == '/' && (state == EmberUiState::Riposo || state == EmberUiState::Nodo))
        {
            bar->onEnterFrase();
            return true;
        }
        return false;
    }

private:
    void setState(EmberUiState s)
    {
        state = s;
        header->updateBadge();
        graph->setUiState(s);
        bar->setUiState(s);
        targetBarH = (s == EmberUiState::Riposo || (s == EmberUiState::Nodo && ! phraseAlive))
                     ? (float) EmberTokens::barRiposoH
                     : (float) EmberTokens::barActiveH;
    }

    void beginApply()
    {
        pendingGhosts = EmberPhrase::parse(bar->getPhrase());
        const float intens = bar->getIntensity();
        for (auto& g : pendingGhosts)
            g.db *= intens;
        if (pendingGhosts.empty())
            return;

        setState(EmberUiState::Apply);
        applyMsLeft = 280.0;
        graph->setGhosts(pendingGhosts, 1.0f);
        graph->setApplyProgress(0.0f);
    }

    void finishApply()
    {
        // Unique write I→F
        for (const auto& g : pendingGhosts)
        {
            int slot = findSlotForGhost(g);
            if (slot < 0) continue;
            if (slot >= processor.getNumActiveBands())
                processor.setNumActiveBands(slot + 1);
            processor.setBandGeometry(slot, g.hz, g.db, g.q, g.type, true);
        }
        pendingGhosts.clear();
        phraseAlive = false;
        bar->clearPhrase();
        graph->setGhosts({}, 1.0f);
        graph->setApplyProgress(1.0f);
        graph->selectBand(-1);
        setState(EmberUiState::Riposo);
    }

    int findSlotForGhost(const EmberGhostBand& g) const
    {
        int best = -1;
        float bestDist = 1.0e9f;
        for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
        {
            auto st = processor.getBandState(i);
            if (! st.enabled || std::abs(st.gain) < 0.05f)
            {
                const float d = std::abs(std::log2(juce::jmax(20.0f, st.frequency) / juce::jmax(20.0f, g.hz)));
                if (d < bestDist) { bestDist = d; best = i; }
            }
        }
        if (best >= 0) return best;
        // reuse closest by frequency among active
        const int n = processor.getNumActiveBands();
        for (int i = 0; i < n; ++i)
        {
            auto st = processor.getBandState(i);
            const float d = std::abs(std::log2(juce::jmax(20.0f, st.frequency) / juce::jmax(20.0f, g.hz)));
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best < 0)
            best = juce::jmin(n, AIEqualizerAudioProcessor::maxBands - 1);
        return best;
    }

    void captureMatchRef()
    {
        // Snapshot current F curve into R (display only)
        matchRefDb.clear();
        const int N = 256;
        matchRefDb.resize((size_t) N, 0.0f);
        std::vector<float> hz((size_t) N), mag((size_t) N, 1.0f);
        for (int i = 0; i < N; ++i)
        {
            const float n = (float) i / (float) (N - 1);
            hz[(size_t) i] = EmberTokens::minHz * std::pow(EmberTokens::maxHz / EmberTokens::minHz, n);
        }
        processor.getEQProcessor().getMagnitudeForFrequencyArray(
            hz.data(), mag.data(), (size_t) N, processor.getSampleRate());
        for (int i = 0; i < N; ++i)
            matchRefDb[(size_t) i] = juce::Decibels::gainToDecibels(juce::jmax(1.0e-6f, mag[(size_t) i]), -48.0f);
        graph->setMatchOverlay(true, matchRefDb, bar->getMatchAmount());
    }

    void timerCallback() override
    {
        // bar height ease ~220ms
        const float alpha = 1.0f - std::exp(-1.0f / (0.220f * 45.0f));
        if (std::abs(barHeightAnim - targetBarH) > 0.25f)
        {
            barHeightAnim += (targetBarH - barHeightAnim) * alpha;
            resized();
        }
        else if (barHeightAnim != targetBarH)
        {
            barHeightAnim = targetBarH;
            resized();
        }

        if (state == EmberUiState::Apply)
        {
            applyMsLeft -= 1000.0 / 45.0;
            const float p = 1.0f - (float) juce::jlimit(0.0, 1.0, applyMsLeft / 280.0);
            graph->setApplyProgress(p);
            if (applyMsLeft <= 0.0)
                finishApply();
        }

        if (spectrumPipeline != nullptr)
        {
            const size_t w = (size_t) juce::jmax(64, (int) graph->getPlotBounds().getWidth());
            if (spectrumPipeline->process(w))
                graph->setSpectrumPixels(spectrumPipeline->getPrePixelDB(),
                                         spectrumPipeline->getPostPixelDB());
        }

        float peakL = processor.getOutputPeakLeft();
        float peakR = processor.getOutputPeakRight();
        float dbL = peakL > 1e-10f ? juce::Decibels::gainToDecibels(peakL) : -100.0f;
        float dbR = peakR > 1e-10f ? juce::Decibels::gainToDecibels(peakR) : -100.0f;
        graph->setLevels(dbL, dbR);

        header->updateBadge();
        graph->repaint();
    }

    AIEqualizerAudioProcessor& processor;
    NewSpectrumPipeline* spectrumPipeline = nullptr;
    EmberLookAndFeel laf;
    std::unique_ptr<EmberHeader> header;
    std::unique_ptr<EmberGraph> graph;
    std::unique_ptr<EmberBar> bar;
    std::unique_ptr<EmberOverflow> overflow;

    EmberUiState state { EmberUiState::Riposo };
    bool phraseAlive = false;
    float barHeightAnim = 20.0f;
    float targetBarH = 20.0f;
    double applyMsLeft = 0.0;
    std::vector<EmberGhostBand> pendingGhosts;
    std::vector<float> matchRefDb;
};
