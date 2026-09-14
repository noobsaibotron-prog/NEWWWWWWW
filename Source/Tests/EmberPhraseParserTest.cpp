#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../GUI/Ember/EmberPhraseParser.h"
#include "../GUI/Ember/EmberTokens.h"
#include "../GUI/Ember/EmberLookAndFeel.h"
#include "../GUI/Ember/EmberGraph.h"
#include "../GUI/Ember/EmberApplyCommit.h"

#include <cmath>
#include <vector>

namespace
{
using Processor = AIEqualizerAudioProcessor;

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

        beginTest("Keyword polarity is local to the matching clause");
        {
            auto expectGhost = [this](const std::vector<EmberGhostBand>& ghosts, float hz, float db)
            {
                for (const auto& g : ghosts)
                    if (std::abs(g.hz - hz) < 0.5f)
                    {
                        expectWithinAbsoluteError(g.db, db, 0.01f);
                        return;
                    }
                expect(false, "missing ghost @ " + juce::String(hz));
            };

            auto g = EmberPhrase::parse("more air, less harsh");
            expectEquals((int) g.size(), 2);
            expectGhost(g, 11200.0f,  2.4f);
            expectGhost(g,  6500.0f, -2.8f);

            g = EmberPhrase::parse("cut the mud, add air");
            expectEquals((int) g.size(), 2);
            expectGhost(g,   280.0f, -2.2f);
            expectGhost(g, 11200.0f,  2.4f);

            g = EmberPhrase::parse("less warm and more punch");
            expectEquals((int) g.size(), 2);
            expectGhost(g, 180.0f, -1.8f);
            expectGhost(g,  95.0f,  1.7f);

            g = EmberPhrase::parse("more clarity and less boxy");
            expectEquals((int) g.size(), 2);
            expectGhost(g, 3200.0f,  1.4f);
            expectGhost(g,  280.0f, -2.2f);
        }

        beginTest("Keyword forms match as whole words from an explicit vocabulary");
        {
            struct Case { const char* phrase; float hz; };
            const Case cases[] = {
                { "muddy",        280.0f }, { "punchy",       95.0f }, { "brighter",  11200.0f },
                { "warmth",       180.0f }, { "harshness",  6500.0f }, { "airy",      11200.0f },
                { "clearer",     3200.0f }, { "sibilant",   6500.0f }, { "piercing",   6500.0f },
                { "sparkly",    11200.0f }, { "thumpy",       95.0f }, { "airiest",   11200.0f },
                { "warming",      180.0f }, { "roundness",   180.0f }, { "punches",      95.0f },
                { "brightness", 11200.0f }, { "muddiness",   280.0f }, { "sibilance",  6500.0f },
            };
            for (const auto& c : cases)
            {
                const auto g = EmberPhrase::parse(c.phrase);
                expectEquals((int) g.size(), 1, c.phrase);
                expectWithinAbsoluteError(g.empty() ? -1.0f : g[0].hz, c.hz, 0.5f, c.phrase);
            }
        }

        beginTest("A key hidden inside another word does not fire");
        {
            // Substring collisions, and word starts a root (mud*, bright*, pierc*, sparkl*, thump*) would catch.
            const char* none[] = { "hair", "chair", "airport", "sandbox", "boxing", "background",
                                   "around", "opening", "somebody", "warmup", "punchline",
                                   "mudguard", "brighton", "pierce", "sparkler", "thumper", "messy" };
            for (auto* phrase : none)
                expectEquals((int) EmberPhrase::parse(phrase).size(), 0, phrase);

            auto g = EmberPhrase::parse("background vocal clarity");
            expectEquals((int) g.size(), 1, "background vocal clarity");
            expectWithinAbsoluteError(g.empty() ? -1.0f : g[0].hz, 3200.0f, 0.5f, "background vocal clarity");

            g = EmberPhrase::parse("clearly too harsh");
            expectEquals((int) g.size(), 1, "clearly too harsh");
            expectWithinAbsoluteError(g.empty() ? -1.0f : g[0].hz, 6500.0f, 0.5f, "clearly too harsh");
        }

