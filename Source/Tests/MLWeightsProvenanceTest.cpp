#include <juce_core/juce_core.h>
#include "AI/MLEngine.h"

/**
 * ML weights provenance gate (AI-evolution A1, category AI-Contract).
 *
 * The shipped MLOnly default silently falls back to heuristics when
 * Resources/Models/ml_weights.bin is missing or unloadable — nothing else in
 * the default gate catches that. This test pins the EXACT shipped blob:
 *   - presence + single-record size (96,488 B = 8 header + 24,120 floats)
 *   - byte-level FNV-1a 64 checksum (seed22 blob, pinned 2026-07-03)
 *   - MLEngine::loadWeights succeeds and witnesses the same checksum
 *
 * Retraining the model is EXPECTED to update kPinnedChecksum — in the SAME
 * commit that ships the new blob (that is the provenance discipline; a diff
 * to this constant without a blob change, or vice versa, must fail review).
 */
class MLWeightsProvenanceTest : public juce::UnitTest
{
public:
    MLWeightsProvenanceTest()
        : juce::UnitTest("ML Weights Provenance — shipped blob presence/size/checksum",
                         "AI-Contract") {}

    void runTest() override
    {
        beginTest("shipped ml_weights.bin: presence, size, pinned checksum, loadability");

        // seed22 single-record blob (commit 71eef41b lineage).
        // NOTE: the checksum uses MLEngine's FNV constant 1469598103934665603
        // (one digit short of the standard FNV-1a offset basis
        // 14695981039346656037 — a long-standing quirk). It is self-consistent
        // across loader + this test + ml/blob_io.py; do NOT "fix" the constant
        // in one place only.
        constexpr juce::int64 kExpectedBytes = 96488;
        const juce::String kPinnedChecksum = "9d322f2466049202";

        // __FILE__ -> Source/Tests/ -> repo root (same pattern as AIBackendSweepTest)
        const juce::File blob =
            juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory()
                .getChildFile("Resources/Models/ml_weights.bin");

        expect(blob.existsAsFile(),
               "Resources/Models/ml_weights.bin MISSING — the MLOnly default would "
               "silently degrade to heuristics in shipping builds");
        if (!blob.existsAsFile())
            return;

        expectEquals(blob.getSize(), kExpectedBytes,
                     "blob size drifted from the single-record contract "
                     "(concatenated records or truncation)");

        // Independent byte-level FNV-1a 64 (do not trust the loader for the pin)
        {
            juce::MemoryBlock contents;
            expect(blob.loadFileAsData(contents), "failed to read blob bytes");
            juce::uint64 h = 1469598103934665603ULL;
            const auto* bytes = static_cast<const juce::uint8*>(contents.getData());
            for (size_t i = 0; i < contents.getSize(); ++i)
            {
                h ^= bytes[i];
                h *= 1099511628211ULL;
            }
            const juce::String fileChecksum =
                juce::String::toHexString(static_cast<juce::int64>(h));
            expectEquals(fileChecksum, kPinnedChecksum,
                         "blob bytes changed without updating the pinned checksum "
                         "(retrain must update pin + blob in the same commit)");
        }

        // Loader-level witness: MLEngine accepts the blob and reports the same identity
        MLEngine engine;
        engine.initialize();
        expect(engine.loadWeights(blob), "MLEngine::loadWeights rejected the shipped blob");
        expect(engine.areWeightsLoadedFromFile(), "loader did not mark weights as file-loaded");
        expectEquals(engine.getLoadedWeightsBytes(), kExpectedBytes);
        expectEquals(engine.getLoadedWeightsChecksum(), kPinnedChecksum,
                     "loader checksum witness disagrees with the byte-level pin");
    }
};

static MLWeightsProvenanceTest gMLWeightsProvenanceTest;
