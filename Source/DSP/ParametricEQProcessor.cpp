#include "ParametricEQProcessor.h"
#include "NumericSafety.h"
#include <cmath>

namespace
{
constexpr double kMinProcessingSampleRate = 32000.0;

inline bool validProcessingConfiguration(double sampleRate, int samplesPerBlock,
                                         int channels) noexcept
{
    return std::isfinite(sampleRate)
        && sampleRate >= kMinProcessingSampleRate
        && sampleRate <= BiquadCoeffs::maxProcessingSampleRate()
        && samplesPerBlock > 0 && channels > 0;
}

}

//==============================================================================
ParametricEQProcessor::ParametricEQProcessor()
{
    // Initialize all bands as disabled
    for (int i = 0; i < getMaxBands(); ++i)
    {
        bandParams[i].enabled.store(false, std::memory_order_relaxed);
        bandParams[i].frequency.store(1000.0f, std::memory_order_relaxed);
        bandParams[i].gain.store(0.0f, std::memory_order_relaxed);
        bandParams[i].q.store(1.0f, std::memory_order_relaxed);
        bandParams[i].type.store(static_cast<int>(Peak), std::memory_order_relaxed);
        bandParams[i].curveMode.store(
            static_cast<uint8_t>(CurveMode::Legacy), std::memory_order_relaxed);
        bandParams[i].version.store(0, std::memory_order_relaxed);
        bandValidationFailures[static_cast<size_t>(i)].store(
            static_cast<uint8_t>(BiquadValidationFailure::IntentionalBypass),
            std::memory_order_relaxed);
    }
}

//==============================================================================
void ParametricEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    if (!validProcessingConfiguration(sampleRate, samplesPerBlock, channels))
    {
        isPrepared.store(false, std::memory_order_release);
        return;
    }
    resetRuntimeStateNoAllocation(sampleRate, samplesPerBlock, channels);
}

bool ParametricEQProcessor::reconfigureNoAllocation(double sampleRate,
                                                    int samplesPerBlock,
                                                    int channels) noexcept
{
    if (!validProcessingConfiguration(sampleRate, samplesPerBlock, channels))
        return false;
    resetRuntimeStateNoAllocation(sampleRate, samplesPerBlock, channels);
    return true;
}

void ParametricEQProcessor::resetRuntimeStateNoAllocation(double sampleRate,
                                                          int samplesPerBlock,
                                                          int channels) noexcept
{
    // Store sample rate atomically
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    numChannels = channels;

    // Reset all band filter states (BiquadState is POD, no prepare needed)
    for (int i = 0; i < getMaxBands(); ++i)
    {
        auto& state = bandStates[i];
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            state.filtersL[s].reset();
            state.filtersR[s].reset();
        }
        state.tptStateL.reset();
        state.tptStateR.reset();
        state.useTpt = false;
        state.appliedVintage = false;
        state.lastVersion = 0;  // Force coefficient update
        state.prepared = true;
    }
    
    // Update coefficients for all active bands
    const int numBands = numActiveBands.load(std::memory_order_acquire);
    for (int i = 0; i < numBands; ++i)
    {
        updateCoefficientsForBand(i);
    }
    
    isPrepared.store(true, std::memory_order_release);
}

void ParametricEQProcessor::reset()
{
    for (int i = 0; i < getMaxBands(); ++i)
    {
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            bandStates[i].filtersL[s].reset();
            bandStates[i].filtersR[s].reset();
            bandStates[i].coefficients[s] = BiquadCoeffs{};
        }
        bandStates[i].tptStateL.reset();
        bandStates[i].tptStateR.reset();
        bandStates[i].tptCoefficients = AIEQDSP::TptSvfCoefficients{};
        bandStates[i].useTpt = false;
        bandStates[i].appliedVintage = false;
    }
}

