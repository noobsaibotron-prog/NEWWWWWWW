#pragma once

#include "SemanticIntent.h"

#include <string_view>

namespace AIEQPerceptual
{

/**
 * Deterministic domain-specific compiler for mix-engineering language.
 *
 * It intentionally does not depend on seed22, GloVe, Torch, the GUI, or the
 * audio thread. Reliability comes from a small explicit grammar with local
 * modifier scope and explicit constraints rather than broad NLP claims.
 */
class SemanticIntentCompiler
{
public:
    [[nodiscard]] SemanticIntent compile(std::string_view text) const;
};

} // namespace AIEQPerceptual
