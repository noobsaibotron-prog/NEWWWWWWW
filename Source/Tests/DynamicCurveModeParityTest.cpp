/*
 * The live dynamic curve must follow the audio in every phase mode.
 *
 * Reported from Ableton: with a band in Compress, Expand or Gate, switching to
 * Natural Phase froze the orange gain-reduction area drawn over the spectrum —
 * either it stopped moving or it never appeared. The spectrum itself kept
 * scrolling, which is what made it obvious that only the dynamic overlay was
 * stuck.
 *
 * There are four Dynamic EQ instances and only one runs at a time: the base one
 * in Zero Latency and Linear Phase, dynamicEQProcessorHQ in Natural Phase, and
 * the Mid/Side pair in M/S. All four get identical band parameters, so they look
 * interchangeable — but only the running one has live envelope state. The
 * display read the base instance unconditionally, so in Natural Phase it was
 * watching an object that had stopped processing.
 *
 * The band meters never showed the defect: they go through the mode-aware cache
 * that updateDynamicMeterCacheFrom() fills from the running instance. Two paths
 * to the same information, only one of them aware of the mode — the same shape
 * as the SpectrumAnalyzer ownership defect.
 *
 * This asserts the display's own source, not the meter, because the meter was
 * already right while the picture was wrong.
 *
 * A note on the stimulus: an earlier version of this measurement used a
 * constant-amplitude sine. The reduction was then constant by construction and
 * the test could not tell "correct" from "frozen" — it passed against the bug.
 * The input here is amplitude-modulated so a working curve has to move.
 */
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <vector>

class DynamicCurveModeParityTest : public juce::UnitTest
{
public:
    DynamicCurveModeParityTest()
        : juce::UnitTest("Dynamic curve follows audio in every phase mode", "AIEQ-DSP") {}

    static constexpr double kSampleRate = 48000.0;
    static constexpr int kBlockSize = 512;
    static constexpr float kBandHz = 500.0f;

    static void setParam(juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
    {
        if (auto* p = s.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    }

    /** Drives an amplitude-modulated tone through the processor and samples the
        delta the DISPLAY would draw, straight from its own source. Returns the
        peak-to-peak excursion in dB: a live curve swings, a frozen one does not. */
    float curveExcursionDb(int phaseMode, int dynMode)
    {
        AIEqualizerAudioProcessor proc;
        proc.prepareToPlay(kSampleRate, kBlockSize);
        auto& s = proc.getAPVTS();

        setParam(s, "dynEqEnabled", 1.0f);
        setParam(s, "band3Enabled", 1.0f);
        setParam(s, "band3Type", 2.0f);              // Peak
        setParam(s, "band3Freq", kBandHz);
        setParam(s, "band3Gain", -12.0f);
        setParam(s, "band3Q", 1.0f);
        setParam(s, "band3DynMode", static_cast<float>(dynMode));
        // Keep both expansion directions away from the range clamp.  The old
        // 8:1 / -35 dB fixture saturated Expand+Above for almost the entire
        // modulation cycle once the detector became the sole time smoother,
        // so a correct live curve was constant by construction.
        setParam(s, "band3Threshold", -20.0f);
        setParam(s, "band3Ratio", 2.0f);
        setParam(s, "band3Attack", 5.0f);
        setParam(s, "band3Release", 20.0f);
        setParam(s, "band3Range", 48.0f);
        setParam(s, "phaseMode", static_cast<float>(phaseMode));
        // This test verifies which live DynamicEQ instance feeds the display;
        // it is not an asynchronous linear-IR timing test. Pin a flat IR so
        // background build completion cannot change the detector stimulus at a
        // nondeterministic block while the tight offline loop is measuring it.
        if (phaseMode == 2)
            proc.forceLinearIRReady();

        const float probeHz = kBandHz;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        double phase = 0.0, mod = 0.0;
        const double inc = 2.0 * juce::MathConstants<double>::pi * probeHz / kSampleRate;
        const double modInc = 2.0 * juce::MathConstants<double>::pi * 2.0 / kSampleRate;

        float minDelta = 1.0e9f, maxDelta = -1.0e9f;

        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < kBlockSize; ++i)
            {
                // Swings across the threshold so the dynamic stage has to move.
                const float env = 0.02f + 0.88f * static_cast<float>(0.5 * (1.0 + std::sin(mod)));
                mod += modInc;
                const float v = env * static_cast<float>(std::sin(phase));
                phase += inc;
                if (phase >= 2.0 * juce::MathConstants<double>::pi)
                    phase -= 2.0 * juce::MathConstants<double>::pi;
                buffer.setSample(0, i, v);
                buffer.setSample(1, i, v);
            }
            proc.processBlock(buffer, midi);

            if (b >= 100)   // after settling
            {
                // Exactly what AdvancedSpectrumDisplay asks for when it rebuilds
                // the dynamic curve.
                const auto& dyn = proc.getActiveDynamicEQProcessorForDisplay();
                float delta = 0.0f;
                dyn.evaluateDynamicReplacementDeltaDbForFrequencyArray(
                    &probeHz, &delta, 1, kSampleRate);
                minDelta = std::min(minDelta, delta);
                maxDelta = std::max(maxDelta, delta);
            }
        }
        return maxDelta - minDelta;
    }

