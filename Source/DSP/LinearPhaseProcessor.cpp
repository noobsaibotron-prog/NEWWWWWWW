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

    // Pre-allocate IR slots
    for (auto& slot : irSlots)
    {
        slot.freqDomain.resize(fftSize * 2, 0.0f);
        slot.valid = false;
    }

    // Pre-allocate per-channel OLA state
    ensureChannels(spec.numChannels);

    // Pre-allocate FFT scratch buffers
    fftWorkBuf.resize(fftSize * 2, 0.0f);
    ifftWorkBuf.resize(fftSize * 2, 0.0f);
    fftWorkBuf2.resize(fftSize * 2, 0.0f);

    // Pre-allocate IR construction scratch
    irBuildFreq.resize(fftSize, { 0.0f, 0.0f });
    irBuildTime.resize(fftSize, { 0.0f, 0.0f });
    irBuildReal.resize(fftSize, 0.0f);

    crossfadeSamplesRemaining = 0;
}

void LinearPhaseProcessor::reset()
{
    for (auto& ch : channels)
        ch.reset();
    crossfadeSamplesRemaining = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// process  — sample-by-sample with internal OLA buffering
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();

    ensureChannels(numChannels);

    const int currentActive = activeSlot.load(std::memory_order_acquire);

    // If no valid IR loaded yet, pass through silence (zero output) to be safe
    if (!irSlots[currentActive].valid)
    {
        block.clear();
        return;
    }

    const float* irFreqA = irSlots[currentActive].freqDomain.data();

    // During crossfade, we also need the old slot
    const bool doCrossfade = (crossfadeSamplesRemaining > 0 && irSlots[crossfadeFromSlot].valid);
    const float* irFreqB = doCrossfade ? irSlots[crossfadeFromSlot].freqDomain.data() : nullptr;

    // Crossfade is tracked per-sample; grab a mutable copy for the loop
    int xfadeRemaining = crossfadeSamplesRemaining;

    for (size_t ch = 0; ch < numChannels; ++ch)
    {
        float* data = block.getChannelPointer(ch);
        auto&  state = channels[ch];
        int localXfade = xfadeRemaining; // reset per-channel (both see same crossfade window)

        for (size_t i = 0; i < numSamples; ++i)
        {
            // ALWAYS accumulate input — independent of output draining
            state.inputRing[state.inputCount++] = data[i];

            // When we have a full hop, run OLA frame(s) and blend if crossfading
            if (state.inputCount >= hopSize)
            {
                state.inputCount = 0;

                if (doCrossfade && irFreqB != nullptr && localXfade > 0)
                {
                    // Run OLA with both IRs — use separate overlap tails to avoid contamination
                    processOLAFrame(state, irFreqA, state.outputQueue.data(),  fftWorkBuf.data());
                    processOLAFrame(state, irFreqB, state.outputQueueB.data(), fftWorkBuf2.data(),
                                    state.overlapTailB.data());

                    // Blend: new IR ramps in, old IR ramps out over crossfadeLengthSamples
                    const float newWeight = 1.0f - static_cast<float>(localXfade) / static_cast<float>(crossfadeLengthSamples);
                    const float oldWeight = 1.0f - newWeight;
                    for (size_t n = 0; n < hopSize; ++n)
                        state.outputQueue[n] = newWeight * state.outputQueue[n]
                                             + oldWeight * state.outputQueueB[n];
                }
                else
                {
                    processOLAFrame(state, irFreqA, state.outputQueue.data(), fftWorkBuf.data());
                }

                state.outputReadPos  = 0;
                state.outputAvailable = hopSize;
            }

            // ALWAYS drain output — if available, emit the convolved sample; else zero (initial latency)
            if (state.outputAvailable > 0)
            {
                data[i] = state.outputQueue[state.outputReadPos];
                state.outputReadPos++;
                state.outputAvailable--;
            }
            else
            {
                data[i] = 0.0f;
            }

            // Advance crossfade counter sample-by-sample
            if (localXfade > 0)
                --localXfade;
        }
    }

    // Update shared crossfade counter (advance by numSamples, clamp to 0)
    if (crossfadeSamplesRemaining > 0)
    {
        crossfadeSamplesRemaining -= static_cast<int>(numSamples);
        if (crossfadeSamplesRemaining <= 0)
            crossfadeSamplesRemaining = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// processOLAFrame  — one hop of Overlap-Add convolution
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::processOLAFrame(ChannelState& ch, const float* irFreq,
                                            float* outBuf, float* workBuf,
                                            float* overlapTailOverride)
{
    // workBuf is fftSize * 2 floats (for JUCE's real-only FFT format)

    // 1. Zero the work buffer, then copy hopSize input samples into the first half
    std::memset(workBuf, 0, sizeof(float) * fftSize * 2);
    std::copy(ch.inputRing.begin(), ch.inputRing.begin() + hopSize, workBuf);

    // 2. Forward FFT (real-only → half-complex)
    fft.performRealOnlyForwardTransform(workBuf);

    // 3. Complex multiply with IR spectrum (in-place in workBuf)
    //    JUCE real-only FFT stores [re0, im0, re1, im1, ...] for fftSize/2 + 1 bins
    //    Total floats used = fftSize + 2, but buffer is fftSize * 2 so safe.
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

    // 4. Inverse FFT
    fft.performRealOnlyInverseTransform(workBuf);

    // 5. Overlap-add: first hopSize samples = new output + tail from previous frame
    float* tail = (overlapTailOverride != nullptr) ? overlapTailOverride : ch.overlapTail.data();
    for (size_t n = 0; n < hopSize; ++n)
        outBuf[n] = workBuf[n] + tail[n];

    // 6. Save new tail: the remaining (irSize - 1) samples
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

    // ── Build zero-phase IR using complex FFT (same algorithm as before) ──

    // Clear frequency domain (complex)
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

    // IFFT to time domain
    std::fill(irBuildTime.begin(), irBuildTime.end(), std::complex<float>{ 0.0f, 0.0f });
    fft.perform(irBuildFreq.data(), irBuildTime.data(), true);

    // Extract real part — JUCE's fft.perform(inverse=true) already applies 1/N scaling
    for (size_t n = 0; n < fftSize; ++n)
        irBuildReal[n] = irBuildTime[n].real();

    // Rotate so the zero-phase peak lands at irSize/2
    const size_t peakTargetIndex = irSize / 2;
    const size_t rotateOffset    = fftSize - peakTargetIndex;
    std::rotate(irBuildReal.begin(),
                irBuildReal.begin() + static_cast<long>(rotateOffset),
                irBuildReal.end());

    // Apply Hann window with gain compensation (first irSize samples only)
    constexpr float hannGainComp = 2.0f;
    for (size_t n = 0; n < irSize; ++n)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                  * static_cast<float>(n)
                                                  / static_cast<float>(irSize - 1)));
        irBuildReal[n] *= (w * hannGainComp);
    }

    // Store the IR's FFT into the build slot
    storeIRToSlot(irBuildReal.data(), irSize);
}

