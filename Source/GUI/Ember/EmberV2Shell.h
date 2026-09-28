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
#include "EmberCopilotPlanAdapter.h"
#include "EmberApplyCommit.h"
#include "EmberVisualState.h"

/** Top-level Ember Core UI v2 host (SPECCHIO). */
class EmberV2Shell : public juce::Component,
                     private juce::Timer
{
public:
    std::function<void(bool)> onCopilotLinkChanged;
    std::function<void()> onCopilotPair;
    std::function<void()> onCopilotProposalRejected;
    std::function<bool(const std::string&)> onCopilotProposalApply;

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

        overflow->onClose = [this] { resized(); };
        overflow->onCopilotLinkChanged = [this](bool enabled)
        {
            if (onCopilotLinkChanged) onCopilotLinkChanged(enabled);
        };
        overflow->onCopilotPair = [this]
        {
            if (onCopilotPair) onCopilotPair();
        };

        graph->onBandSelected = [this](int idx)
        {
            if (idx >= 0)
                setState(EmberUiState::Nodo);
            else if (state == EmberUiState::Nodo)
                setState(phraseAlive ? EmberUiState::Frase : EmberUiState::Riposo);
        };
        graph->onBandEdited = [this]
        {
            if (copilotProposalActive)
                rejectCopilotProposal();
        };

        bar->onEnterFrase = [this]
        {
            phraseAlive = true;
            setState(EmberUiState::Frase);
            bar->focusPhrase();
        };
        bar->onCancelFrase = [this]
        {
            if (copilotProposalActive)
            {
                rejectCopilotProposal();
                return;
            }
            phraseAlive = false;
            bar->clearPhrase();
            graph->setGhosts({}, 1.0f);
            setState(graph->getSelectedBand() >= 0 ? EmberUiState::Nodo : EmberUiState::Riposo);
        };
        bar->onPhraseChanged = [this](const juce::String& text)
        {
            // Parse only while typing updates amber ghosts — Apply commits this same vector.
            graph->setGhosts(EmberPhrase::parse(text), bar->getIntensity());
        };
        bar->onIntensity = [this](float intens)
        {
            graph->setGhosts(EmberPhrase::parse(bar->getPhrase()), intens);
        };
        bar->onApply = [this]{ beginApply(); };
        bar->onRejectCopilot = [this]{ rejectCopilotProposal(); };
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
        visualCurrent = visualTarget; // open settled, no fade-in
        visualSettling = false;
        pushVisualProfile();
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

    void setCopilotUi(const EmberProposal::LinkUiState& ui)
    {
        header->setCopilotUi(ui);
        overflow->setCopilotUi(ui);

        if (! ui.linkEnabled && copilotProposalActive)
            clearCopilotProposalUi();
    }

    void beginCopilotProposal(const juce::String& phrase)
    {
        copilotProposalActive = true;
        copilotProposalReady = false;
        copilotPlanHash.clear();
        phraseAlive = true;
        graph->selectBand(-1);
        graph->setGhosts({}, 1.0f);
        bar->setCopilotProposal(phrase, {}, false);
        setState(EmberUiState::Frase);
    }

    [[nodiscard]] bool stageCopilotPlan(
        const juce::String& phrase,
        const AIEQPerceptual::SemanticPlan& plan,
        const std::string& planHash)
    {
        auto ghosts = EmberCopilot::ghostsFromPlan(plan);
        if (ghosts.empty())
        {
            clearCopilotProposalUi();
            return false;
        }

        copilotProposalActive = true;
        copilotProposalReady = true;
        copilotPlanHash = planHash;
        phraseAlive = true;
        graph->selectBand(-1);
        graph->setGhosts(ghosts, 1.0f);
        bar->setCopilotProposal(phrase, std::move(ghosts), true);
        setState(EmberUiState::Frase);
        return true;
    }

    void failCopilotProposal()
    {
        clearCopilotProposalUi();
    }

    [[nodiscard]] bool hasCopilotProposal() const noexcept
    {
        return copilotProposalActive;
    }

#if JUCE_UNIT_TESTS
    void completeApplyForTests()
    {
        if (state == EmberUiState::Apply)
            finishApply();
    }
#endif

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
            if (copilotProposalActive)
            {
                rejectCopilotProposal();
                return true;
            }
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
        visualTarget = emberVisualProfileFor(s);
        visualSettling = true;
        targetBarH = (s == EmberUiState::Riposo || (s == EmberUiState::Nodo && ! phraseAlive))
                     ? (float) EmberTokens::barRiposoH
                     : (float) EmberTokens::barActiveH;
    }

    void pushVisualProfile()
    {
        graph->setVisualProfile(visualCurrent);
        bar->setVisualPresence(visualCurrent.bar);
    }

    void beginApply()
    {
        // Commit exactly the on-screen amber ghosts (intensity already applied).
        // Do not re-parse here — that can diverge from the displayed ghost vector.
        pendingGhosts = graph->getEffectiveGhosts();
        for (auto& g : pendingGhosts)
            EmberApply::quantizeGhostToApvts(g);
        if (pendingGhosts.empty())
            return; // gate Apply when no ghosts

        applyingCopilotProposal = copilotProposalActive && copilotProposalReady;
        applyingCopilotPlanHash = applyingCopilotProposal ? copilotPlanHash : std::string {};

        setState(EmberUiState::Apply);
        applyMsLeft = EmberTokens::motionApplyMs;
        // Keep display locked to the pending (post-quant) vector during the 280ms morph.
        graph->setGhosts(pendingGhosts, 1.0f);
        graph->setApplyProgress(0.0f);
    }

    void finishApply()
    {
        if (applyingCopilotProposal)
        {
            // External plans return to the canonical Semantic APPLY state machine.
            // This preserves its all-or-nothing slot policy, undo snapshot and
            // provenance instead of treating Copilot as a local additive phrase.
            const bool committed = onCopilotProposalApply
                && onCopilotProposalApply(applyingCopilotPlanHash);
            applyingCopilotProposal = false;
            applyingCopilotPlanHash.clear();

            if (! committed)
            {
                pendingGhosts.clear();
                graph->setApplyProgress(1.0f);
                setState(EmberUiState::Frase);
                return;
            }
        }
        else
        {
            // Local phrases keep their established additive slot policy.
            // EMBER_APPLY_COUNT_DEFER_MS (Live undo experiment) writes the
            // count as a separate host step; unset keeps the immediate write.
            const int countDeferMs = EmberApply::countDeferMsFromEnvironment();
            const auto result = EmberApply::commitGhosts(processor, pendingGhosts,
                countDeferMs >= 0 ? EmberApply::CountWrite::Deferred
                                  : EmberApply::CountWrite::Immediate);
            if (result.countPending)
                EmberApply::scheduleLiveCountFlush(processor, countDeferMs);

            if (! pendingGhosts.empty() && ! result.committed)
            {
                // Nothing was written (no free band for every ghost). Keep the phrase
                // and its amber ghosts on screen rather than pretending it applied.
                pendingGhosts.clear();
                graph->setApplyProgress(1.0f);
                setState(EmberUiState::Frase);
                return;
            }
        }

        pendingGhosts.clear();
        applyingCopilotProposal = false;
        applyingCopilotPlanHash.clear();
        copilotProposalActive = false;
        copilotProposalReady = false;
        copilotPlanHash.clear();
        phraseAlive = false;
        if (bar->isCopilotProposal())
            bar->clearCopilotProposal();
        else
            bar->clearPhrase();
        graph->setGhosts({}, 1.0f);
        graph->setApplyProgress(1.0f);
        graph->selectBand(-1);
        setState(EmberUiState::Riposo);
    }

    void rejectCopilotProposal()
    {
        if (! copilotProposalActive || state == EmberUiState::Apply)
            return;

        if (onCopilotProposalRejected)
            onCopilotProposalRejected();
        clearCopilotProposalUi();
    }

    void clearCopilotProposalUi()
    {
        copilotProposalActive = false;
        copilotProposalReady = false;
        copilotPlanHash.clear();
        applyingCopilotProposal = false;
        applyingCopilotPlanHash.clear();
        pendingGhosts.clear();
        phraseAlive = false;
        bar->clearCopilotProposal();
        graph->setGhosts({}, 1.0f);
        graph->setApplyProgress(1.0f);
        setState(graph->getSelectedBand() >= 0 ? EmberUiState::Nodo
                                               : EmberUiState::Riposo);
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
        const float alpha = 1.0f - std::exp(-1.0f / (EmberTokens::motionBarEaseSec * 45.0f));
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

        // visual hierarchy follows the state with one time constant, on this same timer
        if (visualSettling)
        {
            const float a = 1.0f - std::exp(-(1.0f / 45.0f) / EmberTokens::visualProfileTauSec);
            if (emberApproachProfile(visualCurrent, visualTarget, a) < 0.001f)
            {
                visualCurrent = visualTarget;
                visualSettling = false;
            }
            pushVisualProfile();
        }

        if (state == EmberUiState::Apply)
        {
            applyMsLeft -= 1000.0 / 45.0;
            const float p = 1.0f - (float) juce::jlimit(0.0, 1.0, applyMsLeft / EmberTokens::motionApplyMs);
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
    EmberVisualProfile visualCurrent, visualTarget;
    bool visualSettling = false;
    std::vector<EmberGhostBand> pendingGhosts;
    std::vector<float> matchRefDb;
    bool copilotProposalActive = false;
    bool copilotProposalReady = false;
    bool applyingCopilotProposal = false;
    std::string copilotPlanHash;
    std::string applyingCopilotPlanHash;
};
