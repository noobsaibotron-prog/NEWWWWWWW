/**
 * SemanticPerceptualGateTest — Level 0 of the Semantic perceptual gate.
 *
 * Env-gated offline probe, report-only, no production wiring touched: it does
 * not construct AIEqualizerAudioProcessor, does not go through APVTS, does not
 * use SemanticControlPanel or the async planning service. It drives the same
 * pure pieces the production path drives — PerceptualFrontEnd, SemanticPlanner,
 * ParametricEQProcessor — directly, offline, over real WAV files.
 *
 * What this proves: nothing catastrophic (NaN, new clipping, silently-ignored
 * constraints) and nothing mechanically wrong (the plan the fitter chose is
 * actually what got rendered). It does NOT prove the result sounds good — that
 * is Level 1 (human listening) and beyond, which this is a precondition for,
 * not a substitute for.
 *
 * Usage:
 *   AIEQ_SEMANTIC_CORPUS_DIR=/path/to/wavs \
 *     build-mac/Release/bin/AIEqualizerPro_AI_Tests --category=ManualProbe -v
 *
 * Optional:
 *   AIEQ_SEMANTIC_OUT_DIR=/path/to/output   (default: <corpus>/../semantic_gate_out)
 *
 * Output per (clip, phrase) pair, when the plan has at least one band:
 *   <out>/<clip>__<phrase-slug>__before.wav   (source, through the SAME
 *       ParametricEQProcessor instance with every band left disabled — this
 *       controls for the processor's own signal path so a difference can only
 *       be the EQ, not a processing artifact of comparing WAV-reader output to
 *       ParametricEQProcessor output)
 *   <out>/<clip>__<phrase-slug>__after.wav    (source, same processor, with the
 *       fitter's own planned bands applied directly — no SemanticEQEngine/band-
 *       ownership adapter in between, so this is exactly what the fitter chose,
 *       not a re-interpretation of it)
 *
 * Plus one CSV row per pair: <out>/semantic_gate_report.csv
 *
 * The CSV is meant to be read by a human before Level 1 listening, not treated
 * as a pass/fail gate. Two columns matter most on a first pass:
 *   goal_status        - if a constrained request (e.g. "warmer without mud")
 *                         reads Achieved on material a human would call muddy,
 *                         something is wrong before anyone needs to listen.
 *   output_new_clipping - the one thing this CAN say is definitely wrong
 *                         without a human: an unclipped source that clips
 *                         after planning is never intended.
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "../AI/PerceptualFrontEnd.h"
#include "../AI/SemanticPlanner.h"
#include "../DSP/ParametricEQProcessor.h"

namespace
{
using namespace AIEQPerceptual;

// The fitter's FilterType and the DSP processor's FilterType are two separate
// enums that happen to share numeric values for the three types the fitter can
// choose (LowShelf=1, Peak=2, HighShelf=3). Asserted here, once, rather than
// assumed silently at every call site: if either enum is ever renumbered this
// fails loudly at the first run instead of silently rendering the wrong filter
// shape.
static_assert(static_cast<int>(FilterType::LowShelf) == ParametricEQProcessor::LowShelf, "");
static_assert(static_cast<int>(FilterType::Peak) == ParametricEQProcessor::Peak, "");
static_assert(static_cast<int>(FilterType::HighShelf) == ParametricEQProcessor::HighShelf, "");

/** The canonical phrase set for Level 0. Deliberately only the facets T4/T4.1
    actually adversarially validated (Brightness General/Air/Brilliance,
    Warmth with and without the mud constraint) - padding this list with
    Presence/Weight/Clarity would silently imply a level of validation those
    dimensions have not had. Extend this list only alongside new adversarial
    coverage in SpectralContextAdversarialTest, not independently of it. */
const std::vector<std::string> kCanonicalPhrases = {
    "brighter",
    "darker",
    "more air",
    "more brilliance",
    "warmer",
    "warmer without mud",
    "brighter without harshness",
};