// ═══════════════════════════════════════════════════════════════════════════
// loadImpulseResponse  — load a precomputed time-domain IR
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::loadImpulseResponse(const std::vector<float>& ir, double sampleRate)
{
    currentSampleRate = sampleRate;
    const size_t irLen = std::min(ir.size(), irSize);

    // Pad to irSize if shorter
    std::fill(irBuildReal.begin(), irBuildReal.end(), 0.0f);
    std::copy(ir.begin(), ir.begin() + irLen, irBuildReal.begin());

    storeIRToSlot(irBuildReal.data(), irSize);
}

// ═══════════════════════════════════════════════════════════════════════════
// storeIRToSlot  — FFT the time-domain IR and swap into the active slot
// ═══════════════════════════════════════════════════════════════════════════

void LinearPhaseProcessor::storeIRToSlot(const float* irTimeDomain, size_t irLen)
{
    auto& slot = irSlots[buildSlot];

    // Zero-pad the IR to fftSize, then compute real-only forward FFT
    std::fill(slot.freqDomain.begin(), slot.freqDomain.end(), 0.0f);
    std::copy(irTimeDomain, irTimeDomain + std::min(irLen, irSize), slot.freqDomain.begin());

    fft.performRealOnlyForwardTransform(slot.freqDomain.data());
    slot.valid = true;

    // Activate the new slot
    const int oldActive = activeSlot.load(std::memory_order_relaxed);

    if (irSlots[oldActive].valid)
    {
        // Crossfade from old to new.
        // Copy each channel's current overlapTail into overlapTailB so the old-IR
        // OLA frames start from the correct state instead of silence.
        for (auto& ch : channels)
        {
            if (ch.overlapTailB.size() < ch.overlapTail.size())
                ch.overlapTailB.resize(ch.overlapTail.size(), 0.0f);
            std::copy(ch.overlapTail.begin(), ch.overlapTail.end(), ch.overlapTailB.begin());
        }
        crossfadeFromSlot = oldActive;
        crossfadeSamplesRemaining = crossfadeLengthSamples;
    }

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
        if (ch.overlapTail.size() < irSize)  // irSize - 1, but allocate irSize for safety
        {
            ch.overlapTail.resize(irSize, 0.0f);
        }
        if (ch.outputQueue.size() < hopSize)
        {
            ch.outputQueue.resize(hopSize, 0.0f);
            ch.outputReadPos = 0;
            ch.outputAvailable = 0;
        }
        if (ch.outputQueueB.size() < hopSize)
            ch.outputQueueB.resize(hopSize, 0.0f);
        if (ch.overlapTailB.size() < irSize)
            ch.overlapTailB.resize(irSize, 0.0f);
    }
}
