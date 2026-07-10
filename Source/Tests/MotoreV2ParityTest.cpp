// ============================================================================
// Motore v2 — A0 walking skeleton parity test (category "AI", gated)
// ----------------------------------------------------------------------------
// Validates the PyTorch/numpy -> JSON -> RTNeural C++ bridge inside the real
// JUCE build: loads the committed dummy model (ml_v2/a0_gen_dummy.py fixture),
// runs the forward, and asserts |Delta| < 1e-5 vs the numpy ground truth on all
// 17 outputs. Also measures per-inference cost (reported; the budget is the
// analysis-thread duty cycle, not the audio callback).
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

#ifndef AIEQ_MOTORE_V2_A0_DATA
#error "AIEQ_MOTORE_V2_A0_DATA (fixture dir) must be defined when AIEQ_ENABLE_MOTORE_V2 is on"
#endif

class MotoreV2ParityTest : public juce::UnitTest
{
public:
    MotoreV2ParityTest() : juce::UnitTest("Motore v2 A0 Parity (RTNeural bridge)", "AI") {}

    static std::vector<float> readCsv(const juce::File& f)
    {
        std::vector<float> v;
        juce::StringArray lines;
        f.readLines(lines);
        for (const auto& line : lines)
            if (line.trim().isNotEmpty())
                v.push_back(line.trim().getFloatValue());
        return v;
    }

    void runTest() override
    {
        const juce::File dataDir(AIEQ_MOTORE_V2_A0_DATA);

        beginTest("fixture present");
        const auto modelJson = dataDir.getChildFile("dummy_model.json");
        const auto inputCsv  = dataDir.getChildFile("input.csv");
        const auto expectCsv = dataDir.getChildFile("expected.csv");
        expect(modelJson.existsAsFile(), "dummy_model.json missing: " + modelJson.getFullPathName());
        expect(inputCsv.existsAsFile(),  "input.csv missing");
        expect(expectCsv.existsAsFile(), "expected.csv missing");
        if (! (modelJson.existsAsFile() && inputCsv.existsAsFile() && expectCsv.existsAsFile()))
            return;

        beginTest("RTNeural JSON load + 17-output shape");
        aieq::MotoreV2Model net;
        const bool loaded = net.loadFromJsonFile(modelJson.getFullPathName().toStdString());
        expect(loaded, "loadFromJsonFile failed");
        if (! loaded) return;
        expectEquals(net.outSize(), 17, "output width must be the product-v2 concat head [17]");

        const auto input    = readCsv(inputCsv);
        const auto expected = readCsv(expectCsv);
        expectEquals((int) input.size(), net.inSize(), "input width must match model in_size");
        expectEquals((int) expected.size(), 17, "expected.csv must have 17 values");
        if ((int) input.size() != net.inSize() || expected.size() != 17)
            return;

        beginTest("parity |Delta| < 1e-5 on all 17 outputs");
        const float* out = net.forward(input.data());
        double maxAbsDelta = 0.0;
        for (int i = 0; i < 17; ++i)
        {
            const double d = std::fabs((double) out[i] - (double) expected[i]);
            if (d > maxAbsDelta) maxAbsDelta = d;
        }
        logMessage("  max|delta| over 17 outputs = "
                   + juce::String(maxAbsDelta, 3, true /*scientific*/));
        expect(maxAbsDelta < 1e-5,
               "parity breach: max|delta| = " + juce::String(maxAbsDelta, 9));

        beginTest("per-inference cost (report-only)");
        constexpr int N = 100000;
        volatile float sink = 0.0f;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N; ++i)
        {
            const float* o = net.forward(input.data());
            sink += o[0];
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
        juce::ignoreUnused(sink);
        logMessage("  inference cost = " + juce::String(us, 4) + " us/forward (dummy 16->32->17)");
        logMessage("  budget context: analysis-thread hop ~250 ms, off audio thread => ample");
        expect(us < 250000.0, "single inference must fit inside the analysis hop");
    }
};

static MotoreV2ParityTest motoreV2ParityTest;

#endif // AIEQ_ENABLE_MOTORE_V2
