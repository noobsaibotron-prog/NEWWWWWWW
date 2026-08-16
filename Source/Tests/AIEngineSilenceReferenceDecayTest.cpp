// Regression test for the detector's level reference under true silence.
//
// SCOPE, stated up front because it is narrower than it may look: this covers
// the LEVEL REFERENCE only. It does NOT assert that detections clear on
// silence. Measurement showed those are separate: with the reference frozen the
// detector needed 126 calls to clear, and with it decaying to the floor it
// still needs 126. Whatever governs the clearing rate, it is not this. The
// clear-on-silence contract is asserted by AIHeadlessDetectionParityTest and
// AIFrontEndWiringTest, and is a separate open defect.
//
// AIEngine only refreshes currentRMS/averageRMS from spectrum bins above
// -100 dB. When the whole spectrum drops below that gate the sample count is
// zero, and before this test existed the reference simply stayed frozen at
// whatever the last audible frame produced. Relative-prominence detection then
// keeps re-asserting a correction that the input no longer supports.
//
// The defect was invisible for as long as the detector was fed through the GUI
// SpectrumAnalyzer, whose smoothing tail dragged the spectrum through the
// -100..-80 band for a few frames on the way down and let the reference decay
// before it froze. A raw front-end can legitimately step straight from a tone
// to a -120 dB floor in one hop, which is exactly what this test reproduces.
//
// It deliberately uses NO front end and NO processor: a fresh AIEngine, fed by
// hand. That keeps the contract attached to the detector, so replacing the
// front end again cannot bring the bug back unnoticed.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../AI/AIEngine.h"

#include <cmath>
#include <vector>

namespace
{
class AIEngineSilenceReferenceDecayTest final : public juce::UnitTest
{
public:
    AIEngineSilenceReferenceDecayTest()
        : juce::UnitTest("AI Engine silence reference decay", "AI") {}

    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kBins = 2049;
    static constexpr float kFloorDb = -120.0f;

    // Enough silent frames for the reference to converge on the floor at the
    // existing rmsSmoothing time constant, given that the every-third-frame
    // rate limiter inside analyzeSpectrum() lets only one call in three reach
    // the level-tracking code.
    static constexpr int kSilenceCallBudget = 120;

    static std::vector<float> toneSpectrum()
    {
        std::vector<float> s(kBins, kFloorDb);
        const double binHz = kSampleRate / (2.0 * (kBins - 1));
        for (int i = 0; i < kBins; ++i)
        {
            const double d = std::abs(i * binHz - 440.0);
            if (d < 300.0)
                s[static_cast<size_t>(i)] =
                    kFloorDb + static_cast<float>(40.0 * std::exp(-d / 60.0));
        }
        return s;
    }

    void runTest() override
    {
        beginTest("A hard step to digital silence decays the level reference");

        AIEngine engine;
        engine.setEnabled(true);
        engine.prepare(kSampleRate, kBlockSize);

        const auto tone = toneSpectrum();
        const std::vector<float> silence(kBins, kFloorDb);

        for (int i = 0; i < 120; ++i)
            engine.analyzeSpectrum(tone, /*force=*/false);

        const auto toneCorrections = engine.getPendingCorrections();
        const float toneAvgRms = engine.probeAverageRmsForTests();
        logMessage("  after tone: corrections=" + juce::String((int) toneCorrections.size())
                   + "  averageRMS=" + juce::String(toneAvgRms, 2) + " dB");

        expect(!toneCorrections.empty(),
               "Positive control failed: the detector found nothing in the tone, so a "
               "later empty result would prove nothing.");

        // Hard step. No ballistics, no interpolation: exactly what a raw
        // front-end delivers when the host stops sending audio.
        int callsToClear = -1;
        for (int i = 0; i < kSilenceCallBudget; ++i)
        {
            engine.analyzeSpectrum(silence, /*force=*/false);
            if (callsToClear < 0 && engine.getPendingCorrections().empty())
                callsToClear = i + 1;
        }

        const float silenceAvgRms = engine.probeAverageRmsForTests();
        const float silenceCurRms = engine.probeCurrentRmsForTests();
        juce::ignoreUnused(silenceCurRms);
        logMessage("  after silence: corrections="
                   + juce::String((int) engine.getPendingCorrections().size())
                   + "  averageRMS=" + juce::String(silenceAvgRms, 2) + " dB"
                   + "  currentRMS=" + juce::String(silenceCurRms, 2) + " dB");
        logMessage("  calls needed to clear: "
                   + (callsToClear < 0 ? juce::String("never") : juce::String(callsToClear))
                   + " of " + juce::String(kSilenceCallBudget));

        // Deliberately NOT asserted here: that the corrections are empty by now.
        // Fixing the frozen reference measurably did not change the clearing
        // rate (126 calls either way), so asserting it in this test would
        // attach a contract to the wrong component and mislead the next reader.
        juce::ignoreUnused(callsToClear);

        beginTest("The level reference itself decays, it does not merely stop mattering");

        // The downstream effect is not enough on its own. If the reference stays
        // frozen, any clearing would be incidental and could regress silently.
        // The reference is a mean over bins above the -100 dB gate, so its
        // tone-era value is not a loud number; what matters is that silence
        // drives it to the floor instead of leaving it wherever it stopped.
        // Asserted as movement toward the floor rather than an absolute
        // endpoint: the endpoint depends on the smoothing time constant, and
        // pinning it here would turn a behavioural contract into a tuning
        // fixture that has to be edited whenever the constant is revisited.
        expect(silenceAvgRms < toneAvgRms - 15.0f,
               "averageRMS did not move materially toward the silence floor: "
               + juce::String(toneAvgRms, 2) + " dB -> "
               + juce::String(silenceAvgRms, 2) + " dB. A reference that stops "
               "updating instead of decaying is the defect this test exists for.");

        expect(silenceAvgRms >= kFloorDb - 0.01f,
               "averageRMS decayed below the silence floor, which is not a physical level.");

        beginTest("Audible input after silence still recovers the reference");

        // The decay must not be a one-way trip: a detector that cannot climb
        // back would go deaf after the first pause in the programme material.
        for (int i = 0; i < 120; ++i)
            engine.analyzeSpectrum(tone, /*force=*/false);

        const float recoveredAvgRms = engine.probeAverageRmsForTests();
        logMessage("  after tone returns: averageRMS="
                   + juce::String(recoveredAvgRms, 2) + " dB  corrections="
                   + juce::String((int) engine.getPendingCorrections().size()));

        expect(recoveredAvgRms > silenceAvgRms + 10.0f,
               "The level reference did not recover once audible input returned.");
        expect(!engine.getPendingCorrections().empty(),
               "The detector stayed deaf after silence: the tone was not re-detected.");
    }
};

static AIEngineSilenceReferenceDecayTest aiEngineSilenceReferenceDecayTest;
} // namespace
