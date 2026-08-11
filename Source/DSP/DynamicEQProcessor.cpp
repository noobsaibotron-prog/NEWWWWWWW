#include "DynamicEQProcessor.h"
#include <algorithm>
#include <cmath>

namespace
{
// Control slice width and crossfade settings for live coefficient updates.
// The live DynEQ path rebuilds a biquad whenever the smoothed effective gain
// moves far enough. A zero-state swap is stable but creates a cold-start
// transient; blindly preserving the old state avoids the transient but drifts
// into a coefficient/state mismatch under rapid modulation and breaks the LP
// anti-pop regression. The compromise used here is:
//   1. keep the 16-sample control slice + 16-sample crossfade
//   2. keep a modest 0.25 dB epsilon so tiny retargets do not thrash
//   3. warm-start the new biquad from a short recent input history for this
//      band, so each swap starts near its steady-state for the current signal
//      without reusing stale internal state indefinitely
constexpr int kDynamicControlSliceSamples = 16;
constexpr int kDynamicCoeffCrossfadeSamples = 16;
// Gain change threshold to trigger a coefficient rebuild. Raised from 0.25 dB
// to 0.5 dB combined with the rate-limit below: under continuous compression
// the previous setup triggered ~270 rebuilds/sec (measured via simulation),
// each one a coefficient swap + warm-start cascade. The aggregate noise
// floor of those transients is the audible crackle. 0.5 dB is still below
// the perceptual JND for level (~1 dB) so the dynamic response is unchanged
// to the listener, but the rebuild rate drops drastically.
constexpr float kEffectiveGainEpsilonDb = 0.5f;
// Hard rate-limit on coefficient rebuilds — independent from epsilon.
// At 48 kHz, 64 samples = ~1.33 ms, which is the floor for "fast" attacks
// in practice and well below human time-resolution for amplitude steps.
// Without this rate-limit, a compressor with 1 ms attack still triggers
// at every 16-sample control slice = 3 kHz rebuild rate. With it, the worst
// case is ~750 Hz, and in normal program material 100-200 Hz.
constexpr int kMinSamplesBetweenRebuilds = 64;

[[nodiscard]] bool isGainBearingDynamicFilterType(int filterType) noexcept
{
    switch (filterType)
    {
        case 1: // LowShelf
        case 2: // Peak
        case 3: // HighShelf
        case 7: // VintageLowShelf
        case 8: // VintageHighShelf
            return true;
        default:
            return false;
    }
}

[[nodiscard]] float vintageShelfQ(float q) noexcept
{
    return juce::jlimit(0.3f, 1.0f, q * 0.6f);
}

[[nodiscard]] float processBiquadOrBypass(BiquadState& state,
                                          float input,
                                          const BiquadCoeffs& coeffs) noexcept
{
    if (!coeffs.valid)
        return input;
    return state.processSample(input, coeffs);
}

std::complex<double> evaluateComplexResponse(const BiquadCoeffs& coeffs,
                                             double freq,
                                             double sampleRate) noexcept
{
    if (!coeffs.valid)
        return { 1.0, 0.0 };

    constexpr std::complex<double> kJ(0.0, 1.0);
    const std::complex<double> zInv =
        std::exp(-6.28318530717958647692 * freq * kJ / sampleRate);
    const std::complex<double> zInv2 = zInv * zInv;

    const std::complex<double> numerator = static_cast<double>(coeffs.b0)
                                         + static_cast<double>(coeffs.b1) * zInv
                                         + static_cast<double>(coeffs.b2) * zInv2;
    const std::complex<double> denominator = 1.0
                                           + static_cast<double>(coeffs.a1) * zInv
                                           + static_cast<double>(coeffs.a2) * zInv2;
    return numerator / denominator;
}

[[nodiscard]] float computeEffectiveGainDb(float staticGainDb,
                                           float dynamicDeltaDb) noexcept
{
    return staticGainDb + dynamicDeltaDb;
}
}

//==============================================================================
DynamicEQProcessor::DynamicEQProcessor()
{
    // Set default frequencies spread across spectrum
    const float defaultFreqs[maxBands] = {
        31.0f,   50.0f,   80.0f,   120.0f,  170.0f,  250.0f,
        350.0f,  500.0f,  700.0f,  1000.0f, 1400.0f, 2000.0f,
        2800.0f, 4000.0f, 5600.0f, 8000.0f, 11000.0f,15000.0f,
        18000.0f,22000.0f,26000.0f,30000.0f,34000.0f,38000.0f
    };
    
    for (int i = 0; i < maxBands; ++i)
    {
        bandParams[i].frequency.store(defaultFreqs[i], std::memory_order_relaxed);
        bandParams[i].enabled.store(true, std::memory_order_relaxed);
        bandParams[i].version.store(0, std::memory_order_relaxed);
    }
}

//==============================================================================
void DynamicEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    // Pre-allocate dry buffer with generous headroom so process() never needs to resize.
    // 8x block size covers Reaper dynamic block sizes and any host that delivers
    // larger-than-expected blocks without hitting the RT-unsafe setSize path.
    dryBuffer.setSize(channels, samplesPerBlock * 8, false, false, true);
    dryBuffer.clear();

    // RB-4 FIX: always allocate lookahead buffer for the maximum possible delay (20ms)
    // so that runtime mode changes (setLookahead) never need heap allocation.
    static constexpr float kMaxLookaheadMs = 20.0f;
    const int maxLaSamples = static_cast<int>(
        std::ceil((kMaxLookaheadMs / 1000.0f) * sampleRate));
    lookaheadBuffer.setSize(channels, maxLaSamples + samplesPerBlock);

    resetRuntimeStateNoAllocation(sampleRate, samplesPerBlock, channels);
}

bool DynamicEQProcessor::canReconfigureWithoutAllocation(double sampleRate,
                                                         int samplesPerBlock,
                                                         int channels) const noexcept
{
    if (sampleRate <= 0.0 || samplesPerBlock <= 0 || channels <= 0)
        return false;

    static constexpr double kMaxLookaheadSeconds = 0.020;
    const int requiredLookahead =
        static_cast<int>(std::ceil(kMaxLookaheadSeconds * sampleRate)) + samplesPerBlock;

    return dryBuffer.getNumChannels() >= channels
        && dryBuffer.getNumSamples() >= samplesPerBlock
        && lookaheadBuffer.getNumChannels() >= channels
        && lookaheadBuffer.getNumSamples() >= requiredLookahead;
}

bool DynamicEQProcessor::reconfigureNoAllocation(double sampleRate,
                                                 int samplesPerBlock,
                                                 int channels) noexcept
{
    if (!canReconfigureWithoutAllocation(sampleRate, samplesPerBlock, channels))
    {
        jassertfalse;
        return false;
    }

    resetRuntimeStateNoAllocation(sampleRate, samplesPerBlock, channels);
    return true;
}