juce::String slugify(const std::string& phrase)
{
    juce::String s(phrase);
    s = s.toLowerCase().replaceCharacter(' ', '_');
    juce::String out;
    for (auto c : s)
        out += (juce::CharacterFunctions::isLetterOrDigit(c) || c == '_') ? juce::String::charToString(c) : juce::String();
    return out;
}

bool loadAudioFile(const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    out.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&out, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    sampleRate = reader->sampleRate;
    return true;
}

bool writeAudioFile(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), sampleRate,
                            static_cast<unsigned>(buffer.getNumChannels()), 32, {}, 0));
    if (writer == nullptr)
        return false;
    stream.release(); // writer now owns it

    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

/** Same mono-downmix convention as the production analysis path (equal-weight
    sum then average), so the context this builds matches what the plugin
    itself would have accumulated from the same file. */
std::vector<float> monoMix(const juce::AudioBuffer<float>& audio)
{
    const int n = audio.getNumSamples();
    const int ch = audio.getNumChannels();
    std::vector<float> mono(static_cast<size_t>(std::max(0, n)), 0.0f);
    for (int c = 0; c < ch; ++c)
    {
        const float* src = audio.getReadPointer(c);
        for (int i = 0; i < n; ++i)
            mono[static_cast<size_t>(i)] += src[i];
    }
    if (ch > 0)
        for (auto& v : mono)
            v /= static_cast<float>(ch);
    return mono;
}

/** Builds a mature SpectralContext from a whole real file via the SAME
    PerceptualFrontEnd class the production drain uses (Frame::bandDbFused,
    Frame::lfValid) - not a re-derivation of the analysis, an actual run of it. */
struct ContextDiagnostics
{
    SpectralContext context;
    float meanTemporalStdDevDb = 0.0f; // what the confidence stability term sees
};

ContextDiagnostics buildContextFromFile(const juce::AudioBuffer<float>& audio, double sr)
{
    const auto mono = monoMix(audio);

    PerceptualFrontEnd frontEnd;
    frontEnd.prepare(sr);
    const auto frames = frontEnd.analyzeAll(mono.data(), static_cast<int>(mono.size()));

    std::vector<float> centers;
    centers.reserve(static_cast<size_t>(frontEnd.numBands()));
    for (int b = 0; b < frontEnd.numBands(); ++b)
        centers.push_back(frontEnd.bandCenterHz(b));

    SpectralContextAccumulator acc;
    acc.prepare(centers);
    for (const auto& f : frames)
        acc.pushFrame(f.bandDbFused, f.lfValid);

    // Mean per-band temporal standard deviation, recomputed here rather than
    // read out of the accumulator (which does not expose it). This is the
    // quantity the confidence stability term is a function of, and "why is
    // confidence low on this clip" is the first question a human reading the
    // report will ask - so it belongs in the report, not in a debugger.
    ContextDiagnostics out;
    out.context = acc.snapshot();

    if (frames.size() > 1 && !centers.empty())
    {
        const std::size_t nb = centers.size();
        std::vector<double> mean(nb, 0.0), m2(nb, 0.0);
        std::size_t n = 0;
        for (const auto& f : frames)
        {
            if (f.bandDbFused.size() != nb) continue;
            ++n;
            for (std::size_t i = 0; i < nb; ++i)
            {
                const double x = f.bandDbFused[i];
                const double d = x - mean[i];
                mean[i] += d / static_cast<double>(n);
                m2[i] += d * (x - mean[i]);
            }
        }
        if (n > 1)
        {
            double acc2 = 0.0;
            for (std::size_t i = 0; i < nb; ++i)
                acc2 += std::sqrt(std::max(0.0, m2[i] / static_cast<double>(n - 1)));
            out.meanTemporalStdDevDb = static_cast<float>(acc2 / static_cast<double>(nb));
        }
    }
    return out;
}

/** Runs `audio` through a fresh ParametricEQProcessor. If `bands` is empty, the
    processor stays at its default all-disabled state, i.e. this is the bypass
    render used for `before`. */
