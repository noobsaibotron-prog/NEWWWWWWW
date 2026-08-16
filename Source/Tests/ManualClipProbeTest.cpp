/**
 * ManualClipProbeTest — env-gated offline probe for ad-hoc Ableton clip matrices.
 *
 * This is intentionally report-only. It runs the real test mirror
 * (LiveAIAnalysisPipeline) into the real AIEngine with the shipped weights, then
 * prints per-file detections. It is a practical replacement for repeatedly
 * loading the same diagnostic WAVs in Ableton, not a scientific acceptance gate.
 *
 * Usage:
 *   AIEQ_PROBE_DIR=/path/to/wavs \
 *     build-mac/Release/bin/AIEqualizerPro_AI_Tests --category=ManualProbe -v
 *
 * Optional:
 *   AIEQ_PROBE_OUT=/path/to/results.csv
 *   AIEQ_PROBE_WEIGHTS=/path/to/ml_weights.bin
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include "../AI/AIEngine.h"
#include "Support/LiveAIAnalysisPipeline.h"

namespace
{

juce::File repoRoot()
{
    return juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory();
}

juce::File defaultWeightsFile()
{
    return repoRoot().getChildFile("Resources/Models/ml_weights.bin");
}

juce::File probeWeightsFile()
{
    const auto override = juce::SystemStats::getEnvironmentVariable("AIEQ_PROBE_WEIGHTS", {});
    if (override.isNotEmpty())
        return juce::File(override);
    return defaultWeightsFile();
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

juce::String csvEscape(juce::String s)
{
    s = s.replace("\"", "\"\"");
    if (s.containsChar(',') || s.containsChar('"') || s.containsChar('\n'))
        return "\"" + s + "\"";
    return s;
}

struct TypeStats
{
    int presentFrames = 0;
    float maxConfidence = 0.0f;
    float maxSeverity = 0.0f;
    float frequencyAtMax = 0.0f;
};

struct ProbeResult
{
    juce::String fileName;
    int frames = 0;
    std::map<AIEngine::ProblemType, TypeStats> liveStats;
    std::map<AIEngine::ProblemType, TypeStats> forcedStats;
    std::vector<AIEngine::Correction> finalCorrections;
};

void updateStats(std::map<AIEngine::ProblemType, TypeStats>& stats,
                 const std::vector<AIEngine::Correction>& corrections)
{
    std::array<bool, 9> seenThisFrame {};

    for (const auto& c : corrections)
    {
        const auto idx = static_cast<size_t>(c.type);
        if (idx >= seenThisFrame.size())
            continue;

        auto& s = stats[c.type];
        if (! seenThisFrame[idx])
        {
            ++s.presentFrames;
            seenThisFrame[idx] = true;
        }

        if (c.confidence >= s.maxConfidence)
        {
            s.maxConfidence = c.confidence;
            s.maxSeverity = c.severity;
            s.frequencyAtMax = c.frequency;
        }
    }
}

std::unique_ptr<AIEngine> makeEngine(double sampleRate,
                                     const juce::File& weights,
                                     AIEngine::SourceProfile profile)
{
    auto ai = std::make_unique<AIEngine>();
    ai->prepare(sampleRate, 512);
    ai->setEnabled(true);
    ai->setSensitivity(0.5f);
    ai->setSourceProfile(profile);
    ai->setDetectionBackendMode(AIEngine::DetectionBackendMode::MLOnly);
    ai->setCustomMLWeightsPathForTests(weights);
    ai->forceMLDetectionEnabledForTests(true);
    return ai;
}

ProbeResult runProbe(const juce::File& file,
                     const juce::File& weights,
                     AIEngine::SourceProfile profile)
{
    ProbeResult result;
    result.fileName = file.getFileName();

    juce::AudioBuffer<float> audio;
    double sr = 0.0;
    if (! loadAudioFile(file, audio, sr))
        return result;

    aieq_test::LiveAIAnalysisPipeline pipe(sr);
    const auto frames = pipe.analyze(audio);
    result.frames = static_cast<int>(frames.size());

    auto liveAi = makeEngine(sr, weights, profile);
    auto forcedAi = makeEngine(sr, weights, profile);

    for (const auto& frame : frames)
    {
        liveAi->analyzeSpectrum(frame, /*force=*/false);
        updateStats(result.liveStats, liveAi->getPendingCorrections());

        forcedAi->analyzeSpectrum(frame, /*force=*/true);
        updateStats(result.forcedStats, forcedAi->getPendingCorrections());
    }

    result.finalCorrections = liveAi->getPendingCorrections();
    return result;
}

juce::String summarizeStats(const std::map<AIEngine::ProblemType, TypeStats>& stats, int frames)
{
    if (stats.empty() || frames <= 0)
        return "(none)";

    std::vector<std::pair<AIEngine::ProblemType, TypeStats>> ordered(stats.begin(), stats.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b)
    {
        if (a.second.presentFrames != b.second.presentFrames)
            return a.second.presentFrames > b.second.presentFrames;
        return a.second.maxConfidence > b.second.maxConfidence;
    });

    juce::String out;
    int emitted = 0;
    for (const auto& [type, s] : ordered)
    {
        if (type == AIEngine::ProblemType::None || s.presentFrames <= 0)
            continue;

        const double pct = 100.0 * static_cast<double>(s.presentFrames) / static_cast<double>(frames);
        if (emitted++ > 0)
            out += "; ";
        out += AIEngine::getProblemTypeName(type)
            + " " + juce::String::formatted("%.1f%%", pct)
            + " @" + juce::String(s.frequencyAtMax, 0) + "Hz"
            + " c=" + juce::String(s.maxConfidence, 2);

        if (emitted >= 4)
            break;
    }

    return out.isNotEmpty() ? out : "(none)";
}

