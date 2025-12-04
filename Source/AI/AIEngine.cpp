#include "AIEngine.h"
#include <cmath>

//==============================================================================
/**
 * AIEngine Implementation - Optimized and Improved
 * 
 * Key Improvements:
 * - Fixed: Per-channel filter states (was sharing state between channels)
 * - Optimized: Cached biquad coefficients (recalculated only when corrections change)
 * - Added: Sample rate validation and safety checks
 * - Refactored: Magic numbers extracted to named constants
 * - Improved: Better bounds checking and error handling
 * - Enhanced: Thread-safe spectrum access in calculateBandEnergy
 */

AIEngine::AIEngine()
{
    currentSpectrum.resize(numBins, -100.0f);
    applyProfileThresholds();
    
    // Initialize filter states for dynamic correction (per-channel, per-correction)
    for (auto& correctionStates : filterStates)
    {
        for (auto& channelState : correctionStates)
        {
            channelState.z1 = 0.0f;
            channelState.z2 = 0.0f;
        }
    }
    
    // Initialize coefficient cache
    for (auto& coeffs : coefficientCache)
    {
        coeffs.valid = false;
    }
    
    // Initialize ML Engine
    mlEngine.initialize();
    
    // Initialize spectrum history for temporal smoothing
    spectrumHistory.resize(temporalFrames);
    for (auto& frame : spectrumHistory)
    {
        frame.resize(numBins, -100.0f);
    }
    historyWriteIndex = 0;
}

void AIEngine::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    // Validate sample rate
    if (sampleRate <= 0.0 || sampleRate > maxSampleRate || sampleRate < minSampleRate)
    {
        jassertfalse; // Invalid sample rate
        currentSampleRate = 44100.0;
    }
    else
    {
        currentSampleRate = sampleRate;
    }
    
    clearCorrections();
    clearHistory();
    
    // Reset filter states (per-channel, per-correction)
    for (auto& correctionStates : filterStates)
    {
        for (auto& channelState : correctionStates)
        {
            channelState.z1 = 0.0f;
            channelState.z2 = 0.0f;
        }
    }
    
    // Invalidate coefficient cache
    coefficientsNeedUpdate = true;
    for (auto& coeffs : coefficientCache)
    {
        coeffs.valid = false;
    }
}

void AIEngine::analyzeSpectrum(const std::vector<float>& spectrum)
{
    if (!enabled || spectrum.size() < static_cast<size_t>(numBins))
        return;
    
    // Rate limiting
    if (++analysisCounter < analysisInterval)
        return;
    analysisCounter = 0;
    
    // Thread-safe spectrum copy and history update
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        currentSpectrum = spectrum;
        updateSpectrumHistory(spectrum);
        
        // Update RMS tracking
        float rmsSum = 0.0f;
        int rmsCount = 0;
        for (int i = 2; i < numBins - 2; ++i)
        {
            if (spectrum[i] > -100.0f)
            {
                rmsSum += spectrum[i];
                ++rmsCount;
            }
        }
        if (rmsCount > 0)
        {
            currentRMS = rmsSum / static_cast<float>(rmsCount);
            averageRMS = averageRMS * rmsSmoothing + currentRMS * (1.0f - rmsSmoothing);
        }
    }
    
    // Perform detection (use ML if enabled, otherwise heuristics)
    if (useMLDetection)
    {
        detectProblemsWithML();
    }
    else
    {
        detectProblems();
    }
    detectGenre();
    
    // Save to history
    saveAnalysisSnapshot();
    
    // Signal new analysis available
    newAnalysisAvailable = true;
}

//==============================================================================
// Optimized processCorrections() with cached coefficients and per-channel states
void AIEngine::processCorrections(juce::AudioBuffer<float>& buffer)
{
    if (!enabled || correctionMode == CorrectionMode::Off)
        return;
    
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;
    
    std::lock_guard<std::mutex> lock(correctionsMutex);
    
    if (approvedCorrections.empty())
        return;
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    
    // Update coefficients if needed (only when corrections change)
    if (coefficientsNeedUpdate)
    {
        updateBiquadCoefficients();
        coefficientsNeedUpdate = false;
    }
    
    // Process each approved correction
    const size_t numCorrections = juce::jmin(approvedCorrections.size(), static_cast<size_t>(maxCorrections));
    for (size_t corrIdx = 0; corrIdx < numCorrections; ++corrIdx)
    {
        const auto& coeffs = coefficientCache[corrIdx];
        
        // Skip if coefficients are invalid or gain is negligible
        if (!coeffs.valid)
            continue;
        
        // Apply filter to each channel with separate states
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            auto& state = filterStates[corrIdx][ch];
            
            // Direct Form II Transposed biquad filter
            float z1 = state.z1;
            float z2 = state.z2;
            
            for (int i = 0; i < numSamples; ++i)
            {
                float input = channelData[i];
                float output = coeffs.b0 * input + z1;
                z1 = coeffs.b1 * input - coeffs.a1 * output + z2;
                z2 = coeffs.b2 * input - coeffs.a2 * output;
                channelData[i] = output;
            }
            
            // Store state for continuity
            state.z1 = z1;
            state.z2 = z2;
        }
    }
}

