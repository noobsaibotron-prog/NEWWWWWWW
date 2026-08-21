#pragma once

// Frozen P4-D2 held-out generators extracted from AIAccuracyTest.cpp.
// A1a contract: generator semantics, RNG consumption, ranges and output spectra
// must remain identical to baseline b99b99e3.

#include <juce_core/juce_core.h>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <cmath>
#include <random>
#include <vector>

namespace aieq_test::heldout
{
constexpr double kSampleRate = 44100.0;
constexpr int    kFFTSize    = 4096;
constexpr int    kNumBins    = kFFTSize / 2;
    constexpr float kBaselineDb  = -26.0f;   // == linear 0.05 (training energy regime)
    constexpr int   kVariations  = 12;       // fixtures per class

    // ── P4-M1 FROZEN DISJOINT EVAL — train/eval contract (do NOT violate in P4-M2) ──
    // This held-out is the PRIMARY per-class model metric. To stay a genuine
    // generalization probe AFTER P4-M2 teaches the training generator about tilt, the
    // eval distribution is FROZEN here and declared disjoint from the planned training
    // distribution BY SHAPE, not merely by seed:
    //   • TILT SLOPE — EVAL owns the STEEP EXTRAPOLATION band [-6.5,-5.0] dB/decade
    //     (below). P4-M2 training MUST stay in the gentle-moderate INTERPOLATION band
    //     [-4.5,-1.5] (>=0.5 dB/dec margin). The model is thus tested one notch steeper
    //     than anything it trained on.
    //   • PEAK SHAPE — EVAL uses dB-domain Gaussian humps (addGaussianPeakDb) + the
    //     subtractive low-shelf below. P4-M2 training MUST use a DIFFERENT shape family
    //     (e.g. linear-domain peaks / a distinct sigma range), and MUST NOT call/copy
    //     these heldout builders.
    //   • FREQ GRID — EVAL owns the per-class centre ranges in make(). P4-M2 training
    //     centres MUST be offset from them.
    // Marker: the diagnostic seeds were bumped (heldout_v2) when this freeze landed.
    constexpr float kEvalTiltMinSlope = -6.5f;   // EVAL extrapolation band (steep)
    constexpr float kEvalTiltMaxSlope = -5.0f;   // P4-M2 TRAIN reserved to [-4.5,-1.5]
    constexpr float kTrainTiltMinSlope = -4.5f;  // TRAIN interpolation band (P4-M2a)
    constexpr float kTrainTiltMaxSlope = -1.5f;

    enum class HClass { Resonance, Harshness, Muddiness, Sibilance, Boominess, Boxyness, Thinness };

    inline float dbToLinearMag(float db) { return std::pow(10.0f, db / 20.0f); }

    // Natural pink/steep tilt in dB (slope is NEGATIVE dB per decade, ref @100 Hz).
    inline void addPinkTiltDb(std::vector<float>& specDb, float slopePerDecade)
    {
        const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
        for (int i = 0; i < kNumBins; ++i)
        {
            const float f = juce::jmax(20.0f, static_cast<float>(i) * binHz);
            specDb[static_cast<size_t>(i)] += slopePerDecade * std::log10(f / 100.0f);
        }
    }

    // Additive dB Gaussian hump (same shape as AIBackendSweepTest::addPeakDb).
    inline void addGaussianPeakDb(std::vector<float>& specDb, float freqHz, float peakDb, float sigmaFactor)
    {
        const float binHz   = static_cast<float>(kSampleRate) / kFFTSize;
        const float sigmaHz = juce::jmax(30.0f, freqHz * sigmaFactor);
        for (int i = 0; i < kNumBins; ++i)
        {
            const float f = static_cast<float>(i) * binHz;
            const float d = (f - freqHz) / sigmaHz;
            specDb[static_cast<size_t>(i)] += peakDb * std::exp(-0.5f * d * d);
        }
    }

    // Subtractive low-shelf below cutoff (Thinness = lack of lows). Full depth well
    // below the cutoff, ramping to 0 over ~1 octave up to it.
    inline void addLowShelfCutDb(std::vector<float>& specDb, float cutoffHz, float depthDb)
    {
        const float binHz = static_cast<float>(kSampleRate) / kFFTSize;
        for (int i = 0; i < kNumBins; ++i)
        {
            const float f = static_cast<float>(i) * binHz;
            if (f < cutoffHz)
            {
                const float oct = std::log2(juce::jmax(1.0f, cutoffHz) / juce::jmax(20.0f, f));
                specDb[static_cast<size_t>(i)] -= depthDb * juce::jmin(1.0f, oct);
            }
        }
    }