void DynamicEQProcessor::resetRuntimeStateNoAllocation(double sampleRate,
                                                       int samplesPerBlock,
                                                       int channels) noexcept
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    numChannels = channels;

    // Prepare all band filters and calculate coefficients
    constexpr double smoothingSeconds = 0.02; // 20 ms ramp for dynamic params
    for (int i = 0; i < maxBands; ++i)
    {
        auto& state = bandStates[i];
        
        for (auto& filter : state.eqFiltersL)
            filter.reset();
        for (auto& filter : state.eqFiltersR)
            filter.reset();
        state.scFilterL.reset();
        state.scFilterR.reset();
        state.envelopeL = -100.0f;
        state.envelopeR = -100.0f;
        state.currentGain = 0.0f;
        state.targetGain = 0.0f;
        state.inputHistoryWritePos = 0;
        state.inputHistoryCount = 0;
        state.inputHistoryL.fill(0.0f);
        state.inputHistoryR.fill(0.0f);
        state.samplesSinceLastRebuild = 0;
        
        state.prepared = true;
        state.lastVersion = 0;  // Force update
        
        // Calculate attack/release coefficients
        updateAttackReleaseCoeffs(i);
        
        // Update EQ coefficients
        updateBandCoefficients(i);
        
        // Prime smoothed dynamic parameters
        smoothedThresholds[i].reset(sampleRate, smoothingSeconds);
        smoothedRatios[i].reset(sampleRate, smoothingSeconds);
        smoothedRanges[i].reset(sampleRate, smoothingSeconds);
        smoothedKnees[i].reset(sampleRate, smoothingSeconds);
        
        smoothedThresholds[i].setCurrentAndTargetValue(bandParams[i].threshold.load(std::memory_order_relaxed));
        smoothedRatios[i].setCurrentAndTargetValue(bandParams[i].ratio.load(std::memory_order_relaxed));
        smoothedRanges[i].setCurrentAndTargetValue(bandParams[i].range.load(std::memory_order_relaxed));
        smoothedKnees[i].setCurrentAndTargetValue(bandParams[i].knee.load(std::memory_order_relaxed));
        
        // Sidechain smoothing
        smoothedSidechainFreq[i].reset(sampleRate, smoothingSeconds);
        smoothedSidechainQ[i].reset(sampleRate, smoothingSeconds);
        smoothedSidechainFreq[i].setCurrentAndTargetValue(bandParams[i].sidechainFreq.load(std::memory_order_relaxed));
        smoothedSidechainQ[i].setCurrentAndTargetValue(bandParams[i].sidechainQ.load(std::memory_order_relaxed));
        
        // Cache applied SC params
        state.scFreqApplied = smoothedSidechainFreq[i].getCurrentValue();
        state.scQApplied = smoothedSidechainQ[i].getCurrentValue();
        state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
        state.liveEffectiveGainDb.store(bandParams[i].gain.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
        state.appliedEffectiveGainDb = bandParams[i].gain.load(std::memory_order_relaxed);
        state.liveGateAmount.store(1.0f, std::memory_order_relaxed);
        state.meterInputLevel.store(-100.0f, std::memory_order_relaxed);
        state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
        state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
    }

    lastAppliedMakeupGain = 1.0f;

    // Full-clear intentionally preserves the existing lookahead ring semantics.
    if (lookaheadBuffer.getNumSamples() > 0)
        lookaheadBuffer.clear();

    const float activeLookaheadMs = lookaheadMs.load(std::memory_order_relaxed);
    const int activeLookaheadSamples = static_cast<int>(activeLookaheadMs * 0.001f * static_cast<float>(sampleRate));
    lookaheadSamples.store(activeLookaheadSamples, std::memory_order_relaxed);
    lookaheadWritePos = 0;
    
    isPrepared.store(true, std::memory_order_release);
}

void DynamicEQProcessor::reset()
{
    for (int i = 0; i < maxBands; ++i)
    {
        auto& state = bandStates[i];
        for (auto& filter : state.eqFiltersL)
            filter.reset();
        for (auto& filter : state.eqFiltersR)
            filter.reset();
        state.scFilterL.reset();
        state.scFilterR.reset();
        
        state.envelopeL = -100.0f;
        state.envelopeR = -100.0f;
        state.currentGain = 0.0f;
        state.targetGain = 0.0f;
        state.inputHistoryWritePos = 0;
        state.inputHistoryCount = 0;
        state.inputHistoryL.fill(0.0f);
        state.inputHistoryR.fill(0.0f);
        state.samplesSinceLastRebuild = 0;
        state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
        state.liveEffectiveGainDb.store(
            bandParams[i].gain.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        state.appliedEffectiveGainDb = bandParams[i].gain.load(std::memory_order_relaxed);
        state.liveGateAmount.store(1.0f, std::memory_order_relaxed);
        state.meterInputLevel.store(-100.0f, std::memory_order_relaxed);
        state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
        state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
    }
    
    if (lookaheadBuffer.getNumSamples() > 0)
        lookaheadBuffer.clear();
    lookaheadWritePos = 0;

    lastAppliedMakeupGain = 1.0f;
}

//==============================================================================
void DynamicEQProcessor::setLookahead(float ms)
{
    const float clamped = juce::jlimit(0.0f, 20.0f, ms);
    lookaheadMs.store(clamped, std::memory_order_relaxed);

    // RB-4 FIX: also update the derived sample count so processBlock sees the
    // new value immediately.  The buffer was pre-allocated for max 20 ms in
    // prepare(), so clear + writePos reset are just memset + int write (RT-safe).
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    if (sr > 0.0)
    {
        const int laSamples = static_cast<int>((clamped / 1000.0f) * sr);
        lookaheadSamples.store(laSamples, std::memory_order_relaxed);
        lookaheadBuffer.clear();
        lookaheadWritePos = 0;
    }
}

void DynamicEQProcessor::updateLookaheadBuffer(double sampleRate, int samplesPerBlock, int channels)
{
    const float laMsVal = lookaheadMs.load(std::memory_order_relaxed);
    const int laSamples = static_cast<int>((laMsVal / 1000.0f) * sampleRate);
    lookaheadSamples.store(laSamples, std::memory_order_relaxed);
    
    if (laSamples > 0)
    {
        lookaheadBuffer.setSize(channels, laSamples + samplesPerBlock, false, false, true);
        lookaheadBuffer.clear();
    }
    else
    {
        lookaheadBuffer.setSize(0, 0);
    }
    lookaheadWritePos = 0;
}

void DynamicEQProcessor::pushBandInputHistory(BandState& state,
                                              float inputL,
                                              float inputR) noexcept
{
    const int writePos = state.inputHistoryWritePos;
    state.inputHistoryL[static_cast<size_t>(writePos)] = inputL;
    state.inputHistoryR[static_cast<size_t>(writePos)] = inputR;

    state.inputHistoryWritePos = (writePos + 1) % dynamicWarmupHistorySamples;
    state.inputHistoryCount = juce::jmin(state.inputHistoryCount + 1,
                                         dynamicWarmupHistorySamples);
}

void DynamicEQProcessor::warmBandFiltersFromHistory(BandState& state,
                                                    const BiquadCoeffs& coeffs) noexcept
{
    for (auto& f : state.eqFiltersL)
        f.reset();
    for (auto& f : state.eqFiltersR)
        f.reset();

    if (!coeffs.valid || state.inputHistoryCount <= 0)
        return;

    const bool historyWrapped = state.inputHistoryCount == dynamicWarmupHistorySamples;
    const int start = historyWrapped ? state.inputHistoryWritePos : 0;
    const int count = state.inputHistoryCount;

    for (int i = 0; i < count; ++i)
    {
        const int idx = historyWrapped
            ? (start + i) % dynamicWarmupHistorySamples
            : i;
        (void) state.eqFiltersL[0].processSample(state.inputHistoryL[static_cast<size_t>(idx)], coeffs);
        (void) state.eqFiltersR[0].processSample(state.inputHistoryR[static_cast<size_t>(idx)], coeffs);
    }
}

void DynamicEQProcessor::beginCoeffCrossfade(int bandIndex,
                                             const BiquadCoeffs& newCoeffs,
                                             int fadeSamples) noexcept
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;

    auto& state = bandStates[bandIndex];
    auto& xfade = bandCrossfades[bandIndex];

    if (xfade.remaining > 0)
        return;

    const bool oldWasActive = state.eqCoeffs.valid;
    if (oldWasActive)
    {
        xfade.oldCoeffs = state.eqCoeffs;
        xfade.oldFiltersL = state.eqFiltersL;
        xfade.oldFiltersR = state.eqFiltersR;
        xfade.remaining = fadeSamples;
        xfade.total = fadeSamples;
    }

    state.eqCoeffs = newCoeffs;

    // Warm-start the new filter from the recent pre-EQ input seen by this
    // band. This avoids the cold-start transient of a zeroed state without
    // carrying forward stale old-coefficient state indefinitely.
    warmBandFiltersFromHistory(state, newCoeffs);
}