//==============================================================================
// Update biquad coefficients cache (called only when corrections change)
// NOTE: This function assumes correctionsMutex is already locked by the caller
void AIEngine::updateBiquadCoefficients()
{
    // Validate sample rate
    if (currentSampleRate <= 0.0)
        return;
    
    const size_t numCorrections = juce::jmin(approvedCorrections.size(), static_cast<size_t>(maxCorrections));
    
    for (size_t corrIdx = 0; corrIdx < numCorrections; ++corrIdx)
    {
        const auto& corr = approvedCorrections[corrIdx];
        auto& coeffs = coefficientCache[corrIdx];
        
        // Get scaled gain
        float gainDB = corr.suggestedGain * strength;
        
        // Skip if gain is negligible
        if (std::abs(gainDB) < minGainThreshold)
        {
            coeffs.valid = false;
            continue;
        }
        
        // Validate frequency (must be within valid range and below Nyquist)
        if (corr.frequency < minFrequency || corr.frequency > currentSampleRate * 0.5f)
        {
            coeffs.valid = false;
            continue;
        }
        
        // Calculate biquad coefficients for peak filter
        float omega = 2.0f * juce::MathConstants<float>::pi * corr.frequency / static_cast<float>(currentSampleRate);
        
        // Clamp omega to prevent numerical issues
        omega = juce::jlimit(0.0f, juce::MathConstants<float>::pi * 0.99f, omega);
        
        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        
        // Clamp Q to prevent division by zero
        float q = juce::jlimit(minQValue, maxQValue, corr.suggestedQ);
        float alpha = sinOmega / (2.0f * q);
        float A = std::pow(10.0f, gainDB / 40.0f);
        
        // Peak filter coefficients
        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosOmega;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosOmega;
        float a2 = 1.0f - alpha / A;
        
        // Normalize coefficients
        if (std::abs(a0) > 1e-10f) // Prevent division by zero
        {
            coeffs.b0 = b0 / a0;
            coeffs.b1 = b1 / a0;
            coeffs.b2 = b2 / a0;
            coeffs.a1 = a1 / a0;
            coeffs.a2 = a2 / a0;
            coeffs.valid = true;
        }
        else
        {
            coeffs.valid = false;
        }
    }
    
    // Invalidate unused correction slots
    for (size_t corrIdx = numCorrections; corrIdx < maxCorrections; ++corrIdx)
    {
        coefficientCache[corrIdx].valid = false;
    }
}

//==============================================================================
// Source Profile Implementation

void AIEngine::setSourceProfile(SourceProfile profile)
{
    sourceProfile = profile;
    applyProfileThresholds();
}

void AIEngine::applyProfileThresholds()
{
    // Reset to defaults
    thresholds = ProfileThresholds();
    
    switch (sourceProfile)
    {
        case SourceProfile::Generic:
            // Default values already set
            break;
            
        case SourceProfile::Vocals:
            // High sibilance sensitivity, focus on boxyness 300-400Hz
            thresholds.sibilanceThreshold = -18.0f;  // More sensitive
            thresholds.boxyLow = 250.0f;
            thresholds.boxyHigh = 600.0f;
            thresholds.harshnessThreshold = -12.0f;
            thresholds.muddinessLow = 200.0f;
            thresholds.muddinessHigh = 350.0f;
            break;
            
        case SourceProfile::Drums:
            // Sub resonances focus, less harshness sensitivity (cymbals ok)
            thresholds.lowEndThreshold = -15.0f;     // More sensitive
            thresholds.harshnessThreshold = -8.0f;   // Less sensitive
            thresholds.resonanceThreshold = 4.0f;    // More sensitive for rings
            thresholds.muddinessLow = 100.0f;
            thresholds.muddinessHigh = 300.0f;
            break;
            
        case SourceProfile::Bass:
            // Focus on sub/low frequencies
            thresholds.lowEndThreshold = -12.0f;
            thresholds.muddinessThreshold = -25.0f;  // More sensitive
            thresholds.muddinessLow = 60.0f;
            thresholds.muddinessHigh = 250.0f;
            thresholds.harshnessThreshold = -5.0f;   // Less sensitive
            thresholds.sibilanceThreshold = -5.0f;   // Ignore highs
            break;
            
        case SourceProfile::Synth:
            // Wide range, resonance focus
            thresholds.resonanceThreshold = 4.0f;    // More sensitive
            thresholds.harshnessThreshold = -12.0f;
            thresholds.boxyLow = 400.0f;
            thresholds.boxyHigh = 1000.0f;
            break;
            
        case SourceProfile::Master:
            // Less severe thresholds for full mix
            thresholds.resonanceThreshold = 8.0f;    // Less sensitive
            thresholds.harshnessThreshold = -10.0f;  // Less sensitive
            thresholds.muddinessThreshold = -18.0f;  // Less sensitive
            thresholds.sibilanceThreshold = -8.0f;   // Less sensitive
            break;
            
        case SourceProfile::EDM:
            // Tolerates more bass/sub, brightness
            thresholds.lowEndThreshold = -5.0f;      // Much less sensitive
            thresholds.harshnessThreshold = -10.0f;  // Less sensitive
            thresholds.muddinessLow = 200.0f;
            thresholds.muddinessHigh = 500.0f;
            thresholds.muddinessThreshold = -15.0f;  // Less sensitive
            break;
    }
}

juce::String AIEngine::getProfileName(SourceProfile profile)
{
    switch (profile)
    {
        case SourceProfile::Generic: return "Generic";
        case SourceProfile::Vocals:  return "Vocals";
        case SourceProfile::Drums:   return "Drums";
        case SourceProfile::Bass:    return "Bass";
        case SourceProfile::Synth:   return "Synth";
        case SourceProfile::Master:  return "Master";
        case SourceProfile::EDM:     return "EDM";
        default: return "Unknown";
    }
}

//==============================================================================
// Corrections Management

std::vector<AIEngine::Correction> AIEngine::getPendingCorrections() const
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    return pendingCorrections;
}

std::vector<AIEngine::Correction> AIEngine::getApprovedCorrections() const
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    return approvedCorrections;
}

void AIEngine::approveCorrection(int index)
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    if (index >= 0 && index < static_cast<int>(pendingCorrections.size()))
    {
        pendingCorrections[index].approved = true;
        approvedCorrections.push_back(pendingCorrections[index]);
        coefficientsNeedUpdate = true; // Coefficients need recalculation
    }
}

void AIEngine::approveAllCorrections()
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    for (auto& c : pendingCorrections)
    {
        c.approved = true;
        approvedCorrections.push_back(c);
    }
    coefficientsNeedUpdate = true; // Coefficients need recalculation
}

void AIEngine::rejectCorrection(int index)
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    if (index >= 0 && index < static_cast<int>(pendingCorrections.size()))
    {
        pendingCorrections.erase(pendingCorrections.begin() + index);
    }
}

void AIEngine::clearCorrections()
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    pendingCorrections.clear();
    approvedCorrections.clear();
    
    // Reset filter states (per-channel, per-correction)
    for (auto& correctionStates : filterStates)
    {
        for (auto& channelState : correctionStates)
        {
            channelState.z1 = 0.0f;
            channelState.z2 = 0.0f;
        }
    }
    
    // Invalidate coefficient cache
    coefficientsNeedUpdate = true;
    for (auto& coeffs : coefficientCache)
    {
        coeffs.valid = false;
    }
}

