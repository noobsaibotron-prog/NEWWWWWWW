#include "DynamicEQProcessor.h"

//==============================================================================
DynamicEQProcessor::DynamicEQProcessor()
{
    // Initialize all bands with defaults
    for (int i = 0; i < maxBands; ++i)
    {
        bandParams[i] = DynamicBandParams();
        bandStates[i] = BandState();
        attackCoeffs[i] = 0.0f;
        releaseCoeffs[i] = 0.0f;
    }
    
    // Set default frequencies spread across spectrum
    const float defaultFreqs[maxBands] = {50.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 6000.0f, 12000.0f};
    for (int i = 0; i < maxBands; ++i)
    {
        bandParams[i].frequency = defaultFreqs[i];
    }
}

//==============================================================================
void DynamicEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    numChannels = channels;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;
    
    // Prepare all band filters
    for (int i = 0; i < maxBands; ++i)
    {
        auto& state = bandStates[i];
        
        for (auto& filter : state.eqFiltersL)
            filter.prepare(spec);
        for (auto& filter : state.eqFiltersR)
            filter.prepare(spec);
        state.scFilterL.prepare(spec);
        state.scFilterR.prepare(spec);
        
        state.needsUpdate = true;
        
        // Calculate attack/release coefficients
        const auto& params = bandParams[i];
        float attackSamples = (params.attackMs / 1000.0f) * static_cast<float>(sampleRate);
        float releaseSamples = (params.releaseMs / 1000.0f) * static_cast<float>(sampleRate);
        
        attackCoeffs[i] = attackSamples > 0 ? std::exp(-1.0f / attackSamples) : 0.0f;
        releaseCoeffs[i] = releaseSamples > 0 ? std::exp(-1.0f / releaseSamples) : 0.0f;
    }
    
    // Prepare lookahead buffer
    lookaheadSamples = static_cast<int>((lookaheadMs / 1000.0f) * sampleRate);
    if (lookaheadSamples > 0)
    {
        lookaheadBuffer.setSize(channels, lookaheadSamples + samplesPerBlock);
        lookaheadBuffer.clear();
    }
    lookaheadWritePos = 0;
    
    // Update all coefficients
    for (int i = 0; i < maxBands; ++i)
        updateBandCoefficients(i);
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
void DynamicEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin(buffer.getNumChannels(), 2);
    
    if (numSamples == 0 || channels == 0)
        return;
    
    // Create dry buffer for mix
    juce::AudioBuffer<float> dryBuffer;
    if (globalMix < 0.999f)
    {
        dryBuffer.makeCopyOf(buffer);
    }
    
    // Process each band
    for (int bandIdx = 0; bandIdx < maxBands; ++bandIdx)
    {
        std::lock_guard<std::mutex> lock(paramsMutex);
        
        const auto& params = bandParams[bandIdx];
        auto& state = bandStates[bandIdx];
        
        if (!params.enabled)
            continue;
        
        // Update coefficients if needed
        if (state.needsUpdate)
        {
            updateBandCoefficients(bandIdx);
            state.needsUpdate = false;
        }
        
        // Skip if no valid coefficients
        if (state.eqCoeffs == nullptr)
            continue;
        
        //----------------------------------------------------------------------
        // DYNAMIC PROCESSING
        //----------------------------------------------------------------------
        if (params.dynamicMode != DynamicMode::Off)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Get input samples
                float inL = buffer.getSample(0, sample);
                float inR = channels > 1 ? buffer.getSample(1, sample) : inL;
                
                // Sidechain filtering (optional)
                float scL = inL, scR = inR;
                if (params.sidechainEnabled && state.scCoeffs != nullptr)
                {
                    scL = state.scFilterL.processSample(inL);
                    scR = state.scFilterR.processSample(inR);
                }
                else
                {
                    // Use band-filtered signal for detection
                    scL = state.eqFiltersL[1].processSample(inL);
                    scR = channels > 1 ? state.eqFiltersR[1].processSample(inR) : scL;
                }
                
                // Calculate input level (mono sum for detection)
                float inputLevel = 0.0f;
                if (params.detection == DetectionMode::Peak)
                {
                    inputLevel = std::max(std::abs(scL), std::abs(scR));
                }
                else // RMS approximation
                {
                    inputLevel = std::sqrt((scL * scL + scR * scR) * 0.5f);
                }
                
                // Update envelope follower
                float inputDb = inputLevel > 1e-10f ? 
                    juce::Decibels::gainToDecibels(inputLevel) : -100.0f;
                
                // Smooth envelope with attack/release
                float targetEnv = inputDb;
                float currentEnv = (state.envelopeL + state.envelopeR) * 0.5f;
                
                float coeff = targetEnv > currentEnv ? attackCoeffs[bandIdx] : releaseCoeffs[bandIdx];
                float smoothedEnv = coeff * currentEnv + (1.0f - coeff) * targetEnv;
                
                state.envelopeL = smoothedEnv;
                state.envelopeR = smoothedEnv;
                
                // Calculate dynamic gain
                float dynamicGainDb = calculateDynamicGain(smoothedEnv, params);
                
                // Smooth gain changes
                float gainCoeff = dynamicGainDb < state.currentGain ? 
                    attackCoeffs[bandIdx] : releaseCoeffs[bandIdx];
                state.currentGain = gainCoeff * state.currentGain + (1.0f - gainCoeff) * dynamicGainDb;
                
                // Apply static gain + dynamic gain
                float totalGainDb = params.gain + state.currentGain;
                float linearGain = juce::Decibels::decibelsToGain(totalGainDb);
                
                // Update metering
                state.meter.inputLevel = smoothedEnv;
                state.meter.gainReduction = state.currentGain;
                
                // Apply EQ with dynamic gain
                // For dynamic EQ, we need to recalculate coefficients based on current gain
                // Simplified: apply gain after filtering
                float outL = state.eqFiltersL[0].processSample(inL);
                float outR = channels > 1 ? state.eqFiltersR[0].processSample(inR) : outL;
                
                // Scale by dynamic gain ratio
                float dynamicScale = juce::Decibels::decibelsToGain(state.currentGain);
                
                // Blend between unprocessed and processed based on dynamic gain
                // When dynamicGain is 0, use static EQ; when negative, reduce effect
                if (params.dynamicMode == DynamicMode::Compress)
                {
                    // Compression: reduce the EQ effect when above threshold
                    float eqAmount = 1.0f - (std::abs(state.currentGain) / params.range);
                    eqAmount = juce::jlimit(0.0f, 1.0f, eqAmount);
                    
                    outL = inL + (outL - inL) * eqAmount;
                    outR = inR + (outR - inR) * eqAmount;
                }
                else if (params.dynamicMode == DynamicMode::Expand)
                {
                    // Expansion: increase the EQ effect when above threshold
                    float eqAmount = 1.0f + (state.currentGain / params.range);
                    eqAmount = juce::jlimit(0.0f, 2.0f, eqAmount);
                    
                    outL = inL + (outL - inL) * eqAmount;
                    outR = inR + (outR - inR) * eqAmount;
                }
                else if (params.dynamicMode == DynamicMode::Gate)
                {
                    // Gate: cut when below threshold
                    if (smoothedEnv < params.threshold)
                    {
                        float gateAmount = juce::jmap(smoothedEnv, 
                            params.threshold - params.range, params.threshold, 
                            0.0f, 1.0f);
                        gateAmount = juce::jlimit(0.0f, 1.0f, gateAmount);
                        
                        outL *= gateAmount;
                        outR *= gateAmount;
                    }
                }
                
                buffer.setSample(0, sample, outL);
                if (channels > 1)
                    buffer.setSample(1, sample, outR);
                
                state.meter.outputLevel = juce::Decibels::gainToDecibels(
                    std::max(std::abs(outL), std::abs(outR)));
            }
        }
        else
        {
            //------------------------------------------------------------------
            // STATIC EQ PROCESSING (traditional)
            //------------------------------------------------------------------
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float inL = buffer.getSample(0, sample);
                float outL = state.eqFiltersL[0].processSample(inL);
                buffer.setSample(0, sample, outL);
                
                if (channels > 1)
                {
                    float inR = buffer.getSample(1, sample);
                    float outR = state.eqFiltersR[0].processSample(inR);
                    buffer.setSample(1, sample, outR);
                }
            }
            
            state.meter.gainReduction = 0.0f;
        }
    }
    
    // Apply global mix (dry/wet)
    if (globalMix < 0.999f && dryBuffer.getNumSamples() > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float* wet = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            
            for (int s = 0; s < numSamples; ++s)
            {
                wet[s] = dry[s] * (1.0f - globalMix) + wet[s] * globalMix;
            }
        }
    }
    
    // Auto makeup gain
    if (autoMakeupEnabled)
    {
        float totalGR = getTotalGainReduction();
        if (totalGR < -0.1f)
        {
            float makeupGain = juce::Decibels::decibelsToGain(-totalGR * 0.5f);
            buffer.applyGain(makeupGain);
        }
    }
}

