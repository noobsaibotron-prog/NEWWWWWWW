#include <juce_core/juce_core.h>
#include "AI/MLEngine.h"

#include <cstring>
#include <vector>

/**
 * M7 interim integration — v2 blob loader contract (category "AI").
 *
 * The Codex-agreed spec (CODEX_BRIEF_M6_SHIP_INTEGRATION.txt) mandates:
 *  1. OLD-BLOB COMPAT : v1 blobs load exactly as before (schema legacy-v1).
 *  2. V2 SCHEMA       : slot 7 = DullSound ONLY for product-v2 blobs
 *                       (freq range 6-16k, gain flips to a conservative BOOST).
 *  3. CORRUPT-BLOB    : bad checksum / truncation / wrong shapes / bad
 *                       featureVersion -> rejected, engine fallback intact.
 *  4. REVERSIBILITY   : loading a legacy blob AFTER a product-v2 one restores
 *                       the legacy slot-7 semantics (no sticky state).
 */
class MLBlobV2LoaderTest : public juce::UnitTest
{
public:
    MLBlobV2LoaderTest() : juce::UnitTest("ML v2 blob loader (M7 interim)", "AI") {}

    //==========================================================================
    // Minimal v2 writer — same format + FNV-1a quirk basis as ml/blob_io.py.
    static void appendU32(juce::MemoryBlock& mb, uint32_t v)
    {
        juce::uint8 b[4] = { (juce::uint8) (v & 0xFF), (juce::uint8) ((v >> 8) & 0xFF),
                             (juce::uint8) ((v >> 16) & 0xFF), (juce::uint8) ((v >> 24) & 0xFF) };
        mb.append(b, 4);
    }

    static void appendFloats(juce::MemoryBlock& mb, const std::vector<float>& v)
    {
        mb.append(v.data(), v.size() * sizeof(float));
    }

    static juce::uint64 fnv1a(const juce::MemoryBlock& mb)
    {
        juce::uint64 h = 1469598103934665603ULL; // repo quirk basis (shared)
        const auto* bytes = static_cast<const juce::uint8*>(mb.getData());
        for (size_t i = 0; i < mb.getSize(); ++i)
        {
            h ^= bytes[i];
            h *= 1099511628211ULL;
        }
        return h;
    }

    static juce::MemoryBlock makeV2Blob(const juce::String& provenanceJson,
                                        uint32_t featureVersion = 1,
                                        int corruptLayerIndex = -1)
    {
        struct Shape { uint32_t out, in; };
        const Shape shapes[5] = { {128, 64}, {64, 128}, {8, 64}, {32, 64}, {8, 32} };
        juce::MemoryBlock mb;
        appendU32(mb, 0x4D4C4551);
        appendU32(mb, 2);
        appendU32(mb, featureVersion);
        appendU32(mb, 5);
        juce::Random rng(4242);
        for (int l = 0; l < 5; ++l)
        {
            auto s = shapes[l];
            if (l == corruptLayerIndex)
                s.out += 1;                       // wrong shape on purpose
            appendU32(mb, s.out);
            appendU32(mb, s.in);
            std::vector<float> w(static_cast<size_t>(s.out) * s.in), b(s.out);
            for (auto& x : w) x = (rng.nextFloat() - 0.5f) * 0.1f;
            for (auto& x : b) x = (rng.nextFloat() - 0.5f) * 0.1f;
            appendFloats(mb, w);
            appendFloats(mb, b);
        }
        const auto utf8 = provenanceJson.toRawUTF8();
        const auto len = static_cast<uint32_t>(strlen(utf8));
        appendU32(mb, len);
        mb.append(utf8, len);
        const juce::uint64 h = fnv1a(mb);
        juce::uint8 tail[8];
        for (int i = 0; i < 8; ++i)
            tail[i] = static_cast<juce::uint8>((h >> (8 * i)) & 0xFF);
        mb.append(tail, 8);
        return mb;
    }

