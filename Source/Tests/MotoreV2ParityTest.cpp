// ============================================================================
// Motore v2 — A3 parity test (category "AI", gated)
// ----------------------------------------------------------------------------
// Validates the Python -> JSON -> RTNeural C++ bridge on the REAL A3
// architecture (3x dilated causal Conv1D + 2x Dense, [17]-concat head):
// loads the committed model (ml_v2/a3_gen_fixture.py), reset()s the temporal
// state, streams the 32-frame window, and asserts |Delta| < 1e-5 vs the
// Python ground truth on the LAST forward's 17 outputs. Also measures the
// per-WINDOW cost (reset + 32 forwards) — the analysis-thread budget unit.
//
// Compiled ONLY when AIEQ_ENABLE_MOTORE_V2 is defined (see CMakeLists). The
// no-arg test gate on the shipped configuration never sees this file.
// ============================================================================

#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2

#include <juce_core/juce_core.h>

#include "AI/MotoreV2Model.h"

#include <chrono>
#include <cmath>
#include <vector>

#ifndef AIEQ_MOTORE_V2_A3_DATA
#error "AIEQ_MOTORE_V2_A3_DATA (fixture dir) must be defined when AIEQ_ENABLE_MOTORE_V2 is on"
#endif

class MotoreV2ParityTest : public juce::UnitTest
{
public:
    MotoreV2ParityTest() : juce::UnitTest("Motore v2 A3 Parity (RTNeural bridge)", "AI") {}

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
        const juce::File dataDir(AIEQ_MOTORE_V2_A3_DATA);

        beginTest("fixture present");
        const auto modelJson = dataDir.getChildFile("model.json");
        const auto inputCsv  = dataDir.getChildFile("input.csv");
        const auto expectCsv = dataDir.getChildFile("expected.csv");
        expect(modelJson.existsAsFile(), "model.json missing: " + modelJson.getFullPathName());
        expect(inputCsv.existsAsFile(),  "input.csv missing");
        expect(expectCsv.existsAsFile(), "expected.csv missing");
        if (! (modelJson.existsAsFile() && inputCsv.existsAsFile() && expectCsv.existsAsFile()))
            return;

        beginTest("RTNeural JSON load + 17-output shape (A3 CNN)");
        aieq::MotoreV2Model net;
        const bool loaded = net.loadFromJsonFile(modelJson.getFullPathName().toStdString());
        expect(loaded, "loadFromJsonFile failed");
        if (! loaded) return;
        expectEquals(net.outSize(), 17, "output width must be the product-v2 concat head [17]");

        const auto frames   = readCsvRows(inputCsv);    // 32 x 64
        const auto expected = readCsvRows(expectCsv);   // 17 x 1
        expectEquals((int) frames.size(), 32, "window must be 32 frames");
        expectEquals((int) frames[0].size(), net.inSize(), "frame width must match model in_size");
        expectEquals((int) expected.size(), 17, "expected.csv must have 17 values");
        if ((int) frames.size() != 32 || (int) frames[0].size() != net.inSize()
            || expected.size() != 17)
            return;

        beginTest("sequence parity: |Delta| < 1e-5 on the last forward's 17 outputs");
        net.reset();
        const float* out = nullptr;
        for (const auto& frame : frames)
            out = net.forward(frame.data());

        double maxAbsDelta = 0.0;
        for (int i = 0; i < 17; ++i)
        {
            const double d = std::fabs((double) out[i] - (double) expected[(size_t) i][0]);
            if (d > maxAbsDelta) maxAbsDelta = d;
        }
        logMessage("  max|delta| over 17 outputs = "
                   + juce::String(maxAbsDelta, 3, true /*scientific*/));
        expect(maxAbsDelta < 1e-5,
               "parity breach: max|delta| = " + juce::String(maxAbsDelta, 9));

        beginTest("determinism: reset + re-stream reproduces the same outputs");
        net.reset();
        const float* out2 = nullptr;
        for (const auto& frame : frames)
            out2 = net.forward(frame.data());
        double reDelta = 0.0;
        for (int i = 0; i < 17; ++i)
            reDelta = std::max(reDelta, std::fabs((double) out2[i] - (double) expected[(size_t) i][0]));
        expect(reDelta < 1e-5, "re-streamed window diverges: " + juce::String(reDelta, 9));

        beginTest("per-window cost (report-only)");
        constexpr int N = 2000;
        volatile float sink = 0.0f;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int w = 0; w < N; ++w)
        {
            net.reset();
            const float* o = nullptr;
            for (const auto& frame : frames)
                o = net.forward(frame.data());
            sink += o[0];
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / N;
        juce::ignoreUnused(sink);
        logMessage("  window cost = " + juce::String(ms, 4)
                   + " ms (reset + 32 forwards, A3 ~50.5k params)");
        logMessage("  budget context: analysis-thread hop ~250 ms, off audio thread");
        expect(ms < 125.0, "window cost must stay under 50% of the analysis hop");
    }
};

static MotoreV2ParityTest motoreV2ParityTest;

#endif // AIEQ_ENABLE_MOTORE_V2
