#pragma once

/**
 * PerceptualFrontEnd — Roadmap v1, P2: the AI-owned analysis front-end.
 *
 * P2 Commit 1 status: DIAGNOSTICS-ONLY. This module is compiled into the AI
 * test target only and is NOT instantiated anywhere in production. No detector
 * consumes its output yet. Wiring into the AI thread (draining
 * preEqSpectrumFifo) and any detector migration are separate, gated commits —
 * see docs/AI_SCORECARD.md and the anti-accidental-migration guard in the plan.
 *
 * What it produces per hop (fftSize/2 = 2048 samples):
 *   - rawDb:  2049 UNSMOOTHED dB bins. Same FFT/window/normalization/clamp as
 *     SpectrumAnalyzer, but WITHOUT the attack/release ballistics that the
 *     legacy path bakes in before detection (those destroy per-band dynamics —
 *     the very thing P3 needs). First-frame equivalence with the legacy mirror
 *     is witnessed in AIFrontEndTest.
 *   - bandDb: log-spaced band energies (12 bands/octave from 20 Hz to Nyquist,
 *     triangular weights on the log-frequency axis, same construction pattern
 *     as MLEngine::extractMelBands). The perceptual representation the new
 *     detectors/statistics will consume.
 *   - frame timing accumulators (CPU witness: mean/max ns per frame).
 *
 * Real-time discipline: all buffers are pre-allocated in prepare(); the
 * process path performs no heap allocation. Designed to run on the AI worker
 * thread (NOT the audio thread).
 */

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <functional>

class PerceptualFrontEnd
{
public:
    static constexpr int   kFftOrder       = 12;
    static constexpr int   kFftSize        = 1 << kFftOrder;   // 4096
    static constexpr int   kNumBins        = kFftSize / 2 + 1; // 2049
    static constexpr int   kHopSize        = kFftSize / 2;     // 50% overlap
    static constexpr int   kBandsPerOctave = 12;
    static constexpr float kMinBandHz      = 20.0f;
    static constexpr float kMinDb          = -120.0f;
    static constexpr float kMaxDb          = 12.0f;

    static constexpr int   kLfFftOrder  = 13;
    static constexpr int   kLfFftSize   = 1 << kLfFftOrder;     // 8192
    static constexpr int   kLfNumBins   = kLfFftSize / 2 + 1;   // 4097
    static constexpr int   kLfHopSize   = kLfFftSize / 2;
    static constexpr float kLfCutoverHz = 200.0f; // bands below use the 8192 path

    struct Frame
    {
        std::vector<float> rawDb;       // kNumBins, unsmoothed dB (4096 path)
        std::vector<float> bandDb;      // numBands(), log-band energies (4096 path)
        // P2C3 — perceptual extensions (diagnostics-only, no detector consumes them):
        std::vector<float> bandDbFused; // bandDb, but LF bands (< kLfCutoverHz) come
                                        // from the latest 8192-point spectrum (better
                                        // LF resolution: ~5.86 Hz/bin vs 11.7). Equals
                                        // bandDb until the first LF frame is ready.
        std::vector<float> salienceDb;  // bandDbFused + equal-loudness weight (fixed
                                        // ISO-226-like curve, ~75 phon, anchor table
                                        // interpolated in log-f; approximation for
                                        // salience RANKING, not metrology)
        float fluxDb = 0.0f;            // mean positive per-bin dB delta vs previous
                                        // frame (spectral flux onset signal; ~0 on
                                        // steady material, spikes on transients)
        bool lfValid = false;           // true once >=1 LF (8192) frame contributed
    };

    PerceptualFrontEnd() = default;

    void prepare(double sampleRate);
    void reset();

    /** Streams mono samples in; invokes onFrame for every completed hop.
        The Frame reference is only valid during the callback (reused buffer). */
    void pushMono(const float* samples, int numSamples,
                  const std::function<void(const Frame&)>& onFrame);

    /** Convenience for tests: run a whole mono buffer, collect all frames. */
    std::vector<Frame> analyzeAll(const float* samples, int numSamples);

    int numBands() const noexcept { return static_cast<int>(bandCentersHz.size()); }
    float bandCenterHz(int band) const { return bandCentersHz[static_cast<size_t>(band)]; }

    /** Equal-loudness weight (dB) applied to band `band` in salienceDb (test witness). */
    float loudnessWeightDb(int band) const { return bandLoudnessWeightDb[static_cast<size_t>(band)]; }

    // CPU witness accessors (per processed frame, nanoseconds).
    juce::int64 framesProcessed() const noexcept { return frameCount; }
    double meanFrameNs() const noexcept
    {
        return frameCount > 0 ? static_cast<double>(totalNs) / static_cast<double>(frameCount) : 0.0;
    }
    juce::int64 maxFrameNs() const noexcept { return maxNs; }

private:
    void processOneFrame(const std::function<void(const Frame&)>& onFrame);
    void buildBandTables();
    void pushLfMono(const float* samples, int numSamples); // 8192 LF path (internal)
    void processOneLfFrame();
    static float interpLoudnessWeightDb(float hz);

    double sr = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    // Streaming/overlap state (pre-allocated in prepare).
    std::vector<float> pending;     // accumulation buffer (ring-free: compacted)
    int pendingCount = 0;
    bool primed = false;            // first frame consumed a full kFftSize
    std::vector<float> overlap;     // previous second half (kHopSize)
    std::vector<float> fftData;     // kFftSize*2 work buffer
    Frame work;                     // reused output frame

    // Log-band tables: per band, [binStart, binEnd] + weights (triangular in log-f).
    std::vector<float> bandCentersHz;
    std::vector<int>   bandBinStart, bandBinEnd;
    std::vector<std::vector<float>> bandWeights;
    std::vector<float> bandWeightSum;

    // P2C3 — equal-loudness weight per band (precomputed in buildBandTables).
    std::vector<float> bandLoudnessWeightDb;

    // P2C3 — LF (8192) path state: same framing pattern as the main path, half
    // the bin spacing, own cadence (one frame per kLfHopSize samples). Updates
    // lfBandDb for bands below kLfCutoverHz; consumed by processOneFrame fusion.
    std::unique_ptr<juce::dsp::FFT> lfFft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> lfWindow;
    std::vector<float> lfPending;
    int  lfPendingCount = 0;
    bool lfPrimed = false;
    std::vector<float> lfOverlap;
    std::vector<float> lfFftData;
    std::vector<float> lfRawDb;            // kLfNumBins scratch
    std::vector<float> lfBandDb;           // numBands(); only LF bands maintained
    bool lfHasFrame = false;
    int  numLfBands = 0;                   // bands with center < kLfCutoverHz
    std::vector<int>   lfBandBinStart, lfBandBinEnd;   // 8192-grid mapping (LF bands only)
    std::vector<std::vector<float>> lfBandWeights;
    std::vector<float> lfBandWeightSum;

    // P2C3 — spectral flux state.
    std::vector<float> prevRawDb;          // previous main frame, for flux
    bool hasPrevRaw = false;

    // CPU witness.
    juce::int64 frameCount = 0, totalNs = 0, maxNs = 0;
    bool isPrepared = false; // pushMono is a guarded no-op until prepare() runs
};
