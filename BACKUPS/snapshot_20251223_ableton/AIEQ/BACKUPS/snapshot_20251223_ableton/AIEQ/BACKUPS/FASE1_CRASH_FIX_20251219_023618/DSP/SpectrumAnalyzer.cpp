#include "SpectrumAnalyzer.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer()
{
    // Pre-allocate buffers
    fifoBuffer.resize(static_cast<size_t>(fifo.getTotalSize()), 0.0f);
    fftData.resize(static_cast<size_t>(fftSize * 2), 0.0f);
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(static_cast<size_t>(fftSize),
                                                                   juce::dsp::WindowingFunction<float>::hann);
    
    for (int i = 0; i < 2; ++i)
    {
        spectrumBuffers[i].resize(static_cast<size_t>(numBins), 0.0f);
        spectrumDBBuffers[i].resize(static_cast<size_t>(numBins), minDecibels);
        peakHoldBuffers[i].resize(static_cast<size_t>(numBins), minDecibels);
    }
    
    updateSmoothingCoeffs();
    updateDecayFactor(30.0f); // assume ~30 Hz GUI timer default
}

void SpectrumAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo.reset();
    std::fill(fifoBuffer.begin(), fifoBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    
    for (int i = 0; i < 2; ++i)
    {
        std::fill(spectrumBuffers[i].begin(), spectrumBuffers[i].end(), 0.0f);
        std::fill(spectrumDBBuffers[i].begin(), spectrumDBBuffers[i].end(), minDecibels);
        std::fill(peakHoldBuffers[i].begin(), peakHoldBuffers[i].end(), minDecibels);
    }
    
    newDataAvailable.store(false);
}

void SpectrumAnalyzer::resetPeakHold()
{
    for (int i = 0; i < 2; ++i)
        std::fill(peakHoldBuffers[i].begin(), peakHoldBuffers[i].end(), minDecibels);
}

//==============================================================================
// AUDIO THREAD (lock-free)
//==============================================================================
void SpectrumAnalyzer::pushSamples(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    if (numSamples == 0 || numChannels == 0)
        return;
    
    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    if (size1 + size2 == 0)
        return; // FIFO full, drop gracefully
    
    auto mixSample = [&](int srcIndex) -> float
    {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sample += buffer.getSample(ch, srcIndex);
        return sample / static_cast<float>(numChannels);
    };
    
    if (size1 > 0)
    {
        for (int i = 0; i < size1; ++i)
            fifoBuffer[static_cast<size_t>(start1 + i)] = mixSample(i);
    }
    if (size2 > 0)
    {
        for (int i = 0; i < size2; ++i)
            fifoBuffer[static_cast<size_t>(start2 + i)] = mixSample(size1 + i);
    }
    
    fifo.finishedWrite(size1 + size2);
    newDataAvailable.store(true);
}

//==============================================================================
// GUI THREAD (heavy work)
//==============================================================================
void SpectrumAnalyzer::processFFT()
{
    const int available = fifo.getNumReady();
    if (available < fftSize)
        return;
    
    int start1, size1, start2, size2;
    fifo.prepareToRead(fftSize, start1, size1, start2, size2);
    
    if (size1 > 0)
        std::copy(fifoBuffer.begin() + start1, fifoBuffer.begin() + start1 + size1, fftData.begin());
    if (size2 > 0)
        std::copy(fifoBuffer.begin() + start2, fifoBuffer.begin() + start2 + size2, fftData.begin() + size1);
    
    fifo.finishedRead(fftSize);
    
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    
    if (window)
        window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
    if (fft)
        fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    const int writeIndex = 1 - activeBufferIndex.load();
    const int readIndex = activeBufferIndex.load();
    
    auto& magOut = spectrumBuffers[writeIndex];
    auto& dbOut = spectrumDBBuffers[writeIndex];
    auto& peakOut = peakHoldBuffers[writeIndex];
    
    const auto& prevDB = spectrumDBBuffers[readIndex];
    const auto& prevPeak = peakHoldBuffers[readIndex];
    
    const float smooth = smoothingFactor.load();
    const float oneMinusSmooth = 1.0f - smooth;
    
    for (int i = 0; i < numBins; ++i)
    {
        float mag = fftData[static_cast<size_t>(i)] / static_cast<float>(fftSize);
        mag = std::max(mag, 1e-10f);
        
        magOut[i] = mag;
        
        float magDB = 20.0f * std::log10(mag);
        magDB = juce::jlimit(minDecibels, maxDecibels, magDB);
        
        float smoothedDB = smooth * prevDB[i] + oneMinusSmooth * magDB;
        dbOut[i] = smoothedDB;
        
        float decayedPeak = prevPeak[i] * decayFactor;
        peakOut[i] = std::max(smoothedDB, decayedPeak);
    }
    
    activeBufferIndex.store(writeIndex);
    newDataAvailable.store(false);
}

//==============================================================================
// Helpers
//==============================================================================
void SpectrumAnalyzer::updateSmoothingCoeffs()
{
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

void SpectrumAnalyzer::setPeakHoldDecayTime(float seconds)
{
    peakHoldDecayTime = juce::jlimit(0.2f, 10.0f, seconds);
    // Approximate decay as 100 dB over the chosen time
    decayDBPerSecond.store(100.0f / peakHoldDecayTime);
    updateDecayFactor(30.0f); // assume ~30 Hz GUI updates
}

void SpectrumAnalyzer::setFFTResolution(Resolution res)
{
    int newOrder = static_cast<int>(res);
    if (newOrder == fftOrder)
        return;
    
    rebuildFFT(newOrder);
}

void SpectrumAnalyzer::rebuildFFT(int newOrder)
{
    fftOrder = juce::jlimit(9, 14, newOrder);
    fftSize = 1 << fftOrder;
    numBins = fftSize / 2;
    
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(static_cast<size_t>(fftSize),
                                                                   juce::dsp::WindowingFunction<float>::hann);
    
    fftData.assign(static_cast<size_t>(fftSize * 2), 0.0f);
    
    for (int i = 0; i < 2; ++i)
    {
        spectrumBuffers[i].assign(static_cast<size_t>(numBins), 0.0f);
        spectrumDBBuffers[i].assign(static_cast<size_t>(numBins), minDecibels);
        peakHoldBuffers[i].assign(static_cast<size_t>(numBins), minDecibels);
    }
    
    fifo.reset();
    fifoBuffer.assign(static_cast<size_t>(fifo.getTotalSize()), 0.0f);
    newDataAvailable.store(false);
}

void SpectrumAnalyzer::updateDecayFactor(float guiUpdateHz)
{
    float dbPerSec = decayDBPerSecond.load();
    decayFactor = 1.0f - (dbPerSec / (guiUpdateHz * 100.0f));
    decayFactor = juce::jlimit(0.8f, 0.999f, decayFactor);
}

//==============================================================================
// Accessors
//==============================================================================
const std::vector<float>& SpectrumAnalyzer::getSpectrum() const
{
    return spectrumBuffers[activeBufferIndex.load()];
}

const std::vector<float>& SpectrumAnalyzer::getSpectrumDB() const
{
    return spectrumDBBuffers[activeBufferIndex.load()];
}

const std::vector<float>& SpectrumAnalyzer::getPeakHold() const
{
    return peakHoldBuffers[activeBufferIndex.load()];
}

float SpectrumAnalyzer::getMagnitudeForFrequency(float frequency) const
{
    int bin = getBinForFrequency(frequency);
    if (bin >= 0 && bin < numBins)
        return spectrumDBBuffers[activeBufferIndex.load()][bin];
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
