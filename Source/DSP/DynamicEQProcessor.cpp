#include "DynamicEQProcessor.h"

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
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    numChannels = channels;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    // Pre-allocate dry buffer
    dryBuffer.setSize(channels, samplesPerBlock * 4, false, false, true);
    dryBuffer.clear();
    
    // Prepare all band filters and calculate coefficients
    constexpr double smoothingSeconds = 0.02; // 20 ms ramp for dynamic params
    for (int i = 0; i < maxBands; ++i)
    {
        auto& state = bandStates[i];
        
        for (auto& filter : state.eqFiltersL)
            filter.prepare(spec);
        for (auto& filter : state.eqFiltersR)
            filter.prepare(spec);
        state.scFilterL.prepare(spec);
        state.scFilterR.prepare(spec);
        
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
    }
    
    // Prepare lookahead buffer
    const float laMsVal = lookaheadMs.load(std::memory_order_relaxed);
    const int laSamples = static_cast<int>((laMsVal / 1000.0f) * sampleRate);
    lookaheadSamples.store(laSamples, std::memory_order_relaxed);
    
    if (laSamples > 0)
    {
        lookaheadBuffer.setSize(channels, laSamples + samplesPerBlock);
        lookaheadBuffer.clear();
    }
    lookaheadWritePos = 0;
    
    isPrepared.store(true, std::memory_order_release);
}

void DynamicEQProcessor::reset()
{
    for (auto& state : bandStates)
    {
        for (auto& filter : state.eqFiltersL)
            filter.reset();
        for (auto& filter : state.eqFiltersR)
            filter.reset();
        state.scFilterL.reset();
        state.scFilterR.reset();
        
        state.envelopeL = 0.0f;
        state.envelopeR = 0.0f;
        state.currentGain = 0.0f;
        state.targetGain = 0.0f;
    }
    
    if (lookaheadBuffer.getNumSamples() > 0)
        lookaheadBuffer.clear();
    lookaheadWritePos = 0;
}

