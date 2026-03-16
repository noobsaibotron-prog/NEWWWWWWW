#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <complex>

/**
 * Linear-phase convolver using zero-phase IR built from a magnitude response.
 * Uses one convolution instance per channel.
 */
class LinearPhaseProcessor : public juce::dsp::ProcessorBase
{
public:
    static constexpr size_t fftOrder = 13;            // 8192-point FFT
    static constexpr size_t fftSize = 1u << fftOrder; // 8192
    static constexpr size_t irSize = fftSize / 2;     // 4096 taps

    LinearPhaseProcessor();
    ~LinearPhaseProcessor() override = default;

    // juce::dsp::ProcessorBase
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(const juce::dsp::ProcessContextReplacing<float>& context) override;

    /** Update the impulse response from a magnitude spectrum (dB per bin).
        The resulting IR is zero-phase, Hann-windowed (with gain compensation),
        and loaded into each convolution instance (no peak normalisation to
        preserve the requested EQ gain).
        @param magnitudeResponse Magnitude in dB for bins [0 .. N/2)
        @param sampleRate        Current sample rate
    */
    void updateImpulseResponse(const std::vector<float>& magnitudeResponse, double sampleRate);

    /** Load a precomputed time-domain IR (mono) into all convolvers. */
    void loadImpulseResponse(const std::vector<float>& ir, double sampleRate);

private:
    void ensureConvolutionCount(size_t channels);
    void ensureScratchBuffer(size_t channels, size_t samples);

    // Two convolver sets to allow crossfade between IRs without clicks.
    std::array<std::vector<std::unique_ptr<juce::dsp::Convolution>>, 2> convolverSets;
    size_t activeSetIndex = 0;
    size_t pendingSetIndex = 0;
    bool   activeSetValid = false;
    bool   pendingSetValid = false;

    // Crossfade state (sample-accurate ramp).
    static constexpr int crossfadeLengthSamples = 64; // 1.45 ms @44.1k
    int crossfadeSamplesRemaining = 0;

    // Scratch buffer for dual-processing during crossfade.
    juce::AudioBuffer<float> scratchBuffer;
    size_t maxPreparedChannels = 0;
    size_t maxPreparedSamples  = 0;

    juce::dsp::FFT fft { static_cast<int>(fftOrder) };
    double currentSampleRate = 44100.0;
    // CRITICAL FIX: Pre-allocated buffers to avoid dynamic allocation in audio path
    // These are resized in prepare() and reused in updateImpulseResponse()
    std::vector<std::complex<float>> freqDomainBuf;  // size fftSize (complex)
    std::vector<std::complex<float>> timeDomainBuf;  // size fftSize (complex)
    std::vector<float> irBuf;                        // size fftSize (real)
};