juce::AudioBuffer<float> renderThroughProcessor(const juce::AudioBuffer<float>& audio,
                                                double sr,
                                                const std::vector<PlannedBand>& bands)
{
    ParametricEQProcessor proc;
    proc.prepare(sr, 512, audio.getNumChannels());

    // addBand(), not setBandParameters()+setBandEnabled(): the setters only
    // reconfigure a band that already exists (they return early while
    // numActiveBands is 0, which is what a fresh processor starts at) - they
    // are the wrong call for initial setup from empty and were silently a
    // no-op here on the first version of this tool, verified by a before/after
    // byte-for-byte diff before this fix.
    const int n = std::min(static_cast<int>(bands.size()), ParametricEQProcessor::getMaxBands());
    for (int i = 0; i < n; ++i)
    {
        const auto& b = bands[static_cast<size_t>(i)];
        proc.addBand(b.frequencyHz, b.gainDb, b.q, static_cast<int>(b.type));
    }

    juce::AudioBuffer<float> out(audio);
    for (int start = 0; start < out.getNumSamples(); start += 512)
    {
        const int len = std::min(512, out.getNumSamples() - start);
        juce::AudioBuffer<float> block(out.getArrayOfWritePointers(), out.getNumChannels(), start, len);
        proc.process(block);
    }
    return out;
}

struct HealthCheck
{
    bool hasNaNOrInf = false;
    float peakDb = -std::numeric_limits<float>::infinity();
    float rmsDb = -std::numeric_limits<float>::infinity();
};

HealthCheck measure(const juce::AudioBuffer<float>& buffer)
{
    HealthCheck h;
    double sumSq = 0.0;
    juce::int64 count = 0;
    float peak = 0.0f;

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        const float* d = buffer.getReadPointer(c);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float v = d[i];
            if (!std::isfinite(v))
            {
                h.hasNaNOrInf = true;
                continue;
            }
            peak = std::max(peak, std::abs(v));
            sumSq += static_cast<double>(v) * static_cast<double>(v);
            ++count;
        }
    }

    h.peakDb = juce::Decibels::gainToDecibels(peak, -160.0f);
    if (count > 0)
        h.rmsDb = juce::Decibels::gainToDecibels(
            static_cast<float>(std::sqrt(sumSq / static_cast<double>(count))), -160.0f);
    return h;
}

juce::String csvEscape(juce::String s)
{
    s = s.replace("\"", "\"\"");
    if (s.containsChar(',') || s.containsChar('"') || s.containsChar('\n'))
        return "\"" + s + "\"";
    return s;
}
} // namespace

class SemanticPerceptualGateTest final : public juce::UnitTest
{
public:
    SemanticPerceptualGateTest()
        : juce::UnitTest("Semantic perceptual gate — Level 0 offline render", "ManualProbe") {}