//==============================================================================
void ParametricEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    // Hardware denormal flushing (DAZ/FTZ) - relies on this, no manual loop needed
    juce::ScopedNoDenormals noDenormals;
    
    // Early exit if bypassed
    if (bypassed.load(std::memory_order_relaxed))
        return;
    
    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    
    // CRITICAL: Safety check - if not prepared, just pass through with gain
    if (!isPrepared.load(std::memory_order_acquire) || sr <= 0.0 || numSamples <= 0 || bufferChannels <= 0)
    {
        const float gain = outputGain.load(std::memory_order_relaxed);
        if (std::abs(gain - 1.0f) > 0.0001f)
            buffer.applyGain(gain);
        return;
    }
    
    //==========================================================================
    // LOCK-FREE PARAMETER READ
    // Read all band parameters atomically at start of block
    //==========================================================================

    //==========================================================================
    // DRAIN CROSSFADE COMMAND QUEUE (message thread → audio thread handoff)
    // Must run before version-change detection so that when the message
    // thread has BOTH queued a crossfade AND updated bandParams (the typical
    // pattern for type changes), the crossfade snapshot captures the OLD
    // coefficients before the version-change path overwrites them.
    //==========================================================================
    {
        CrossfadeCommand cmd;
        // Bounded drain: the queue has 32 slots; processing all in one
        // block is trivial (< 1µs for 24 bands of copy). No allocation.
        while (crossfadeCommands.tryPop(cmd))
        {
            if (cmd.kind == CrossfadeCommand::PerBand)
                executePerBandCrossfadeSetup(cmd.bandIndex, cmd.fadeSamples);
            else
                executeWholeChainCrossfadeSetup(cmd.fadeSamples);
        }
    }

    const int localNumBands = numActiveBands.load(std::memory_order_acquire);
    bool hasSolo = false;
    
    // First pass: check for solo and update coefficients if needed
    for (int i = 0; i < localNumBands; ++i)
    {
        const auto& params = bandParams[i];
        auto& state = bandStates[i];
        
        // Check if coefficients need update (version changed)
        const uint64_t currentVersion = params.version.load(std::memory_order_acquire);
        if (currentVersion != state.lastVersion)
        {
            // ── Arm per-band output crossfade BEFORE updating coefficients ──
            // Save old filter state so ordinary coefficient motion can use the
            // adaptive 1024/2048-sample blend below. Explicit Type/CurveMode
            // changes are armed separately by PluginProcessor with the signed
            // 128-sample topology contract.
            auto& xfade = bandCrossfades[i];
            const bool oldPathValid = state.useTpt
                ? state.tptCoefficients.isValid()
                : state.coefficients[0].valid;
            if (xfade.remaining <= 0 && oldPathValid)
            {
                xfade.oldNumStages = state.numActiveStages;
                xfade.oldUseTpt = state.useTpt;
                xfade.oldVintage = state.appliedVintage;
                xfade.oldTptStateL = state.tptStateL;
                xfade.oldTptStateR = state.tptStateR;
                xfade.oldTptCoefficients = state.tptCoefficients;
                for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
                {
                    xfade.oldCoeffs[s] = state.coefficients[s];
                    xfade.oldFiltersL[s] = state.filtersL[s];
                    xfade.oldFiltersR[s] = state.filtersR[s];
                }
                // Adaptive fade: 1024 base, 2048 for high-Q filters where
                // narrow resonant peaks ring longer and need more settling time.
                // Rationale (Fix A, 2026-04-22): a 60Hz UI drag leaves ~735 samples
                // gap between mouseDrag events @ 44.1kHz. The previous 128/256
                // window ended ~3ms in and left ~12ms of "new steady-state" before
                // the next version-bump re-armed — producing periodic clicks at the
                // drag cadence. Extending to 1024/2048 keeps the crossfade active
                // across consecutive drag updates, so adjacent transitions blend
                // instead of butt-joining. Still sample-based and deterministic.
                const float qVal = params.q.load(std::memory_order_relaxed);
                const int fadeSamples = (qVal > 10.0f) ? 2048 : 1024;
                xfade.remaining = fadeSamples;
                xfade.total = fadeSamples;

                // Cat 2 Fix: do NOT reset live filter state here.
                // During sample-count-driven bounded slew, setBandParameters is
                // called every block → version bumps → auto-arm triggers here.
                // With fadeSamples==blockSize==128, the crossfade ends exactly at
                // the block boundary, so the NEXT block re-arms and would reset
                // the filter again. That cumulative reset produces a ~0.4 click
                // (observed at AB switch@block60 in HostSessionClickTest).
                //
                // Keeping the live state intact gives both paths a common starting
                // point (current state). The blend becomes:
                //   out = oldCoefs(state) + (newCoefs(state) - oldCoefs(state)) * t
                // For small smoothing deltas, oldCoefs(state) ≈ newCoefs(state) and
                // the crossfade is near-identity — no audible artifact. For large
                // discontinuous changes (type switch), the explicit external
                // beginBandCrossfade() path still performs the reset via the
                // dedicated API below.
                //
                // (External resets for type changes continue to be issued by
                //  PluginProcessor::applySmoothedBandParams via beginBandCrossfade.)
            }

            // Read parameters
            const float freq = params.frequency.load(std::memory_order_relaxed);
            const float gain = params.gain.load(std::memory_order_relaxed);
            const float q = params.q.load(std::memory_order_relaxed);
            const int type = params.type.load(std::memory_order_relaxed);
            const int slope = params.slope.load(std::memory_order_relaxed);
            const auto curveMode = static_cast<CurveMode>(
                params.curveMode.load(std::memory_order_relaxed));
            const bool vintage = params.vintageMode.load(std::memory_order_relaxed);

            const auto design = makeFilterDesign(
                static_cast<FilterType>(type), freq, gain, q, slope, sr, curveMode);
            const bool topologyChanged = state.useTpt != design.useTpt;
            state.numActiveStages = design.numStages;
            state.coefficients = design.coefficients;
            state.tptCoefficients = design.tptCoefficients;
            state.useTpt = design.useTpt;
            state.appliedVintage = !design.useTpt && vintage
                && (type == static_cast<int>(VintageLowShelf)
                    || type == static_cast<int>(VintageHighShelf));
            if (topologyChanged)
            {
                state.tptStateL.reset();
                state.tptStateR.reset();
                for (auto& filter : state.filtersL) filter.reset();
                for (auto& filter : state.filtersR) filter.reset();
            }
            bandValidationFailures[static_cast<size_t>(i)].store(
                static_cast<uint8_t>(design.failure), std::memory_order_release);

            state.lastVersion = currentVersion;
        }
        
        // Check solo status
        if (params.solo.load(std::memory_order_relaxed) && 
            params.enabled.load(std::memory_order_relaxed))
        {
            hasSolo = true;
        }
    }
    
    //==========================================================================
    // AUDIO PROCESSING - Completely lock-free
    //==========================================================================

    // Whole-chain crossfade (A/B switch): process input through BOTH old chain and new chain,
    // blend at output. This is correct for multi-band cascades unlike per-band crossfade.
    auto& wc = wholeChainXfade;
    if (wc.remaining > 0)
    {
        const float invTotal = 1.0f / static_cast<float>(wc.total);

        for (int i = 0; i < numSamples; ++i)
        {
            const float t = 1.0f - static_cast<float>(wc.remaining) * invTotal;

            // --- Left channel ---
            if (bufferChannels > 0)
            {
                const float input = buffer.getSample(0, i);

                // Old chain: cascade through all old bands
                float oldSample = input;
                for (int b = 0; b < wc.oldNumBands; ++b)
                {
                    auto& ob = wc.oldBands[b];
                    if (!ob.enabled) continue;
                    if (wc.hadSolo && !ob.solo) continue;
                    oldSample = processFilterPath(
                        ob.useTpt, ob.tptStateL, ob.tptCoefficients,
                        ob.filtersL, ob.coeffs, ob.numStages, oldSample);
                    if (ob.vintage)
                        oldSample = applyVintageSaturation(oldSample);
                }

                // New chain: cascade through all new bands
                float newSample = input;
                for (int b = 0; b < localNumBands; ++b)
                {
                    const bool en = bandParams[b].enabled.load(std::memory_order_relaxed);
                    const bool ab = bandParams[b].audioBypass.load(std::memory_order_relaxed);
                    const bool sl = bandParams[b].solo.load(std::memory_order_relaxed);
                    if (!en || ab) continue;
                    if (hasSolo && !sl) continue;
                    auto& st = bandStates[b];
                    const bool pathValid = st.useTpt
                        ? st.tptCoefficients.isValid() : st.coefficients[0].valid;
                    if (!pathValid) continue;
                    newSample = processFilterPath(
                        st.useTpt, st.tptStateL, st.tptCoefficients,
                        st.filtersL, st.coefficients, st.numActiveStages, newSample);
                    if (st.appliedVintage)
                        newSample = applyVintageSaturation(newSample);
                }

                buffer.setSample(0, i, oldSample + (newSample - oldSample) * t);
            }

            // --- Right channel ---
            if (bufferChannels > 1)
            {
                const float input = buffer.getSample(1, i);

                float oldSample = input;
                for (int b = 0; b < wc.oldNumBands; ++b)
                {
                    auto& ob = wc.oldBands[b];
                    if (!ob.enabled) continue;
                    if (wc.hadSolo && !ob.solo) continue;
                    oldSample = processFilterPath(
                        ob.useTpt, ob.tptStateR, ob.tptCoefficients,
                        ob.filtersR, ob.coeffs, ob.numStages, oldSample);
                    if (ob.vintage)
                        oldSample = applyVintageSaturation(oldSample);
                }

                float newSample = input;
                for (int b = 0; b < localNumBands; ++b)
                {
                    const bool en = bandParams[b].enabled.load(std::memory_order_relaxed);
                    const bool ab = bandParams[b].audioBypass.load(std::memory_order_relaxed);
                    const bool sl = bandParams[b].solo.load(std::memory_order_relaxed);
                    if (!en || ab) continue;
                    if (hasSolo && !sl) continue;
                    auto& st = bandStates[b];
                    const bool pathValid = st.useTpt
                        ? st.tptCoefficients.isValid() : st.coefficients[0].valid;
                    if (!pathValid) continue;
                    newSample = processFilterPath(
                        st.useTpt, st.tptStateR, st.tptCoefficients,
                        st.filtersR, st.coefficients, st.numActiveStages, newSample);
                    if (st.appliedVintage)
                        newSample = applyVintageSaturation(newSample);
                }

                buffer.setSample(1, i, oldSample + (newSample - oldSample) * t);
            }

            if (wc.remaining > 0) --wc.remaining;
        }

        // Apply output gain and return (skip per-band processing below)
        const float gain = outputGain.load(std::memory_order_relaxed);
        if (std::abs(gain - 1.0f) > 0.0001f)
            buffer.applyGain(gain);
        return;
    }

    for (int bandIdx = 0; bandIdx < localNumBands; ++bandIdx)
    {
        const auto& params = bandParams[bandIdx];
        auto& state = bandStates[bandIdx];

        // Read band state atomically
        const bool enabled = params.enabled.load(std::memory_order_relaxed);
        const bool bypassed_for_dyn = params.audioBypass.load(std::memory_order_relaxed);
        const bool solo = params.solo.load(std::memory_order_relaxed);

        // Skip if not enabled or audio-bypassed (DynEQ handles this band)
        if (!enabled || bypassed_for_dyn)
            continue;
        if (hasSolo && !solo)
            continue;

        // Skip if no valid coefficients (check first stage)
        const bool pathValid = state.useTpt
            ? state.tptCoefficients.isValid() : state.coefficients[0].valid;
        if (!pathValid)
            continue;

        const int numStages = state.numActiveStages;
        auto& xfade = bandCrossfades[bandIdx];
        const bool crossfading = xfade.remaining > 0;

        const bool applyVintage = state.appliedVintage;

        if (crossfading)
        {
            // Per-sample crossfade: process same input through both old and new filters,
            // blend linearly from old → new. Both L and R advance in lockstep.
            const float invTotal = 1.0f / static_cast<float>(xfade.total);

            float* dataL = (bufferChannels > 0) ? buffer.getWritePointer(0) : nullptr;
            float* dataR = (bufferChannels > 1) ? buffer.getWritePointer(1) : nullptr;

            for (int i = 0; i < numSamples; ++i)
            {
                const float t = (xfade.remaining > 0)
                    ? (1.0f - static_cast<float>(xfade.remaining) * invTotal)
                    : 1.0f;

                // Left channel
                if (dataL != nullptr)
                {
                    const float input = dataL[i];

                    float newSample = processFilterPath(
                        state.useTpt, state.tptStateL, state.tptCoefficients,
                        state.filtersL, state.coefficients, numStages, input);
                    float oldSample = processFilterPath(
                        xfade.oldUseTpt, xfade.oldTptStateL, xfade.oldTptCoefficients,
                        xfade.oldFiltersL, xfade.oldCoeffs, xfade.oldNumStages, input);

                    if (applyVintage)
                        newSample = applyVintageSaturation(newSample);
                    if (xfade.oldVintage)
                        oldSample = applyVintageSaturation(oldSample);

                    dataL[i] = oldSample + (newSample - oldSample) * t;
                }

                // Right channel
                if (dataR != nullptr)
                {
                    const float input = dataR[i];

                    float newSample = processFilterPath(
                        state.useTpt, state.tptStateR, state.tptCoefficients,
                        state.filtersR, state.coefficients, numStages, input);
                    float oldSample = processFilterPath(
                        xfade.oldUseTpt, xfade.oldTptStateR, xfade.oldTptCoefficients,
                        xfade.oldFiltersR, xfade.oldCoeffs, xfade.oldNumStages, input);

                    if (applyVintage)
                        newSample = applyVintageSaturation(newSample);
                    if (xfade.oldVintage)
                        oldSample = applyVintageSaturation(oldSample);

                    dataR[i] = oldSample + (newSample - oldSample) * t;
                }

                // Per-sample decrement
                if (xfade.remaining > 0)
                {
                    --xfade.remaining;
                }
            }
        }
        else
        {
            // Standard processing (no crossfade active)
            if (bufferChannels > 0)
            {
                float* channelData = buffer.getWritePointer(0);
                if (applyVintage)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        float sample = processFilterPath(
                            state.useTpt, state.tptStateL, state.tptCoefficients,
                            state.filtersL, state.coefficients, numStages, channelData[i]);
                        channelData[i] = applyVintageSaturation(sample);
                    }
                }
                else
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        channelData[i] = processFilterPath(
                            state.useTpt, state.tptStateL, state.tptCoefficients,
                            state.filtersL, state.coefficients, numStages, channelData[i]);
                    }
                }
            }

            if (bufferChannels > 1)
            {
                float* channelData = buffer.getWritePointer(1);
                if (applyVintage)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        float sample = processFilterPath(
                            state.useTpt, state.tptStateR, state.tptCoefficients,
                            state.filtersR, state.coefficients, numStages, channelData[i]);
                        channelData[i] = applyVintageSaturation(sample);
                    }
                }
                else
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        channelData[i] = processFilterPath(
                            state.useTpt, state.tptStateR, state.tptCoefficients,
                            state.filtersR, state.coefficients, numStages, channelData[i]);
                    }
                }
            }
        }
    }
    
    // Apply output gain
    const float gain = outputGain.load(std::memory_order_relaxed);
    if (std::abs(gain - 1.0f) > 0.0001f)
    {
        buffer.applyGain(gain);
    }
    
    // NOTE: No manual denormal cleanup needed - ScopedNoDenormals handles this via DAZ/FTZ
}

