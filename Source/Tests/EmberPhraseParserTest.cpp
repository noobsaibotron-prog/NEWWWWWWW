#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberPhraseParser.h"
#include "../GUI/Ember/EmberTokens.h"
#include "../GUI/Ember/EmberLookAndFeel.h"
#include "../GUI/Ember/EmberGraph.h"

#include <cmath>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;

/** Mirror EmberV2Shell::quantizeGhostToApvts — same APVTS intervals. */
void quantizeGhostToApvts(EmberGhostBand& g) noexcept
{
    g.hz = std::round(juce::jlimit(20.0f, 20000.0f, g.hz));
    g.db = std::round(juce::jlimit(-24.0f, 24.0f, g.db) * 10.0f) * 0.1f;
    g.q  = std::round(juce::jlimit(0.1f, 10.0f, g.q) * 100.0f) * 0.01f;
    g.type = juce::jlimit(0, 8, g.type);
}

/** Mirror EmberV2Shell::finishApply replace commit (T1). */
void commitGhostsReplaceStyle(Processor& processor, std::vector<EmberGhostBand> ghosts)
{
    for (auto& g : ghosts)
        quantizeGhostToApvts(g);

    const int nGhosts = (int) ghosts.size();
    if (nGhosts <= 0)
        return;

    const int n = juce::jlimit(1, Processor::maxBands, nGhosts);
    const int prevActive = processor.getNumActiveBands();

    if (auto* param = processor.getAPVTS().getParameter("numActiveBands"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(n - 1)));
        param->endChangeGesture();
    }
    processor.setNumActiveBands(n);

    for (int i = 0; i < n; ++i)
    {
        const auto& g = ghosts[(size_t) i];
        processor.setBandGeometry(i, g.hz, g.db, g.q, g.type, true);
    }

    for (int i = n; i < Processor::maxBands && i < prevActive; ++i)
    {
        auto st = processor.getBandState(i);
        if (st.enabled || std::abs(st.gain) > 0.01f)
            processor.setBandGeometry(i, st.frequency, 0.0f, st.q, st.type, false);
    }
}

void seedDirtyEq(juce::UnitTest& t, Processor& proc)
{
    // Ableton DoD setup: dirty multi-band F before phrase Apply.
    if (auto* param = proc.getAPVTS().getParameter("numActiveBands"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(3.0f)); // 4 bands
        param->endChangeGesture();
    }
    proc.setNumActiveBands(4);
    proc.setBandGeometry(0, 120.0f,  3.0f, 0.8f, 1, true);   // LS boost
    proc.setBandGeometry(1, 450.0f, -2.5f, 1.2f, 2, true);   // low-mid cut
    proc.setBandGeometry(2, 3500.0f, 2.0f, 1.0f, 2, true);   // high-mid boost
    proc.setBandGeometry(3, 9000.0f, -1.5f, 0.9f, 3, true);  // HS cut
    t.expectEquals(proc.getNumActiveBands(), 4);
}
} // namespace

class EmberPhraseParserTest : public juce::UnitTest
{
public:
    EmberPhraseParserTest()
        : juce::UnitTest("Ember Phrase Parser absolute+keyword", "Integration") {}

