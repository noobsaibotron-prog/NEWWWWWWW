#include "AdaptiveAIEngine.h"
#include "AIEngine.h"
#include <algorithm>
#include <cmath>

//==============================================================================
AdaptiveAIEngine::AdaptiveAIEngine()
{
    // Preallocate FFT buffers for transient frequency estimation
    transientFftBuffer.assign(static_cast<size_t>(transientFftSize * 2), 0.0f);
    transientWindow.resize(static_cast<size_t>(transientFftSize));
    for (size_t n = 0; n < transientWindow.size(); ++n)
    {
        transientWindow[n] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi *
                                                     static_cast<float>(n) /
                                                     static_cast<float>(transientFftSize - 1)));
    }
}

//==============================================================================
AdaptiveAIEngine::SignalAnalysis AdaptiveAIEngine::analyzeSignal(
    const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    SignalAnalysis analysis;
    
    if (buffer.getNumSamples() == 0)
        return analysis;
    
    // Calculate dynamic range
    analysis.dynamicRange = calculateDynamicRange(buffer);
    
    // Calculate peak-to-RMS ratio
    analysis.peakToRMS = calculatePeakToRMS(buffer);
    
    // Detect transients
    auto transients = detectTransients(buffer, sampleRate);
    analysis.transientDensity = calculateTransientDensity(transients, 
        static_cast<double>(buffer.getNumSamples()) / sampleRate);
    
    // Calculate silence ratio
    analysis.silenceRatio = calculateSilenceRatio(buffer, sampleRate);
    
    // Estimate compression
    analysis.compressionRatio = estimateCompressionRatio(buffer);
    
    // Detect clipping
    float maxSample = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float absSample = std::abs(channelData[i]);
            if (absSample > maxSample)
                maxSample = absSample;
        }
    }
    analysis.clippingAmount = maxSample > 0.95f ? (maxSample - 0.95f) / 0.05f : 0.0f;
    analysis.clippingAmount = juce::jlimit(0.0f, 1.0f, analysis.clippingAmount);
    
    // Classify signal characteristic
    analysis.characteristic = classifySignal(analysis);
    
    // Calculate confidence
    float confidenceFactors = 0.0f;
    int factorCount = 0;
    
    if (analysis.dynamicRange > 0.0f) { confidenceFactors += 1.0f; factorCount++; }
    if (analysis.peakToRMS > 0.0f) { confidenceFactors += 1.0f; factorCount++; }
    if (analysis.transientDensity > 0.0f) { confidenceFactors += 1.0f; factorCount++; }
    
    analysis.confidence = factorCount > 0 ? (confidenceFactors / static_cast<float>(factorCount)) : 0.5f;
    
    // Store in history
    {
        std::lock_guard<std::mutex> lock(historyMutex);
        AnalysisFrame frame;
        frame.analysis = analysis;
        frame.timestamp = juce::Time::currentTimeMillis();
        
        analysisHistory.push_back(frame);
        if (analysisHistory.size() > maxHistorySize)
            analysisHistory.pop_front();
    }
    
    return analysis;
}

AdaptiveAIEngine::SignalAnalysis AdaptiveAIEngine::analyzeSignal(
    const std::vector<float>& spectrum, double sampleRate)
{
    // Convert spectrum to audio buffer for analysis
    // This is a simplified conversion - in practice, you'd want proper reconstruction
    SignalAnalysis analysis;
    
    if (spectrum.empty())
        return analysis;
    
    // Estimate dynamic range from spectrum
    float maxMag = -1000.0f;
    float minMag = 1000.0f;
    
    for (float mag : spectrum)
    {
        if (mag > -100.0f)
        {
            if (mag > maxMag) maxMag = mag;
            if (mag < minMag) minMag = mag;
        }
    }
    
    analysis.dynamicRange = maxMag - minMag;
    analysis.peakToRMS = maxMag - (maxMag + minMag) / 2.0f;
    
    // Estimate other parameters from spectrum characteristics
    int activeBins = 0;
    for (float mag : spectrum)
    {
        if (mag > -80.0f)
            activeBins++;
    }
    
    analysis.silenceRatio = 1.0f - (static_cast<float>(activeBins) / static_cast<float>(spectrum.size()));
    analysis.transientDensity = 0.0f;  // Cannot estimate from spectrum alone
    analysis.compressionRatio = analysis.dynamicRange < 20.0f ? (20.0f - analysis.dynamicRange) / 20.0f : 0.0f;
    analysis.clippingAmount = maxMag > 0.0f ? juce::jlimit(0.0f, 1.0f, (maxMag - 0.0f) / 12.0f) : 0.0f;
    
    analysis.characteristic = classifySignal(analysis);
    analysis.confidence = 0.7f;  // Lower confidence for spectrum-only analysis
    
    return analysis;
}

