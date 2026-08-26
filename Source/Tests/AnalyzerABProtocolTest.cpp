#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Core/LockFreeAudioFIFO.h"
#include "../GUI/NewSpectrumPipeline.h"
#include "../GUI/SpectrumHopLerp.h"

#include <cmath>
#include <vector>

/**
 * Analyzer A/B protocol counter-check. GUI / pipeline only — no EQ DSP.
 *
 * These facts are shared by A1 and A2. They must not be "fixed" inside the
 * presentation A/B: a hop-rate or overlap change would confound the comparison.
 *
 * Declared hop advance is fftSize/4. At High (4096) that is 46.875 hop/s @ 48 kHz
 * and 93.75 hop/s @ 96 kHz. The editor drains the pipeline at 60 Hz and injects
 * only the last hop of that tick, so visual updates cannot exceed 60 Hz.
 */
class AnalyzerABProtocolTest final : public juce::UnitTest
{
public:
    AnalyzerABProtocolTest()
        : juce::UnitTest ("Analyzer A/B protocol", "Integration") {}

    void runTest() override
    {
        testHopAdvanceAt48And96kHz();
        testVisualInjectIsCappedByEditorTimer();
        testLerpIntervalClampVsTrueHopInterval();
        testAssembledFrameDoesNotRetainThreeQuarters();
        testSmoothingAdvancesPerDrainedHop();
        testA2ShowsPreviousHopAtT0();
    }

private:
    static constexpr double kEps = 1.0e-9;
    static constexpr double kEditorTimerHz = 60.0;
    static constexpr int kMaxHopsPerTick = 16;

    struct Case
    {
        double sampleRate = 0.0;
        size_t fftOrder = 0;
        const char* label = "";
    };

    static size_t fftSizeOf (size_t fftOrder) noexcept { return size_t { 1 } << fftOrder; }
    static size_t hopSizeOf (size_t fftSize) noexcept { return fftSize / 4; }

    static double hopHz (double sampleRate, size_t fftSize) noexcept
    {
        return sampleRate / static_cast<double> (hopSizeOf (fftSize));
    }

    static double hopIntervalMs (double sampleRate, size_t fftSize) noexcept
    {
        return 1000.0 * static_cast<double> (hopSizeOf (fftSize)) / sampleRate;
    }

    static void pushSilence (LockFreeAudioFIFO<float>& fifo, int numSamples)
    {
        juce::AudioBuffer<float> buf (1, numSamples);
        buf.clear();
        fifo.pushStereoMix (buf);
    }

    void testHopAdvanceAt48And96kHz()
    {
        beginTest ("Hop advance is fftSize/4 at 48 kHz and 96 kHz");

        const Case cases[] = {
            { 48000.0, 12, "48 kHz / 4096" },
            { 96000.0, 12, "96 kHz / 4096" },
            { 48000.0, 13, "48 kHz / 8192" },
            { 96000.0, 13, "96 kHz / 8192" },
        };

        for (const auto& c : cases)
        {
            const size_t fftSize = fftSizeOf (c.fftOrder);
            const size_t hop = hopSizeOf (fftSize);
            const int hopsToPush = 8;

            LockFreeAudioFIFO<float> preFIFO, postFIFO;
            preFIFO.prepare (65536);
            postFIFO.prepare (65536);

            NewSpectrumPipeline pipeline (preFIFO, postFIFO, c.fftOrder, c.sampleRate);
            pushSilence (preFIFO, static_cast<int> (hop * static_cast<size_t> (hopsToPush)));
            pushSilence (postFIFO, static_cast<int> (hop * static_cast<size_t> (hopsToPush)));
            pipeline.process (512);

            const auto stats = pipeline.getStats();
            expectEquals ((int) stats.pre.hopsCompleted, hopsToPush,
                          juce::String (c.label) + " hop count");
            logMessage (juce::String (c.label) + ": hop=" + juce::String ((int) hop)
                        + "  hop/s=" + juce::String (hopHz (c.sampleRate, fftSize), 3)
                        + "  interval=" + juce::String (hopIntervalMs (c.sampleRate, fftSize), 3)
                        + " ms");
        }

        expectWithinAbsoluteError (hopHz (48000.0, 4096), 46.875, kEps, "48k/4096 hop/s");
        expectWithinAbsoluteError (hopHz (96000.0, 4096), 93.75, kEps, "96k/4096 hop/s");
        expectWithinAbsoluteError (hopHz (48000.0, 8192), 23.4375, kEps, "48k/8192 hop/s");
        expectWithinAbsoluteError (hopHz (96000.0, 8192), 46.875, kEps, "96k/8192 hop/s");
    }