    void runTest() override
    {
        juce::MessageManager::getInstance();

        beginTest("Absolute numeric peak phrase → Peak 2000 / -4 / 2.50");
        {
            auto g = EmberPhrase::parse("add a peak at 2khz with -4db gain and 2.5 q");
            expectEquals((int) g.size(), 1);
            if (! g.empty())
            {
                expectEquals(g[0].type, 2);
                expectWithinAbsoluteError(g[0].hz, 2000.0f, 0.5f);
                expectWithinAbsoluteError(g[0].db, -4.0f, 0.01f);
                expectWithinAbsoluteError(g[0].q, 2.50f, 0.01f);
            }
        }

        beginTest("Keyword warm still works (no absolute units)");
        {
            auto g = EmberPhrase::parse("warmer body");
            expect((int) g.size() >= 1);
            if (! g.empty())
            {
                expectEquals(g[0].type, 1);
                expectWithinAbsoluteError(g[0].hz, 180.0f, 0.5f);
            }
        }

        beginTest("Absolute wins over keyword stems in same phrase");
        {
            auto g = EmberPhrase::parse("peak 5khz +1.5db q1.2");
            expectEquals((int) g.size(), 1);
            if (! g.empty())
            {
                expectWithinAbsoluteError(g[0].hz, 5000.0f, 0.5f);
                expectWithinAbsoluteError(g[0].db, 1.5f, 0.01f);
                expectWithinAbsoluteError(g[0].q, 1.2f, 0.01f);
            }
        }

        beginTest("T1-2 harness: exact phrase → setGhosts → getEffectiveGhosts → Apply → bands");
        {
            constexpr float kIntensity = 1.0f;
            const juce::String phrase = "add a peak at 2khz with -4db gain and 2.5 q";

            Processor proc;
            proc.prepareToPlay(48000.0, 512);
            seedDirtyEq(*this, proc);

            EmberLookAndFeel laf;
            EmberGraph graph(proc, laf);

            // Same path as EmberV2Shell::onPhraseChanged (display I).
            auto parsed = EmberPhrase::parse(phrase);
            expectEquals((int) parsed.size(), 1, "numeric parse must feed display ghosts");
            graph.setGhosts(parsed, kIntensity);

            // Locked commit source: on-screen effective ghosts (not a second parse).
            auto pending = graph.getEffectiveGhosts();
            expectEquals((int) pending.size(), 1);
            expectWithinAbsoluteError(pending[0].hz, 2000.0f, 0.5f);
            expectWithinAbsoluteError(pending[0].db, -4.0f * kIntensity, 0.01f);
            expectWithinAbsoluteError(pending[0].q, 2.50f, 0.01f);
            expectEquals(pending[0].type, 2);

            // Display vector == pending vector (no divergent parse at click).
            expectWithinAbsoluteError(pending[0].hz, parsed[0].hz, 0.01f);
            expectWithinAbsoluteError(pending[0].db, parsed[0].db * kIntensity, 0.01f);
            expectWithinAbsoluteError(pending[0].q, parsed[0].q, 0.01f);

            commitGhostsReplaceStyle(proc, pending);

            expectEquals(proc.getNumActiveBands(), 1);
            auto st0 = proc.getBandState(0);
            expect(st0.enabled);
            expectEquals(st0.type, 2);
            expectWithinAbsoluteError(st0.frequency, 2000.0f, 0.5f);
            expectWithinAbsoluteError(st0.gain, -4.0f, 0.05f);
            expectWithinAbsoluteError(st0.q, 2.50f, 0.02f);

            // Leftover dirty slots disabled / zeroed gain (T1 replace).
            for (int i = 1; i < 4; ++i)
            {
                auto st = proc.getBandState(i);
                expect(! st.enabled || std::abs(st.gain) < 0.05f);
            }
        }

        beginTest("T1-2 harness: intensity scales commit dB from same ghost vector");
        {
            constexpr float kIntensity = 0.5f;
            const juce::String phrase = "add a peak at 2khz with -4db gain and 2.5 q";

            Processor proc;
            proc.prepareToPlay(48000.0, 512);

            EmberLookAndFeel laf;
            EmberGraph graph(proc, laf);
            graph.setGhosts(EmberPhrase::parse(phrase), kIntensity);
            auto pending = graph.getEffectiveGhosts();
            expectEquals((int) pending.size(), 1);
            expectWithinAbsoluteError(pending[0].db, -2.0f, 0.01f);

            commitGhostsReplaceStyle(proc, pending);
            auto st0 = proc.getBandState(0);
            expectWithinAbsoluteError(st0.gain, -2.0f, 0.05f);
            expectWithinAbsoluteError(st0.frequency, 2000.0f, 0.5f);
            expectWithinAbsoluteError(st0.q, 2.50f, 0.02f);
        }
    }
};

static EmberPhraseParserTest emberPhraseParserTest;

#endif
