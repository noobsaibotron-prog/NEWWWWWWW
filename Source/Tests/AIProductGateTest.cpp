#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "AI/AIEngine.h"
#include "AI/MLEngine.h"
#include "Tests/Support/AIHeldoutFixtures.h"

namespace
{
namespace heldout = aieq_test::heldout;

constexpr double kSampleRate = heldout::kSampleRate;
constexpr int kBlockSize = 512;
constexpr int kMinRecall = 8;
constexpr int kMaxCleanFp = 0;
constexpr int kMaxCrossFp = 1;
constexpr juce::int64 kExpectedModelBytes = 96488;
constexpr const char* kExpectedModelChecksum = "9d322f2466049202";
constexpr std::array<float, 4> kSensitivities {{ 0.25f, 0.50f, 0.75f, 1.00f }};

std::vector<float> linearToDb(const std::vector<float>& linear)
{
    std::vector<float> db(linear.size());
    for (std::size_t i = 0; i < linear.size(); ++i)
        db[i] = linear[i] > 1.0e-10f
              ? juce::jlimit(-120.0f, 12.0f, 20.0f * std::log10(linear[i]))
              : -120.0f;
    return db;
}

constexpr std::size_t classIndex(heldout::ProductClass c) noexcept
{
    return static_cast<std::size_t>(c);
}

const char* className(heldout::ProductClass c) noexcept
{
    switch (c)
    {
        case heldout::ProductClass::Resonance: return "Resonance";
        case heldout::ProductClass::Harshness: return "Harshness";
        case heldout::ProductClass::Muddiness: return "Muddiness";
        case heldout::ProductClass::Sibilance: return "Sibilance";
        case heldout::ProductClass::Boominess: return "Boominess";
        case heldout::ProductClass::Boxyness:  return "Boxyness";
        case heldout::ProductClass::Thinness:  return "Thinness";
        case heldout::ProductClass::DullSound: return "DullSound";
        case heldout::ProductClass::Count:     break;
    }
    return "Unknown";
}

std::optional<heldout::ProductClass> toProductClass(AIEngine::ProblemType t) noexcept
{
    switch (t)
    {
        case AIEngine::ProblemType::Resonance:  return heldout::ProductClass::Resonance;
        case AIEngine::ProblemType::Harshness:  return heldout::ProductClass::Harshness;
        case AIEngine::ProblemType::Muddiness:  return heldout::ProductClass::Muddiness;
        case AIEngine::ProblemType::Sibilance:  return heldout::ProductClass::Sibilance;
        case AIEngine::ProblemType::LowEndBoom: return heldout::ProductClass::Boominess;
        case AIEngine::ProblemType::Boxyness:   return heldout::ProductClass::Boxyness;
        case AIEngine::ProblemType::ThinSound:  return heldout::ProductClass::Thinness;
        case AIEngine::ProblemType::DullSound:  return heldout::ProductClass::DullSound;
        case AIEngine::ProblemType::None:       break;
    }
    return std::nullopt;
}

struct FixtureObservation
{
    std::array<bool, heldout::kProductClassCount> detected {};
    std::array<std::optional<float>, heldout::kProductClassCount> firstFrequencyHz {};
};

struct ClassMetrics
{
    int positive = 0;
    int tp = 0;
    int fn = 0;
    int cleanNegative = 0;
    int cleanFp = 0;
    int crossNegative = 0;
    int crossFp = 0;
    int localizationCount = 0;
    double localizationAbsHzSum = 0.0;
    double localizationRelativeSum = 0.0;
};

struct SensitivityResult
{
    float sensitivity = 0.5f;
    std::array<ClassMetrics, heldout::kProductClassCount> metrics {};
    std::array<std::array<int, heldout::kProductClassCount>, heldout::kProductClassCount> crossDetection {};
};

juce::String ratioString(int n, int d)
{
    return juce::String(n) + "/" + juce::String(d);
}

juce::String percentString(double v)
{
    if (! std::isfinite(v))
        return "N/A";
    return juce::String(v * 100.0, 1) + "%";
}

juce::String hex64(std::uint64_t value)
{
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << value;
    return juce::String(ss.str());
}

} // namespace

