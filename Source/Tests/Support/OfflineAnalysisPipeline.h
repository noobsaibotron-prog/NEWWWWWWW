#pragma once

/**
 * OfflineAnalysisPipeline — TEST-ONLY mirror of the plugin's audio→AI spectrum path.
 * (Roadmap v1, P1. Lives in the test tree; never compiled into the plugin.)
 *
 * Purpose: corpus/fixture evaluation must predict in-plugin behavior at the
 * representation level. This class re-implements, deterministically and without
 * threads, EXACTLY what SpectrumAnalyzer does to audio before the AI sees it.
 *
 * REPLICATES (kept in lockstep with Source/DSP/SpectrumAnalyzer.cpp — any change
 * to that pipeline must update this mirror in the same commit):
 *   - mono-sum: mean of all channels (pushSamples mixSample)
 *   - Hann window via the SAME juce::dsp::WindowingFunction class/params
 *   - FFT 4096 via the SAME juce::dsp::FFT (performFrequencyOnlyForwardTransform)
 *   - 50% overlap: first frame consumes a full fftSize, then hop = fftSize/2
 *     with the previous second half reused
 *   - magnitude = bin / fftSize, floored at 1e-10
 *   - dB = 20*log10(mag), clamped to [minDecibels=-120, maxDecibels=+12]
 *   - per-bin attack/release smoothing (attack 2 ms / release 50 ms defaults)
 *     with coefficients computed at the hop update rate, starting from
 *     prevDB = minDecibels (same as SpectrumAnalyzer::reset)
 *
 * DOES NOT REPLICATE (out of scope by design):
 *   - thread scheduling, FIFO capacity/drop behavior, GUI cadence
 *     (maxFramesPerCall) — pure scheduling, not math
 *   - APVTS / plugin state
 *   - anything downstream of the spectrum (AI rate limiter, temporal
 *     persistence, detection itself)
 */

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace aieq_test
{

class OfflineAnalysisPipeline
{
public:
    static constexpr int   kFftOrder    = 12;       // SpectrumAnalyzer Resolution::High
    static constexpr int   kFftSize     = 1 << kFftOrder; // 4096
    static constexpr int   kNumBins     = kFftSize / 2 + 1; // 2049 (AI representation)
    static constexpr float kMinDecibels = -120.0f;  // SpectrumAnalyzer::minDecibels
    static constexpr float kMaxDecibels = 12.0f;    // SpectrumAnalyzer::maxDecibels
    static constexpr float kAttackMs    = 2.0f;     // SpectrumAnalyzer default
    static constexpr float kReleaseMs   = 50.0f;    // SpectrumAnalyzer default

    explicit OfflineAnalysisPipeline(double sampleRate)
        : sr(sampleRate),
          fft(kFftOrder),
          window(static_cast<size_t>(kFftSize),
                 juce::dsp::WindowingFunction<float>::hann)
    {
        // Same coefficient formula as SpectrumAnalyzer::updateSmoothingCoeffs —
        // evaluated at the CONSTRUCTION-TIME default sample rate (44100), because
        // SpectrumAnalyzer::prepare() does NOT recompute the coefficients: in the
        // real plugin at 48 kHz the smoothing still runs on 44100-derived coeffs
        // (production quirk, measured: using the session SR here put the mirror
        // ~1.6 dB off the real analyzer). Mirror replicates reality, not intent.
        // If that quirk is ever fixed in SpectrumAnalyzer, update this line in
        // the same commit.
        const float coeffSr = 44100.0f;
        const float hopSize = static_cast<float>(kFftSize) / 2.0f;
        const float rate    = std::max(10.0f, coeffSr / hopSize);
        attackCoeff  = 1.0f - std::exp(-1.0f / (kAttackMs  * 0.001f * rate));
        releaseCoeff = 1.0f - std::exp(-1.0f / (kReleaseMs * 0.001f * rate));
    }

    /** Runs the full mirror over a (possibly multichannel) buffer and returns
        one smoothed dB frame (kNumBins bins) per produced FFT hop, in order. */
    std::vector<std::vector<float>> analyze(const juce::AudioBuffer<float>& audio)
    {
        // ── mono-sum (pushSamples mixSample: mean over channels) ──
        const int n  = audio.getNumSamples();
        const int ch = audio.getNumChannels();
        std::vector<float> mono(static_cast<size_t>(std::max(0, n)), 0.0f);
        for (int c = 0; c < ch; ++c)
        {
            const float* src = audio.getReadPointer(c);
            for (int i = 0; i < n; ++i)
                mono[static_cast<size_t>(i)] += src[i];
        }
        if (ch > 0)
            for (auto& v : mono)
                v /= static_cast<float>(ch);

        // ── framing: first frame full kFftSize, then hop with overlap reuse ──
        std::vector<std::vector<float>> frames;
        const int hop = kFftSize / 2;
        if (n < kFftSize)
            return frames; // same as the real path: no frame until a full FFT fits

        std::vector<float> prevDB(static_cast<size_t>(kNumBins), kMinDecibels);
        std::vector<float> fftData(static_cast<size_t>(kFftSize) * 2, 0.0f);
        std::vector<float> overlap(static_cast<size_t>(hop), 0.0f);

        int pos = 0;
        bool first = true;
        while (true)
        {
            if (first)
            {
                std::copy(mono.begin(), mono.begin() + kFftSize, fftData.begin());
                pos = kFftSize;
                first = false;
            }
            else
            {
                if (pos + hop > n)
                    break;
                std::copy(overlap.begin(), overlap.end(), fftData.begin());
                std::copy(mono.begin() + pos, mono.begin() + pos + hop,
                          fftData.begin() + hop);
                pos += hop;
            }
            std::copy(fftData.begin() + hop, fftData.begin() + kFftSize, overlap.begin());

            std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);
            window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(kFftSize));
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            std::vector<float> frame(static_cast<size_t>(kNumBins));
            for (int i = 0; i < kNumBins; ++i)
            {
                float mag = fftData[static_cast<size_t>(i)] / static_cast<float>(kFftSize);
                mag = std::max(mag, 1.0e-10f);
                float magDB = 20.0f * std::log10(mag);
                magDB = juce::jlimit(kMinDecibels, kMaxDecibels, magDB);

                const bool rising = magDB > prevDB[static_cast<size_t>(i)];
                const float coeff = rising ? attackCoeff : releaseCoeff;
                const float smoothed = prevDB[static_cast<size_t>(i)]
                                     + coeff * (magDB - prevDB[static_cast<size_t>(i)]);
                frame[static_cast<size_t>(i)] = smoothed;
                prevDB[static_cast<size_t>(i)] = smoothed;
            }
            frames.push_back(std::move(frame));
        }
        return frames;
    }

private:
    double sr;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
};

} // namespace aieq_test
