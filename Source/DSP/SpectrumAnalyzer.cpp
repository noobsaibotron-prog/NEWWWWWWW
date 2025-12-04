#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    fifoBuffer.resize(fftSize, 0.0f);
    fftData.resize(fftSize * 2, 0.0f);
    rawSpectrum.resize(numBins, -100.0f);
    smoothedSpectrum.resize(numBins, -100.0f);
    peakHoldSpectrum.resize(numBins, -100.0f);
    
    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(rawSpectrum.begin(), rawSpectrum.end(), -100.0f);
    std::fill(smoothedSpectrum.begin(), smoothedSpectrum.end(), -100.0f);
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), -100.0f);
    
    fifoIndex = 0;
    fftDataReady = false;
    
    // Calculate peak decay rate (dB per FFT frame)
    float updateRate = 60.0f;
    peakDecayRate = 100.0f / (peakHoldDecayTime * updateRate); // Total dB / (time * fps)
    
    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::reset()
{
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(rawSpectrum.begin(), rawSpectrum.end(), -100.0f);
    std::fill(smoothedSpectrum.begin(), smoothedSpectrum.end(), -100.0f);
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), -100.0f);
    
    fifoIndex = 0;
    fftDataReady = false;
}

void SpectrumAnalyzer::resetPeakHold()
{
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), -100.0f);
}

void SpectrumAnalyzer::process(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;
    
    const float* channelData = buffer.getReadPointer(0);
    const int numSamples = buffer.getNumSamples();
    
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = channelData[i];
        
        // Safety check: handle invalid values and denormals
        if (std::isnan(sample) || std::isinf(sample))
        {
            sample = 0.0f;
        }
        else if (std::abs(sample) < 1e-30f)  // Flush denormals
        {
            sample = 0.0f;
        }
        
        fifoBuffer[fifoIndex] = sample;
        ++fifoIndex;
        
        if (fifoIndex >= fftSize)
        {
            fifoIndex = 0;
            processFFT();
        }
    }
}

void SpectrumAnalyzer::processFFT()
{
    // Copy FIFO to FFT buffer
    std::copy(fifoBuffer.begin(), fifoBuffer.end(), fftData.begin());
    
    // Apply window
    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    
    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    
    // Convert to dB, smooth, and update peak hold
    for (int i = 0; i < numBins; ++i)
    {
        float magnitude = fftData[i];
        
        // Normalize
        magnitude /= static_cast<float>(fftSize);
        
        // Convert to dB
        float dB = magnitude > 0.0f ? 
            juce::Decibels::gainToDecibels(magnitude, -100.0f) : -100.0f;
        
        // Clamp
        dB = juce::jlimit(-100.0f, 6.0f, dB);
        
        // Store raw
        rawSpectrum[i] = dB;
        
        // Smooth
        float coeff = (dB > smoothedSpectrum[i]) ? attackCoeff : releaseCoeff;
        smoothedSpectrum[i] = smoothedSpectrum[i] + coeff * (dB - smoothedSpectrum[i]);
        
        // Peak Hold
        if (peakHoldEnabled)
        {
            if (dB > peakHoldSpectrum[i])
            {
                // New peak
                peakHoldSpectrum[i] = dB;
            }
            else
            {
                // Decay peak
                peakHoldSpectrum[i] -= peakDecayRate;
                if (peakHoldSpectrum[i] < -100.0f)
                    peakHoldSpectrum[i] = -100.0f;
            }
        }
    }
    
    fftDataReady = true;
}

void SpectrumAnalyzer::updateSmoothingCoeffs()
{
    // Calculate smoothing coefficients based on time constants
    // Assuming ~60Hz update rate
    float updateRate = 60.0f;
    
    attackCoeff = 1.0f - std::exp(-1.0f / (attackTimeMs * 0.001f * updateRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTimeMs * 0.001f * updateRate));
}

float SpectrumAnalyzer::getMagnitudeForFrequency(float frequency) const
{
    int bin = getBinForFrequency(frequency);
    if (bin >= 0 && bin < numBins)
        return smoothedSpectrum[bin];
    return -100.0f;
}

int SpectrumAnalyzer::getBinForFrequency(float frequency) const
{
    int bin = static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    return juce::jlimit(0, numBins - 1, bin);
}

float SpectrumAnalyzer::getFrequencyForBin(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
}
