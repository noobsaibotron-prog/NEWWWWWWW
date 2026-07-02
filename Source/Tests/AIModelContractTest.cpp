#include "../AI/AIEngine.h"
#include "../AI/MLEngine.h"

#include <juce_core/juce_core.h>

#include <array>

namespace
{
class AIModelContractTest final : public juce::UnitTest
{
public:
    AIModelContractTest()
        : juce::UnitTest("AI Model Contract - product vs ML schema", "AI-Contract") {}

    void runTest() override
    {
        beginTest("Interim backend contract");
        {
            AIEngine ai;
            expect(ai.getDetectionBackendMode() == AIEngine::DetectionBackendMode::MLOnly,
                   "The interim product must default to MLOnly until heuristic Resonance Assist is explicitly revalidated.");
        }

        beginTest("Product problem enum contract");
        expectEquals(static_cast<int>(AIEngine::ProblemType::None), 0,
                     "None must remain the neutral/no-problem value.");
        expectEquals(static_cast<int>(AIEngine::ProblemType::DullSound), 8,
                     "DullSound is a product-facing class and must stay visible in the contract.");

        beginTest("MLEngine v1 schema contract");
        expectEquals(MLEngine::numProblemTypes, 8,
                     "MLEngine v1 has eight outputs. Adding full product coverage requires an explicit schema/model update.");
        expect(MLEngine::getProblemName(MLEngine::ProblemType::Clipping) == "Clipping",
               "MLEngine v1 output 7 is Clipping, not DullSound.");

        beginTest("Known full-coverage gap is explicit");
        logMessage("  KnownDebt/FutureModel: product DullSound is not represented in MLEngine v1.");
        logMessage("  KnownDebt/FutureModel: MLEngine Clipping is not a product Problem Panel class.");
        logMessage("  Full ML coverage requires schema/model v2 plus updated tests and scorecard.");
        expect(true, "Current interim gap recorded: this test must be updated with any schema/model v2 migration.");

        beginTest("Product-to-ML coverage matrix");
        struct CoverageRow
        {
            AIEngine::ProblemType productType;
            MLEngine::ProblemType mlType;
            const char* status;
        };

        constexpr std::array<CoverageRow, 8> coverage {{
            { AIEngine::ProblemType::Resonance,  MLEngine::ProblemType::Resonance,    "covered" },
            { AIEngine::ProblemType::Harshness,  MLEngine::ProblemType::Harshness,    "covered" },
            { AIEngine::ProblemType::Muddiness,  MLEngine::ProblemType::Muddiness,    "covered" },
            { AIEngine::ProblemType::Boxyness,   MLEngine::ProblemType::BoxyMidrange, "covered" },
            { AIEngine::ProblemType::Sibilance,  MLEngine::ProblemType::Sibilance,    "covered" },
            { AIEngine::ProblemType::LowEndBoom, MLEngine::ProblemType::Boominess,    "covered" },
            { AIEngine::ProblemType::ThinSound,  MLEngine::ProblemType::Thinness,     "covered" },
            { AIEngine::ProblemType::DullSound,  MLEngine::ProblemType::NumProblems,  "missing-in-ml-v1" }
        }};

        int covered = 0;
        int missing = 0;
        for (const auto& row : coverage)
        {
            const bool hasMLClass = row.mlType != MLEngine::ProblemType::NumProblems;
            covered += hasMLClass ? 1 : 0;
            missing += hasMLClass ? 0 : 1;

            const auto mlName = hasMLClass ? MLEngine::getProblemName(row.mlType)
                                           : juce::String("none");
            logMessage("  " + AIEngine::getProblemTypeName(row.productType)
                       + " -> " + mlName
                       + " [" + row.status + "]");
        }

        expectEquals(static_cast<int>(coverage.size()), 8,
                     "Every product-facing problem class must be present in the coverage matrix.");
        expectEquals(covered, 7,
                     "MLEngine v1 should currently cover exactly seven product classes.");
        expectEquals(missing, 1,
                     "MLEngine v1 should currently miss exactly DullSound.");
    }
};

static AIModelContractTest aiModelContractTest;
}
