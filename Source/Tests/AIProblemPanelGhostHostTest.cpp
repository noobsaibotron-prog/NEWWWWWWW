#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../GUI/AIProblemPanel.h"

#include <array>
#include <cmath>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;

AIEngine::Correction makeHit()
{
    AIEngine::Correction c;
    c.type = AIEngine::ProblemType::Sibilance;
    c.frequency = 7000.0f;
    c.severity = 0.8f;
    c.confidence = 0.9f;
    c.suggestedGain = -4.0f;
    c.suggestedQ = 3.0f;
    return c;
}

void persistHits (AIEngine& engine, int frames, bool present)
{
    const auto hit = makeHit();
    for (int i = 0; i < frames; ++i)
        engine.persistRawDetectionsForTests (present ? std::vector<AIEngine::Correction> { hit }
                                                     : std::vector<AIEngine::Correction> {});
}

void refreshList (AIProblemPanel& panel)
{
    panel.refreshFromProcessor();
    panel.timerCallback();
}

juce::ListBox* problemList (AIProblemPanel& panel)
{
    return dynamic_cast<juce::ListBox*> (panel.findChildWithID ("aiProblemList"));
}

juce::Component* visibleRow (AIProblemPanel& panel, int row)
{
    if (auto* list = problemList (panel))
        return list->getComponentForRowNumber (row);
    return nullptr;
}

template <typename T>
T* rowChild (juce::Component* row, const char* id)
{
    return row == nullptr ? nullptr : dynamic_cast<T*> (row->findChildWithID (id));
}

juce::MouseEvent dummyMouse (int clicks)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (source, {}, juce::ModifierKeys {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             nullptr, nullptr, now, {}, now, clicks, false);
}

std::array<Processor::BandState, Processor::maxBands> snapshotBands (const Processor& proc)
{
    std::array<Processor::BandState, Processor::maxBands> out {};
    for (int i = 0; i < Processor::maxBands; ++i)
        out[static_cast<size_t> (i)] = proc.getBandState (i);
    return out;
}

bool bandsUnchanged (const Processor& proc,
                     const std::array<Processor::BandState, Processor::maxBands>& before)
{
    for (int i = 0; i < Processor::maxBands; ++i)
    {
        const auto now = proc.getBandState (i);
        const auto& was = before[static_cast<size_t> (i)];
        if (now.enabled != was.enabled
            || now.type != was.type
            || std::abs (now.frequency - was.frequency) > 1.0e-3f
            || std::abs (now.gain - was.gain) > 1.0e-3f
            || std::abs (now.q - was.q) > 1.0e-3f)
            return false;
    }
    return true;
}

struct HeadlessProblemPanel
{
    AIProblemPanel panel;

    explicit HeadlessProblemPanel (Processor& proc)
        : panel (proc)
    {
        panel.setSize (440, 420);
        panel.setVisible (true);
        panel.addToDesktop (juce::ComponentPeer::windowIsTemporary
                            | juce::ComponentPeer::windowIgnoresMouseClicks);
        panel.setTopLeftPosition (-4000, -4000);
    }

    ~HeadlessProblemPanel()
    {
        panel.setVisible (false);
        if (panel.isOnDesktop())
            panel.removeFromDesktop();
    }
};
}

class AIProblemPanelGhostHostTest final : public juce::UnitTest
{
public:
    AIProblemPanelGhostHostTest()
        : juce::UnitTest ("AI problem panel ghost host path", "Integration")
    {}

