#include "DynamicEQProcessor.h"
#include "DefaultBandFrequencies.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr double kMinProcessingSampleRate = 32000.0;

[[nodiscard]] bool validProcessingConfiguration(double sampleRate,
                                                int samplesPerBlock,
                                                int channels) noexcept
{
    return std::isfinite(sampleRate)
        && sampleRate >= kMinProcessingSampleRate
        && sampleRate <= BiquadCoeffs::maxProcessingSampleRate()
        && samplesPerBlock > 0 && channels > 0;
}

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

[[nodiscard]] bool isDynamicGainMode(int mode) noexcept
{
    return mode == DynamicEQProcessor::DynamicMode_Compress
        || mode == DynamicEQProcessor::DynamicMode_Expand
        || mode == DynamicEQProcessor::DynamicMode_Gate;
}

[[nodiscard]] int clampDetectorSource(int source) noexcept
{
    return juce::jlimit(DynamicEQProcessor::DetectorSource_InternalWideband,
                        DynamicEQProcessor::DetectorSource_ExternalFiltered,
                        source);
}

[[nodiscard]] bool isExternalDetectorSource(int source) noexcept
{
    const int clamped = clampDetectorSource(source);
    return clamped == DynamicEQProcessor::DetectorSource_ExternalWideband
        || clamped == DynamicEQProcessor::DetectorSource_ExternalFiltered;
}

[[nodiscard]] bool isFilteredDetectorSource(int source,
                                            bool legacySidechainEnabled) noexcept
{
    const int clamped = clampDetectorSource(source);
    if (clamped == DynamicEQProcessor::DetectorSource_InternalWideband)
        return legacySidechainEnabled;
    return clamped == DynamicEQProcessor::DetectorSource_InternalFiltered
        || clamped == DynamicEQProcessor::DetectorSource_ExternalFiltered;
}

[[nodiscard]] AIEQDSP::DynamicGainModel::Action dynamicActionForMode(int mode) noexcept
{
    if (mode == DynamicEQProcessor::DynamicMode_Compress)
        return AIEQDSP::DynamicGainModel::Action::Compress;
    if (mode == DynamicEQProcessor::DynamicMode_Expand
        || mode == DynamicEQProcessor::DynamicMode_Gate)
        return AIEQDSP::DynamicGainModel::Action::Expand;
    return AIEQDSP::DynamicGainModel::Action::Off;
}

[[nodiscard]] AIEQDSP::DynamicGainModel::TriggerSide dynamicTriggerForMode(
    int mode, int triggerSide) noexcept
{
    // Host ABI value 3 remains permanently valid and deterministically
    // overrides the separately stored trigger: legacy Gate == Expand+Below.
    if (mode == DynamicEQProcessor::DynamicMode_Gate)
        return AIEQDSP::DynamicGainModel::TriggerSide::Below;
    return triggerSide == DynamicEQProcessor::TriggerSide_Below
        ? AIEQDSP::DynamicGainModel::TriggerSide::Below
        : AIEQDSP::DynamicGainModel::TriggerSide::Above;
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

}

//==============================================================================
DynamicEQProcessor::DynamicEQProcessor()
{
    // Set default frequencies spread across spectrum
    static_assert(maxBands == static_cast<int>(AIEQDSP::defaultBandFrequencies.size()));
    
    for (int i = 0; i < maxBands; ++i)
    {
        const auto index = static_cast<size_t>(i);
        bandParams[index].frequency.store(AIEQDSP::defaultBandFrequencies[index],
                                          std::memory_order_relaxed);
        bandParams[index].enabled.store(true, std::memory_order_relaxed);
        bandParams[index].version.store(0, std::memory_order_relaxed);
        bandValidationFailures[index].store(
            static_cast<uint8_t>(BiquadValidationFailure::IntentionalBypass),
            std::memory_order_relaxed);
        sidechainValidationFailures[index].store(
            static_cast<uint8_t>(BiquadValidationFailure::IntentionalBypass),
            std::memory_order_relaxed);
        detectorAvailability[index].store(
            static_cast<uint8_t>(DetectorAvailability::Internal),
            std::memory_order_relaxed);
    }
}