//==============================================================================
float ParametricEQProcessor::processBiquadWithFault(
    BiquadState& state, float input, const BiquadCoeffs& coefficients) noexcept
{
    bool fault = false;
    const float output = state.processSample(input, coefficients, &fault);
    if (fault)
        numericalFaultCount.fetch_add(1, std::memory_order_relaxed);
    return output;
}

float ParametricEQProcessor::processFilterPath(
    bool useTpt,
    AIEQDSP::TptSvfState& tptState,
    const AIEQDSP::TptSvfCoefficients& tptCoefficients,
    std::array<BiquadState, BandProcessingState::maxFilterStages>& filters,
    const std::array<BiquadCoeffs, BandProcessingState::maxFilterStages>& coefficients,
    int numStages,
    float input) noexcept
{
    if (useTpt)
    {
        bool fault = false;
        const double output = tptState.processSample(
            static_cast<double>(input), tptCoefficients, &fault);
        if (fault)
            numericalFaultCount.fetch_add(1, std::memory_order_relaxed);
        return static_cast<float>(output);
    }

    float output = input;
    const int safeStages = juce::jlimit(
        0, BandProcessingState::maxFilterStages, numStages);
    for (int stage = 0; stage < safeStages; ++stage)
        output = processBiquadWithFault(
            filters[static_cast<size_t>(stage)], output,
            coefficients[static_cast<size_t>(stage)]);
    return output;
}