class AIProductGateTest final : public juce::UnitTest
{
public:
    AIProductGateTest()
        : juce::UnitTest("AI Product Gate — frozen heldout measurement", "AI-Diag") {}

    void runTest() override
    {
        beginTest("seed22 measurement: frozen spectra -> MLOnly -> production detector/veto path");

        const auto shippedModel = juce::File(__FILE__).getParentDirectory()
            .getParentDirectory().getParentDirectory()
            .getChildFile("Resources/Models/ml_weights.bin");

        expect(shippedModel.existsAsFile(), "shipped Resources/Models/ml_weights.bin missing");
        if (! shippedModel.existsAsFile())
            return;
        expectEquals(shippedModel.getSize(), kExpectedModelBytes, "shipped model size drifted");

        const auto positives = heldout::generateProductGatePositiveFixtures();
        const auto clean = heldout::generateProductGateCleanFixtures();

        expectEquals(static_cast<int>(positives.size()), 7 * heldout::kVariations,
                     "frozen positive fixture count drifted");
        expectEquals(static_cast<int>(clean.size()), heldout::kVariations,
                     "frozen clean fixture count drifted");
        validateFixtureAnnotations(positives, clean);

        const auto fingerprintA = heldout::computeProductGateRuntimeFingerprint();
        const auto fingerprintB = heldout::computeProductGateRuntimeFingerprint();
        expect(fingerprintA == fingerprintB,
               "heldout runtime fingerprint is not deterministic on this toolchain");

        auto probe = makeProductEngine(0.50f, shippedModel);
        if (! probe)
            return;

        auto& ml = probe->getMLEngineForTest();
        expectEquals(ml.getLoadedWeightsBytes(), kExpectedModelBytes,
                     "Product Gate did not load the pinned shipping blob size");
        expectEquals(ml.getLoadedWeightsChecksum(), juce::String(kExpectedModelChecksum),
                     "Product Gate did not load the pinned seed22 checksum");

        logMessage("");
        logMessage("  PRODUCT GATE IDENTITY");
        logMessage("  model path:       " + ml.getLoadedWeightsPath());
        logMessage("  model bytes:      " + juce::String(ml.getLoadedWeightsBytes()));
        logMessage("  model fnv:        " + ml.getLoadedWeightsChecksum());
        logMessage("  problem schema:   " + ml.getLoadedProblemSchema());
        logMessage("  backend status:   " + AIEngine::getMLBackendStatusName(probe->getMLBackendStatus()));
        logMessage("  backend mode:     MLOnly");
        logMessage("  source profile:   Generic");
        logMessage("  fixture count:    " + juce::String(static_cast<int>(positives.size()))
                   + " positive + " + juce::String(static_cast<int>(clean.size())) + " clean");
        logMessage("  fixture runtime fingerprint (platform-scoped): " + hex64(fingerprintA));
        logMessage("  NOTE: std::*_distribution output is not a cross-standard-library bit contract;");
        logMessage("        pin this generated-fixture fingerprint only on the authoritative Mac toolchain.");

        std::array<SensitivityResult, kSensitivities.size()> firstRun {};
        std::array<SensitivityResult, kSensitivities.size()> secondRun {};
        for (std::size_t i = 0; i < kSensitivities.size(); ++i)
            firstRun[i] = measureSensitivity(kSensitivities[i], shippedModel, positives, clean);
        for (std::size_t i = 0; i < kSensitivities.size(); ++i)
            secondRun[i] = measureSensitivity(kSensitivities[i], shippedModel, positives, clean);

        expect(resultsEquivalent(firstRun, secondRun),
               "Product Gate metrics changed across two identical measurement runs");

        for (const auto& result : firstRun)
            printResult(result);

        logMessage("");
        logMessage("  PRODUCT-GATE INTERPRETATION");
        logMessage("  Candidate thresholds are measurement labels only: recall >= 8/12, clean FP = 0,");
        logMessage("  certified cross FP <= 1. They DO NOT fail this unit test and DO NOT alter runtime policy.");
        logMessage("  Positive fixtures certify only their primary class; all other class labels are Unknown.");
        logMessage("  Cross detections are diagnostic, not false positives, unless TruthState::Negative is certified.");
        logMessage("  DullSound is UNSUPPORTED by the shipped legacy-v1 blob.");
        logMessage("  Localization remains N/A in A2; do not perturb the frozen generator merely to recover metadata.");
        logMessage("  Real-WAV full-front-end sanity remains the separate AICorpus/LiveAIAnalysisPipeline evidence layer.");
    }

private:
    std::unique_ptr<AIEngine> makeProductEngine(float sensitivity, const juce::File& shippedModel)
    {
        auto ai = std::make_unique<AIEngine>();
        const bool loaded = ai->setCustomMLWeightsPathForTests(shippedModel);
        expect(loaded, "AIEngine rejected shipped ml_weights.bin");
        if (! loaded)
            return nullptr;

        ai->prepare(kSampleRate, kBlockSize);
        ai->setEnabled(true);
        ai->setSensitivity(sensitivity);
        ai->setSourceProfile(AIEngine::SourceProfile::Generic);
        ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);

