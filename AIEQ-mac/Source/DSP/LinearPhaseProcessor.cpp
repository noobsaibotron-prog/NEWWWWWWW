#include "LinearPhaseProcessor.h"
#include <algorithm>

LinearPhaseProcessor::LinearPhaseProcessor() = default;

void LinearPhaseProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;
    ensureConvolutionCount(spec.numChannels);
    maxPreparedChannels = std::max(maxPreparedChannels, static_cast<size_t>(spec.numChannels));
    maxPreparedSamples  = std::max<size_t>(maxPreparedSamples, static_cast<size_t>(spec.maximumBlockSize));
    ensureScratchBuffer(maxPreparedChannels, maxPreparedSamples);

    for (auto& set : convolverSets)
    {
        for (auto& c : set)
            c->prepare(spec);
    }
    
    // CRITICAL FIX: Pre-allocate buffers as class members to avoid dynamic allocation in audio path
    // These buffers are reused across updateImpulseResponse() calls, preventing allocations during IR rebuild
    freqDomainBuf.resize(fftSize, { 0.0f, 0.0f });
    timeDomainBuf.resize(fftSize, { 0.0f, 0.0f });
    irBuf.resize(fftSize, 0.0f);
}

void LinearPhaseProcessor::reset()
{
    for (auto& set : convolverSets)
    {
        for (auto& c : set)
            c->reset();
    }
}

void LinearPhaseProcessor::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    ensureConvolutionCount(numChannels);

    auto& activeSet = convolverSets[activeSetIndex];
    auto& pendingSet = convolverSets[pendingSetIndex];

    // No crossfade: process once with active set.
    if (crossfadeSamplesRemaining <= 0 || !pendingSetValid)
    {
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto channelBlock = block.getSingleChannelBlock(ch);
            juce::dsp::ProcessContextReplacing<float> channelContext(channelBlock);
            activeSet[ch]->process(channelContext);
        }
        return;
    }

    // Crossfade: process active into output, pending into scratch, then blend.
    const auto numSamples = block.getNumSamples();
    ensureScratchBuffer(numChannels, numSamples);

    // Copy input to scratch for second pass
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        scratchBuffer.copyFrom(static_cast<int>(ch), 0,
                               block.getChannelPointer(ch),
                               static_cast<int>(numSamples));
    }

    // Process active set (in-place in the output block)
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> channelContext(channelBlock);
        activeSet[ch]->process(channelContext);
    }

    // Process pending set into scratch buffer
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        auto scratchBlock = juce::dsp::AudioBlock<float>(scratchBuffer).getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> scratchContext(scratchBlock);
        pendingSet[ch]->process(scratchContext);
    }

    const int fadeLen = crossfadeLengthSamples;
    const int startPos = fadeLen - crossfadeSamplesRemaining;

    // Blend per channel
    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        float* out = block.getChannelPointer(ch);
        const float* alt = scratchBuffer.getReadPointer(static_cast<int>(ch));

        for (size_t n = 0; n < numSamples; ++n)
        {
            const int pos = startPos + static_cast<int>(n);
            const float t = pos < fadeLen ? static_cast<float>(pos) / static_cast<float>(fadeLen) : 1.0f;
            const float fadeIn  = t;
            const float fadeOut = 1.0f - t;
            out[n] = out[n] * fadeOut + alt[n] * fadeIn;
        }
    }

    crossfadeSamplesRemaining = std::max(0, crossfadeSamplesRemaining - static_cast<int>(numSamples));
    if (crossfadeSamplesRemaining == 0)
    {
        activeSetIndex = pendingSetIndex;
        activeSetValid = true;
        pendingSetValid = false;
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

    // Center the zero-phase IR so that the peak lands at irSize/2 (within the portion we actually use).
    // If we rotate by halfSize (fftSize/2), the peak ends at index halfSize (4096) and is then discarded
    // when we load only the first irSize (0..4095). Instead, rotate so index 0 -> irSize/2 (2048).
    // std::rotate moves 'middle' to the beginning, so we need middle = fftSize - targetIndex.
    const size_t peakTargetIndex = irSize / 2;              // 2048
    const size_t rotateOffset   = fftSize - peakTargetIndex; // 8192 - 2048 = 6144
    std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(rotateOffset), irBuf.end());

    // Apply Hann window (only to first irSize elements)
    // Apply Hann window (only to first irSize elements) with gain compensation (~2.0 for Hann average 0.5)
    constexpr float hannGainComp = 2.0f;
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n) / static_cast<float>(irSize - 1)));
        irBuf[n] *= (w * hannGainComp);
    }

    // Load into the inactive set, then trigger crossfade.
    auto targetSet = 1 - activeSetIndex;
    const size_t channels = std::max({ maxPreparedChannels,
                                       convolverSets[activeSetIndex].size(),
                                       convolverSets[targetSet].size() });
    ensureConvolutionCount(channels);

    for (auto& c : convolverSets[targetSet])
    {
        juce::AudioBuffer<float> channelIR(1, static_cast<int>(irSize));
        std::copy(irBuf.begin(), irBuf.begin() + irSize, channelIR.getWritePointer(0));

        c->loadImpulseResponse(std::move(channelIR), static_cast<double>(currentSampleRate),
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
    }

    if (!activeSetValid)
    {
        activeSetIndex = targetSet;
        activeSetValid = true;
        pendingSetValid = false;
        crossfadeSamplesRemaining = 0;
    }
    else
    {
        pendingSetIndex = targetSet;
        pendingSetValid = true;
        crossfadeSamplesRemaining = crossfadeLengthSamples;
    }
}

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    currentSampleRate = sampleRate;

    const size_t channels = std::max({ maxPreparedChannels,
                                       convolverSets[activeSetIndex].size(),
                                       convolverSets[pendingSetIndex].size() });
    ensureConvolutionCount(channels);

    const int irLen = static_cast<int>(std::min(ir.size(), irSize));

    for (auto& c : convolverSets[activeSetIndex])
    {
        juce::AudioBuffer<float> channelIR(1, irLen);
        std::copy(ir.begin(), ir.begin() + irLen, channelIR.getWritePointer(0));

        c->loadImpulseResponse(std::move(channelIR),
                               currentSampleRate,
                               juce::dsp::Convolution::Stereo::no,
                               juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
    }

    // Mirror into both sets to avoid uninitialised pending set.
    pendingSetIndex = activeSetIndex;
    activeSetValid = true;
    pendingSetValid = false;
    crossfadeSamplesRemaining = 0;
}

void LinearPhaseProcessor::ensureConvolutionCount(size_t channels)
{
    for (auto& set : convolverSets)
    {
        if (set.size() < channels)
        {
            const auto oldSize = set.size();
            set.resize(channels);
            for (size_t i = oldSize; i < channels; ++i)
                set[i] = std::make_unique<juce::dsp::Convolution>();
        }
    }
}

void LinearPhaseProcessor::ensureScratchBuffer(size_t channels, size_t samples)
{
    if (scratchBuffer.getNumChannels() < static_cast<int>(channels) ||
        scratchBuffer.getNumSamples() < static_cast<int>(samples))
    {
        scratchBuffer.setSize(static_cast<int>(channels),
                              static_cast<int>(samples),
                              false,  // keep existing content
                              false,  // clear extra space
                              true);  // avoid realloc if possible
    }
}