//==============================================================================
void DynamicEQProcessor::setLookahead(float ms)
{
    lookaheadMs.store(juce::jlimit(0.0f, 20.0f, ms), std::memory_order_relaxed);
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

    // Bug G fix: if host delivers a larger block than pre-allocated (e.g. Reaper dynamic block),
    // grow dryBuffer now. This is a safe path: it only allocates when strictly necessary,
    // and the call is on the audio thread which is acceptable for a one-time resize.
    if (mix < 0.999f && numSamples > dryBuffer.getNumSamples())
        dryBuffer.setSize(channels, numSamples * 2, false, false, true); // *2 to amortize future grows

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
            state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
            state.meterInputLevel.store(-100.0f, std::memory_order_relaxed);
            state.meterOutputLevel.store(-100.0f, std::memory_order_relaxed);
            continue;
        }
        
        // Check if coefficients need update
        const uint64_t currentVersion = params.version.load(std::memory_order_acquire);
        if (currentVersion != state.lastVersion)
        {
            updateBandCoefficients(bandIdx);
            updateAttackReleaseCoeffs(bandIdx);
            state.lastVersion = currentVersion;

            // FIX: When parameters change (threshold/ratio/range tweaked), immediately
            // recalculate and store the GR that corresponds to the current envelope level.
            // Without this, the meter freezes until the next audio block triggers the
            // per-sample loop — which never updates if envelope is below threshold or
            // if dynamic mode is off.
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
                state.currentGain = freshGR; // snap audio-smoothed gain too
            }
        }
        
        // Sidechain smoothing at control rate: advance to target, update coeffs if changed
        smoothedSidechainFreq[bandIdx].skip(numSamples);
        smoothedSidechainQ[bandIdx].skip(numSamples);
        const float scFreqNow = smoothedSidechainFreq[bandIdx].getCurrentValue();
        const float scQNow = smoothedSidechainQ[bandIdx].getCurrentValue();
        if (params.sidechainEnabled.load(std::memory_order_relaxed) && state.scCoeffs != nullptr)
        {
            constexpr float scEps = 1e-3f;
            if (std::abs(scFreqNow - state.scFreqApplied) > scEps
                || std::abs(scQNow - state.scQApplied) > scEps)
            {
                const double sr = currentSampleRate.load(std::memory_order_relaxed);
                auto newCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, scFreqNow, scQNow);
                if (newCoeffs != nullptr)
                {
                    state.scCoeffs = newCoeffs;
                    *state.scFilterL.coefficients = *state.scCoeffs;
                    *state.scFilterR.coefficients = *state.scCoeffs;
                    state.scFreqApplied = scFreqNow;
                    state.scQApplied = scQNow;
                }
            }
        }
        
        // Skip if no valid coefficients
        if (state.eqCoeffs == nullptr)
            continue;
        
        // Read dynamic mode
        const int dynMode = params.dynamicMode.load(std::memory_order_relaxed);
        
        //----------------------------------------------------------------------
        // DYNAMIC PROCESSING
        //----------------------------------------------------------------------
        if (dynMode != DynamicMode_Off)
        {
            const int detMode = params.detection.load(std::memory_order_relaxed);
            const bool scEnabled = params.sidechainEnabled.load(std::memory_order_relaxed);
            const float attackCoeff = state.attackCoeff;
            const float releaseCoeff = state.releaseCoeff;
            
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
                if (scEnabled && state.scCoeffs != nullptr)
                {
                    scL = state.scFilterL.processSample(detectL);
                    scR = state.scFilterR.processSample(detectR);
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
                
                // Apply EQ
                float outL = state.eqFiltersL[0].processSample(inL);
                float outR = channels > 1 ? state.eqFiltersR[0].processSample(inR) : outL;
                
                // Apply dynamic behavior
                if (dynMode == DynamicMode_Compress)
                {
                    float eqAmount = 1.0f - (std::abs(state.currentGain) / range);
                    eqAmount = juce::jlimit(0.0f, 1.0f, eqAmount);
                    outL = inL + (outL - inL) * eqAmount;
                    outR = inR + (outR - inR) * eqAmount;
                }
                else if (dynMode == DynamicMode_Expand)
                {
                    float eqAmount = 1.0f + (state.currentGain / range);
                    eqAmount = juce::jlimit(0.0f, 2.0f, eqAmount);
                    outL = inL + (outL - inL) * eqAmount;
                    outR = inR + (outR - inR) * eqAmount;
                }
                else if (dynMode == DynamicMode_Gate)
                {
                    if (smoothedEnv < threshold)
                    {
                        float gateAmount = juce::jmap(smoothedEnv, 
                            threshold - range, threshold, 0.0f, 1.0f);
                        gateAmount = juce::jlimit(0.0f, 1.0f, gateAmount);
                        outL *= gateAmount;
                        outR *= gateAmount;
                    }
                }
                
                outLPtr[sample] = outL;
                if (channels > 1)
                    outRPtr[sample] = outR;
                
                state.meterOutputLevel.store(
                    juce::Decibels::gainToDecibels(std::max(std::abs(outL), std::abs(outR))),
                    std::memory_order_relaxed);
            }
        }
        else
        {
            //------------------------------------------------------------------
            // STATIC EQ PROCESSING
            //------------------------------------------------------------------
            float* outLPtr = buffer.getWritePointer(0);
            float* outRPtr = channels > 1 ? buffer.getWritePointer(1) : nullptr;
            
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float outL = state.eqFiltersL[0].processSample(outLPtr[sample]);
                outLPtr[sample] = outL;
                
                if (channels > 1)
                {
                    float outR = state.eqFiltersR[0].processSample(outRPtr[sample]);
                    outRPtr[sample] = outR;
                }
            }
            
            state.meterGainReduction.store(0.0f, std::memory_order_relaxed);
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
        float totalGainLinear = 1.0f;
        for (int i = 0; i < maxBands; ++i)
        {
            if (bandParams[i].enabled.load(std::memory_order_relaxed) &&
                bandParams[i].dynamicMode.load(std::memory_order_relaxed) != DynamicMode_Off)
            {
                const float grDb = bandStates[i].meterGainReduction.load(std::memory_order_relaxed);
                totalGainLinear *= juce::Decibels::decibelsToGain(grDb);
            }
        }
        
        if (totalGainLinear < 0.999f)
        {
            // Safety clamp to avoid extreme boosts
            const float makeupGain = juce::jlimit(0.25f, 4.0f, 1.0f / totalGainLinear);
            buffer.applyGain(makeupGain);
        }
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
    
    state.eqCoeffs = makeEQCoefficients(filterType, freq, gain, q);
    
    if (state.eqCoeffs != nullptr)
    {
        for (auto& filter : state.eqFiltersL)
            *filter.coefficients = *state.eqCoeffs;
        for (auto& filter : state.eqFiltersR)
            *filter.coefficients = *state.eqCoeffs;
    }
    
    // Update sidechain filter if enabled
    if (params.sidechainEnabled.load(std::memory_order_relaxed))
    {
        const float scFreq = params.sidechainFreq.load(std::memory_order_relaxed);
        const float scQ = params.sidechainQ.load(std::memory_order_relaxed);
        const double sr = currentSampleRate.load(std::memory_order_relaxed);
        
        state.scCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, scFreq, scQ);
        
        if (state.scCoeffs != nullptr)
        {
            *state.scFilterL.coefficients = *state.scCoeffs;
            *state.scFilterR.coefficients = *state.scCoeffs;
            state.scFreqApplied = scFreq;
            state.scQApplied = scQ;
        }
    }
    
    // Update smoothed targets for dynamic parameters
    smoothedThresholds[bandIndex].setTargetValue(params.threshold.load(std::memory_order_relaxed));
    smoothedRatios[bandIndex].setTargetValue(params.ratio.load(std::memory_order_relaxed));
    smoothedRanges[bandIndex].setTargetValue(params.range.load(std::memory_order_relaxed));
    smoothedKnees[bandIndex].setTargetValue(params.knee.load(std::memory_order_relaxed));
    smoothedSidechainFreq[bandIndex].setTargetValue(params.sidechainFreq.load(std::memory_order_relaxed));
    smoothedSidechainQ[bandIndex].setTargetValue(params.sidechainQ.load(std::memory_order_relaxed));
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

