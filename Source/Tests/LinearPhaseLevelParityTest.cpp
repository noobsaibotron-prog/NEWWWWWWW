/*
 * Phase mode must not change the level.
 *
 * A user sets an EQ curve and picks a processing mode. Zero Latency, Natural
 * Phase and Linear Phase differ in phase response and latency — that is the
 * whole point — but they must agree on how loud the result is. If they do not,
 * switching mode becomes a volume control, and any A/B between modes is
 * meaningless.
 *
 * This was not true. The linear-phase IR builder scaled the impulse response by
 * 1/(mean linear magnitude over the FFT bins). FFT bins are linearly spaced, so
 * half of them sit above a quarter of the sample rate: a high cut silences most
 * of the bins, the mean collapses, and the "compensation" boosts the whole
 * filter. Measured on the reported case, a high cut got LOUDER the lower it was
 * dragged — about +3.6 dB at 10 kHz, +8.6 dB at 5 kHz, +15.9 dB at 2 kHz.
 *
 * The existing LinearPhaseGainRegressionTest could not catch it: it drives
 * LinearPhaseProcessor directly with a magnitude array of its own and never
 * reaches the builder in PluginProcessor, and it only exercises flat curves.
 * This test goes through the real processor so the builder is on the path.
 */
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>

class LinearPhaseLevelParityTest : public juce::UnitTest
{
public:
    LinearPhaseLevelParityTest()
        : juce::UnitTest("LinearPhase level parity across phase modes", "AIEQ-DSP") {}

    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr float kProbeHz = 300.0f;   // well inside the pass band of every cut below

    static void setParam(juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
    {
        if (auto* p = s.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    }

    /** Feed a steady sine at probeHz and return the RMS of the last blocks. */
    float measureRms(AIEqualizerAudioProcessor& proc, float probeHz, int blocks = 160)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        double phase = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * probeHz / kSampleRate;

        double sumSq = 0.0;
        int counted = 0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float v = static_cast<float>(std::sin(phase));
                phase += inc;
                if (phase >= 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
                buffer.setSample(0, i, v);
                buffer.setSample(1, i, v);
            }
            proc.processBlock(buffer, midi);

            if (b >= blocks - 20)
            {
                for (int i = 0; i < kBlockSize; ++i)
                {
                    const double x = buffer.getSample(0, i);
                    sumSq += x * x;
                    ++counted;
                }
            }
        }
        return counted > 0 ? static_cast<float>(std::sqrt(sumSq / counted)) : 0.0f;
    }

    static float toDb(float rms)
    {
        const float inRms = std::sqrt(0.5f);   // full-scale sine
        return 20.0f * std::log10(std::max(rms / inRms, 1.0e-9f));
    }

    struct Measured { float stopBandDb = 0.0f; float passBandDb = 0.0f; bool engaged = false; };

    /** phaseMode: 0 Zero Latency, 1 Natural Phase, 2 Linear Phase.
        Measures the stop band first: that doubles as the readiness signal, because
        an unloaded linear-phase IR passes audio through untouched and would make
        the pass-band comparison below vacuously true. */
    Measured measure(int phaseMode, float highCutHz)
    {
        Measured out;

        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& s = proc.getAPVTS();

        setParam(s, "phaseMode", static_cast<float>(phaseMode));
        setParam(s, "band3Type", 4.0f);          // "High Cut"
        setParam(s, "band3Freq", highCutHz);
        setParam(s, "band3Q", 0.707f);
        setParam(s, "band3Enabled", 1.0f);

        proc.triggerLinearPhaseIRUpdate();
        proc.requestIRBuild();

        // Bounded wait for the builder: pump audio (the swap-in happens from the
        // audio path) and stop as soon as the stop band is genuinely attenuated.
        // 1.5x the corner: above the knee, and still below Nyquist for every cut
        // swept here. A 4x probe put 10 kHz and 15 kHz cuts past Nyquist, where the
        // aliased tone came back unattenuated and the engagement check lied.
        const float stopHz = std::min(highCutHz * 1.5f, 20000.0f);
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            out.stopBandDb = toDb(measureRms(proc, stopHz, 60));
            if (out.stopBandDb < -4.0f) { out.engaged = true; break; }   // 12 dB/oct at 1.5x
            juce::Thread::sleep(120);
        }

