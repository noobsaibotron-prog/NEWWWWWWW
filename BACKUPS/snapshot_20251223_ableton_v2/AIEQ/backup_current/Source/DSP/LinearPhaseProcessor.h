#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
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
        The resulting IR is zero-phase, Hann-windowed, normalised, and loaded
        into each convolution instance.
        @param magnitudeResponse Magnitude in dB for bins [0 .. N/2)
        @param sampleRate        Current sample rate
    */
    void updateImpulseResponse(const std::vector<float>& magnitudeResponse, double sampleRate);

    /** Load a precomputed time-domain IR (mono) into all convolvers. */
    void loadImpulseResponse(const std::vector<float>& ir, double sampleRate);

private:
    void ensureConvolutionCount(size_t channels);

    std::vector<std::unique_ptr<juce::dsp::Convolution>> convolvers;
    juce::dsp::FFT fft { static_cast<int>(fftOrder) };
    double currentSampleRate = 44100.0;
    std::vector<float> freqDomainBuf; // size fftSize*2
    std::vector<float> timeDomainBuf; // size fftSize*2
    std::vector<float> irBuf;         // size fftSize
    juce::AudioBuffer<float> tempIRBuffer;
};