    void testVisualInjectIsCappedByEditorTimer()
    {
        beginTest ("Editor injects at most 60 Hz; 96 kHz/4096 hops are collapsed");

        struct Row
        {
            double sampleRate;
            size_t fftSize;
            bool hopsFasterThanEditor;
            const char* label;
        };

        const Row rows[] = {
            { 48000.0, 4096, false, "48 kHz / 4096" },
            { 96000.0, 4096, true,  "96 kHz / 4096" },
            { 48000.0, 8192, false, "48 kHz / 8192" },
            { 96000.0, 8192, false, "96 kHz / 8192" },
        };

        for (const auto& row : rows)
        {
            const double produced = hopHz (row.sampleRate, row.fftSize);
            const double visual = std::min (produced, kEditorTimerHz);
            const bool faster = produced > kEditorTimerHz;
            expect (faster == row.hopsFasterThanEditor, juce::String (row.label) + " vs 60 Hz");
            expect (visual <= kEditorTimerHz + kEps, juce::String (row.label) + " visual cap");
            logMessage (juce::String (row.label)
                        + ": FFT hop/s=" + juce::String (produced, 3)
                        + "  visual inject <= " + juce::String (visual, 3)
                        + " Hz  lerp-between-hops="
                        + (faster ? "no, hops collapse per editor tick" : "yes"));
        }

        expect (kMaxHopsPerTick >= 2, "pipeline may drain multiple hops per editor tick");
    }

    void testLerpIntervalClampVsTrueHopInterval()
    {
        beginTest ("A2 lerp clock clamp 16-50 ms vs true hop interval");

        const double hop48k4096 = hopIntervalMs (48000.0, 4096);
        const double hop96k4096 = hopIntervalMs (96000.0, 4096);
        const double hop48k8192 = hopIntervalMs (48000.0, 8192);
        const double hop96k8192 = hopIntervalMs (96000.0, 8192);

        expectWithinAbsoluteError (SpectrumHopLerp::clampHopIntervalMs (hop48k4096),
                                   hop48k4096, 1.0e-9, "48k/4096 unclamped");
        expectWithinAbsoluteError (SpectrumHopLerp::clampHopIntervalMs (hop48k8192),
                                   hop48k8192, 1.0e-9, "48k/8192 unclamped");
        expectWithinAbsoluteError (SpectrumHopLerp::clampHopIntervalMs (hop96k8192),
                                   hop96k8192, 1.0e-9, "96k/8192 unclamped");

        expect (hop96k4096 < SpectrumHopLerp::kHopIntervalMinMs,
                "96k/4096 true hop is below the lerp clamp");
        expectWithinAbsoluteError (SpectrumHopLerp::clampHopIntervalMs (hop96k4096),
                                   SpectrumHopLerp::kHopIntervalMinMs, 1.0e-9,
                                   "96k/4096 clamped to 16 ms");

        logMessage ("Shared A1/A2 debt: measured hop gap is editor inject gap, not FFT hop, "
                    "and A2 additionally clamps that gap into 16-50 ms.");
    }