        out.passBandDb = toDb(measureRms(proc, kProbeHz));
        return out;
    }

    void runTest() override
    {
        // Pre-declared: the pass-band probe sits at 300 Hz, far below every cut,
        // so an honest filter leaves it alone. A tolerance of 1.5 dB absorbs
        // filter skirt and windowing differences between the modes without
        // hiding anything of the size reported.
        constexpr float kTol = 1.5f;

        beginTest("Flat EQ: all three modes agree on level");
        {
            // A very high cut leaves the 300 Hz probe alone in every mode.
            const auto zl = measure(0, 20000.0f);
            const auto np = measure(1, 20000.0f);
            const auto lp = measure(2, 20000.0f);
            logMessage("  near-flat  ZL=" + juce::String(zl.passBandDb, 2)
                       + "  NP=" + juce::String(np.passBandDb, 2)
                       + "  LP=" + juce::String(lp.passBandDb, 2) + " dB");
            expectWithinAbsoluteError(lp.passBandDb, zl.passBandDb, kTol,
                                      "Linear Phase differs from Zero Latency on a near-flat EQ.");
            expectWithinAbsoluteError(np.passBandDb, zl.passBandDb, kTol,
                                      "Natural Phase differs from Zero Latency on a near-flat EQ.");
        }

        beginTest("High cut: Linear Phase must not get louder than Zero Latency");
        {
            // Swept deliberately downward: the defect grew as the cut moved down,
            // so a single frequency could have missed it.
            for (float fc : { 12000.0f, 8000.0f, 5000.0f, 2000.0f })
            {
                const auto zl = measure(0, fc);
                const auto lp = measure(2, fc);
                logMessage("  high cut " + juce::String((int) fc) + " Hz:  ZL="
                           + juce::String(zl.passBandDb, 2) + "  LP=" + juce::String(lp.passBandDb, 2)
                           + "  delta=" + juce::String(lp.passBandDb - zl.passBandDb, 2)
                           + " dB   (LP stop band " + juce::String(lp.stopBandDb, 1) + " dB)");

                // Without this the comparison is vacuous: an IR that never loaded
                // passes audio through and matches Zero Latency perfectly.
                expect(lp.engaged,
                       "Linear Phase never engaged the high cut at " + juce::String((int) fc)
                       + " Hz (stop band only " + juce::String(lp.stopBandDb, 1)
                       + " dB) — the level comparison below would prove nothing.");

                expectWithinAbsoluteError(lp.passBandDb, zl.passBandDb, kTol,
                    "Linear Phase level diverges from Zero Latency with a high cut at "
                    + juce::String((int) fc) + " Hz. Switching phase mode is acting as a "
                    "volume control.");
            }
        }

        beginTest("A cut never raises the level above unity");
        {
            // The direction matters on its own: whatever the modes agree on, an
            // attenuating curve that ends up louder than no EQ at all is wrong.
            for (float fc : { 8000.0f, 5000.0f, 2000.0f })
            {
                const auto m = measure(2, fc);
                const float lp = m.passBandDb;
                logMessage("  high cut " + juce::String((int) fc) + " Hz, Linear Phase: "
                           + juce::String(lp, 2) + " dB  (stop band "
                           + juce::String(m.stopBandDb, 1) + " dB)");
                expect(m.engaged, "Linear Phase did not engage the cut; the check below is vacuous.");
                expect(lp <= kTol,
                       "A high cut at " + juce::String((int) fc) + " Hz made the signal LOUDER ("
                       + juce::String(lp, 2) + " dB) in Linear Phase.");
            }
        }
    }
};

static LinearPhaseLevelParityTest linearPhaseLevelParityTest;