    inline void addNoiseDb(std::vector<float>& specDb, std::mt19937& rng, float sigmaDb = 1.0f)
    {
        std::normal_distribution<float> n(0.0f, sigmaDb);
        for (auto& v : specDb)
            v += n(rng);
    }

    inline std::vector<float> toLinear(const std::vector<float>& specDb)
    {
        std::vector<float> lin(specDb.size());
        for (size_t i = 0; i < specDb.size(); ++i)
            lin[i] = dbToLinearMag(juce::jlimit(-120.0f, 12.0f, specDb[i]));
        return lin;
    }

    // Returns a LINEAR-magnitude fixture for one class variation.
    inline std::vector<float> make(HClass c, std::mt19937& rng)
    {
        auto U = [&rng](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };

        std::vector<float> db(static_cast<size_t>(kNumBins), kBaselineDb);
        addPinkTiltDb(db, U(kEvalTiltMinSlope, kEvalTiltMaxSlope)); // steep extrapolation band, NATURAL

        switch (c)
        {
            case HClass::Resonance: addGaussianPeakDb(db, U(250.0f, 4000.0f), U(14.0f, 20.0f), 0.04f); break; // narrow
            case HClass::Harshness: addGaussianPeakDb(db, U(2500.0f, 5000.0f), U(10.0f, 16.0f), 0.12f); break;
            case HClass::Muddiness: addGaussianPeakDb(db, U(180.0f, 400.0f),  U(10.0f, 14.0f), 0.25f); break; // broad
            case HClass::Sibilance: addGaussianPeakDb(db, U(5500.0f, 8500.0f), U(12.0f, 16.0f), 0.18f); break;
            case HClass::Boominess: addGaussianPeakDb(db, U(45.0f, 110.0f),   U(12.0f, 18.0f), 0.30f); break;
            case HClass::Boxyness:  addGaussianPeakDb(db, U(350.0f, 750.0f),  U(8.0f, 12.0f),  0.20f); break;
            case HClass::Thinness:  addLowShelfCutDb(db, U(150.0f, 300.0f),   U(8.0f, 14.0f));         break; // subtractive
        }

        addNoiseDb(db, rng, 1.0f);
        return toLinear(db);
    }

    inline std::vector<float> makeCleanWithTiltBand(std::mt19937& rng, float minSlope, float maxSlope)
    {
        std::vector<float> db(static_cast<size_t>(kNumBins), kBaselineDb);
        addPinkTiltDb(db, std::uniform_real_distribution<float>(minSlope, maxSlope)(rng));
        addNoiseDb(db, rng, 1.0f);
        return toLinear(db);
    }

    // Clean control: eval extrapolation band + noise, NO injected problem.
    inline std::vector<float> makeClean(std::mt19937& rng)
    {
        return makeCleanWithTiltBand(rng, kEvalTiltMinSlope, kEvalTiltMaxSlope);
    }

    // Clean control in the P4-M2a training interpolation band. This is NOT the
    // held-out score; it distinguishes "model learned the new training regime"
    // from "model extrapolates to the frozen held-out band."
    inline std::vector<float> makeCleanTrainBand(std::mt19937& rng)
    {
        return makeCleanWithTiltBand(rng, kTrainTiltMinSlope, kTrainTiltMaxSlope);
    }

} // namespace aieq_test::heldout

// -----------------------------------------------------------------------------
// A1b — non-perturbative Product Gate annotations.
// These wrappers do NOT change make()/makeClean() and add no RNG draws.
// Positive fixtures certify only their primary class. Other product classes are
// Unknown rather than being invented as negatives. Frozen clean fixtures certify
// Negative for every product-facing class.
// -----------------------------------------------------------------------------
namespace aieq_test::heldout
{
enum class ProductClass : std::uint8_t
{
    Resonance,
    Harshness,
    Muddiness,
    Sibilance,
    Boominess,
    Boxyness,
    Thinness,
    DullSound,
    Count
};

constexpr std::size_t kProductClassCount = static_cast<std::size_t>(ProductClass::Count);

enum class TruthState : std::uint8_t
{
    Unknown = 0,
    Negative,
    Positive
};

struct TruthMask
{
    std::array<TruthState, kProductClassCount> values {};

    TruthMask() noexcept
    {
        values.fill(TruthState::Unknown);
    }

    TruthState get(ProductClass c) const noexcept
    {
        return values[static_cast<std::size_t>(c)];
    }

    void set(ProductClass c, TruthState value) noexcept
    {
        values[static_cast<std::size_t>(c)] = value;
    }

    static TruthMask primaryOnly(ProductClass primary) noexcept
    {
        TruthMask mask;
        mask.set(primary, TruthState::Positive);
        return mask;
    }

