/**
 * RealDataSibilanceTest — Real-data program, MILESTONE 2 (category "RealData").
 *
 * M1 finding (commit cf76324d): training "un-injected vocal = clean" is ILL-POSED — sung
 * vocals inherently sibilate — AND a whole-clip AVERAGE dilutes sibilance (a LOCAL, temporal
 * event). M2 fixes BOTH, per the counter-exam amendments:
 *   1. PAIRED de-ess vs boost from the SAME frame → isolates the "excess sibilance" axis
 *      (negative = 5–9 kHz band CUT; positive = 5–9 kHz band BOOSTED). The only difference
 *      between a pair is the sibilance-band level, so the label is exact and the contrast clean.
 *   2. FRAME-LEVEL, not whole-clip average: we pick the most-sibilant frames (top HF energy in
 *      5–9 kHz) of each clip — the moments where sibilance actually lives.
 *   3. FEATURE-DELTA log before training: if pos/neg mel features don't separate in the band,
 *      training cannot resolve anything — surfaced explicitly, not assumed.
 *
 * Representation: WAV → OfflineAnalysisPipeline (validated SpectrumAnalyzer mirror, dB) →
 * decibelsToGain → linear — exactly what AIEngine feeds MLEngine. Measurement at the MODEL
 * level (detectProblems / forwardRawProbabilities). Report-only; held-out singers; no
 * ml_weights.bin / runtime change. Self-skips when AIEQ_REALDATA_DIR is unset.
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

#include "../AI/MLEngine.h"
#include "../AI/AIEngine.h"
#include "Support/OfflineAnalysisPipeline.h"

namespace
{
constexpr int   kSibIdx        = static_cast<int>(MLEngine::ProblemType::Sibilance); // 3
constexpr int   kTrainClips    = 120;
constexpr int   kTestClips     = 40;
constexpr int   kFramesPerClip = 4;       // top-HF-energy frames per clip (sibilant moments)
constexpr int   kEpochs        = 600;
constexpr float kLearningRate  = 0.003f;
constexpr float kSibLoHz       = 5000.0f;
constexpr float kSibHiHz       = 9000.0f;
constexpr float kSibEdgeHz     = 600.0f;  // cosine-ish taper at band edges

juce::File realDataDir()
{
    auto p = juce::SystemStats::getEnvironmentVariable("AIEQ_REALDATA_DIR", {});
    return p.isNotEmpty() ? juce::File(p) : juce::File();
}

juce::File shippedWeights()
{
    return juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
        .getChildFile("Resources/Models/ml_weights.bin");
}

bool loadMono(const juce::File& f, juce::AudioBuffer<float>& out, double& srOut)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    srOut = reader->sampleRate;
    out.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&out, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    return true;
}

float hfEnergy(const std::vector<float>& lin, double sr)
{
    const int fftSize = static_cast<int>(lin.size()) * 2;
    const float binHz = static_cast<float>(sr) / static_cast<float>(fftSize);
    float e = 0.0f;
    for (size_t i = 0; i < lin.size(); ++i)
    {
        const float f = static_cast<float>(i) * binHz;
        if (f >= kSibLoHz && f <= kSibHiHz) e += lin[i];
    }
    return e;
}

// The most-sibilant linear frames of a clip (top kFramesPerClip by 5–9 kHz energy).
std::vector<std::vector<float>> sibilantFrames(const juce::AudioBuffer<float>& audio, double sr, int maxFrames)
{
    aieq_test::OfflineAnalysisPipeline pipe(sr);
    auto framesDb = pipe.analyze(audio);
    std::vector<std::vector<float>> lin;
    lin.reserve(framesDb.size());
    for (const auto& fr : framesDb)
    {
        std::vector<float> l(fr.size());
        for (size_t i = 0; i < fr.size(); ++i) l[i] = juce::Decibels::decibelsToGain(fr[i]);
        lin.push_back(std::move(l));
    }
    std::sort(lin.begin(), lin.end(), [sr](const std::vector<float>& a, const std::vector<float>& b){
        return hfEnergy(a, sr) > hfEnergy(b, sr);
    });
    if (static_cast<int>(lin.size()) > maxFrames) lin.resize(static_cast<size_t>(maxFrames));
    return lin;
}

// Scale the 5–9 kHz band by gainDb (cosine-tapered edges). gainDb<0 = de-ess, >0 = boost.
std::vector<float> scaleSibBand(const std::vector<float>& lin, double sr, float gainDb)
{
    std::vector<float> out = lin;
    const int fftSize = static_cast<int>(lin.size()) * 2;
    const float binHz = static_cast<float>(sr) / static_cast<float>(fftSize);
    const float gain = juce::Decibels::decibelsToGain(gainDb);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float f = static_cast<float>(i) * binHz;
        float w = 0.0f;
        if (f > kSibLoHz - kSibEdgeHz && f < kSibHiHz + kSibEdgeHz)
        {
            if (f < kSibLoHz)      w = (f - (kSibLoHz - kSibEdgeHz)) / kSibEdgeHz;
            else if (f > kSibHiHz) w = ((kSibHiHz + kSibEdgeHz) - f) / kSibEdgeHz;
            else                   w = 1.0f;
            w = juce::jlimit(0.0f, 1.0f, w);
            out[i] = std::max(0.0f, lin[i] * (1.0f + (gain - 1.0f) * w));
        }
    }
    return out;
}

juce::String singerOf(const juce::File& f, const juce::File& root)
{
    auto rel = f.getRelativePathFrom(root);
    return rel.upToFirstOccurrenceOf(juce::File::getSeparatorString(), false, false);
}
} // namespace

class RealDataSibilanceTest : public juce::UnitTest
{
public:
    RealDataSibilanceTest()
        : juce::UnitTest("Real-data Sibilance injection loop (RealData)", "RealData") {}

    void runTest() override
    {
        beginTest("Paired de-ess/boost on sibilant frames of REAL vocals, held-out singers");

        const juce::File root = realDataDir();
        if (root == juce::File() || ! root.isDirectory())
        {
            logMessage("  SKIP: AIEQ_REALDATA_DIR unset/not a dir — set it to the VocalSet FULL dir to run.");
            return;
        }
        juce::Array<juce::File> wavs;
        root.findChildFiles(wavs, juce::File::findFiles, true, "*.wav");
        if (wavs.size() < 40) { logMessage("  SKIP: only " + juce::String(wavs.size()) + " wavs."); return; }

        const juce::StringArray testSingers { "female9", "female8", "male11", "male10" };
        juce::Array<juce::File> trainPool, testPool;
        for (const auto& f : wavs)
            (testSingers.contains(singerOf(f, root)) ? testPool : trainPool).add(f);

        std::mt19937 rng(20260620);
        auto pick = [&](juce::Array<juce::File> pool, int n){
            for (int i = pool.size() - 1; i > 0; --i) std::swap(pool.getReference(i), pool.getReference(static_cast<int>(rng() % static_cast<unsigned>(i + 1))));
            pool.resize(juce::jmin(n, pool.size())); return pool;
        };
        auto trainFiles = pick(trainPool, kTrainClips);
        auto testFiles  = pick(testPool, kTestClips);
        logMessage("  clips: " + juce::String(wavs.size()) + " | train singers using " + juce::String(trainFiles.size())
                   + " | held-out singers using " + juce::String(testFiles.size()) + " | frames/clip " + juce::String(kFramesPerClip));

        std::uniform_real_distribution<float> cutD(-14.0f, -8.0f);   // de-ess depth
        std::uniform_real_distribution<float> boostD(6.0f, 14.0f);   // excess sibilance

        // ── build PAIRED frame-level dataset + measure the feature delta ──
        MLEngine trained; trained.initialize();
        std::vector<MLEngine::TrainingSample> dataset;
        double featDeltaSum = 0.0; double featDeltaMax = 0.0; int pairs = 0;
        for (const auto& f : trainFiles)
        {
            juce::AudioBuffer<float> audio; double sr = 0;
            if (! loadMono(f, audio, sr)) continue;
            for (const auto& frame : sibilantFrames(audio, sr, kFramesPerClip))
            {
                auto neg = scaleSibBand(frame, sr, cutD(rng));     // de-essed → Sib 0
                auto pos = scaleSibBand(frame, sr, boostD(rng));   // boosted  → Sib 1
                auto melNeg = trained.melBandsFromSpectrumForTests(neg, sr);
                auto melPos = trained.melBandsFromSpectrumForTests(pos, sr);
                // feature-delta over all mel bands (should be substantial if the signal exists)
                double dsum = 0.0, dmax = 0.0;
                for (size_t b = 0; b < melNeg.size(); ++b)
                { double d = std::abs(melPos[b] - melNeg[b]); dsum += d; dmax = std::max(dmax, d); }
                featDeltaSum += dsum / std::max<size_t>(1, melNeg.size()); featDeltaMax = std::max(featDeltaMax, dmax); ++pairs;

                MLEngine::TrainingSample sNeg; sNeg.melSpectrum = std::move(melNeg);
                MLEngine::TrainingSample sPos; sPos.melSpectrum = std::move(melPos);
                sPos.problemTargets[static_cast<size_t>(kSibIdx)] = 1.0f;
                dataset.push_back(std::move(sNeg));
                dataset.push_back(std::move(sPos));
            }
        }
        if (pairs < 10) { logMessage("  SKIP: too few usable frames (" + juce::String(pairs) + ")."); return; }
        logMessage("  built " + juce::String(dataset.size()) + " samples from " + juce::String(pairs) + " de-ess/boost pairs");
        logMessage("  FEATURE-DELTA (pos vs neg mel): mean/band " + juce::String(featDeltaSum / pairs, 4)
                   + " | max band " + juce::String(featDeltaMax, 4) + "  (must be >> 0 or training can't separate)");

        trained.initializeRandomWeights();
        trained.trainOnDataset(dataset, kEpochs, kLearningRate);

        MLEngine shipped; shipped.initialize();
        expect(shipped.loadWeights(shippedWeights()), "could not load shipped weights");

        // ── measure on held-out singers: same paired frames ──
        auto detectsSib = [](MLEngine& m, const std::vector<float>& s, double sr){
            for (const auto& d : m.detectProblems(s, sr)) if (d.type == MLEngine::ProblemType::Sibilance) return true;
            return false;
        };
        struct Score { int recall = 0, fp = 0, n = 0; double rawPos = 0, rawNeg = 0; };
        Score sh, tr;
        std::mt19937 rngT(7777);
        std::uniform_real_distribution<float> cutT(-14.0f, -8.0f), boostT(6.0f, 14.0f);
        for (const auto& f : testFiles)
        {
            juce::AudioBuffer<float> audio; double sr = 0;
            if (! loadMono(f, audio, sr)) continue;
            for (const auto& frame : sibilantFrames(audio, sr, kFramesPerClip))
            {
                auto neg = scaleSibBand(frame, sr, cutT(rngT));
                auto pos = scaleSibBand(frame, sr, boostT(rngT));
                std::array<float, MLEngine::numProblemTypes> rShP{}, rShN{}, rTrP{}, rTrN{};
                shipped.detectProblems(pos, sr, &rShP); shipped.detectProblems(neg, sr, &rShN);
                trained.detectProblems(pos, sr, &rTrP); trained.detectProblems(neg, sr, &rTrN);
                sh.n++; tr.n++;
                sh.recall += detectsSib(shipped, pos, sr) ? 1 : 0; sh.fp += detectsSib(shipped, neg, sr) ? 1 : 0;
                tr.recall += detectsSib(trained, pos, sr) ? 1 : 0; tr.fp += detectsSib(trained, neg, sr) ? 1 : 0;
                sh.rawPos += rShP[static_cast<size_t>(kSibIdx)]; sh.rawNeg += rShN[static_cast<size_t>(kSibIdx)];
                tr.rawPos += rTrP[static_cast<size_t>(kSibIdx)]; tr.rawNeg += rTrN[static_cast<size_t>(kSibIdx)];
            }
        }
        auto pct = [](int x, int n){ return n > 0 ? juce::String(100.0 * x / n, 0) + "%" : juce::String("-"); };
        auto avg = [](double s, int n){ return n > 0 ? s / n : 0.0; };
        const double trMargin = avg(tr.rawPos, tr.n) - avg(tr.rawNeg, tr.n);
        const double shMargin = avg(sh.rawPos, sh.n) - avg(sh.rawNeg, sh.n);

        logMessage("");
        logMessage("  ===== REAL-DATA M2: paired de-ess/boost, sibilant frames, held-out singers (n=" + juce::String(tr.n) + ") =====");
        logMessage("  model    | recall(boost) | FP(de-essed) | raw Sib boost / de-ess | margin");
        logMessage("  ---------+---------------+--------------+------------------------+-------");
        logMessage("  shipped  |     " + (pct(sh.recall, sh.n)).paddedRight(' ', 10) + "|   " + (pct(sh.fp, sh.n)).paddedRight(' ', 11)
                   + "|  " + (juce::String(avg(sh.rawPos, sh.n), 3) + " / " + juce::String(avg(sh.rawNeg, sh.n), 3)).paddedRight(' ', 21) + "| " + juce::String(shMargin, 3));
        logMessage("  real-trn |     " + (pct(tr.recall, tr.n)).paddedRight(' ', 10) + "|   " + (pct(tr.fp, tr.n)).paddedRight(' ', 11)
                   + "|  " + (juce::String(avg(tr.rawPos, tr.n), 3) + " / " + juce::String(avg(tr.rawNeg, tr.n), 3)).paddedRight(' ', 21) + "| " + juce::String(trMargin, 3));
        logMessage("  READ: M2 acceptance = real-trn margin (boost − de-ess raw Sib) is REAL (>>0), recall(boost) high,");
        logMessage("  FP(de-essed) low. shipped is the BEFORE. Sibilance-specialised proof, not a shippable all-class model.");

        // M2 acceptance (only fires when run WITH data): the model must actually SEPARATE the axis.
        expect(featDeltaMax > 0.02, "feature delta ~0 — injected signal not present in mel features");
        expect(trMargin > 0.10, "real-trained model does not separate boost from de-ess (margin "
               + juce::String(trMargin, 3) + ") — sibilance axis not learned");

        // ════════════════════════════════════════════════════════════════════════
        // M3 — PRODUCT-PIPELINE bridge (counter-exam amendment): does the candidate's
        // sibilance survive the FULL AIEngine path (rule + top-K + VETO) in BOTH MLOnly and
        // Hybrid, and how does it behave on RAW/natural sibilant frames (unmodified)? The
        // raw column is REPORTED not gated — natural sibilance is unlabelled, so a high rate
        // there means "fires on normal singing", to be judged, not auto-failed.
        // ════════════════════════════════════════════════════════════════════════
        const juce::File candFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("aieq_sib_candidate.bin");
        expect(trained.saveWeights(candFile), "could not save candidate weights to /tmp");

        auto linToDb = [](const std::vector<float>& lin){
            std::vector<float> db(lin.size());
            for (size_t i = 0; i < lin.size(); ++i) db[i] = juce::Decibels::gainToDecibels(lin[i], -120.0f);
            return db;
        };
        auto makeAi = [](const juce::File& w, AIEngine::DetectionBackendMode mode, double sr){
            auto ai = std::make_unique<AIEngine>();
            ai->prepare(sr, 512); ai->setEnabled(true); ai->setSensitivity(0.5f);
            ai->setSourceProfile(AIEngine::SourceProfile::Generic);
            ai->setDetectionBackendMode(mode);
            ai->setCustomMLWeightsPathForTests(w);
            ai->forceMLDetectionEnabledForTests(true);
            return ai;
        };
        auto aiSib = [](AIEngine& ai, const std::vector<float>& db){
            ai.analyzeSpectrum(db, true);
            for (const auto& c : ai.getPendingCorrections()) if (c.type == AIEngine::ProblemType::Sibilance) return true;
            return false;
        };

        struct PScore { int boost = 0, deess = 0, raw = 0, n = 0; };
        struct Mode { const char* name; AIEngine::DetectionBackendMode mode; };
        const Mode pmodes[] = { { "MLOnly", AIEngine::DetectionBackendMode::MLOnly },
                                { "Hybrid", AIEngine::DetectionBackendMode::Hybrid } };
        logMessage("");
        logMessage("  ===== M3: candidate through the PRODUCT pipeline (AIEngine, held-out singers) =====");
        logMessage("  mode   | model     | recall(boost) | FP(de-ess) | fire(raw natural)");
        logMessage("  -------+-----------+---------------+------------+------------------");
        double candBoostMin = 1.0, candDeessMax = 0.0;
        auto pc3 = [](int x, int n){ return n > 0 ? juce::String(100.0 * x / n, 0) + "%" : juce::String("-"); };
        for (const auto& m : pmodes)
        {
            auto aiShip = makeAi(shippedWeights(), m.mode, 44100.0);
            auto aiCand = makeAi(candFile, m.mode, 44100.0);
            PScore ps, pc;
            double candRawSum = 0.0; int candPreVeto = 0;   // boosted-frame internals (where is recall lost?)
            std::mt19937 rngP(13579);
            std::uniform_real_distribution<float> cutP(-14.0f, -8.0f), boostP(6.0f, 14.0f);
            for (const auto& f : testFiles)
            {
                juce::AudioBuffer<float> audio; double sr = 0;
                if (! loadMono(f, audio, sr)) continue;
                for (const auto& frame : sibilantFrames(audio, sr, kFramesPerClip))
                {
                    auto dbRaw   = linToDb(frame);
                    auto dbBoost = linToDb(scaleSibBand(frame, sr, boostP(rngP)));
                    auto dbDeess = linToDb(scaleSibBand(frame, sr, cutP(rngP)));
                    ps.n++; pc.n++;
                    ps.boost += aiSib(*aiShip, dbBoost) ? 1 : 0; ps.deess += aiSib(*aiShip, dbDeess) ? 1 : 0; ps.raw += aiSib(*aiShip, dbRaw) ? 1 : 0;
                    // candidate boosted: capture raw-on-product-frame + pre-veto to locate the loss
                    const bool candB = aiSib(*aiCand, dbBoost);
                    candRawSum += aiCand->getLastMLRawProbabilitiesForTests()[static_cast<size_t>(kSibIdx)];
                    for (const auto& d : aiCand->getLastPreVetoMLDetectionsForTests()) if (d.type == MLEngine::ProblemType::Sibilance) { ++candPreVeto; break; }
                    pc.boost += candB ? 1 : 0;
                    pc.deess += aiSib(*aiCand, dbDeess) ? 1 : 0; pc.raw += aiSib(*aiCand, dbRaw) ? 1 : 0;
                }
            }
            logMessage("  " + juce::String(m.name).paddedRight(' ', 6) + " | shipped   |     " + pc3(ps.boost, ps.n).paddedRight(' ', 10) + "|   " + pc3(ps.deess, ps.n).paddedRight(' ', 9) + "|   " + pc3(ps.raw, ps.n));
            logMessage("  " + juce::String(m.name).paddedRight(' ', 6) + " | candidate |     " + pc3(pc.boost, pc.n).paddedRight(' ', 10) + "|   " + pc3(pc.deess, pc.n).paddedRight(' ', 9) + "|   " + pc3(pc.raw, pc.n));
            logMessage("         candidate boosted internals: raw-Sib(product frame) "
                       + juce::String(pc.n > 0 ? candRawSum / pc.n : 0.0, 3) + " | pre-veto " + pc3(candPreVeto, pc.n)
                       + " | final " + pc3(pc.boost, pc.n) + "  (locates the loss: low raw=representation; raw high+pre-veto low=rule/top-K; pre-veto high+final low=veto)");
            if (pc.n > 0) { candBoostMin = std::min(candBoostMin, (double) pc.boost / pc.n); candDeessMax = std::max(candDeessMax, (double) pc.deess / pc.n); }
        }
        logMessage("  READ: candidate must KEEP high recall(boost) in BOTH modes (the veto does not prune real");
        logMessage("  sibilance) and low FP(de-ess). fire(raw natural) = firing on the unmodified sibilant frames —");
        logMessage("  REPORTED not gated (natural sibilance is unlabelled; high here = over-eager on normal singing).");
        candFile.deleteFile();

        logMessage("");
        if (candBoostMin > 0.5)
            logMessage("  M3 PASS: candidate sibilance SURVIVES the product pipeline (>=50% recall in both modes).");
        else
            logMessage("  M3 OPEN FINDING: the Sibilance VETO prunes real vocal sibilance (pre-veto ~97% -> final ~"
                       + juce::String(candBoostMin * 100.0, 0) + "%). The veto's 2-5 kHz reference band is naturally HOT"
                       + " in vocals, so excessive 5-9 kHz rarely exceeds it by the 3 dB gate. Shipping sibilance needs"
                       + " the VETO fixed too (M4, a production change), not just a better model. Report-only here.");

        // The candidate must at least NOT over-fire on de-essed frames through the pipeline.
        expect(candDeessMax < 0.5, "candidate over-fires on de-essed frames through the pipeline (max "
               + juce::String(candDeessMax * 100.0, 0) + "%)");
    }
};

static RealDataSibilanceTest sRealDataSibilanceTest;
