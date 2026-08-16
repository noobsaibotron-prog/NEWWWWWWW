/**
 * AIHeadlessDetectionParityTest — EC-001/B4 regression.
 *
 * Continuous AI detection must be physically independent of PluginEditor and
 * SpectrumAnalyzer::processFFT(). Three processors receive identical audio:
 *   A) editor/GUI FFT never runs
 *   B) GUI FFT runs throughout
 *   C) GUI FFT runs for the first half, then stops
 *
 * The headless PerceptualFrontEnd must execute the same number of productive AI
 * analyses and produce equivalent decisions in all three cases. A positive
 * detect->clear witness ensures the test cannot pass merely because AI is blind.
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <vector>

namespace
{
/** Resolve the shipped ML weights from the source tree and force them into the
    engine. The test binary has no models/ folder beside it, so prepare() leaves
    the ML path unavailable and AIEngine silently falls back to the heuristic
    detector — a backend the product does not ship. The same __FILE__-relative
    idiom is already used by AIBackendSweepTest, AICorpusTest and the other AI
    tests that need the real network.

    Returns false if the ML path could not be made live, so the caller fails the
    test instead of quietly measuring the wrong detector. */
[[nodiscard]] inline bool forceShippedMLWeights(AIEqualizerAudioProcessor& proc,
                                                juce::String& detail)
{
    const juce::File weights =
        juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");

    if (! weights.existsAsFile())
    {
        detail = "Resources/Models/ml_weights.bin is missing from the source tree ("
               + weights.getFullPathName() + ")";
        return false;
    }

    proc.getAIEngine().setCustomMLWeightsPathForTests(weights);

    const auto status = proc.getAIEngine().getMLBackendStatus();
    if (status != AIEngine::MLBackendStatus::Active)
    {
        detail = "ML backend is " + AIEngine::getMLBackendStatusName(status)
               + " after loading the shipped weights. This test asserts the shipping "
                 "detection path, so it must fail here rather than exercise the "
                 "heuristic fallback.";
        return false;
    }

    detail = "ML backend Active (shipped ml_weights.bin)";
    return true;
}
} // namespace

class AIHeadlessDetectionParityTest : public juce::UnitTest
{
public:
    AIHeadlessDetectionParityTest()
        : juce::UnitTest("AI Headless Detection Parity", "Integration") {}

    void runTest() override
    {
        auto* mm = juce::MessageManager::getInstance();
        juce::ignoreUnused(mm);

        beginTest("Never-open / always-open / open-then-close produce equivalent AI detection");

        const auto a = runScenario(GuiMode::Never);
        const auto b = runScenario(GuiMode::Always);
        const auto c = runScenario(GuiMode::OpenThenClose);

        expectEquals(a.guiFftCalls, 0, "Headless witness unexpectedly ran GUI FFT.");
        expect(b.guiFftCalls > 0, "Always-open witness did not run GUI FFT.");
        expect(c.guiFftCalls > 0 && c.guiFftCalls < b.guiFftCalls,
               "Open-then-close witness did not exercise a distinct GUI lifecycle.");

        expect(a.analysisCount > 0, "Headless processor executed zero productive AI analyses.");
        expectEquals(a.analysisCount, b.analysisCount,
                     "Opening the GUI changed the number of headless AI analyses.");
        expectEquals(a.analysisCount, c.analysisCount,
                     "Closing the GUI mid-stream changed the number of headless AI analyses.");

        expectEquals(a.droppedSamples, static_cast<juce::int64>(0),
                     "Headless bounded witness dropped frontend samples.");
        expectEquals(b.droppedSamples, static_cast<juce::int64>(0),
                     "Always-open bounded witness dropped frontend samples.");
        expectEquals(c.droppedSamples, static_cast<juce::int64>(0),
                     "Open-close bounded witness dropped frontend samples.");

        // The positive witness is the INPUT stream, not a detection. Requiring a
        // detection here would tie the EC-001 gate to the recall of the shipped
        // model, which is a separate question and currently answers "none": the
        // project's own AI-Corpus scorecard records res3200_pink.wav as
        // KNOWN_FAIL under both ML and Hybrid at every sensitivity, and twelve
        // synthetic stimuli produced zero detections. Searching for an input
        // that happens to make seed22 fire would be test tuning, not validation.
        //
        // Comparing the recorded streams is a strictly stronger statement than
        // comparing detections anyway. It shows the detector received the same
        // frames, from the same sample positions, in the same order, while the
        // GUI did between zero and 640 FFTs. Detection quality cannot mask a
        // provenance difference here, because provenance is what is asserted.
        expect(!a.records.empty(),
               "Positive witness failed: no AI analysis was recorded at all, so the "
               "comparison below would be vacuous.");
        expectEquals(static_cast<int>(a.records.size()),
                     static_cast<int>(a.analysisCount),
                     "Recorded analyses do not match the processor's own counter.");

        expectIdenticalInputStreams(a.records, b.records, "never vs always");
        expectIdenticalInputStreams(a.records, c.records, "never vs open-close");

        // Corrections must still agree, empty or not: if the model ever starts
        // detecting, this keeps asserting that the GUI does not change what it
        // detects, with no edit needed here.
        expectCorrectionsEquivalent(a.toneCorrections, b.toneCorrections, "never vs always");
        expectCorrectionsEquivalent(a.toneCorrections, c.toneCorrections, "never vs open-close");

        beginTest("All GUI lifecycles clear the same detection after sustained silence");
        expect(a.silenceCorrections.empty(),
               "Headless detect->clear witness retained corrections after sustained silence.");
        expect(b.silenceCorrections.empty(),
               "Always-open detect->clear witness retained corrections after sustained silence.");
        expect(c.silenceCorrections.empty(),
               "Open-then-close detect->clear witness retained corrections after sustained silence.");
    }

private:
    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kToneBlocks = 320;    // ~3.4 s audio, enough for persistence
    static constexpr int kSilenceBlocks = 320;

