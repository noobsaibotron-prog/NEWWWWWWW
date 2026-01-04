#include "ReferenceMatcher.h"
#include <cmath>
#include <functional>
#include <algorithm>

//==============================================================================
ReferenceMatcher::ReferenceMatcher()
{
    referenceSpectrum.resize(static_cast<size_t>(numBins), -100.0f);
    averagedInputSpectrum.resize(static_cast<size_t>(numBins), -100.0f);
    matchCurve.resize(static_cast<size_t>(numBins), 0.0f);
    
    initializePresets();
}

//==============================================================================
void ReferenceMatcher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    reset();
}

void ReferenceMatcher::reset()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    std::fill(averagedInputSpectrum.begin(), averagedInputSpectrum.end(), -100.0f);
    std::fill(matchCurve.begin(), matchCurve.end(), 0.0f);
    
    inputCaptureCount = 0;
    captureState.store(CaptureState::Idle);
}

//==============================================================================
bool ReferenceMatcher::loadReferenceFromFile(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
        return false;
    
    // Read entire file into buffer
    const int numSamples = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, true);
    
    // Mix to mono if stereo
    juce::AudioBuffer<float> monoBuffer(1, numSamples);
    monoBuffer.clear();
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        monoBuffer.addFrom(0, 0, buffer, ch, 0, numSamples, 1.0f / buffer.getNumChannels());
    
    // Analyze in chunks and average
    std::vector<float> tempSpectrum(static_cast<size_t>(numBins), 0.0f);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    int hopSize = fftSize / 2;
    int numFrames = 0;
    
    for (int pos = 0; pos + fftSize <= numSamples; pos += hopSize)
    {
        // Copy chunk to FFT buffer
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            fftData[static_cast<size_t>(i)] = monoBuffer.getSample(0, pos + i);
        
        // Apply window
        window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
        
        // Perform FFT
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        
        // Accumulate magnitude
        for (int bin = 0; bin < numBins; ++bin)
        {
            float mag = fftData[static_cast<size_t>(bin)] / static_cast<float>(fftSize) * 2.0f;
            if (mag < 1e-10f) mag = 1e-10f;
            float magDB = 20.0f * std::log10(mag);
            tempSpectrum[static_cast<size_t>(bin)] += magDB;
        }
        
        numFrames++;
    }
    
    if (numFrames == 0)
        return false;
    
    // Average and store
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        
        for (int bin = 0; bin < numBins; ++bin)
        {
            referenceSpectrum[static_cast<size_t>(bin)] = 
                tempSpectrum[static_cast<size_t>(bin)] / static_cast<float>(numFrames);
        }
        
        // Apply smoothing
        smoothSpectrum(referenceSpectrum, smoothingAmount);
    }
    
    captureState.store(CaptureState::Ready);
    calculateMatchCurve();
    
    return true;
}

//==============================================================================
void ReferenceMatcher::captureFromSidechain(const juce::AudioBuffer<float>& buffer)
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    std::vector<float> frameSpectrum(static_cast<size_t>(numBins), -100.0f);
    analyzeAudioBuffer(buffer, frameSpectrum);
    
    // Running average
    float alpha = 1.0f / (referenceCaptureCount + 1);
    for (int bin = 0; bin < numBins; ++bin)
    {
        referenceSpectrum[static_cast<size_t>(bin)] = 
            (1.0f - alpha) * referenceSpectrum[static_cast<size_t>(bin)] + 
            alpha * frameSpectrum[static_cast<size_t>(bin)];
    }
    
    referenceCaptureCount++;
    
    if (referenceCaptureCount >= targetCaptureCount)
    {
        smoothSpectrum(referenceSpectrum, smoothingAmount);
        captureState.store(CaptureState::Ready);
        calculateMatchCurve();
    }
}

//==============================================================================
void ReferenceMatcher::startInputCapture()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    std::fill(averagedInputSpectrum.begin(), averagedInputSpectrum.end(), -100.0f);
    inputCaptureCount = 0;
    captureState.store(CaptureState::CapturingInput);
}