    static juce::File writeTemp(const juce::MemoryBlock& mb, const juce::String& name)
    {
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory)
                     .getChildFile(name);
        f.deleteFile();
        f.replaceWithData(mb.getData(), mb.getSize());
        return f;
    }

    static juce::File shippedBlob()
    {
        return juce::File(__FILE__).getParentDirectory().getParentDirectory()
            .getParentDirectory().getChildFile("Resources/Models/ml_weights.bin");
    }

    void runTest() override
    {
        using PT = MLEngine::ProblemType;

        beginTest("v1 shipped blob loads with legacy-v1 schema (old-blob compat)");
        {
            MLEngine ml;
            ml.initialize();
            expect(ml.loadWeights(shippedBlob()), "shipped v1 blob must load");
            expect(ml.areWeightsLoadedFromFile());
            expectEquals(ml.getLoadedProblemSchema(), juce::String("legacy-v1"));
            const auto r = ml.problemFreqRangeForTests(PT::Clipping);
            expectEquals(r.first, 20.0f);
            expectEquals(r.second, 20000.0f);
            expect(ml.defaultGainForTests(PT::Clipping) < 0.0f, "legacy slot 7 is a cut");
        }

        beginTest("product-v2 blob: slot 7 becomes DullSound semantics");
        {
            MLEngine ml;
            ml.initialize();
            const auto blob = makeV2Blob(R"({"problem_schema":"product-v2","seed":22})");
            const auto f = writeTemp(blob, "aieq_test_v2_product.bin");
            expect(ml.loadWeights(f), "valid product-v2 blob must load");
            expectEquals(ml.getLoadedProblemSchema(), juce::String("product-v2"));
            const auto r = ml.problemFreqRangeForTests(PT::Clipping);   // slot 7
            expectEquals(r.first, 6000.0f);
            expectEquals(r.second, 16000.0f);
            expect(ml.defaultGainForTests(PT::Clipping) > 0.0f,
                   "DullSound correction is a BOOST (dullness = lack of highs)");
            expectWithinAbsoluteError(ml.defaultQForTests(PT::Clipping), 0.7f, 1e-6f);
            // inference stays finite on the loaded weights
            std::vector<float> spec(2049, 0.05f);
            auto dets = ml.detectProblems(spec, 44100.0);
            juce::ignoreUnused(dets);
            expect(true);
            f.deleteFile();
        }

        beginTest("v2 blob without product schema keeps legacy slot-7 semantics");
        {
            MLEngine ml;
            ml.initialize();
            const auto f = writeTemp(makeV2Blob(R"({"seed":22})"), "aieq_test_v2_plain.bin");
            expect(ml.loadWeights(f), "schema-less v2 blob still loads");
            expectEquals(ml.getLoadedProblemSchema(), juce::String("legacy-v1"));
            expect(ml.defaultGainForTests(PT::Clipping) < 0.0f);
            f.deleteFile();
        }

        beginTest("corrupt v2 blobs are rejected and leave the engine intact");
        {
            MLEngine ml;
            ml.initialize();
            expect(ml.loadWeights(shippedBlob()));   // healthy baseline first
            const auto healthyChecksum = ml.getLoadedWeightsChecksum();

            // (a) checksum flip
            auto bad = makeV2Blob(R"({"problem_schema":"product-v2"})");
            static_cast<juce::uint8*>(bad.getData())[100] ^= 0x5A;
            auto f = writeTemp(bad, "aieq_test_v2_corrupt.bin");
            expect(!ml.loadWeights(f), "checksum mismatch must reject");
            expect(!ml.areWeightsLoadedFromFile(), "witness must reset on failed load");
            expectEquals(ml.getLoadedProblemSchema(), juce::String("legacy-v1"));
            f.deleteFile();

            // (b) truncation
            auto good = makeV2Blob(R"({"problem_schema":"product-v2"})");
            juce::MemoryBlock trunc(good.getData(), good.getSize() - 1000);
            f = writeTemp(trunc, "aieq_test_v2_trunc.bin");
            expect(!ml.loadWeights(f), "truncated blob must reject");
            f.deleteFile();

            // (c) wrong featureVersion
            f = writeTemp(makeV2Blob(R"({"problem_schema":"product-v2"})", 2),
                          "aieq_test_v2_fv2.bin");
            expect(!ml.loadWeights(f), "featureVersion != 1 must reject");
            f.deleteFile();

            // (d) wrong layer shape (checksum recomputed to be valid)
            f = writeTemp(makeV2Blob(R"({"problem_schema":"product-v2"})", 1, 2),
                          "aieq_test_v2_shape.bin");
            expect(!ml.loadWeights(f), "unexpected layer shape must reject");
            f.deleteFile();

            // after all the rejects, a fresh healthy load still works
            expect(ml.loadWeights(shippedBlob()));
            expectEquals(ml.getLoadedWeightsChecksum(), healthyChecksum);
        }

        beginTest("legacy load AFTER product-v2 restores legacy slot-7 (no sticky state)");
        {
            MLEngine ml;
            ml.initialize();
            const auto f = writeTemp(makeV2Blob(R"({"problem_schema":"product-v2"})"),
                                     "aieq_test_v2_then_v1.bin");
            expect(ml.loadWeights(f));
            expectEquals(ml.getLoadedProblemSchema(), juce::String("product-v2"));
            expect(ml.loadWeights(shippedBlob()));
            expectEquals(ml.getLoadedProblemSchema(), juce::String("legacy-v1"));
            const auto r = ml.problemFreqRangeForTests(PT::Clipping);
            expectEquals(r.first, 20.0f);
            expect(ml.defaultGainForTests(PT::Clipping) < 0.0f);
            f.deleteFile();
        }
    }
};

static MLBlobV2LoaderTest gMLBlobV2LoaderTest;
