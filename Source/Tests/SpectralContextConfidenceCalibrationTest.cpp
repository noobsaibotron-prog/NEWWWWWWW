#if JUCE_UNIT_TESTS

/*
 * T5.4 step 1 - calibration data for what "enough confidence" should mean.
 *
 * Measured, not changed. This test alters no threshold and no formula; it
 * exists so that the decision about them rests on numbers rather than on the
 * arithmetic-by-hand that produced a nearly-but-not-quite-right decomposition
 * the first time.
 *
 * The problem it characterises: on the first real material the Level 0 gate was
 * pointed at (a lead vocal), confidence came out at 0.18 against 0.92 for
 * stationary pink noise, which is below SemanticContextualizer's 0.45 gate and
 * therefore switches source awareness off entirely. Measured factors were
 * level 0.642, coverage 0.709, stability 0.405. No single one of those explains
 * it - the product is what fails, and repairing any one term alone still leaves
 * the result under the gate.
 *
 * Real musical material differs from the synthetic fixtures the formula was
 * built against along TWO independent axes, so this separates them instead of
 * conflating them:
 *
 *   SPARSITY  - a vocal has little energy at 35 Hz or 18 kHz. sourceLevelDb is
 *               a median over all bands and coverage counts bands above an
 *               absolute floor, so both fall on sparse sources even at a
 *               perfectly normal listening level.
 *   DYNAMICS  - a vocal has pauses, transients and changing pitch, giving
 *               12-15 dB per-band deviation against the 1.4 dB of a stationary
 *               synthetic. The existing T4 stability test used a 1.25 dB
 *               ripple, roughly ten times less.
 *
 * The four synthetic cases below are a 2x2 over those two axes, so each factor
 * can be attributed rather than inferred. They are proxies with the right
 * STRUCTURE, not stand-ins for real audio - real bass and real full-mix
 * measurements still have to come from the Level 0 tool on real files.
 */

#include <juce_core/juce_core.h>
#include "../AI/SpectralContext.h"

#include <cmath>
#include <vector>

namespace
{
using namespace AIEQPerceptual;

std::vector<float> centers12PerOctave()
{
    std::vector<float> out;
    const float step = std::pow(2.0f, 1.0f / 12.0f);
    for (float f = 20.0f; f < 24000.0f / step; f *= step)
        out.push_back(f);
    return out;
}

/** Broadband: every band carries energy, like the pink-noise fixtures. */
std::vector<float> broadbandProfile(const std::vector<float>& c, float baseDb)
{
    std::vector<float> db(c.size(), baseDb);
    for (std::size_t i = 0; i < c.size(); ++i)
        db[i] = baseDb - 0.8f * std::log2(c[i] / 1000.0f); // gentle pink-ish tilt
    return db;
}

/** Sparse: energy concentrated in a vocal-like span, decaying steeply outside
    it, so the bands below ~100 Hz and above ~12 kHz sit near the usable floor
    exactly as they do on a real vocal. */
std::vector<float> sparseProfile(const std::vector<float>& c, float baseDb)
{
    std::vector<float> db(c.size(), -120.0f);
    for (std::size_t i = 0; i < c.size(); ++i)
    {
        const float oct = std::log2(c[i] / 700.0f);           // centred in vocal range
        db[i] = baseDb - 9.0f * std::abs(oct) * std::abs(oct) * 0.35f;
        db[i] = std::max(db[i], -125.0f);
    }
    return db;
}

/** Push `frames` frames, optionally with musical-scale temporal variation:
    per-band level movement, plus (when `silentGaps`) true pauses between
    phrases rather than merely quieter frames. The distinction turned out to
    matter: attenuating gaps by 18 dB produced confidence 0.47, above the gate,
    while the real vocal sits at 0.185. A proxy that does not reach the floor
    during pauses does not reproduce what real performance does to these terms. */
SpectralContext accumulate(const std::vector<float>& c,
                           const std::vector<float>& profile,
                           int frames,
                           float dynamicsDb,
                           bool silentGaps = false)
{
    SpectralContextAccumulator acc;
    acc.prepare(c);

    juce::Random rng(20260818);
    for (int f = 0; f < frames; ++f)
    {
        auto frame = profile;
        if (dynamicsDb > 0.0f)
        {
            // A quarter of frames are near-silent, like the gaps between
            // phrases; the rest move by up to +/- dynamicsDb per band.
            const bool gap = (f % 4) == 3;
            for (std::size_t i = 0; i < frame.size(); ++i)
            {
                if (gap)
                    frame[i] = silentGaps ? -120.0f : frame[i] - dynamicsDb * 1.5f;
                else
                    frame[i] += dynamicsDb * (rng.nextFloat() * 2.0f - 1.0f);
            }
        }
        acc.pushFrame(frame, true);
    }
    return acc.snapshot();
}

juce::String f3(float v) { return juce::String(v, 3); }
}

class SpectralContextConfidenceCalibrationTest final : public juce::UnitTest
{
public:
    SpectralContextConfidenceCalibrationTest()
        : juce::UnitTest("Spectral Context Confidence Calibration (T5.4)", "AI-Diag") {}

