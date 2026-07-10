// ============================================================================
// Motore v2 — A2 feature parity test (category "AI", gated)
// ----------------------------------------------------------------------------
// Validates the FEATURE contract between Python (ml_v2/feature.py, the A4
// training pipeline) and C++ (the REAL PerceptualFrontEnd + aieq::melBandsFromDb,
// the A5 inference pipeline): loads the committed WAV fixture, streams it
// through PerceptualFrontEnd at 44.1k, converts every rawDb frame to 64
// log-mel bands, and asserts max|Delta| < 1e-5 against the Python-computed
// frames — across ALL frames, not just one.
//
// Compiled ONLY when AIEQ_ENABLE_MOTORE_V2 is defined (see CMakeLists).
// ============================================================================

#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "AI/MotoreV2Features.h"
#include "AI/PerceptualFrontEnd.h"

#include <chrono>
#include <cmath>
#include <vector>

#ifndef AIEQ_MOTORE_V2_A2_DATA
#error "AIEQ_MOTORE_V2_A2_DATA (fixture dir) must be defined when AIEQ_ENABLE_MOTORE_V2 is on"
#endif

class MotoreV2FeatureParityTest : public juce::UnitTest
{
public:
    MotoreV2FeatureParityTest()
        : juce::UnitTest("Motore v2 A2 Feature Parity (log-mel)", "AI") {}

    static std::vector<std::vector<float>> readCsvRows(const juce::File& f)
    {
        std::vector<std::vector<float>> rows;
        juce::StringArray lines;
        f.readLines(lines);
        for (const auto& line : lines)
        {
            if (line.trim().isEmpty())
                continue;
            std::vector<float> row;
            juce::StringArray cells;
            cells.addTokens(line, ",", "");
            for (const auto& cell : cells)
                row.push_back(cell.trim().getFloatValue());
            rows.push_back(std::move(row));
        }
        return rows;
    }

    void runTest() override
    {
        const juce::File dataDir(AIEQ_MOTORE_V2_A2_DATA);
        constexpr double kSr = 44100.0;
        constexpr int kMels = 64;

        beginTest("fixture present");
        const auto wavFile = dataDir.getChildFile("input.wav");
        const auto melCsv  = dataDir.getChildFile("expected_mel.csv");
        expect(wavFile.existsAsFile(), "input.wav missing: " + wavFile.getFullPathName());
        expect(melCsv.existsAsFile(), "expected_mel.csv missing");
        if (! (wavFile.existsAsFile() && melCsv.existsAsFile()))
            return;

        beginTest("WAV load (float32 mono)");
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(wavFile));
        expect(reader != nullptr, "cannot read input.wav");
        if (reader == nullptr) return;
        expectEquals((int) reader->sampleRate, (int) kSr, "fixture must be 44.1k");
        const int numSamples = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> buf(1, numSamples);
        reader->read(&buf, 0, numSamples, 0, true, false);

        beginTest("front-end frames match fixture frame count");
        PerceptualFrontEnd fe;
        fe.prepare(kSr);
        const auto frames = fe.analyzeAll(buf.getReadPointer(0), numSamples);
        const auto expected = readCsvRows(melCsv);
        logMessage("  frames C++ = " + juce::String(frames.size())
                   + ", frames Python = " + juce::String(expected.size()));
        expectEquals((int) frames.size(), (int) expected.size(),
                     "frame count mismatch C++ vs Python");
        if (frames.size() != expected.size() || frames.empty())
            return;
        expectEquals((int) expected[0].size(), kMels, "CSV must have 64 mel columns");

        beginTest("log-mel parity: max|Delta| < 1e-5 across ALL frames");
        double maxAbsDelta = 0.0;
        int worstFrame = -1, worstBand = -1;
        for (size_t f = 0; f < frames.size(); ++f)
        {
            const auto mel = aieq::melBandsFromDb(frames[f].rawDb.data(),
                                                  (int) frames[f].rawDb.size(),
                                                  kSr, kMels);
            for (int b = 0; b < kMels; ++b)
            {
                const double d = std::fabs((double) mel[(size_t) b]
                                           - (double) expected[f][(size_t) b]);
                if (d > maxAbsDelta)
                {
                    maxAbsDelta = d;
                    worstFrame = (int) f;
                    worstBand = b;
                }
            }
        }
        logMessage("  max|delta| = " + juce::String(maxAbsDelta, 3, true)
                   + " (frame " + juce::String(worstFrame)
                   + ", band " + juce::String(worstBand) + ")");
        expect(maxAbsDelta < 1e-5,
               "feature parity breach: max|delta| = " + juce::String(maxAbsDelta, 9));

        beginTest("per-frame mel cost (report-only)");
        constexpr int N = 20000;
        volatile float sink = 0.0f;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i)
        {
            const auto mel = aieq::melBandsFromDb(frames[0].rawDb.data(),
                                                  (int) frames[0].rawDb.size(),
                                                  kSr, kMels);
            sink += mel[0];
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
        juce::ignoreUnused(sink);
        logMessage("  melBandsFromDb cost = " + juce::String(us, 3)
                   + " us/frame (2049 bins -> 64 mel)");
        expect(us < 1000.0, "mel extraction must be trivially cheap per frame");
    }
};

static MotoreV2FeatureParityTest motoreV2FeatureParityTest;

#endif // AIEQ_ENABLE_MOTORE_V2