//==============================================================================
// Analysis History

void AIEngine::saveAnalysisSnapshot()
{
    std::lock_guard<std::mutex> lock(historyMutex);
    
    AnalysisSnapshot snapshot;
    {
        std::lock_guard<std::mutex> cLock(correctionsMutex);
        snapshot.corrections = pendingCorrections;
    }
    snapshot.timestamp = juce::Time::currentTimeMillis();
    snapshot.genre = detectedGenre.load();
    
    analysisHistory.push_back(snapshot);
    
    // Keep only last N snapshots
    while (analysisHistory.size() > static_cast<size_t>(maxHistorySize))
    {
        analysisHistory.erase(analysisHistory.begin());
    }
}

std::vector<AIEngine::AnalysisSnapshot> AIEngine::getAnalysisHistory() const
{
    std::lock_guard<std::mutex> lock(historyMutex);
    return analysisHistory;
}

void AIEngine::clearHistory()
{
    std::lock_guard<std::mutex> lock(historyMutex);
    analysisHistory.clear();
}

//==============================================================================
// Settings Implementation

void AIEngine::setStrength(float s)
{
    float oldStrength = strength;
    strength = juce::jlimit(0.0f, 1.0f, s);
    // If strength changed significantly, coefficients need update
    if (std::abs(strength - oldStrength) > strengthChangeThreshold)
    {
        std::lock_guard<std::mutex> lock(correctionsMutex);
        coefficientsNeedUpdate = true;
    }
}

//==============================================================================
// Scaled Correction (applies strength factor)

AIEngine::Correction AIEngine::getScaledCorrection(const Correction& c) const
{
    Correction scaled = c;
    scaled.suggestedGain = c.suggestedGain * strength;
    return scaled;
}

//==============================================================================
// Problem Detection

void AIEngine::detectProblems()
{
    std::lock_guard<std::mutex> lock(correctionsMutex);
    pendingCorrections.clear();
    
    // Adjust thresholds based on sensitivity (higher sensitivity = lower thresholds)
    float sensitivityFactor = 1.0f - (sensitivity * 0.5f);  // 0.5 to 1.0
    
    float resonanceThresh = thresholds.resonanceThreshold * sensitivityFactor;
    float harshnessThresh = thresholds.harshnessThreshold + (sensitivity * 5.0f);
    float muddinessThresh = thresholds.muddinessThreshold + (sensitivity * 5.0f);
    
    detectResonances(resonanceThresh);
    detectHarshness(harshnessThresh);
    detectMuddiness(muddinessThresh);
    detectBoxyness();
    detectSibilance();
    detectLowEndBoom();
    detectThinSound();
    detectDullSound();
    
    // Sort by severity (highest first)
    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  return a.severity > b.severity;
              });
    
    // Limit to top 8 problems
    if (pendingCorrections.size() > 8)
        pendingCorrections.resize(8);
}