juce::dsp::IIR::Coefficients<float>::Ptr DynamicEQProcessor::makeEQCoefficients(
    int filterType, float freq, float gain, float q) const
{
    const double sr = currentSampleRate.load(std::memory_order_relaxed);

    if (sr <= 0.0)
        return nullptr;

    freq = juce::jlimit(20.0f, static_cast<float>(sr * 0.499), freq);
    q    = juce::jlimit(0.1f, 40.0f, q);

    switch (filterType)
    {
        case 0: // LowCut
            return juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, freq, q);
        case 1: // LowShelf
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 2: // Peak
            if (std::abs(gain) < 0.05f)
                return juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 20.0, 0.1);
            return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 3: // HighShelf
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));
        case 4: // HighCut
            return juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, freq, q);
        case 5: // Notch
            return juce::dsp::IIR::Coefficients<float>::makeNotch(sr, freq, q);
        case 6: // BandPass
            return juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freq, q);
        default:
            return juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 20.0, 0.1);
    }
}

float DynamicEQProcessor::getMagnitudeForFrequency(float freq, double sampleRate) const
{
    double magnitude = 1.0;
    
    for (int i = 0; i < maxBands; ++i)
    {
        if (!bandParams[i].enabled.load(std::memory_order_relaxed))
            continue;
        
        const auto& state = bandStates[i];
        if (state.eqCoeffs == nullptr)
            continue;
        
        magnitude *= state.eqCoeffs->getMagnitudeForFrequency(freq, sampleRate);
    }
    
    return static_cast<float>(magnitude);
}