    void runTest() override
    {
        juce::MessageManager::getInstance();
        constexpr double kSr = 48000.0;
        constexpr int kBlock = 512;
        constexpr int kHistory = static_cast<int> (AIEngine::kLivePersistenceHistoryLen);

        beginTest ("live row shows APPLY and FIX ALL after persistence admits the hit");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            proc.getAIEngine().setEnabled (true);
            persistHits (proc.getAIEngine(), kHistory, true);
            expectEquals (static_cast<int> (proc.getAIEngine().getPendingCorrections().size()), 1);

            HeadlessProblemPanel host (proc);
            refreshList (host.panel);
            expectEquals (host.panel.getNumRows(), 1);

            auto* list = problemList (host.panel);
            expect (list != nullptr);
            if (list == nullptr)
                return;
            list->selectRow (0, true);

            auto* row = visibleRow (host.panel, 0);
            expect (row != nullptr, "ListBox must instantiate the live problem row");
            auto* apply = rowChild<juce::Button> (row, "problemRowApply");
            auto* listen = rowChild<juce::Button> (row, "problemRowListen");
            auto* dismiss = rowChild<juce::Button> (row, "problemRowDismiss");
            auto* details = rowChild<juce::Button> (row, "problemRowDetails");
            auto* fixAll = dynamic_cast<juce::Button*> (host.panel.findChildWithID ("problemFixAll"));
            expect (apply != nullptr && listen != nullptr && dismiss != nullptr && details != nullptr);
            expect (fixAll != nullptr);
            if (apply == nullptr || listen == nullptr || dismiss == nullptr || details == nullptr
                || fixAll == nullptr)
                return;

            expect (apply->isVisible() && apply->isEnabled());
            expect (listen->isVisible() && listen->isEnabled());
            expect (dismiss->isVisible() && dismiss->isEnabled());
            expect (details->isVisible() && details->isEnabled());
            expect (fixAll->isEnabled());
        }

        beginTest ("ghost row keeps PRE, SKIP, details and closes APPLY without moving EQ");
        {
            Processor proc;
            proc.prepareToPlay (kSr, kBlock);
            proc.getAIEngine().setEnabled (true);

            HeadlessProblemPanel host (proc);
            persistHits (proc.getAIEngine(), kHistory, true);
            refreshList (host.panel);
            expectEquals (host.panel.getNumRows(), 1);

            persistHits (proc.getAIEngine(), kHistory, false);
            expect (proc.getAIEngine().getPendingCorrections().empty());
            refreshList (host.panel);
            expectEquals (host.panel.getNumRows(), 1,
                          "exit-hold must keep the vanished problem as a ghost");

            auto* list = problemList (host.panel);
            expect (list != nullptr);
            if (list == nullptr)
                return;
            list->selectRow (0, true);

            auto* row = visibleRow (host.panel, 0);
            expect (row != nullptr, "ListBox must instantiate the ghost problem row");
            auto* apply = rowChild<juce::Button> (row, "problemRowApply");
            auto* listen = rowChild<juce::Button> (row, "problemRowListen");
            auto* dismiss = rowChild<juce::Button> (row, "problemRowDismiss");
            auto* details = rowChild<juce::Button> (row, "problemRowDetails");
            auto* fixAll = dynamic_cast<juce::Button*> (host.panel.findChildWithID ("problemFixAll"));
            auto* detailCard = host.panel.findChildWithID ("problemDetailCard");
            expect (apply != nullptr && listen != nullptr && dismiss != nullptr && details != nullptr);
            expect (fixAll != nullptr);
            if (apply == nullptr || listen == nullptr || dismiss == nullptr || details == nullptr
                || fixAll == nullptr)
                return;

            expect (apply->isVisible());
            expect (! apply->isEnabled(), "ghost APPLY must stay disabled");
            expect (listen->isEnabled());
            expect (dismiss->isEnabled());
            expect (details->isEnabled());
            expect (! fixAll->isEnabled(), "FIX ALL must not act on a ghost-only list");

            bool previewed = false;
            host.panel.onProblemSelected = [&previewed] (float, float, float, AIEngine::ProblemType)
            {
                previewed = true;
            };
            if (listen->onClick)
                listen->onClick();
            expect (previewed, "PRE must still highlight a ghost");

            if (details->onClick)
                details->onClick();
            expect (detailCard != nullptr && detailCard->isVisible(),
                    "details must still expand on a ghost");

            const auto beforeApply = snapshotBands (proc);
            if (apply->onClick)
                apply->onClick();
            expectEquals (host.panel.getNumRows(), 1,
                          "refused APPLY must not eat the visual hold");
            expect (bandsUnchanged (proc, beforeApply),
                    "refused APPLY must not move EQ bands");

            host.panel.listBoxItemDoubleClicked (0, dummyMouse (2));
            expectEquals (host.panel.getNumRows(), 1,
                          "ghost double-click must not eat the visual hold");
            expect (bandsUnchanged (proc, beforeApply),
                    "ghost double-click must not move EQ bands");

            if (dismiss->onClick)
                dismiss->onClick();
            expectEquals (host.panel.getNumRows(), 0,
                          "SKIP must remove the ghost");
        }
    }
};

static AIProblemPanelGhostHostTest aiProblemPanelGhostHostTest;

#endif