        expect(ai->getMLBackendStatus() == AIEngine::MLBackendStatus::Active,
               "ML backend is not Active; Product Gate would be invalid");
        expect(ai->getDetectionBackendMode() == AIEngine::DetectionBackendMode::MLOnly,
               "Product Gate backend mode drifted away from MLOnly");
        expect(ai->isUsingMLDetection(),
               "MLOnly requested but AIEngine is not actually using ML detection");

        if (ai->getMLBackendStatus() != AIEngine::MLBackendStatus::Active
            || ai->getDetectionBackendMode() != AIEngine::DetectionBackendMode::MLOnly
            || ! ai->isUsingMLDetection())
            return nullptr;

        return ai;
    }

    static FixtureObservation observeFixture(AIEngine& ai, const heldout::AnnotatedFixture& fixture)
    {
        FixtureObservation observation;
        ai.analyzeSpectrum(linearToDb(fixture.spectrum), true);
        for (const auto& correction : ai.getPendingCorrections())
        {
            const auto pc = toProductClass(correction.type);
            if (! pc.has_value())
                continue;
            const auto idx = classIndex(*pc);
            if (! observation.detected[idx])
            {
                observation.detected[idx] = true;
                observation.firstFrequencyHz[idx] = correction.frequency;
            }
        }
        return observation;
    }

    SensitivityResult measureSensitivity(float sensitivity,
                                         const juce::File& shippedModel,
                                         const std::vector<heldout::AnnotatedFixture>& positives,
                                         const std::vector<heldout::AnnotatedFixture>& clean)
    {
        SensitivityResult result;
        result.sensitivity = sensitivity;

        for (const auto& fixture : positives)
        {
            auto ai = makeProductEngine(sensitivity, shippedModel); // fresh temporal state per fixture
            if (! ai || ! fixture.primaryClass.has_value())
                continue;

            const auto observed = observeFixture(*ai, fixture);
            const auto primary = *fixture.primaryClass;
            const auto primaryIdx = classIndex(primary);
            auto& primaryMetrics = result.metrics[primaryIdx];
            ++primaryMetrics.positive;

            if (observed.detected[primaryIdx])
            {
                ++primaryMetrics.tp;
                if (fixture.targetFrequencyHz.has_value() && observed.firstFrequencyHz[primaryIdx].has_value())
                {
                    const double expected = static_cast<double>(*fixture.targetFrequencyHz);
                    const double actual = static_cast<double>(*observed.firstFrequencyHz[primaryIdx]);
                    const double absError = std::abs(actual - expected);
                    ++primaryMetrics.localizationCount;
                    primaryMetrics.localizationAbsHzSum += absError;
                    if (expected > 0.0)
                        primaryMetrics.localizationRelativeSum += absError / expected;
                }
            }
            else
            {
                ++primaryMetrics.fn;
            }

            for (std::size_t predicted = 0; predicted < heldout::kProductClassCount; ++predicted)
            {
                if (observed.detected[predicted])
                    ++result.crossDetection[primaryIdx][predicted];

                if (predicted == primaryIdx)
                    continue;

                const auto predictedClass = static_cast<heldout::ProductClass>(predicted);
                auto& metrics = result.metrics[predicted];
                if (fixture.truth.get(predictedClass) == heldout::TruthState::Negative)
                {
                    ++metrics.crossNegative;
                    if (observed.detected[predicted])
                        ++metrics.crossFp;
                }
            }
        }

        for (const auto& fixture : clean)
        {
            auto ai = makeProductEngine(sensitivity, shippedModel);
            if (! ai)
                continue;
            const auto observed = observeFixture(*ai, fixture);
            for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
            {
                const auto pc = static_cast<heldout::ProductClass>(c);
                if (fixture.truth.get(pc) != heldout::TruthState::Negative)
                    continue;
                auto& metrics = result.metrics[c];
                ++metrics.cleanNegative;
                if (observed.detected[c])
                    ++metrics.cleanFp;
            }
        }
        return result;
    }

    void validateFixtureAnnotations(const std::vector<heldout::AnnotatedFixture>& positives,
                                    const std::vector<heldout::AnnotatedFixture>& clean)
    {
        int positiveTruthCount = 0;
        int accidentalCrossNegatives = 0;
        for (const auto& fixture : positives)
        {
            expect(fixture.primaryClass.has_value(), "positive fixture missing primary class annotation");
            expect(! fixture.isClean, "positive fixture marked clean");
            if (! fixture.primaryClass.has_value())
                continue;
            const auto primary = *fixture.primaryClass;
            expect(fixture.truth.get(primary) == heldout::TruthState::Positive,
                   "positive fixture primary truth is not Positive");
            if (fixture.truth.get(primary) == heldout::TruthState::Positive)
                ++positiveTruthCount;

            for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
            {
                const auto pc = static_cast<heldout::ProductClass>(c);
                if (pc != primary && fixture.truth.get(pc) == heldout::TruthState::Negative)
                    ++accidentalCrossNegatives;
            }
        }

        expectEquals(positiveTruthCount, static_cast<int>(positives.size()),
                     "not every positive fixture has its primary positive truth");
        expectEquals(accidentalCrossNegatives, 0,
                     "A1b unexpectedly certified cross-class negatives; review corpus semantics first");

        int cleanCertifiedNegatives = 0;
        for (const auto& fixture : clean)
        {
            expect(fixture.isClean, "clean fixture missing isClean annotation");
            expect(! fixture.primaryClass.has_value(), "clean fixture unexpectedly has a primary class");
            for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
                if (fixture.truth.get(static_cast<heldout::ProductClass>(c)) == heldout::TruthState::Negative)
                    ++cleanCertifiedNegatives;
        }
        expectEquals(cleanCertifiedNegatives,
                     static_cast<int>(clean.size() * heldout::kProductClassCount),
                     "clean heldout is not fully annotated as certified negative");
    }

    static bool resultsEquivalent(const std::array<SensitivityResult, kSensitivities.size()>& a,
                                  const std::array<SensitivityResult, kSensitivities.size()>& b)
    {
        for (std::size_t s = 0; s < a.size(); ++s)
        {
            if (a[s].sensitivity != b[s].sensitivity)
                return false;
            for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
            {
                const auto& x = a[s].metrics[c];
                const auto& y = b[s].metrics[c];
                if (x.positive != y.positive || x.tp != y.tp || x.fn != y.fn
                    || x.cleanNegative != y.cleanNegative || x.cleanFp != y.cleanFp
                    || x.crossNegative != y.crossNegative || x.crossFp != y.crossFp
                    || x.localizationCount != y.localizationCount
                    || x.localizationAbsHzSum != y.localizationAbsHzSum
                    || x.localizationRelativeSum != y.localizationRelativeSum)
                    return false;
                for (std::size_t p = 0; p < heldout::kProductClassCount; ++p)
                    if (a[s].crossDetection[c][p] != b[s].crossDetection[c][p])
                        return false;
            }
        }
        return true;
    }

    static juce::String candidateDecision(const ClassMetrics& m, bool unsupported)
    {
        if (unsupported) return "UNSUPPORTED";
        if (m.positive < heldout::kVariations) return "INVALID";
        if (m.tp < kMinRecall || m.cleanFp > kMaxCleanFp) return "FAIL";
        if (m.crossNegative == 0) return "REVIEW";
        if (m.crossFp > kMaxCrossFp) return "FAIL";
        return "PASS";
    }

    void printResult(const SensitivityResult& result)
    {
        logMessage("");
        logMessage("  ============================================================================");
        logMessage("  AI PRODUCT GATE — sensitivity " + juce::String(result.sensitivity, 2));
        logMessage("  class       | recall | FP-clean | FP-cross(cert) | Prec(cert) | localization | candidate");
        logMessage("  ------------+--------+----------+----------------+------------+--------------+-----------");

        for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
        {
            const auto pc = static_cast<heldout::ProductClass>(c);
            const bool unsupported = pc == heldout::ProductClass::DullSound;
            const auto& m = result.metrics[c];
            const double precisionDenom = static_cast<double>(m.tp + m.cleanFp + m.crossFp);
            const double precision = precisionDenom > 0.0
                                   ? static_cast<double>(m.tp) / precisionDenom
                                   : std::numeric_limits<double>::quiet_NaN();

            const juce::String recall = unsupported ? "--" : ratioString(m.tp, m.positive);
            const juce::String cleanFp = unsupported ? "--" : ratioString(m.cleanFp, m.cleanNegative);
            const juce::String crossFp = unsupported ? "--"
                : (m.crossNegative > 0 ? ratioString(m.crossFp, m.crossNegative) : "N/A");
            const juce::String prec = unsupported ? "--" : percentString(precision);
            const juce::String loc = unsupported ? "--"
                : (m.localizationCount > 0
                    ? juce::String(m.localizationAbsHzSum / m.localizationCount, 1) + "Hz mean"
                    : "N/A");

            logMessage("  " + juce::String(className(pc)).paddedRight(' ', 12)
                       + "| " + recall.paddedLeft(' ', 6)
                       + " | " + cleanFp.paddedLeft(' ', 8)
                       + " | " + crossFp.paddedLeft(' ', 14)
                       + " | " + prec.paddedLeft(' ', 10)
                       + " | " + loc.paddedLeft(' ', 12)
                       + " | " + candidateDecision(m, unsupported));
        }

        logMessage("");
        logMessage("  CROSS-DETECTION MATRIX (rows=fixture primary, cols=detected; binary per fixture)");
        logMessage("               Res Har Mud Sib Boo Box Thi Dul");
        for (std::size_t row = 0; row < heldout::kLegacyV1SupportedClasses.size(); ++row)
        {
            juce::String line = "  " + juce::String(className(static_cast<heldout::ProductClass>(row))).paddedRight(' ', 12);
            for (std::size_t col = 0; col < heldout::kProductClassCount; ++col)
                line += juce::String(result.crossDetection[row][col]).paddedLeft(' ', 4);
            logMessage(line);
        }
    }
};

static AIProductGateTest gAIProductGateTest;
