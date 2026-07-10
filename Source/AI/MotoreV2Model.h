#pragma once

// ============================================================================
// Motore v2 — A0 walking skeleton (gated loader)
// ----------------------------------------------------------------------------
// Loads an RTNeural model exported from the Python side as JSON and runs a
// single forward. Its purpose in A0 is to DE-RISK the PyTorch/numpy -> JSON ->
// C++ inference bridge, off the shipped path.
//
// Entirely INERT unless AIEQ_ENABLE_MOTORE_V2 is defined: this header compiles
// to nothing otherwise, so it does NOT touch the shipped MLP forward
// (MLEngine::detectProblems) or the heuristic engine. The production wiring
// into MLEngine lands in Phase A5, after A0 is green and Manus has designed the
// real architecture (A3).
//
// Output contract (product-v2): the model emits 17 RAW logits, laid out as the
// concat head [8 classes | 1 presence | 8 freq]. The per-segment sigmoid is the
// CONSUMER's job (MLEngine, A5), not the model's — A0 validates the raw bridge.
// ============================================================================

#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2

#include <RTNeural/RTNeural.h>

#include <fstream>
#include <memory>
#include <string>

namespace aieq
{

class MotoreV2Model
{
public:
    static constexpr int kProductV2OutSize = 17;   // [8 classes | 1 presence | 8 freq]

    /** Loads an RTNeural JSON model. Returns false on parse failure or if the
        output width != expectedOut (shape validation — the A5 ship format will
        additionally carry an FNV-1a integrity checksum, reusing the
        loadWeightsV2 logic; A0 relies on shape + the parity test). */
    bool loadFromJsonFile(const std::string& path, int expectedOut = kProductV2OutSize)
    {
        std::ifstream stream(path, std::ifstream::binary);
        if (! stream.good())
            return false;

        model = RTNeural::json_parser::parseJson<float>(stream, /*debug*/ false);
        if (model == nullptr)
            return false;

        if (model->getOutSize() != expectedOut)
        {
            model.reset();
            return false;
        }
        return true;
    }

    bool isLoaded() const noexcept { return model != nullptr; }
    int  inSize()   const noexcept { return model ? model->layers[0]->in_size : 0; }
    int  outSize()  const noexcept { return model ? model->getOutSize() : 0; }

    /** Zeroes the temporal state (Conv1D ring buffers). Call before feeding a
        new analysis window: the A3 CNN is stateful frame-by-frame. */
    void reset() { model->reset(); }

    /** Feeds one frame. Returns a pointer to outSize() raw logits owned by the
        model (valid until the next forward). For the A3 CNN the prediction for
        a window is the output of the LAST frame's forward. Caller must ensure
        isLoaded(). */
    const float* forward(const float* input)
    {
        model->forward(input);
        return model->getOutputs();
    }

private:
    std::unique_ptr<RTNeural::Model<float>> model;
};

} // namespace aieq

#endif // AIEQ_ENABLE_MOTORE_V2
