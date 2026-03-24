#include "LinearPhaseProcessor.h"
#include <algorithm>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

LinearPhaseProcessor::LinearPhaseProcessor() = default;

// ═══════════════════════════════════════════════════════════════════════════
// prepare / reset
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    if constexpr (usePartitioned)
    {
        partConvolver.prepare(spec.numChannels, spec.sampleRate);
    }

    // Always prepare OLA buffers — needed for IR construction (updateImpulseResponse, etc.)
    for (auto& slot : irSlots)
    {
        slot.freqDomain.resize(fftSize * 2, 0.0f);
        slot.valid = false;
    }

    ensureChannels(spec.numChannels);

    fftWorkBuf.resize(fftSize * 2, 0.0f);
    ifftWorkBuf.resize(fftSize * 2, 0.0f);


    irBuildFreq.resize(fftSize, { 0.0f, 0.0f });
    irBuildTime.resize(fftSize, { 0.0f, 0.0f });
    irBuildReal.resize(fftSize, 0.0f);

    
}

void LinearPhaseProcessor::reset()
{
    if constexpr (usePartitioned)
        partConvolver.reset();

    for (auto& ch : channels)
        ch.reset();
    
}

// ═══════════════════════════════════════════════════════════════════════════
// process  — delegates to partitioned or OLA path
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    if constexpr (usePartitioned)
    {
        auto block = context.getOutputBlock();
        partConvolver.process(block);
        return;
    }

    // ── Legacy OLA path (usePartitioned == false) ──────────────────────
    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();

    ensureChannels(numChannels);

    const int currentActive = activeSlot.load(std::memory_order_acquire);

    if (!irSlots[currentActive].valid)
    {
        block.clear();
        return;
    }

    // Cache activeSlot once per block — avoids ~48k acquire loads/sec per channel
    const int cachedSlot = activeSlot.load(std::memory_order_acquire);

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        float* data = block.getChannelPointer(ch);
        auto&  state = channels[ch];

        for (size_t i = 0; i < numSamples; ++i)
        {
            state.inputRing[state.inputCount++] = data[i];

            if (state.inputCount >= hopSize)
            {
                state.inputCount = 0;

                const float* currentIR = irSlots[cachedSlot].freqDomain.data();
                processOLAFrame(state, currentIR, state.outputQueue.data(), fftWorkBuf.data());

                state.outputReadPos  = 0;
                state.outputAvailable = hopSize;
            }

            if (state.outputAvailable > 0)
            {
                data[i] = state.outputQueue[state.outputReadPos++];
                state.outputAvailable--;
            }
            else
            {
                data[i] = 0.0f;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// processOLAFrame  — one hop of Overlap-Add convolution (legacy path)
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::processOLAFrame(ChannelState& ch, const float* irFreq,
                                            float* outBuf, float* workBuf,
                                            float* overlapTailOverride)
{
    std::memset(workBuf, 0, sizeof(float) * fftSize * 2);
    std::copy(ch.inputRing.begin(), ch.inputRing.begin() + hopSize, workBuf);

    fft.performRealOnlyForwardTransform(workBuf);

    for (size_t k = 0; k <= fftSize / 2; ++k)
    {
        const size_t idx = k * 2;
        const float aRe = workBuf[idx];
        const float aIm = workBuf[idx + 1];
        const float bRe = irFreq[idx];
        const float bIm = irFreq[idx + 1];

        workBuf[idx]     = aRe * bRe - aIm * bIm;
        workBuf[idx + 1] = aRe * bIm + aIm * bRe;
    }

    fft.performRealOnlyInverseTransform(workBuf);

    float* tail = (overlapTailOverride != nullptr) ? overlapTailOverride : ch.overlapTail.data();
    for (size_t n = 0; n < hopSize; ++n)
        outBuf[n] = workBuf[n] + tail[n];

    const size_t tailLen = irSize - 1;
    for (size_t n = 0; n < tailLen; ++n)
    {
        if (hopSize + n < fftSize)
            tail[n] = workBuf[hopSize + n];
        else
            tail[n] = 0.0f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// updateImpulseResponse  — build zero-phase IR from magnitude spectrum
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::updateImpulseResponse(const std::vector<float>& magnitudeResponse,
                                                   double sampleRate)
{
    currentSampleRate = sampleRate;

    const size_t halfSize = fftSize / 2;
    const size_t bins = std::min(halfSize, magnitudeResponse.size());

    std::fill(irBuildFreq.begin(), irBuildFreq.end(), std::complex<float>{ 0.0f, 0.0f });

    for (size_t k = 0; k < bins; ++k)
    {
        const float magLin = juce::Decibels::decibelsToGain(magnitudeResponse[k]);
        irBuildFreq[k] = { magLin, 0.0f };

        if (k > 0 && k < halfSize)
        {
            const size_t mirror = fftSize - k;
            irBuildFreq[mirror] = { magLin, 0.0f };
        }
    }

    std::fill(irBuildTime.begin(), irBuildTime.end(), std::complex<float>{ 0.0f, 0.0f });
    fft.perform(irBuildFreq.data(), irBuildTime.data(), true);

    for (size_t n = 0; n < fftSize; ++n)
        irBuildReal[n] = irBuildTime[n].real();

    const size_t peakTargetIndex = irSize / 2;
    const size_t rotateOffset    = fftSize - peakTargetIndex;
    std::rotate(irBuildReal.begin(),
                irBuildReal.begin() + static_cast<long>(rotateOffset),
                irBuildReal.end());

    constexpr float hannGainComp = 1.0f;
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                  * static_cast<float>(n)
                                                  / static_cast<float>(irSize - 1)));
        irBuildReal[n] *= (w * hannGainComp);
    }

    if constexpr (usePartitioned)
    {
        // Feed time-domain IR directly to partitioned convolver
        partConvolver.storeTimeDomainIR(irBuildReal.data(), irSize);
    }
    else
    {
        storeIRToSlot(irBuildReal.data(), irSize);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// loadImpulseResponse  — load a precomputed time-domain IR
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    currentSampleRate = sampleRate;
    const size_t irLen = std::min(ir.size(), irSize);

    std::fill(irBuildReal.begin(), irBuildReal.end(), 0.0f);
    std::copy(ir.begin(), ir.begin() + irLen, irBuildReal.begin());

    if constexpr (usePartitioned)
    {
        partConvolver.storeTimeDomainIR(irBuildReal.data(), irSize);
    }
    else
    {
        storeIRToSlot(irBuildReal.data(), irSize);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// storeFreqIRDirect  — accept pre-computed freq-domain IR
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::storeFreqIRDirect(const float* freqDomainData)
{
    if constexpr (usePartitioned)
    {
        // Partitioned convolver handles IFFT→partition→FFT internally
        partConvolver.storeFreqIR(freqDomainData);
    }
    else
    {
        auto& slot = irSlots[buildSlot];
        std::copy(freqDomainData, freqDomainData + fftSize * 2, slot.freqDomain.begin());
        slot.valid = true;

        activeSlot.store(buildSlot, std::memory_order_release);
        buildSlot = 1 - buildSlot;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// storeIRToSlot  — FFT the time-domain IR and swap into the active slot
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::storeIRToSlot(const float* irTimeDomain, size_t irLen)
{
    auto& slot = irSlots[buildSlot];

    std::fill(slot.freqDomain.begin(), slot.freqDomain.end(), 0.0f);
    std::copy(irTimeDomain, irTimeDomain + std::min(irLen, irSize), slot.freqDomain.begin());

    fft.performRealOnlyForwardTransform(slot.freqDomain.data());
    slot.valid = true;

    const int oldActive = activeSlot.load(std::memory_order_relaxed);

    // IR transition: OLA overlap tail naturally blends old→new IR over irSize samples.
    juce::ignoreUnused(oldActive);

    activeSlot.store(buildSlot, std::memory_order_release);
    buildSlot = 1 - buildSlot;
}

// ═══════════════════════════════════════════════════════════════════════════
// ensureChannels  — grow channel state if needed
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::ensureChannels(size_t numChannels)
{
    if (channels.size() >= numChannels)
        return;

    channels.resize(numChannels);

    for (auto& ch : channels)
    {
        if (ch.inputRing.size() < hopSize)
        {
            ch.inputRing.resize(hopSize, 0.0f);
            ch.inputCount = 0;
        }
        if (ch.overlapTail.size() < irSize)
        {
            ch.overlapTail.resize(irSize, 0.0f);
        }
        if (ch.outputQueue.size() < hopSize)
        {
            ch.outputQueue.resize(hopSize, 0.0f);
            ch.outputReadPos = 0;
            ch.outputAvailable = 0;
        }

    }
}
