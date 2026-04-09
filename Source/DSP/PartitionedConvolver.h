#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <atomic>
#include <cstring>

/**
 * Uniformly Partitioned Convolution engine.
 *
 * Splits a 4096-sample IR into 32 partitions of 128 samples each.
 * Each partition block uses a 256-point FFT (zero-padded).
 * Latency = partSize = 128 samples (≈ 2.9 ms @ 44.1 kHz).
 *
 * Uses a Frequency Delay Line (FDL) per channel for efficient overlap-save.
 * Double-buffered IR partitions for click-free updates from the audio thread.
 *
 * IR CROSSFADE STRATEGY — Frequency-Domain IR Blending:
 * Instead of running two parallel convolutions and crossfading their outputs
 * (which doubles CPU cost), we blend the old and new IR partitions in the
 * frequency domain BEFORE convolution.  Since convolution is linear:
 *   (oldIR * (1-t) + newIR * t) ⊛ x  =  (oldIR ⊛ x) * (1-t) + (newIR ⊛ x) * t
 * The result is mathematically equivalent to output crossfade, but requires
 * only a single convolution path.  The blend ratio updates once per partition
 * block (128 samples / 2.9 ms), which is inaudibly smooth.
 */
class PartitionedConvolver
{
public:
    // ── Constants ──────────────────────────────────────────────────────
    static constexpr size_t irSize       = 4096;
    static constexpr size_t partSize     = 128;
    static constexpr size_t fftPartSize  = partSize * 2;           // 256
    static constexpr size_t numParts     = irSize / partSize;      // 32
    static constexpr int    fftPartOrder = 8;                      // 2^8 = 256

    // For IFFT of the full-size IR received from PluginProcessor
    static constexpr size_t fftOrder     = 13;                     // 8192
    static constexpr size_t fftSize      = 1u << fftOrder;         // 8192

    PartitionedConvolver() = default;
    ~PartitionedConvolver() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────

    void prepare(size_t numChannels, double sampleRate)
    {
        (void)sampleRate;
        numCh = numChannels;

        // IR partition sets (double-buffered)
        for (auto& s : irSets)
        {
            s.partitions.resize(numParts);
            for (auto& p : s.partitions)
                p.assign(fftPartSize * 2, 0.0f);
            s.valid = false;
        }

        // Blended IR partitions (scratch for crossfade)
        blendedParts.resize(numParts);
        for (auto& p : blendedParts)
            p.assign(fftPartSize * 2, 0.0f);

        // Per-channel state
        chState.resize(numCh);
        for (auto& ch : chState)
            allocChannel(ch);

        // Scratch buffers
        fftScratch.assign(fftPartSize * 2, 0.0f);
        accumScratch.assign(fftPartSize * 2, 0.0f);
        fullIrTime.assign(fftSize * 2, 0.0f);

        // Latest-wins pending IR buffer (pre-allocated, audio-thread safe)
        pendingPartitions.assign(numParts * fftPartSize * 2, 0.0f);
        hasPendingIR.store(false, std::memory_order_relaxed);

        crossfadeSamplesLeft.store(0, std::memory_order_relaxed);
    }

    void reset()
    {
        for (auto& ch : chState)
            ch.resetState();
        crossfadeSamplesLeft.store(0, std::memory_order_relaxed);
        hasPendingIR.store(false, std::memory_order_relaxed);
    }

    // ── IR loading ─────────────────────────────────────────────────────

    /** Accept a freq-domain IR (fftSize*2 floats, JUCE interleaved).
     *  IFFT → split into 32 time-domain chunks → FFT each → store.
     *  Thread-safe via atomic slot swap. */
    void storeFreqIR(const float* freqDomainData)
    {
        // 1. IFFT the full-size IR back to time domain
        std::copy(freqDomainData, freqDomainData + fftSize * 2, fullIrTime.data());
        fftFull.performRealOnlyInverseTransform(fullIrTime.data());

        // 2. Partition into the build set
        auto& dest = irSets[buildSet];
        partitionTimeDomainIR(fullIrTime.data(), dest);
        dest.valid = true;

        // 3. Arm crossfade and swap
        armCrossfadeAndSwap();
    }

