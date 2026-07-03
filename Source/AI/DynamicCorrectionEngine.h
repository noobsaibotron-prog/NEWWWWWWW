#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

/**
 * DynamicCorrectionEngine — per-band DYNAMIC correction (AI-evolution D1).
 *
 * soothe-class behaviour on the AI's approved corrections: each correction
 * becomes a band-passed envelope detector + a peaking CUT whose depth follows
 * the band energy above a threshold. Static corrections attenuate always;
 * dynamic corrections attenuate ONLY while the problem is audible.
 *
 * ── Threading design (supersedes the abandoned AIEngine::processCorrections
 *    path and the REV-5 Timer machinery; audit findings designed OUT) ──────
 *  - The message thread PUBLISHES an immutable, FIXED-SIZE params snapshot
 *    (no vectors, no reallocation — the original race root cause) via a
 *    double buffer + release/acquire index swap (single writer).
 *  - The audio thread is a PURE READER of the snapshot: one acquire load per
 *    block (B2 lesson: one coherent frame per pass), then works exclusively
 *    on audio-thread-owned filter/envelope state. No CAS loops, no ABA, no
 *    shared mutable coefficients.
 *  - prepare()/reset() run under the host contract (never concurrent with
 *    processBlock) and touch only audio-owned state + defaults.
 *
 * ── DSP v1 (documented tradeoffs) ─────────────────────────────────────────
 *  - Detector: RBJ constant-skirt bandpass biquad at (freq, Q); per-sample
 *    rectified peak envelope with attack/release one-poles.
 *  - Gain computer: soft-knee downward above threshold, ratio-style depth,
 *    clamped to the correction's maxCutDb.
 *  - Application: RBJ peaking filter; COEFFICIENTS UPDATE ONCE PER BLOCK with
 *    a slew-limited gain step (kMaxGainStepDbPerBlock) — block-rate zipper is
 *    bounded and inaudible at the shipped depths; a per-sample SVF is the
 *    documented v2 upgrade if depths beyond -12 dB are ever needed.
 *  - Output gain staging is NOT applied (pure cut engine).
 *
 * Default state: DISABLED and empty — zero effect until both a snapshot is
 * published and setEnabled(true) is called (B1-style opt-in gating).
 */
class DynamicCorrectionEngine
{
public:
    static constexpr int kMaxCorrections = 8;
    static constexpr int kMaxChannels = 2;
    static constexpr float kMaxGainStepDbPerBlock = 1.5f;

    struct CorrectionParams
    {
        float frequencyHz = 1000.0f;
        float q = 4.0f;
        float maxCutDb = 6.0f;        // positive number of dB of maximum attenuation
        float thresholdDb = -40.0f;   // band envelope level where cutting starts
        float ratio = 3.0f;           // >1: dB of overshoot per dB of cut driving
        float attackMs = 5.0f;
        float releaseMs = 80.0f;
        bool dynamic = true;          // false = static cut at maxCutDb
        bool enabled = false;
    };

    struct Snapshot
    {
        std::array<CorrectionParams, kMaxCorrections> corrections {};
        int numActive = 0;
        uint32_t version = 0;         // monotonically increasing per publish
    };

    DynamicCorrectionEngine() = default;

    /** Message thread / host contract only (never concurrent with process). */
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    /** Message-thread publication of the correction set (single writer).
        Fixed-size copy into the inactive slot + release swap — never blocks
        the audio thread, never reallocates. */
    void publishCorrections(const Snapshot& snapshot);

    /** Any thread. OFF by default; when off, process() is a guaranteed no-op. */
    void setEnabled(bool shouldBeEnabled) noexcept
    {
        enabled.store(shouldBeEnabled, std::memory_order_relaxed);
    }
    bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }

    /** AUDIO THREAD. RT-safe: no locks, no allocation, bounded work. */
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    /** Observability (GUI meters / tests): current smoothed cut in dB
        (negative = cutting) for correction slot i. */
    float getCurrentCutDb(int index) const noexcept
    {
        if (index < 0 || index >= kMaxCorrections)
            return 0.0f;
        return currentGainDb[static_cast<size_t>(index)].load(std::memory_order_relaxed);
    }

    /** Observability: version of the snapshot the audio thread last consumed. */
    uint32_t getActiveSnapshotVersion() const noexcept
    {
        return activeVersion.load(std::memory_order_relaxed);
    }

private:
    struct BiquadCoeffs { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };
    struct BiquadState  { float z1 = 0.0f, z2 = 0.0f; };

    static BiquadCoeffs makeBandpass(float freq, float q, double sampleRate) noexcept;
    static BiquadCoeffs makePeak(float freq, float q, float gainDb, double sampleRate) noexcept;

    // ── published params (message thread writes, audio thread reads) ──
    std::array<Snapshot, 2> snapshots;
    std::atomic<int> activeSnapshotIndex { 0 };
    std::atomic<bool> enabled { false };

    // ── audio-thread-owned state ──
    double sr = 44100.0;
    int channels = 2;
    uint32_t lastSeenVersion = 0;

    struct SlotState
    {
        BiquadCoeffs detectorCoeffs;
        std::array<BiquadState, kMaxChannels> detectorState {};
        BiquadCoeffs applyCoeffs;
        std::array<BiquadState, kMaxChannels> applyState {};
        float envelope = 0.0f;        // linear peak envelope (max of channels)
        float gainDb = 0.0f;          // current smoothed cut (<= 0)
        float appliedGainDb = 1.0e9f; // gain the applyCoeffs were built for
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
    };
    std::array<SlotState, kMaxCorrections> slots {};

    // Observability mirrors (audio writes relaxed, GUI reads relaxed)
    std::array<std::atomic<float>, kMaxCorrections> currentGainDb {};
    std::atomic<uint32_t> activeVersion { 0 };

    void rebuildSlotFromParams(int slotIndex, const CorrectionParams& params) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicCorrectionEngine)
};
