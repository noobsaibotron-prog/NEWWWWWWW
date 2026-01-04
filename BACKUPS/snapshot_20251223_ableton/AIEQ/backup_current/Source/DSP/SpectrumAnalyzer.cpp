#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    rebuildFFT(fftOrder);
    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    juce::SpinLock::ScopedLockType lock(dataLock);
    currentSampleRate = sampleRate;

    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(rawSpectrum.begin(), rawSpectrum.end(), minDecibels);
    std::fill(smoothedSpectrum.begin(), smoothedSpectrum.end(), minDecibels);
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), minDecibels);

    fifoIndex = 0;
    fftDataReady = false;

    float updateRate = 60.0f;
    peakDecayRate = 100.0f / (peakHoldDecayTime * updateRate);

    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::reset()
{
    juce::SpinLock::ScopedLockType lock(dataLock);
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(rawSpectrum.begin(), rawSpectrum.end(), minDecibels);
    std::fill(smoothedSpectrum.begin(), smoothedSpectrum.end(), minDecibels);
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), minDecibels);

    fifoIndex = 0;
    fftDataReady = false;
}

void SpectrumAnalyzer::resetPeakHold()
{
    std::fill(peakHoldSpectrum.begin(), peakHoldSpectrum.end(), minDecibels);
}

void SpectrumAnalyzer::process(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    juce::SpinLock::ScopedTryLockType lock(dataLock);
    if (!lock.isLocked())
        return;

    const float* channelData = buffer.getReadPointer(0);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = channelData[i];

        if (std::isnan(sample) || std::isinf(sample))
        {
            sample = 0.0f;
        }
        else if (std::abs(sample) < 1e-30f)
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
    if (window)
        window->multiplyWithWindowingTable(fftData.data(), fftSize);
    
    // Perform FFT
    if (fft)
        fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    // Convert to dB, smooth, and update peak hold
    for (int i = 0; i < numBins; ++i)
    {
        // Normalize FFT magnitude: convert to linear gain for positive frequency bins.
        const float normalization = 2.0f / static_cast<float>(fftSize);
        float magnitude = fftData[i] * normalization;
        
        // Convert to dB
        float dB = magnitude > 0.0f ? 
            juce::Decibels::gainToDecibels(magnitude, minDecibels) : minDecibels;
        
        // Clamp
        dB = juce::jlimit(minDecibels, maxDecibels, dB);
        
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
                if (peakHoldSpectrum[i] < minDecibels)
                    peakHoldSpectrum[i] = minDecibels;
            }
        }
    }
    
    fftDataReady = true;
}

void SpectrumAnalyzer::updateSmoothingCoeffs()
{
    // Calculate smoothing coefficients based on time constants (~60 Hz update)
    constexpr float updateRate = 60.0f;
    attackCoeff = 1.0f - std::exp(-1.0f / (attackTimeMs * 0.001f * updateRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTimeMs * 0.001f * updateRate));
}

void SpectrumAnalyzer::setSpeed(Speed s)
{
    speedMode = s;
    switch (s)
    {
        case Speed::Fast:
            attackTimeMs = 3.0f;
            releaseTimeMs = 40.0f;
            break;
        case Speed::Slow:
            attackTimeMs = 15.0f;
            releaseTimeMs = 180.0f;
            break;
        case Speed::Medium:
        default:
            attackTimeMs = 7.0f;
            releaseTimeMs = 90.0f;
            break;
    }
    updateSmoothingCoeffs();
}

void SpectrumAnalyzer::setFFTResolution(Resolution res)
{
    int newOrder = static_cast<int>(res);
    if (newOrder == fftOrder)
        return;

    juce::SpinLock::ScopedLockType lock(dataLock);
    resolution = res;
    rebuildFFT(newOrder);
}

void SpectrumAnalyzer::rebuildFFT(int newOrder)
{
    fftOrder = juce::jlimit(9, 14, newOrder); // safety
    fftSize = 1 << fftOrder;
    numBins = fftSize / 2 + 1;

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(static_cast<size_t>(fftSize),
                                                                   juce::dsp::WindowingFunction<float>::hann);

    fifoBuffer.assign(fftSize, 0.0f);
    fftData.assign(fftSize * 2, 0.0f);
    rawSpectrum.assign(numBins, minDecibels);
    smoothedSpectrum.assign(numBins, minDecibels);
    peakHoldSpectrum.assign(numBins, minDecibels);
    fifoIndex = 0;
    fftDataReady = false;
}

float SpectrumAnalyzer::getMagnitudeForFrequency(float frequency) const
{
    int bin = getBinForFrequency(frequency);
    if (bin >= 0 && bin < numBins)
        return smoothedSpectrum[bin];
    return minDecibels;
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