void AIEngine::detectResonances(float threshold)
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    // Early exit if spectrum is empty or too small
    if (currentSpectrum.size() < static_cast<size_t>(numBins))
        return;
    
    // Use temporally smoothed spectrum for more stable detection
    std::vector<float> smoothedSpectrum = getTemporallySmoothedSpectrum();
    
    const float minLevel = minSpectrumLevel + 10.0f;  // Slightly higher to reduce noise
    const int spectrumSize = static_cast<int>(smoothedSpectrum.size());
    
    // Calculate adaptive threshold based on signal level
    float adaptedThreshold = calculateAdaptiveThreshold(threshold);
    
    // Multi-scale window sizes for comprehensive detection
    const std::vector<int> windowSizes = {7, 11, 15, 21};
    
    // Use adaptive window size based on frequency
    auto getAdaptiveWindowSize = [](float freq) -> int {
        if (freq < 150.0f) return 21;      // Sub/bass: wide window
        if (freq < 400.0f) return 15;      // Low-mids
        if (freq < 1500.0f) return 11;     // Mids
        if (freq < 5000.0f) return 9;      // Upper-mids
        return 7;                           // Highs: narrow window
    };
    
    // Parabolic interpolation for precise frequency estimation
    auto parabolicInterpolation = [&smoothedSpectrum, this](int bin) -> float {
        if (bin <= 0 || bin >= static_cast<int>(smoothedSpectrum.size()) - 1)
            return binToFrequency(bin);
        
        float y0 = smoothedSpectrum[bin - 1];
        float y1 = smoothedSpectrum[bin];
        float y2 = smoothedSpectrum[bin + 1];
        
        float denom = 2.0f * (2.0f * y1 - y0 - y2);
        if (std::abs(denom) < 1e-10f)
            return binToFrequency(bin);
        
        float delta = (y0 - y2) / denom;
        delta = juce::jlimit(-0.5f, 0.5f, delta);
        
        return binToFrequency(bin) + delta * (static_cast<float>(currentSampleRate) / static_cast<float>(fftSize));
    };
    
    // First pass: find all potential peaks using multi-scale detection
    std::vector<PeakCandidate> detectedPeaks;
    
    for (int i = 12; i < spectrumSize - 12; ++i)
    {
        float centerMag = smoothedSpectrum[i];
        float freq = binToFrequency(i);
        
        if (centerMag < minLevel)
            continue;
        
        // Check if this is a local maximum (using 3 bins each side)
        bool isLocalMax = true;
        for (int j = 1; j <= 3; ++j)
        {
            if (smoothedSpectrum[i] <= smoothedSpectrum[i - j] || 
                smoothedSpectrum[i] <= smoothedSpectrum[i + j])
            {
                isLocalMax = false;
                break;
            }
        }
        
        if (!isLocalMax)
            continue;
        
        int windowSize = getAdaptiveWindowSize(freq);
        int halfWindow = windowSize / 2;
        
        // Ensure we don't go out of bounds
        int startBin = juce::jmax(0, i - halfWindow);
        int endBin = juce::jmin(spectrumSize - 1, i + halfWindow);
        
        // Calculate weighted average of surrounding bins (excluding center region)
        float surroundSum = 0.0f;
        float weightSum = 0.0f;
        for (int j = startBin; j <= endBin; ++j)
        {
            if (std::abs(j - i) >= 2)  // Exclude center ±1 bins
            {
                float weight = 1.0f - static_cast<float>(std::abs(j - i)) / static_cast<float>(halfWindow);
                weight = juce::jmax(0.3f, weight);  // Minimum weight
                surroundSum += smoothedSpectrum[j] * weight;
                weightSum += weight;
            }
        }
        
        if (weightSum < 0.1f) continue;
        float surroundAvg = surroundSum / weightSum;
        float peakHeight = centerMag - surroundAvg;
        
        // Multi-level sensitivity response
        // Creates a more nuanced sensitivity curve with three zones
        float sensitivityFactor;
        if (sensitivity < 0.3f)
        {
            // Low sensitivity: only catch obvious problems
            sensitivityFactor = 1.3f - sensitivity * 0.5f;  // 1.3 to 1.15
        }
        else if (sensitivity < 0.7f)
        {
            // Medium sensitivity: balanced detection
            sensitivityFactor = 1.15f - (sensitivity - 0.3f) * 0.75f;  // 1.15 to 0.85
        }
        else
        {
            // High sensitivity: catch subtle issues
            sensitivityFactor = 0.85f - (sensitivity - 0.7f) * 1.0f;  // 0.85 to 0.55
        }
        
        float effectiveThreshold = adaptedThreshold * sensitivityFactor;
        
        if (peakHeight > effectiveThreshold)
        {
            PeakCandidate peak;
            peak.bin = i;
            peak.frequency = parabolicInterpolation(i);
            peak.magnitude = centerMag;
            peak.peakHeight = peakHeight;
            peak.bandwidth = calculateBandwidth(i);
            peak.calculatedQ = bandwidthToQ(peak.frequency, peak.bandwidth);
            detectedPeaks.push_back(peak);
        }
    }
    
    // Update persistent peaks for temporal stability
    updatePersistentPeaks(detectedPeaks);
    
    // Create corrections from persistent peaks (require 2+ frames for stability)
    for (const auto& peak : persistentPeaks)
    {
        // Require persistence to reduce false positives
        if (peak.frameCount < 2)
            continue;
        
        // Skip sub-bass resonances (often intentional)
        if (peak.frequency < 35.0f)
            continue;
        
        // Check if we already have a nearby resonance
        bool tooClose = false;
        for (const auto& c : pendingCorrections)
        {
            if (c.type == ProblemType::Resonance)
            {
                float ratio = peak.frequency / c.frequency;
                if (ratio > 0.90f && ratio < 1.10f)
                {
                    tooClose = true;
                    break;
                }
            }
        }
        
        if (!tooClose)
        {
            Correction c;
            c.type = ProblemType::Resonance;
            c.frequency = peak.frequency;
            
            // Calculate suggested gain based on peak height and sensitivity
            float gainFactor = 0.55f + sensitivity * 0.25f;  // 0.55 to 0.80
            c.suggestedGain = -peak.peakHeight * gainFactor;
            c.suggestedGain = juce::jlimit(-18.0f, -1.0f, c.suggestedGain);
            
            // Use calculated Q from actual bandwidth measurement
            c.suggestedQ = peak.calculatedQ;
            c.suggestedQ = juce::jlimit(1.0f, 15.0f, c.suggestedQ);
            
            // Severity based on peak height and persistence
            float heightSeverity = juce::jlimit(0.0f, 1.0f, peak.peakHeight / 10.0f);
            float persistSeverity = juce::jlimit(0.0f, 0.3f, static_cast<float>(peak.frameCount - 1) * 0.1f);
            c.severity = juce::jmin(1.0f, heightSeverity + persistSeverity);
            
            // Confidence based on peak prominence, level, and persistence
            float levelConfidence = juce::jlimit(0.0f, 1.0f, (peak.magnitude + 60.0f) / 50.0f);
            float heightConfidence = juce::jlimit(0.0f, 1.0f, peak.peakHeight / 8.0f);
            float persistConfidence = juce::jlimit(0.0f, 0.2f, static_cast<float>(peak.frameCount - 1) * 0.05f);
            c.confidence = juce::jmin(1.0f, levelConfidence * 0.3f + heightConfidence * 0.5f + persistConfidence);
            
            // Detailed description with bandwidth info
            juce::String bandName = getBandName(peak.frequency);
            c.description = juce::String::formatted("Resonant peak at %.1f Hz (%s) - %.1f dB above surroundings, BW: %.0f Hz, Q: %.1f",
                                                     peak.frequency, bandName.toRawUTF8(), peak.peakHeight, 
                                                     peak.bandwidth, peak.calculatedQ);
            
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectHarshness(float threshold)
{
    // Use lock-free access pattern - calculateBandEnergy already locks
    float energy = calculateBandEnergy(thresholds.harshnessLow, thresholds.harshnessHigh);
    float overallEnergy = calculateBandEnergy(200.0f, 15000.0f);
    float relativeEnergy = energy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.0f * sensitivityMultiplier;
    float adaptedThreshold = calculateAdaptiveThreshold(threshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && energy > adaptedThreshold)
    {
        // Find the peak frequency within the harshness range
        float peakFreq = findPeakInRange(thresholds.harshnessLow, thresholds.harshnessHigh);
        
        Correction c;
        c.type = ProblemType::Harshness;
        c.frequency = peakFreq > 0 ? peakFreq : 3500.0f;
        c.suggestedGain = -(relativeEnergy * (0.45f + sensitivity * 0.25f));
        c.suggestedGain = juce::jlimit(-12.0f, -1.0f, c.suggestedGain);
        c.suggestedQ = 0.7f + (relativeEnergy * 0.08f);  // Wider Q for broad harshness
        c.suggestedQ = juce::jlimit(0.4f, 2.5f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 7.0f);
        c.confidence = 0.70f + (sensitivity * 0.20f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Harshness centered at %.0f Hz (%s) - %.1f dB above average, causing ear fatigue",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectMuddiness(float threshold)
{
    float lowMidEnergy = calculateBandEnergy(thresholds.muddinessLow, thresholds.muddinessHigh);
    float overallEnergy = calculateBandEnergy(100.0f, 10000.0f);
    float relativeEnergy = lowMidEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.0f * sensitivityMultiplier;
    float adaptedThreshold = calculateAdaptiveThreshold(threshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && lowMidEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the muddiness range
        float peakFreq = findPeakInRange(thresholds.muddinessLow, thresholds.muddinessHigh);
        
        Correction c;
        c.type = ProblemType::Muddiness;
        c.frequency = peakFreq > 0 ? peakFreq : (thresholds.muddinessLow + thresholds.muddinessHigh) / 2.0f;
        c.suggestedGain = -(relativeEnergy * (0.35f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-10.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.6f + (relativeEnergy * 0.04f);  // Wide Q for broad muddiness
        c.suggestedQ = juce::jlimit(0.4f, 1.5f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 6.0f);
        c.confidence = 0.65f + (sensitivity * 0.20f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Low-mid buildup at %.0f Hz (%s) - %.1f dB excess masking clarity",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectBoxyness()
{
    float boxEnergy = calculateBandEnergy(thresholds.boxyLow, thresholds.boxyHigh);
    float overallEnergy = calculateBandEnergy(100.0f, 8000.0f);
    float relativeEnergy = boxEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.5f * sensitivityMultiplier;
    float adaptedThreshold = calculateAdaptiveThreshold(thresholds.boxyThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && boxEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the boxyness range
        float peakFreq = findPeakInRange(thresholds.boxyLow, thresholds.boxyHigh);
        
        Correction c;
        c.type = ProblemType::Boxyness;
        c.frequency = peakFreq > 0 ? peakFreq : (thresholds.boxyLow + thresholds.boxyHigh) / 2.0f;
        c.suggestedGain = -(relativeEnergy * (0.40f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-9.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.9f + (relativeEnergy * 0.06f);
        c.suggestedQ = juce::jlimit(0.6f, 3.0f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 8.0f);
        c.confidence = 0.60f + (sensitivity * 0.25f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Boxy resonance at %.0f Hz (%s) - +%.1f dB cardboard-like coloration",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectSibilance()
{
    float sibilanceEnergy = calculateBandEnergy(thresholds.sibilanceLow, thresholds.sibilanceHigh);
    float midEnergy = calculateBandEnergy(2000.0f, 5000.0f);
    float relativeEnergy = sibilanceEnergy - midEnergy;
    
    // Use exponential sensitivity curve - sibilance is very sensitivity-dependent
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 2.0f * sensitivityMultiplier;
    float adaptedThreshold = calculateAdaptiveThreshold(thresholds.sibilanceThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && sibilanceEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the sibilance range
        float peakFreq = findPeakInRange(thresholds.sibilanceLow, thresholds.sibilanceHigh);
        
        Correction c;
        c.type = ProblemType::Sibilance;
        c.frequency = peakFreq > 0 ? peakFreq : 7000.0f;
        c.suggestedGain = -(relativeEnergy * (0.45f + sensitivity * 0.25f));
        c.suggestedGain = juce::jlimit(-12.0f, -1.0f, c.suggestedGain);
        c.suggestedQ = 1.0f + (relativeEnergy * 0.12f);
        c.suggestedQ = juce::jlimit(0.7f, 4.0f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 6.0f);
        c.confidence = 0.70f + (sensitivity * 0.20f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Sibilance peak at %.0f Hz (%s) - harsh S/T/F sounds +%.1f dB above presence",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectLowEndBoom()
{
    float subEnergy = calculateBandEnergy(30.0f, 100.0f);
    float overallEnergy = calculateBandEnergy(100.0f, 5000.0f);
    float relativeEnergy = subEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 5.0f * sensitivityMultiplier;
    float adaptedThreshold = calculateAdaptiveThreshold(thresholds.lowEndThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && subEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the sub-bass range
        float peakFreq = findPeakInRange(30.0f, 100.0f);
        
        Correction c;
        c.type = ProblemType::LowEndBoom;
        c.frequency = peakFreq > 0 ? peakFreq : 60.0f;
        c.suggestedGain = -(relativeEnergy * (0.30f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-10.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.5f + (relativeEnergy * 0.02f);  // Wide Q for low frequencies
        c.suggestedQ = juce::jlimit(0.4f, 1.2f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 9.0f);
        c.confidence = 0.60f + (sensitivity * 0.25f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Sub-bass buildup at %.0f Hz (%s) - +%.1f dB above mix, causing rumble/masking",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectThinSound()
{
    float lowMidEnergy = calculateBandEnergy(200.0f, 600.0f);
    float highEnergy = calculateBandEnergy(2000.0f, 8000.0f);
    float relativeEnergy = highEnergy - lowMidEnergy;
    
    // Use exponential sensitivity curve for subtle issues
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 7.0f * sensitivityMultiplier;
    
    // Also check overall signal level (don't flag thin sound in quiet signals)
    if (averageRMS > -50.0f && relativeEnergy > adjustedRelativeThreshold)
    {
        // Find where the deficiency is most pronounced
        float deficientFreq = findLowestInRange(200.0f, 600.0f);
        
        Correction c;
        c.type = ProblemType::ThinSound;
        c.frequency = deficientFreq > 0 ? deficientFreq : 350.0f;
        c.suggestedGain = relativeEnergy * (0.22f + sensitivity * 0.12f);  // Boost, not cut
        c.suggestedGain = juce::jlimit(0.5f, 6.0f, c.suggestedGain);
        c.suggestedQ = 0.6f + (relativeEnergy * 0.02f);  // Wide shelf-like boost
        c.suggestedQ = juce::jlimit(0.4f, 1.2f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 10.0f);
        c.confidence = 0.50f + (sensitivity * 0.25f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted("Thin/weak sound - low-mids at %.0f Hz (%s) are %.1f dB below highs, lacks body",
                                                c.frequency, bandName.toRawUTF8(), relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

void AIEngine::detectDullSound()
{
    float highEnergy = calculateBandEnergy(8000.0f, 16000.0f);
    float midEnergy = calculateBandEnergy(1000.0f, 4000.0f);
    float relativeEnergy = midEnergy - highEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 9.0f * sensitivityMultiplier;
    
    // Also check overall signal level
    if (averageRMS > -50.0f && relativeEnergy > adjustedRelativeThreshold)
    {
        // Find where to apply the boost
        float airFreq = findLowestInRange(8000.0f, 14000.0f);
        
        Correction c;
        c.type = ProblemType::DullSound;
        c.frequency = airFreq > 0 ? airFreq : 10000.0f;
        c.suggestedGain = relativeEnergy * (0.16f + sensitivity * 0.10f);  // Boost, not cut
        c.suggestedGain = juce::jlimit(0.5f, 6.0f, c.suggestedGain);
        c.suggestedQ = 0.5f + (relativeEnergy * 0.015f);  // Very wide high shelf
        c.suggestedQ = juce::jlimit(0.3f, 1.0f, c.suggestedQ);
        c.severity = juce::jlimit(0.0f, 1.0f, relativeEnergy / 12.0f);
        c.confidence = 0.45f + (sensitivity * 0.30f);
        
        c.description = juce::String::formatted("Dull/lifeless sound - high frequencies at %.0f Hz are %.1f dB below mids, lacks air and sparkle",
                                                c.frequency, relativeEnergy);
        
        pendingCorrections.push_back(c);
    }
}

//==============================================================================
// FIX: detectGenre() - Uses internal calculateBandEnergyUnlocked to avoid deadlock
void AIEngine::detectGenre()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    // Use unlocked version since we already hold the mutex
    float subBass = calculateBandEnergyUnlocked(20.0f, 60.0f);
    float bass = calculateBandEnergyUnlocked(60.0f, 200.0f);
    float lowMid = calculateBandEnergyUnlocked(200.0f, 500.0f);
    float mid = calculateBandEnergyUnlocked(500.0f, 2000.0f);
    float highMid = calculateBandEnergyUnlocked(2000.0f, 6000.0f);
    float high = calculateBandEnergyUnlocked(6000.0f, 16000.0f);
    
    DetectedGenre detected = DetectedGenre::Unknown;
    
    // Improved heuristics using ALL bands including lowMid
    if (subBass > bass + 3.0f && high > mid)
    {
        // Dub Techno: deep sub, spacious mids, present highs
        detected = DetectedGenre::DubTechno;
    }
    else if (highMid > mid + 2.0f && bass > subBass && lowMid < mid)
    {
        // Industrial: aggressive mids, harsh high-mids, scooped low-mids
        detected = DetectedGenre::Industrial;
    }
    else if (high > highMid + 3.0f && bass > subBass + 2.0f && lowMid > mid - 5.0f)
    {
        // Jungle: crisp highs, punchy bass, present low-mids for warmth
        detected = DetectedGenre::Jungle;
    }
    else if (bass > mid && subBass < bass - 3.0f && lowMid > subBass)
    {
        // Breakbeat: bass focus, controlled sub, warm low-mids
        detected = DetectedGenre::Breakbeat;
    }
    else if (subBass > mid && high < mid - 3.0f && lowMid > bass - 2.0f)
    {
        // Deep House: deep sub, warm low-mids, rolled-off highs
        detected = DetectedGenre::DeepHouse;
    }
    else if (mid > bass && high < mid - 5.0f && lowMid > highMid)
    {
        // Ambient: mid focus, warm low-mids dominate over high-mids
        detected = DetectedGenre::Ambient;
    }
    else if (bass > mid && highMid > mid && lowMid < mid)
    {
        // Techno: bass + high-mids, scooped low-mids for clarity
        detected = DetectedGenre::Techno;
    }
    
    detectedGenre = detected;
}

//==============================================================================
// Utility Functions

// Find the peak frequency within a given range (thread-safe)
float AIEngine::findPeakInRange(float lowFreq, float highFreq)
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    int lowBin = frequencyToBin(lowFreq);
    int highBin = frequencyToBin(highFreq);
    
    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);
    
    if (highBin <= lowBin || currentSpectrum.empty())
        return -1.0f;
    
    float maxMag = -200.0f;
    int maxBin = lowBin;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < static_cast<int>(currentSpectrum.size()))
        {
            if (currentSpectrum[i] > maxMag)
            {
                maxMag = currentSpectrum[i];
                maxBin = i;
            }
        }
    }
    
    // Apply parabolic interpolation for precise frequency
    if (maxBin > 0 && maxBin < static_cast<int>(currentSpectrum.size()) - 1)
    {
        float y0 = currentSpectrum[maxBin - 1];
        float y1 = currentSpectrum[maxBin];
        float y2 = currentSpectrum[maxBin + 1];
        
        float denom = 2.0f * (2.0f * y1 - y0 - y2);
        if (std::abs(denom) > 1e-10f)
        {
            float delta = (y0 - y2) / denom;
            delta = juce::jlimit(-0.5f, 0.5f, delta);
            return binToFrequency(maxBin) + delta * (static_cast<float>(currentSampleRate) / static_cast<float>(fftSize));
        }
    }
    
    return binToFrequency(maxBin);
}

// Find the lowest energy frequency within a given range (thread-safe)
float AIEngine::findLowestInRange(float lowFreq, float highFreq)
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    
    int lowBin = frequencyToBin(lowFreq);
    int highBin = frequencyToBin(highFreq);
    
    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);
    
    if (highBin <= lowBin || currentSpectrum.empty())
        return -1.0f;
    
    float minMag = 100.0f;
    int minBin = lowBin;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < static_cast<int>(currentSpectrum.size()))
        {
            if (currentSpectrum[i] < minMag)
            {
                minMag = currentSpectrum[i];
                minBin = i;
            }
        }
    }
    
    return binToFrequency(minBin);
}

// Thread-safe version - acquires lock
float AIEngine::calculateBandEnergy(float lowFreq, float highFreq)
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    return calculateBandEnergyUnlocked(lowFreq, highFreq);
}

// Internal version - caller must hold spectrumMutex
float AIEngine::calculateBandEnergyUnlocked(float lowFreq, float highFreq) const
{
    int lowBin = frequencyToBin(lowFreq);
    int highBin = frequencyToBin(highFreq);
    
    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);
    
    if (highBin <= lowBin)
        return -100.0f;
    
    // Ensure we don't exceed spectrum size
    const size_t spectrumSize = currentSpectrum.size();
    if (spectrumSize == 0)
        return -100.0f;
    
    highBin = juce::jmin(highBin, static_cast<int>(spectrumSize) - 1);
    
    float sum = 0.0f;
    int count = 0;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < static_cast<int>(spectrumSize))
        {
            sum += currentSpectrum[i];
            ++count;
        }
    }
    
    return count > 0 ? sum / static_cast<float>(count) : -100.0f;
}

float AIEngine::binToFrequency(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
}

int AIEngine::frequencyToBin(float frequency) const
{
    int bin = static_cast<int>(frequency * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    return juce::jlimit(0, numBins - 1, bin);
}

juce::String AIEngine::getProblemTypeName(ProblemType type)
{
    switch (type)
    {
        case ProblemType::None:       return "None";
        case ProblemType::Resonance:  return "Resonance";
        case ProblemType::Harshness:  return "Harshness";
        case ProblemType::Muddiness:  return "Muddiness";
        case ProblemType::Boxyness:   return "Boxyness";
        case ProblemType::Sibilance:  return "Sibilance";
        case ProblemType::LowEndBoom: return "Low-End Boom";
        case ProblemType::ThinSound:  return "Thin Sound";
        case ProblemType::DullSound:  return "Dull Sound";
        default: return "Unknown";
    }
}

juce::String AIEngine::getGenreName(DetectedGenre genre)
{
    switch (genre)
    {
        case DetectedGenre::Unknown:   return "Unknown";
        case DetectedGenre::Techno:    return "Techno";
        case DetectedGenre::Industrial: return "Industrial";
        case DetectedGenre::Jungle:    return "Jungle";
        case DetectedGenre::Breakbeat: return "Breakbeat";
        case DetectedGenre::DubTechno: return "Dub Techno";
        case DetectedGenre::DeepHouse: return "Deep House";
        case DetectedGenre::Ambient:   return "Ambient";
        default: return "Unknown";
    }
}

// Helper: Get human-readable band name for a frequency
juce::String AIEngine::getBandName(float freq)
{
    if (freq < 30.0f)       return "Sub";
    if (freq < 60.0f)       return "Sub-Bass";
    if (freq < 120.0f)      return "Bass";
    if (freq < 250.0f)      return "Low-Bass";
    if (freq < 500.0f)      return "Low-Mids";
    if (freq < 1000.0f)     return "Mids";
    if (freq < 2000.0f)     return "Upper-Mids";
    if (freq < 4000.0f)     return "Presence";
    if (freq < 8000.0f)     return "Brilliance";
    if (freq < 12000.0f)    return "Air";
    return "Ultra-Highs";
}

//==============================================================================
// Enhanced Detection v2.0 - Helper Functions
//==============================================================================

void AIEngine::updateSpectrumHistory(const std::vector<float>& spectrum)
{
    // Store spectrum in circular buffer
    if (historyWriteIndex >= 0 && historyWriteIndex < temporalFrames)
    {
        spectrumHistory[historyWriteIndex] = spectrum;
        historyWriteIndex = (historyWriteIndex + 1) % temporalFrames;
    }
}

std::vector<float> AIEngine::getTemporallySmoothedSpectrum() const
{
    std::vector<float> smoothed(numBins, -100.0f);
    
    // Average across temporal frames with weighting (newer = more weight)
    const float weights[3] = { 0.2f, 0.3f, 0.5f };  // Oldest to newest
    
    for (int bin = 0; bin < numBins; ++bin)
    {
        float sum = 0.0f;
        float weightSum = 0.0f;
        
        for (int frame = 0; frame < temporalFrames; ++frame)
        {
            if (!spectrumHistory[frame].empty() && 
                bin < static_cast<int>(spectrumHistory[frame].size()) &&
                spectrumHistory[frame][bin] > -99.0f)
            {
                // Adjust weight index based on age
                int ageIndex = (historyWriteIndex - 1 - frame + temporalFrames) % temporalFrames;
                ageIndex = juce::jlimit(0, 2, ageIndex);
                
                sum += spectrumHistory[frame][bin] * weights[ageIndex];
                weightSum += weights[ageIndex];
            }
        }
        
        if (weightSum > 0.0f)
            smoothed[bin] = sum / weightSum;
    }
    
    return smoothed;
}

float AIEngine::calculateAdaptiveThreshold(float baseThreshold) const
{
    // Adjust threshold based on signal level
    // Louder signals need higher thresholds to avoid false positives
    // Quieter signals need lower thresholds to catch subtle issues
    float rmsOffset = (averageRMS - (-40.0f)) * 0.15f;  // Reference is -40 dB
    float adaptedThreshold = baseThreshold + rmsOffset;
    
    // Apply sensitivity with exponential curve
    float sensMultiplier = getSensitivityMultiplier();
    return adaptedThreshold * sensMultiplier;
}

float AIEngine::getSensitivityMultiplier() const
{
    // Exponential curve: sensitivity 0.0 = 1.5x threshold, 1.0 = 0.4x threshold
    // This gives much finer control than linear
    // y = 1.5 * exp(-1.32 * x)  where x is sensitivity
    float exponent = -1.32f * sensitivity;
    return 1.5f * std::exp(exponent);
}

float AIEngine::calculateBandwidth(int peakBin) const
{
    // Find -3dB points on either side of peak
    if (peakBin < 2 || peakBin >= static_cast<int>(currentSpectrum.size()) - 2)
        return 100.0f;  // Default fallback
    
    float peakMag = currentSpectrum[peakBin];
    float threshold3dB = peakMag - 3.0f;
    
    // Search left for -3dB point
    int leftBin = peakBin;
    for (int i = peakBin - 1; i >= 0 && i >= peakBin - 50; --i)
    {
        if (i < static_cast<int>(currentSpectrum.size()) && currentSpectrum[i] < threshold3dB)
        {
            leftBin = i;
            break;
        }
        leftBin = i;
    }
    
    // Search right for -3dB point
    int rightBin = peakBin;
    for (int i = peakBin + 1; i < static_cast<int>(currentSpectrum.size()) && i <= peakBin + 50; ++i)
    {
        if (currentSpectrum[i] < threshold3dB)
        {
            rightBin = i;
            break;
        }
        rightBin = i;
    }
    
    // Convert bin width to Hz
    float leftFreq = binToFrequency(leftBin);
    float rightFreq = binToFrequency(rightBin);
    
    return std::max(10.0f, rightFreq - leftFreq);  // Minimum 10 Hz bandwidth
}

float AIEngine::bandwidthToQ(float frequency, float bandwidth) const
{
    // Q = f0 / bandwidth
    if (bandwidth < 1.0f)
        return 20.0f;  // Maximum Q for very narrow
    return juce::jlimit(0.3f, 20.0f, frequency / bandwidth);
}

void AIEngine::updatePersistentPeaks(const std::vector<PeakCandidate>& newPeaks)
{
    // Update existing peaks or add new ones
    for (const auto& newPeak : newPeaks)
    {
        bool found = false;
        for (auto& existing : persistentPeaks)
        {
            // Check if this is the same peak (within 5% frequency)
            float ratio = newPeak.frequency / existing.frequency;
            if (ratio > 0.95f && ratio < 1.05f)
            {
                // Update existing peak
                existing.magnitude = newPeak.magnitude;
                existing.peakHeight = (existing.peakHeight + newPeak.peakHeight) * 0.5f;  // Smooth
                existing.bandwidth = (existing.bandwidth + newPeak.bandwidth) * 0.5f;
                existing.calculatedQ = newPeak.calculatedQ;
                existing.frameCount++;
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            // Add new peak
            PeakCandidate peak = newPeak;
            peak.frameCount = 1;
            persistentPeaks.push_back(peak);
        }
    }
    
    // Decay peaks that weren't detected this frame
    for (auto it = persistentPeaks.begin(); it != persistentPeaks.end();)
    {
        bool foundInNew = false;
        for (const auto& newPeak : newPeaks)
        {
            float ratio = newPeak.frequency / it->frequency;
            if (ratio > 0.95f && ratio < 1.05f)
            {
                foundInNew = true;
                break;
            }
        }
        
        if (!foundInNew)
        {
            it->frameCount--;
            if (it->frameCount <= 0)
            {
                it = persistentPeaks.erase(it);
                continue;
            }
        }
        ++it;
    }
    
    // Limit to max peaks
    if (persistentPeaks.size() > 16)
    {
        // Sort by peak height and keep top 16
        std::sort(persistentPeaks.begin(), persistentPeaks.end(),
            [](const PeakCandidate& a, const PeakCandidate& b) {
                return a.peakHeight > b.peakHeight;
            });
        persistentPeaks.resize(16);
    }
}

//==============================================================================
// ML-Enhanced Problem Detection
//==============================================================================
void AIEngine::detectProblemsWithML()
{
    std::vector<float> spectrumCopy;
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        spectrumCopy = currentSpectrum;
    }
    
    if (spectrumCopy.empty())
        return;
    
    // Set ML context based on source profile
    MLEngine::GenreType mlContext = MLEngine::GenreType::Unknown;
    switch (sourceProfile)
    {
        case SourceProfile::Vocals:  mlContext = MLEngine::GenreType::Vocals; break;
        case SourceProfile::Drums:   mlContext = MLEngine::GenreType::Drums; break;
        case SourceProfile::Bass:    mlContext = MLEngine::GenreType::Bass; break;
        case SourceProfile::Synth:   mlContext = MLEngine::GenreType::Synth; break;
        case SourceProfile::Master:  mlContext = MLEngine::GenreType::Master; break;
        case SourceProfile::EDM:     mlContext = MLEngine::GenreType::EDM; break;
        default:                     mlContext = MLEngine::GenreType::Unknown; break;
    }
    mlEngine.setContext(mlContext);
    mlEngine.setSensitivity(sensitivity);
    
    // Run ML detection
    auto mlDetections = mlEngine.detectProblems(spectrumCopy, currentSampleRate);
    
    // Convert ML detections to AIEngine corrections
    std::lock_guard<std::mutex> lock(correctionsMutex);
    pendingCorrections.clear();
    
    for (const auto& mlDet : mlDetections)
    {
        Correction c;
        
        // Map ML problem type to AIEngine problem type
        switch (mlDet.type)
        {
            case MLEngine::ProblemType::Resonance:
                c.type = ProblemType::Resonance;
                break;
            case MLEngine::ProblemType::Harshness:
                c.type = ProblemType::Harshness;
                break;
            case MLEngine::ProblemType::Muddiness:
                c.type = ProblemType::Muddiness;
                break;
            case MLEngine::ProblemType::Sibilance:
                c.type = ProblemType::Sibilance;
                break;
            case MLEngine::ProblemType::Boominess:
                c.type = ProblemType::LowEndBoom;
                break;
            case MLEngine::ProblemType::Thinness:
                c.type = ProblemType::ThinSound;
                break;
            case MLEngine::ProblemType::BoxyMidrange:
                c.type = ProblemType::Boxyness;
                break;
            case MLEngine::ProblemType::Clipping:
                c.type = ProblemType::Harshness; // Map clipping to harshness for now
                break;
            default:
                c.type = ProblemType::None;
                break;
        }
        
        if (c.type == ProblemType::None)
            continue;
        
        c.frequency = mlDet.frequency;
        c.suggestedGain = mlDet.suggestedGain;
        c.suggestedQ = mlDet.suggestedQ;
        c.severity = mlDet.severity;
        c.confidence = mlDet.confidence;
        c.approved = false;
        
        // Generate description
        c.description = juce::String::formatted("%s at %.0f Hz (ML confidence: %.0f%%)",
                                                 getProblemTypeName(c.type).toRawUTF8(),
                                                 c.frequency,
                                                 c.confidence * 100.0f);
        
        pendingCorrections.push_back(c);
    }
    
    // Also run heuristic detection to catch anything ML might miss
    // and combine results
    detectResonances(thresholds.resonanceThreshold * (1.0f - sensitivity * 0.5f));
    
    // Remove duplicates (same type within 10% frequency range)
    auto it = std::unique(pendingCorrections.begin(), pendingCorrections.end(),
        [](const Correction& a, const Correction& b) {
            if (a.type != b.type)
                return false;
            float ratio = a.frequency / b.frequency;
            return ratio > 0.9f && ratio < 1.1f;
        });
    pendingCorrections.erase(it, pendingCorrections.end());
    
    // Sort by severity (highest first)
    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  return a.severity > b.severity;
              });
    
    // Limit to top 8 problems
    if (pendingCorrections.size() > 8)
        pendingCorrections.resize(8);
}
