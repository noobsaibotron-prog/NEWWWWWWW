#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <complex>
#include <atomic>

/**
 * Linear-phase convolver using zero-phase IR built from a magnitude response.
 *
 * Uses a **synchronous** Overlap-Add (OLA) frequency-domain convolution engine
 * instead of juce::dsp::Convolution.  This eliminates all async/timing issues:
 * when updateImpulseResponse() returns, the new IR is immediately available
 * for the next process() call — no background threads, no silent gaps.
 *
 * Architecture:
 *   - fftOrder = 13  →  fftSize = 8192
 *   - irSize   = 4096  (fftSize / 2)
 *   - hopSize  = 4096  (fftSize - irSize)
 *   - Two IR frequency-domain slots (A/B) for click-free crossfade on IR changes.
 *   - Per-channel OLA state (input ring, overlap tail).
 *
 * When usePartitioned == true, delegates to PartitionedConvolver for 128-sample
 * latency instead of the 4096-sample OLA path.
 */

#include "PartitionedConvolver.h"

class LinearPhaseProcessor : public juce::dsp::ProcessorBase
{
public:
    static constexpr size_t fftOrder = 13;            // 8192-point FFT
    static constexpr size_t fftSize  = 1u << fftOrder; // 8192
    static constexpr size_t irSize   = fftSize / 2;    // 4096 taps
    static constexpr size_t hopSize  = fftSize - irSize; // 4096 (= irSize in this config)

    // ── Partitioned convolution toggle ─────────────────────────────────
    static constexpr bool   usePartitioned = true;
    static constexpr size_t partSize       = PartitionedConvolver::partSize; // 128

    LinearPhaseProcessor();
    ~LinearPhaseProcessor() override = default;

    // juce::dsp::ProcessorBase
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::ProcessContextReplacing<float>& context) override;

    /** Update the impulse response from a magnitude spectrum (dB per bin).
        The resulting IR is zero-phase, Hann-windowed (with gain compensation),
        and its FFT is stored for immediate use by process().
        @param magnitudeResponse  Magnitude in dB for bins [0 .. N/2)
        @param sampleRate         Current sample rate
    */
    void updateImpulseResponse(const std::vector<float>& magnitudeResponse, double sampleRate);

    /** Load a precomputed time-domain IR (mono) into the convolver. */
    void loadImpulseResponse(const std::vector<float>& ir, double sampleRate);

    /** Store a pre-computed frequency-domain IR directly (audio-thread safe).
        Accepts fftSize*2 floats in JUCE real-only FFT interleaved format.
        Triggers the internal A/B crossfade. No FFT is performed. */
    void storeFreqIRDirect(const float* freqDomainData);

private:
    // ── Partitioned convolver (used when usePartitioned == true) ───────
    PartitionedConvolver partConvolver;

    // ── IR frequency-domain storage (double-buffered) ──────────────────
    // Each slot holds the complex FFT of the zero-padded IR (fftSize floats,
    // stored in JUCE's interleaved real/imag format for performRealOnlyForwardTransform).
    struct IRSlot
    {
        std::vector<float> freqDomain;   // size = fftSize * 2 (complex interleaved)
        bool valid = false;
    };
    std::array<IRSlot, 2> irSlots;
    std::atomic<int> activeSlot { 0 };   // which slot process() reads from
    int buildSlot = 1;                    // which slot updateIR writes to

    // NOTE: Crossfade between IR slots is handled implicitly by the OLA overlap tail:
    // when the active slot changes, the overlap tail from the previous IR naturally blends
    // into the new IR output over irSize samples. No explicit crossfade needed.

    // ── Per-channel OLA state ──────────────────────────────────────────
    struct ChannelState
    {
        std::vector<float> inputRing;    // circular input buffer (hopSize)
        size_t inputCount = 0;           // samples accumulated so far

        std::vector<float> overlapTail;  // overlap-add tail from previous block (irSize - 1 samples)

        std::vector<float> outputQueue;    // buffered output waiting to be drained
        size_t outputReadPos = 0;
        size_t outputAvailable = 0;

        void reset()
        {
            std::fill(inputRing.begin(), inputRing.end(), 0.0f);
            inputCount = 0;
            std::fill(overlapTail.begin(), overlapTail.end(), 0.0f);
            outputReadPos = 0;
            outputAvailable = 0;
            std::fill(outputQueue.begin(), outputQueue.end(), 0.0f);
        }
    };
    std::vector<ChannelState> channels;

    // ── Scratch buffers (pre-allocated, reused each OLA frame) ─────────
    std::vector<float> fftWorkBuf;       // size = fftSize * 2 (for real-only FFT)
    std::vector<float> ifftWorkBuf;      // size = fftSize * 2

    // ── IR construction scratch (reused across updateImpulseResponse calls) ──
    std::vector<std::complex<float>> irBuildFreq;   // fftSize
    std::vector<std::complex<float>> irBuildTime;   // fftSize
    std::vector<float> irBuildReal;                  // fftSize

    juce::dsp::FFT fft { static_cast<int>(fftOrder) };
    double currentSampleRate = 44100.0;

    // ── Internal helpers ───────────────────────────────────────────────
    void ensureChannels(size_t numChannels);

    /** Run one OLA frame for a single channel using the given IR slot.
        Writes hopSize samples into outBuf.
        @param overlapTailOverride  If non-null, use this tail buffer instead of ch.overlapTail.
                                    The tail is updated in-place after the frame. */
    void processOLAFrame(ChannelState& ch, const float* irFreq, float* outBuf, float* workBuf,
                         float* overlapTailOverride = nullptr);

    /** Store a time-domain IR (irSize samples) into the build slot's frequency domain. */
    void storeIRToSlot(const float* irTimeDomain, size_t irLen);
};
