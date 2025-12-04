#include "ParametricEQProcessor.h"
#include <cmath>

//==============================================================================
ParametricEQProcessor::ParametricEQProcessor()
{
    // Reserve space for max bands
    bands.reserve(static_cast<size_t>(getMaxBands()));
}

//==============================================================================
void ParametricEQProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    numChannels = channels;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(channels);
    
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    for (auto& band : bands)
    {
        band.filterL.prepare(spec);
        band.filterR.prepare(spec);
        band.needsUpdate = true;
    }
    
    updateAllCoefficients();
}

void ParametricEQProcessor::reset()
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    for (auto& band : bands)
    {
        band.filterL.reset();
        band.filterR.reset();
    }
}

void ParametricEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (bypassed)
        return;
    
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    // Check for solo'd bands
    bool hasSolo = false;
    for (const auto& band : bands)
    {
        if (band.solo && band.enabled)
        {
            hasSolo = true;
            break;
        }
    }
    
    // Process each band
    for (auto& band : bands)
    {
        // Skip if not enabled, or if other bands are solo'd and this one isn't
        if (!band.enabled)
            continue;
        if (hasSolo && !band.solo)
            continue;
        
        // Update coefficients if needed
        if (band.needsUpdate)
        {
            if (band.coefficients != nullptr)
            {
                *band.filterL.coefficients = *band.coefficients;
                *band.filterR.coefficients = *band.coefficients;
            }
            band.needsUpdate = false;
        }
        
        // Process left channel
        if (buffer.getNumChannels() > 0)
        {
            auto* channelData = buffer.getWritePointer(0);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                channelData[i] = band.filterL.processSample(channelData[i]);
            }
        }
        
        // Process right channel
        if (buffer.getNumChannels() > 1)
        {
            auto* channelData = buffer.getWritePointer(1);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                channelData[i] = band.filterR.processSample(channelData[i]);
            }
        }
    }
    
    // Apply output gain
    if (std::abs(outputGain - 1.0f) > 0.0001f)
    {
        buffer.applyGain(outputGain);
    }
    
    // Safety: clip extreme values to prevent blowup and denormals
    constexpr float maxSampleValue = 10.0f;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s)
        {
            float sample = channelData[s];
            // Check for invalid values first
            if (std::isnan(sample) || std::isinf(sample))
            {
                channelData[s] = 0.0f;
            }
            else
            {
                // Clamp to safe range and handle denormals
                sample = juce::jlimit(-maxSampleValue, maxSampleValue, sample);
                // Flush denormals to zero (prevents performance issues)
                if (std::abs(sample) < 1e-30f)
                    sample = 0.0f;
                channelData[s] = sample;
            }
        }
    }
}

//==============================================================================
int ParametricEQProcessor::addBand(float freq, float gain, float q, int type)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (bands.size() >= static_cast<size_t>(getMaxBands()))
        return -1;
    
    Band newBand;
    newBand.frequency = juce::jlimit(20.0f, 20000.0f, freq);
    newBand.gain = juce::jlimit(-24.0f, 24.0f, gain);
    newBand.q = juce::jlimit(0.1f, 18.0f, q);
    newBand.type = static_cast<FilterType>(juce::jlimit(0, 6, type));
    newBand.enabled = true;
    newBand.solo = false;
    newBand.needsUpdate = true;
    
    // Prepare filter
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(currentBlockSize);
    spec.numChannels = 1;
    
    newBand.filterL.prepare(spec);
    newBand.filterR.prepare(spec);
    
    // Create coefficients
    newBand.coefficients = makeCoefficients(newBand.type, newBand.frequency, 
                                            newBand.gain, newBand.q, currentSampleRate);
    if (newBand.coefficients != nullptr)
    {
        *newBand.filterL.coefficients = *newBand.coefficients;
        *newBand.filterR.coefficients = *newBand.coefficients;
    }
    
    bands.emplace_back(std::move(newBand));
    
    return static_cast<int>(bands.size()) - 1;
}

void ParametricEQProcessor::removeBand(int index)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands.erase(bands.begin() + index);
    }
}

void ParametricEQProcessor::clearAllBands()
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    bands.clear();
}

//==============================================================================
void ParametricEQProcessor::setBandFrequency(int index, float freq)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].frequency = juce::jlimit(20.0f, 20000.0f, freq);
        updateCoefficients(index);
    }
}

void ParametricEQProcessor::setBandGain(int index, float gain)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].gain = juce::jlimit(-24.0f, 24.0f, gain);
        updateCoefficients(index);
    }
}

void ParametricEQProcessor::setBandQ(int index, float q)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].q = juce::jlimit(0.1f, 18.0f, q);
        updateCoefficients(index);
    }
}

void ParametricEQProcessor::setBandType(int index, int type)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].type = static_cast<FilterType>(juce::jlimit(0, 6, type));
        updateCoefficients(index);
    }
}

void ParametricEQProcessor::setBandEnabled(int index, bool enabled)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].enabled = enabled;
    }
}

void ParametricEQProcessor::setBandSolo(int index, bool solo)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        bands[static_cast<size_t>(index)].solo = solo;
    }
}