//==============================================================================
AdaptiveAIEngine::AdaptiveConfig AdaptiveAIEngine::getAdaptiveConfig(
    const SignalAnalysis& analysis) const
{
    AdaptiveConfig config;
    
    // Base configuration
    config.detectionThreshold = 0.5f;
    config.temporalSmoothing = 0.1f;
    config.sensitivity = 0.5f;
    config.enableTransientMode = true;
    config.enableSparseMode = true;
    
    // Adapt based on signal characteristics
    switch (analysis.characteristic)
    {
        case SignalCharacteristic::ExtremeDynamic:
            // Lower threshold for extreme dynamics
            config.detectionThreshold = 0.3f;
            config.temporalSmoothing = 0.2f;  // More smoothing for dynamics
            config.sensitivity = 0.7f;
            break;
            
        case SignalCharacteristic::TransientHeavy:
            // Higher threshold, less smoothing for transients
            config.detectionThreshold = 0.6f;
            config.temporalSmoothing = 0.05f;
            config.sensitivity = 0.6f;
            config.enableTransientMode = true;
            break;
            
        case SignalCharacteristic::Sparse:
            // Lower threshold for sparse signals
            config.detectionThreshold = 0.4f;
            config.temporalSmoothing = 0.15f;
            config.sensitivity = 0.8f;
            config.enableSparseMode = true;
            break;
            
        case SignalCharacteristic::Compressed:
            // Higher threshold for compressed signals
            config.detectionThreshold = 0.7f;
            config.temporalSmoothing = 0.1f;
            config.sensitivity = 0.4f;
            break;
            
        case SignalCharacteristic::Overdriven:
            // Very high threshold, focus on clipping
            config.detectionThreshold = 0.8f;
            config.temporalSmoothing = 0.05f;
            config.sensitivity = 0.3f;
            break;
            
        default:
            break;
    }
    
    // Apply adaptation speed
    float speed = adaptationSpeed.load();
    config.detectionThreshold = config.detectionThreshold * (1.0f - speed * 0.3f) + 0.5f * (speed * 0.3f);
    config.sensitivity = config.sensitivity * (1.0f - speed * 0.2f) + 0.5f * (speed * 0.2f);
    
    return config;
}

void AdaptiveAIEngine::applyAdaptiveConfig(AIEngine& aiEngine, const AdaptiveConfig& config)
{
    aiEngine.setSensitivity(config.sensitivity);
    aiEngine.setDetectionThreshold(config.detectionThreshold);
    aiEngine.setTemporalSmoothing(config.temporalSmoothing);
    aiEngine.setTransientModeEnabled(config.enableTransientMode);
    aiEngine.setSparseModeEnabled(config.enableSparseMode);
}

//==============================================================================
std::vector<AdaptiveAIEngine::Transient> AdaptiveAIEngine::detectTransients(
    const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    std::vector<Transient> transients;
    
    if (buffer.getNumSamples() < 2)
        return transients;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Simple transient detection: look for rapid amplitude changes
    const float threshold = transientThreshold;
    const int windowSize = static_cast<int>(sampleRate * 0.01f);  // 10ms window
    const int fftSize    = transientFftSize;
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        
        for (int i = windowSize; i < numSamples - windowSize; ++i)
        {
            // Calculate local energy
            float energyBefore = 0.0f;
            float energyAfter = 0.0f;
            
            for (int j = i - windowSize; j < i; ++j)
            {
                energyBefore += channelData[j] * channelData[j];
            }
            energyBefore /= static_cast<float>(windowSize);
            
            for (int j = i; j < i + windowSize; ++j)
            {
                energyAfter += channelData[j] * channelData[j];
            }
            energyAfter /= static_cast<float>(windowSize);
            
            // Check for significant energy increase (attack)
            float energyRatio = energyAfter / (energyBefore + 1e-10f);
            
            if (energyRatio > (1.0f + threshold))
            {
                Transient transient;
                transient.samplePosition = i;
                transient.amplitude = std::sqrt(energyAfter);
                transient.duration = static_cast<float>(windowSize) / static_cast<float>(sampleRate);

                // FFT-based dominant frequency estimation on a local window
                const int start = juce::jlimit(0, numSamples, i - fftSize / 2);
                const int available = juce::jmin(fftSize, numSamples - start);

                // Clear FFT buffer and copy windowed samples (real/imag interleaved expected by JUCE FFT real-only)
                std::fill(transientFftBuffer.begin(), transientFftBuffer.end(), 0.0f);
                for (int n = 0; n < available; ++n)
                {
                    const float w = transientWindow[static_cast<size_t>(n)];
                    transientFftBuffer[2 * n] = channelData[start + n] * w;     // real
                    transientFftBuffer[2 * n + 1] = 0.0f;                       // imag
                }

                transientFft.performRealOnlyForwardTransform(transientFftBuffer.data());

                float maxMag = 0.0f;
                int maxBin = 0;
                const int nyquistBin = fftSize / 2;
                for (int k = 1; k < nyquistBin; ++k) // skip DC
                {
                    const float re = transientFftBuffer[2 * k];
                    const float im = transientFftBuffer[2 * k + 1];
                    const float mag = re * re + im * im;
                    if (mag > maxMag)
                    {
                        maxMag = mag;
                        maxBin = k;
                    }
                }

                transient.frequency = static_cast<float>(maxBin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);

                transients.push_back(transient);
                
                // Skip ahead to avoid duplicate detections
                i += windowSize;
            }
        }
    }
    
    return transients;
}