//==============================================================================
void DynamicEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    // Hardware denormal flushing
    juce::ScopedNoDenormals noDenormals;
    
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), 2);
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    
    // CRITICAL: Safety check - if not prepared, just pass through
    if (!isPrepared.load(std::memory_order_acquire) || numSamples == 0 || channels == 0 || sr <= 0.0)
        return;
    
    //==========================================================================
    // LOCK-FREE: Copy dry signal for mix (no allocations)
    //==========================================================================
    const float mix = globalMix.load(std::memory_order_relaxed);

    // Safety: if host delivers a block larger than the 8x headroom pre-allocated in prepare(),
    // skip dry/wet mix rather than allocating on the audio thread.
    // In practice this should never trigger with 8x headroom.

    const int safeSamples = juce::jmin(numSamples, dryBuffer.getNumSamples());
    
    if (mix < 0.999f)
    {
        jassert(dryBuffer.getNumChannels() >= channels);
        jassert(dryBuffer.getNumSamples() >= numSamples);
        
        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, safeSamples);
    }
    
    //======================================================================
    // Lookahead delay: delay audio path, detector uses undelayed samples
    //======================================================================
    const int laSamples = lookaheadSamples.load(std::memory_order_relaxed);
    const int delaySize = lookaheadBuffer.getNumSamples();
    const bool useLookahead = (laSamples > 0 && delaySize >= laSamples + numSamples && delaySize > 0);
    const int lookaheadWriteStart = lookaheadWritePos;
    int writePos = lookaheadWriteStart;
    
    if (useLookahead)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float* in = buffer.getWritePointer(ch);
            float* delay = lookaheadBuffer.getWritePointer(ch);
            
            // write incoming samples into delay buffer
            for (int n = 0; n < numSamples; ++n)
            {
                const int wp = (writePos + n) % delaySize;
                delay[wp] = in[n];
            }
            
            // read delayed samples back into buffer for processing
            for (int n = 0; n < numSamples; ++n)
            {
                int rp = writePos + n - laSamples;
                if (rp < 0) rp += delaySize;
                else if (rp >= delaySize) rp -= delaySize;
                in[n] = delay[rp];
            }
        }
        
        writePos = (writePos + numSamples) % delaySize;
        lookaheadWritePos = writePos;
    }
    
    //==========================================================================
    // LOCK-FREE PROCESSING
    //==========================================================================
    
    const float* detectDelayL = useLookahead ? lookaheadBuffer.getReadPointer(0) : nullptr;
    const float* detectDelayR = (useLookahead && channels > 1) ? lookaheadBuffer.getReadPointer(1) : detectDelayL;
    
    for (int bandIdx = 0; bandIdx < maxBands; ++bandIdx)
    {
        const auto& params = bandParams[bandIdx];
        auto& state = bandStates[bandIdx];
        
        // Read enabled flag atomically
        if (!params.enabled.load(std::memory_order_relaxed))
        {
            // FIX: zero the meter when band is disabled so it doesn't freeze at last value
            state.inputHistoryWritePos = 0;
            state.inputHistoryCount = 0;
            state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
            state.meterInputLevel.store(-100.0f, std::memory_order_relaxed);
            state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
            state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
            state.liveEffectiveGainDb.store(0.0f, std::memory_order_relaxed);
            state.liveGateAmount.store(1.0f, std::memory_order_relaxed);
            continue;
        }
        
        // Check if coefficients need update
        const uint64_t currentVersion = params.version.load(std::memory_order_acquire);
        if (currentVersion != state.lastVersion)
        {
            const float freqNow = params.frequency.load(std::memory_order_relaxed);
            const float gainNow = params.gain.load(std::memory_order_relaxed);
            const float qNow = params.q.load(std::memory_order_relaxed);
            const int filterTypeNow = params.filterType.load(std::memory_order_relaxed);
            const bool sidechainEnabledNow = params.sidechainEnabled.load(std::memory_order_relaxed);
            const bool staticShapeChanged =
                state.staticFilterTypeApplied != filterTypeNow
                || std::abs(state.staticFreqApplied - freqNow) > 1.0e-6f
                || std::abs(state.staticGainApplied - gainNow) > 1.0e-6f
                || std::abs(state.staticQApplied - qNow) > 1.0e-6f;

            const bool sidechainStateChanged =
                state.sidechainEnabledApplied != sidechainEnabledNow;

            const BiquadCoeffs previousCoeffs = state.eqCoeffs;
            updateDynamicTargets(bandIdx);
            if (staticShapeChanged)
                updateBandCoefficients(bandIdx);
            if (sidechainStateChanged)
                updateSidechainState(bandIdx);
            updateAttackReleaseCoeffs(bandIdx);
            state.lastVersion = currentVersion;

            const bool coeffsChanged = staticShapeChanged
                && (previousCoeffs.valid != state.eqCoeffs.valid
                    || std::abs(previousCoeffs.b0 - state.eqCoeffs.b0) > 1.0e-6f
                    || std::abs(previousCoeffs.b1 - state.eqCoeffs.b1) > 1.0e-6f
                    || std::abs(previousCoeffs.b2 - state.eqCoeffs.b2) > 1.0e-6f
                    || std::abs(previousCoeffs.a1 - state.eqCoeffs.a1) > 1.0e-6f
                    || std::abs(previousCoeffs.a2 - state.eqCoeffs.a2) > 1.0e-6f);

            if (coeffsChanged && previousCoeffs.valid)
            {
                const float qVal = params.q.load(std::memory_order_relaxed);
                const int dynModeNow = params.dynamicMode.load(std::memory_order_relaxed);
                const bool dynamicActiveBand =
                    (dynModeNow == DynamicMode_Compress || dynModeNow == DynamicMode_Expand)
                    && isGainBearingDynamicFilterType(filterTypeNow);

                // PATH-ARBITRATION FIX (residual drag crackle).
                // A long (128-256 sample) drag crossfade STARVES the dynamic
                // control-slice rebuild: that path is guarded by
                // `xfade.remaining <= 0` (see the gain-bearing branch below), so
                // while the long drag fade is in flight the control slice cannot
                // retune. state.currentGain keeps drifting (per-sample smoother)
                // while state.appliedEffectiveGainDb stays frozen at the drag
                // instant's effective gain. When the long fade finally ends, the
                // accumulated |target - applied| jumps past the epsilon and fires
                // a single oversized coefficient step — an audible click on every
                // drag frame. For a dynamic-owned band we therefore keep the drag
                // crossfade as short as the dynamic cadence (16 samples), so the
                // control slice resumes almost immediately and effective-gain
                // tracking stays tight. Static bands keep the original 128/256
                // fade (no competing modulation path to starve).
                const int fadeSamples = dynamicActiveBand
                    ? kDynamicCoeffCrossfadeSamples
                    : ((qVal > 10.0f) ? 256 : 128);
                auto& xfade = bandCrossfades[bandIdx];

                // Always re-arm the crossfade on a real coefficient change.
                // The previous guard (xfade.remaining <= 0) skipped this block
                // if a crossfade was already in flight, but updateBandCoefficients()
                // had already mutated state.eqCoeffs underneath. The "new"
                // branch of the in-flight crossfade was then running with
                // freshly-rebuilt coefficients against filter states warmed
                // for the previous "new" coefficient set — exact mismatch
                // condition that produces the residual drag crackle.
                //
                // Now we treat the current state.eqFiltersL/R (warmed against
                // previousCoeffs, possibly mid-fade) as the new "old", and
                // warm a fresh "new" set against the just-rebuilt eqCoeffs.
                // Any in-flight crossfade is replaced from this point forward.
                xfade.oldCoeffs = previousCoeffs;
                xfade.oldFiltersL = state.eqFiltersL;
                xfade.oldFiltersR = state.eqFiltersR;
                xfade.remaining = fadeSamples;
                xfade.total = fadeSamples;

                // Warm-start the new biquad from the recent input history
                // instead of starting from zero state. Same continuity
                // mechanism used by beginCoeffCrossfade() for dynamic gain
                // modulation. Without warm-start, UI drag on a dynamic-owned
                // band produces an audible click on every parameter version
                // bump (~60 Hz at typical UI rate).
                warmBandFiltersFromHistory(state, state.eqCoeffs);
            }

            // Update meter GR immediately (for GUI responsiveness) but do NOT
            // snap state.currentGain — the per-sample smoother handles the audio
            // transition. Snapping caused an audible pop.
            const int dynMode = params.dynamicMode.load(std::memory_order_relaxed);
            if (dynMode != DynamicMode_Off)
            {
                const float currentEnv = (state.envelopeL + state.envelopeR) * 0.5f;
                const float threshold = params.threshold.load(std::memory_order_relaxed);
                const float ratio     = params.ratio.load(std::memory_order_relaxed);
                const float knee      = params.knee.load(std::memory_order_relaxed);
                const float range     = params.range.load(std::memory_order_relaxed);
                const float freshGR   = calculateDynamicGain(currentEnv, dynMode, threshold, ratio, knee, range);
                state.meterGainReduction.store(freshGR, std::memory_order_relaxed);
                // NOTE: state.currentGain NOT snapped — per-sample smoother avoids pop
            }
        }
        
        // Sidechain smoothing at control rate: advance to target, update coeffs if changed
        smoothedSidechainFreq[bandIdx].skip(numSamples);
        smoothedSidechainQ[bandIdx].skip(numSamples);
        const float scFreqNow = smoothedSidechainFreq[bandIdx].getCurrentValue();
        const float scQNow = smoothedSidechainQ[bandIdx].getCurrentValue();
        if (params.sidechainEnabled.load(std::memory_order_relaxed) && state.scCoeffs.valid)
        {
            constexpr float scEps = 1e-3f;
            if (std::abs(scFreqNow - state.scFreqApplied) > scEps
                || std::abs(scQNow - state.scQApplied) > scEps)
            {
                const double sr2 = currentSampleRate.load(std::memory_order_relaxed);
                auto newCoeffs = BiquadCoeffs::makeBandPass(sr2, scFreqNow, scQNow);
                if (newCoeffs.valid)
                {
                    state.scCoeffs = newCoeffs;
                    state.scFreqApplied = scFreqNow;
                    state.scQApplied = scQNow;
                }
            }
        }

        // Read dynamic mode
        const int dynMode = params.dynamicMode.load(std::memory_order_relaxed);
        const int filterType = params.filterType.load(std::memory_order_relaxed);
        const bool gainBearingMode = (dynMode == DynamicMode_Compress || dynMode == DynamicMode_Expand)
            && isGainBearingDynamicFilterType(filterType);
        
        //----------------------------------------------------------------------
        // DYNAMIC PROCESSING
        //----------------------------------------------------------------------
        if (dynMode != DynamicMode_Off)
        {
            const int detMode = params.detection.load(std::memory_order_relaxed);
            const bool scEnabled = params.sidechainEnabled.load(std::memory_order_relaxed);
            const float attackCoeff = state.attackCoeff;
            const float releaseCoeff = state.releaseCoeff;
            const float staticGainDb = params.gain.load(std::memory_order_relaxed);
            const float bandFreq = params.frequency.load(std::memory_order_relaxed);
            const float bandQ = params.q.load(std::memory_order_relaxed);
            
            float* outLPtr = buffer.getWritePointer(0);
            float* outRPtr = channels > 1 ? buffer.getWritePointer(1) : nullptr;
            const float* detectBaseL = useLookahead && detectDelayL ? detectDelayL : outLPtr;
            const float* detectBaseR = (useLookahead && detectDelayR) ? detectDelayR : (channels > 1 ? outRPtr : detectBaseL);
            float lastGateAmount = 1.0f;
            
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float threshold = smoothedThresholds[bandIdx].getNextValue();
                const float ratio = smoothedRatios[bandIdx].getNextValue();
                const float range = smoothedRanges[bandIdx].getNextValue();
                const float knee = smoothedKnees[bandIdx].getNextValue();
                
                // Get input samples (audio path already delayed if lookahead active)
                float inL = outLPtr[sample];
                float inR = channels > 1 ? outRPtr[sample] : inL;
                
                // Detector uses undelayed signal when lookahead is active
                float detectL = useLookahead ? detectBaseL[(lookaheadWriteStart + sample) % delaySize] : detectBaseL[sample];
                float detectR = (channels > 1)
                                ? (useLookahead ? detectBaseR[(lookaheadWriteStart + sample) % delaySize] : detectBaseR[sample])
                                : detectL;
                
                // Sidechain filtering
                // When sidechain is enabled: filter the detect signal with the sidechain bandpass.
                // When disabled: use the raw detect signal directly for level detection.
                // NOTE: do NOT use eqFiltersL[1] here — that cascades a second EQ stage onto
                // the detector signal, which distorts the gain-reduction curve. The second filter
                // slot in the array is reserved for future 2nd-order (12dB) filter support.
                float scL = detectL, scR = detectR;
                if (scEnabled && state.scCoeffs.valid)
                {
                    scL = state.scFilterL.processSample(detectL, state.scCoeffs);
                    scR = state.scFilterR.processSample(detectR, state.scCoeffs);
                }
                
                // Calculate input level
                float inputLevel = 0.0f;
                if (detMode == DetectionMode_Peak)
                {
                    inputLevel = std::max(std::abs(scL), std::abs(scR));
                }
                else // RMS
                {
                    inputLevel = std::sqrt((scL * scL + scR * scR) * 0.5f);
                }
                
                // Convert to dB
                float inputDb = inputLevel > 1e-10f ? 
                    juce::Decibels::gainToDecibels(inputLevel) : -100.0f;
                
                // Smooth envelope
                float currentEnv = (state.envelopeL + state.envelopeR) * 0.5f;
                float coeff = inputDb > currentEnv ? attackCoeff : releaseCoeff;
                float smoothedEnv = coeff * currentEnv + (1.0f - coeff) * inputDb;
                
                state.envelopeL = smoothedEnv;
                state.envelopeR = smoothedEnv;
                
                // Calculate dynamic gain
                float dynamicGainDb = calculateDynamicGain(smoothedEnv, dynMode, threshold, ratio, knee, range);
                
                // Smooth gain changes
                float gainCoeff = dynamicGainDb < state.currentGain ? attackCoeff : releaseCoeff;
                state.currentGain = gainCoeff * state.currentGain + (1.0f - gainCoeff) * dynamicGainDb;
                
                // Update metering (atomic)
                // IMPORTANT: store dynamicGainDb (instantaneous, pre-smooth) rather than
                // state.currentGain (audio-smoothed). The GUI meter has its own visual
                // smoothing (DynamicEQPanel::timerCallback). Storing the audio-smoothed
                // value here caused the meter to lag 100ms behind parameter changes —
                // it was effectively double-smoothed (audio + GUI decay).
                state.meterInputLevel.store(smoothedEnv, std::memory_order_relaxed);
                state.meterGainReduction.store(dynamicGainDb, std::memory_order_relaxed);

                // Track samples since the last coefficient rebuild for this
                // band, used by the hard rate-limit below.
                if (gainBearingMode)
                    ++state.samplesSinceLastRebuild;

                if (gainBearingMode
                    && (sample % kDynamicControlSliceSamples) == 0
                    && state.samplesSinceLastRebuild >= kMinSamplesBetweenRebuilds)
                {
                    const float targetEffectiveGainDb =
                        computeEffectiveGainDb(staticGainDb, state.currentGain);
                    if (std::abs(targetEffectiveGainDb - state.appliedEffectiveGainDb)
                        > kEffectiveGainEpsilonDb)
                    {
                        auto& xfade = bandCrossfades[bandIdx];
                        if (xfade.remaining <= 0)
                        {
                            const BiquadCoeffs newCoeffs = makeEQCoefficients(
                                filterType, bandFreq, targetEffectiveGainDb, bandQ);
                            beginCoeffCrossfade(bandIdx, newCoeffs,
                                                kDynamicCoeffCrossfadeSamples);
                            state.appliedEffectiveGainDb = targetEffectiveGainDb;
                            state.samplesSinceLastRebuild = 0;
                        }
                    }
                }
                
                // Apply EQ (with crossfade if coefficients just changed)
                float outL, outR;
                {
                    auto& xfade = bandCrossfades[bandIdx];
                    if (xfade.remaining > 0)
                    {
                        const float newL = processBiquadOrBypass(state.eqFiltersL[0], inL, state.eqCoeffs);
                        const float newR = channels > 1
                            ? processBiquadOrBypass(state.eqFiltersR[0], inR, state.eqCoeffs)
                            : newL;
                        const float oldL = processBiquadOrBypass(xfade.oldFiltersL[0], inL, xfade.oldCoeffs);
                        const float oldR = channels > 1
                            ? processBiquadOrBypass(xfade.oldFiltersR[0], inR, xfade.oldCoeffs)
                            : oldL;
                        const float fade = 1.0f - static_cast<float>(xfade.remaining) / static_cast<float>(xfade.total);
                        outL = oldL + (newL - oldL) * fade;
                        outR = oldR + (newR - oldR) * fade;
                        --xfade.remaining;
                    }
                    else
                    {
                        outL = processBiquadOrBypass(state.eqFiltersL[0], inL, state.eqCoeffs);
                        outR = channels > 1
                            ? processBiquadOrBypass(state.eqFiltersR[0], inR, state.eqCoeffs)
                            : outL;
                    }
                }
                
                // Gate remains a post-filter amplitude control. Compress/expand
                // now modulate the live filter gain directly via effectiveGainDb.
                if (dynMode == DynamicMode_Gate)
                {
                    if (smoothedEnv < threshold)
                    {
                        float gateAmount = juce::jmap(smoothedEnv,
                            threshold - range, threshold, 0.0f, 1.0f);
                        gateAmount = juce::jlimit(0.0f, 1.0f, gateAmount);
                        outL *= gateAmount;
                        outR *= gateAmount;
                        lastGateAmount = gateAmount;
                    }
                    else
                    {
                        lastGateAmount = 1.0f;
                    }
                }
                
                outLPtr[sample] = outL;
                if (channels > 1)
                    outRPtr[sample] = outR;

                pushBandInputHistory(state, inL, inR);
                
                const float outputMagnitude = std::max(std::abs(outL), std::abs(outR));
                const float outputDb = std::isfinite(outputMagnitude) && outputMagnitude > 1.0e-10f
                    ? juce::Decibels::gainToDecibels(outputMagnitude, -100.0f)
                    : -100.0f;
                state.meterOutputLevel.store(outputDb, std::memory_order_relaxed);
            }

            state.liveCurrentGainDb.store(state.currentGain, std::memory_order_relaxed);
            state.liveEffectiveGainDb.store(
                gainBearingMode ? state.appliedEffectiveGainDb : staticGainDb,
                std::memory_order_relaxed);
            state.liveGateAmount.store(lastGateAmount, std::memory_order_relaxed);
        }
        else
        {
            //------------------------------------------------------------------
            // STATIC EQ PROCESSING
            //------------------------------------------------------------------
            float* outLPtr = buffer.getWritePointer(0);
            float* outRPtr = channels > 1 ? buffer.getWritePointer(1) : nullptr;
            
            auto& xfade = bandCrossfades[bandIdx];
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float inL = outLPtr[sample];
                const float inR = channels > 1 ? outRPtr[sample] : inL;

                if (xfade.remaining > 0)
                {
                    const float fade = 1.0f - static_cast<float>(xfade.remaining) / static_cast<float>(xfade.total);
                    float newL = processBiquadOrBypass(state.eqFiltersL[0], inL, state.eqCoeffs);
                    float oldL = processBiquadOrBypass(xfade.oldFiltersL[0], inL, xfade.oldCoeffs);
                    outLPtr[sample] = oldL + (newL - oldL) * fade;

                    if (channels > 1)
                    {
                        float newR = processBiquadOrBypass(state.eqFiltersR[0], inR, state.eqCoeffs);
                        float oldR = processBiquadOrBypass(xfade.oldFiltersR[0], inR, xfade.oldCoeffs);
                        outRPtr[sample] = oldR + (newR - oldR) * fade;
                    }
                    --xfade.remaining;
                }
                else
                {
                    float outL = processBiquadOrBypass(state.eqFiltersL[0], inL, state.eqCoeffs);
                    outLPtr[sample] = outL;

                    if (channels > 1)
                    {
                        float outR = processBiquadOrBypass(state.eqFiltersR[0], inR, state.eqCoeffs);
                        outRPtr[sample] = outR;
                    }
                }

                pushBandInputHistory(state, inL, inR);
            }

            state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
            state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
            state.liveEffectiveGainDb.store(params.gain.load(std::memory_order_relaxed),
                                            std::memory_order_relaxed);
            state.liveGateAmount.store(1.0f, std::memory_order_relaxed);
        }
    }
    
    //==========================================================================
    // Apply global mix
    //==========================================================================
    if (mix < 0.999f && dryBuffer.getNumSamples() > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            
            for (int s = 0; s < safeSamples; ++s)
                wet[s] = dry[s] * (1.0f - mix) + wet[s] * mix;
            
            if (numSamples > safeSamples)
                buffer.clear(ch, safeSamples, numSamples - safeSamples);
        }
    }
    
    //==========================================================================
    // Auto makeup gain
    //==========================================================================
    if (autoMakeupEnabled.load(std::memory_order_relaxed))
    {
        // Ramp the makeup gain across the block instead of stamping a single
        // value with applyGain(): under fast GR modulation, computeAutoMakeup
        // returned a different value every block (because it reads
        // meterGainReduction, which is pre-smoothing dynamicGainDb). A
        // stepwise gain on a continuous signal generates a click at every
        // block boundary, distinct from the coefficient-rebuild crackle.
        const float targetMakeupGain = computeAutoMakeupGainLinear();
        buffer.applyGainRamp(0, numSamples, lastAppliedMakeupGain, targetMakeupGain);
        lastAppliedMakeupGain = targetMakeupGain;
    }
    else
    {
        // When auto-makeup is off, decay the stored gain toward unity so a
        // future re-enable resumes from a sensible starting ramp.
        lastAppliedMakeupGain = 1.0f;
    }
}