    void runTest() override
    {
        // Pre-declared. The reduction swings roughly 18 dB on this stimulus, so a
        // curve that tracks it moves by several dB. 2 dB is far below that and far
        // above the frozen case, which is exactly 0.
        constexpr float kMinExcursion = 2.0f;

        struct Mode { const char* name; int index; };
        const Mode phaseModes[] = { { "Zero Latency ", 0 },
                                    { "Natural Phase", 1 },
                                    { "Linear Phase ", 2 } };

        for (const auto& dm : { Mode{ "Compress", 1 }, Mode{ "Expand", 2 } })
        {
            beginTest(juce::String("Dynamic mode ") + dm.name
                      + ": the drawn curve moves in every phase mode");

            const float reference = curveExcursionDb(0, dm.index);   // Zero Latency

            for (const auto& pm : phaseModes)
            {
                const float exc = curveExcursionDb(pm.index, dm.index);
                logMessage("  " + juce::String(dm.name) + " / " + juce::String(pm.name)
                           + ":  escursione curva = " + juce::String(exc, 2) + " dB");

                expect(exc >= kMinExcursion,
                       juce::String("The dynamic curve is frozen in ") + pm.name
                       + " with the band in " + dm.name + ": the display source moved only "
                       + juce::String(exc, 2) + " dB while the audio swung across the "
                       "threshold. The display is reading an instance that is not "
                       "processing.");

                // Movement alone is not enough. A curve that moves by half is still
                // lying about how much the band is doing, and that is exactly what
                // the oversampled instance produced while its coefficients were
                // evaluated at the host rate.
                expectWithinAbsoluteError(exc, reference, 1.0f,
                       juce::String("The dynamic curve moves in ") + pm.name
                       + " but by a different amount than Zero Latency ("
                       + juce::String(exc, 2) + " vs " + juce::String(reference, 2)
                       + " dB) on the same audio and the same settings.");
            }
        }

        beginTest("Legacy Gate (Expand+Below): the drawn curve moves in every phase mode");
        {
            // Host value 3 remains the legacy Gate ABI, but its authoritative
            // semantics are now Expand+Below.  It uses the same coefficient path
            // as the other signed dynamic actions, so the display must follow it
            // rather than merely agreeing on a frozen zero.
            const float zl = curveExcursionDb(0, 3);
            const float np = curveExcursionDb(1, 3);
            const float lp = curveExcursionDb(2, 3);
            logMessage("  Gate:  ZL=" + juce::String(zl, 2)
                       + "  NP=" + juce::String(np, 2)
                       + "  LP=" + juce::String(lp, 2) + " dB");
            expect(zl >= kMinExcursion,
                   "Legacy Gate is active in audio but its Zero Latency display curve is frozen.");
            expect(np >= kMinExcursion,
                   "Legacy Gate is active in audio but its Natural Phase display curve is frozen.");
            expect(lp >= kMinExcursion,
                   "Legacy Gate is active in audio but its Linear Phase display curve is frozen.");

            expectWithinAbsoluteError(np, zl, 2.0f,
                "Natural Phase diverges from Zero Latency on a gated band. Whatever "
                "the gate curve does, the two modes must at least agree — that part "
                "IS this change's claim.");
        }
    }
};

static DynamicCurveModeParityTest dynamicCurveModeParityTest;