void ParametricEQProcessor::setBandParameters(int index, float freq, float gain, float q, int type)
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    if (index >= 0 && index < static_cast<int>(bands.size()))
    {
        auto& band = bands[static_cast<size_t>(index)];
        band.frequency = juce::jlimit(20.0f, 20000.0f, freq);
        band.gain = juce::jlimit(-24.0f, 24.0f, gain);
        band.q = juce::jlimit(0.1f, 18.0f, q);
        band.type = static_cast<FilterType>(juce::jlimit(0, 6, type));
        updateCoefficients(index);
    }
}

//==============================================================================
float ParametricEQProcessor::getBandFrequency(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)].frequency;
    return 1000.0f;
}

float ParametricEQProcessor::getBandGain(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)].gain;
    return 0.0f;
}

float ParametricEQProcessor::getBandQ(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)].q;
    return 1.0f;
}

int ParametricEQProcessor::getBandType(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return static_cast<int>(bands[static_cast<size_t>(index)].type);
    return static_cast<int>(Peak);
}

bool ParametricEQProcessor::isBandEnabled(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)].enabled;
    return false;
}

bool ParametricEQProcessor::isBandSolo(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)].solo;
    return false;
}

const ParametricEQProcessor::Band& ParametricEQProcessor::getBand(int index) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    if (index >= 0 && index < static_cast<int>(bands.size()))
        return bands[static_cast<size_t>(index)];
    return dummyBand;
}

//==============================================================================
float ParametricEQProcessor::getMagnitudeForFrequency(float freq, double sampleRate) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    double magnitude = 1.0;
    
    for (const auto& band : bands)
    {
        if (!band.enabled || band.coefficients == nullptr)
            continue;
        
        magnitude *= band.coefficients->getMagnitudeForFrequency(static_cast<double>(freq), sampleRate);
    }
    
    return static_cast<float>(magnitude) * outputGain;
}

void ParametricEQProcessor::getMagnitudeForFrequencyArray(const float* frequencies, 
                                                          float* magnitudes,
                                                          size_t numPoints,
                                                          double sampleRate) const
{
    std::lock_guard<std::mutex> lock(bandsMutex);
    
    // Initialize to unity
    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] = 1.0f;
    
    // Multiply by each band's response
    for (const auto& band : bands)
    {
        if (!band.enabled || band.coefficients == nullptr)
            continue;
        
        for (size_t i = 0; i < numPoints; ++i)
        {
            double mag = band.coefficients->getMagnitudeForFrequency(
                static_cast<double>(frequencies[i]), sampleRate);
            magnitudes[i] *= static_cast<float>(mag);
        }
    }
    
    // Apply output gain
    for (size_t i = 0; i < numPoints; ++i)
        magnitudes[i] *= outputGain;
}

//==============================================================================
void ParametricEQProcessor::updateCoefficients(int index)
{
    // Note: caller must hold bandsMutex
    if (index < 0 || index >= static_cast<int>(bands.size()))
        return;
    
    auto& band = bands[static_cast<size_t>(index)];
    
    band.coefficients = makeCoefficients(band.type, band.frequency, 
                                         band.gain, band.q, currentSampleRate);
    band.needsUpdate = true;
}

void ParametricEQProcessor::updateAllCoefficients()
{
    // Note: caller must hold bandsMutex
    for (int i = 0; i < static_cast<int>(bands.size()); ++i)
    {
        updateCoefficients(i);
    }
}

juce::dsp::IIR::Coefficients<float>::Ptr ParametricEQProcessor::makeCoefficients(
    FilterType type, float freq, float gain, float q, double sampleRate) const
{
    // Clamp frequency to valid range
    freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49), freq);
    
    switch (type)
    {
        case LowCut:
            return juce::dsp::IIR::Coefficients<float>::makeHighPass(
                sampleRate, static_cast<double>(freq), static_cast<double>(q));
            
        case LowShelf:
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                sampleRate, static_cast<double>(freq), static_cast<double>(q),
                juce::Decibels::decibelsToGain(static_cast<double>(gain)));
            
        case Peak:
            if (std::abs(gain) < 0.01f)
            {
                // Return unity gain coefficients for near-zero gain
                return juce::dsp::IIR::Coefficients<float>::makeAllPass(
                    sampleRate, static_cast<double>(freq), static_cast<double>(q));
            }
            return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, static_cast<double>(freq), static_cast<double>(q),
                juce::Decibels::decibelsToGain(static_cast<double>(gain)));
            
        case HighShelf:
            return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sampleRate, static_cast<double>(freq), static_cast<double>(q),
                juce::Decibels::decibelsToGain(static_cast<double>(gain)));
            
        case HighCut:
            return juce::dsp::IIR::Coefficients<float>::makeLowPass(
                sampleRate, static_cast<double>(freq), static_cast<double>(q));
            
        case Notch:
            return juce::dsp::IIR::Coefficients<float>::makeNotch(
                sampleRate, static_cast<double>(freq), static_cast<double>(q));
            
        case BandPass:
            return juce::dsp::IIR::Coefficients<float>::makeBandPass(
                sampleRate, static_cast<double>(freq), static_cast<double>(q));
            
        default:
            return juce::dsp::IIR::Coefficients<float>::makeAllPass(
                sampleRate, static_cast<double>(freq), static_cast<double>(q));
    }
}