        beginTest("Punctuation around a word is a word boundary");
        {
            auto expectGhost = [this](const std::vector<EmberGhostBand>& ghosts, float hz, float db)
            {
                for (const auto& g : ghosts)
                    if (std::abs(g.hz - hz) < 0.5f)
                    {
                        expectWithinAbsoluteError(g.db, db, 0.01f);
                        return;
                    }
                expect(false, "missing ghost @ " + juce::String(hz));
            };

            struct Case { juce::String phrase; float hz; };
            const Case cases[] = {
                { "more (air)",   11200.0f },
                { "more \"air\"", 11200.0f },
                { juce::String(juce::CharPointer_UTF8("more \xe2\x80\x9c" "air\xe2\x80\x9d")), 11200.0f },
                { "[muddy]",        280.0f },
                { "the vocal's harshness", 6500.0f },
                { "full-bodied",    180.0f },
            };
            for (const auto& c : cases)
            {
                const auto g = EmberPhrase::parse(c.phrase);
                expectEquals((int) g.size(), 1, c.phrase);
                expectWithinAbsoluteError(g.empty() ? -1.0f : g[0].hz, c.hz, 0.5f, c.phrase);
            }

            // Polarity words use the same boundaries.
            auto g = EmberPhrase::parse("(less) air");
            expectEquals((int) g.size(), 1, "(less) air");
            expectWithinAbsoluteError(g.empty() ? 0.0f : g[0].db, -2.4f, 0.01f, "(less) air");

            g = EmberPhrase::parse("more (air), less \"harsh\"");
            expectEquals((int) g.size(), 2, "more (air), less \"harsh\"");
            expectGhost(g, 11200.0f,  2.4f);
            expectGhost(g,  6500.0f, -2.8f);

            g = EmberPhrase::parse("(more) harsh");
            expectEquals((int) g.size(), 1, "(more) harsh");
            expectWithinAbsoluteError(g.empty() ? 0.0f : g[0].db, 2.8f, 0.01f, "(more) harsh");
        }

        beginTest("An apostrophe between letters stays in the word");
        {
            // Decision (Marco, 2026-09-14): ASCII ' or U+2019 between two word characters is part of the word;
            // at a word edge it is a boundary. With this vocabulary the visible effect is that a possessive
            // such as "air's" is not the key "air".
            expectEquals((int) EmberPhrase::parse("the air's too thin").size(), 0, "the air's too thin");
            const juce::String typographic(juce::CharPointer_UTF8("the air\xe2\x80\x99" "s too thin"));
            expectEquals((int) EmberPhrase::parse(typographic).size(), 0, typographic);

            struct Case { juce::String phrase; float hz; };
            const Case cases[] = {
                { "'air'", 11200.0f },
                { juce::String(juce::CharPointer_UTF8("\xe2\x80\x98" "air\xe2\x80\x99")), 11200.0f },
                { juce::String(juce::CharPointer_UTF8("the vocal\xe2\x80\x99" "s harshness")), 6500.0f },
            };
            for (const auto& c : cases)
            {
                const auto g = EmberPhrase::parse(c.phrase);
                expectEquals((int) g.size(), 1, c.phrase);
                expectWithinAbsoluteError(g.empty() ? -1.0f : g[0].hz, c.hz, 0.5f, c.phrase);
            }
        }

        beginTest("Absolute numeric parse keeps its existing signed-dB semantics");
        {
            auto g = EmberPhrase::parse("add a peak at 2khz with -4db gain and 2.5 q.");
            expectEquals((int) g.size(), 1);
            if (! g.empty())
            {
                expectWithinAbsoluteError(g[0].hz, 2000.0f, 0.5f);
                expectWithinAbsoluteError(g[0].db, -4.0f, 0.01f);
                expectWithinAbsoluteError(g[0].q, 2.50f, 0.01f);
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

            // The function EmberV2Shell::finishApply calls — not a copy of it.
            const auto result = EmberApply::commitGhosts(proc, pending);

            expect(result.committed);
            expectEquals(EmberApply::readLiveCount(proc), 5, "Apply adds one band to four");
            const int slot = result.slots.empty() ? -1 : result.slots[0];
            expectEquals(slot, 4, "the ghost lands past the four live bands");
            auto stNew = proc.getBandState(juce::jmax(0, slot));
            expect(stNew.enabled);
            expectEquals(stNew.type, 2);
            expectWithinAbsoluteError(stNew.frequency, 2000.0f, 0.5f);
            expectWithinAbsoluteError(stNew.gain, -4.0f, 0.05f);
            expectWithinAbsoluteError(stNew.q, 2.50f, 0.02f);

            // Additive (Marco, 2026-09-11): the dirty EQ is still there.
            const float dirtyGain[4] = { 3.0f, -2.5f, 2.0f, -1.5f };
            for (int i = 0; i < 4; ++i)
            {
                auto st = proc.getBandState(i);
                expect(st.enabled, "dirty band " + juce::String(i) + " still on");
                expectWithinAbsoluteError(st.gain, dirtyGain[i], 0.05f);
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

            const auto result = EmberApply::commitGhosts(proc, pending);
            expect(result.committed && ! result.slots.empty());
            auto stNew = proc.getBandState(result.slots.empty() ? 0 : result.slots[0]);
            expectWithinAbsoluteError(stNew.gain, -2.0f, 0.05f);
            expectWithinAbsoluteError(stNew.frequency, 2000.0f, 0.5f);
            expectWithinAbsoluteError(stNew.q, 2.50f, 0.02f);
        }
    }
};

static EmberPhraseParserTest emberPhraseParserTest;

#endif
