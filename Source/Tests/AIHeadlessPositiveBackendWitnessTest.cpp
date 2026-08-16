/*
 * Structural companion to AIHeadlessDetectionParityTest.
 *
 * That test proves the authoritative EC-001 claim on the SHIPPING path: with
 * ml_weights.bin loaded and MLBackendStatus::Active, the detector receives a
 * bit-identical stream of frames whether the editor is never opened, always
 * open, or opened and closed mid-stream. It compares inputs rather than
 * detections, because the shipped model produces none on synthetic material —
 * the project's own AI-Corpus scorecard already records res3200_pink.wav as
 * KNOWN_FAIL under both ML and Hybrid at every sensitivity.
 *
 * That leaves one thing unproven: when the downstream detector DOES emit
 * corrections, are those corrections themselves unaffected by the GUI? This
 * test answers that, and it has to select a backend that actually fires, which
 * is the heuristic one.
 *
 * It is therefore explicitly NOT a test of the shipping detector, and must not
 * be read as one. It is a structural witness: given a backend that produces
 * output, opening or closing the editor does not change that output.
 */
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

class AIHeadlessPositiveBackendWitnessTest : public juce::UnitTest
{
public:
    AIHeadlessPositiveBackendWitnessTest()
        : juce::UnitTest("AI Headless Detection Parity — positive backend witness",
                         "Integration") {}

    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr int kToneBlocks = 320;

    enum class GuiMode { Never, Always, OpenThenClose };

    struct Result
    {
        int guiFftCalls = 0;
        std::vector<AIEngine::Correction> corrections;
    };

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

    Result runScenario(GuiMode mode)
    {
        Result out;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);

        // The whole point of this witness: pick the backend that emits, and say
        // so out loud rather than letting a missing weights file decide it.
        proc.getAIEngine().setDetectionBackendMode(
            AIEngine::DetectionBackendMode::HeuristicOnly);

        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * 440.0 / kSampleRate;

        for (int b = 0; b < kToneBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer(2, kBlockSize);
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

            const bool runGui = mode == GuiMode::Always
                             || (mode == GuiMode::OpenThenClose && b < kToneBlocks / 2);
            if (runGui)
            {
                proc.getSpectrumAnalyzer().processFFT();
                ++out.guiFftCalls;
            }

            // Paced: an unpaced producer outruns the AI consumer and the
            // fail-closed overrun policy then resets the frontend, which has
            // nothing to do with what this test measures.
            juce::Thread::sleep(2);
        }

        pollUntil([&] { return ! proc.getAIEngine().getPendingCorrections().empty(); }, 2000);
        out.corrections = proc.getAIEngine().getPendingCorrections();

        logMessage("  gui fft calls=" + juce::String(out.guiFftCalls)
                   + "  corrections=" + juce::String(static_cast<int>(out.corrections.size())));
        return out;
    }

    void expectSameCorrections(const std::vector<AIEngine::Correction>& lhs,
                               const std::vector<AIEngine::Correction>& rhs,
                               const juce::String& label)
    {
        expectEquals(static_cast<int>(lhs.size()), static_cast<int>(rhs.size()),
                     label + ": different number of corrections.");

        auto sorted = [](std::vector<AIEngine::Correction> v)
        {
            std::sort(v.begin(), v.end(),
                      [](const AIEngine::Correction& a, const AIEngine::Correction& b)
                      {
                          if (a.type != b.type) return a.type < b.type;
                          return a.frequency < b.frequency;
                      });
            return v;
        };

        const auto l = sorted(lhs);
        const auto r = sorted(rhs);
        const auto n = std::min(l.size(), r.size());

        for (size_t i = 0; i < n; ++i)
        {
            expect(l[i].type == r[i].type,
                   label + ": correction " + juce::String(static_cast<int>(i))
                   + " changed type.");
            // Frequencies come from the same deterministic detector on the same
            // audio, so a tolerance here would only hide a real divergence.
            expect(std::abs(l[i].frequency - r[i].frequency) < 0.01f,
                   label + ": correction " + juce::String(static_cast<int>(i))
                   + " moved from " + juce::String(l[i].frequency, 2)
                   + " Hz to " + juce::String(r[i].frequency, 2) + " Hz.");
        }
    }

    void runTest() override
    {
        beginTest("With a backend that emits, GUI lifecycle does not change detections");

        const auto a = runScenario(GuiMode::Never);
        const auto b = runScenario(GuiMode::Always);
        const auto c = runScenario(GuiMode::OpenThenClose);

        expectEquals(a.guiFftCalls, 0, "Headless witness unexpectedly ran GUI FFT.");
        expect(b.guiFftCalls > 0, "Always-open witness did not run GUI FFT.");
        expect(c.guiFftCalls > 0 && c.guiFftCalls < b.guiFftCalls,
               "Open-then-close witness did not sit between the other two.");

        // Without this the comparisons below would be three empty sets agreeing
        // with each other, which is exactly the vacuity this test exists to
        // avoid.
        expect(! a.corrections.empty(),
               "Positive witness failed: the heuristic backend produced no corrections, "
               "so the equality checks below would prove nothing.");

        expectSameCorrections(a.corrections, b.corrections, "never vs always");
        expectSameCorrections(a.corrections, c.corrections, "never vs open-close");
    }
};

static AIHeadlessPositiveBackendWitnessTest aiHeadlessPositiveBackendWitnessTest;
