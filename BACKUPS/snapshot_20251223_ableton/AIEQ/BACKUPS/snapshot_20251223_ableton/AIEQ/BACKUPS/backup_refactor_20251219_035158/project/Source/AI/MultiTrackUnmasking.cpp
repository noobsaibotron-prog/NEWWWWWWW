#include "MultiTrackUnmasking.h"
#include <algorithm>
#include <cmath>

//==============================================================================
MultiTrackUnmasking::MultiTrackUnmasking()
{
}

//==============================================================================
void MultiTrackUnmasking::registerTrack(const juce::String& trackId, const juce::String& trackName)
{
    std::lock_guard<std::mutex> lock(tracksMutex);
    
    TrackInfo info;
    info.trackId = trackId;
    info.trackName = trackName.isEmpty() ? trackId : trackName;
    info.isActive = false;
    info.spectrum.resize(2048, -100.0f);  // Default spectrum size
    info.lastUpdateTime = juce::Time::currentTimeMillis();
    
    registeredTracks[trackId] = info;
}

void MultiTrackUnmasking::unregisterTrack(const juce::String& trackId)
{
    std::lock_guard<std::mutex> lock(tracksMutex);
    registeredTracks.erase(trackId);
}

void MultiTrackUnmasking::updateTrackSpectrum(const juce::String& trackId, const std::vector<float>& spectrum)
{
    std::lock_guard<std::mutex> lock(tracksMutex);
    
    auto it = registeredTracks.find(trackId);
    if (it != registeredTracks.end())
    {
        it->second.spectrum = spectrum;
        it->second.lastUpdateTime = juce::Time::currentTimeMillis();
    }
}

void MultiTrackUnmasking::updateTrackLevels(const juce::String& trackId, float rmsLevel, float peakLevel)
{
    std::lock_guard<std::mutex> lock(tracksMutex);
    
    auto it = registeredTracks.find(trackId);
    if (it != registeredTracks.end())
    {
        it->second.rmsLevel = rmsLevel;
        it->second.peakLevel = peakLevel;
    }
}

void MultiTrackUnmasking::setTrackActive(const juce::String& trackId, bool active)
{
    std::lock_guard<std::mutex> lock(tracksMutex);
    
    auto it = registeredTracks.find(trackId);
    if (it != registeredTracks.end())
    {
        it->second.isActive = active;
    }
}

//==============================================================================
std::vector<MultiTrackUnmasking::MaskingAnalysis> MultiTrackUnmasking::analyzeMasking(double sampleRate)
{
    if (!isEnabled.load())
        return {};
    
    std::vector<TrackInfo> tracks;
    {
        std::lock_guard<std::mutex> lock(tracksMutex);
        
        // Copy active tracks only
        for (const auto& [id, info] : registeredTracks)
        {
            if (info.isActive && !info.spectrum.empty())
            {
                tracks.push_back(info);
            }
        }
    }
    
    if (tracks.size() < 2)
        return {};  // Need at least 2 tracks for masking analysis
    
    return detectCrossTrackMasking(tracks, sampleRate);
}

std::vector<MultiTrackUnmasking::UnmaskingCorrection> MultiTrackUnmasking::generateCorrections(
    double sampleRate, float sensitivityParam)
{
    auto maskingAnalyses = analyzeMasking(sampleRate);
    std::vector<UnmaskingCorrection> corrections;
    
    float sens = sensitivityParam > 0.0f ? sensitivityParam : sensitivity.load();
    
    for (const auto& analysis : maskingAnalyses)
    {
        if (analysis.maskingAmount < minMaskingThreshold)
            continue;
        
        // Apply sensitivity
        float adjustedMasking = analysis.maskingAmount * sens;
        if (adjustedMasking < minMaskingThreshold)
            continue;
        
        UnmaskingCorrection correction;
        correction.trackId = analysis.maskedTrackId;
        correction.frequency = analysis.frequency;
        correction.gain = analysis.suggestedGain * sens;
        correction.q = 1.0f + (analysis.bandwidth / analysis.frequency) * 0.5f;  // Estimate Q from bandwidth
        correction.q = juce::jlimit(0.5f, 10.0f, correction.q);
        correction.confidence = analysis.confidence;
        
        corrections.push_back(correction);
    }
    
    return corrections;
}