//==============================================================================
float DynamicEQProcessor::calculateDynamicGain(float inputLevelDb,
                                               int dynMode,
                                               float threshold,
                                               float ratio,
                                               float knee,
                                               float range) const
{
    if (dynMode == DynamicMode_Off)
        return 0.0f;
    
    float gainDb = 0.0f;
    
    if (dynMode == DynamicMode_Compress)
    {
        if (inputLevelDb > threshold - knee * 0.5f)
        {
            gainDb = computeSoftKnee(inputLevelDb, threshold, ratio, knee);
            gainDb = juce::jlimit(-range, 0.0f, gainDb);
        }
    }
    else if (dynMode == DynamicMode_Expand)
    {
        if (inputLevelDb > threshold - knee * 0.5f)
        {
            float excess = inputLevelDb - threshold;
            if (knee > 0.0f && excess < knee * 0.5f)
            {
                float kneeRatio = (excess + knee * 0.5f) / knee;
                gainDb = kneeRatio * excess * (1.0f - 1.0f / ratio);
            }
            else
            {
                gainDb = excess * (1.0f - 1.0f / ratio);
            }
            gainDb = juce::jlimit(0.0f, range, gainDb);
        }
    }
    else if (dynMode == DynamicMode_Gate)
    {
        if (inputLevelDb < threshold)
        {
            float below = threshold - inputLevelDb;
            gainDb = -below * ratio;
            gainDb = juce::jlimit(-range, 0.0f, gainDb);
        }
    }
    
    return gainDb;
}

