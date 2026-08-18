#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SpectralContext.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
using namespace AIEQPerceptual;

std::vector<float> makeContextBandCenters(float nyquist = 24000.0f)
{
    std::vector<float> out;
    const float step = std::pow(2.0f, 1.0f / 12.0f);
    for (float f = 20.0f; f < nyquist / step; f *= step)
        out.push_back(f);
    return out;
}

std::vector<float> makeContextProfile(const std::vector<float>& centers,
                                      float baseDb,
                                      float tiltDbPerOctave,
                                      float lowMidBoost = 0.0f,
                                      float airBoost = 0.0f)
{
    std::vector<float> out;
    out.reserve(centers.size());
    for (float f : centers)
    {
        float db = baseDb + tiltDbPerOctave * std::log2(f / 1000.0f);
        db += lowMidBoost * std::exp(-0.5f * std::pow(std::log2(f / 300.0f) / 0.70f, 2.0f));
        db += airBoost * 0.5f * (1.0f + std::tanh(std::log2(f / 9000.0f) / 0.55f));
        out.push_back(std::clamp(db, -120.0f, 12.0f));
    }
    return out;
}

SpectralContext makeStableContext(const std::vector<float>& centers,
                                  const std::vector<float>& profile,
                                  float jitterDb = 0.0f)
{
    SpectralContextAccumulator accumulator;
    accumulator.prepare(centers);
    for (int frame = 0; frame < 24; ++frame)
    {
        auto p = profile;
        if (jitterDb > 0.0f)
        {
            for (std::size_t i = 0; i < p.size(); ++i)
            {
                const float phase = static_cast<float>((frame * 13 + static_cast<int>(i) * 7) % 17) / 17.0f;
                p[i] += jitterDb * std::sin(phase * juce::MathConstants<float>::twoPi);
            }
        }
        accumulator.pushFrame(p, true);
    }
    return accumulator.snapshot();
}
} // namespace

class SpectralContextTest final : public juce::UnitTest
{
public:
    SpectralContextTest() : juce::UnitTest("Spectral Context", "AI-Diag") {}

    void runTest() override
    {
        const auto centers = makeContextBandCenters();

        beginTest("Tonal descriptors are invariant to global level shift");
        {
            const auto a = makeStableContext(centers,
                makeContextProfile(centers, -48.0f, -0.8f, 1.5f, 0.7f));
            const auto b = makeStableContext(centers,
                makeContextProfile(centers, -66.0f, -0.8f, 1.5f, 0.7f));

            expect(a.valid && b.valid);
            for (std::size_t i = 0; i < kSpectralRegionCount; ++i)
                expectWithinAbsoluteError(a.regionRelativeDb[i], b.regionRelativeDb[i], 1.0e-4f);
            expectWithinAbsoluteError(a.spectralTiltDbPerOctave, b.spectralTiltDbPerOctave, 1.0e-4f);
            expectWithinAbsoluteError(a.perceptualCentroidHz, b.perceptualCentroidHz, 0.05f);
            expectWithinAbsoluteError(a.perceptualRolloffHz, b.perceptualRolloffHz, 0.05f);
            expect(std::abs(a.sourceLevelDb - b.sourceLevelDb) > 15.0f,
                   "diagnostic absolute source level should still record the level difference");
        }

        beginTest("Dark and bright sources are ordered consistently");
        {
            const auto dark = makeStableContext(centers,
                makeContextProfile(centers, -50.0f, -2.2f, 0.0f, -2.0f));
            const auto bright = makeStableContext(centers,
                makeContextProfile(centers, -50.0f, 1.4f, 0.0f, 3.0f));

            expect(bright.spectralTiltDbPerOctave > dark.spectralTiltDbPerOctave + 2.0f);
            expect(bright.perceptualCentroidHz > dark.perceptualCentroidHz * 1.4f);
            expect(bright.region(SpectralRegion::Air) > dark.region(SpectralRegion::Air) + 4.0f);
        }

        beginTest("Low-mid fullness is distinguishable");
        {
            const auto lean = makeStableContext(centers,
                makeContextProfile(centers, -50.0f, -0.5f, -3.5f));
            const auto full = makeStableContext(centers,
                makeContextProfile(centers, -50.0f, -0.5f, 4.0f));
            expect(full.region(SpectralRegion::LowMid) > lean.region(SpectralRegion::LowMid) + 4.0f);
        }

        beginTest("Near silence fails closed through confidence");
        {
            const auto silence = makeStableContext(centers,
                makeContextProfile(centers, -116.0f, 0.0f));
            expect(silence.valid,
                   "silence is a structurally valid shape and should fail closed via confidence");
            expect(silence.confidence < 0.10f);
        }

        beginTest("Temporal jitter does not destabilize tonal context");
        {
            const auto base = makeContextProfile(centers, -48.0f, -0.8f, 1.5f, 0.7f);
            const auto steady = makeStableContext(centers, base);
            const auto jittered = makeStableContext(centers, base, 1.25f);
            expectWithinAbsoluteError(steady.spectralTiltDbPerOctave,
                                      jittered.spectralTiltDbPerOctave, 0.12f);
            expectWithinAbsoluteError(steady.region(SpectralRegion::Air),
                                      jittered.region(SpectralRegion::Air), 0.15f);
            expect(jittered.confidence > 0.70f);
        }

        beginTest("Malformed inputs fail closed without NaN");
        {
            SpectralContextBuilder builder;
            std::vector<float> badCenters { 20.0f, 40.0f, 30.0f, 80.0f, 160.0f, 320.0f, 640.0f, 1280.0f };
            std::vector<float> values(badCenters.size(), -50.0f);
            const auto invalid = builder.build(badCenters, values, {}, 24, 1.0f);
            expect(!invalid.valid);
            expect(std::isfinite(invalid.confidence));
        }
    }
};

static SpectralContextTest gSpectralContextTest;

#endif