//==============================================================================
float MultiTrackUnmasking::calculateMaskingThreshold(float maskerLevel, float maskerFreq, float probeFreq) const
{
    // Simplified psychoacoustic masking model
    // Based on ISO/IEC 11172-3 (MPEG-1) masking model
    
    // Spread of masking function
    float spread = calculateSpreadOfMasking(maskerLevel, maskerFreq, probeFreq);
    
    // Masking threshold = masker level - spread - offset
    float threshold = maskerLevel - spread - maskingThresholdOffset;
    
    return threshold;
}

float MultiTrackUnmasking::calculateCriticalBandwidth(float centerFreq) const
{
    // Critical bandwidth approximation (Bark scale)
    // Simplified: CBW ≈ 25 + 75 * (1 + 1.4 * (f/1000)^2)^0.69
    float f_kHz = centerFreq / 1000.0f;
    float cbw = 25.0f + 75.0f * std::pow(1.0f + 1.4f * f_kHz * f_kHz, 0.69f);
    
    return cbw;
}

float MultiTrackUnmasking::calculateSpreadOfMasking(float maskerLevel, float maskerFreq, float probeFreq) const
{
    // Spread of masking function
    // Higher frequencies mask lower frequencies more than vice versa
    
    float freqRatio = probeFreq / maskerFreq;
    float spread = 0.0f;
    
    if (probeFreq > maskerFreq)
    {
        // Upward spread: less masking
        spread = 27.0f * (freqRatio - 1.0f);
    }
    else
    {
        // Downward spread: more masking
        spread = -23.0f * (1.0f - freqRatio);
    }
    
    // Scale by masker level
    spread *= (maskerLevel + 60.0f) / 60.0f;  // Normalize around -60dB
    
    return juce::jlimit(0.0f, 100.0f, spread);
}

//==============================================================================
std::vector<MultiTrackUnmasking::MaskingAnalysis> MultiTrackUnmasking::detectCrossTrackMasking(
    const std::vector<TrackInfo>& tracks, double sampleRate) const
{
    std::vector<MaskingAnalysis> analyses;
    
    // Compare each pair of tracks
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        for (size_t j = 0; j < tracks.size(); ++j)
        {
            if (i == j)
                continue;
            
            const auto& masker = tracks[i];
            const auto& masked = tracks[j];
            
            // Only analyze if masker is louder
            if (masker.rmsLevel <= masked.rmsLevel)
                continue;
            
            // Analyze masking across frequency spectrum
            const int numBins = static_cast<int>(std::min(masker.spectrum.size(), masked.spectrum.size()));
            const float binWidth = static_cast<float>(sampleRate) / (2.0f * static_cast<float>(numBins));
            
            // Find frequency bands with significant masking
            for (int bin = 1; bin < numBins - 1; ++bin)
            {
                float freq = bin * binWidth;
                float maskerMag = masker.spectrum[bin];
                float maskedMag = masked.spectrum[bin];
                
                if (maskerMag < -80.0f || maskedMag < -80.0f)
                    continue;  // Below noise floor
                
                // Calculate masking threshold
                float maskingThreshold = calculateMaskingThreshold(maskerMag, freq, freq);
                
                // Check if masked track is below masking threshold
                if (maskedMag < maskingThreshold)
                {
                    float maskingAmount = maskingThreshold - maskedMag;
                    
                    // Only report significant masking
                    if (maskingAmount >= minMaskingThreshold)
                    {
                        MaskingAnalysis analysis;
                        analysis.maskedTrackId = masked.trackId;
                        analysis.maskingTrackId = masker.trackId;
                        analysis.frequency = freq;
                        analysis.bandwidth = calculateCriticalBandwidth(freq);
                        analysis.maskingAmount = maskingAmount / 20.0f;  // Normalize to 0-1
                        analysis.maskingAmount = juce::jlimit(0.0f, 1.0f, analysis.maskingAmount);
                        
                        // Suggested gain: boost masked track to just above masking threshold
                        analysis.suggestedGain = (maskingThreshold + 3.0f) - maskedMag;  // +3dB safety margin
                        analysis.suggestedGain = juce::jlimit(0.0f, 12.0f, analysis.suggestedGain);
                        
                        // Confidence based on level difference and masking amount
                        float levelDiff = masker.rmsLevel - masked.rmsLevel;
                        analysis.confidence = juce::jlimit(0.0f, 1.0f, 
                            (levelDiff / 20.0f) * (maskingAmount * 2.0f));
                        
                        analyses.push_back(analysis);
                    }
                }
            }
        }
    }
    
    return analyses;
}