juce::String summarizeFinal(const std::vector<AIEngine::Correction>& corrections)
{
    if (corrections.empty())
        return "(none)";

    juce::String out;
    for (const auto& c : corrections)
    {
        if (out.isNotEmpty())
            out += "; ";
        out += AIEngine::getProblemTypeName(c.type)
            + "@" + juce::String(c.frequency, 0) + "Hz"
            + " g=" + juce::String(c.suggestedGain, 1)
            + " c=" + juce::String(c.confidence, 2);
    }
    return out;
}

AIEngine::SourceProfile profileFromEnv()
{
    const auto p = juce::SystemStats::getEnvironmentVariable("AIEQ_PROBE_PROFILE", "Generic").toLowerCase();
    if (p == "vocals") return AIEngine::SourceProfile::Vocals;
    if (p == "drums")  return AIEngine::SourceProfile::Drums;
    if (p == "bass")   return AIEngine::SourceProfile::Bass;
    if (p == "synth")  return AIEngine::SourceProfile::Synth;
    if (p == "master") return AIEngine::SourceProfile::Master;
    if (p == "edm")    return AIEngine::SourceProfile::EDM;
    if (p == "techno") return AIEngine::SourceProfile::Techno;
    return AIEngine::SourceProfile::Generic;
}

} // namespace

class ManualClipProbeTest final : public juce::UnitTest
{
public:
    ManualClipProbeTest()
        : juce::UnitTest("Manual clip probe — offline AI report", "ManualProbe") {}

    void runTest() override
    {
        beginTest("Probe WAV folder through LiveAIAnalysisPipeline + AIEngine MLOnly");

        const auto probeDirEnv = juce::SystemStats::getEnvironmentVariable("AIEQ_PROBE_DIR", {});
        if (probeDirEnv.isEmpty())
        {
            logMessage("  SKIP: set AIEQ_PROBE_DIR=/path/to/wavs to run this report-only probe.");
            expect(true, "ManualProbe skipped because AIEQ_PROBE_DIR is unset");
            return;
        }

        const juce::File probeDir(probeDirEnv);
        if (! probeDir.isDirectory())
        {
            expect(false, "AIEQ_PROBE_DIR is not a directory: " + probeDir.getFullPathName());
            return;
        }

        const auto weights = probeWeightsFile();
        if (! weights.existsAsFile())
        {
            expect(false, "ML weights not found: " + weights.getFullPathName());
            return;
        }

        juce::Array<juce::File> wavs;
        probeDir.findChildFiles(wavs, juce::File::findFiles, false, "*.wav;*.WAV");
        wavs.sort();

        if (wavs.isEmpty())
        {
            expect(false, "No WAV files found in AIEQ_PROBE_DIR: " + probeDir.getFullPathName());
            return;
        }

        const auto profile = profileFromEnv();
        logMessage("  dir=" + probeDir.getFullPathName());
        logMessage("  weights=" + weights.getFullPathName());
        logMessage("  profile=" + juce::SystemStats::getEnvironmentVariable("AIEQ_PROBE_PROFILE", "Generic"));
        logMessage("  file | frames | LIVE final pending | LIVE present-over-time | FORCE frame-level recognitions");
        logMessage("  -----+--------+--------------------+------------------------+-------------------------------");

        juce::String csv = "file,frames,live_final,live_present_over_time,force_frame_level_recognitions\n";
        for (const auto& wav : wavs)
        {
            const auto r = runProbe(wav, weights, profile);
            const auto liveFinal = summarizeFinal(r.finalCorrections);
            const auto liveStats = summarizeStats(r.liveStats, r.frames);
            const auto forcedStats = summarizeStats(r.forcedStats, r.frames);

            logMessage("  " + r.fileName.paddedRight(' ', 54)
                       + " | " + juce::String(r.frames).paddedLeft(' ', 4)
                       + " | " + liveFinal
                       + " | " + liveStats
                       + " | " + forcedStats);

            csv += csvEscape(r.fileName) + ","
                + juce::String(r.frames) + ","
                + csvEscape(liveFinal) + ","
                + csvEscape(liveStats) + ","
                + csvEscape(forcedStats) + "\n";
        }

        auto outPath = juce::SystemStats::getEnvironmentVariable("AIEQ_PROBE_OUT", {});
        if (outPath.isEmpty())
            outPath = probeDir.getParentDirectory().getChildFile("probe_results.csv").getFullPathName();

        const juce::File outFile(outPath);
        outFile.replaceWithText(csv);
        logMessage("  CSV written: " + outFile.getFullPathName());
        logMessage("  READ: report-only. LIVE uses the production frame-counter + temporal persistence; FORCE shows raw frame-level model recognitions.");

        expect(true, "ManualProbe completed");
    }
};

static ManualClipProbeTest manualClipProbeTest;
