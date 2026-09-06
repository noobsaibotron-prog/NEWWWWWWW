#pragma once
// Ember climate / thermal — stub matching GROK_BOT_JUCE_UI_V2.md §7.
// Real thermal.h was not found under Downloads/Desktop/repo; this implements
// the contracted math. Climate remains OFF by default in the editor.

#include <array>
#include <algorithm>
#include <cmath>

namespace ember
{

struct ThermalState
{
    static constexpr int kNumCells = 64;

    std::array<float, kNumCells> L {}; // log-cell levels (dB)
    std::array<float, kNumCells> T {}; // prominence heat 0..1

    void reset() noexcept
    {
        L.fill(0.0f);
        T.fill(0.0f);
    }

    /** Update from 64 log-spaced level cells (dB). dt in seconds. */
    void process(const float* levelsDb, float dtSeconds) noexcept
    {
        if (levelsDb == nullptr)
            return;

        const float dt = std::max(1.0e-4f, dtSeconds);
        const float atk = 1.0f - std::exp(-dt / 0.10f); // tau atk 0.10
        const float rel = 1.0f - std::exp(-dt / 0.90f); // tau rel 0.90

        for (int k = 0; k < kNumCells; ++k)
        {
            const float target = levelsDb[k];
            const float coeff = (target > L[(size_t) k]) ? atk : rel;
            L[(size_t) k] += (target - L[(size_t) k]) * coeff;
        }

        for (int k = 0; k < kNumCells; ++k)
        {
            const int neighbor = k - 2; // log-cell neighbor, not linear FFT bin
            float P = 0.0f;
            if (neighbor >= 0)
                P = std::max(0.0f, L[(size_t) k] - L[(size_t) neighbor] - 4.0f);
            T[(size_t) k] = std::min(1.0f, P / 10.0f);
        }
    }
};

} // namespace ember