//==============================================================================
float DynamicEQProcessor::calculateDynamicGain(float inputLevelDb, const DynamicBandParams& params) const
{
    if (params.dynamicMode == DynamicMode::Off)
        return 0.0f;
    
    float threshold = params.threshold;
    float ratio = params.ratio;
    float knee = params.knee;
    float range = params.range;
    
    float gainDb = 0.0f;
    
    if (params.dynamicMode == DynamicMode::Compress)
    {
        // Compression: reduce gain when above threshold
        if (inputLevelDb > threshold - knee * 0.5f)
        {
            gainDb = computeSoftKnee(inputLevelDb, threshold, ratio, knee);
            gainDb = juce::jlimit(-range, 0.0f, gainDb);
        }
    }
    else if (params.dynamicMode == DynamicMode::Expand)
    {
        // Upward expansion: increase gain when above threshold
        if (inputLevelDb > threshold - knee * 0.5f)
        {
            float excess = inputLevelDb - threshold;
            if (knee > 0.0f && excess < knee * 0.5f)
            {
                // Soft knee region
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
    else if (params.dynamicMode == DynamicMode::Gate)
    {
        // Gate: reduce gain when below threshold
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
        // Hard knee
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
    
    // Soft knee region (quadratic interpolation)
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
    
    std::lock_guard<std::mutex> lock(paramsMutex);
    bandParams[bandIndex] = params;
    bandStates[bandIndex].needsUpdate = true;
    
    // Recalculate attack/release coefficients
    float attackSamples = (params.attackMs / 1000.0f) * static_cast<float>(currentSampleRate);
    float releaseSamples = (params.releaseMs / 1000.0f) * static_cast<float>(currentSampleRate);
    
    attackCoeffs[bandIndex] = attackSamples > 0 ? std::exp(-1.0f / attackSamples) : 0.0f;
    releaseCoeffs[bandIndex] = releaseSamples > 0 ? std::exp(-1.0f / releaseSamples) : 0.0f;
}

DynamicEQProcessor::DynamicBandParams DynamicEQProcessor::getBandParams(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return DynamicBandParams();
    
    std::lock_guard<std::mutex> lock(paramsMutex);
    return bandParams[bandIndex];
}

void DynamicEQProcessor::setBandEnabled(int bandIndex, bool enabled)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    std::lock_guard<std::mutex> lock(paramsMutex);
    bandParams[bandIndex].enabled = enabled;
}

void DynamicEQProcessor::setDynamicMode(int bandIndex, DynamicMode mode)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    std::lock_guard<std::mutex> lock(paramsMutex);
    bandParams[bandIndex].dynamicMode = mode;
}

//==============================================================================
DynamicEQProcessor::BandMeter DynamicEQProcessor::getBandMeter(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return BandMeter();
    
    return bandStates[bandIndex].meter;
}

float DynamicEQProcessor::getTotalGainReduction() const
{
    float totalGR = 0.0f;
    for (int i = 0; i < maxBands; ++i)
    {
        if (bandParams[i].enabled && bandParams[i].dynamicMode != DynamicMode::Off)
        {
            totalGR += bandStates[i].meter.gainReduction;
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
    
    // Create EQ coefficients
    state.eqCoeffs = makeEQCoefficients(params.filterType, params.frequency, params.gain, params.q);
    
    if (state.eqCoeffs != nullptr)
    {
        for (auto& filter : state.eqFiltersL)
            *filter.coefficients = *state.eqCoeffs;
        for (auto& filter : state.eqFiltersR)
            *filter.coefficients = *state.eqCoeffs;
    }
    
    // Create sidechain filter coefficients (bandpass around detection frequency)
    if (params.sidechainEnabled)
    {
        state.scCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(
            currentSampleRate, params.sidechainFreq, params.sidechainQ);
        
        if (state.scCoeffs != nullptr)
        {
            *state.scFilterL.coefficients = *state.scCoeffs;
            *state.scFilterR.coefficients = *state.scCoeffs;
        }
    }
}

juce::dsp::IIR::Coefficients<float>::Ptr DynamicEQProcessor::makeEQCoefficients(
    int filterType, float freq, float gain, float q) const
{
    freq = juce::jlimit(20.0f, static_cast<float>(currentSampleRate * 0.49), freq);
    
    switch (filterType)
    {
        case 0: // LowCut
            return juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, freq, q);
        case 1: // LowShelf
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
        case 2: // Peak
            if (std::abs(gain) < 0.01f)
                return juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, freq, q);
            return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
        case 3: // HighShelf
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                currentSampleRate, freq, q, juce::Decibels::decibelsToGain(gain));
        case 4: // HighCut
            return juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, freq, q);
        default:
            return juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, freq, q);
    }
}

float DynamicEQProcessor::getMagnitudeForFrequency(float freq, double sampleRate) const
{
    std::lock_guard<std::mutex> lock(paramsMutex);
    
    double magnitude = 1.0;
    
    for (int i = 0; i < maxBands; ++i)
    {
        if (!bandParams[i].enabled || bandStates[i].eqCoeffs == nullptr)
            continue;
        
        magnitude *= bandStates[i].eqCoeffs->getMagnitudeForFrequency(freq, sampleRate);
    }
    
    return static_cast<float>(magnitude);
}