    static TruthMask allNegative() noexcept
    {
        TruthMask mask;
        mask.values.fill(TruthState::Negative);
        return mask;
    }
};

struct AnnotatedFixture
{
    std::vector<float> spectrum;
    std::optional<ProductClass> primaryClass;
    TruthMask truth;
    std::optional<float> targetFrequencyHz; // deliberately N/A in A1b; do not perturb legacy RNG contract
    bool isClean = false;
};

inline HClass toHeldoutClass(ProductClass c)
{
    switch (c)
    {
        case ProductClass::Resonance: return HClass::Resonance;
        case ProductClass::Harshness: return HClass::Harshness;
        case ProductClass::Muddiness: return HClass::Muddiness;
        case ProductClass::Sibilance: return HClass::Sibilance;
        case ProductClass::Boominess: return HClass::Boominess;
        case ProductClass::Boxyness:  return HClass::Boxyness;
        case ProductClass::Thinness:  return HClass::Thinness;
        case ProductClass::DullSound:
        case ProductClass::Count:     break;
    }
    jassertfalse;
    return HClass::Resonance;
}

inline constexpr std::array<ProductClass, 7> kLegacyV1SupportedClasses {{
    ProductClass::Resonance,
    ProductClass::Harshness,
    ProductClass::Muddiness,
    ProductClass::Sibilance,
    ProductClass::Boominess,
    ProductClass::Boxyness,
    ProductClass::Thinness
}};

inline std::vector<AnnotatedFixture> generateProductGatePositiveFixtures()
{
    std::vector<AnnotatedFixture> out;
    out.reserve(kLegacyV1SupportedClasses.size() * static_cast<std::size_t>(kVariations));

    // This reproduces AIAccuracyTest_PipelineAllClasses: ONE RNG stream across
    // all seven class groups, so class order and RNG consumption stay frozen.
    std::mt19937 rng(20260617);
    for (const auto c : kLegacyV1SupportedClasses)
    {
        for (int v = 0; v < kVariations; ++v)
        {
            AnnotatedFixture fixture;
            fixture.spectrum = make(toHeldoutClass(c), rng);
            fixture.primaryClass = c;
            fixture.truth = TruthMask::primaryOnly(c);
            out.push_back(std::move(fixture));
        }
    }
    return out;
}

inline std::vector<AnnotatedFixture> generateProductGateCleanFixtures()
{
    std::vector<AnnotatedFixture> out;
    out.reserve(static_cast<std::size_t>(kVariations));

    // Frozen held-out clean seed used by the existing pipeline-all-classes witness.
    std::mt19937 rng(20260618);
    for (int v = 0; v < kVariations; ++v)
    {
        AnnotatedFixture fixture;
        fixture.spectrum = makeClean(rng);
        fixture.truth = TruthMask::allNegative();
        fixture.isClean = true;
        out.push_back(std::move(fixture));
    }
    return out;
}

inline std::uint64_t fnv1aAppend(std::uint64_t hash, const void* data, std::size_t bytes) noexcept
{
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < bytes; ++i)
    {
        hash ^= static_cast<std::uint64_t>(p[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::uint64_t computeProductGateRuntimeFingerprint()
{
    // Runtime/toolchain fingerprint only. std::*_distribution does not promise
    // identical generated floats across standard-library implementations, so this
    // value must NOT be treated as a cross-platform immutable constant.
    std::uint64_t hash = 1469598103934665603ull;

    const auto appendFixture = [&hash](const AnnotatedFixture& fixture)
    {
        const std::uint8_t clean = fixture.isClean ? 1u : 0u;
        hash = fnv1aAppend(hash, &clean, sizeof(clean));

        const std::uint8_t hasPrimary = fixture.primaryClass.has_value() ? 1u : 0u;
        hash = fnv1aAppend(hash, &hasPrimary, sizeof(hasPrimary));
        if (fixture.primaryClass.has_value())
        {
            const auto primary = static_cast<std::uint8_t>(*fixture.primaryClass);
            hash = fnv1aAppend(hash, &primary, sizeof(primary));
        }

        for (const auto state : fixture.truth.values)
        {
            const auto v = static_cast<std::uint8_t>(state);
            hash = fnv1aAppend(hash, &v, sizeof(v));
        }

        for (const float value : fixture.spectrum)
        {
            const auto bits = std::bit_cast<std::uint32_t>(value);
            hash = fnv1aAppend(hash, &bits, sizeof(bits));
        }
    };

    for (const auto& fixture : generateProductGatePositiveFixtures())
        appendFixture(fixture);
    for (const auto& fixture : generateProductGateCleanFixtures())
        appendFixture(fixture);

    return hash;
}

} // namespace aieq_test::heldout
