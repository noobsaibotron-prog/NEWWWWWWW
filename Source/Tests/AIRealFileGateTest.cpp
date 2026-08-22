/**
 * AIRealFileGateTest -- measurement-only live-file Product Gate.
 *
 * Answers: what does the SHIPPED Assist detector fire on WAV files
 * (PFE + live cadence + persistence), class by class?
 *
 * This is the file/live ruler. The frozen synthetic Product Gate remains
 * the model ruler. Labels FAIL / REVIEW / PASS / INSUFFICIENT do not fail
 * this unit test and do not change runtime policy.
 *
 * Section A always runs the committed TestAssets/ai_corpus WAVs.
 * Section B runs only when AIEQ_REAL_GATE_DIR is set (class-named folders).
 * Do not point Section B at Semantic / L1 listening material.
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "AI/AIEngine.h"
#include "AI/MLEngine.h"
#include "Tests/Support/AIHeldoutFixtures.h"
#include "Tests/Support/LiveAIAnalysisPipeline.h"

namespace
{
namespace heldout = aieq_test::heldout;

constexpr int kBlockSize = 512;
constexpr int kMinRecall = 8;
constexpr int kMaxCleanFp = 0;
constexpr int kMaxCrossFp = 1;
constexpr int kCandidateMinN = 12;
constexpr juce::int64 kExpectedModelBytes = 96488;
constexpr const char* kExpectedModelChecksum = "9d322f2466049202";
constexpr std::array<float, 4> kSensitivities {{ 0.25f, 0.50f, 0.75f, 1.00f }};

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

juce::File repoRoot()
{
    return juce::File(__FILE__).getParentDirectory()
        .getParentDirectory().getParentDirectory();
}

juce::File shippedWeights()
{
    return repoRoot().getChildFile("Resources/Models/ml_weights.bin");
}

juce::File corpusDir()
{
#ifdef AIEQ_CORPUS_DIR
    return juce::File(AIEQ_CORPUS_DIR);
#else
    return repoRoot().getChildFile("TestAssets/ai_corpus");
#endif
}

bool isAudioFile(const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac";
}

bool loadAudioFile(const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (reader == nullptr)
        return false;

    out.setSize(static_cast<int>(reader->numChannels),
                static_cast<int>(reader->lengthInSamples));
    reader->read(&out, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    sampleRate = reader->sampleRate;
    return true;
}

struct ClipSpec
{
    juce::File file;
    std::optional<heldout::ProductClass> primaryClass;
    std::optional<float> targetFrequencyHz;
    bool isClean = false;
    juce::String note;
};

struct ClipObservation
{
    juce::String fileName;
    juce::String primaryLabel;
    int frames = 0;
    std::array<bool, heldout::kProductClassCount> detected {};
    std::array<std::optional<float>, heldout::kProductClassCount> firstFrequencyHz {};
    std::array<std::optional<float>, heldout::kProductClassCount> firstConfidence {};
    juce::String detections;
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
};

struct SensitivityResult
{
    float sensitivity = 0.5f;
    std::array<ClassMetrics, heldout::kProductClassCount> metrics {};
    std::array<std::array<int, heldout::kProductClassCount>, heldout::kProductClassCount> crossDetection {};
    std::vector<ClipObservation> files;
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

juce::String formatDetections(const ClipObservation& obs)
{
    juce::String out;
    for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
    {
        if (! obs.detected[c])
            continue;
        if (out.isNotEmpty())
            out += "; ";
        out += juce::String(className(static_cast<heldout::ProductClass>(c)));
        if (obs.firstFrequencyHz[c].has_value())
            out += "@" + juce::String(*obs.firstFrequencyHz[c], 0) + "Hz";
        if (obs.firstConfidence[c].has_value())
            out += " c=" + juce::String(*obs.firstConfidence[c], 2);
    }
    return out.isNotEmpty() ? out : juce::String("(none)");
}

std::optional<heldout::ProductClass> classFromFolderName(const juce::String& name)
{
    if (name.equalsIgnoreCase("Resonance")) return heldout::ProductClass::Resonance;
    if (name.equalsIgnoreCase("Harshness")) return heldout::ProductClass::Harshness;
    if (name.equalsIgnoreCase("Muddiness")) return heldout::ProductClass::Muddiness;
    if (name.equalsIgnoreCase("Sibilance")) return heldout::ProductClass::Sibilance;
    if (name.equalsIgnoreCase("Boominess")) return heldout::ProductClass::Boominess;
    if (name.equalsIgnoreCase("Boxyness") || name.equalsIgnoreCase("Boxiness"))
        return heldout::ProductClass::Boxyness;
    if (name.equalsIgnoreCase("Thinness")) return heldout::ProductClass::Thinness;
    if (name.equalsIgnoreCase("DullSound") || name.equalsIgnoreCase("Dull"))
        return heldout::ProductClass::DullSound;
    return std::nullopt;
}

void collectAudioOneLevel(const juce::File& dir, juce::Array<juce::File>& out)
{
    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false);
    for (const auto& f : files)
        if (isAudioFile(f))
            out.add(f);

    juce::Array<juce::File> subs;
    dir.findChildFiles(subs, juce::File::findDirectories, false);
    for (const auto& sub : subs)
    {
        juce::Array<juce::File> nested;
        sub.findChildFiles(nested, juce::File::findFiles, false);
        for (const auto& f : nested)
            if (isAudioFile(f))
                out.add(f);
    }
}

} // namespace

class AIRealFileGateTest final : public juce::UnitTest
{
public:
    AIRealFileGateTest()
        : juce::UnitTest("AI Real-File Gate - live WAV per-class measurement", "AI-Diag") {}

    void runTest() override
    {
        beginTest("Identity: shipped MLOnly backend is Active");

        const auto weights = shippedWeights();
        expect(weights.existsAsFile(), "shipped Resources/Models/ml_weights.bin missing");
        if (! weights.existsAsFile())
            return;
        expectEquals(weights.getSize(), kExpectedModelBytes, "shipped model size drifted");

        auto probe = makeLiveEngine(48000.0, 0.50f, weights);
        if (! probe)
            return;

        auto& ml = probe->getMLEngineForTest();
        expectEquals(ml.getLoadedWeightsBytes(), kExpectedModelBytes,
                     "Real-File Gate did not load the pinned shipping blob size");
        expectEquals(ml.getLoadedWeightsChecksum(), juce::String(kExpectedModelChecksum),
                     "Real-File Gate did not load the pinned seed22 checksum");

        logMessage("");
        logMessage("  REAL-FILE GATE IDENTITY");
        logMessage("  model path:       " + ml.getLoadedWeightsPath());
        logMessage("  model bytes:      " + juce::String(ml.getLoadedWeightsBytes()));
        logMessage("  model fnv:        " + ml.getLoadedWeightsChecksum());
        logMessage("  problem schema:   " + ml.getLoadedProblemSchema());
        logMessage("  backend status:   " + AIEngine::getMLBackendStatusName(probe->getMLBackendStatus()));
        logMessage("  backend mode:     MLOnly (shipping default)");
        logMessage("  source profile:   Generic");
        logMessage("  path:             LiveAIAnalysisPipeline + analyzeSpectrum(force=false)");
        logMessage("  NOTE: persistence and the live rate limiter are ON. This is what the user sees.");
        logMessage("  NOTE: this is the file/live ruler. Frozen Product Gate remains the model ruler.");
        logMessage("  NOTE: do not point AIEQ_REAL_GATE_DIR at Semantic / L1 listening WAVs.");

        runSectionA(weights);
        runSectionB(weights);
    }

private:
    std::unique_ptr<AIEngine> makeLiveEngine(double sampleRate, float sensitivity, const juce::File& weights)
    {
        auto ai = std::make_unique<AIEngine>();
        const bool loaded = ai->setCustomMLWeightsPathForTests(weights);
        expect(loaded, "AIEngine rejected shipped ml_weights.bin");
        if (! loaded)
            return nullptr;

        ai->prepare(sampleRate, kBlockSize);
        ai->setEnabled(true);
        ai->setSensitivity(sensitivity);
        ai->setSourceProfile(AIEngine::SourceProfile::Generic);
        ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);

        expect(ai->getMLBackendStatus() == AIEngine::MLBackendStatus::Active,
               "ML backend is not Active; Real-File Gate would be invalid");
        expect(ai->getDetectionBackendMode() == AIEngine::DetectionBackendMode::MLOnly,
               "Real-File Gate backend mode drifted away from MLOnly");
        expect(ai->isUsingMLDetection(),
               "MLOnly requested but AIEngine is not actually using ML detection");

        if (ai->getMLBackendStatus() != AIEngine::MLBackendStatus::Active
            || ai->getDetectionBackendMode() != AIEngine::DetectionBackendMode::MLOnly
            || ! ai->isUsingMLDetection())
            return nullptr;

        return ai;
    }

    bool observeClip(const ClipSpec& spec, float sensitivity, const juce::File& weights,
                     ClipObservation& out)
    {
        out = {};
        out.fileName = spec.file.getFileName();
        out.primaryLabel = spec.isClean ? juce::String("Clean")
                         : (spec.primaryClass.has_value()
                                ? juce::String(className(*spec.primaryClass))
                                : juce::String("Unknown"));

        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;
        expect(loadAudioFile(spec.file, audio, sampleRate),
               "Cannot load WAV: " + spec.file.getFullPathName());
        if (audio.getNumSamples() <= 0 || sampleRate <= 0.0)
        {
            expect(false, "Empty or unreadable audio: " + spec.file.getFileName());
            return false;
        }

        aieq_test::LiveAIAnalysisPipeline pipe(sampleRate);
        const auto frames = pipe.analyze(audio);
        expect(! frames.empty(), "LiveAIAnalysisPipeline produced no frames: " + spec.file.getFileName());
        if (frames.empty())
            return false;
        out.frames = static_cast<int>(frames.size());

        auto ai = makeLiveEngine(sampleRate, sensitivity, weights);
        if (! ai)
            return false;

        for (const auto& frame : frames)
            ai->analyzeSpectrum(frame, /*force=*/false);

        for (const auto& correction : ai->getPendingCorrections())
        {
            const auto pc = toProductClass(correction.type);
            if (! pc.has_value())
                continue;
            const auto idx = classIndex(*pc);
            if (! out.detected[idx])
            {
                out.detected[idx] = true;
                out.firstFrequencyHz[idx] = correction.frequency;
                out.firstConfidence[idx] = correction.confidence;
            }
        }
        out.detections = formatDetections(out);
        return true;
    }

    void accumulate(SensitivityResult& result, const ClipSpec& spec, const ClipObservation& observed)
    {
        result.files.push_back(observed);

        if (spec.isClean)
        {
            for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
            {
                const auto pc = static_cast<heldout::ProductClass>(c);
                if (pc == heldout::ProductClass::DullSound)
                    continue;
                auto& metrics = result.metrics[c];
                ++metrics.cleanNegative;
                if (observed.detected[c])
                    ++metrics.cleanFp;
            }
            return;
        }

        if (! spec.primaryClass.has_value())
            return;

        const auto primary = *spec.primaryClass;
        const auto primaryIdx = classIndex(primary);
        auto& primaryMetrics = result.metrics[primaryIdx];
        ++primaryMetrics.positive;

        if (observed.detected[primaryIdx])
        {
            ++primaryMetrics.tp;
            if (spec.targetFrequencyHz.has_value() && observed.firstFrequencyHz[primaryIdx].has_value())
            {
                const double expected = static_cast<double>(*spec.targetFrequencyHz);
                const double actual = static_cast<double>(*observed.firstFrequencyHz[primaryIdx]);
                ++primaryMetrics.localizationCount;
                primaryMetrics.localizationAbsHzSum += std::abs(actual - expected);
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
            // Cross-class labels on positives stay Unknown: do not certify FP-cross.
        }
    }

    SensitivityResult measureClips(float sensitivity,
                                   const juce::File& weights,
                                   const std::vector<ClipSpec>& clips)
    {
        SensitivityResult result;
        result.sensitivity = sensitivity;
        for (const auto& spec : clips)
        {
            ClipObservation observed;
            if (! observeClip(spec, sensitivity, weights, observed))
                continue;
            accumulate(result, spec, observed);
        }
        return result;
    }

    static bool resultsEquivalent(const SensitivityResult& a, const SensitivityResult& b)
    {
        if (a.sensitivity != b.sensitivity || a.files.size() != b.files.size())
            return false;
        for (std::size_t i = 0; i < a.files.size(); ++i)
        {
            if (a.files[i].fileName != b.files[i].fileName
                || a.files[i].detections != b.files[i].detections
                || a.files[i].detected != b.files[i].detected)
                return false;
        }
        for (std::size_t c = 0; c < heldout::kProductClassCount; ++c)
        {
            const auto& x = a.metrics[c];
            const auto& y = b.metrics[c];
            if (x.positive != y.positive || x.tp != y.tp || x.fn != y.fn
                || x.cleanNegative != y.cleanNegative || x.cleanFp != y.cleanFp
                || x.crossNegative != y.crossNegative || x.crossFp != y.crossFp)
                return false;
            for (std::size_t p = 0; p < heldout::kProductClassCount; ++p)
                if (a.crossDetection[c][p] != b.crossDetection[c][p])
                    return false;
        }
        return true;
    }

    static juce::String candidateDecision(const ClassMetrics& m, bool unsupported)
    {
        if (unsupported)
            return "UNSUPPORTED";
        if (m.positive < kCandidateMinN)
            return "INSUFFICIENT";
        if (m.tp < kMinRecall || m.cleanFp > kMaxCleanFp)
            return "FAIL";
        if (m.crossNegative == 0)
            return "REVIEW";
        if (m.crossFp > kMaxCrossFp)
            return "FAIL";
        return "PASS";
    }

    void printResult(const juce::String& title, const SensitivityResult& result)
    {
        logMessage("");
        logMessage("  ============================================================================");
        logMessage("  " + title + " - sensitivity " + juce::String(result.sensitivity, 2));
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
        logMessage("  CROSS-DETECTION MATRIX (rows=file primary, cols=detected; binary per file)");
        logMessage("               Res Har Mud Sib Boo Box Thi Dul");
        for (std::size_t row = 0; row < heldout::kLegacyV1SupportedClasses.size(); ++row)
        {
            juce::String line = "  " + juce::String(className(static_cast<heldout::ProductClass>(row))).paddedRight(' ', 12);
            for (std::size_t col = 0; col < heldout::kProductClassCount; ++col)
                line += juce::String(result.crossDetection[row][col]).paddedLeft(' ', 4);
            logMessage(line);
        }

        logMessage("");
        logMessage("  PER-FILE  filename | primary | detected types@Hz | conf");
        for (const auto& f : result.files)
            logMessage("  " + f.fileName.paddedRight(' ', 24)
                       + " | " + f.primaryLabel.paddedRight(' ', 10)
                       + " | " + f.detections
                       + " | frames=" + juce::String(f.frames));
    }

    std::vector<ClipSpec> sectionAClips()
    {
        const auto dir = corpusDir();
        expect(dir.isDirectory(), "Committed corpus dir missing: " + dir.getFullPathName());

        std::vector<ClipSpec> clips;
        {
            ClipSpec c;
            c.file = dir.getChildFile("res3200_pink.wav");
            c.primaryClass = heldout::ProductClass::Resonance;
            c.targetFrequencyHz = 3200.0f;
            c.note = "primary Resonance (corpus known_fail on live path)";
            clips.push_back(std::move(c));
        }
        {
            ClipSpec c;
            c.file = dir.getChildFile("clean_pink.wav");
            c.isClean = true;
            c.note = "all-class Negative";
            clips.push_back(std::move(c));
        }
        {
            ClipSpec c;
            c.file = dir.getChildFile("clean_dark_tilt.wav");
            c.isClean = true;
            c.note = "Clean Negative for MLOnly; corpus known_fail is Hybrid heuristic only";
            clips.push_back(std::move(c));
        }

        for (const auto& c : clips)
            expect(c.file.existsAsFile(), "Committed corpus WAV missing: " + c.file.getFileName());
        return clips;
    }

    void runSectionA(const juce::File& weights)
    {
        beginTest("Section A: committed live-file baseline (ai_corpus WAVs)");

        const auto clips = sectionAClips();
        logMessage("");
        logMessage("  SECTION A -- committed TestAssets/ai_corpus/");
        logMessage("  res3200_pink.wav:     primary Resonance (live path is a known corpus miss)");
        logMessage("  clean_pink.wav:       all-class Negative");
        logMessage("  clean_dark_tilt.wav:  Clean Negative for MLOnly.");
        logMessage("                        Corpus known_fail is Hybrid-heuristic (not measured here).");
        logMessage("  Cross-class negatives on positives stay Unknown.");
        logMessage("  Candidate FAIL/REVIEW/PASS apply only when n>=12; n<12 is INSUFFICIENT.");
        logMessage("  Those labels do not fail this unit test and do not change runtime.");

        std::array<SensitivityResult, kSensitivities.size()> firstRun {};
        for (std::size_t i = 0; i < kSensitivities.size(); ++i)
            firstRun[i] = measureClips(kSensitivities[i], weights, clips);

        for (const auto& result : firstRun)
            printResult("AI REAL-FILE GATE Section A", result);

        // Cheap determinism: two identical runs of the labeled resonance clip.
        beginTest("Section A: two identical live runs of one labeled clip match");
        const ClipSpec* labeled = nullptr;
        for (const auto& c : clips)
            if (c.primaryClass.has_value())
            {
                labeled = &c;
                break;
            }
        expect(labeled != nullptr, "Section A has no labeled clip for the determinism check");
        if (labeled != nullptr)
        {
            ClipObservation a, b;
            const bool okA = observeClip(*labeled, 0.50f, weights, a);
            const bool okB = observeClip(*labeled, 0.50f, weights, b);
            expect(okA && okB, "Determinism clip failed to run");
            logMessage("  run A: " + a.fileName + " | " + a.detections);
            logMessage("  run B: " + b.fileName + " | " + b.detections);
            expect(a.detections == b.detections && a.detected == b.detected,
                   "Two identical live-file runs produced different detections");
        }
    }

    std::vector<ClipSpec> scanExternalDir(const juce::File& root)
    {
        std::vector<ClipSpec> clips;
        juce::Array<juce::File> folders;
        root.findChildFiles(folders, juce::File::findDirectories, false);
        folders.sort();

        for (const auto& folder : folders)
        {
            const auto name = folder.getFileName();
            if (name.equalsIgnoreCase("Clean") || name.equalsIgnoreCase("Negative"))
            {
                juce::Array<juce::File> files;
                collectAudioOneLevel(folder, files);
                files.sort();
                for (const auto& f : files)
                {
                    ClipSpec spec;
                    spec.file = f;
                    spec.isClean = true;
                    clips.push_back(std::move(spec));
                }
                continue;
            }

            const auto pc = classFromFolderName(name);
            if (! pc.has_value())
            {
                logMessage("  ignore folder (not a product class): " + name);
                continue;
            }
            if (*pc == heldout::ProductClass::DullSound)
            {
                logMessage("  ignore DullSound/ (UNSUPPORTED on legacy-v1)");
                continue;
            }

            juce::Array<juce::File> files;
            collectAudioOneLevel(folder, files);
            files.sort();
            for (const auto& f : files)
            {
                ClipSpec spec;
                spec.file = f;
                spec.primaryClass = pc;
                clips.push_back(std::move(spec));
            }
        }
        return clips;
    }

    void runSectionB(const juce::File& weights)
    {
        beginTest("Section B: optional external real files (AIEQ_REAL_GATE_DIR)");

        const auto env = juce::SystemStats::getEnvironmentVariable("AIEQ_REAL_GATE_DIR", {});
        if (env.isEmpty())
        {
            logMessage("  SKIP Section B: AIEQ_REAL_GATE_DIR unset (optional external real-file gate).");
            expect(true, "Section B skipped because AIEQ_REAL_GATE_DIR is unset");
            return;
        }

        const juce::File root(env);
        if (! root.isDirectory())
        {
            expect(false, "AIEQ_REAL_GATE_DIR is not a directory: " + root.getFullPathName());
            return;
        }

        logMessage("  SECTION B -- AIEQ_REAL_GATE_DIR=" + root.getFullPathName());
        logMessage("  Class folders are primary-only Positive; Clean/ certifies Negative for every supported class.");
        logMessage("  Cross-class negatives on positives stay Unknown.");
        logMessage("  Candidate FAIL/REVIEW/PASS apply only when n>=12; n<12 is INSUFFICIENT.");
        logMessage("  Those labels do not fail this unit test and do not change runtime.");

        const auto clips = scanExternalDir(root);
        logMessage("  scanned clips: " + juce::String(static_cast<int>(clips.size())));
        if (clips.empty())
        {
            logMessage("  no labeled audio under class folders; Section B has nothing to measure.");
            expect(true, "Section B directory contained no labeled audio");
            return;
        }

        for (std::size_t i = 0; i < kSensitivities.size(); ++i)
            printResult("AI REAL-FILE GATE Section B",
                        measureClips(kSensitivities[i], weights, clips));
    }
};

static AIRealFileGateTest gAIRealFileGateTest;