    enum class GuiMode { Never, Always, OpenThenClose };

    /** One recorded analysis: what the detector was actually given, not just
        that it was given something. */
    struct AnalysisRecord
    {
        juce::int64 sequence = 0;
        juce::int64 sourceEndSample = 0;
        std::vector<float> spectrum;
    };

    struct ScenarioResult
    {
        int guiFftCalls = 0;
        juce::int64 analysisCount = 0;
        juce::int64 droppedSamples = 0;
        std::vector<AIEngine::Correction> toneCorrections;
        std::vector<AIEngine::Correction> silenceCorrections;
        std::vector<AnalysisRecord> records;
    };

    /** Compare two recorded input streams bin for bin.

        This is the heart of the EC-001 witness. The defect was that the
        detector only received data when the GUI ran SpectrumAnalyzer::
        processFFT(), so equal analysis COUNTS are not enough: two runs could
        agree on how many analyses happened and still have been fed different
        audio. Comparing the 2049 dB bins and the end-sample each frame came
        from is what actually rules that out. Exact equality is required, not a
        tolerance — same input through the same deterministic front end must
        produce the same floats. */
    void expectIdenticalInputStreams(const std::vector<AnalysisRecord>& lhs,
                                     const std::vector<AnalysisRecord>& rhs,
                                     const juce::String& label)
    {
        expectEquals(static_cast<int>(lhs.size()), static_cast<int>(rhs.size()),
                     label + ": different number of recorded AI analyses.");
        const auto n = std::min(lhs.size(), rhs.size());

        int seqMismatch = 0, sampleMismatch = 0, sizeMismatch = 0;
        double worstBinDelta = 0.0;
        juce::String firstBinDetail;

        for (size_t i = 0; i < n; ++i)
        {
            if (lhs[i].sequence != rhs[i].sequence) ++seqMismatch;
            if (lhs[i].sourceEndSample != rhs[i].sourceEndSample) ++sampleMismatch;
            if (lhs[i].spectrum.size() != rhs[i].spectrum.size()) { ++sizeMismatch; continue; }

            for (size_t b = 0; b < lhs[i].spectrum.size(); ++b)
            {
                const double d = std::abs(static_cast<double>(lhs[i].spectrum[b])
                                        - static_cast<double>(rhs[i].spectrum[b]));
                if (d > worstBinDelta)
                {
                    worstBinDelta = d;
                    firstBinDetail = " (analysis " + juce::String(static_cast<int>(i))
                                   + ", bin " + juce::String(static_cast<int>(b)) + ")";
                }
            }
        }

        expectEquals(seqMismatch, 0, label + ": analysis sequence numbers diverged.");
        expectEquals(sampleMismatch, 0,
                     label + ": frames came from different source sample positions.");
        expectEquals(sizeMismatch, 0, label + ": spectra had different bin counts.");
        expect(worstBinDelta == 0.0,
               label + ": the detector was fed different spectra. Worst bin delta "
               + juce::String(worstBinDelta, 6) + " dB" + firstBinDetail);

        logMessage("  " + label + ": " + juce::String(static_cast<int>(n))
                   + " analyses compared, worst bin delta "
                   + juce::String(worstBinDelta, 6) + " dB");
    }

