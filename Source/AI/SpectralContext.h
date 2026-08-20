#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace AIEQPerceptual
{

inline constexpr std::uint32_t kSpectralContextSchemaVersion = 4;

enum class SpectralRegion : std::uint8_t
{
    Sub = 0,
    Bass,
    LowMid,
    Mid,
    Presence,
    Brilliance,
    Air,
    Count
};

inline constexpr std::size_t kSpectralRegionCount =
    static_cast<std::size_t>(SpectralRegion::Count);

/**
 * Deterministic, source-level-normalized tonal context.
 *
 * regionRelativeDb, tilt, centroid and rolloff describe SHAPE, not absolute
 * playback level. sourceLevelDb/confidence remain diagnostic gates only.
 * The contract is intentionally independent of seed22/problem classification.
 */
struct SpectralContext
{
    std::uint32_t schemaVersion = kSpectralContextSchemaVersion;
    std::array<float, kSpectralRegionCount> regionRelativeDb {};

    float spectralTiltDbPerOctave = 0.0f;
    float perceptualCentroidHz = 1000.0f;

    // Renamed in schema 2. This is the frequency below which 85% of the
    // LOG-BAND pseudo-energy lies. Log-spaced bands weight each octave equally
    // rather than by Hz, so it is not the classical energy rolloff and must not
    // be reused as one — the old name invited exactly that mistake.
    float perceptualRolloffHz = 1000.0f;

    // [0, 1] tonal-prominence summaries derived from normalized shape.
    float lowMidActivity = 0.0f;

    // Renamed in schema 2: this is BROAD prominence over Brilliance+Air. It is
    // averaged over whole regions and is deliberately blind to narrow features;
    // hfPeakProminenceDb below is what sees those.
    float broadHighFrequencyProminence = 0.0f;

    // Schema 2 additions. The policy needs distinctions the seven public
    // regions cannot express:
    //
    //  hfPeakProminenceDb - how far the strongest HF band stands above the
    //      robust HF baseline (95th percentile minus median over ~5-18 kHz).
    //      Separates "broadly dark up top" from "dark with an isolated spike",
    //      which a regional mean cannot: a 14 dB spike at 12 kHz moves the Air
    //      mean by ~4.5 dB and leaves broad prominence at exactly zero.
    //
    //  warmthZoneDb / mudZoneDb - the 160-500 Hz region is a single number, so
    //      a bump at 170 Hz and a bump at 370 Hz land in the same bucket even
    //      though one is body and the other is mud. These split it.
    float hfPeakProminenceDb = 0.0f;
    float warmthZoneDb = 0.0f;   // ~120-250 Hz
    float mudZoneDb = 0.0f;      // ~250-500 Hz

    // Absolute level is kept only to make confidence fail closed near silence.
    float sourceLevelDb = -120.0f;
    float confidence = 0.0f;

    // Schema 4 - confidence answers "have I got reliable evidence?", not
    // "does this look like stationary broadband noise?".
    //
    // Schema 3 published the factors and the measurement that followed gated
    // six of nine real sources off: a bass failed on coverage, raw vocals on
    // stability, hi-hat/pad/shaker on level. Each term was the binding one
    // somewhere, so no single repair worked. What they had in common is that
    // they measured properties of the SOURCE - sparse, dynamic, quiet - and
    // charged them as uncertainty. Sparseness is information about a bass, not
    // doubt about it.
    //
    // Three structural changes follow, each measured before it was chosen:
    //
    //  - Absolute level is a near-silence guard, not a graded quality. Shape
    //    descriptors are computed from level-normalized dB and are already
    //    level-invariant, and real single tracks legitimately sit low (measured
    //    band peaks: -23 dB for a full mix, -70 dB for a hi-hat). Percentiles
    //    over "active" bands were measured as a replacement and were WORSE:
    //    they widened the spread across sources from 25 dB to 46-56 dB.
    //
    //  - Stability is the standard error of the aggregate, not frame-to-frame
    //    similarity. The old term never improved with observation - measured
    //    flat from 12 to 3600 frames - so a dynamic performance could never
    //    become knowable. Across nine real sources sigma/sqrt(N) lands between
    //    0.165 and 0.413 dB: with hundreds of frames the mean spectrum really
    //    is well determined, however much the source moves.
    //
    //  - Occupancy went regional. It was never the wrong measurement, only the
    //    wrong scope: a bass has no evidence above 10 kHz and that must not
    //    reduce confidence in its low end. coverageScore is still computed and
    //    published, but it no longer multiplies into confidence.
    float levelScore = 0.0f;      // near-silence margin of bandPeakDb above the floor
    float coverageScore = 0.0f;   // global occupancy, DESCRIPTIVE ONLY since schema 4
    float warmupScore = 0.0f;     // observation sufficiency, from ACTIVE frames
    float stabilityScore = 0.0f;  // from aggregateStandardErrorDb
    float lfScore = 0.0f;

    // Per-region evidence. This is what a goal should consult: "more air" on a
    // bass must be able to find that there is nothing reliable up there,
    // without the bass being declared unanalysable overall.
    //
    // KNOWN LIMITATION, measured not assumed: occupancy is judged against the
    // absolute -108 dB analysis floor, so on a quietly recorded source the
    // floor admits low-level noise as evidence. A hi-hat whose band peak is
    // -70 dB reports Sub = 1.00, which is room rumble and bleed, not cymbal.
    // The sources this matters for are exactly the quiet ones; a full mix at
    // -23 dB is unaffected. Fixing it needs a per-source noise-floor estimate
    // (a low percentile of the band distribution) rather than a fixed floor,
    // which is deliberately left out of this tranche.
    std::array<float, kSpectralRegionCount> regionConfidence {};

    int framesObserved = 0;
    int activeFrames = 0;          // frames carrying tonal evidence (not pauses)
    float activeFrameFraction = 0.0f;
    float aggregateStandardErrorDb = 0.0f;
    float bandPeakDb = -120.0f;
    float lfValidFraction = 0.0f;

    // Layer 1. Structural sufficiency, kept separate from graded confidence so
    // that silence and two-frame buffers fail hard rather than merely scoring
    // low. `valid` still means "this snapshot is well-formed".
    bool hasUsableEvidence = false;
    bool valid = false;

    [[nodiscard]] float region_confidence(SpectralRegion r) const noexcept
    {
        return regionConfidence[static_cast<std::size_t>(r)];
    }

    /** Region level measured against this source's OWN fitted slope rather than
        against the median band.

        Music is pink-tilted, so a level-versus-median reading puts every low
        region above the reference and every high region below it. Measured on
        nine real sources, that saturated the Brightness axis in 37 of 45 cases
        and reported a commercial full mix as maximally dark. The residual asks
        the question that actually matters - is this region above or below what
        this source's own trend predicts - and reads +0.9 dB at Air on the same
        full mix.

        CAUTION: the slope is fitted across 80 Hz to 12 kHz, so on a source
        whose content occupies a narrow span it is an extrapolation and can be
        wildly wrong. The bass in the corpus fits -19.2 dB/octave and yields an
        Air residual of +39.8 dB, for a region with no content at all. Callers
        must gate on regionConfidence; this function cannot do it for them
        because a caller averaging several regions has to drop the unsupported
        ones rather than average a sentinel. */
    [[nodiscard]] float regionResidualDb(SpectralRegion r) const noexcept;

    [[nodiscard]] float region(SpectralRegion regionIn) const noexcept
    {
        return regionRelativeDb[static_cast<std::size_t>(regionIn)];
    }
};

struct SpectralContextBuilderOptions
{
    float minAnalysisHz = 30.0f;
    float maxAnalysisHz = 18000.0f;
    float minUsableDb = -108.0f;

    // Schema 4 replaces fullLevelConfidenceDb = -58 dB. Every one of nine real
    // sources measured BELOW that reference, including a commercial full mix at
    // -65 dB, so it was not a strict setting but a mis-set one. The replacement
    // asks a different question: is the band peak far enough above the analysis
    // floor for the SHAPE to be measurable?
    //
    // Placement is set by the margin below real material, not by any test:
    // measured band peaks ran from -23 dB (full mix) to -70.3 dB (a quietly
    // recorded hi-hat), so -90 leaves roughly 20 dB of headroom beneath the
    // quietest real source while sitting 18 dB above the -108 analysis floor.
    //
    // This layer has to do the vetoing, because the geometric mean deliberately
    // will not: a lone 0.10 factor becomes 0.47 under a cube root. That is the
    // point of it for three mediocre-but-real evidences, and the reason a
    // near-silent buffer has to be excluded structurally rather than by scoring
    // low. A source below this floor is not analysed at all.
    float nearSilenceFloorDb = -90.0f;
    float fullEvidenceLevelDb = -80.0f;

    // A frame carries tonal evidence when its strongest band is within this of
    // the running peak. Measured duty cycles barely move across 30/40/50 dB
    // (0.73/0.77/0.79 on a lead vocal), so this sits on a flat part of the
    // curve rather than on a cliff.
    float activeFrameRangeDb = 40.0f;

    // Observation sufficiency, in ACTIVE frames. 48 frames is about two seconds
    // at the 2048-sample hop, which is the point where a tonal average stops
    // being a snapshot of one phrase.
    int minObservationFrames = 8;
    int fullObservationFrames = 48;

    // Standard-error band for the stability term, in dB. Nine real sources
    // measured 0.165-0.413; twelve frames of dynamic material lands near 2.3.
    float fullStabilityStdErrDb = 0.5f;
    float noStabilityStdErrDb = 3.0f;

    float rolloffFraction = 0.85f;
};

/** Pure-core builder over already-computed perceptual log bands. */
class SpectralContextBuilder
{
public:
    SpectralContextBuilder() = default;
    explicit SpectralContextBuilder(SpectralContextBuilderOptions optionsIn)
        : options(optionsIn) {}

    [[nodiscard]] SpectralContext build(
        std::span<const float> bandCentersHz,
        std::span<const float> meanBandDbFused,
        std::span<const float> temporalStdDevDb = {},
        int framesObserved = 1,
        float lfValidFraction = 0.0f,
        int activeFrames = -1) const;   // <0 means "all frames were active"

private:
    SpectralContextBuilderOptions options;
};

/**
 * Worker-owned temporal accumulator. prepare() allocates; pushFrame() performs
 * no allocation when the supplied band count matches prepare().
 */
class SpectralContextAccumulator
{
public:
    explicit SpectralContextAccumulator(SpectralContextBuilderOptions optionsIn = {})
        : builder(optionsIn), builderOptions(optionsIn) {}

    void prepare(std::span<const float> bandCentersHz);
    void reset() noexcept;

    [[nodiscard]] bool pushFrame(std::span<const float> bandDbFused,
                                 bool lfValid) noexcept;

    [[nodiscard]] SpectralContext snapshot() const;
    [[nodiscard]] int frameCount() const noexcept { return frames; }

private:
    SpectralContextBuilder builder;
    SpectralContextBuilderOptions builderOptions;
    std::vector<float> centers;
    std::vector<double> mean;
    std::vector<double> m2;
    int frames = 0;
    int activeFrames = 0;
    int lfValidFrames = 0;

    // Monotonic running peak of the per-frame strongest band. It only rises, so
    // a pause can never lower the bar that defines "playing". Frames seen
    // before the source reaches full level are therefore judged against a lower
    // peak and counted active; that errs towards keeping evidence rather than
    // discarding it, which is the safe direction for an online classifier that
    // cannot revisit earlier frames.
    float runningPeakDb = -200.0f;
};

} // namespace AIEQPerceptual
