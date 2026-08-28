#pragma once

#include <string>

namespace EmberUI
{

/** How a pending problem entered the list.

    Live is gated by the 8-frame temporal ring. Capture is a one-shot and
    must never display a fabricated n/8 stability count.
*/
enum class PersistenceSource : unsigned char
{
    Live = 0,
    Capture
};

/** Sidecar for one pending correction. Same index, same lock, copied out
    together. Not a second confidence score.
*/
struct PersistenceEvidence
{
    int hits = 0;
    int windowSize = 0;
    float persistenceFraction = 0.0f;
    bool historyReady = false;
    PersistenceSource source = PersistenceSource::Live;

    [[nodiscard]] bool showsLiveStability() const noexcept
    {
        return source == PersistenceSource::Live && historyReady && windowSize > 0;
    }
};

/** Compact row glyph: "91%" or "91%  7/8". Not a second confidence score. */
inline std::string formatCompactConfidence(int percent, const PersistenceEvidence& ev)
{
    std::string text = std::to_string(percent) + "%";
    if (ev.showsLiveStability())
        text += "  " + std::to_string(ev.hits) + "/" + std::to_string(ev.windowSize);
    return text;
}

} // namespace EmberUI
