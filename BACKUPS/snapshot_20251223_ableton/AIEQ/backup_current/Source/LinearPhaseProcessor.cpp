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

    // Prepare reusable buffers to avoid per-call allocations
    if (freqDomainBuf.size() < fftSize * 2)
        freqDomainBuf.resize(fftSize * 2, 0.0f);
    if (timeDomainBuf.size() < fftSize * 2)
        timeDomainBuf.resize(fftSize * 2, 0.0f);
    if (irBuf.size() < fftSize)
        irBuf.resize(fftSize, 0.0f);

    auto* freqDomain = freqDomainBuf.data();
    auto* timeDomain = timeDomainBuf.data();
    auto* ir = irBuf.data();

    // Clear buffers
    std::fill(freqDomainBuf.begin(), freqDomainBuf.end(), 0.0f);
    std::fill(timeDomainBuf.begin(), timeDomainBuf.end(), 0.0f);
    std::fill(irBuf.begin(), irBuf.end(), 0.0f);

    const size_t halfSize = fftSize / 2;
    const size_t bins = std::min(halfSize, magnitudeResponse.size());

    for (size_t k = 0; k < bins; ++k)
    {
        const float magLin = juce::Decibels::decibelsToGain(magnitudeResponse[k]);
        freqDomain[2 * k] = magLin; // real part
        freqDomain[2 * k + 1] = 0.0f;

        if (k > 0 && k < halfSize) // mirror to keep zero-phase (skip DC)
        {
            const size_t mirror = fftSize - k;
            freqDomain[2 * mirror] = magLin;
            freqDomain[2 * mirror + 1] = 0.0f;
        }
    }

    // IFFT to time domain
    fft.perform(freqDomain, timeDomain, true);

    // timeDomain is interleaved real/imag; take real part
    for (size_t n = 0; n < fftSize; ++n)
        ir[n] = timeDomain[2 * n];

    // Center the zero-phase IR (circular shift by fftSize/2)
    std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(halfSize), irBuf.end());

    // Trim to desired IR size
    // (irBuf size is fftSize; we will copy only irSize samples)

    // Apply Hann window
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n) / static_cast<float>(irSize - 1)));
        ir[n] *= w;
    }

    // Normalize to prevent gain build-up
    const float maxAbs = *std::max_element(irBuf.begin(), irBuf.begin() + irSize,
                                           [](float a, float b) { return std::abs(a) < std::abs(b); });
    if (maxAbs > 0.0f)
    {
        const float inv = 1.0f / maxAbs;
        for (size_t n = 0; n < irSize; ++n)
            ir[n] *= inv;
    }

    // Load into convolvers (one per channel)
    juce::AudioBuffer<float> irBuffer(1, static_cast<int>(irSize));
    std::copy(irBuf.begin(), irBuf.begin() + irSize, irBuffer.getWritePointer(0));

    for (auto& c : convolvers)
    {
        c->loadImpulseResponse(std::move(irBuffer), static_cast<double>(currentSampleRate),
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
        // After move, re-copy for next channel
        irBuffer.setSize(1, static_cast<int>(irSize), false, false, true);
        std::copy(irBuf.begin(), irBuf.begin() + irSize, irBuffer.getWritePointer(0));
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

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    if (ir.empty())
        return;

    ensureConvolutionCount(convolvers.size());

    if (tempIRBuffer.getNumSamples() != static_cast<int>(ir.size()))
        tempIRBuffer.setSize(1, static_cast<int>(ir.size()), false, false, true);
    std::copy(ir.begin(), ir.end(), tempIRBuffer.getWritePointer(0));

    for (auto& c : convolvers)
    {
        c->loadImpulseResponse(tempIRBuffer, sampleRate,
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
        // tempIRBuffer is reused; no move, no re-copy needed per channel
    }
}