    static bool pollUntil(const std::function<bool()>& pred, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter()
                            + static_cast<juce::uint32>(timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (pred())
                return true;
            juce::Thread::yield();
        }
        return pred();
    }

    static juce::int64 expectedFrontEndFrames(juce::int64 totalSamples)
    {
        if (totalSamples < PerceptualFrontEnd::kFftSize)
            return 0;
        return 1 + (totalSamples - PerceptualFrontEnd::kFftSize)
                   / PerceptualFrontEnd::kHopSize;
    }

    static juce::int64 expectedAnalysisCalls(juce::int64 frontEndFrames)
    {
        if (frontEndFrames <= 0)
            return 0;
        const auto interval = static_cast<juce::int64>(
            std::round(kSampleRate * 0.1));
        const auto lastFrameEnd = static_cast<juce::int64>(PerceptualFrontEnd::kFftSize)
                                + (frontEndFrames - 1) * PerceptualFrontEnd::kHopSize;
        return lastFrameEnd / interval;
    }

    static void maybeRunGuiFft(AIEqualizerAudioProcessor& proc,
                               GuiMode mode,
                               int absoluteBlock,
                               int totalBlocks,
                               int& callCounter)
    {
        bool run = mode == GuiMode::Always;
        if (mode == GuiMode::OpenThenClose)
            run = absoluteBlock < totalBlocks / 2;

        if (run)
        {
            proc.getSpectrumAnalyzer().processFFT();
            ++callCounter;
        }
    }

    static void feedToneBlock(AIEqualizerAudioProcessor& proc,
                              double& phase,
                              juce::MidiBuffer& midi)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        const double inc = 2.0 * juce::MathConstants<double>::pi * 440.0 / kSampleRate;
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float s = 0.25f * static_cast<float>(std::sin(phase));
            phase += inc;
            if (phase >= 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        proc.processBlock(buffer, midi);
    }

