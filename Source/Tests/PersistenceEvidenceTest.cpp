#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/AIEngine.h"
#include "../AI/PersistenceEvidence.h"

class PersistenceEvidenceTest final : public juce::UnitTest
{
public:
    PersistenceEvidenceTest()
        : juce::UnitTest ("Live persistence evidence vs confidence", "AI-Diag")
    {}

    void runTest() override
    {
        using EmberUI::PersistenceSource;

        beginTest ("compact glyph is percent-only until live stability is ready");
        {
            EmberUI::PersistenceEvidence none;
            expect (! none.showsLiveStability());
            expectEquals (juce::String (EmberUI::formatCompactConfidence (91, none)),
                          juce::String ("91%"));

            EmberUI::PersistenceEvidence capture;
            capture.source = PersistenceSource::Capture;
            capture.hits = 8;
            capture.windowSize = 8;
            capture.historyReady = true;
            expect (! capture.showsLiveStability(),
                    "capture must not display a fabricated n/8 even if counts are filled");
            expectEquals (juce::String (EmberUI::formatCompactConfidence (86, capture)),
                          juce::String ("86%"));

            EmberUI::PersistenceEvidence live;
            live.source = PersistenceSource::Live;
            live.hits = 7;
            live.windowSize = 8;
            live.historyReady = true;
            expect (live.showsLiveStability());
            expectEquals (juce::String (EmberUI::formatCompactConfidence (91, live)),
                          juce::String ("91%  7/8"));
        }

        beginTest ("1/1 does not surface before the 8-frame window is full");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            const auto hit = makeHit();
            for (int n = 1; n < static_cast<int> (AIEngine::kLivePersistenceHistoryLen); ++n)
            {
                engine.persistRawDetectionsForTests ({ hit });
                const auto snap = engine.getPendingListSnapshot();
                expect (snap.corrections.empty(),
                        "n=" + juce::String (n) + " must stay empty during warmup");
                expect (snap.evidence.empty(),
                        "warmup must not publish fabricated stability");
            }
        }

        beginTest ("threshold crossing at 5/8");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            const auto hit = makeHit();

            // 4 hits in an 8-frame window = 0.50 < 0.6 → drop
            for (int i = 0; i < 4; ++i)
                engine.persistRawDetectionsForTests ({});
            for (int i = 0; i < 4; ++i)
                engine.persistRawDetectionsForTests ({ hit });
            expect (engine.getPendingCorrections().empty(),
                    "4/8 must stay below the persistence fraction");

            // Slide one miss out, one extra hit in → 5/8
            engine.persistRawDetectionsForTests ({ hit });
            const auto snap = engine.getPendingListSnapshot();
            expectEquals ((int) snap.corrections.size(), 1);
            expectEquals ((int) snap.evidence.size(), 1);
            if (! snap.evidence.empty())
            {
                expect (snap.evidence.front().showsLiveStability());
                expectEquals (snap.evidence.front().hits, 5);
                expectEquals (snap.evidence.front().windowSize,
                              (int) AIEngine::kLivePersistenceHistoryLen);
                expect (snap.evidence.front().source == PersistenceSource::Live);
                expectEquals (juce::String (EmberUI::formatCompactConfidence (91, snap.evidence.front())),
                              juce::String ("91%  5/8"));
            }
        }

        beginTest ("eviction from the 8-frame ring drops a vanished problem");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            const auto hit = makeHit();
            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({ hit });
            expect (! engine.getPendingCorrections().empty());

            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({});
            const auto snap = engine.getPendingListSnapshot();
            expect (snap.corrections.empty(), "full miss window must evict");
            expect (snap.evidence.empty());
        }

        beginTest ("live metadata corresponds to the gated list");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            const auto hit = makeHit();
            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({ hit });
            const auto snap = engine.getPendingListSnapshot();
            expectEquals ((int) snap.corrections.size(), (int) snap.evidence.size());
            if (! snap.evidence.empty())
            {
                expectEquals (snap.evidence.front().hits, 8);
                expect (snap.evidence.front().historyReady);
                expectWithinAbsoluteError (snap.corrections.front().confidence, 0.9f, 1.0e-5f);
                expectEquals (juce::String (EmberUI::formatCompactConfidence (90, snap.evidence.front())),
                              juce::String ("90%  8/8"));
            }
        }

        beginTest ("capture bypass never fabricates an 8-frame count");
        {
            AIEngine engine;
            engine.prepare (44100.0, 512);
            engine.setEnabled (true);
            engine.setSensitivity (1.0f);
            engine.setDetectionBackendMode (AIEngine::DetectionBackendMode::HeuristicOnly);
            engine.forceMLDetectionEnabledForTests (false);
            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({ makeHit() });
            expect (! engine.getPendingCorrections().empty());
            expect (engine.getPendingListSnapshot().evidence.front().showsLiveStability());

            constexpr int kNumBins = 2049;
            const int resonanceBin = juce::roundToInt (1000.0f * 4096.0f / 44100.0f);
            auto peak = std::vector<float> (kNumBins, -60.0f);
            if (resonanceBin >= 0 && resonanceBin < (int) peak.size())
                peak[(size_t) juce::jlimit (0, (int) peak.size() - 1, resonanceBin)] = -10.0f;
            engine.analyzeSpectrum (peak, /*force=*/true);

            const auto snap = engine.getPendingListSnapshot();
            expectEquals ((int) snap.corrections.size(), (int) snap.evidence.size());
            for (const auto& ev : snap.evidence)
            {
                expect (ev.source == PersistenceSource::Capture);
                expect (! ev.showsLiveStability());
                expectEquals (ev.hits, 0);
                expectEquals (ev.windowSize, 0);
                expectEquals (juce::String (EmberUI::formatCompactConfidence (86, ev)),
                              juce::String ("86%"));
            }
        }

        beginTest ("reset clears published evidence");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({ makeHit() });
            engine.resetLiveDetectionState();
            const auto snap = engine.getPendingListSnapshot();
            expect (snap.corrections.empty());
            expect (snap.evidence.empty());
        }

        beginTest ("snapshot sizes match under a GUI-style copy");
        {
            AIEngine engine;
            engine.prepare (48000.0, 512);
            engine.setEnabled (true);
            for (int i = 0; i < 8; ++i)
                engine.persistRawDetectionsForTests ({ makeHit() });
            const auto a = engine.getPendingListSnapshot();
            const auto b = engine.getPendingCorrections();
            expectEquals ((int) a.corrections.size(), (int) b.size());
            expectEquals ((int) a.corrections.size(), (int) a.evidence.size());
        }
    }

private:
    static AIEngine::Correction makeHit()
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

};

static PersistenceEvidenceTest persistenceEvidenceTest;

#endif
