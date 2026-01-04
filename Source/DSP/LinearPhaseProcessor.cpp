#include "LinearPhaseProcessor.h"
#include <algorithm>

LinearPhaseProcessor::LinearPhaseProcessor() = default;

void LinearPhaseProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;
    ensureConvolutionCount(spec.numChannels);
    for (auto& c : convolvers)
        c->prepare(spec);
    
    // CRITICAL FIX: Pre-allocate buffers as class members to avoid dynamic allocation in audio path
    // These buffers are reused across updateImpulseResponse() calls, preventing allocations during IR rebuild
    freqDomainBuf.resize(fftSize, { 0.0f, 0.0f });
    timeDomainBuf.resize(fftSize, { 0.0f, 0.0f });
    irBuf.resize(fftSize, 0.0f);
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

    // CRITICAL FIX: Use pre-allocated class member buffers instead of local allocations
    // This eliminates dynamic allocations in updateImpulseResponse() which can be called from background thread
    const size_t halfSize = fftSize / 2;
    const size_t bins = std::min(halfSize, magnitudeResponse.size());

    // Clear and reuse pre-allocated buffer
    std::fill(freqDomainBuf.begin(), freqDomainBuf.end(), std::complex<float>{ 0.0f, 0.0f });

    for (size_t k = 0; k < bins; ++k)
    {
        const float magLin = juce::Decibels::decibelsToGain(magnitudeResponse[k]);
        freqDomainBuf[k] = { magLin, 0.0f }; // real part

        if (k > 0 && k < halfSize) // mirror to keep zero-phase (skip DC)
        {
            const size_t mirror = fftSize - k;
            freqDomainBuf[mirror] = { magLin, 0.0f };
        }
    }

    // IFFT to time domain using pre-allocated buffer
    std::fill(timeDomainBuf.begin(), timeDomainBuf.end(), std::complex<float>{ 0.0f, 0.0f });
    fft.perform(freqDomainBuf.data(), timeDomainBuf.data(), true);

    // timeDomain is complex; take real part into pre-allocated irBuf with IFFT scaling (1/N)
    // CRITICAL: Do NOT peak-normalise; preserve EQ gain from magnitudeResponse
    const float ifftScale = 1.0f / static_cast<float>(fftSize);
    for (size_t n = 0; n < fftSize; ++n)
        irBuf[n] = timeDomainBuf[n].real() * ifftScale;

    // Center the zero-phase IR (circular shift by fftSize/2)
    // CRITICAL FIX: Work directly on irBuf (no resize needed, we only use first irSize elements)
    std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(halfSize), irBuf.end());

    // Apply Hann window (only to first irSize elements)
    // Apply Hann window (only to first irSize elements) with gain compensation (~2.0 for Hann average 0.5)
    constexpr float hannGainComp = 2.0f;
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n) / static_cast<float>(irSize - 1)));
        irBuf[n] *= (w * hannGainComp);
    }

    // Load into convolvers (one per channel)
    // FIX 1: Create NEW buffer for each channel - never reuse after move
    for (auto& c : convolvers)
    {
        juce::AudioBuffer<float> channelIR(1, static_cast<int>(irSize));
        std::copy(irBuf.begin(), irBuf.begin() + irSize, channelIR.getWritePointer(0));
        
        c->loadImpulseResponse(std::move(channelIR), static_cast<double>(currentSampleRate),
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
    }
}

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    currentSampleRate = sampleRate;

    // Ensure we have enough convolvers for current channels (will be set in prepare)
    ensureConvolutionCount(convolvers.size());

    const int irLen = static_cast<int>(std::min(ir.size(), irSize));

    // FIX 1: Create NEW buffer for each channel - never reuse after move
    for (auto& c : convolvers)
    {
        juce::AudioBuffer<float> channelIR(1, irLen);
        std::copy(ir.begin(), ir.begin() + irLen, channelIR.getWritePointer(0));
        
        c->loadImpulseResponse(std::move(channelIR),
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

