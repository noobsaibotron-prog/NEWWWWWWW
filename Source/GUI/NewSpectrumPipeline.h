#pragma once
/**
 * NewSpectrumPipeline — Coordinator for the 5-layer metrological spectrum analyzer.
 *
 * Owns Strati 1-3 (SpectrumAnalyzerCore, SpectrumDisplayMapper, LogScaleMapper).
 * Reads from Strato 0 (LockFreeAudioFIFO, owned by PluginProcessor).
 *
 * Lifecycle:
 *   - Created by PluginEditor (or AdvancedSpectrumDisplay)
 *   - process() called once per GUI timer tick (~45-60 Hz)
 *   - Destroyed when editor closes -> zero CPU for spectrum
 *
 * Thread safety:
 *   - process() called from GUI timer thread
 *   - setFFTOrder() / setSpeed() called from message thread
 *   - pipelineLock (SpinLock) serializes all mutations
 *   - FIFO read is inherently SPSC-safe
 */

#include "../Core/LockFreeAudioFIFO.h"
#include "../DSP/SpectrumAnalyzerCore.h"
#include "../DSP/SpectrumDisplayMapper.h"
#include "LogScaleMapper.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

class NewSpectrumPipeline
{
public:
    /** Construct with references to the processor's FIFOs.
     *  fftOrder: 10=1024, 11=2048, 12=4096, 13=8192 */
    NewSpectrumPipeline (LockFreeAudioFIFO<float>& preEqFifo,
                         LockFreeAudioFIFO<float>& postEqFifo,
                         size_t fftOrder,
                         double sampleRate,
                         double uiFrameRate = 60.0)
        : preEqFIFO (preEqFifo),
          postEqFIFO (postEqFifo),
          fftSize (1u << fftOrder),
          hopSize (fftSize / 2),
          sr (sampleRate),
          preCore  (std::make_unique<SpectrumAnalyzerCore> (fftOrder, sampleRate)),
          postCore (std::make_unique<SpectrumAnalyzerCore> (fftOrder, sampleRate)),
          preMapper  (std::make_unique<SpectrumDisplayMapper> (fftSize / 2 + 1, uiFrameRate, sampleRate)),
          postMapper (std::make_unique<SpectrumDisplayMapper> (fftSize / 2 + 1, uiFrameRate, sampleRate)),
          preLogMapper  (sampleRate, fftSize),
          postLogMapper (sampleRate, fftSize),
          inputBuffer (fftSize, 0.0f),
          overlapBuffer (fftSize, 0.0f),
          postInputBuffer (fftSize, 0.0f),
          postOverlapBuffer (fftSize, 0.0f),
          preFftFrame (fftSize, 0.0f),
          postFftFrame (fftSize, 0.0f)
    {
    }

    /** Call once per GUI timer tick. Drains ALL pending hops from the FIFO,
     *  running FFT + smoothing for each. Only the final log-mapping is kept for display.
     *  This prevents FIFO accumulation which causes visual stuttering. */
    bool process (size_t displayWidthPixels)
    {
        const juce::SpinLock::ScopedLockType lock (pipelineLock);
        bool anyNewData = false;

        // -- Pre-EQ path: drain all queued hops --
        static constexpr int kMaxHopsPerTick = 16;  // safety cap
        for (int i = 0; i < kMaxHopsPerTick; ++i)
        {
            if (! processOneFIFO (preEqFIFO, *preCore, *preMapper, preLogMapper,
                                  inputBuffer, overlapBuffer, preFftFrame, prePixelDB, displayWidthPixels))
                break;
            anyNewData = true;
        }

        // -- Post-EQ path: drain all queued hops --
        for (int i = 0; i < kMaxHopsPerTick; ++i)
        {
            if (! processOneFIFO (postEqFIFO, *postCore, *postMapper, postLogMapper,
                                  postInputBuffer, postOverlapBuffer, postFftFrame, postPixelDB, displayWidthPixels))
                break;
            anyNewData = true;
        }

        if (anyNewData)
            ++version;

        return anyNewData;
    }

    /** Per-pixel dB values for rendering (pre-EQ). */
    const std::vector<float>& getPrePixelDB() const noexcept { return prePixelDB; }

    /** Per-pixel dB values for rendering (post-EQ). */
    const std::vector<float>& getPostPixelDB() const noexcept { return postPixelDB; }

    /** Version counter for cache invalidation. */
    uint64_t getVersion() const noexcept { return version; }

