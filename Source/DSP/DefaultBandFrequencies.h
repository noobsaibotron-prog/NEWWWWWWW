#pragma once

#include <array>

namespace AIEQDSP
{

// One authoritative, host-valid default layout for all 24 bands. Every value
// lies inside the public 20 Hz .. 20 kHz parameter range; runtime designers
// still clamp to the current sample-rate Nyquist limit.
inline constexpr std::array<float, 24> defaultBandFrequencies {
    31.0f,    50.0f,    80.0f,    120.0f,   170.0f,   250.0f,
    350.0f,   500.0f,   700.0f,   1000.0f,  1400.0f,  2000.0f,
    2800.0f,  4000.0f,  5600.0f,  8000.0f,  10500.0f, 12500.0f,
    14500.0f, 16000.0f, 17500.0f, 18500.0f, 19500.0f, 20000.0f
};

} // namespace AIEQDSP