float ParametricEQProcessor::applyVintageSaturation(float input) noexcept
{
    bool fault = false;
    const float output = AIEQDSP::NumericSafety::vintageSaturate(input, fault);
    if (fault)
        numericalFaultCount.fetch_add(1, std::memory_order_relaxed);
    return output;
}

//==============================================================================
int ParametricEQProcessor::addBand(float freq, float gainDb, float q, int type)
{
    const int currentNum = numActiveBands.load(std::memory_order_acquire);
    
    if (currentNum >= getMaxBands())
        return -1;
    
    const int newIndex = currentNum;
    
    // Set parameters atomically
    auto& params = bandParams[newIndex];
    params.frequency.store(juce::jlimit(20.0f, 20000.0f, freq), std::memory_order_relaxed);
    params.gain.store(juce::jlimit(-24.0f, 24.0f, gainDb), std::memory_order_relaxed);
    params.q.store(juce::jlimit(0.1f, 18.0f, q), std::memory_order_relaxed);
    params.type.store(juce::jlimit(0, 8, type), std::memory_order_relaxed);
    params.solo.store(false, std::memory_order_relaxed);
    params.vintageMode.store(false, std::memory_order_relaxed);
    params.curveMode.store(
        static_cast<uint8_t>(CurveMode::Legacy), std::memory_order_relaxed);
    params.enabled.store(true, std::memory_order_relaxed);
    params.version.fetch_add(1, std::memory_order_release);  // Signal update
    
    // Prepare filter if we have valid sample rate
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    if (sr > 0.0 && isPrepared.load(std::memory_order_acquire))
    {
        auto& state = bandStates[newIndex];

        // Reset filter states (BiquadState is POD, no prepare needed)
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            state.filtersL[s].reset();
            state.filtersR[s].reset();
        }
        state.tptStateL.reset();
        state.tptStateR.reset();
        state.numActiveStages = 1;
        state.prepared = true;

        const auto design = makeFilterDesign(
            static_cast<FilterType>(type), freq, gainDb, q, 0, sr, CurveMode::Legacy);
        state.numActiveStages = design.numStages;
        state.coefficients = design.coefficients;
        state.tptCoefficients = design.tptCoefficients;
        state.useTpt = design.useTpt;
        state.appliedVintage = false;
        bandValidationFailures[static_cast<size_t>(newIndex)].store(
            static_cast<uint8_t>(design.failure), std::memory_order_release);
        state.lastVersion = params.version.load(std::memory_order_acquire);
    }
    
    // Increment band count (atomic)
    numActiveBands.store(newIndex + 1, std::memory_order_release);
    
    return newIndex;
}