float DynamicEQProcessor::computeSoftKnee(float inputDb, float threshold, float ratio, float knee) const
{
    // Guard: unity ratio means no compression — avoid indeterminate gain
    if (std::abs(ratio - 1.0f) < 0.01f)
        return 0.0f;

    if (knee <= 0.0f)
    {
        if (inputDb <= threshold)
            return 0.0f;
        return (threshold - inputDb) * (1.0f - 1.0f / ratio);
    }
    
    float kneeStart = threshold - knee * 0.5f;
    float kneeEnd = threshold + knee * 0.5f;
    
    if (inputDb <= kneeStart)
        return 0.0f;
    
    if (inputDb >= kneeEnd)
        return (threshold - inputDb) * (1.0f - 1.0f / ratio);
    
    float x = inputDb - kneeStart;
    float kneeWidth = knee;
    float kneeGain = (x * x) / (2.0f * kneeWidth) * (1.0f / ratio - 1.0f);
    
    return kneeGain;
}

//==============================================================================
void DynamicEQProcessor::setBandParams(int bandIndex, const DynamicBandParams& params)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    auto& p = bandParams[bandIndex];
    p.frequency.store(params.frequency, std::memory_order_relaxed);
    p.gain.store(params.gain, std::memory_order_relaxed);
    p.q.store(params.q, std::memory_order_relaxed);
    p.filterType.store(params.filterType, std::memory_order_relaxed);
    p.enabled.store(params.enabled, std::memory_order_relaxed);
    p.dynamicMode.store(params.dynamicMode, std::memory_order_relaxed);
    p.threshold.store(params.threshold, std::memory_order_relaxed);
    p.ratio.store(params.ratio, std::memory_order_relaxed);
    p.attackMs.store(params.attackMs, std::memory_order_relaxed);
    p.releaseMs.store(params.releaseMs, std::memory_order_relaxed);
    p.range.store(params.range, std::memory_order_relaxed);
    p.knee.store(params.knee, std::memory_order_relaxed);
    p.detection.store(params.detection, std::memory_order_relaxed);
    p.sidechainEnabled.store(params.sidechainEnabled, std::memory_order_relaxed);
    p.sidechainFreq.store(params.sidechainFreq, std::memory_order_relaxed);
    p.sidechainQ.store(params.sidechainQ, std::memory_order_relaxed);
    p.version.fetch_add(1, std::memory_order_release);
}

