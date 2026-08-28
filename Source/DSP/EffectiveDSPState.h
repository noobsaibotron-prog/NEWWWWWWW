#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace EmberDSP
{

inline constexpr int kEffectiveDSPBandCount = 24;

enum class EffectiveDSPSource : std::uint8_t
{
    CommittedA = 0,
    ProjectedB = 1
};

/** Fixed-size band payload for committed A and projected B. Message thread
    fills it; the audio thread may copy it. No heap types. */
struct PackedBandDSPState
{
    float frequency = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
    float sidechainFrequency = 1000.0f;
    float sidechainQ = 1.0f;
    float dynThreshold = -24.0f;
    float dynRatio = 2.0f;
    float dynAttack = 10.0f;
    float dynRelease = 100.0f;
    float dynRange = 24.0f;
    float dynKnee = 6.0f;
    std::int32_t type = 2;
    std::int32_t slope = 0;
    std::int32_t curveMode = 1;
    std::int32_t dynMode = 0;
    std::int32_t dynTrigger = 0;
    std::int32_t detectionMode = 1;
    std::int32_t detectorSource = 0;
    std::uint8_t enabled = 1;
    std::uint8_t solo = 0;
    std::uint8_t bandOwnedByDynamicStage = 0;
    std::uint8_t pad = 0;
};

struct EffectiveDSPState
{
    std::array<PackedBandDSPState, kEffectiveDSPBandCount> bands {};
    std::int32_t effectiveActiveBandCount = 8;
    std::uint8_t dynEqEnabled = 1;
    std::uint8_t source = static_cast<std::uint8_t>(EffectiveDSPSource::CommittedA);
    std::uint8_t pad[2] {};
    /** Monotonic id of this preview/plan payload. Distinct from the two epochs:
        two plans against the same committed A must not share this value. */
    std::uint64_t previewGeneration = 0;
    /** Committed-A clock captured when the projection was built — not "now". */
    std::uint64_t projectionBaseEpoch = 0;
    /** Mix/routing/prepare clock captured when the projection was built. */
    std::uint64_t auditionContextEpoch = 0;
};

static_assert(std::is_trivially_copyable_v<PackedBandDSPState>,
              "PackedBandDSPState must be mailbox-safe");
static_assert(std::is_trivially_copyable_v<EffectiveDSPState>,
              "EffectiveDSPState must be mailbox-safe");
static_assert(sizeof(EffectiveDSPState) <= 4096,
              "EffectiveDSPState must stay a small RT-copyable payload");

} // namespace EmberDSP