    /** Accept pre-partitioned frequency-domain data (numParts * fftPartSize*2 floats, packed).
     *  Each partition is fftPartSize*2 floats in JUCE interleaved complex format.
     *  Skips the IFFT + re-FFT — intended for use on the audio thread with data
     *  pre-computed by the builder thread. Only copies + swap + crossfade arm. */
    void storePrePartitionedIR(const float* packedPartitions)
    {
        const bool midFade = crossfadeSamplesLeft.load(std::memory_order_relaxed) > 0;

        if (midFade)
        {
            // ── Latest-wins: don't swap mid-crossfade ──
            // Store the new IR in the pending buffer. When the current
            // crossfade ends, process() will promote it with a fresh fade.
            std::copy(packedPartitions,
                      packedPartitions + numParts * fftPartSize * 2,
                      pendingPartitions.begin());
            hasPendingIR.store(true, std::memory_order_release);
            return;
        }

        // No crossfade active — write to build slot and start fade normally
        auto& dest = irSets[buildSet];
        for (size_t p = 0; p < numParts; ++p)
        {
            std::copy(packedPartitions + p * fftPartSize * 2,
                      packedPartitions + (p + 1) * fftPartSize * 2,
                      dest.partitions[p].begin());
        }
        dest.valid = true;

        armCrossfadeAndSwap();
        hasPendingIR.store(false, std::memory_order_relaxed);
    }

    /** Accept a time-domain IR (up to irSize samples) directly. */
    void storeTimeDomainIR(const float* irTimeDomain, size_t len)
    {
        // Pad to irSize
        std::vector<float> padded(irSize, 0.0f);
        std::copy(irTimeDomain, irTimeDomain + std::min(len, irSize), padded.begin());

        auto& dest = irSets[buildSet];
        partitionTimeDomainIR(padded.data(), dest);
        dest.valid = true;

        armCrossfadeAndSwap();
    }

