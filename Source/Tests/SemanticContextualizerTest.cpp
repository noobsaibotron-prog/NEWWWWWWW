#if JUCE_UNIT_TESTS

#include <juce_core/juce_core.h>
#include "../AI/SemanticContextualizer.h"
#include "../AI/SemanticTargetBuilder.h"
#include "../AI/SparseParametricFitter.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
using namespace AIEQPerceptual;

std::vector<float> makeSemanticContextCenters()
{
    std::vector<float> out;
    const float step = std::pow(2.0f, 1.0f / 12.0f);
    for (float f = 20.0f; f < 24000.0f / step; f *= step)
        out.push_back(f);
    return out;
}

std::vector<float> makeSemanticContextProfile(const std::vector<float>& centers,
                                              float tiltDbPerOctave,
                                              float lowMidBoost,
                                              float airBoost)
{
    std::vector<float> out;
    for (float f : centers)
    {
        float db = -50.0f + tiltDbPerOctave * std::log2(f / 1000.0f);
        db += lowMidBoost * std::exp(-0.5f * std::pow(std::log2(f / 300.0f) / 0.70f, 2.0f));
        db += airBoost * 0.5f * (1.0f + std::tanh(std::log2(f / 9000.0f) / 0.55f));
        out.push_back(std::clamp(db, -120.0f, 12.0f));
    }
    return out;
}

SpectralContext semanticContext(const std::vector<float>& centers,
                                const std::vector<float>& profile)
{
    SpectralContextAccumulator a;
    a.prepare(centers);
    for (int i = 0; i < 24; ++i)
        a.pushFrame(profile, true);
    return a.snapshot();
}

SemanticIntent oneGoal(SemanticDimension d, float amount, const char* phrase)
{
    SemanticIntent i;
    i.hasRecognizedContent = true;
    i.confidence = 1.0f;
    i.goals.push_back({ d, amount, 1.0f, phrase });
    return i;
}

float maxPositiveTarget(const PerceptualTarget& target)
{
    float m = 0.0f;
    for (const auto& p : target.points) m = std::max(m, p.deltaDb);
    return m;
}
}

class SemanticContextualizerTest final : public juce::UnitTest
{
public:
    SemanticContextualizerTest()
        : juce::UnitTest("Semantic Contextualizer", "AI-Diag") {}