    static void feedSilenceBlock(AIEqualizerAudioProcessor& proc,
                                 juce::MidiBuffer& midi)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        buffer.clear();
        proc.processBlock(buffer, midi);
    }

    // Keep the synthetic producer bounded relative to the async worker. Every
    // eight 512-sample blocks form one main FFT window; wait until the frontend
    // has caught up before sending the next group. This tests AI semantics, not
    // FIFO-overrun policy.
    static bool waitForFrontEndCatchup(AIEqualizerAudioProcessor& proc,
                                       juce::int64 samplesFed)
    {
        const auto wanted = expectedFrontEndFrames(samplesFed);
        if (wanted <= 0)
            return true;
        return pollUntil([&]
        {
            return proc.getAIFrontEndDiagnostics().frames >= wanted;
        }, 1500);
    }

    ScenarioResult runScenario(GuiMode mode)
    {
        // Declared BEFORE the processor on purpose. The observer fires on the AI
        // worker thread, and that worker is only joined by ~AIEqualizerAudio-
        // Processor. Locals are destroyed in reverse order, so anything the
        // observer captures has to outlive the processor or a late callback
        // would write into destroyed storage.
        std::mutex recordMutex;
        std::vector<AnalysisRecord> records;

        AIEqualizerAudioProcessor proc;

        // Installed BEFORE prepareToPlay, which is what starts the AI worker.
        // Assigning a std::function that a running worker may already be
        // reading is a data race, and it crashed here the first time.
        proc.setAIAnalysisObserverForTests(
            [&records, &recordMutex](juce::int64 seq, juce::int64 endSample,
                                     const std::vector<float>& spectrum)
            {
                std::lock_guard<std::mutex> lock(recordMutex);
                records.push_back({ seq, endSample, spectrum });
            });

        proc.prepareToPlay(kSampleRate, kBlockSize);

        ScenarioResult out;

        {
            juce::String mlDetail;
            const bool mlLive = forceShippedMLWeights(proc, mlDetail);
            logMessage("  " + mlDetail);
            expect(mlLive, mlDetail);
            if (! mlLive) return out;
        }

        juce::MidiBuffer midi;
        double phase = 0.0;
        juce::int64 samplesFed = 0;

        for (int b = 0; b < kToneBlocks; ++b)
        {
            feedToneBlock(proc, phase, midi);
            samplesFed += kBlockSize;
            maybeRunGuiFft(proc, mode, b, kToneBlocks + kSilenceBlocks, out.guiFftCalls);

            if ((b + 1) % 8 == 0)
                expect(waitForFrontEndCatchup(proc, samplesFed),
                       "Front-end did not keep up with bounded tone input.");
        }

        const auto toneFrames = expectedFrontEndFrames(samplesFed);
        const auto toneAnalyses = expectedAnalysisCalls(toneFrames);
        expect(pollUntil([&]
        {
            return proc.getAIHeadlessAnalysisCountForTests() >= toneAnalyses;
        }, 2500), "Timed out waiting for headless tone analyses.");

        out.toneCorrections = proc.getAIEngine().getPendingCorrections();
        const auto toneAnalysisCount = proc.getAIHeadlessAnalysisCountForTests();

        for (int b = 0; b < kSilenceBlocks; ++b)
        {
            feedSilenceBlock(proc, midi);
            samplesFed += kBlockSize;
            maybeRunGuiFft(proc, mode, kToneBlocks + b,
                           kToneBlocks + kSilenceBlocks, out.guiFftCalls);

            if ((b + 1) % 8 == 0)
                expect(waitForFrontEndCatchup(proc, samplesFed),
                       "Front-end did not keep up with bounded silence input.");
        }

        const auto allFrames = expectedFrontEndFrames(samplesFed);
        const auto allAnalyses = expectedAnalysisCalls(allFrames);
        expect(pollUntil([&]
        {
            return proc.getAIHeadlessAnalysisCountForTests() >= allAnalyses;
        }, 2500), "Timed out waiting for headless silence analyses.");

        out.analysisCount = proc.getAIHeadlessAnalysisCountForTests();
        out.droppedSamples = proc.getAIFrontEndDiagnostics().droppedSamples;
        out.silenceCorrections = proc.getAIEngine().getPendingCorrections();

        {
            std::lock_guard<std::mutex> lock(recordMutex);
            out.records = records;
        }

        logMessage("  GUI calls=" + juce::String(out.guiFftCalls)
                   + "  tone analyses=" + juce::String(toneAnalysisCount)
                   + "  total analyses=" + juce::String(out.analysisCount)
                   + "  tone corrections=" + juce::String(static_cast<int>(out.toneCorrections.size()))
                   + "  after silence=" + juce::String(static_cast<int>(out.silenceCorrections.size())));
        // Detach before the locals the lambda captures go out of scope.
        proc.setAIAnalysisObserverForTests(nullptr);
        return out;
    }

    void expectCorrectionsEquivalent(const std::vector<AIEngine::Correction>& lhs,
                                     const std::vector<AIEngine::Correction>& rhs,
                                     const juce::String& label)
    {
        expectEquals(static_cast<int>(lhs.size()), static_cast<int>(rhs.size()),
                     label + ": correction count differs.");
        const auto n = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i < n; ++i)
        {
            expectEquals(static_cast<int>(lhs[i].type), static_cast<int>(rhs[i].type),
                         label + ": problem type differs.");
            expectWithinAbsoluteError(lhs[i].frequency, rhs[i].frequency, 1.0f,
                                      label + ": frequency differs.");
            expectWithinAbsoluteError(lhs[i].confidence, rhs[i].confidence, 1.0e-4f,
                                      label + ": confidence differs.");
        }
    }
};

static AIHeadlessDetectionParityTest sAIHeadlessDetectionParityTest;