    void testAssembledFrameDoesNotRetainThreeQuarters()
    {
        beginTest ("KnownDebt: hopSize=fftSize/4 does not keep 75% of the previous window");

        constexpr size_t fftSize = 4096;
        constexpr size_t hopSize = fftSize / 4;
        std::vector<float> overlap (fftSize, 0.0f);
        std::vector<float> staging (hopSize, 0.0f);
        std::vector<float> fftFrame (fftSize, 0.0f);

        for (size_t i = 0; i < hopSize; ++i)
        {
            overlap[i] = 1.0f;
            staging[i] = 2.0f;
        }

        // Replica of NewSpectrumPipeline::processOneHopFromStaging window build.
        std::fill (fftFrame.begin(), fftFrame.end(), 0.0f);
        std::copy (overlap.begin(), overlap.begin() + static_cast<std::ptrdiff_t> (hopSize),
                   fftFrame.begin());
        std::copy (staging.begin(), staging.begin() + static_cast<std::ptrdiff_t> (hopSize),
                   fftFrame.begin() + static_cast<std::ptrdiff_t> (hopSize));

        size_t filled = 0;
        for (float sample : fftFrame)
            if (sample != 0.0f)
                ++filled;

        expectEquals ((int) hopSize, (int) (fftSize / 4), "hop advance remains fftSize/4");
        expectEquals ((int) filled, (int) (2 * hopSize), "assembled frame fills 2*hopSize");
        expectEquals ((int) (fftSize - hopSize), (int) (3 * hopSize),
                      "true 75% overlap would retain 3*hopSize previous samples");
        expect ((int) filled != (int) fftSize, "current assembly is not a full fftSize window");
        logMessage ("Out of A/B: overlap buffer is fftSize but only hopSize samples are reused.");
    }

    void testSmoothingAdvancesPerDrainedHop()
    {
        beginTest ("KnownDebt: mapper coefficients assume 60 Hz, ticks follow hop drain");

        // SpectrumDisplayMapper builds alpha from a 60 Hz UI rate. NewSpectrumPipeline
        // then calls generateSmoothedUIFrame() once per drained hop, so the realised
        // attack/release depends on sample rate and FFT size. Shared by A1 and A2.
        constexpr float attackMs = 15.0f;
        const double designedAttackS = attackMs * 0.001;
        const double hopsTo1eAtUiRate = kEditorTimerHz * designedAttackS;

        const double hop48k4096 = hopHz (48000.0, 4096);
        const double hop96k4096 = hopHz (96000.0, 4096);
        const double hop48k8192 = hopHz (48000.0, 8192);

        const double tau48k4096 = hopsTo1eAtUiRate / hop48k4096;
        const double tau96k4096 = hopsTo1eAtUiRate / hop96k4096;
        const double tau48k8192 = hopsTo1eAtUiRate / hop48k8192;

        expect (tau96k4096 < designedAttackS, "96k/4096 attack is faster than the 15 ms design");
        expect (tau48k8192 > designedAttackS, "48k/8192 attack is slower than the 15 ms design");
        expectWithinAbsoluteError (tau48k4096, hopsTo1eAtUiRate / 46.875, 1.0e-9,
                                   "48k/4096 effective attack");

        logMessage ("Designed attack 15 ms @ 60 Hz UI. Effective if ticked per hop: "
                    + juce::String (tau48k4096 * 1000.0, 2) + " ms @ 48k/4096, "
                    + juce::String (tau96k4096 * 1000.0, 2) + " ms @ 96k/4096, "
                    + juce::String (tau48k8192 * 1000.0, 2) + " ms @ 48k/8192.");
    }

    void testA2ShowsPreviousHopAtT0()
    {
        beginTest ("A2 t=0 presents the previous hop; this is the A1/A2 policy split");

        SpectrumHopLerp hop;
        const std::vector<float> oldHop { -60.0f, -50.0f };
        const std::vector<float> newHop { -30.0f, -20.0f };
        hop.ingestHop (oldHop, oldHop, 0.0, false);
        hop.ingestHop (newHop, newHop, 21.0, true);

        std::vector<float> a2;
        SpectrumHopLerp::lerpSpectrumColumns (hop.prevPre, hop.currPre, hop.t, a2);
        expectEquals ((int) a2.size(), 2, "A2 column count");
        expectWithinAbsoluteError (a2[0], oldHop[0], 1.0e-5f, "A2 t=0 is previous hop");
        expect (hop.t <= 0.0f + 1.0e-5f, "new hop starts at t=0");
        logMessage ("A1 would show the current hop here. Do not change hop ingest in the A/B.");
    }
};

static AnalyzerABProtocolTest analyzerABProtocolTest;
