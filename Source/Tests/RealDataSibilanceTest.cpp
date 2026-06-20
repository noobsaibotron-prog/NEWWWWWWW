/**
 * RealDataSibilanceTest — Real-data program, MILESTONE 1 (category "RealData").
 *
 * Proof of the INJECTION LOOP on ONE class (Sibilance): teach the model on REAL vocal
 * audio (VocalSet) into which we inject a KNOWN sibilance problem (a multiplicative
 * Gaussian boost in 5.5–9 kHz — the same shape buildSyntheticSpectrum uses, but on a
 * REAL spectrum), then measure on HELD-OUT singers vs the shipped (synthetic-trained)
 * model. The label is exact and free (we put the boost there); the audio is real.
 *
 * Faithful representation: WAV → OfflineAnalysisPipeline (the validated mirror of the
 * plugin's SpectrumAnalyzer, dB) → decibelsToGain → linear magnitude — i.e. EXACTLY
 * what AIEngine feeds MLEngine at runtime. Measurement is at the MODEL level
 * (MLEngine::detectProblems / forwardRawProbabilities), since shipped Sibilance dies at
 * the model stage (0/12 pre-veto), so the veto is not the variable here.
 *
 * Report-only (no hard gate): this is the BEFORE/AFTER baseline for the program. The
 * "after" model is Sibilance-specialised (trained on Sib-positive + clean only) — a
 * proof the loop works, NOT a shippable all-class model. Skips cleanly when the env var
 * AIEQ_REALDATA_DIR is unset/missing (so it never blocks CI). Point it at the VocalSet
 * FULL dir, e.g. ~/aieq_data/real_audio/vocalset_extracted/FULL.
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

#include "../AI/MLEngine.h"
#include "Support/OfflineAnalysisPipeline.h"

namespace
{
constexpr int   kSibIdx       = static_cast<int>(MLEngine::ProblemType::Sibilance); // 3
constexpr int   kTrainClips   = 140;   // sampled from train singers
constexpr int   kTestClips    = 48;    // sampled from held-out singers
constexpr int   kEpochs       = 600;
constexpr float kLearningRate = 0.003f;

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

// WAV → one representative LINEAR-magnitude spectrum (mean over frames), in the exact
// representation AIEngine feeds MLEngine (mirror dB → decibelsToGain). Empty if too short.
std::vector<float> clipSpectrumLinear(const juce::AudioBuffer<float>& audio, double sr)
{
    aieq_test::OfflineAnalysisPipeline pipe(sr);
    auto frames = pipe.analyze(audio);
    if (frames.empty()) return {};
    const size_t bins = frames.front().size();
    std::vector<double> meanDb(bins, 0.0);
    for (const auto& fr : frames)
        for (size_t i = 0; i < bins; ++i) meanDb[i] += fr[i];
    std::vector<float> lin(bins, 0.0f);
    for (size_t i = 0; i < bins; ++i)
        lin[i] = juce::Decibels::decibelsToGain(static_cast<float>(meanDb[i] / static_cast<double>(frames.size())));
    return lin;
}

// Inject a known sibilance problem: multiplicative Gaussian boost (mirrors
// buildSyntheticSpectrum), centred in 5.5–9 kHz, prominence promDb. Returns a copy.
std::vector<float> injectSibilance(const std::vector<float>& lin, double sr,
                                   float centerHz, float promDb)
{
    std::vector<float> out = lin;
    const int fftSize = static_cast<int>(lin.size()) * 2;
    const float binHz = static_cast<float>(sr) / static_cast<float>(fftSize);
    const float sigmaHz = std::max(30.0f, centerHz * 0.08f);
    const float gain = juce::Decibels::decibelsToGain(promDb);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float freq = static_cast<float>(i) * binHz;
        const float d = (freq - centerHz) / sigmaHz;
        const float peak = std::exp(-0.5f * d * d);
        out[i] = std::max(0.0f, out[i] * (1.0f + (gain - 1.0f) * peak));
    }
    return out;
}

// Singer folder = first path component under the root (e.g. FULL/female1/... -> female1).
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
        beginTest("Inject known sibilance into REAL vocals, train, measure on held-out singers");

        const juce::File root = realDataDir();
        if (root == juce::File() || ! root.isDirectory())
        {
            logMessage("  SKIP: AIEQ_REALDATA_DIR unset or not a directory — no real audio to run on.");
            logMessage("  Set it to the VocalSet FULL dir to run this milestone.");
            return; // pass (nothing to measure)
        }

        // ── gather clips, split by singer (held-out singers = real generalization test) ──
        juce::Array<juce::File> wavs;
        root.findChildFiles(wavs, juce::File::findFiles, true, "*.wav");
        if (wavs.size() < 40)
        {
            logMessage("  SKIP: found only " + juce::String(wavs.size()) + " wav files under " + root.getFullPathName());
            return;
        }
        // held-out singers (not seen in training)
        const juce::StringArray testSingers { "female9", "female8", "male11", "male10" };
        juce::Array<juce::File> trainPool, testPool;
        for (const auto& f : wavs)
            (testSingers.contains(singerOf(f, root)) ? testPool : trainPool).add(f);

        std::mt19937 rng(20260620);
        auto sample = [&](juce::Array<juce::File>& pool, int n){
            juce::Array<juce::File> picked = pool;
            for (int i = picked.size() - 1; i > 0; --i) std::swap(picked.getReference(i), picked.getReference(static_cast<int>(rng() % static_cast<unsigned>(i + 1))));
            picked.resize(juce::jmin(n, picked.size()));
            return picked;
        };
        auto trainFiles = sample(trainPool, kTrainClips);
        auto testFiles  = sample(testPool, kTestClips);
        logMessage("  clips: " + juce::String(wavs.size()) + " total | train pool " + juce::String(trainPool.size())
                   + " (using " + juce::String(trainFiles.size()) + ") | held-out pool " + juce::String(testPool.size())
                   + " (using " + juce::String(testFiles.size()) + ")");

        // Sibilance freq range for the normalized frequency target.
        MLEngine probe; probe.initialize();
        const auto sibRange = probe.problemFreqRangeForTests(MLEngine::ProblemType::Sibilance);

        std::uniform_real_distribution<float> centerD(5500.0f, 9000.0f);
        std::uniform_real_distribution<float> promD(10.0f, 16.0f);

        // ── build dataset from train clips: clean (targets 0) + injected (Sib=1) ──
        MLEngine trained; trained.initialize();
        std::vector<MLEngine::TrainingSample> dataset;
        int built = 0;
        for (const auto& f : trainFiles)
        {
            juce::AudioBuffer<float> audio; double sr = 0;
            if (! loadMono(f, audio, sr)) continue;
            auto lin = clipSpectrumLinear(audio, sr);
            if (lin.empty()) continue;

            // clean negative
            MLEngine::TrainingSample neg;
            neg.melSpectrum = trained.melBandsFromSpectrumForTests(lin, sr);
            dataset.push_back(std::move(neg));

            // injected positive
            const float center = centerD(rng);
            const float prom = promD(rng);
            auto inj = injectSibilance(lin, sr, center, prom);
            MLEngine::TrainingSample pos;
            pos.melSpectrum = trained.melBandsFromSpectrumForTests(inj, sr);
            pos.problemTargets[static_cast<size_t>(kSibIdx)] = 1.0f;
            pos.frequencyTargets[static_cast<size_t>(kSibIdx)] =
                juce::jlimit(0.0f, 1.0f, (center - sibRange.first) / std::max(1.0f, sibRange.second - sibRange.first));
            dataset.push_back(std::move(pos));
            ++built;
        }
        logMessage("  built " + juce::String(dataset.size()) + " training samples from " + juce::String(built) + " real clips");
        if (built < 10) { logMessage("  SKIP: too few usable clips."); return; }

        // ── train the real-data model (Sibilance-specialised; proof of loop, not shippable) ──
        trained.initializeRandomWeights();
        trained.trainOnDataset(dataset, kEpochs, kLearningRate);

        // ── shipped (synthetic-trained) model for the BEFORE column ──
        MLEngine shipped; shipped.initialize();
        const bool shipOk = shipped.loadWeights(shippedWeights());
        expect(shipOk, "could not load shipped weights for the BEFORE comparison");

        // ── measure on held-out singers: Sibilance recall (injected) + clean FP ──
        auto detectsSib = [](MLEngine& m, const std::vector<float>& lin, double sr){
            auto dets = m.detectProblems(lin, sr);
            for (const auto& d : dets) if (d.type == MLEngine::ProblemType::Sibilance) return true;
            return false;
        };
        struct Score { int recall = 0, cleanFp = 0, n = 0; double rawInj = 0, rawClean = 0; };
        Score sh, tr;
        std::uniform_real_distribution<float> centerT(5500.0f, 9000.0f);
        std::uniform_real_distribution<float> promT(10.0f, 16.0f);
        std::mt19937 rngT(7777);
        for (const auto& f : testFiles)
        {
            juce::AudioBuffer<float> audio; double sr = 0;
            if (! loadMono(f, audio, sr)) continue;
            auto lin = clipSpectrumLinear(audio, sr);
            if (lin.empty()) continue;
            auto inj = injectSibilance(lin, sr, centerT(rngT), promT(rngT));

            std::array<float, MLEngine::numProblemTypes> rawShInj{}, rawShCl{}, rawTrInj{}, rawTrCl{};
            shipped.detectProblems(inj, sr, &rawShInj);
            shipped.detectProblems(lin, sr, &rawShCl);
            trained.detectProblems(inj, sr, &rawTrInj);
            trained.detectProblems(lin, sr, &rawTrCl);

            sh.n++; tr.n++;
            sh.recall  += detectsSib(shipped, inj, sr) ? 1 : 0;
            sh.cleanFp += detectsSib(shipped, lin, sr) ? 1 : 0;
            tr.recall  += detectsSib(trained, inj, sr) ? 1 : 0;
            tr.cleanFp += detectsSib(trained, lin, sr) ? 1 : 0;
            sh.rawInj += rawShInj[static_cast<size_t>(kSibIdx)]; sh.rawClean += rawShCl[static_cast<size_t>(kSibIdx)];
            tr.rawInj += rawTrInj[static_cast<size_t>(kSibIdx)]; tr.rawClean += rawTrCl[static_cast<size_t>(kSibIdx)];
        }

        auto pct = [](int x, int n){ return n > 0 ? juce::String(100.0 * x / n, 0) + "%" : juce::String("-"); };
        auto avg = [](double s, int n){ return n > 0 ? juce::String(s / n, 3) : juce::String("-"); };
        logMessage("");
        logMessage("  ===== REAL-DATA MILESTONE 1: Sibilance injection loop (held-out singers, n=" + juce::String(sh.n) + ") =====");
        logMessage("  model    | Sib recall (injected) | Sib clean-FP | raw Sib prob inj / clean");
        logMessage("  ---------+-----------------------+--------------+-------------------------");
        logMessage("  shipped  |        " + (pct(sh.recall, sh.n) + "  (" + juce::String(sh.recall) + "/" + juce::String(sh.n) + ")").paddedRight(' ', 14)
                   + "|   " + (pct(sh.cleanFp, sh.n)).paddedRight(' ', 11) + "|  " + avg(sh.rawInj, sh.n) + " / " + avg(sh.rawClean, sh.n));
        logMessage("  real-inj |        " + (pct(tr.recall, tr.n) + "  (" + juce::String(tr.recall) + "/" + juce::String(tr.n) + ")").paddedRight(' ', 14)
                   + "|   " + (pct(tr.cleanFp, tr.n)).paddedRight(' ', 11) + "|  " + avg(tr.rawInj, tr.n) + " / " + avg(tr.rawClean, tr.n));
        logMessage("  READ: shipped = synthetic-trained (the BEFORE). real-inj = trained on REAL vocals + injected");
        logMessage("  sibilance, evaluated on singers it never saw. Loop WORKS if real-inj recall >> shipped recall");
        logMessage("  while clean-FP stays low. This is a Sibilance-specialised proof, not a shippable all-class model.");

        // Soft signal (report-only): the injected raw prob should separate from clean for the trained model.
        expect(tr.n > 0, "no held-out clips evaluated");
    }
};

static RealDataSibilanceTest sRealDataSibilanceTest;