//==============================================================================
void DynamicEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    if (!validProcessingConfiguration(sampleRate, samplesPerBlock, channels))
    {
        isPrepared.store(false, std::memory_order_release);
        return;
    }

    // Pre-allocate the dry copy so process() never resizes on the audio thread.
    // PluginProcessor forwards blocks up to jmax(samplesPerBlock * 4, 32768)
    // without another prepare(). samplesPerBlock * 8 is shorter than that
    // whenever the host prepared below 4096 samples, and the mix loop then
    // zeroed the tail whenever Dynamic EQ mix was not fully wet.
    static constexpr int kHostBlockCeiling = 32768;
    const int dryCapacity = juce::jmax(samplesPerBlock * 8, kHostBlockCeiling);
    dryBuffer.setSize(channels, dryCapacity, false, false, true);
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
    if (!validProcessingConfiguration(sampleRate, samplesPerBlock, channels))
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
        state.detectorEnvelope = 0.0;
        state.currentGain = 0.0f;
        state.inputHistoryWritePos = 0;
        state.inputHistoryCount = 0;
        state.inputHistoryL.fill(0.0f);
        state.inputHistoryR.fill(0.0f);
        state.samplesSinceLastRebuild = 0;
        state.dynamicModeApplied = -1;
        state.detectionModeApplied = -1;
        state.detectorSourceApplied = -1;
        detectorAvailability[static_cast<size_t>(i)].store(
            static_cast<uint8_t>(DetectorAvailability::Internal),
            std::memory_order_relaxed);
        
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
        state.meterInputLevel.store(-160.0f, std::memory_order_relaxed);
        state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
        state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
    }

    lastAppliedMakeupGain = 1.0f;
    lastAppliedMix = globalMix.load(std::memory_order_relaxed);

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
        
        state.detectorEnvelope = 0.0;
        state.currentGain = 0.0f;
        state.inputHistoryWritePos = 0;
        state.inputHistoryCount = 0;
        state.inputHistoryL.fill(0.0f);
        state.inputHistoryR.fill(0.0f);
        state.samplesSinceLastRebuild = 0;
        state.dynamicModeApplied = -1;
        state.detectionModeApplied = -1;
        state.detectorSourceApplied = -1;
        detectorAvailability[static_cast<size_t>(i)].store(
            static_cast<uint8_t>(DetectorAvailability::Internal),
            std::memory_order_relaxed);
        state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
        state.liveEffectiveGainDb.store(
            bandParams[i].gain.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        state.appliedEffectiveGainDb = bandParams[i].gain.load(std::memory_order_relaxed);
        state.meterInputLevel.store(-160.0f, std::memory_order_relaxed);
        state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
        state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
    }
    
    if (lookaheadBuffer.getNumSamples() > 0)
        lookaheadBuffer.clear();
    lookaheadWritePos = 0;

    lastAppliedMakeupGain = 1.0f;
    lastAppliedMix = globalMix.load(std::memory_order_relaxed);
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
void DynamicEQProcessor::process(juce::AudioBuffer<float>& buffer,
                                 const juce::AudioBuffer<float>* externalDetector)
{
    // Hardware denormal flushing
    juce::ScopedNoDenormals noDenormals;
    
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), 2);
    const double sr = currentSampleRate.load(std::memory_order_relaxed);

    const int externalChannels = externalDetector != nullptr
        ? juce::jmin(externalDetector->getNumChannels(), 2)
        : 0;
    const bool externalDetectorAvailable = externalDetector != nullptr
        && externalChannels > 0
        && externalDetector->getNumSamples() >= numSamples;
    
    // CRITICAL: Safety check - if not prepared, just pass through
    if (!isPrepared.load(std::memory_order_acquire) || numSamples == 0 || channels == 0 || sr <= 0.0)
        return;
    
    //==========================================================================
    // LOCK-FREE: Copy dry signal for mix (no allocations)
    //==========================================================================
    const float mix = globalMix.load(std::memory_order_relaxed);

    // A block past the dry copy (host ceiling, or 8x prepare when that is larger)
    // cannot be blended without allocating. The tail is cleared rather than read
    // off the end of dryBuffer. prepare() sizes that copy for the host ceiling,
    // so this is the overflow past that contract, not a normal variable block.

    const int safeSamples = juce::jmin(numSamples, dryBuffer.getNumSamples());
    const bool mixNeedsDry = lastAppliedMix < 0.999f || mix < 0.999f;

    if (mixNeedsDry)
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
            const bool externalRequested = isExternalDetectorSource(
                params.detectorSource.load(std::memory_order_relaxed));
            detectorAvailability[static_cast<size_t>(bandIdx)].store(
                static_cast<uint8_t>(externalRequested
                    ? (externalDetectorAvailable
                        ? DetectorAvailability::ExternalAvailable
                        : DetectorAvailability::ExternalUnavailable)
                    : DetectorAvailability::Internal),
                std::memory_order_release);
            // FIX: zero the meter when band is disabled so it doesn't freeze at last value
            state.inputHistoryWritePos = 0;
            state.inputHistoryCount = 0;
            state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
            state.meterInputLevel.store(-160.0f, std::memory_order_relaxed);
            state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
            state.liveCurrentGainDb.store(0.0f, std::memory_order_relaxed);
            state.liveEffectiveGainDb.store(0.0f, std::memory_order_relaxed);
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
            const int dynModeNow = params.dynamicMode.load(std::memory_order_relaxed);
            const int detectionModeNow = params.detection.load(std::memory_order_relaxed);
            const int detectorSourceNow = clampDetectorSource(
                params.detectorSource.load(std::memory_order_relaxed));
            const bool dynamicModeChanged = state.dynamicModeApplied != dynModeNow;
            const bool detectionModeChanged = state.detectionModeApplied != detectionModeNow;
            const bool detectorSourceChanged = state.detectorSourceApplied != detectorSourceNow;
            if (dynamicModeChanged || detectionModeChanged || detectorSourceChanged)
            {
                state.detectorEnvelope = 0.0;
                state.currentGain = 0.0f;
            }
            if (detectorSourceChanged)
            {
                // A filtered detector carries source-specific history. Never
                // let samples from the previous source leak into the first
                // block evaluated after an internal/external source switch.
                state.scFilterL.reset();
                state.scFilterR.reset();
            }
            const bool sidechainEnabledNow = isFilteredDetectorSource(
                detectorSourceNow,
                params.sidechainEnabled.load(std::memory_order_relaxed));
            const bool staticShapeChanged =
                state.staticFilterTypeApplied != filterTypeNow
                || std::abs(state.staticFreqApplied - freqNow) > 1.0e-6f
                || std::abs(state.staticGainApplied - gainNow) > 1.0e-6f
                || std::abs(state.staticQApplied - qNow) > 1.0e-6f
                || dynamicModeChanged
                || detectionModeChanged;

            const bool sidechainStateChanged =
                state.sidechainEnabledApplied != sidechainEnabledNow;

            const BiquadCoeffs previousCoeffs = state.eqCoeffs;
            updateDynamicTargets(bandIdx);
            if (staticShapeChanged)
                updateBandCoefficients(bandIdx);
            if (sidechainStateChanged)
                updateSidechainState(bandIdx);
            updateAttackReleaseCoeffs(bandIdx);
            state.dynamicModeApplied = dynModeNow;
            state.detectionModeApplied = detectionModeNow;
            state.detectorSourceApplied = detectorSourceNow;
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
                const bool dynamicActiveBand =
                    isDynamicGainMode(dynModeNow)
                    && isGainBearingDynamicFilterType(filterTypeNow);

                // PATH-ARBITRATION FIX (residual drag crackle).
                // A long (128-256 sample) drag crossfade STARVES the dynamic
                // control-slice rebuild: that path is guarded by
                // `xfade.remaining <= 0` (see the gain-bearing branch below), so
                // while the long drag fade is in flight the control slice cannot
                // retune. state.currentGain keeps following the smoothed detector
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

            // Update the meter immediately from the already-smoothed detector.
            // The audio-side value is recomputed per sample below; there is no
            // second attack/release stage on gain.
            const int dynMode = params.dynamicMode.load(std::memory_order_relaxed);
            if (dynMode != DynamicMode_Off)
            {
                const int detMode = params.detection.load(std::memory_order_relaxed);
                const auto detectionMode = detMode == DetectionMode_Peak
                    ? AIEQDSP::DynamicGainModel::DetectionMode::Peak
                    : AIEQDSP::DynamicGainModel::DetectionMode::RMS;
                const float currentEnv = static_cast<float>(
                    AIEQDSP::DynamicGainModel::envelopeToDb(
                        state.detectorEnvelope, detectionMode));
                const float threshold = params.threshold.load(std::memory_order_relaxed);
                const float ratio     = params.ratio.load(std::memory_order_relaxed);
                const float knee      = params.knee.load(std::memory_order_relaxed);
                const float range     = params.range.load(std::memory_order_relaxed);
                const float freshGR   = calculateDynamicGain(
                    currentEnv,
                    dynMode,
                    params.triggerSide.load(std::memory_order_relaxed),
                    params.gain.load(std::memory_order_relaxed),
                    threshold, ratio, knee, range);
                state.meterGainReduction.store(freshGR, std::memory_order_relaxed);
            }
        }
        
        // Sidechain smoothing at control rate: advance to target, update coeffs if changed
        smoothedSidechainFreq[bandIdx].skip(numSamples);
        smoothedSidechainQ[bandIdx].skip(numSamples);
        const float scFreqNow = smoothedSidechainFreq[bandIdx].getCurrentValue();
        const float scQNow = smoothedSidechainQ[bandIdx].getCurrentValue();
        const int detectorSource = clampDetectorSource(
            params.detectorSource.load(std::memory_order_relaxed));
        const bool detectorFiltered = isFilteredDetectorSource(
            detectorSource,
            params.sidechainEnabled.load(std::memory_order_relaxed));
        const bool externalRequested = isExternalDetectorSource(detectorSource);
        const bool detectorAvailable = !externalRequested || externalDetectorAvailable;
        detectorAvailability[static_cast<size_t>(bandIdx)].store(
            static_cast<uint8_t>(externalRequested
                ? (detectorAvailable ? DetectorAvailability::ExternalAvailable
                                     : DetectorAvailability::ExternalUnavailable)
                : DetectorAvailability::Internal),
            std::memory_order_release);

        if (externalRequested && !detectorAvailable)
        {
            // The bus may be reconnected later. Reset while unavailable so a
            // filtered external detector resumes from a neutral state rather
            // than stale history captured before disconnection.
            state.scFilterL.reset();
            state.scFilterR.reset();
        }

        if (detectorFiltered && state.scCoeffs.valid)
        {
            constexpr float scEps = 1e-3f;
            if (std::abs(scFreqNow - state.scFreqApplied) > scEps
                || std::abs(scQNow - state.scQApplied) > scEps)
            {
                const double sr2 = currentSampleRate.load(std::memory_order_relaxed);
                const auto precision = highPrecisionMode.load(std::memory_order_acquire)
                    ? BiquadPrecision::HighPrecision : BiquadPrecision::Automatic;
                auto newCoeffs = BiquadCoeffs::makeBandPass(
                    sr2, scFreqNow, scQNow, precision);
                sidechainValidationFailures[static_cast<size_t>(bandIdx)].store(
                    static_cast<uint8_t>(newCoeffs.failure), std::memory_order_release);
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
        const bool gainBearingMode = isDynamicGainMode(dynMode)
            && isGainBearingDynamicFilterType(filterType);
        
        //----------------------------------------------------------------------
        // DYNAMIC PROCESSING
        //----------------------------------------------------------------------
        if (dynMode != DynamicMode_Off)
        {
            const int detMode = params.detection.load(std::memory_order_relaxed);
            const int triggerSide = params.triggerSide.load(std::memory_order_relaxed);
            const float attackCoeff = state.attackCoeff;
            const float releaseCoeff = state.releaseCoeff;
            const float staticGainDb = params.gain.load(std::memory_order_relaxed);
            const float bandFreq = params.frequency.load(std::memory_order_relaxed);
            const float bandQ = params.q.load(std::memory_order_relaxed);
            
            float* outLPtr = buffer.getWritePointer(0);
            float* outRPtr = channels > 1 ? buffer.getWritePointer(1) : nullptr;
            const float* detectBaseL = useLookahead && detectDelayL ? detectDelayL : outLPtr;
            const float* detectBaseR = (useLookahead && detectDelayR) ? detectDelayR : (channels > 1 ? outRPtr : detectBaseL);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float threshold = smoothedThresholds[bandIdx].getNextValue();
                const float ratio = smoothedRatios[bandIdx].getNextValue();
                const float range = smoothedRanges[bandIdx].getNextValue();
                const float knee = smoothedKnees[bandIdx].getNextValue();
                
                // Get input samples (audio path already delayed if lookahead active)
                float inL = outLPtr[sample];
                float inR = channels > 1 ? outRPtr[sample] : inL;
                
                // Internal detection sees the undelayed main signal when
                // lookahead is active. External detection is already aligned by
                // PluginProcessor for the active phase path; lookahead then
                // intentionally delays only the main program.
                float detectL = 0.0f;
                float detectR = 0.0f;
                int detectorChannelCount = channels;
                if (externalRequested && detectorAvailable)
                {
                    detectL = externalDetector->getReadPointer(0)[sample];
                    detectR = externalChannels > 1
                        ? externalDetector->getReadPointer(1)[sample]
                        : detectL;
                    detectorChannelCount = externalChannels;
                }
                else if (!externalRequested)
                {
                    detectL = useLookahead
                        ? detectBaseL[(lookaheadWriteStart + sample) % delaySize]
                        : detectBaseL[sample];
                    detectR = channels > 1
                        ? (useLookahead
                            ? detectBaseR[(lookaheadWriteStart + sample) % delaySize]
                            : detectBaseR[sample])
                        : detectL;
                }
                
                // Sidechain filtering
                // When sidechain is enabled: filter the detect signal with the sidechain bandpass.
                // When disabled: use the raw detect signal directly for level detection.
                // NOTE: do NOT use eqFiltersL[1] here — that cascades a second EQ stage onto
                // the detector signal, which distorts the gain-reduction curve. The second filter
                // slot in the array is reserved for future 2nd-order (12dB) filter support.
                float scL = detectL, scR = detectR;
                if (detectorAvailable && detectorFiltered && state.scCoeffs.valid)
                {
                    scL = state.scFilterL.processSample(detectL, state.scCoeffs);
                    scR = state.scFilterR.processSample(detectR, state.scCoeffs);
                }
                
                const auto detectionMode = detMode == DetectionMode_Peak
                    ? AIEQDSP::DynamicGainModel::DetectionMode::Peak
                    : AIEQDSP::DynamicGainModel::DetectionMode::RMS;
                float smoothedEnv = -160.0f;
                float dynamicGainDb = 0.0f;
                if (detectorAvailable)
                {
                    const double detectorValue = AIEQDSP::DynamicGainModel::detectorInput(
                        static_cast<double>(scL), static_cast<double>(scR),
                        detectorChannelCount, detectionMode);
                    state.detectorEnvelope = AIEQDSP::DynamicGainModel::advanceEnvelope(
                        state.detectorEnvelope, detectorValue,
                        static_cast<double>(attackCoeff), static_cast<double>(releaseCoeff));
                    smoothedEnv = static_cast<float>(
                        AIEQDSP::DynamicGainModel::envelopeToDb(
                            state.detectorEnvelope, detectionMode));

                    // The detector is the sole attack/release stage during
                    // ordinary signal-driven operation.
                    dynamicGainDb = calculateDynamicGain(
                        smoothedEnv, dynMode, triggerSide, staticGainDb,
                        threshold, ratio, knee, range);
                    state.currentGain = dynamicGainDb;
                }
                else
                {
                    // An absent external bus is not silence: feeding zero into a
                    // Below detector would incorrectly open a gate/expander. Keep
                    // the detector unavailable and release the last applied
                    // control value toward neutral without evaluating the curve.
                    state.detectorEnvelope = AIEQDSP::DynamicGainModel::advanceEnvelope(
                        state.detectorEnvelope, 0.0,
                        static_cast<double>(attackCoeff), static_cast<double>(releaseCoeff));
                    state.currentGain = std::isfinite(state.currentGain)
                        ? state.currentGain * releaseCoeff
                        : 0.0f;
                    if (std::abs(state.currentGain) < 1.0e-6f)
                        state.currentGain = 0.0f;
                    dynamicGainDb = state.currentGain;
                }
                
                // Update metering (atomic)
                // The GUI owns any purely visual decay. The audio value is already
                // temporally defined by the single detector envelope above.
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
                        juce::jlimit(-36.0f, 36.0f, staticGainDb + state.currentGain);
                    if (std::abs(targetEffectiveGainDb - state.appliedEffectiveGainDb)
                        > kEffectiveGainEpsilonDb)
                    {
                        auto& xfade = bandCrossfades[bandIdx];
                        if (xfade.remaining <= 0)
                        {
                            const BiquadCoeffs newCoeffs = makeEQCoefficients(
                                filterType, bandFreq, targetEffectiveGainDb, bandQ);
                            bandValidationFailures[static_cast<size_t>(bandIdx)].store(
                                static_cast<uint8_t>(newCoeffs.failure),
                                std::memory_order_release);
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
        }
    }
    
    //==========================================================================
    // Apply global mix
    //==========================================================================
    if (mixNeedsDry && dryBuffer.getNumSamples() > 0)
    {
        const float mixStart = lastAppliedMix;
        const float mixEnd = mix;
        const float inv = safeSamples > 1 ? 1.0f / static_cast<float>(safeSamples - 1) : 1.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);

            for (int s = 0; s < safeSamples; ++s)
            {
                const float m = mixStart + (mixEnd - mixStart) * static_cast<float>(s) * inv;
                wet[s] = dry[s] * (1.0f - m) + wet[s] * m;
            }

            if (numSamples > safeSamples)
                buffer.clear(ch, safeSamples, numSamples - safeSamples);
        }
    }
    lastAppliedMix = mix;
    
    //==========================================================================
    // Auto makeup gain
    //==========================================================================
    if (autoMakeupEnabled.load(std::memory_order_relaxed))
    {
        // Ramp the makeup gain across the block instead of stamping a single
        // value with applyGain(): under fast GR modulation, computeAutoMakeup
        // returns a different value every block because it reads the live
        // detector-derived dynamic gain. A
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
                                               int triggerSide,
                                               float staticGainDb,
                                               float threshold,
                                               float ratio,
                                               float knee,
                                               float range) const
{
    return static_cast<float>(AIEQDSP::DynamicGainModel::evaluate(
        static_cast<double>(inputLevelDb),
        static_cast<double>(staticGainDb),
        dynamicActionForMode(dynMode),
        dynamicTriggerForMode(dynMode, triggerSide),
        static_cast<double>(threshold),
        static_cast<double>(ratio),
        static_cast<double>(knee),
        static_cast<double>(range)).dynamicGainDb);
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
    p.triggerSide.store(params.triggerSide, std::memory_order_relaxed);
    p.detectorSource.store(clampDetectorSource(params.detectorSource),
                           std::memory_order_relaxed);
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
    result.triggerSide = p.triggerSide.load(std::memory_order_relaxed);
    result.detectorSource = p.detectorSource.load(std::memory_order_relaxed);
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
        isDynamicGainMode(dynMode)
        && isGainBearingDynamicFilterType(filterType);
    const float effectiveGain = dynamicActive
        ? juce::jlimit(-36.0f, 36.0f, gain + state.currentGain)
        : gain;

    state.eqCoeffs = makeEQCoefficients(filterType, freq, effectiveGain, q);
    bandValidationFailures[static_cast<size_t>(bandIndex)].store(
        static_cast<uint8_t>(state.eqCoeffs.failure), std::memory_order_release);
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
    const bool enabled = isFilteredDetectorSource(
        params.detectorSource.load(std::memory_order_relaxed),
        params.sidechainEnabled.load(std::memory_order_relaxed));
    state.sidechainEnabledApplied = enabled;

    if (!enabled)
    {
        state.scCoeffs = BiquadCoeffs::makeBypass();
        sidechainValidationFailures[static_cast<size_t>(bandIndex)].store(
            static_cast<uint8_t>(state.scCoeffs.failure), std::memory_order_release);
        state.scFreqApplied = params.sidechainFreq.load(std::memory_order_relaxed);
        state.scQApplied = params.sidechainQ.load(std::memory_order_relaxed);
        state.scFilterL.reset();
        state.scFilterR.reset();
        return;
    }

    const float scFreq = params.sidechainFreq.load(std::memory_order_relaxed);
    const float scQ = params.sidechainQ.load(std::memory_order_relaxed);
    const double sr = currentSampleRate.load(std::memory_order_relaxed);

    const auto precision = highPrecisionMode.load(std::memory_order_acquire)
        ? BiquadPrecision::HighPrecision : BiquadPrecision::Automatic;
    state.scCoeffs = BiquadCoeffs::makeBandPass(sr, scFreq, scQ, precision);
    sidechainValidationFailures[static_cast<size_t>(bandIndex)].store(
        static_cast<uint8_t>(state.scCoeffs.failure), std::memory_order_release);
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

    if (!std::isfinite(sr) || sr < kMinProcessingSampleRate
        || sr > BiquadCoeffs::maxProcessingSampleRate())
        return BiquadCoeffs::makeRejected(BiquadValidationFailure::InvalidDomain);

    freq = juce::jlimit(20.0f, static_cast<float>(sr * 0.499), freq);
    q    = juce::jlimit(0.1f, 40.0f, q);
    const auto precision = highPrecisionMode.load(std::memory_order_acquire)
        ? BiquadPrecision::HighPrecision : BiquadPrecision::Automatic;

    switch (filterType)
    {
        case 0: // LowCut
            return BiquadCoeffs::makeHighPass(sr, freq, q, precision);
        case 1: // LowShelf
            return BiquadCoeffs::makeLowShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain), precision);
        case 2: // Peak
            if (std::abs(gain) < 0.05f)
                return BiquadCoeffs::makeBypass();
            return BiquadCoeffs::makePeakFilter(
                sr, freq, q, juce::Decibels::decibelsToGain(gain), precision);
        case 3: // HighShelf
            return BiquadCoeffs::makeHighShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain), precision);
        case 4: // HighCut
            return BiquadCoeffs::makeLowPass(sr, freq, q, precision);
        case 5: // Notch
            return BiquadCoeffs::makeNotch(sr, freq, q, precision);
        case 6: // BandPass
            return BiquadCoeffs::makeBandPass(sr, freq, q, precision);
        case 7: // VintageLowShelf
            return BiquadCoeffs::makeLowShelf(
                sr, freq, vintageShelfQ(q), juce::Decibels::decibelsToGain(gain), precision);
        case 8: // VintageHighShelf
            return BiquadCoeffs::makeHighShelf(
                sr, freq, vintageShelfQ(q), juce::Decibels::decibelsToGain(gain), precision);
        default:
            return BiquadCoeffs::makeAllPass(sr, 20.0f, 0.1f, precision);
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

    // The coefficients compared below are built by makeEQCoefficients(), which
    // designs them against THIS instance's currentSampleRate. Evaluating them at
    // any other rate reads a filter that was never built: the same bell comes out
    // shallower, because its normalised centre frequency has moved.
    //
    // That is not hypothetical. The Natural Phase instance runs on the
    // oversampled buffer at 2x or 4x the host rate, and the display asked for the
    // response at the host rate. Measured on a 500 Hz band with 8:1 compression,
    // the drawn curve swung 4.93 dB where the underlying gain swung 10.40 —
    // identical to the other modes, which happen to agree with the host rate and
    // so never showed it.
    //
    // The response of a filter is a physical fact, independent of the rate it is
    // computed at, PROVIDED design and evaluation agree. So the instance's own
    // rate wins here, and the argument is kept only as a fallback for an
    // unprepared processor.
    const double coeffSampleRate = currentSampleRate.load(std::memory_order_relaxed);
    if (coeffSampleRate > 0.0)
        sampleRate = coeffSampleRate;
    else if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    struct DynamicBandSnapshot
    {
        BiquadCoeffs staticCoeffs;
        BiquadCoeffs liveCoeffs;
        int mode = DynamicMode_Off;
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
            (!isDynamicGainMode(mode) || !isGainBearingDynamicFilterType(filterType))
                ? snapshot.staticCoeffs
                : makeEQCoefficients(filterType, freq, liveEffectiveGainDb, q);
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
            if (isDynamicGainMode(snapshot.mode))
            {
                effectiveResponse =
                    evaluateComplexResponse(snapshot.liveCoeffs, freq, sampleRate);
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
