#include "LinearPhaseProcessor.h"
#include <algorithm>

LinearPhaseProcessor::LinearPhaseProcessor() = default;

void LinearPhaseProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;
    ensureConvolutionCount(spec.numChannels);
    for (auto& c : convolvers)
        c->prepare(spec);
}

void LinearPhaseProcessor::reset()
{
    for (auto& c : convolvers)
        c->reset();
}

void LinearPhaseProcessor::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    ensureConvolutionCount(numChannels);

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> channelContext(channelBlock);
        convolvers[ch]->process(channelContext);
    }
}

void LinearPhaseProcessor::updateImpulseResponse(const std::vector<float>& magnitudeResponse, double sampleRate)
{
    currentSampleRate = sampleRate;

    // Frequency domain buffer (complex)
    std::vector<std::complex<float>> freqDomain(fftSize, { 0.0f, 0.0f });
    const size_t halfSize = fftSize / 2;
    const size_t bins = std::min(halfSize, magnitudeResponse.size());

    for (size_t k = 0; k < bins; ++k)
    {
        const float magLin = juce::Decibels::decibelsToGain(magnitudeResponse[k]);
        freqDomain[k] = { magLin, 0.0f }; // real part

        if (k > 0 && k < halfSize) // mirror to keep zero-phase (skip DC)
        {
            const size_t mirror = fftSize - k;
            freqDomain[mirror] = { magLin, 0.0f };
        }
    }

    // IFFT to time domain
    std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
    fft.perform(freqDomain.data(), timeDomain.data(), true);

    // timeDomain is complex; take real part
    std::vector<float> ir(fftSize, 0.0f);
    for (size_t n = 0; n < fftSize; ++n)
        ir[n] = timeDomain[n].real();

    // Center the zero-phase IR (circular shift by fftSize/2)
    std::rotate(ir.begin(), ir.begin() + static_cast<long>(halfSize), ir.end());

    // Trim to desired IR size
    ir.resize(irSize);

    // Apply Hann window
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n) / static_cast<float>(irSize - 1)));
        ir[n] *= w;
    }

    // Normalize to prevent gain build-up
    const float maxAbs = *std::max_element(ir.begin(), ir.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
    if (maxAbs > 0.0f)
    {
        const float inv = 1.0f / maxAbs;
        for (auto& s : ir)
            s *= inv;
    }

    // Load into convolvers (one per channel)
    juce::AudioBuffer<float> irBuffer(1, static_cast<int>(irSize));
    std::copy(ir.begin(), ir.end(), irBuffer.getWritePointer(0));

    for (auto& c : convolvers)
    {
        c->loadImpulseResponse(std::move(irBuffer), static_cast<double>(currentSampleRate),
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
        // After move, re-copy for next channel
        irBuffer.setSize(1, static_cast<int>(irSize), false, false, true);
        std::copy(ir.begin(), ir.end(), irBuffer.getWritePointer(0));
    }
}

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    currentSampleRate = sampleRate;

    // Ensure we have enough convolvers for current channels (will be set in prepare)
    ensureConvolutionCount(convolvers.size());

    const int irLen = static_cast<int>(std::min(ir.size(), irSize));

    juce::AudioBuffer<float> irBuffer(1, irLen);
    std::copy(ir.begin(), ir.begin() + irLen, irBuffer.getWritePointer(0));

    for (auto& c : convolvers)
    {
        juce::AudioBuffer<float> bufCopy(irBuffer);
        c->loadImpulseResponse(std::move(bufCopy),
                               currentSampleRate,
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
    }
}

void LinearPhaseProcessor::ensureConvolutionCount(size_t channels)
{
    if (convolvers.size() < channels)
    {
        const auto oldSize = convolvers.size();
        convolvers.resize(channels);
        for (size_t i = oldSize; i < channels; ++i)
            convolvers[i] = std::make_unique<juce::dsp::Convolution>();
    }
}