    void runTest() override
    {
        const auto c = centers12PerOctave();
        constexpr int kLongObservation = 600;

        auto report = [this](const char* label, const SpectralContext& ctx)
        {
            logMessage("  " + juce::String(label).paddedRight(' ', 34)
                       + " conf=" + f3(ctx.confidence)
                       + "  level=" + f3(ctx.levelScore)
                       + " coverage=" + f3(ctx.coverageScore)
                       + " stability=" + f3(ctx.stabilityScore)
                       + " warmup=" + f3(ctx.warmupScore)
                       + (ctx.confidence < 0.45f ? "   [GATED OFF]" : ""));
        };

        //==================================================================
        beginTest("Confidence across the sparsity x dynamics grid");
        {
            // The 2x2. Same level, same observation length; only structure differs.
            const auto broadStatic  = accumulate(c, broadbandProfile(c, -50.0f), kLongObservation, 0.0f);
            const auto broadDynamic = accumulate(c, broadbandProfile(c, -50.0f), kLongObservation, 12.0f);
            const auto sparseStatic = accumulate(c, sparseProfile(c, -50.0f),    kLongObservation, 0.0f);
            const auto sparseDynamic= accumulate(c, sparseProfile(c, -50.0f),    kLongObservation, 12.0f);

            report("broadband + stationary", broadStatic);
            report("broadband + dynamic", broadDynamic);
            report("sparse + stationary", sparseStatic);
            report("sparse + dynamic (attenuated gaps)", sparseDynamic);

            // The case that actually brackets real material. Everything above
            // over-estimates confidence relative to a real performance.
            const auto sparsePhrased = accumulate(c, sparseProfile(c, -50.0f), kLongObservation, 12.0f, true);
            report("sparse + dynamic + silent gaps", sparsePhrased);

            logMessage("  ---");
            logMessage("  Real vocal measured by the L0 tool, for comparison:");
            logMessage("    0 Lead Vocal.wav                 conf=0.185  level=0.642 coverage=0.709 stability=0.405");
            logMessage("  READ: the attenuated-gap row scores ABOVE the 0.45 gate and therefore does");
            logMessage("  NOT reproduce the problem. Silence between phrases - not sparsity, and not");
            logMessage("  per-band movement - is what drives the level and coverage terms down.");

            // Assertions only where the direction is not a judgement call.
            expect(broadStatic.confidence > sparsePhrased.confidence,
                   "the synthetic fixture family does not score above vocal-like "
                   "material - this grid is not reproducing the problem it exists "
                   "to characterise");
            expect(broadStatic.confidence >= 0.45f,
                   "even broadband stationary material is now gated off, which "
                   "would mean something changed in the formula, not in the input");
        }

        //==================================================================
        beginTest("Silence must stay near zero whatever else is calibrated");
        {
            // The reason confidence exists at all. Any recalibration that lets
            // this rise has gone too far, so it is asserted rather than reported.
            std::vector<float> digitalSilence(c.size(), -120.0f);
            const auto silent = accumulate(c, digitalSilence, kLongObservation, 0.0f);
            report("digital silence", silent);
            expect(silent.confidence < 0.05f,
                   "digital silence produced usable confidence");

            // A single burst is not evidence either, however loud it is.
            const auto brief = accumulate(c, broadbandProfile(c, -50.0f), 2, 0.0f);
            report("broadband, 2 frames only", brief);
            expect(brief.confidence < 0.45f,
                   "two frames of audio already clear the contextualizer gate");
        }

        //==================================================================
        beginTest("Does more observation converge, on dynamic material?");
        {
            // The stated intent in SpectralContext.cpp is that "dynamic music
            // should still reach high confidence after sufficient observation".
            // This measures whether that actually happens: if stability is a
            // per-frame similarity measure rather than a support measure, more
            // frames will NOT help and the curve will flatten well below the gate.
            logMessage("  vocal-like source, increasing observation:");
            for (int frames : { 12, 60, 300, 1200, 3600 })
            {
                const auto ctx = accumulate(c, sparseProfile(c, -50.0f), frames, 12.0f, true);
                logMessage("    frames=" + juce::String(frames).paddedLeft(' ', 5)
                           + "  conf=" + f3(ctx.confidence)
                           + "  stability=" + f3(ctx.stabilityScore)
                           + "  warmup=" + f3(ctx.warmupScore));
            }
            logMessage("  READ: warmup saturates at 12 frames by construction. If confidence "
                       "stops improving after that on dynamic material, then observation "
                       "length is not what the stability term is measuring.");
        }

        //==================================================================
        beginTest("Which single factor, if repaired, would clear the gate?");
        {
            // Attribution the measured product cannot give directly: the factors
            // multiply, so a term can be the largest contributor to the shortfall
            // and still not be sufficient on its own.
            // Use the phrased variant: the attenuated-gap one already clears the
            // gate, so single-factor attribution against it would be vacuous.
            const auto v = accumulate(c, sparseProfile(c, -50.0f), kLongObservation, 12.0f, true);
            const float gate = 0.45f;

            auto ifPerfect = [&](const char* name, float excluded)
            {
                const float rest = excluded > 1.0e-6f ? v.confidence / excluded : 0.0f;
                logMessage("    " + juce::String(name).paddedRight(' ', 12)
                           + " -> product without it = " + f3(rest)
                           + (rest >= gate ? "   CLEARS the gate" : "   still gated"));
            };
            logMessage("  measured product=" + f3(v.confidence) + ", gate=" + f3(gate));
            ifPerfect("level", v.levelScore);
            ifPerfect("coverage", v.coverageScore);
            ifPerfect("stability", v.stabilityScore);
            logMessage("  READ: this is the calibration question in one line - if no single "
                       "term clears it, the fix is not 'the stability term is wrong', it is "
                       "what the product of these factors is supposed to mean.");
        }
    }
};

static SpectralContextConfidenceCalibrationTest sSpectralContextConfidenceCalibrationTest;

#endif // JUCE_UNIT_TESTS