void ReferenceMatcher::stopInputCapture()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    if (inputCaptureCount > 0)
    {
        smoothSpectrum(averagedInputSpectrum, smoothingAmount);
        calculateMatchCurve();
    }
    
    captureState.store(hasReference() ? CaptureState::Ready : CaptureState::Idle);
}

void ReferenceMatcher::captureInput(const juce::AudioBuffer<float>& buffer)
{
    if (captureState.load() != CaptureState::CapturingInput)
        return;
    
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    std::vector<float> frameSpectrum(static_cast<size_t>(numBins), -100.0f);
    analyzeAudioBuffer(buffer, frameSpectrum);
    
    // Running average
    float alpha = 1.0f / (inputCaptureCount + 1);
    for (int bin = 0; bin < numBins; ++bin)
    {
        averagedInputSpectrum[static_cast<size_t>(bin)] = 
            (1.0f - alpha) * averagedInputSpectrum[static_cast<size_t>(bin)] + 
            alpha * frameSpectrum[static_cast<size_t>(bin)];
    }
    
    inputCaptureCount++;
    
    if (inputCaptureCount >= targetCaptureCount)
    {
        stopInputCapture();
    }
}

//==============================================================================
void ReferenceMatcher::setInputSpectrum(const std::vector<float>& spectrum)
{
    if (captureState.load() != CaptureState::CapturingInput)
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        
        float alpha = 0.05f; // Slow averaging
        for (size_t i = 0; i < std::min(averagedInputSpectrum.size(), spectrum.size()); ++i)
        {
            averagedInputSpectrum[i] = (1.0f - alpha) * averagedInputSpectrum[i] + alpha * spectrum[i];
        }
        
        inputCaptureCount++;
        
        // Recalculate match curve periodically
        if (inputCaptureCount % 30 == 0 && hasReference())
        {
            calculateMatchCurve();
        }
    }
}

//==============================================================================
void ReferenceMatcher::calculateMatchCurve()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    if (referenceSpectrum.empty() || averagedInputSpectrum.empty())
        return;
    
    for (int bin = 0; bin < numBins; ++bin)
    {
        float freq = binToFreq(bin);
        float weight = getFrequencyWeight(freq);
        
        // Calculate difference
        float diff = referenceSpectrum[static_cast<size_t>(bin)] - 
                    averagedInputSpectrum[static_cast<size_t>(bin)];
        
        // Apply amount and weight
        diff *= matchAmount * weight;
        
        // Clamp to limits
        diff = juce::jlimit(-maxCutDB, maxBoostDB, diff);
        
        matchCurve[static_cast<size_t>(bin)] = diff;
    }
    
    // Apply smoothing to match curve
    smoothSpectrum(matchCurve, smoothingAmount * 0.5f);
}

//==============================================================================
void ReferenceMatcher::getMatchedMagnitudes(std::vector<float>& magnitudes,
                                            const std::vector<float>& frequencies,
                                            double /*sampleRate*/) const
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    magnitudes.resize(frequencies.size());
    
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        float freq = frequencies[i];
        int bin = freqToBin(freq);
        
        if (bin >= 0 && bin < static_cast<int>(matchCurve.size()))
        {
            // Linear interpolation between bins
            float fracBin = (freq * static_cast<float>(fftSize)) / static_cast<float>(currentSampleRate);
            int bin1 = static_cast<int>(fracBin);
            int bin2 = bin1 + 1;
            float frac = fracBin - static_cast<float>(bin1);
            
            float gainDB = 0.0f;
            if (bin1 >= 0 && bin2 < static_cast<int>(matchCurve.size()))
            {
                gainDB = (1.0f - frac) * matchCurve[static_cast<size_t>(bin1)] + 
                        frac * matchCurve[static_cast<size_t>(bin2)];
            }
            else if (bin1 >= 0 && bin1 < static_cast<int>(matchCurve.size()))
            {
                gainDB = matchCurve[static_cast<size_t>(bin1)];
            }
            
            magnitudes[i] = std::pow(10.0f, gainDB / 20.0f);
        }
        else
        {
            magnitudes[i] = 1.0f;
        }
    }
}