DynamicEQProcessor::DynamicBandParams DynamicEQProcessor::getBandParams(int bandIndex) const
{
    DynamicBandParams result;
    
    if (bandIndex < 0 || bandIndex >= maxBands)
        return result;
    
    const auto& p = bandParams[bandIndex];
    result.frequency = p.frequency.load(std::memory_order_relaxed);
    result.gain = p.gain.load(std::memory_order_relaxed);
    result.q = p.q.load(std::memory_order_relaxed);
    result.filterType = p.filterType.load(std::memory_order_relaxed);
    result.enabled = p.enabled.load(std::memory_order_relaxed);
    result.dynamicMode = p.dynamicMode.load(std::memory_order_relaxed);
    result.threshold = p.threshold.load(std::memory_order_relaxed);
    result.ratio = p.ratio.load(std::memory_order_relaxed);
    result.attackMs = p.attackMs.load(std::memory_order_relaxed);
    result.releaseMs = p.releaseMs.load(std::memory_order_relaxed);
    result.range = p.range.load(std::memory_order_relaxed);
    result.knee = p.knee.load(std::memory_order_relaxed);
    result.detection = p.detection.load(std::memory_order_relaxed);
    result.sidechainEnabled = p.sidechainEnabled.load(std::memory_order_relaxed);
    result.sidechainFreq = p.sidechainFreq.load(std::memory_order_relaxed);
    result.sidechainQ = p.sidechainQ.load(std::memory_order_relaxed);
    
    return result;
}