    /** Update FFT resolution. Call from message thread.
     *  SpinLock guarantees this never executes concurrently with process(). */
    void setFFTOrder (size_t newOrder, double sampleRate)
    {
        const juce::SpinLock::ScopedLockType lock (pipelineLock);

        fftSize = 1u << newOrder;
        hopSize = fftSize / 2;
        sr = sampleRate;

        // FIX: use unique_ptr reset instead of placement new (no UB)
        preCore  = std::make_unique<SpectrumAnalyzerCore> (newOrder, sampleRate);
        postCore = std::make_unique<SpectrumAnalyzerCore> (newOrder, sampleRate);

        const size_t numBins = fftSize / 2 + 1;
        preMapper  = std::make_unique<SpectrumDisplayMapper> (numBins, 60.0, sampleRate);
        postMapper = std::make_unique<SpectrumDisplayMapper> (numBins, 60.0, sampleRate);

        preLogMapper.setParameters (sampleRate, fftSize);
        postLogMapper.setParameters (sampleRate, fftSize);

        inputBuffer.assign (fftSize, 0.0f);
        overlapBuffer.assign (fftSize, 0.0f);
        postInputBuffer.assign (fftSize, 0.0f);
        postOverlapBuffer.assign (fftSize, 0.0f);
        preFftFrame.assign (fftSize, 0.0f);
        postFftFrame.assign (fftSize, 0.0f);
    }

    /** Update display ballistics (speed setting). */
    void setSpeed (float attackMs, float releaseMs, double frameRate)
    {
        const juce::SpinLock::ScopedLockType lock (pipelineLock);
        preMapper->setTimeConstants (attackMs, releaseMs, frameRate);
        postMapper->setTimeConstants (attackMs, releaseMs, frameRate);
    }

private:
    LockFreeAudioFIFO<float>& preEqFIFO;
    LockFreeAudioFIFO<float>& postEqFIFO;

    size_t fftSize;
    size_t hopSize;
    double sr;

    std::unique_ptr<SpectrumAnalyzerCore>   preCore, postCore;    // FIX: unique_ptr (no placement new UB)
    std::unique_ptr<SpectrumDisplayMapper>  preMapper, postMapper;
    LogScaleMapper         preLogMapper, postLogMapper;

    // Per-analyzer working buffers (50% overlap)
    std::vector<float> inputBuffer, overlapBuffer;
    std::vector<float> postInputBuffer, postOverlapBuffer;

    // Pre-allocated FFT frame buffers (Fix 4: zero allocations at 60 Hz)
    std::vector<float> preFftFrame, postFftFrame;

    // Output: per-pixel dB values
    std::vector<float> prePixelDB, postPixelDB;

    uint64_t version = 0;

    // Serializes process() vs setFFTOrder()/setSpeed() (Fix 3)
    juce::SpinLock pipelineLock;

    /** Process one FIFO->Core->Mapper->LogMapper chain.
     *  fftFrame is a pre-allocated scratch buffer (no per-frame allocation). */
    bool processOneFIFO (LockFreeAudioFIFO<float>& fifo,
                         SpectrumAnalyzerCore& core,
                         SpectrumDisplayMapper& mapper,
                         LogScaleMapper& logMapper,
                         std::vector<float>& inBuf,
                         std::vector<float>& overlap,
                         std::vector<float>& fftFrame,
                         std::vector<float>& pixelOut,
                         size_t displayWidth)
    {
        // Try to read hopSize (50% overlap) samples from FIFO
        const size_t pulled = fifo.pullAudioBlock (inBuf.data(), hopSize);
        if (pulled < hopSize)
            return false;

        // Build FFT frame: [overlap | new data]
        // Reuse pre-allocated fftFrame (zero-fill then copy)
        std::fill (fftFrame.begin(), fftFrame.end(), 0.0f);
        std::copy (overlap.begin(), overlap.begin() + static_cast<std::ptrdiff_t>(hopSize),
                   fftFrame.begin());
        std::copy (inBuf.begin(), inBuf.begin() + static_cast<std::ptrdiff_t>(hopSize),
                   fftFrame.begin() + static_cast<std::ptrdiff_t>(hopSize));

        // Save current data as overlap for next frame
        std::copy (inBuf.begin(), inBuf.begin() + static_cast<std::ptrdiff_t>(hopSize),
                   overlap.begin());

        // Strato 1: FFT -> PSD
        const auto& psd = core.processBlock (fftFrame.data());

        // Strato 2: PSD -> smoothed dB
        mapper.pushRawSpectrum (psd);
        const auto& smoothedDB = mapper.generateSmoothedUIFrame();

        // Strato 3: bins -> pixels
        logMapper.updateScreenWidth (displayWidth);
        logMapper.mapToPixels (smoothedDB, pixelOut);

        return true;
    }
};