//==============================================================================
float ReferenceMatcher::getCaptureProgress() const
{
    CaptureState state = captureState.load();
    
    if (state == CaptureState::CapturingInput)
        return static_cast<float>(inputCaptureCount) / static_cast<float>(targetCaptureCount);
    else if (state == CaptureState::CapturingReference)
        return static_cast<float>(referenceCaptureCount) / static_cast<float>(targetCaptureCount);
    
    return state == CaptureState::Ready ? 1.0f : 0.0f;
}

//==============================================================================
void ReferenceMatcher::loadPresetReference(const juce::String& presetName)
{
    auto it = presetCurves.find(presetName);
    if (it != presetCurves.end())
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        referenceSpectrum = it->second;
        captureState.store(CaptureState::Ready);
        calculateMatchCurve();
    }
}

juce::StringArray ReferenceMatcher::getAvailablePresets() const
{
    juce::StringArray presets;
    for (const auto& pair : presetCurves)
        presets.add(pair.first);
    return presets;
}

//==============================================================================
void ReferenceMatcher::initializePresets()
{
    // Initialize with genre-based target curves
    // These are idealized spectral profiles based on professional masters
    
    auto createCurve = [this](std::function<float(float)> shapeFunc) {
        std::vector<float> curve(static_cast<size_t>(numBins));
        for (int bin = 0; bin < numBins; ++bin)
        {
            float freq = binToFreq(bin);
            curve[static_cast<size_t>(bin)] = shapeFunc(freq);
        }
        return curve;
    };
    
    // Techno - Strong sub, controlled mids, bright highs
    presetCurves["Techno"] = createCurve([](float freq) {
        if (freq < 30.0f) return -6.0f;
        if (freq < 60.0f) return 0.0f;
        if (freq < 120.0f) return -2.0f;
        if (freq < 300.0f) return -6.0f;
        if (freq < 800.0f) return -10.0f;
        if (freq < 3000.0f) return -12.0f;
        if (freq < 8000.0f) return -15.0f;
        return -20.0f - (freq - 8000.0f) / 1000.0f;
    });
    
    // Industrial - Aggressive mids, harsh highs
    presetCurves["Industrial"] = createCurve([](float freq) {
        if (freq < 40.0f) return -4.0f;
        if (freq < 80.0f) return 2.0f;
        if (freq < 200.0f) return -2.0f;
        if (freq < 500.0f) return -6.0f;
        if (freq < 2000.0f) return -8.0f;
        if (freq < 5000.0f) return -10.0f;
        if (freq < 10000.0f) return -14.0f;
        return -18.0f;
    });
    
    // Jungle/DnB - Punchy bass, crisp highs
    presetCurves["Jungle"] = createCurve([](float freq) {
        if (freq < 40.0f) return -8.0f;
        if (freq < 100.0f) return -2.0f;
        if (freq < 200.0f) return 0.0f;
        if (freq < 400.0f) return -4.0f;
        if (freq < 1000.0f) return -8.0f;
        if (freq < 4000.0f) return -12.0f;
        if (freq < 10000.0f) return -16.0f;
        return -22.0f;
    });
    
    // Dub Techno - Deep sub, spacious mids
    presetCurves["Dub Techno"] = createCurve([](float freq) {
        if (freq < 50.0f) return -2.0f;
        if (freq < 100.0f) return 2.0f;
        if (freq < 200.0f) return 0.0f;
        if (freq < 400.0f) return -4.0f;
        if (freq < 1000.0f) return -8.0f;
        if (freq < 3000.0f) return -14.0f;
        if (freq < 8000.0f) return -20.0f;
        return -28.0f;
    });
    
    // Deep House - Warm, balanced
    presetCurves["Deep House"] = createCurve([](float freq) {
        if (freq < 40.0f) return -6.0f;
        if (freq < 80.0f) return 0.0f;
        if (freq < 150.0f) return -2.0f;
        if (freq < 400.0f) return -5.0f;
        if (freq < 1500.0f) return -10.0f;
        if (freq < 5000.0f) return -15.0f;
        if (freq < 12000.0f) return -22.0f;
        return -30.0f;
    });
    
    // Balanced/Flat reference
    presetCurves["Balanced"] = createCurve([](float freq) {
        // Pink noise slope: -3dB/octave
        return -3.0f * std::log2(freq / 1000.0f);
    });
}