void ParametricEQProcessor::removeBand(int index)
{
    const int currentNum = numActiveBands.load(std::memory_order_acquire);
    
    if (index < 0 || index >= currentNum)
        return;
    
    // Shift all bands after index down
    for (int i = index; i < currentNum - 1; ++i)
    {
        // Copy parameters from next band
        auto& dest = bandParams[i];
        const auto& src = bandParams[i + 1];
        
        dest.frequency.store(src.frequency.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.gain.store(src.gain.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.q.store(src.q.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.type.store(src.type.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.enabled.store(src.enabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.solo.store(src.solo.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.vintageMode.store(src.vintageMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.curveMode.store(src.curveMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.slope.store(src.slope.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.version.fetch_add(1, std::memory_order_release);
        
        // Copy filter state
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
            bandStates[i].coefficients[s] = bandStates[i + 1].coefficients[s];
        bandStates[i].tptCoefficients = bandStates[i + 1].tptCoefficients;
        bandStates[i].tptStateL = bandStates[i + 1].tptStateL;
        bandStates[i].tptStateR = bandStates[i + 1].tptStateR;
        bandStates[i].useTpt = bandStates[i + 1].useTpt;
        bandStates[i].appliedVintage = bandStates[i + 1].appliedVintage;
        bandStates[i].numActiveStages = bandStates[i + 1].numActiveStages;
        bandStates[i].lastVersion = 0;  // Force update
        bandValidationFailures[static_cast<size_t>(i)].store(
            bandValidationFailures[static_cast<size_t>(i + 1)].load(std::memory_order_acquire),
            std::memory_order_release);
    }
    
    // Disable the last band
    bandParams[currentNum - 1].enabled.store(false, std::memory_order_relaxed);
    bandValidationFailures[static_cast<size_t>(currentNum - 1)].store(
        static_cast<uint8_t>(BiquadValidationFailure::IntentionalBypass),
        std::memory_order_release);
    
    // Decrement band count
    numActiveBands.store(currentNum - 1, std::memory_order_release);
}

void ParametricEQProcessor::clearAllBands()
{
    const int currentNum = numActiveBands.load(std::memory_order_acquire);
    
    // Disable all bands
    for (int i = 0; i < currentNum; ++i)
    {
        bandParams[i].enabled.store(false, std::memory_order_relaxed);
        bandValidationFailures[static_cast<size_t>(i)].store(
            static_cast<uint8_t>(BiquadValidationFailure::IntentionalBypass),
            std::memory_order_release);
    }
    
    // Reset count
    numActiveBands.store(0, std::memory_order_release);
}

//==============================================================================
void ParametricEQProcessor::setBandFrequency(int index, float freq)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].frequency.store(juce::jlimit(20.0f, 20000.0f, freq), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandGain(int index, float gain)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].gain.store(juce::jlimit(-24.0f, 24.0f, gain), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandQ(int index, float q)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].q.store(juce::jlimit(0.1f, 18.0f, q), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandType(int index, int type)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].type.store(juce::jlimit(0, 8, type), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandEnabled(int index, bool enabled)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;

    bandParams[index].enabled.store(enabled, std::memory_order_relaxed);
}

void ParametricEQProcessor::setBandAudioBypass(int index, bool bypass)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;

    bandParams[index].audioBypass.store(bypass, std::memory_order_relaxed);
}

void ParametricEQProcessor::setBandSolo(int index, bool solo)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].solo.store(solo, std::memory_order_relaxed);
}

void ParametricEQProcessor::setBandVintageMode(int index, bool vintage)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    bandParams[index].vintageMode.store(vintage, std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandCurveMode(int index, CurveMode mode)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;

    const CurveMode safeMode = mode == CurveMode::Surgical
        ? CurveMode::Surgical : CurveMode::Legacy;
    bandParams[index].curveMode.store(
        static_cast<uint8_t>(safeMode), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandParameters(int index, float freq, float gain, float q, int type)
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return;
    
    auto& params = bandParams[index];
    params.frequency.store(juce::jlimit(20.0f, 20000.0f, freq), std::memory_order_relaxed);
    params.gain.store(juce::jlimit(-24.0f, 24.0f, gain), std::memory_order_relaxed);
    params.q.store(juce::jlimit(0.1f, 18.0f, q), std::memory_order_relaxed);
    params.type.store(juce::jlimit(0, 8, type), std::memory_order_relaxed);
    params.version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::setBandSlope(int index, int s)
{
    if (index < 0 || index >= getMaxBands()) return;
    bandParams[index].slope.store(juce::jlimit(0, 2, s), std::memory_order_relaxed);
    bandParams[index].version.fetch_add(1, std::memory_order_release);
}

void ParametricEQProcessor::clearBandFilterState(int index) noexcept
{
    if (index < 0 || index >= getMaxBands()) return;
    auto& state = bandStates[index];
    for (auto& f : state.filtersL) f.reset();
    for (auto& f : state.filtersR) f.reset();
    state.tptStateL.reset();
    state.tptStateR.reset();
}

void ParametricEQProcessor::beginBandCrossfade(int index, int fadeSamples) noexcept
{
    if (index < 0 || index >= getMaxBands()) return;

    // Message-thread-safe: enqueue command, actual setup runs on audio thread.
    CrossfadeCommand cmd;
    cmd.kind = CrossfadeCommand::PerBand;
    cmd.bandIndex = index;
    cmd.fadeSamples = fadeSamples;

    // Discard on overflow: advisory fade, not correctness-critical.
    // If the queue is full the audio thread is heavily loaded;
    // skipping a fade is preferable to blocking the UI.
    (void) crossfadeCommands.tryPush(cmd);
}

void ParametricEQProcessor::beginWholeChainCrossfade(int fadeSamples) noexcept
{
    CrossfadeCommand cmd;
    cmd.kind = CrossfadeCommand::WholeChain;
    cmd.bandIndex = 0; // unused
    cmd.fadeSamples = fadeSamples;

    (void) crossfadeCommands.tryPush(cmd);
}

// ============================================================================
// Audio-thread executors — formerly the bodies of beginBandCrossfade /
// beginWholeChainCrossfade. Called only from process() after draining the
// command queue. Safe to mutate bandStates[] / bandCrossfades[] /
// wholeChainXfade here because we are on the audio thread.
// ============================================================================

void ParametricEQProcessor::executePerBandCrossfadeSetup(int index, int fadeSamples) noexcept
{
    if (index < 0 || index >= getMaxBands()) return;

    auto& state = bandStates[index];
    auto& xfade = bandCrossfades[index];

    // Don't overwrite an active crossfade with a shorter one
    if (xfade.remaining > 0 && fadeSamples <= xfade.remaining)
        return;

    // Save current coefficients and filter state
    xfade.oldNumStages = state.numActiveStages;
    xfade.oldUseTpt = state.useTpt;
    xfade.oldVintage = state.appliedVintage;
    xfade.oldTptStateL = state.tptStateL;
    xfade.oldTptStateR = state.tptStateR;
    xfade.oldTptCoefficients = state.tptCoefficients;
    for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
    {
        xfade.oldCoeffs[s]   = state.coefficients[s];
        xfade.oldFiltersL[s] = state.filtersL[s];
        xfade.oldFiltersR[s] = state.filtersR[s];
    }

    // Reset live filter state so new coefficients start clean
    for (auto& f : state.filtersL) f.reset();
    for (auto& f : state.filtersR) f.reset();
    state.tptStateL.reset();
    state.tptStateR.reset();

    // Start crossfade
    xfade.remaining = fadeSamples;
    xfade.total     = fadeSamples;
}

void ParametricEQProcessor::executeWholeChainCrossfadeSetup(int fadeSamples) noexcept
{
    auto& wc = wholeChainXfade;

    // Don't overwrite an active crossfade with a shorter one
    if (wc.remaining > 0 && fadeSamples <= wc.remaining)
        return;

    const int localNumBands = numActiveBands.load(std::memory_order_acquire);
    wc.oldNumBands = localNumBands;

    // Snapshot all bands.
    // Note: we're on the audio thread now, so reading bandStates is safe.
    // We determine "was active" from bandStates (not bandParams) because
    // the message thread may have updated bandParams before issuing this
    // command (e.g. loadStateFromSlot); bandStates reflects what process()
    // was actually using last block.
    wc.hadSolo = false;
    for (int i = 0; i < localNumBands; ++i)
    {
        auto& ob = wc.oldBands[i];
        auto& state = bandStates[i];
        ob.numStages = state.numActiveStages;
        ob.useTpt = state.useTpt;
        ob.vintage = state.appliedVintage;
        ob.tptStateL = state.tptStateL;
        ob.tptStateR = state.tptStateR;
        ob.tptCoefficients = state.tptCoefficients;
        ob.enabled = state.useTpt
            ? state.tptCoefficients.isValid() : state.coefficients[0].valid;
        ob.solo = false;

        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            ob.coeffs[s]   = state.coefficients[s];
            ob.filtersL[s] = state.filtersL[s];
            ob.filtersR[s] = state.filtersR[s];
        }

        // Reset live filter state so new coefficients start clean
        for (auto& f : state.filtersL) f.reset();
        for (auto& f : state.filtersR) f.reset();
        state.tptStateL.reset();
        state.tptStateR.reset();
    }

    // Cancel any per-band crossfades (whole-chain supersedes)
    for (int i = 0; i < localNumBands; ++i)
        bandCrossfades[i].remaining = 0;

    wc.remaining = fadeSamples;
    wc.total     = fadeSamples;
}

//==============================================================================
float ParametricEQProcessor::getBandFrequency(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return 1000.0f;
    return bandParams[index].frequency.load(std::memory_order_relaxed);
}

float ParametricEQProcessor::getBandGain(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return 0.0f;
    return bandParams[index].gain.load(std::memory_order_relaxed);
}

float ParametricEQProcessor::getBandQ(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return 1.0f;
    return bandParams[index].q.load(std::memory_order_relaxed);
}

int ParametricEQProcessor::getBandType(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return static_cast<int>(Peak);
    return bandParams[index].type.load(std::memory_order_relaxed);
}

bool ParametricEQProcessor::isBandEnabled(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return false;
    return bandParams[index].enabled.load(std::memory_order_relaxed);
}

bool ParametricEQProcessor::isBandSolo(int index) const
{
    if (index < 0 || index >= numActiveBands.load(std::memory_order_acquire))
        return false;
    return bandParams[index].solo.load(std::memory_order_relaxed);
}

int ParametricEQProcessor::getBandSlope(int index) const
{
    if (index < 0 || index >= getMaxBands()) return 0;
    return bandParams[index].slope.load(std::memory_order_relaxed);
}

ParametricEQProcessor::CurveMode ParametricEQProcessor::getBandCurveMode(int index) const
{
    if (index < 0 || index >= getMaxBands())
        return CurveMode::Legacy;
    const auto mode = static_cast<CurveMode>(
        bandParams[index].curveMode.load(std::memory_order_relaxed));
    return mode == CurveMode::Surgical ? CurveMode::Surgical : CurveMode::Legacy;
}

ParametricEQProcessor::BandInfo ParametricEQProcessor::getBandInfo(int index) const
{
    BandInfo info;
    
    if (index >= 0 && index < numActiveBands.load(std::memory_order_acquire))
    {
        const auto& params = bandParams[index];
        info.frequency = params.frequency.load(std::memory_order_relaxed);
        info.gain = params.gain.load(std::memory_order_relaxed);
        info.q = params.q.load(std::memory_order_relaxed);
        info.type = params.type.load(std::memory_order_relaxed);
        info.enabled = params.enabled.load(std::memory_order_relaxed);
        info.solo = params.solo.load(std::memory_order_relaxed);
        info.vintageMode = params.vintageMode.load(std::memory_order_relaxed);
        const auto mode = static_cast<CurveMode>(
            params.curveMode.load(std::memory_order_relaxed));
        info.curveMode = mode == CurveMode::Surgical
            ? CurveMode::Surgical : CurveMode::Legacy;
    }
    
    return info;
}

//==============================================================================
float ParametricEQProcessor::getMagnitudeForFrequency(float freq, double sampleRate) const
{
    // GUI-thread safe: recompute coefficients locally from atomic band params
    // to avoid reading audio-thread-only bandStates (data race risk).
    if (sampleRate <= 0.0)
        sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    if (sampleRate <= 0.0)
        return outputGain.load(std::memory_order_relaxed);

    double magnitude = 1.0;

    const int numBands = numActiveBands.load(std::memory_order_acquire);

    for (int i = 0; i < numBands; ++i)
    {
        if (!bandParams[i].enabled.load(std::memory_order_relaxed))
            continue;

        const float bFreq = bandParams[i].frequency.load(std::memory_order_relaxed);
        const float bGain = bandParams[i].gain.load(std::memory_order_relaxed);
        const float bQ    = bandParams[i].q.load(std::memory_order_relaxed);
        const int   bType = bandParams[i].type.load(std::memory_order_relaxed);
        const int   bSlope = bandParams[i].slope.load(std::memory_order_relaxed);
        const auto curveMode = static_cast<CurveMode>(
            bandParams[i].curveMode.load(std::memory_order_relaxed));

        const auto design = makeFilterDesign(
            static_cast<FilterType>(bType), bFreq, bGain, bQ, bSlope,
            sampleRate, curveMode);
        magnitude *= getFilterDesignMagnitude(
            design, static_cast<double>(freq), sampleRate);
    }

    return static_cast<float>(magnitude) * outputGain.load(std::memory_order_relaxed);
}

void ParametricEQProcessor::getMagnitudeForFrequencyArray(const float* frequencies, 
                                                          float* magnitudes,
                                                          size_t numPoints,
                                                          double sampleRate) const
{
    if (sampleRate <= 0.0)
        sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    if (sampleRate <= 0.0)
    {
        for (size_t i = 0; i < numPoints; ++i)
            magnitudes[i] = outputGain.load(std::memory_order_relaxed);
        return;
    }

    // Initialize to unity
    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] = 1.0f;

    const int numBands = numActiveBands.load(std::memory_order_acquire);

    // Multiply by each band's response (recomputed locally for thread-safety)
    for (int bandIdx = 0; bandIdx < numBands; ++bandIdx)
    {
        if (!bandParams[bandIdx].enabled.load(std::memory_order_relaxed))
            continue;

        const float bFreq = bandParams[bandIdx].frequency.load(std::memory_order_relaxed);
        const float bGain = bandParams[bandIdx].gain.load(std::memory_order_relaxed);
        const float bQ    = bandParams[bandIdx].q.load(std::memory_order_relaxed);
        const int   bType = bandParams[bandIdx].type.load(std::memory_order_relaxed);
        const int   bSlope = bandParams[bandIdx].slope.load(std::memory_order_relaxed);
        const auto curveMode = static_cast<CurveMode>(
            bandParams[bandIdx].curveMode.load(std::memory_order_relaxed));

        const auto design = makeFilterDesign(
            static_cast<FilterType>(bType), bFreq, bGain, bQ, bSlope,
            sampleRate, curveMode);

        for (size_t i = 0; i < numPoints; ++i)
        {
            magnitudes[i] *= static_cast<float>(getFilterDesignMagnitude(
                design, static_cast<double>(frequencies[i]), sampleRate));
        }
    }

    // Apply output gain
    const float gain = outputGain.load(std::memory_order_relaxed);
    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] *= gain;
}

//==============================================================================
void ParametricEQProcessor::getMagnitudeForFrequencyArrayWithGainOffsets(
    const float* frequencies, float* magnitudes, size_t numPoints,
    double sampleRate, const float* gainOffsets, int numOffsets) const
{
    if (sampleRate <= 0.0)
        sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    if (sampleRate <= 0.0)
    {
        for (size_t i = 0; i < numPoints; ++i)
            magnitudes[i] = outputGain.load(std::memory_order_relaxed);
        return;
    }

    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] = 1.0f;

    const int numBands = numActiveBands.load(std::memory_order_acquire);

    for (int bandIdx = 0; bandIdx < numBands; ++bandIdx)
    {
        if (!bandParams[bandIdx].enabled.load(std::memory_order_relaxed))
            continue;

        const float bFreq  = bandParams[bandIdx].frequency.load(std::memory_order_relaxed);
        const float bGainBase = bandParams[bandIdx].gain.load(std::memory_order_relaxed);
        const float bQ     = bandParams[bandIdx].q.load(std::memory_order_relaxed);
        const int   bType  = bandParams[bandIdx].type.load(std::memory_order_relaxed);
        const int   bSlope = bandParams[bandIdx].slope.load(std::memory_order_relaxed);
        const auto curveMode = static_cast<CurveMode>(
            bandParams[bandIdx].curveMode.load(std::memory_order_relaxed));

        // Apply per-band gain offset (gain reduction from dynamic EQ)
        const float offset = (gainOffsets && bandIdx < numOffsets) ? gainOffsets[bandIdx] : 0.0f;
        const float bGain  = bGainBase + offset;

        const auto design = makeFilterDesign(
            static_cast<FilterType>(bType), bFreq, bGain, bQ, bSlope,
            sampleRate, curveMode);

        for (size_t i = 0; i < numPoints; ++i)
        {
            magnitudes[i] *= static_cast<float>(getFilterDesignMagnitude(
                design, static_cast<double>(frequencies[i]), sampleRate));
        }
    }

    const float gain = outputGain.load(std::memory_order_relaxed);
    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] *= gain;
}

//==============================================================================
void ParametricEQProcessor::updateCoefficientsForBand(int index)
{
    if (index < 0 || index >= getMaxBands())
        return;
    
    const auto& params = bandParams[index];
    auto& state = bandStates[index];
    
    const float freq = params.frequency.load(std::memory_order_relaxed);
    const float gain = params.gain.load(std::memory_order_relaxed);
    const float q = params.q.load(std::memory_order_relaxed);
    const int type = params.type.load(std::memory_order_relaxed);
    const int slope = params.slope.load(std::memory_order_relaxed);
    const auto curveMode = static_cast<CurveMode>(
        params.curveMode.load(std::memory_order_relaxed));
    const bool vintage = params.vintageMode.load(std::memory_order_relaxed);
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    
    const auto design = makeFilterDesign(
        static_cast<FilterType>(type), freq, gain, q, slope, sr, curveMode);
    state.numActiveStages = design.numStages;
    state.coefficients = design.coefficients;
    state.tptCoefficients = design.tptCoefficients;
    state.useTpt = design.useTpt;
    state.appliedVintage = !design.useTpt && vintage
        && (type == static_cast<int>(VintageLowShelf)
            || type == static_cast<int>(VintageHighShelf));
    bandValidationFailures[static_cast<size_t>(index)].store(
        static_cast<uint8_t>(design.failure), std::memory_order_release);
    
    state.lastVersion = params.version.load(std::memory_order_acquire);
}

ParametricEQProcessor::FilterDesign ParametricEQProcessor::makeFilterDesign(
    FilterType type, float freq, float gain, float q, int slope,
    double sampleRate, CurveMode curveMode) const
{
    FilterDesign result;

    const bool useSurgicalTpt = curveMode == CurveMode::Surgical
        && (type == LowShelf || type == Peak || type == HighShelf);
    if (useSurgicalTpt)
    {
        AIEQDSP::TptSvfType tptType = AIEQDSP::TptSvfType::Bell;
        if (type == LowShelf)
            tptType = AIEQDSP::TptSvfType::LowShelf;
        else if (type == HighShelf)
            tptType = AIEQDSP::TptSvfType::HighShelf;

        result.tptCoefficients = AIEQDSP::TptSvfCoefficients::design(
            tptType, sampleRate, static_cast<double>(freq),
            static_cast<double>(gain), static_cast<double>(q));
        result.useTpt = result.tptCoefficients.isValid();
        result.numStages = result.useTpt ? 1 : 0;
        result.failure = result.useTpt
            ? BiquadValidationFailure::None
            : BiquadValidationFailure::InvalidDomain;
        return result;
    }

    if (type == LowCut || type == HighCut)
    {
        const auto precision = highPrecisionMode.load(std::memory_order_acquire)
            ? BiquadPrecision::HighPrecision : BiquadPrecision::Automatic;
        const auto cut = CutFilterDesigner::design(
            type == LowCut, slope, sampleRate, freq, q, precision);
        if (cut.numStages <= 0)
        {
            result.failure = cut.failure;
            return result;
        }
        result.coefficients = cut.coefficients;
        result.numStages = juce::jlimit(0,
            BandProcessingState::maxFilterStages, cut.numStages);
        result.failure = BiquadValidationFailure::None;
        return result;
    }

    result.coefficients[0] = makeCoefficients(type, freq, gain, q, sampleRate);
    result.numStages = result.coefficients[0].valid ? 1 : 0;
    result.failure = result.coefficients[0].failure;
    return result;
}

double ParametricEQProcessor::getFilterDesignMagnitude(
    const FilterDesign& design, double frequency, double sampleRate) noexcept
{
    if (design.useTpt)
    {
        const double magnitude = design.tptCoefficients.getMagnitudeForFrequency(
            frequency, sampleRate);
        return std::isfinite(magnitude) ? magnitude : 1.0;
    }

    double magnitude = 1.0;
    for (int stage = 0; stage < design.numStages; ++stage)
    {
        magnitude *= design.coefficients[static_cast<size_t>(stage)]
            .getMagnitudeForFrequency(frequency, sampleRate);
    }
    return magnitude;
}

BiquadCoeffs ParametricEQProcessor::makeCoefficients(
    FilterType type, float freq, float gain, float q, double sampleRate) const
{
    if (!std::isfinite(sampleRate) || sampleRate < kMinProcessingSampleRate
        || sampleRate > BiquadCoeffs::maxProcessingSampleRate())
        return BiquadCoeffs::makeRejected(BiquadValidationFailure::InvalidDomain);

    const auto precision = highPrecisionMode.load(std::memory_order_acquire)
        ? BiquadPrecision::HighPrecision : BiquadPrecision::Automatic;

    // Clamp frequency safely below Nyquist
    freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.499), freq);

    // Clamp Q to a sane range (avoid degenerate filters)
    q = juce::jlimit(0.1f, 40.0f, q);

    // Zero-allocation coefficient computation — BiquadCoeffs is a POD struct on the stack
    switch (type)
    {
        case LowCut:
            return BiquadCoeffs::makeHighPass(sampleRate, freq, q, precision);

        case LowShelf:
            return BiquadCoeffs::makeLowShelf(
                sampleRate, freq, q, juce::Decibels::decibelsToGain(gain), precision);

        case Peak:
            // Gain near zero: return bypass so the caller skips processing entirely.
            if (std::abs(gain) < 0.05f)
                return BiquadCoeffs::makeBypass();
            return BiquadCoeffs::makePeakFilter(
                sampleRate, freq, q, juce::Decibels::decibelsToGain(gain), precision);

        case HighShelf:
            return BiquadCoeffs::makeHighShelf(
                sampleRate, freq, q, juce::Decibels::decibelsToGain(gain), precision);

        case HighCut:
            return BiquadCoeffs::makeLowPass(sampleRate, freq, q, precision);

        case Notch:
            return BiquadCoeffs::makeNotch(sampleRate, freq, q, precision);

        case BandPass:
            return BiquadCoeffs::makeBandPass(sampleRate, freq, q, precision);

        case VintageLowShelf:
        {
            float vintageQ = juce::jlimit(0.3f, 1.0f, q * 0.6f);
            return BiquadCoeffs::makeLowShelf(
                sampleRate, freq, vintageQ, juce::Decibels::decibelsToGain(gain), precision);
        }

        case VintageHighShelf:
        {
            float vintageQ = juce::jlimit(0.3f, 1.0f, q * 0.6f);
            return BiquadCoeffs::makeHighShelf(
                sampleRate, freq, vintageQ, juce::Decibels::decibelsToGain(gain), precision);
        }

        default:
            return BiquadCoeffs::makeAllPass(sampleRate, 20.0f, 0.1f, precision);
    }
}
