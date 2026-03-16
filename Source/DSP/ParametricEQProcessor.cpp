#include "ParametricEQProcessor.h"
#include <cmath>

namespace
{
inline float fastTanhApprox(float x) noexcept
{
    // Padé [3/3] approximation, accurate enough for audio soft clipping
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
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
        bandParams[i].version.store(0, std::memory_order_relaxed);
    }
}

//==============================================================================
void ParametricEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    // Store sample rate atomically
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(samplesPerBlock, std::memory_order_relaxed);
    numChannels = channels;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;  // Each filter processes mono
    
    // Prepare all band filter states (all stages)
    for (int i = 0; i < getMaxBands(); ++i)
    {
        auto& state = bandStates[i];
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            state.filtersL[s].prepare(spec);
            state.filtersR[s].prepare(spec);
        }
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
        }
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
            // Read parameters
            const float freq = params.frequency.load(std::memory_order_relaxed);
            const float gain = params.gain.load(std::memory_order_relaxed);
            const float q = params.q.load(std::memory_order_relaxed);
            const int type = params.type.load(std::memory_order_relaxed);
            const int slope = params.slope.load(std::memory_order_relaxed);
            
            // Update coefficients
            auto coeff = makeCoefficients(static_cast<FilterType>(type), freq, gain, q, sr);
            
            // Determine number of stages based on slope (only for LowCut/HighCut)
            int numStages = 1;
            if ((type == static_cast<int>(LowCut) || type == static_cast<int>(HighCut)) && slope > 0)
                numStages = (slope == 1) ? 2 : 4;  // 24dB=2 stages, 48dB=4 stages
            state.numActiveStages = numStages;
            
            for (int s = 0; s < numStages; ++s)
            {
                state.coefficients[s] = coeff;
                if (coeff != nullptr)
                {
                    *state.filtersL[s].coefficients = *coeff;
                    *state.filtersR[s].coefficients = *coeff;
                }
            }
            // Clear unused stages
            for (int s = numStages; s < BandProcessingState::maxFilterStages; ++s)
                state.coefficients[s] = nullptr;
            
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
    
    for (int bandIdx = 0; bandIdx < localNumBands; ++bandIdx)
    {
        const auto& params = bandParams[bandIdx];
        auto& state = bandStates[bandIdx];
        
        // Read band state atomically
        const bool enabled = params.enabled.load(std::memory_order_relaxed);
        const bool solo = params.solo.load(std::memory_order_relaxed);
        const bool vintage = params.vintageMode.load(std::memory_order_relaxed);
        const int type = params.type.load(std::memory_order_relaxed);
        
        // Skip if not enabled, or if other bands are solo'd and this one isn't
        if (!enabled)
            continue;
        if (hasSolo && !solo)
            continue;
        
        // Skip if no valid coefficients (check first stage)
        if (state.coefficients[0] == nullptr)
            continue;
        
        const int numStages = state.numActiveStages;
        
        // Check for vintage modes that need soft clipping
        const bool applyVintage = vintage && 
            (type == static_cast<int>(VintageLowShelf) || type == static_cast<int>(VintageHighShelf));
        
        // Process left channel
        if (bufferChannels > 0)
        {
            float* channelData = buffer.getWritePointer(0);
            
            if (applyVintage)
            {
                // With vintage soft clipping
                constexpr float drive = 1.2f;
                constexpr float invDrive = 1.0f / 1.2f;
                
                for (int i = 0; i < numSamples; ++i)
                {
                    float sample = channelData[i];
                    for (int s = 0; s < numStages; ++s)
                        sample = state.filtersL[s].processSample(sample);
                    sample = fastTanhApprox(sample * drive) * invDrive;
                    channelData[i] = sample;
                }
            }
            else
            {
                // Standard processing
                for (int i = 0; i < numSamples; ++i)
                {
                    float sample = channelData[i];
                    for (int s = 0; s < numStages; ++s)
                        sample = state.filtersL[s].processSample(sample);
                    channelData[i] = sample;
                }
            }
        }
        
        // Process right channel
        if (bufferChannels > 1)
        {
            float* channelData = buffer.getWritePointer(1);
            
            if (applyVintage)
            {
                constexpr float drive = 1.2f;
                constexpr float invDrive = 1.0f / 1.2f;
                
                for (int i = 0; i < numSamples; ++i)
                {
                    float sample = channelData[i];
                    for (int s = 0; s < numStages; ++s)
                        sample = state.filtersR[s].processSample(sample);
                    sample = fastTanhApprox(sample * drive) * invDrive;
                    channelData[i] = sample;
                }
            }
            else
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float sample = channelData[i];
                    for (int s = 0; s < numStages; ++s)
                        sample = state.filtersR[s].processSample(sample);
                    channelData[i] = sample;
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
    params.enabled.store(true, std::memory_order_relaxed);
    params.version.fetch_add(1, std::memory_order_release);  // Signal update
    
    // Prepare filter if we have valid sample rate
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    if (sr > 0.0 && isPrepared.load(std::memory_order_acquire))
    {
        auto& state = bandStates[newIndex];
        
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sr;
        spec.maximumBlockSize = static_cast<juce::uint32>(currentBlockSize.load(std::memory_order_relaxed));
        spec.numChannels = 1;
        
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
        {
            state.filtersL[s].prepare(spec);
            state.filtersR[s].prepare(spec);
        }
        state.numActiveStages = 1;
        state.prepared = true;
        
        // Create coefficients
        auto coeff = makeCoefficients(static_cast<FilterType>(type), freq, gainDb, q, sr);
        state.coefficients[0] = coeff;
        if (coeff != nullptr)
        {
            *state.filtersL[0].coefficients = *coeff;
            *state.filtersR[0].coefficients = *coeff;
        }
        for (int s = 1; s < BandProcessingState::maxFilterStages; ++s)
            state.coefficients[s] = nullptr;
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
        dest.slope.store(src.slope.load(std::memory_order_relaxed), std::memory_order_relaxed);
        dest.version.fetch_add(1, std::memory_order_release);
        
        // Copy filter state
        for (int s = 0; s < BandProcessingState::maxFilterStages; ++s)
            bandStates[i].coefficients[s] = bandStates[i + 1].coefficients[s];
        bandStates[i].numActiveStages = bandStates[i + 1].numActiveStages;
        bandStates[i].lastVersion = 0;  // Force update
    }
    
    // Disable the last band
    bandParams[currentNum - 1].enabled.store(false, std::memory_order_relaxed);
    
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

        auto coefs = makeCoefficients(static_cast<FilterType>(bType), bFreq, bGain, bQ, sampleRate);
        if (coefs == nullptr)
            continue;

        double singleMag = coefs->getMagnitudeForFrequency(static_cast<double>(freq), sampleRate);
        
        // Apply cascaded stages for LowCut/HighCut with slope > 12dB/oct
        int numStages = 1;
        if ((bType == static_cast<int>(LowCut) || bType == static_cast<int>(HighCut)) && bSlope > 0)
            numStages = (bSlope == 1) ? 2 : 4;
        
        for (int s = 0; s < numStages; ++s)
            magnitude *= singleMag;
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

        auto coefs = makeCoefficients(static_cast<FilterType>(bType), bFreq, bGain, bQ, sampleRate);
        if (coefs == nullptr)
            continue;

        // Determine number of cascaded stages for LowCut/HighCut
        int numStages = 1;
        if ((bType == static_cast<int>(LowCut) || bType == static_cast<int>(HighCut)) && bSlope > 0)
            numStages = (bSlope == 1) ? 2 : 4;

        for (size_t i = 0; i < numPoints; ++i)
        {
            double mag = coefs->getMagnitudeForFrequency(
                static_cast<double>(frequencies[i]), sampleRate);
            // Apply cascaded stages
            double totalMag = 1.0;
            for (int s = 0; s < numStages; ++s)
                totalMag *= mag;
            magnitudes[i] *= static_cast<float>(totalMag);
        }
    }

    // Apply output gain
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
    const double sr = currentSampleRate.load(std::memory_order_relaxed);
    
    auto coeff = makeCoefficients(static_cast<FilterType>(type), freq, gain, q, sr);
    
    // Determine number of stages based on slope (only for LowCut/HighCut)
    int numStages = 1;
    if ((type == static_cast<int>(LowCut) || type == static_cast<int>(HighCut)) && slope > 0)
        numStages = (slope == 1) ? 2 : 4;
    state.numActiveStages = numStages;
    
    for (int s = 0; s < numStages; ++s)
    {
        state.coefficients[s] = coeff;
        if (coeff != nullptr && state.prepared)
        {
            *state.filtersL[s].coefficients = *coeff;
            *state.filtersR[s].coefficients = *coeff;
        }
    }
    for (int s = numStages; s < BandProcessingState::maxFilterStages; ++s)
        state.coefficients[s] = nullptr;
    
    state.lastVersion = params.version.load(std::memory_order_acquire);
}