    void runTest() override
    {
        beginTest("Render corpus x canonical phrases, report mechanical health + plan provenance");

        const auto corpusDirEnv = juce::SystemStats::getEnvironmentVariable("AIEQ_SEMANTIC_CORPUS_DIR", {});
        if (corpusDirEnv.isEmpty())
        {
            logMessage("  SKIP: set AIEQ_SEMANTIC_CORPUS_DIR=/path/to/wavs to run this report-only probe.");
            expect(true, "SemanticPerceptualGate skipped because AIEQ_SEMANTIC_CORPUS_DIR is unset");
            return;
        }

        const juce::File corpusDir(corpusDirEnv);
        if (!corpusDir.isDirectory())
        {
            expect(false, "AIEQ_SEMANTIC_CORPUS_DIR is not a directory: " + corpusDir.getFullPathName());
            return;
        }

        juce::Array<juce::File> wavs;
        corpusDir.findChildFiles(wavs, juce::File::findFiles, false, "*.wav;*.WAV");
        wavs.sort();
        if (wavs.isEmpty())
        {
            expect(false, "No WAV files found in AIEQ_SEMANTIC_CORPUS_DIR: " + corpusDir.getFullPathName());
            return;
        }

        auto outDirPath = juce::SystemStats::getEnvironmentVariable("AIEQ_SEMANTIC_OUT_DIR", {});
        const juce::File outDir = outDirPath.isNotEmpty()
            ? juce::File(outDirPath)
            : corpusDir.getParentDirectory().getChildFile("semantic_gate_out");
        outDir.createDirectory();

        if (corpusDir.getFullPathName().contains("ai_corpus"))
            logMessage("  NOTE: this looks like TestAssets/ai_corpus (synthetic ML-detector "
                       "fixtures). Treat this run as a MECHANISM SMOKE TEST ONLY - it proves "
                       "the tool works, it is NOT the real gate corpus. Real material is a "
                       "separate, deliberate decision.");

        logMessage("  corpus=" + corpusDir.getFullPathName());
        logMessage("  out=" + outDir.getFullPathName());
        logMessage("  clips=" + juce::String(wavs.size())
                   + "  phrases=" + juce::String((int) kCanonicalPhrases.size()));

        // Counted in code. A shell parser over this log previously dropped every
        // "darker" row, because its amount is negative and the pattern only
        // accepted digits, and the resulting figure was reported as evidence
        // while the axis was being calibrated. Counting here removes the step
        // that can silently lose cases.
        int axisCases = 0, axisRail = 0;
        int brightCases = 0, brightRail = 0;
        int warmCases = 0, warmRail = 0;

        juce::String csv = "clip,phrase,dimension,focus,context_scale,context_applied,"
                           "target_peak_db,goal_status,input_peak_db,output_peak_db,"
                           "input_rms_db,output_rms_db,input_clipped,output_new_clipping,"
                           "has_nan_or_inf,plan_bands\n";

        const SemanticPlanner planner;

        for (const auto& wav : wavs)
        {
            juce::AudioBuffer<float> source;
            double sr = 0.0;
            if (!loadAudioFile(wav, source, sr))
            {
                logMessage("  SKIP (unreadable): " + wav.getFileName());
                continue;
            }

            const auto diagnostics = buildContextFromFile(source, sr);
            const auto& context = diagnostics.context;
            logMessage("  " + wav.getFileName() + "  context: valid=" + juce::String(context.valid ? 1 : 0)
                       + " confidence=" + juce::String(context.confidence, 2)
                       + " frames=" + juce::String(context.framesObserved)
                       + " sourceLevel=" + juce::String(context.sourceLevelDb, 1) + "dB"
                       + " meanTemporalStdDev=" + juce::String(diagnostics.meanTemporalStdDevDb, 1) + "dB");
            logMessage("      evidence: level=" + juce::String(context.levelScore, 3)
                       + " stability=" + juce::String(context.stabilityScore, 3)
                       + " observation=" + juce::String(context.warmupScore, 3)
                       + " stdErr=" + juce::String(context.aggregateStandardErrorDb, 3) + "dB"
                       + " active=" + juce::String(context.activeFrames)
                       + "/" + juce::String(context.framesObserved)
                       + " peak=" + juce::String(context.bandPeakDb, 1) + "dB"
                       + (context.hasUsableEvidence ? "" : "  [NO USABLE EVIDENCE]"));
            juce::String regions;
            static const char* kRegionNames[] =
                { "Sub", "Bass", "LoMid", "Mid", "Pres", "Brill", "Air" };
            for (std::size_t r = 0; r < AIEQPerceptual::kSpectralRegionCount; ++r)
                regions += juce::String(kRegionNames[r]) + "="
                         + juce::String(context.regionConfidence[r], 2) + " ";
            logMessage("      regional: " + regions
                       + " (global coverage=" + juce::String(context.coverageScore, 3) + ")");

            const auto inputHealth = measure(source);
            const bool inputClipped = inputHealth.peakDb >= -0.05f;

            const SemanticContextualizer contextualizer;

            for (const auto& phrase : kCanonicalPhrases)
            {
                const auto plan = planner.plan(phrase, sr, 1.0f, context);

                juce::String dimension = "-", focus = "-", goalStatus = "-";
                float contextScale = 1.0f;
                float regionalConfidence = -1.0f;
                float originalAmount = 0.0f, adjustedAmount = 0.0f, axisPosition = 0.0f;
                if (!plan.intent.goals.empty())
                {
                    const auto& g = plan.intent.goals.front();
                    dimension = semanticDimensionName(g.dimension);
                    focus = juce::String((int) g.focus);
                    // Which region this goal actually depends on. Reported rather
                    // than inferred from the phrase, because the mapping is the
                    // thing under test: "more air" must consult Air, not the
                    // global figure that says every real source is analysable.
                    regionalConfidence = contextualizer.evidenceConfidence(
                        g.dimension, g.focus, context);
                    for (const auto& adj : plan.contextAdjustments)
                        if (adj.dimension == g.dimension)
                        {
                            contextScale = adj.scale;
                            originalAmount = adj.originalAmount;
                            adjustedAmount = adj.adjustedAmount;
                            axisPosition = adj.axisPosition;
                            break;
                        }
                }
                if (!plan.goalOutcomes.empty())
                {
                    switch (plan.goalOutcomes.front().status)
                    {
                        case GoalOutcomeStatus::Achieved:          goalStatus = "Achieved"; break;
                        case GoalOutcomeStatus::Partial:           goalStatus = "Partial"; break;
                        case GoalOutcomeStatus::ConstraintLimited: goalStatus = "ConstraintLimited"; break;
                        case GoalOutcomeStatus::Unmet:             goalStatus = "Unmet"; break;
                    }
                }

                if (!plan.intent.goals.empty())
                {
                    const auto dim = plan.intent.goals.front().dimension;
                    const bool railed = std::abs(axisPosition) >= 0.999f;
                    ++axisCases; if (railed) ++axisRail;
                    if (dim == SemanticDimension::Brightness)
                    { ++brightCases; if (railed) ++brightRail; }
                    else if (dim == SemanticDimension::Warmth)
                    { ++warmCases; if (railed) ++warmRail; }
                }

                float targetPeak = 0.0f;
                for (const auto& p : plan.target.points)
                    targetPeak = std::max(targetPeak, p.deltaDb);

                const juce::String pairName = wav.getFileNameWithoutExtension() + "__" + slugify(phrase);

                if (!plan.valid || plan.fit.bands.empty())
                {
                    logMessage("    " + juce::String(phrase).paddedRight(' ', 28) + " -> NO PLAN (status="
                               + goalStatus + ", interpretation='" + plan.interpretation + "')");
                    csv += csvEscape(wav.getFileName()) + "," + csvEscape(phrase) + ","
                        + csvEscape(dimension) + "," + focus + "," + juce::String(contextScale, 3) + ","
                        + juce::String(plan.contextApplied ? 1 : 0) + "," + juce::String(targetPeak, 3) + ","
                        + goalStatus + "," + juce::String(inputHealth.peakDb, 2) + ",,"
                        + juce::String(inputHealth.rmsDb, 2) + ",,"
                        + juce::String(inputClipped ? 1 : 0) + ",0,0,0\n";
                    continue;
                }

                const auto before = renderThroughProcessor(source, sr, {});
                const auto after  = renderThroughProcessor(source, sr, plan.fit.bands);
                const auto outputHealth = measure(after);
                const bool outputNewClipping = !inputClipped && outputHealth.peakDb >= -0.05f;

                writeAudioFile(outDir.getChildFile(pairName + "__before.wav"), before, sr);
                writeAudioFile(outDir.getChildFile(pairName + "__after.wav"), after, sr);

                logMessage("    " + juce::String(phrase).paddedRight(' ', 28)
                           + " -> " + dimension.paddedRight(' ', 11) + " focus=" + focus.paddedRight(' ', 2)
                           + " scale=" + juce::String(contextScale, 2)
                           + " targetPeak=" + juce::String(targetPeak, 2) + "dB"
                           + " status=" + goalStatus.paddedRight(' ', 17)
                           + " outPeak=" + juce::String(outputHealth.peakDb, 1) + "dB"
                           + (outputNewClipping ? "  ** NEW CLIPPING **" : "")
                           + (outputHealth.hasNaNOrInf ? "  ** NaN/Inf **" : ""));

                // Per-case detail. Level 1 is a listening judgement, and a
                // listener who hears something odd needs to be able to see what
                // the planner actually did without re-running anything.
                logMessage("        confidence: global=" + juce::String(context.confidence, 2)
                           + "  relevant-region=" + juce::String(regionalConfidence, 2)
                           + "  axisPosition=" + juce::String(axisPosition, 2)
                           + "   amount " + juce::String(originalAmount, 3)
                           + " -> " + juce::String(adjustedAmount, 3));
                {
                    juce::String bands;
                    for (const auto& b : plan.fit.bands)
                    {
                        static const char* kTypeNames[] = { "LowShelf", "Peak", "HighShelf" };
                        const int ti = juce::jlimit(0, 2, static_cast<int>(b.type));
                        bands += juce::String(kTypeNames[ti]) + " "
                               + juce::String(b.frequencyHz, 0) + "Hz "
                               + (b.gainDb >= 0.0f ? "+" : "") + juce::String(b.gainDb, 2) + "dB "
                               + "Q" + juce::String(b.q, 2) + "   ";
                    }
                    logMessage("        EQ: " + bands);
                }
                if (!plan.goalOutcomes.empty()
                    && plan.goalOutcomes.front().status == GoalOutcomeStatus::ConstraintLimited)
                    logMessage("        constraint: limited by '"
                               + juce::String(plan.goalOutcomes.front().limitingPhrase)
                               + "'  achieved=" + juce::String(plan.goalOutcomes.front().achievedFraction, 2));

                expect(!outputHealth.hasNaNOrInf,
                       "NaN/Inf in rendered output: " + pairName);
                expect(!outputNewClipping,
                       "unclipped source clips after planning: " + pairName);

                csv += csvEscape(wav.getFileName()) + "," + csvEscape(phrase) + ","
                    + csvEscape(dimension) + "," + focus + "," + juce::String(contextScale, 3) + ","
                    + juce::String(plan.contextApplied ? 1 : 0) + "," + juce::String(targetPeak, 3) + ","
                    + goalStatus + "," + juce::String(inputHealth.peakDb, 2) + ","
                    + juce::String(outputHealth.peakDb, 2) + "," + juce::String(inputHealth.rmsDb, 2) + ","
                    + juce::String(outputHealth.rmsDb, 2) + "," + juce::String(inputClipped ? 1 : 0) + ","
                    + juce::String(outputNewClipping ? 1 : 0) + "," + juce::String(outputHealth.hasNaNOrInf ? 1 : 0)
                    + "," + juce::String((int) plan.fit.bands.size()) + "\n";
            }
        }

        const auto reportFile = outDir.getChildFile("semantic_gate_report.csv");
        reportFile.replaceWithText(csv);
        auto rate = [](int n, int d)
        {
            return juce::String(n) + " / " + juce::String(d)
                 + (d > 0 ? "  (" + juce::String(juce::roundToInt(100.0 * n / d)) + "%)" : "");
        };
        logMessage("");
        logMessage("  AXIS SATURATION - contextualization cases at the +/-1.0 clamp:");
        logMessage("    overall     " + rate(axisRail, axisCases));
        logMessage("    Brightness  " + rate(brightRail, brightCases));
        logMessage("    Warmth      " + rate(warmRail, warmCases));
        logMessage("    READ: a railed axis cannot rank sources - every source at the rail");
        logMessage("    receives the same treatment. This is the T5.5 measurement; it is");
        logMessage("    printed by the harness so it never depends on parsing this log.");
        logMessage("");
        logMessage("  CSV written: " + reportFile.getFullPathName());
        logMessage("  Audio pairs written to: " + outDir.getFullPathName());
        logMessage("  READ: this is Level 0 (mechanical) only. goal_status=ConstraintLimited on "
                   "material you'd call muddy/harsh/etc. is EXPECTED and correct, not a failure. "
                   "A human still has to listen before this counts as validated - see Level 1.");
    }
};

static SemanticPerceptualGateTest sSemanticPerceptualGateTest;