    /** Pre-partition a time-domain IR into packed frequency-domain partitions.
     *  Output buffer must hold numParts * fftPartSize * 2 floats.
     *  Safe to call from any thread (uses its own FFT instance). */
    static void buildPackedPartitions(const float* timeDomainIR, size_t irLen, float* packedOut)
    {
        juce::dsp::FFT partFFT(fftPartOrder);
        std::vector<float> buf(fftPartSize * 2, 0.0f);

        for (size_t p = 0; p < numParts; ++p)
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            const size_t offset = p * partSize;
            const size_t count = (offset + partSize <= irLen) ? partSize
                               : (offset < irLen ? irLen - offset : 0);
            if (count > 0)
                std::copy(timeDomainIR + offset, timeDomainIR + offset + count, buf.begin());

            partFFT.performRealOnlyForwardTransform(buf.data());

            std::copy(buf.begin(), buf.end(), packedOut + p * fftPartSize * 2);
        }
    }

    /** @return true if a valid IR is loaded. */
    bool hasValidIR() const
    {
        return irSets[activeSet.load(std::memory_order_acquire)].valid;
    }

    // ── Audio processing ───────────────────────────────────────────────

    void process(juce::dsp::AudioBlock<float>& block)
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        // RT-SAFETY: never allocate in audio thread — silently limit to prepared channels
        if (numChannels > chState.size())
        {
            jassertfalse;
            block.clear();
            return;
        }

        const int currentActive = activeSet.load(std::memory_order_acquire);
        if (!irSets[currentActive].valid)
        {
            block.clear();
            return;
        }

        // Snapshot crossfade counter ONCE before the channel loop so that
        // every channel sees the identical fade curve for each sample position.
        const int cfLeftAtStart = crossfadeSamplesLeft.load(std::memory_order_acquire);
        const bool crossfading = cfLeftAtStart > 0 && irSets[crossfadeFromSet].valid;

        for (size_t c = 0; c < numChannels; ++c)
        {
            float* data = block.getChannelPointer(c);
            auto&  ch   = chState[c];

            // Track crossfade progress for IR blending (per-block granularity)
            int cfLeft = cfLeftAtStart;

            for (size_t i = 0; i < numSamples; ++i)
            {
                // Accumulate input
                ch.inputBuf[ch.inputCount++] = data[i];

                if (ch.inputCount >= partSize)
                {
                    ch.inputCount = 0;

                    if (crossfading && cfLeft > 0)
                    {
                        // ── IR-blending crossfade ──────────────────────────
                        // Blend old and new IR partitions in frequency domain,
                        // then convolve ONCE.  This is mathematically equivalent
                        // to running two convolutions and crossfading their
                        // outputs, but uses half the CPU.
                        //
                        // Blend ratio is computed at the block midpoint for
                        // best accuracy within the 128-sample block.
                        const int cfAtMid = juce::jmax(0, cfLeft - static_cast<int>(partSize / 2));
                        const float fadeNew = 1.0f - static_cast<float>(cfAtMid)
                                                     / static_cast<float>(crossfadeLength);
                        const float fadeOld = 1.0f - fadeNew;

                        const auto& newParts = irSets[currentActive];
                        const auto& oldParts = irSets[crossfadeFromSet];

                        // Blend all partitions (frequency-domain linear interpolation)
                        for (size_t p = 0; p < numParts; ++p)
                        {
                            const float* np = newParts.partitions[p].data();
                            const float* op = oldParts.partitions[p].data();
                            float*       bp = blendedParts[p].data();

                            for (size_t k = 0; k < fftPartSize * 2; ++k)
                                bp[k] = op[k] * fadeOld + np[k] * fadeNew;
                        }

                        // Single convolution with blended IR
                        processOneBlockBlended(ch, ch.outputQueue.data());
                    }
                    else
                    {
                        // No crossfade — process with active IR directly
                        const auto& irParts = irSets[currentActive];
                        processOneBlock(ch, irParts, ch.outputQueue.data());
                    }

                    // Advance crossfade counter by one partition block
                    if (cfLeft > 0)
                        cfLeft = juce::jmax(0, cfLeft - static_cast<int>(partSize));

                    ch.outputReadPos = 0;
                    ch.outputAvailable = partSize;
                }

                // Drain output
                if (ch.outputAvailable > 0)
                {
                    data[i] = ch.outputQueue[ch.outputReadPos++];
                    ch.outputAvailable--;
                }
                else
                {
                    data[i] = 0.0f;
                }
            }
        }

        // Commit the decremented counter once, after all channels are done.
        if (cfLeftAtStart > 0)
        {
            const int consumed = juce::jmin(cfLeftAtStart, static_cast<int>(numSamples));
            const int newLeft = cfLeftAtStart - consumed;
            crossfadeSamplesLeft.store(newLeft, std::memory_order_release);

            // ── Latest-wins promotion ──
            if (newLeft <= 0 && hasPendingIR.load(std::memory_order_acquire))
            {
                promotePendingIR();
            }
        }
    }