//==============================================================================
void ReferenceMatcher::analyzeAudioBuffer(const juce::AudioBuffer<float>& buffer,
                                          std::vector<float>& outputSpectrum)
{
    if (buffer.getNumSamples() < fftSize)
        return;
    
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
    
    // Mix to mono and copy
    for (int i = 0; i < fftSize; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            sample += buffer.getSample(ch, i);
        sample /= static_cast<float>(buffer.getNumChannels());
        fftData[static_cast<size_t>(i)] = sample;
    }
    
    // Apply window and perform FFT
    window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    
    // Convert to dB
    for (int bin = 0; bin < numBins; ++bin)
    {
        float mag = fftData[static_cast<size_t>(bin)] / static_cast<float>(fftSize) * 2.0f;
        if (mag < 1e-10f) mag = 1e-10f;
        outputSpectrum[static_cast<size_t>(bin)] = 20.0f * std::log10(mag);
    }
}

void ReferenceMatcher::smoothSpectrum(std::vector<float>& spectrum, float amount)
{
    if (amount < 0.01f || spectrum.empty())
        return;
    
    // Simple moving average with variable window size based on amount
    int windowSize = static_cast<int>(amount * 50.0f) + 1;
    std::vector<float> smoothed(spectrum.size());
    
    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        float sum = 0.0f;
        int count = 0;
        
        for (int j = -windowSize; j <= windowSize; ++j)
        {
            int idx = static_cast<int>(i) + j;
            if (idx >= 0 && idx < static_cast<int>(spectrum.size()))
            {
                sum += spectrum[static_cast<size_t>(idx)];
                count++;
            }
        }
        
        smoothed[i] = sum / static_cast<float>(count);
    }
    
    spectrum = smoothed;
}

float ReferenceMatcher::getFrequencyWeight(float freq) const
{
    switch (matchMode)
    {
        case MatchMode::LowEnd:
            if (freq < 20.0f || freq > 200.0f) return 0.0f;
            return 1.0f;
            
        case MatchMode::MidRange:
            if (freq < 200.0f || freq > 4000.0f) return 0.0f;
            return 1.0f;
            
        case MatchMode::HighEnd:
            if (freq < 4000.0f || freq > 20000.0f) return 0.0f;
            return 1.0f;
            
        case MatchMode::Custom:
            if (freq < customMinFreq || freq > customMaxFreq) return 0.0f;
            return 1.0f;
            
        case MatchMode::Full:
        default:
            return 1.0f;
    }
}

void ReferenceMatcher::setCustomRange(float minFreq, float maxFreq)
{
    customMinFreq = juce::jlimit(20.0f, 20000.0f, minFreq);
    customMaxFreq = juce::jlimit(20.0f, 20000.0f, maxFreq);
    if (customMaxFreq < customMinFreq)
        std::swap(customMinFreq, customMaxFreq);
}

int ReferenceMatcher::freqToBin(float freq) const
{
    return std::max(0, juce::roundToInt((freq * static_cast<float>(fftSize)) / 
                                        static_cast<float>(currentSampleRate)));
}

float ReferenceMatcher::binToFreq(int bin) const
{
    return (static_cast<float>(bin) * static_cast<float>(currentSampleRate)) / 
           static_cast<float>(fftSize);
}