void DynamicEQProcessor::setBandEnabled(int bandIndex, bool enabled)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    bandParams[bandIndex].enabled.store(enabled, std::memory_order_relaxed);
}

void DynamicEQProcessor::setDynamicMode(int bandIndex, int mode)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    bandParams[bandIndex].dynamicMode.store(mode, std::memory_order_relaxed);
    bandParams[bandIndex].version.fetch_add(1, std::memory_order_release);
}

//==============================================================================
DynamicEQProcessor::BandMeter DynamicEQProcessor::getBandMeter(int bandIndex) const
{
    BandMeter meter;
    
    if (bandIndex >= 0 && bandIndex < maxBands)
    {
        const auto& state = bandStates[bandIndex];
        meter.inputLevel = state.meterInputLevel.load(std::memory_order_relaxed);
        meter.gainReduction = state.meterGainReduction.load(std::memory_order_relaxed);
        meter.outputLevel = state.meterOutputLevel.load(std::memory_order_relaxed);
    }
    
    return meter;
}

float DynamicEQProcessor::getTotalGainReduction() const
{
    float totalGR = 0.0f;
    
    for (int i = 0; i < maxBands; ++i)
    {
        if (bandParams[i].enabled.load(std::memory_order_relaxed) && 
            bandParams[i].dynamicMode.load(std::memory_order_relaxed) != DynamicMode_Off)
        {
            totalGR += bandStates[i].meterGainReduction.load(std::memory_order_relaxed);
        }
    }
    
    return totalGR;
}

float DynamicEQProcessor::computeAutoMakeupGainLinear() const noexcept
{
    float totalGainLinear = 1.0f;

    for (int i = 0; i < maxBands; ++i)
    {
        if (bandParams[i].enabled.load(std::memory_order_relaxed)
            && bandParams[i].dynamicMode.load(std::memory_order_relaxed) != DynamicMode_Off)
        {
            const float measuredGrDb =
                bandStates[i].meterGainReduction.load(std::memory_order_relaxed);
            const float safeGrDb = std::isfinite(measuredGrDb)
                ? juce::jlimit(-120.0f, 0.0f, measuredGrDb)
                : 0.0f;
            totalGainLinear *= juce::Decibels::decibelsToGain(safeGrDb);
        }
    }

    if (totalGainLinear < 0.999f)
    {
        const float safeGain = std::max(totalGainLinear, 1.0e-6f);
        return juce::jlimit(0.25f, 4.0f, 1.0f / safeGain);
    }

    return 1.0f;
}

//==============================================================================
void DynamicEQProcessor::updateBandCoefficients(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;

    const auto& params = bandParams[bandIndex];
    auto& state = bandStates[bandIndex];

    const float freq = params.frequency.load(std::memory_order_relaxed);
    const float gain = params.gain.load(std::memory_order_relaxed);
    const float q = params.q.load(std::memory_order_relaxed);
    const int filterType = params.filterType.load(std::memory_order_relaxed);
    const int dynMode = params.dynamicMode.load(std::memory_order_relaxed);

    // If the band is actively dynamic-owned (Compress/Expand on a gain-bearing
    // filter type), rebuild the live biquad against the CURRENT effective gain
    // — not the raw static gain. Otherwise a UI freq/Q/gain drag bumps the
    // version, this helper rebuilds against staticGainDb, then the dynamic
    // control-slice path notices the mismatch with staticGainDb + currentGain
    // and triggers ANOTHER crossfade right after. Two crossfades within a few
    // ms = audible coefficient churn on every drag frame, which is the residual
    // crackle the previous warm-start fix did not close.
    const bool dynamicActive =
        (dynMode == DynamicMode_Compress || dynMode == DynamicMode_Expand)
        && isGainBearingDynamicFilterType(filterType);
    const float effectiveGain = dynamicActive ? (gain + state.currentGain) : gain;

    state.eqCoeffs = makeEQCoefficients(filterType, freq, effectiveGain, q);
    state.staticFreqApplied = freq;
    state.staticGainApplied = gain;
    state.staticQApplied = q;
    state.staticFilterTypeApplied = filterType;
    state.appliedEffectiveGainDb = effectiveGain;
    state.liveEffectiveGainDb.store(effectiveGain, std::memory_order_relaxed);
}

void DynamicEQProcessor::updateDynamicTargets(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;

    const auto& params = bandParams[bandIndex];
    smoothedThresholds[bandIndex].setTargetValue(params.threshold.load(std::memory_order_relaxed));
    smoothedRatios[bandIndex].setTargetValue(params.ratio.load(std::memory_order_relaxed));
    smoothedRanges[bandIndex].setTargetValue(params.range.load(std::memory_order_relaxed));
    smoothedKnees[bandIndex].setTargetValue(params.knee.load(std::memory_order_relaxed));
    smoothedSidechainFreq[bandIndex].setTargetValue(params.sidechainFreq.load(std::memory_order_relaxed));
    smoothedSidechainQ[bandIndex].setTargetValue(params.sidechainQ.load(std::memory_order_relaxed));
}

void DynamicEQProcessor::updateSidechainState(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;

    const auto& params = bandParams[bandIndex];
    auto& state = bandStates[bandIndex];
    const bool enabled = params.sidechainEnabled.load(std::memory_order_relaxed);
    state.sidechainEnabledApplied = enabled;

    if (!enabled)
    {
        state.scCoeffs = BiquadCoeffs::makeBypass();
        state.scFreqApplied = params.sidechainFreq.load(std::memory_order_relaxed);
        state.scQApplied = params.sidechainQ.load(std::memory_order_relaxed);
        state.scFilterL.reset();
        state.scFilterR.reset();
        return;
    }

    const float scFreq = params.sidechainFreq.load(std::memory_order_relaxed);
    const float scQ = params.sidechainQ.load(std::memory_order_relaxed);
    const double sr = currentSampleRate.load(std::memory_order_relaxed);

    state.scCoeffs = BiquadCoeffs::makeBandPass(sr, scFreq, scQ);
    state.scFreqApplied = scFreq;
    state.scQApplied = scQ;
    state.scFilterL.reset();
    state.scFilterR.reset();
}