    void runTest() override
    {
        const auto centers = makeSemanticContextCenters();
        const auto dark = semanticContext(centers,
            makeSemanticContextProfile(centers, -2.0f, 0.0f, -2.0f));
        const auto bright = semanticContext(centers,
            makeSemanticContextProfile(centers, 1.2f, 0.0f, 3.0f));
        const auto leanLowMid = semanticContext(centers,
            makeSemanticContextProfile(centers, -0.4f, -3.0f, 0.0f));
        const auto fullLowMid = semanticContext(centers,
            makeSemanticContextProfile(centers, -0.4f, 4.0f, 0.0f));

        SemanticContextualizer contextualizer;

        beginTest("Same brightness intent produces a larger move on a dark source");
        {
            const auto intent = oneGoal(SemanticDimension::Brightness, 0.8f, "more air");
            const auto darkResult = contextualizer.contextualize(intent, dark);
            const auto brightResult = contextualizer.contextualize(intent, bright);
            expect(darkResult.intent.goals[0].amount > brightResult.intent.goals[0].amount + 0.10f);
            expect(darkResult.intent.goals[0].amount > 0.0f);
            expect(brightResult.intent.goals[0].amount > 0.0f,
                   "context must never flip the requested semantic direction");
        }

        beginTest("Already-full low mids reduce a positive warmth move");
        {
            const auto intent = oneGoal(SemanticDimension::Warmth, 0.8f, "warmer");
            const auto lean = contextualizer.contextualize(intent, leanLowMid);
            const auto full = contextualizer.contextualize(intent, fullLowMid);
            expect(lean.intent.goals[0].amount > full.intent.goals[0].amount + 0.05f);
        }

        beginTest("Context never edits semantic constraints");
        {
            auto intent = oneGoal(SemanticDimension::Warmth, 0.8f, "warmer");
            intent.constraints.push_back({ SemanticDimension::Clarity,
                                           SemanticConstraintKind::AvoidDirection,
                                           -1, 1.0f, "without mud" });
            const auto result = contextualizer.contextualize(intent, fullLowMid);
            expectEquals(static_cast<int>(result.intent.constraints.size()), 1);
            expect(result.intent.constraints[0].dimension == intent.constraints[0].dimension);
            expect(result.intent.constraints[0].kind == intent.constraints[0].kind);
            expectEquals(result.intent.constraints[0].direction, intent.constraints[0].direction);
            expect(result.intent.constraints[0].sourcePhrase == intent.constraints[0].sourcePhrase);
        }

        beginTest("Static spectral context does not invent Punch/Tightness/Smoothness semantics");
        {
            for (auto dimension : { SemanticDimension::Punch,
                                    SemanticDimension::Tightness,
                                    SemanticDimension::Smoothness })
            {
                const auto intent = oneGoal(dimension, 0.75f, "requested");
                const auto result = contextualizer.contextualize(intent, bright);
                expectWithinAbsoluteError(result.intent.goals[0].amount, 0.75f, 1.0e-6f);
            }
        }

        beginTest("Low-confidence context bypasses contextualization exactly");
        {
            auto low = dark;
            low.confidence = 0.0f;
            const auto intent = oneGoal(SemanticDimension::Brightness, 0.8f, "more air");
            const auto result = contextualizer.contextualize(intent, low);
            expectWithinAbsoluteError(result.intent.goals[0].amount, intent.goals[0].amount, 1.0e-6f);
            expect(!result.contextApplied);
        }

        beginTest("Context-adjusted intent materially changes target while preserving constraints");
        {
            SemanticTargetBuilder builder;
            SparseParametricFitter fitter;

            const auto airIntent = oneGoal(SemanticDimension::Brightness, 0.8f, "more air");
            const auto darkIntent = contextualizer.contextualize(airIntent, dark);
            const auto brightIntent = contextualizer.contextualize(airIntent, bright);
            const auto darkTarget = builder.build(darkIntent.intent, 48000.0);
            const auto brightTarget = builder.build(brightIntent.intent, 48000.0);
            // The 0.50 dB margin was tied to the old axis scale, where a
            // pink-tilted source railed at -1.00 and the gap between "dark" and
            // "bright" was therefore maximal by construction. T5.5.2 measures a
            // tilt-corrected residual over a 9 dB full scale, chosen so the
            // largest holdback measured on real material (+6.8 dB) stays off
            // the clamp, so the same fixtures now separate by less. What must
            // hold is the ordering and a real, not-merely-numerical difference.
            logMessage("  dark target peak=" + juce::String(maxPositiveTarget(darkTarget), 3)
                       + " dB   bright=" + juce::String(maxPositiveTarget(brightTarget), 3) + " dB");
            expect(maxPositiveTarget(darkTarget) > maxPositiveTarget(brightTarget) + 0.10f,
                   "a dark source and a bright source receive materially the same brightness "
                   "target, so context is not ranking them");

            auto warmIntent = oneGoal(SemanticDimension::Warmth, 0.8f, "warmer");
            warmIntent.constraints.push_back({ SemanticDimension::Clarity,
                                               SemanticConstraintKind::AvoidDirection,
                                               -1, 1.0f, "without mud" });
            const auto fullIntent = contextualizer.contextualize(warmIntent, fullLowMid);
            const auto fullTarget = builder.build(fullIntent.intent, 48000.0);
            const auto fit = fitter.fit(fullTarget, 48000.0);
            expect(fit.valid);
            expect(fit.maxResponseBoundViolationDb <= 1.0e-3f);
        }
    }
};

static SemanticContextualizerTest gSemanticContextualizerTest;

#endif
