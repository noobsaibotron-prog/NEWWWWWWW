#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace AIEQPerceptual
{

inline constexpr std::uint32_t kSpectralContextSchemaVersion = 2;

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
    int framesObserved = 0;
    float lfValidFraction = 0.0f;
    bool valid = false;

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
    float fullLevelConfidenceDb = -58.0f;
    int fullWarmupFrames = 12;
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
        float lfValidFraction = 0.0f) const;

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
        : builder(optionsIn) {}

    void prepare(std::span<const float> bandCentersHz);
    void reset() noexcept;

    [[nodiscard]] bool pushFrame(std::span<const float> bandDbFused,
                                 bool lfValid) noexcept;

    [[nodiscard]] SpectralContext snapshot() const;
    [[nodiscard]] int frameCount() const noexcept { return frames; }

private:
    SpectralContextBuilder builder;
    std::vector<float> centers;
    std::vector<double> mean;
    std::vector<double> m2;
    int frames = 0;
    int lfValidFrames = 0;
};

} // namespace AIEQPerceptual