void DynamicEQProcessor::updateAttackReleaseCoeffs(int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    const auto& params = bandParams[bandIndex];
    auto& state = bandStates[bandIndex];
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    
    // Safety guard: avoid division/exp on invalid sample rate
    if (sr <= 0.0)
        return;
    
    const float attackMs = params.attackMs.load(std::memory_order_relaxed);
    const float releaseMs = params.releaseMs.load(std::memory_order_relaxed);
    
    float attackSamples = (attackMs / 1000.0f) * static_cast<float>(sr);
    float releaseSamples = (releaseMs / 1000.0f) * static_cast<float>(sr);
    
    state.attackCoeff = attackSamples > 0 ? std::exp(-1.0f / attackSamples) : 0.0f;
    state.releaseCoeff = releaseSamples > 0 ? std::exp(-1.0f / releaseSamples) : 0.0f;
}

BiquadCoeffs DynamicEQProcessor::makeEQCoefficients(
    int filterType, float freq, float gain, float q) const
{
    const double sr = currentSampleRate.load(std::memory_order_relaxed);

    if (sr <= 0.0)
        return BiquadCoeffs::makeBypass();

    freq = juce::jlimit(20.0f, static_cast<float>(sr * 0.499), freq);
    q    = juce::jlimit(0.1f, 40.0f, q);

    switch (filterType)
    {
        case 0: // LowCut
            return BiquadCoeffs::makeHighPass(sr, freq, q);
        case 1: // LowShelf
            return BiquadCoeffs::makeLowShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 2: // Peak
            if (std::abs(gain) < 0.05f)
                return BiquadCoeffs::makeBypass();
            return BiquadCoeffs::makePeakFilter(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 3: // HighShelf
            return BiquadCoeffs::makeHighShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 4: // HighCut
            return BiquadCoeffs::makeLowPass(sr, freq, q);
        case 5: // Notch
            return BiquadCoeffs::makeNotch(sr, freq, q);
        case 6: // BandPass
            return BiquadCoeffs::makeBandPass(sr, freq, q);
        case 7: // VintageLowShelf
            return BiquadCoeffs::makeLowShelf(
                sr, freq, vintageShelfQ(q), juce::Decibels::decibelsToGain(gain));
        case 8: // VintageHighShelf
            return BiquadCoeffs::makeHighShelf(
                sr, freq, vintageShelfQ(q), juce::Decibels::decibelsToGain(gain));
        default:
            return BiquadCoeffs::makeAllPass(sr, 20.0f, 0.1f);
    }
}

float DynamicEQProcessor::getMagnitudeForFrequency(float freq, double sampleRate) const
{
    double magnitude = 1.0;
    
    for (int i = 0; i < maxBands; ++i)
    {
        const auto& params = bandParams[i];
        if (!params.enabled.load(std::memory_order_relaxed))
            continue;

        const BiquadCoeffs coeffs = makeEQCoefficients(
            params.filterType.load(std::memory_order_relaxed),
            params.frequency.load(std::memory_order_relaxed),
            params.gain.load(std::memory_order_relaxed),
            params.q.load(std::memory_order_relaxed));
        if (!coeffs.valid)
            continue;

        magnitude *= coeffs.getMagnitudeForFrequency(freq, sampleRate);
    }
    
    return static_cast<float>(magnitude);
}

void DynamicEQProcessor::evaluateDynamicReplacementDeltaDbForFrequencyArray(
    const float* frequenciesHz,
    float* deltaDbOut,
    size_t numPoints,
    double sampleRate) const noexcept
{
    if (frequenciesHz == nullptr || deltaDbOut == nullptr || numPoints == 0)
        return;

    if (sampleRate <= 0.0)
        sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    struct DynamicBandSnapshot
    {
        BiquadCoeffs staticCoeffs;
        BiquadCoeffs liveCoeffs;
        int mode = DynamicMode_Off;
        float liveGateAmount = 1.0f;
    };

    std::array<DynamicBandSnapshot, maxBands> activeBands {};
    size_t activeCount = 0;

    for (int i = 0; i < maxBands; ++i)
    {
        if (!bandParams[i].enabled.load(std::memory_order_relaxed))
            continue;

        const int mode = bandParams[i].dynamicMode.load(std::memory_order_relaxed);
        if (mode == DynamicMode_Off)
            continue;

        const int filterType = bandParams[i].filterType.load(std::memory_order_relaxed);
        const float freq = bandParams[i].frequency.load(std::memory_order_relaxed);
        const float q = bandParams[i].q.load(std::memory_order_relaxed);
        const float staticGainDb = bandParams[i].gain.load(std::memory_order_relaxed);
        const float liveEffectiveGainDb =
            bandStates[i].liveEffectiveGainDb.load(std::memory_order_relaxed);

        auto& snapshot = activeBands[activeCount++];
        snapshot.mode = mode;
        snapshot.staticCoeffs = makeEQCoefficients(filterType, freq, staticGainDb, q);
        snapshot.liveCoeffs =
            (mode == DynamicMode_Gate || !isGainBearingDynamicFilterType(filterType))
                ? snapshot.staticCoeffs
                : makeEQCoefficients(filterType, freq, liveEffectiveGainDb, q);
        snapshot.liveGateAmount = bandStates[i].liveGateAmount.load(std::memory_order_relaxed);
    }

    if (activeCount == 0)
    {
        std::fill(deltaDbOut, deltaDbOut + numPoints, 0.0f);
        return;
    }

    const double mix = static_cast<double>(globalMix.load(std::memory_order_relaxed));
    const double makeupGain = autoMakeupEnabled.load(std::memory_order_relaxed)
        ? static_cast<double>(computeAutoMakeupGainLinear())
        : 1.0;
    constexpr double kFloor = 1.0e-6;
    constexpr std::complex<double> kOne(1.0, 0.0);

    for (size_t point = 0; point < numPoints; ++point)
    {
        const double freq = static_cast<double>(frequenciesHz[point]);
        double staticMagnitudeProduct = 1.0;
        std::complex<double> liveProduct = kOne;

        for (size_t band = 0; band < activeCount; ++band)
        {
            const auto& snapshot = activeBands[band];
            const std::complex<double> staticResponse =
                evaluateComplexResponse(snapshot.staticCoeffs, freq, sampleRate);

            staticMagnitudeProduct *= std::abs(staticResponse);

            std::complex<double> effectiveResponse = staticResponse;
            if (snapshot.mode == DynamicMode_Compress || snapshot.mode == DynamicMode_Expand)
            {
                effectiveResponse =
                    evaluateComplexResponse(snapshot.liveCoeffs, freq, sampleRate);
            }
            else if (snapshot.mode == DynamicMode_Gate)
            {
                effectiveResponse = staticResponse * static_cast<double>(snapshot.liveGateAmount);
            }

            liveProduct *= effectiveResponse;
        }

        const std::complex<double> stageResponse =
            ((1.0 - mix) + mix * liveProduct) * makeupGain;
        const double stageMag = std::max(kFloor, std::abs(stageResponse));
        const double staticMag = std::max(kFloor, staticMagnitudeProduct);

        deltaDbOut[point] = static_cast<float>(
            20.0 * std::log10(stageMag) - 20.0 * std::log10(staticMag));
    }
}
