#if JUCE_UNIT_TESTS

/*
 * T5.5.1 - measurement only. No production behaviour is changed and nothing
 * here is asserted as ground truth.
 *
 * The problem this probes: the generic axisPosition asks "how far is this
 * region above or below the median of the whole spectrum?", and real music is
 * pink-tilted, so low regions sit above that median and high regions below it
 * by more than the +/-6 dB full scale. Measured on nine real sources, the axis
 * was at its clamp rail in 43 of 54 cases and in 33 of 36 Brightness cases -
 * a commercial full mix reading -1.00, "maximally dark", along with everything
 * else. A saturated ruler cannot rank sources.
 *
 * Two families of replacement are measured side by side, because they fail
 * differently and the difference is not obvious from the algebra:
 *
 *  NEIGHBOUR CONTRAST - a region against its immediate neighbours. A global
 *      tilt does NOT cancel here; it leaves a constant offset proportional to
 *      the octave distance between the regions, so every source keeps a shared
 *      bias. It should still RANK correctly, which is what matters.
 *
 *  TILT RESIDUAL - a region against the source's own fitted spectral slope.
 *      This removes the pink slope by construction, so a perfectly pink source
 *      lands at zero on every axis, and only departures from its own trend
 *      register. The risk is the opposite one: a source whose slope IS the
 *      musical content (a genuinely dull mix) may read as neutral.
 *
 * Usage:
 *   AIEQ_CANDIDATE_DIR=/path/to/wavs \
 *     build-verify/Release/bin/AIEqualizerPro_IntegrationTests \
 *       --category=ManualProbe -v
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../AI/PerceptualFrontEnd.h"
#include "../AI/SpectralContext.h"

#include <cmath>
#include <memory>
#include <vector>

namespace
{
using AIEQPerceptual::SpectralContext;
using AIEQPerceptual::SpectralContextAccumulator;
using AIEQPerceptual::SpectralRegion;

// Geometric centres of the seven regions, used to project the fitted tilt.
constexpr float kRegionCentreHz[] =
    { 34.6f, 98.0f, 283.0f, 1000.0f, 3162.0f, 7071.0f, 15492.0f };

bool loadAudio(const juce::File& f, juce::AudioBuffer<float>& out, double& sr)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr) return false;
    out.setSize(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(&out, 0, static_cast<int>(r->lengthInSamples), 0, true, true);
    sr = r->sampleRate;
    return true;
}

SpectralContext contextOf(const juce::AudioBuffer<float>& a, double sr)
{
    std::vector<float> mono(static_cast<size_t>(std::max(0, a.getNumSamples())), 0.0f);
    for (int c = 0; c < a.getNumChannels(); ++c)
    {
        const float* s = a.getReadPointer(c);
        for (int i = 0; i < a.getNumSamples(); ++i) mono[static_cast<size_t>(i)] += s[i];
    }
    if (a.getNumChannels() > 0)
        for (auto& v : mono) v /= static_cast<float>(a.getNumChannels());

    PerceptualFrontEnd fe;
    fe.prepare(sr);
    const auto frames = fe.analyzeAll(mono.data(), static_cast<int>(mono.size()));
    std::vector<float> centers;
    for (int b = 0; b < fe.numBands(); ++b) centers.push_back(fe.bandCenterHz(b));

    SpectralContextAccumulator acc;
    acc.prepare(centers);
    for (const auto& f : frames) acc.pushFrame(f.bandDbFused, f.lfValid);
    return acc.snapshot();
}

float residual(const SpectralContext& c, SpectralRegion r)
{
    const float centre = kRegionCentreHz[static_cast<std::size_t>(r)];
    const float predicted = c.spectralTiltDbPerOctave * std::log2(centre / 1000.0f);
    return c.region(r) - predicted;
}

juce::String d1(float v) { return juce::String(v, 1); }
juce::String pad(const juce::String& s, int w) { return s.paddedLeft(' ', w); }
} // namespace

class GoalSpecificContextProbe final : public juce::UnitTest
{
public:
    GoalSpecificContextProbe()
        : juce::UnitTest("Goal-specific context feature probe (T5.5.1)", "ManualProbe") {}

    void runTest() override
    {
        beginTest("Measure descriptors and candidate goal-specific contrasts");

        const auto dirEnv = juce::SystemStats::getEnvironmentVariable("AIEQ_CANDIDATE_DIR", {});
        if (dirEnv.isEmpty())
        {
            logMessage("  SKIP: set AIEQ_CANDIDATE_DIR=/path/to/wavs.");
            expect(true, "skipped");
            return;
        }
        juce::Array<juce::File> wavs;
        juce::File(dirEnv).findChildFiles(wavs, juce::File::findFiles, false, "*.wav;*.WAV");
        wavs.sort();
        if (wavs.isEmpty()) { expect(false, "no WAVs in " + dirEnv); return; }

        std::vector<std::pair<juce::String, SpectralContext>> all;
        for (const auto& w : wavs)
        {
            juce::AudioBuffer<float> a; double sr = 0.0;
            if (!loadAudio(w, a, sr)) continue;
            all.emplace_back(w.getFileNameWithoutExtension(), contextOf(a, sr));
        }

        logMessage("");
        logMessage("  RAW DESCRIPTORS (region values are dB relative to the median band)");
        logMessage("    source           tilt  centroid rolloff   Sub  Bass LoMid   Mid  Pres Brill   Air  hfPk");
        for (const auto& [n, c] : all)
            logMessage("    " + n.paddedRight(' ', 16)
                + pad(d1(c.spectralTiltDbPerOctave), 5)
                + pad(juce::String(c.perceptualCentroidHz, 0), 9)
                + pad(juce::String(c.perceptualRolloffHz, 0), 8)
                + pad(d1(c.region(SpectralRegion::Sub)), 6)
                + pad(d1(c.region(SpectralRegion::Bass)), 6)
                + pad(d1(c.region(SpectralRegion::LowMid)), 6)
                + pad(d1(c.region(SpectralRegion::Mid)), 6)
                + pad(d1(c.region(SpectralRegion::Presence)), 6)
                + pad(d1(c.region(SpectralRegion::Brilliance)), 6)
                + pad(d1(c.region(SpectralRegion::Air)), 6)
                + pad(d1(c.hfPeakProminenceDb), 6));

        logMessage("");
        logMessage("  WARMTH / MUD ZONES and REGIONAL CONFIDENCE");
        logMessage("    source          warmth   mud   | Sub Bass LoMid  Mid Pres Bril  Air");
        for (const auto& [n, c] : all)
        {
            juce::String rc;
            for (std::size_t r = 0; r < AIEQPerceptual::kSpectralRegionCount; ++r)
                rc += pad(juce::String(c.regionConfidence[r], 1), 5);
            logMessage("    " + n.paddedRight(' ', 16)
                + pad(d1(c.warmthZoneDb), 6) + pad(d1(c.mudZoneDb), 6) + "   |" + rc);
        }

        logMessage("");
        logMessage("  CANDIDATE A - NEIGHBOUR CONTRAST (dB). Positive = this region stands");
        logMessage("  ABOVE its neighbours, i.e. the source already has it and needs less.");
        logMessage("    source           airC  brilC  presC  warmC  mudC");
        for (const auto& [n, c] : all)
        {
            const float airC   = c.region(SpectralRegion::Air)
                               - (0.6f * c.region(SpectralRegion::Brilliance)
                                + 0.4f * c.region(SpectralRegion::Presence));
            const float brilC  = c.region(SpectralRegion::Brilliance)
                               - (0.5f * c.region(SpectralRegion::Presence)
                                + 0.5f * c.region(SpectralRegion::Air));
            const float presC  = c.region(SpectralRegion::Presence)
                               - (0.5f * c.region(SpectralRegion::Mid)
                                + 0.5f * c.region(SpectralRegion::Brilliance));
            const float warmC  = c.warmthZoneDb
                               - (0.5f * c.region(SpectralRegion::Bass)
                                + 0.5f * c.region(SpectralRegion::Mid));
            const float mudC   = c.mudZoneDb - (0.5f * c.warmthZoneDb
                                              + 0.5f * c.region(SpectralRegion::Mid));
            logMessage("    " + n.paddedRight(' ', 16)
                + pad(d1(airC), 6) + pad(d1(brilC), 7) + pad(d1(presC), 7)
                + pad(d1(warmC), 7) + pad(d1(mudC), 6));
        }

        logMessage("");
        logMessage("  CANDIDATE B - TILT RESIDUAL (dB). Each region against the source's OWN");
        logMessage("  fitted slope, so a perfectly pink source lands at 0.0 everywhere.");
        logMessage("    source            Sub  Bass LoMid   Mid  Pres Brill   Air");
        for (const auto& [n, c] : all)
        {
            juce::String row;
            for (std::size_t r = 0; r < AIEQPerceptual::kSpectralRegionCount; ++r)
                row += pad(d1(residual(c, static_cast<SpectralRegion>(r))), 6);
            logMessage("    " + n.paddedRight(' ', 16) + row);
        }

        logMessage("");
        logMessage("  CLAMP CHECK: how many of these candidates would saturate a +/-6 dB scale?");
        int nA = 0, nB = 0, tot = 0;
        for (const auto& [n, c] : all)
        {
            juce::ignoreUnused(n);
            const float airC = c.region(SpectralRegion::Air)
                             - (0.6f * c.region(SpectralRegion::Brilliance)
                              + 0.4f * c.region(SpectralRegion::Presence));
            const float warmC = c.warmthZoneDb
                              - (0.5f * c.region(SpectralRegion::Bass)
                               + 0.5f * c.region(SpectralRegion::Mid));
            for (float v : { airC, warmC }) { ++tot; if (std::abs(v) >= 6.0f) ++nA; }
            for (auto r : { SpectralRegion::Air, SpectralRegion::Bass })
                if (std::abs(residual(c, r)) >= 6.0f) ++nB;
        }
        logMessage("    neighbour contrast: " + juce::String(nA) + "/" + juce::String(tot) + " at rail");
        logMessage("    tilt residual:      " + juce::String(nB) + "/" + juce::String(tot) + " at rail");
        logMessage("    (today's generic axis, for comparison: 43/54 overall, 33/36 on Brightness)");
        logMessage("");
        logMessage("  READ: report-only. What matters is not the clamp count but whether the");
        logMessage("  ORDERING is musically plausible, and that is a listening judgement, not");
        logMessage("  something this probe can settle. No fixture here is ground truth.");
        expect(true, "probe completed");
    }
};

static GoalSpecificContextProbe sGoalSpecificContextProbe;

#endif