float MultiTrackUnmasking::calculateMaskingAmount(const TrackInfo& masker, const TrackInfo& masked,
                                                   float frequency, double sampleRate) const
{
    // Simplified: calculate masking at specific frequency
    int bin = static_cast<int>(frequency / (sampleRate / (2.0f * static_cast<float>(masker.spectrum.size()))));
    bin = juce::jlimit(0, static_cast<int>(masker.spectrum.size()) - 1, bin);
    
    float maskerMag = masker.spectrum[bin];
    float maskedMag = masked.spectrum[bin];
    
    float maskingThreshold = calculateMaskingThreshold(maskerMag, frequency, frequency);
    
    if (maskedMag < maskingThreshold)
        return maskingThreshold - maskedMag;
    
    return 0.0f;
}

//==============================================================================
float MultiTrackUnmasking::findPeakInRange(const std::vector<float>& spectrum, double sampleRate,
                                          float minFreq, float maxFreq) const
{
    if (spectrum.empty())
        return 0.0f;
    
    float binWidth = static_cast<float>(sampleRate) / (2.0f * static_cast<float>(spectrum.size()));
    int minBin = static_cast<int>(minFreq / binWidth);
    int maxBin = static_cast<int>(maxFreq / binWidth);
    
    minBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, minBin);
    maxBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, maxBin);
    
    if (maxBin <= minBin)
        return (minFreq + maxFreq) / 2.0f;
    
    float maxMag = -1000.0f;
    int maxBinIdx = minBin;
    
    for (int i = minBin; i <= maxBin; ++i)
    {
        if (spectrum[i] > maxMag)
        {
            maxMag = spectrum[i];
            maxBinIdx = i;
        }
    }
    
    return maxBinIdx * binWidth;
}

float MultiTrackUnmasking::calculateBandEnergy(const std::vector<float>& spectrum, double sampleRate,
                                               float lowFreq, float highFreq) const
{
    if (spectrum.empty())
        return -100.0f;
    
    float binWidth = static_cast<float>(sampleRate) / (2.0f * static_cast<float>(spectrum.size()));
    int lowBin = static_cast<int>(lowFreq / binWidth);
    int highBin = static_cast<int>(highFreq / binWidth);
    
    lowBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, lowBin);
    highBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, highBin);
    
    if (highBin <= lowBin)
        return -100.0f;
    
    // Convert dB to linear, sum, convert back to dB
    float sum = 0.0f;
    int count = 0;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (spectrum[i] > -100.0f)
        {
            sum += juce::Decibels::decibelsToGain(spectrum[i]);
            ++count;
        }
    }
    
    if (count == 0)
        return -100.0f;
    
    float avgGain = sum / static_cast<float>(count);
    return juce::Decibels::gainToDecibels(avgGain);
}