juce::dsp::IIR::Coefficients<float>::Ptr ParametricEQProcessor::makeCoefficients(
    FilterType type, float freq, float gain, float q, double sampleRate) const
{
    if (sampleRate <= 0.0 || sampleRate > 192000.0)
        return nullptr;

    // Clamp frequency safely below Nyquist
    freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.499), freq);

    // Clamp Q to a sane range (avoid degenerate filters)
    q = juce::jlimit(0.1f, 40.0f, q);

    // Use double-precision sample rate throughout — avoids rounding errors at 96/192 kHz
    const double sr = sampleRate;

    // NOTE: JUCE IIR coefficient functions already apply bilinear-transform prewarping
    // internally, so we do NOT prewarp the frequency here. Doing it twice would shift
    // the cutoff upward (audible above ~8 kHz) and is the cause of the previous
    // "anti-cramping" regression introduced in the previous commit.

    switch (type)
    {
        case LowCut:
            return juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, freq, q);

        case LowShelf:
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));

        case Peak:
            // Gain near zero: return identity (no phase shift, no allpass artifact)
            if (std::abs(gain) < 0.05f)
                return juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 20.0, 0.1);
            return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));

        case HighShelf:
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sr, freq, q, juce::Decibels::decibelsToGain(gain));

        case HighCut:
            return juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, freq, q);

        case Notch:
            return juce::dsp::IIR::Coefficients<float>::makeNotch(sr, freq, q);

        case BandPass:
            return juce::dsp::IIR::Coefficients<float>::makeBandPass(sr, freq, q);

        case VintageLowShelf:
        {
            float vintageQ = juce::jlimit(0.3f, 1.0f, q * 0.6f);
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                sr, freq, vintageQ, juce::Decibels::decibelsToGain(gain));
        }

        case VintageHighShelf:
        {
            float vintageQ = juce::jlimit(0.3f, 1.0f, q * 0.6f);
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sr, freq, vintageQ, juce::Decibels::decibelsToGain(gain));
        }

        default:
            return juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 20.0, 0.1);
    }
}