private:
    // ── IR partition storage (double-buffered) ─────────────────────────
    struct IRPartitionSet
    {
        std::vector<std::vector<float>> partitions;  // [numParts][fftPartSize*2]
        bool valid = false;
    };
    std::array<IRPartitionSet, 2> irSets;
    std::atomic<int> activeSet { 0 };
    int buildSet = 1;

    // Crossfade
    // Crossfade spans the full IR length (32 partition blocks = 4096 samples)
    // so that ALL FDL slots cycle through under the blended IR.
    static constexpr int crossfadeLength = static_cast<int>(irSize); // 4096 samples
    std::atomic<int> crossfadeSamplesLeft { 0 };
    int crossfadeFromSet = 0;

    // Blended IR partitions (scratch for crossfade — avoids 2× convolution)
    std::vector<std::vector<float>> blendedParts;  // [numParts][fftPartSize*2]

    // ── Per-channel state ──────────────────────────────────────────────
    struct ChannelState
    {
        std::vector<std::vector<float>> fdl;       // [numParts][fftPartSize*2]  FDL
        std::vector<float> inputBuf;               // [partSize]
        size_t inputCount = 0;
        std::vector<float> overlap;                // [partSize]
        std::vector<float> outputQueue;            // [partSize]
        size_t outputReadPos = 0;
        size_t outputAvailable = 0;

        void resetState()
        {
            for (auto& f : fdl)
                std::fill(f.begin(), f.end(), 0.0f);
            std::fill(inputBuf.begin(), inputBuf.end(), 0.0f);
            inputCount = 0;
            std::fill(overlap.begin(), overlap.end(), 0.0f);
            std::fill(outputQueue.begin(), outputQueue.end(), 0.0f);
            outputReadPos = 0;
            outputAvailable = 0;
        }
    };
    std::vector<ChannelState> chState;
    size_t numCh = 0;

    // ── Latest-wins pending IR ────────────────────────────────────────
    std::vector<float> pendingPartitions;     // [numParts * fftPartSize * 2]
    std::atomic<bool>  hasPendingIR { false };

    /** Arm crossfade from current active → buildSet, then swap active. */
    void armCrossfadeAndSwap()
    {
        const int oldActive = activeSet.load(std::memory_order_relaxed);
        if (irSets[oldActive].valid)
        {
            if (crossfadeSamplesLeft.load(std::memory_order_relaxed) <= 0)
            {
                crossfadeFromSet = oldActive;
                crossfadeSamplesLeft.store(crossfadeLength, std::memory_order_release);
            }
            // If mid-fade: latest-wins — the active IR changes immediately
            // and the blend target smoothly shifts to the newest IR.
            // No need to snapshot FDL (IR-blending doesn't need a parallel FDL).
        }

        activeSet.store(buildSet, std::memory_order_release);
        buildSet = 1 - buildSet;
    }

    void promotePendingIR()
    {
        auto& dest = irSets[buildSet];
        for (size_t p = 0; p < numParts; ++p)
        {
            std::copy(pendingPartitions.begin() + p * fftPartSize * 2,
                      pendingPartitions.begin() + (p + 1) * fftPartSize * 2,
                      dest.partitions[p].begin());
        }
        dest.valid = true;

        const int oldActive = activeSet.load(std::memory_order_relaxed);
        crossfadeFromSet = oldActive;
        crossfadeSamplesLeft.store(crossfadeLength, std::memory_order_release);
        activeSet.store(buildSet, std::memory_order_release);
        buildSet = 1 - buildSet;
        hasPendingIR.store(false, std::memory_order_relaxed);
    }

    // ── Scratch ────────────────────────────────────────────────────────
    std::vector<float> fftScratch;     // fftPartSize * 2
    std::vector<float> accumScratch;   // fftPartSize * 2
    std::vector<float> fullIrTime;     // fftSize * 2 (for IFFT of full IR)

    // ── FFT engines ────────────────────────────────────────────────────
    juce::dsp::FFT fftFull { static_cast<int>(fftOrder) };   // 8192 for IR IFFT
    juce::dsp::FFT fftP    { fftPartOrder };                  // 256 for partitions

    // ── Helpers ────────────────────────────────────────────────────────

    void allocChannel(ChannelState& ch)
    {
        ch.fdl.resize(numParts);
        for (auto& f : ch.fdl)
            f.assign(fftPartSize * 2, 0.0f);
        ch.inputBuf.assign(partSize, 0.0f);
        ch.inputCount = 0;
        ch.overlap.assign(partSize, 0.0f);
        ch.outputQueue.assign(partSize, 0.0f);
        ch.outputReadPos = 0;
        ch.outputAvailable = 0;
    }

    /** Partition irSize samples into numParts FFT'd chunks. */
    void partitionTimeDomainIR(const float* ir, IRPartitionSet& dest)
    {
        dest.partitions.resize(numParts);
        for (size_t p = 0; p < numParts; ++p)
        {
            auto& part = dest.partitions[p];
            std::fill(part.begin(), part.end(), 0.0f);

            // Copy partSize samples, zero-padded to fftPartSize
            std::copy(ir + p * partSize, ir + p * partSize + partSize, part.begin());

            // FFT in place
            fftP.performRealOnlyForwardTransform(part.data());
        }
    }

    /** Process one partSize block using the channel's own FDL + given IR partitions. */
    void processOneBlock(ChannelState& ch, const IRPartitionSet& irParts, float* outBuf)
    {
        // 1. Shift FDL: fdl[N-1] = fdl[N-2], ..., fdl[1] = fdl[0]
        for (size_t k = numParts - 1; k > 0; --k)
            ch.fdl[k].swap(ch.fdl[k - 1]);

        // 2. FFT the input block (zero-padded to fftPartSize)
        auto& slot0 = ch.fdl[0];
        std::fill(slot0.begin(), slot0.end(), 0.0f);
        std::copy(ch.inputBuf.begin(), ch.inputBuf.begin() + partSize, slot0.begin());
        fftP.performRealOnlyForwardTransform(slot0.data());

        // 3. Accumulate: sum of fdl[i] * irParts[i] (complex multiply)
        std::fill(accumScratch.begin(), accumScratch.end(), 0.0f);

        for (size_t p = 0; p < numParts; ++p)
        {
            const float* fdlP = ch.fdl[p].data();
            const float* irP  = irParts.partitions[p].data();

            for (size_t k = 0; k <= fftPartSize / 2; ++k)
            {
                const size_t idx = k * 2;
                const float aRe = fdlP[idx];
                const float aIm = fdlP[idx + 1];
                const float bRe = irP[idx];
                const float bIm = irP[idx + 1];

                accumScratch[idx]     += aRe * bRe - aIm * bIm;
                accumScratch[idx + 1] += aRe * bIm + aIm * bRe;
            }
        }

        // 4. IFFT
        fftP.performRealOnlyInverseTransform(accumScratch.data());

        // 5. Overlap-add
        for (size_t n = 0; n < partSize; ++n)
            outBuf[n] = accumScratch[n] + ch.overlap[n];

        // 6. Save tail for next block
        for (size_t n = 0; n < partSize; ++n)
            ch.overlap[n] = accumScratch[partSize + n];
    }

    /** Process one partSize block using blended IR partitions (crossfade path). */
    void processOneBlockBlended(ChannelState& ch, float* outBuf)
    {
        // 1. Shift FDL
        for (size_t k = numParts - 1; k > 0; --k)
            ch.fdl[k].swap(ch.fdl[k - 1]);

        // 2. FFT the input block
        auto& slot0 = ch.fdl[0];
        std::fill(slot0.begin(), slot0.end(), 0.0f);
        std::copy(ch.inputBuf.begin(), ch.inputBuf.begin() + partSize, slot0.begin());
        fftP.performRealOnlyForwardTransform(slot0.data());

        // 3. Accumulate with BLENDED IR partitions
        std::fill(accumScratch.begin(), accumScratch.end(), 0.0f);

        for (size_t p = 0; p < numParts; ++p)
        {
            const float* fdlP = ch.fdl[p].data();
            const float* irP  = blendedParts[p].data();  // ← blended, not single set

            for (size_t k = 0; k <= fftPartSize / 2; ++k)
            {
                const size_t idx = k * 2;
                const float aRe = fdlP[idx];
                const float aIm = fdlP[idx + 1];
                const float bRe = irP[idx];
                const float bIm = irP[idx + 1];

                accumScratch[idx]     += aRe * bRe - aIm * bIm;
                accumScratch[idx + 1] += aRe * bIm + aIm * bRe;
            }
        }

        // 4. IFFT
        fftP.performRealOnlyInverseTransform(accumScratch.data());

        // 5. Overlap-add
        for (size_t n = 0; n < partSize; ++n)
            outBuf[n] = accumScratch[n] + ch.overlap[n];

        // 6. Save tail
        for (size_t n = 0; n < partSize; ++n)
            ch.overlap[n] = accumScratch[partSize + n];
    }
};