//==============================================================================
bool AdaptiveAIEngine::isSparseSignal(const juce::AudioBuffer<float>& buffer,
                                     double sampleRate, float silenceThreshold) const
{
    float silenceRatio = calculateSilenceRatio(buffer, sampleRate, silenceThreshold);
    return silenceRatio > 0.5f;  // More than 50% silence
}

float AdaptiveAIEngine::calculateSilenceRatio(const juce::AudioBuffer<float>& buffer,
                                            double sampleRate, float silenceThreshold) const
{
    if (buffer.getNumSamples() == 0)
        return 1.0f;
    
    const float thresholdLinear = juce::Decibels::decibelsToGain(silenceThreshold);
    int silentSamples = 0;
    int totalSamples = 0;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::abs(channelData[i]) < thresholdLinear)
                silentSamples++;
            totalSamples++;
        }
    }
    
    return totalSamples > 0 ? static_cast<float>(silentSamples) / static_cast<float>(totalSamples) : 1.0f;
}

//==============================================================================
float AdaptiveAIEngine::calculateDynamicRange(const juce::AudioBuffer<float>& buffer) const
{
    if (buffer.getNumSamples() == 0)
        return 0.0f;
    
    float maxLevel = -100.0f;
    float minLevel = 0.0f;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float level = juce::Decibels::gainToDecibels(std::abs(channelData[i]), -100.0f);
            if (level > maxLevel) maxLevel = level;
            if (level < minLevel) minLevel = level;
        }
    }
    
    return maxLevel - minLevel;
}

float AdaptiveAIEngine::calculatePeakToRMS(const juce::AudioBuffer<float>& buffer) const
{
    if (buffer.getNumSamples() == 0)
        return 0.0f;
    
    float peak = 0.0f;
    float rms = 0.0f;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* channelData = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float absSample = std::abs(channelData[i]);
            if (absSample > peak)
                peak = absSample;
            rms += channelData[i] * channelData[i];
        }
    }
    
    rms = std::sqrt(rms / static_cast<float>(buffer.getNumSamples() * buffer.getNumChannels()));
    
    if (rms > 0.0f)
        return juce::Decibels::gainToDecibels(peak / rms);
    
    return 0.0f;
}

float AdaptiveAIEngine::estimateCompressionRatio(const juce::AudioBuffer<float>& buffer) const
{
    // Estimate compression by analyzing dynamic range
    float dr = calculateDynamicRange(buffer);
    
    // Normal range: 40-60dB, compressed: <20dB
    if (dr > 40.0f)
        return 0.0f;  // Not compressed
    
    if (dr < 10.0f)
        return 1.0f;  // Heavily compressed
    
    return (40.0f - dr) / 30.0f;  // Linear interpolation
}

//==============================================================================
AdaptiveAIEngine::SignalCharacteristic AdaptiveAIEngine::classifySignal(
    const SignalAnalysis& analysis) const
{
    // Classify based on analysis parameters
    if (analysis.clippingAmount > 0.3f)
        return SignalCharacteristic::Overdriven;
    
    if (analysis.compressionRatio > 0.7f)
        return SignalCharacteristic::Compressed;
    
    if (analysis.silenceRatio > 0.5f)
        return SignalCharacteristic::Sparse;
    
    if (analysis.transientDensity > 10.0f)  // More than 10 transients per second
        return SignalCharacteristic::TransientHeavy;
    
    if (analysis.dynamicRange > maxDynamicRange || analysis.dynamicRange < minDynamicRange)
        return SignalCharacteristic::ExtremeDynamic;
    
    return SignalCharacteristic::Normal;
}

float AdaptiveAIEngine::calculateTransientDensity(const std::vector<Transient>& transients,
                                                  double duration) const
{
    if (duration <= 0.0)
        return 0.0f;
    
    return static_cast<float>(transients.size()) / static_cast<float>(duration);
}

