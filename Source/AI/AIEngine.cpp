#include "AIEngine.h"
#include "DisplayedProblemIdentity.h"
#include <array>
#include <cmath>
#include <map>
#include <algorithm>
#include "../Utils/Logger.h"

namespace
{
juce::File findPackagedModel(const juce::String& fileName)
{
    const auto applicationFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    const auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const std::array<juce::File, 8> candidates {{
        applicationFile.getParentDirectory().getSiblingFile("Resources").getChildFile("models").getChildFile(fileName),
        applicationFile.getChildFile("Contents").getChildFile("Resources").getChildFile("models").getChildFile(fileName),
        executableFile.getParentDirectory().getSiblingFile("Resources").getChildFile("models").getChildFile(fileName),
        executableFile.getChildFile("Contents").getChildFile("Resources").getChildFile("models").getChildFile(fileName),
        applicationFile.getParentDirectory().getChildFile("models").getChildFile(fileName),
        executableFile.getParentDirectory().getChildFile("models").getChildFile(fileName),
        applicationFile.getSiblingFile(fileName),
        executableFile.getSiblingFile(fileName)
    }};

    for (const auto& candidate : candidates)
        if (candidate.existsAsFile())
            return candidate;

    return candidates.front();
}
}

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
    normalizedBuffer.resize(numBins, -100.0f);

    // EXPLICITLY initialize triple-buffer to prevent uninitialized access
    for (auto& buf : spectrumBuffers)
    {
        buf.bins.fill(-100.0f);
        buf.rmsLevel = -60.0f;
        buf.avgRmsLevel = -40.0f;
        buf.version = 0;
    }
    
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
    for (auto& cache : cachedCoeffs)
    {
        cache.numActive = 0;
        cache.enabled.fill(false);
    }
    
    // Initialize ML Engine
    mlEngine.initialize();
    
    // Advanced AI systems are lazy-initialized when enabled via their respective setters.
    // This avoids allocating unused subsystems and reduces startup time / memory footprint.
    // See setMultiTrackUnmaskingEnabled(), setNeuralNetworksEnabled(), etc.
    
    // Initialize spectrum history for temporal smoothing
    spectrumHistory.resize(temporalFrames);
    for (auto& frame : spectrumHistory)
    {
        frame.resize(numBins, -100.0f);
    }
    historyWriteIndex = 0;
}

// NOTE: Triple-buffer methods were removed - using mutex-protected spectrum instead

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
    
    // Invalidate cached coeffs
    for (auto& cache : cachedCoeffs)
    {
        cache.numActive = 0;
        cache.enabled.fill(false);
    }
    correctionCoeffsNeedUpdate.store(true);

    // Load MLEQ v1 weights if available (auto-enables ML detection).
    // If a test has already force-loaded weights via setCustomMLWeightsPathForTests(),
    // skip the runtime path search.
    if (!useMLDetection)
    {
        const auto mlModelPath = findPackagedModel("ml_weights.bin");
        if (mlModelPath.existsAsFile())
        {
            if (mlEngine.loadWeights(mlModelPath))
            {
                useMLDetection = true;
                mlBackendStatus.store(static_cast<int>(MLBackendStatus::Active), std::memory_order_relaxed);
                AIEQ_LOG_INFO("ML model loaded: " + mlEngine.getLoadedWeightsPath()
                              + " (" + juce::String(mlEngine.getLoadedWeightsBytes()) + " bytes, fnv64 "
                              + mlEngine.getLoadedWeightsChecksum() + ")");
            }
            else
            {
                mlBackendStatus.store(static_cast<int>(MLBackendStatus::LoadFailed), std::memory_order_relaxed);
                AIEQ_LOG_WARNING("Failed to load ML model: " + mlModelPath.getFullPathName()
                                 + " - using heuristic fallback. ML/Hybrid backend modes will run "
                                   "the heuristic path until valid weights are present.");
            }
        }
        else
        {
            // Previously this was a SILENT no-op: with no weights next to the
            // binary, useMLDetection stayed false and ML/Hybrid silently became
            // heuristic with no warning. Make the degradation explicit.
            mlBackendStatus.store(static_cast<int>(MLBackendStatus::WeightsMissing), std::memory_order_relaxed);
            AIEQ_LOG_WARNING("ML weights not found at " + mlModelPath.getFullPathName()
                             + " - ML/Hybrid backend modes will run the heuristic path. "
                               "Ship Contents/Resources/models/ml_weights.bin to enable ML detection.");
        }
    }
    else
    {
        // Weights were force-loaded before prepare() (e.g. test hook); reflect that.
        mlBackendStatus.store(static_cast<int>(MLBackendStatus::Active), std::memory_order_relaxed);
    }

    // Attempt to load TFLite model if enabled and not already loaded
    if (enableNeuralNetworks && neuralNetwork && !neuralNetwork->isModelLoaded())
    {
        const auto modelPath = findPackagedModel("problem_detection.tflite");
        if (modelPath.existsAsFile())
        {
            if (neuralNetwork->loadModel(modelPath, NeuralNetworkWrapper::ModelType::ProblemDetection))
            {
                AIEQ_LOG_INFO("TFLite model loaded: " + modelPath.getFullPathName());
            }
            else
            {
                AIEQ_LOG_WARNING("Failed to load TFLite model: " + modelPath.getFullPathName()
                                 + " - using classical ML fallback.");
            }
        }
        else
        {
            AIEQ_LOG_WARNING("TFLite model not found at " + modelPath.getFullPathName()
                             + " - using classical ML fallback.");
        }
    }
}

void AIEngine::resetLiveDetectionState()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    pendingCorrections.clear();
    pendingEvidence.clear();
    detectionHistory.clear();
    analysisCounter = 0;
    newAnalysisAvailable.store(false, std::memory_order_relaxed);
}

void AIEngine::analyzeSpectrum(const std::vector<float>& spectrum, bool force)
{
    // Lazy-apply profile thresholds on the AI thread (avoids data race with audio thread).
    // Only re-applies when the profile has actually changed.
    const int currentProfile = sourceProfile.load(std::memory_order_acquire);
    const int appliedProfile = lastAppliedProfile.load(std::memory_order_relaxed);
    if (currentProfile != appliedProfile)
    {
        applyProfileThresholds();
        lastAppliedProfile.store(currentProfile, std::memory_order_relaxed);
    }

    // FORCE: Always run detection, even if spectrum is empty or disabled
    // (detectProblems will create test problem if no real problems found)

    if (spectrum.empty())
    {
        // Spectrum empty - still run detection to create test problem
        detectProblems();  // This will create test problem
        newAnalysisAvailable = true;
        return;
    }

    if (!isEnabled() && !force)
        return;  // Only skip if disabled AND not forced
    
    // Reuse pre-allocated buffer to avoid heap allocation per call
    auto& normalized = normalizedBuffer;
    if (static_cast<int>(spectrum.size()) == numBins)
    {
        std::copy(spectrum.begin(), spectrum.end(), normalized.begin());
    }
    else
    {
        std::fill(normalized.begin(), normalized.end(), -100.0f);
        if (spectrum.size() > 1)
        {
            const float scale = static_cast<float>(spectrum.size() - 1) / static_cast<float>(numBins - 1);
            for (int i = 0; i < numBins; ++i)
            {
                float srcIndex = i * scale;
                int idx = static_cast<int>(srcIndex);
                float frac = srcIndex - idx;
                float v0 = spectrum[idx];
                float v1 = spectrum[juce::jmin<int>(idx + 1, static_cast<int>(spectrum.size()) - 1)];
                normalized[i] = v0 + (v1 - v0) * frac;
            }
        }
    }

    // FIX #5: Simple rate limiting (~30Hz) to reduce CPU usage
    // Kept simple to avoid potential issues - just frame counting
    if (!force)
    {
        if (++analysisCounter < 3)  // Analyze every 3rd frame
            return;
        analysisCounter = 0;
    }
    
    // Update RMS tracking (no lock needed - local calculation)
    float rmsSum = 0.0f;
    int rmsCount = 0;
    for (int i = 2; i < numBins - 2; ++i)
    {
        if (normalized[i] > -100.0f)
        {
            rmsSum += normalized[i];
            ++rmsCount;
        }
    }
    if (rmsCount > 0)
    {
        currentRMS = rmsSum / static_cast<float>(rmsCount);
        averageRMS = averageRMS * rmsSmoothing + currentRMS * (1.0f - rmsSmoothing);
    }
    else
    {
        // No bin cleared the -100 dB gate, so there is nothing to measure. This
        // used to leave the reference untouched, which meant it stayed frozen at
        // the last audible value for as long as the input remained quiet, and
        // relative-prominence detection kept scoring quiet input against a level
        // that was no longer there. Decay toward the floor instead: absence of
        // measurable content is evidence of a quiet input, not a reason to keep
        // believing the old one.
        //
        // Deliberately a decay and not a reset. A hard jump to the floor would
        // make the reference discontinuous across a single quiet frame, and the
        // detector reads it every frame. The rmsSmoothing already used above is
        // reused so the reference moves on one time constant rather than
        // acquiring a second, independently tunable one.
        currentRMS = kSilenceReferenceFloorDb;
        averageRMS = averageRMS * rmsSmoothing
                   + kSilenceReferenceFloorDb * (1.0f - rmsSmoothing);
    }
    const float localRMS = currentRMS;
    const float localAvgRMS = averageRMS;
    
    // LOCK-FREE: Publish spectrum to triple-buffer for GUI/other threads
    publishSpectrum(normalized, localRMS, localAvgRMS);
    
    // Update temporal history (internal use only, no lock needed)
    updateSpectrumHistory(normalized);
    
    // Also update the internal currentSpectrum for legacy functions
    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        currentSpectrum = normalized;
    }
    
    // Perform detection — routed via shouldUseMLDetection() which respects
    // DetectionBackendMode, forceMLDetectionForTests, and useMLDetection.
    bool usedMotoreV2ExpRouting = false;
#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2 \
    && defined(AIEQ_MOTORE_V2_EXP) && AIEQ_MOTORE_V2_EXP
    if (motoreV2Loaded())
    {
        detectProblems();            // EXP baseline keeps heuristic Sibilance/Harshness.
        applyMotoreV2ExpRouting();   // CNN-routes only the measured Round5 core classes.
        usedMotoreV2ExpRouting = true;
    }
#endif
    if (! usedMotoreV2ExpRouting && shouldUseMLDetection())
    {
        detectProblemsWithML();
    }
    else if (! usedMotoreV2ExpRouting)
    {
        detectProblems();
    }

    // Temporal persistence (hysteresis): stabilise the LIVE problem list so it
    // stops flickering frame-to-frame. The capture/freeze path (force == true)
    // analyses a single window-averaged spectrum once, so it is already stable
    // and must surface its full result immediately — bypass persistence there
    // and reset the live history so the two paths never contaminate each other.
    if (force)
    {
        std::lock_guard<std::mutex> lock(correctionsWriteMutex);
        detectionHistory.clear();
        EmberUI::PersistenceEvidence capture;
        capture.source = EmberUI::PersistenceSource::Capture;
        capture.historyReady = false;
        capture.hits = 0;
        capture.windowSize = 0;
        capture.persistenceFraction = 0.0f;
        pendingEvidence.assign(pendingCorrections.size(), capture);
    }
    else
    {
        applyTemporalPersistence();
    }

    detectGenre();
    
    // Save to history
    saveAnalysisSnapshot();
    
    // Signal new analysis available
    newAnalysisAvailable = true;
}

//==============================================================================
// Optimized processCorrections() with cached coefficients and per-channel states
// FIX 4: Separate lock for coefficient updates to avoid data race
void AIEngine::processCorrections(juce::AudioBuffer<float>& buffer)
{
    if (!isEnabled() || getCorrectionMode() == CorrectionMode::Off)
        return;
    
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;
    
    // Recompute cached coefficients if flagged (e.g., strength changed)
    if (correctionCoeffsNeedUpdate.exchange(false))
    {
        const int activeIdx = activeCorrectionsIndex.load();
        updateCachedCoefficients(activeIdx, approvedCorrectionBuffers[activeIdx]);
        activeCoefficientIndex.store(activeIdx);
    }
    
    // Lock-free read of cached coefficients
    const int coeffIndex = activeCoefficientIndex.load();
    const auto& cache = cachedCoeffs[coeffIndex];
    if (cache.numActive == 0)
        return;
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    
    const int numCorrections = cache.numActive;
    for (int corrIdx = 0; corrIdx < numCorrections; ++corrIdx)
    {
        if (!cache.enabled[corrIdx])
            continue;
        
        const auto& coeffs = cache.coeffs[corrIdx];
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            auto& state = filterStates[corrIdx][ch];
            
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
            
            state.z1 = z1;
            state.z2 = z2;
        }
    }
}

//==============================================================================
// Update biquad coefficients cache (called only when corrections change)
// NOTE: This function assumes correctionsMutex is already locked by the caller
void AIEngine::updateCachedCoefficients(int bufferIndex, const std::vector<Correction>& corrections)
{
    if (currentSampleRate <= 0.0)
        return;

    auto& cache = cachedCoeffs[bufferIndex];
    cache.numActive = 0;

    const size_t numCorrections = juce::jmin(corrections.size(), static_cast<size_t>(maxCorrections));
    for (size_t corrIdx = 0; corrIdx < numCorrections && cache.numActive < maxCorrections; ++corrIdx)
    {
        const auto& corr = corrections[corrIdx];

        float gainDB = corr.suggestedGain * strength;

        bool isGainBased = (corr.suggestedFilter != Correction::FilterType::LowCut &&
                           corr.suggestedFilter != Correction::FilterType::HighCut &&
                           corr.suggestedFilter != Correction::FilterType::Notch);

        if (isGainBased && std::abs(gainDB) < minGainThreshold)
        {
            cache.enabled[cache.numActive] = false;
            continue;
        }

        if (corr.frequency < minFrequency || corr.frequency > static_cast<float>(currentSampleRate) * 0.499f)
        {
            cache.enabled[cache.numActive] = false;
            continue;
        }

        float omega = 2.0f * juce::MathConstants<float>::pi * corr.frequency / static_cast<float>(currentSampleRate);
        omega = juce::jlimit(0.0001f, juce::MathConstants<float>::pi * 0.99f, omega);

        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        float q = juce::jlimit(minQValue, maxQValue, corr.suggestedQ);
        float alpha = sinOmega / (2.0f * q);
        float A = std::pow(10.0f, gainDB / 40.0f);

        float b0, b1, b2, a0, a1, a2;

        switch (corr.suggestedFilter)
        {
            case Correction::FilterType::LowShelf:
            {
                float sqrtA = std::sqrt(A);
                float sqrtA2alpha = 2.0f * sqrtA * alpha;
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega + sqrtA2alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega - sqrtA2alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosOmega + sqrtA2alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) + (A - 1.0f) * cosOmega - sqrtA2alpha;
                break;
            }

            case Correction::FilterType::HighShelf:
            {
                float sqrtA = std::sqrt(A);
                float sqrtA2alpha = 2.0f * sqrtA * alpha;
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega + sqrtA2alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosOmega);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega - sqrtA2alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosOmega + sqrtA2alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosOmega);
                a2 = (A + 1.0f) - (A - 1.0f) * cosOmega - sqrtA2alpha;
                break;
            }

            case Correction::FilterType::LowCut:
            {
                b0 = (1.0f + cosOmega) * 0.5f;
                b1 = -(1.0f + cosOmega);
                b2 = (1.0f + cosOmega) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }

            case Correction::FilterType::HighCut:
            {
                b0 = (1.0f - cosOmega) * 0.5f;
                b1 = 1.0f - cosOmega;
                b2 = (1.0f - cosOmega) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }

            case Correction::FilterType::Notch:
            {
                b0 = 1.0f;
                b1 = -2.0f * cosOmega;
                b2 = 1.0f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha;
                break;
            }

            case Correction::FilterType::Peak:
            default:
            {
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosOmega;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosOmega;
                a2 = 1.0f - alpha / A;
                break;
            }
        }

        auto& coeffs = cache.coeffs[cache.numActive];
        if (std::abs(a0) > 1e-10f)
        {
            coeffs.b0 = b0 / a0;
            coeffs.b1 = b1 / a0;
            coeffs.b2 = b2 / a0;
            coeffs.a1 = a1 / a0;
            coeffs.a2 = a2 / a0;
            coeffs.valid = true;
            cache.enabled[cache.numActive] = true;
            cache.numActive++;
        }
        else
        {
            cache.enabled[cache.numActive] = false;
        }
    }

    // Invalidate remaining slots
    for (int i = cache.numActive; i < maxCorrections; ++i)
    {
        cache.enabled[i] = false;
        cache.coeffs[i].valid = false;
    }
}

//==============================================================================
// Source Profile Implementation

void AIEngine::setSourceProfile(SourceProfile profile)
{
    // FIX RT-SAFETY: Only store the profile atomically. Do NOT call
    // applyProfileThresholds() here — this method is called from the audio
    // thread (processBlock), and applyProfileThresholds() writes a non-atomic
    // struct (ProfileThresholds) that the AI thread reads. Thresholds are now
    // applied lazily on the AI thread in analyzeSpectrum().
    sourceProfile.store(static_cast<int>(profile), std::memory_order_release);
}

void AIEngine::applyProfileThresholds()
{
    // Reset to defaults
    thresholds = ProfileThresholds();
    
    // FIX RT-SAFETY: Load atomic sourceProfile
    const auto profile = static_cast<SourceProfile>(sourceProfile.load(std::memory_order_relaxed));
    
    switch (profile)
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
            
        case SourceProfile::Techno:
            // Techno-specific: focus on kick resonances, sub clarity, hi-hat harshness
            thresholds.resonanceThreshold = 5.0f;    // More sensitive to resonances (kick rings)
            thresholds.lowEndThreshold = -8.0f;       // Moderate sensitivity (sub clarity)
            thresholds.harshnessThreshold = -12.0f;   // Sensitive to hi-hat harshness
            thresholds.sibilanceThreshold = -14.0f;  // Sensitive to sibilance
            thresholds.muddinessLow = 150.0f;         // Lower range for kick mud
            thresholds.muddinessHigh = 400.0f;
            thresholds.muddinessThreshold = -18.0f;   // Sensitive to muddiness
            thresholds.boxyLow = 200.0f;              // Kick boxyness range
            thresholds.boxyHigh = 600.0f;
            thresholds.boxyThreshold = -16.0f;
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
        case SourceProfile::Techno:  return "Techno";
        default: return "Unknown";
    }
}

//==============================================================================
// Corrections Management

std::vector<AIEngine::Correction> AIEngine::getPendingCorrections() const
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    return pendingCorrections;
}

AIEngine::PendingListSnapshot AIEngine::getPendingListSnapshot() const
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    PendingListSnapshot snap;
    snap.corrections = pendingCorrections;
    snap.evidence = pendingEvidence;
    if (snap.evidence.size() != snap.corrections.size())
        snap.evidence.assign(snap.corrections.size(), EmberUI::PersistenceEvidence{});
    return snap;
}

std::vector<AIEngine::Correction> AIEngine::getApprovedCorrections() const
{
    return approvedCorrectionBuffers[activeCorrectionsIndex.load()];
}

std::vector<AIEngine::Correction> AIEngine::getApprovedCorrectionsForUI() const
{
    return approvedCorrectionBuffers[activeCorrectionsIndex.load()];
}

void AIEngine::approveCorrection(int index)
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    if (index < 0 || index >= static_cast<int>(pendingCorrections.size()))
        return;

    const int writeIndex = 1 - activeCorrectionsIndex.load();
    const int readIndex = activeCorrectionsIndex.load();

    // Copy current active approved corrections
    approvedCorrectionBuffers[writeIndex] = approvedCorrectionBuffers[readIndex];

    // Append approved pending correction
    pendingCorrections[index].approved = true;
    approvedCorrectionBuffers[writeIndex].push_back(pendingCorrections[index]);
    pendingCorrections.erase(pendingCorrections.begin() + index);
    if (index < static_cast<int>(pendingEvidence.size()))
        pendingEvidence.erase(pendingEvidence.begin() + static_cast<std::ptrdiff_t>(index));

    // Update cached coefficients for inactive buffer then swap
    updateCachedCoefficients(writeIndex, approvedCorrectionBuffers[writeIndex]);
    activeCorrectionsIndex.store(writeIndex);
    activeCoefficientIndex.store(writeIndex);
    correctionCoeffsNeedUpdate.store(true);
}

void AIEngine::approveAllCorrections()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);

    const int writeIndex = 1 - activeCorrectionsIndex.load();
    const int readIndex = activeCorrectionsIndex.load();
    approvedCorrectionBuffers[writeIndex] = approvedCorrectionBuffers[readIndex];

    for (auto& c : pendingCorrections)
    {
        c.approved = true;
        approvedCorrectionBuffers[writeIndex].push_back(c);
    }
    pendingCorrections.clear();
    pendingEvidence.clear();

    updateCachedCoefficients(writeIndex, approvedCorrectionBuffers[writeIndex]);
    activeCorrectionsIndex.store(writeIndex);
    activeCoefficientIndex.store(writeIndex);
    correctionCoeffsNeedUpdate.store(true);
}

void AIEngine::rejectCorrection(int index)
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    if (index >= 0 && index < static_cast<int>(pendingCorrections.size()))
    {
        pendingCorrections.erase(pendingCorrections.begin() + index);
        if (index < static_cast<int>(pendingEvidence.size()))
            pendingEvidence.erase(pendingEvidence.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void AIEngine::clearCorrections()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    pendingCorrections.clear();
    pendingEvidence.clear();
    detectionHistory.clear();  // drop temporal-persistence window too
    for (auto& buf : approvedCorrectionBuffers)
        buf.clear();

    for (auto& cache : cachedCoeffs)
    {
        cache.numActive = 0;
        cache.enabled.fill(false);
    }
    activeCorrectionsIndex.store(0);
    activeCoefficientIndex.store(0);
    correctionCoeffsNeedUpdate.store(true);

    for (auto& correctionStates : filterStates)
        for (auto& channelState : correctionStates)
            channelState = {};
}

void AIEngine::clearApprovedCorrections()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    const int writeIndex = 1 - activeCorrectionsIndex.load();
    approvedCorrectionBuffers[writeIndex].clear();
    cachedCoeffs[writeIndex].numActive = 0;
    cachedCoeffs[writeIndex].enabled.fill(false);
    activeCorrectionsIndex.store(writeIndex);
    activeCoefficientIndex.store(writeIndex);
    correctionCoeffsNeedUpdate.store(true);
}

//==============================================================================
// Analysis History

void AIEngine::saveAnalysisSnapshot()
{
    std::lock_guard<std::mutex> lock(historyMutex);
    
    AnalysisSnapshot snapshot;
    {
        std::lock_guard<std::mutex> cLock(correctionsWriteMutex);
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
    // FIX RT-SAFETY: Removed mutex, use atomic operations only
    float oldStrength = strength.load(std::memory_order_relaxed);
    float newStrength = juce::jlimit(0.0f, 1.0f, s);
    strength.store(newStrength, std::memory_order_relaxed);
    
    // If strength changed significantly, mark coefficients for update
    if (std::abs(newStrength - oldStrength) > strengthChangeThreshold)
    {
        // NO MUTEX! Just atomic store
        correctionCoeffsNeedUpdate.store(true, std::memory_order_release);
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

AIEngine::Correction::FilterType AIEngine::selectOptimalFilterType(
    ProblemType problem,
    float frequency,
    float bandwidth,
    float peakHeight) const
{
    // Q calcolato dalla bandwidth
    float q = (bandwidth > 0.0f) ? (frequency / bandwidth) : 1.0f;
    
    switch (problem)
    {
        case ProblemType::Resonance:
        {
            // A musical resonance cut should be a bell (Peak): it removes only the
            // measured excess and is reversible. A Notch fully rejects the band and
            // is reserved for genuinely pathological spikes (mains hum, feedback,
            // a single ringing line). The previous gate (q>8 && peakHeight>8) fired
            // far too often — a Q of 8 is a normal-ish bell — so Notch was chosen
            // for ordinary resonances. Default to Peak; only pick Notch for an
            // EXTREMELY narrow AND tall spike.
            if (q > 14.0f && peakHeight > 14.0f)
                return Correction::FilterType::Notch; // surgical only
            else
                return Correction::FilterType::Peak;   // musical default
        }
        
        case ProblemType::Muddiness:
        {
            // Muddiness sotto 250Hz: LowShelf per intervento naturale
            // Muddiness sopra 250Hz: Peak largo
            if (frequency < 250.0f)
                return Correction::FilterType::LowShelf;
            else
                return Correction::FilterType::Peak;
        }
        
        case ProblemType::LowEndBoom:
        {
            // Boom sotto 50Hz: LowCut (rimuove sub eccessivo)
            // Boom 50-150Hz: LowShelf
            if (frequency < 50.0f)
                return Correction::FilterType::LowCut;
            else
                return Correction::FilterType::LowShelf;
        }
        
        case ProblemType::Harshness:
        {
            // Harshness estesa (bandwidth > 2kHz): HighShelf tilt
            // Harshness localizzata: Peak
            if (bandwidth > 2000.0f)
                return Correction::FilterType::HighShelf;
            else
                return Correction::FilterType::Peak;
        }
        
        case ProblemType::Sibilance:
        {
            // Sibilance tipicamente richiede Peak per controllo preciso
            // Ma se molto estesa: HighShelf
            if (bandwidth > 3000.0f)
                return Correction::FilterType::HighShelf;
            else
                return Correction::FilterType::Peak;
        }
        
        case ProblemType::ThinSound:
            // Suono sottile: SEMPRE LowShelf boost per aggiungere corpo
            return Correction::FilterType::LowShelf;
            
        case ProblemType::DullSound:
            // Suono spento: SEMPRE HighShelf boost per aggiungere aria
            return Correction::FilterType::HighShelf;
            
        case ProblemType::Boxyness:
        default:
            // Boxyness e default: Peak standard
            return Correction::FilterType::Peak;
    }
}

//==============================================================================
// Problem Detection

void AIEngine::detectProblems()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    pendingCorrections.clear();

    // B2: capture ONE coherent frame for the ENTIRE heuristic pass. Every
    // detector and helper below reads this frame instead of doing its own
    // consuming readSpectrumSnapshot() — a concurrent audio-thread publish can
    // no longer mix bins from different frames inside one pass (the heuristic
    // twin of the P4-BUG-001 fix on the ML vetoes). Empty = never published.
    scratchHeuristicFrame.clear();
    {
        const auto snapshot = readSpectrumSnapshot();
        if (snapshot.version != 0)
            scratchHeuristicFrame.assign(snapshot.bins.begin(), snapshot.bins.end());
    }
    const auto& frame = scratchHeuristicFrame;

    // Adjust thresholds based on sensitivity (higher sensitivity = lower thresholds)
    float sensitivityFactor = 1.0f - (sensitivity * 0.5f);  // 0.5 to 1.0
    
    float resonanceThresh = thresholds.resonanceThreshold * sensitivityFactor;
    float harshnessThresh = thresholds.harshnessThreshold + (sensitivity * 5.0f);
    float muddinessThresh = thresholds.muddinessThreshold + (sensitivity * 5.0f);
    
    // Run all detection functions (they will add to pendingCorrections)
    detectResonances(frame, resonanceThresh);
    detectHarshness(frame, harshnessThresh);
    detectMuddiness(frame, muddinessThresh);
    detectBoxyness(frame);
    detectSibilance(frame);
    detectLowEndBoom(frame);
    detectThinSound(frame);
    detectDullSound(frame);

    // Shelf filters render their "Q" as corner resonance: a bell-derived Q (which
    // the broad-band detectors may set as high as ~2.5-4) overshoots into a
    // "peaky shelf" with a bump/dip at the corner instead of a smooth tonal tilt.
    // Cap shelves to a clean Butterworth slope (Q<=0.71); bells/cuts keep their Q.
    for (auto& sc : pendingCorrections)
        if (sc.suggestedFilter == Correction::FilterType::LowShelf
            || sc.suggestedFilter == Correction::FilterType::HighShelf)
            sc.suggestedQ = std::min(sc.suggestedQ, 0.71f);


    // Test problem is already added, so we always have at least one problem
    
    // Sort by priority (severity * confidence, highest first)
    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  float priorityA = a.severity * a.confidence;
                  float priorityB = b.severity * b.confidence;
                  if (std::abs(priorityA - priorityB) < 0.01f)
                      return a.severity > b.severity;  // Tie-break by severity
                  return priorityA > priorityB;
              });

    // No hard limit - let filtering/merging handle it
}

//==============================================================================
// Temporal persistence (hysteresis) — see AIEngine.h for rationale.
//
// A problem is surfaced to the user only if a matching detection (same type,
// frequency within a quarter-octave) is present in at least kPersistenceFraction
// of the last kHistoryLen live analyses. Transient single-frame detections on
// non-stationary audio (the "flicker" that made FIX ALL non-deterministic) are
// dropped; surviving problems get their frequency / severity / confidence / gain
// averaged across the window, which also de-jitters their displayed values.
namespace
{
    constexpr std::size_t kHistoryLen          = AIEngine::kLivePersistenceHistoryLen;
    constexpr float       kPersistenceFraction = AIEngine::kLivePersistenceFraction;

    inline bool sameProblem (const AIEngine::Correction& a, const AIEngine::Correction& b)
    {
        return EmberAI::isSameDisplayedProblem (a, b);
    }
}

void AIEngine::resetDetectionHistory()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    detectionHistory.clear();
}

void AIEngine::applyTemporalPersistence()
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);

    // Push a copy of THIS frame's raw detection into the ring before gating,
    // so the history reflects raw per-frame detections (never the gated output).
    detectionHistory.push_back(pendingCorrections);
    while (detectionHistory.size() > kHistoryLen)
        detectionHistory.pop_front();

    const std::size_t n = detectionHistory.size();
    if (n < kHistoryLen)
    {
        // Ring is still filling after play-start / resetDetectionHistory.
        // A 1/1 hit would otherwise pass the 0.6 fraction gate and surface
        // immediately. Do not skip the push — that would mute Assist forever.
        pendingCorrections.clear();
        pendingEvidence.clear();
        return;
    }

    // Candidates come from the most recent frame (we never invent a problem the
    // current spectrum does not show); each is kept only if temporally stable.
    const auto latest = detectionHistory.back();  // copy: we overwrite pendingCorrections below
    std::vector<Correction> stable;
    std::vector<int> stableHits;
    stable.reserve(latest.size());
    stableHits.reserve(latest.size());

    for (const auto& cand : latest)
    {
        int    hits     = 0;
        float  fSum     = 0.0f, sevSum = 0.0f, confSum = 0.0f, gainSum = 0.0f;

        for (const auto& frame : detectionHistory)
        {
            for (const auto& d : frame)
            {
                if (sameProblem(d, cand))
                {
                    ++hits;                       // count this frame once
                    fSum    += d.frequency;
                    sevSum  += d.severity;
                    confSum += d.confidence;
                    gainSum += d.suggestedGain;
                    break;
                }
            }
        }

        const float frac = static_cast<float>(hits) / static_cast<float>(n);
        if (frac < kPersistenceFraction)
            continue;  // transient → drop

        // De-duplicate: a stabilised problem of the same type/freq already kept.
        bool dup = false;
        for (const auto& s : stable)
            if (sameProblem(s, cand)) { dup = true; break; }
        if (dup)
            continue;

        Correction s   = cand;
        const float inv = 1.0f / static_cast<float>(hits);
        s.frequency     = fSum    * inv;   // averaged → stable, de-jittered value
        s.severity      = sevSum  * inv;
        s.confidence    = confSum * inv;
        s.suggestedGain = gainSum * inv;
        stable.push_back(s);
        stableHits.push_back(hits);
    }

    pendingCorrections.swap(stable);
    pendingEvidence.resize(pendingCorrections.size());
    for (size_t i = 0; i < pendingEvidence.size(); ++i)
    {
        pendingEvidence[i].hits = stableHits[i];
        pendingEvidence[i].windowSize = static_cast<int>(kHistoryLen);
        pendingEvidence[i].persistenceFraction = kPersistenceFraction;
        pendingEvidence[i].historyReady = true;
        pendingEvidence[i].source = EmberUI::PersistenceSource::Live;
    }
}

void AIEngine::detectResonances(const std::vector<float>& frame, float threshold)
{
   #if JUCE_UNIT_TESTS
    ResonanceDebugSnapshot resonanceDebug;
    resonanceDebug.outerThreshold = threshold;
    resonanceDebug.outerSensitivityFactor = 1.0f - (sensitivity * 0.5f);
    resonanceDebug.adaptiveSensitivityMultiplier = getSensitivityMultiplier();
    for (float targetFreq : resonanceDebugProbeFreqsForTests)
        resonanceDebug.probes.push_back({ targetFreq });
    lastResonanceDebugForTests = {};
   #endif

    // B2: the frame is captured ONCE by detectProblems (empty = never
    // published). No consuming snapshot read here — and `spectrumCopy` no
    // longer aliases scratchTemp, so the adaptive-threshold / cross-validate
    // helpers can't clobber it mid-loop (pre-B2 they rewrote scratchTemp with
    // their OWN fresh snapshot reads).
    if (frame.empty())
    {
       #if JUCE_UNIT_TESTS
        lastResonanceDebugForTests = resonanceDebug;
       #endif
        return;
    }

    const auto& spectrumCopy = frame;
    
    // Use temporally smoothed spectrum for more stable detection
    std::vector<float> smoothedSpectrum = getTemporallySmoothedSpectrum();
    
    // If smoothed is empty, use raw copy
    if (smoothedSpectrum.empty() || smoothedSpectrum.size() < static_cast<size_t>(numBins))
        smoothedSpectrum = spectrumCopy;
    
    const float minLevel = minSpectrumLevel + 10.0f;  // Slightly higher to reduce noise
    const int spectrumSize = static_cast<int>(smoothedSpectrum.size());
    
    // Calculate adaptive threshold based on signal level
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, threshold);
   #if JUCE_UNIT_TESTS
    resonanceDebug.adaptedThreshold = adaptedThreshold;
   #endif
    
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

    struct HFOctaveSalience
    {
        float bandExcessDb = 0.0f;
        float peakProminenceDb = 0.0f;
        float widthHz = 0.0f;
        bool valid = false;
    };

    auto medianOf = [](auto& values, int count) -> float
    {
        if (count <= 0)
            return -100.0f;

        auto begin = values.begin();
        auto end = begin + count;
        std::sort(begin, end);

        const int mid = count / 2;
        if ((count & 1) != 0)
            return values[static_cast<size_t>(mid)];

        return 0.5f * (values[static_cast<size_t>(mid - 1)] + values[static_cast<size_t>(mid)]);
    };

    auto computeHFOctaveSalience = [&](int peakBin, float targetFreq) -> HFOctaveSalience
    {
        constexpr float kBandHalfOctaves = 0.04f;
        constexpr float kContextHalfOctaves = 0.25f;
        constexpr float kContextExcludeHalfOctaves = 0.08f;
        constexpr int kMaxBandBins = 192;
        constexpr int kMaxContextBins = 768;

        HFOctaveSalience result;
        if (peakBin <= 0 || peakBin >= spectrumSize || targetFreq <= 0.0f)
            return result;

        const float bandLo = targetFreq * std::pow(2.0f, -kBandHalfOctaves);
        const float bandHi = targetFreq * std::pow(2.0f,  kBandHalfOctaves);
        const float contextLo = targetFreq * std::pow(2.0f, -kContextHalfOctaves);
        const float contextHi = targetFreq * std::pow(2.0f,  kContextHalfOctaves);
        const float excludeLo = targetFreq * std::pow(2.0f, -kContextExcludeHalfOctaves);
        const float excludeHi = targetFreq * std::pow(2.0f,  kContextExcludeHalfOctaves);

        std::array<float, kMaxBandBins> band {};
        std::array<float, kMaxContextBins> context {};
        int bandCount = 0;
        int contextCount = 0;
        int octavePeakBin = -1;
        float octavePeak = -1000.0f;

        for (int bin = 1; bin < spectrumSize; ++bin)
        {
            const float f = binToFrequency(bin);
            const float value = smoothedSpectrum[static_cast<size_t>(bin)];

            if (f >= bandLo && f <= bandHi)
            {
                if (bandCount < kMaxBandBins)
                    band[static_cast<size_t>(bandCount++)] = value;

                if (value > octavePeak)
                {
                    octavePeak = value;
                    octavePeakBin = bin;
                }
            }

            if (f >= contextLo && f <= contextHi && (f < excludeLo || f > excludeHi))
            {
                if (contextCount < kMaxContextBins)
                    context[static_cast<size_t>(contextCount++)] = value;
            }
        }

        if (bandCount <= 0 || contextCount < 4 || octavePeakBin < 0)
            return result;

        const float bandMedian = medianOf(band, bandCount);
        const float contextMedian = medianOf(context, contextCount);
        result.bandExcessDb = bandMedian - contextMedian;
        result.peakProminenceDb = octavePeak - contextMedian;

        if (result.peakProminenceDb > 0.0f)
        {
            const float halfHeight = contextMedian + 0.5f * result.peakProminenceDb;
            int left = octavePeakBin;
            int right = octavePeakBin;

            while (left > 1 && smoothedSpectrum[static_cast<size_t>(left - 1)] >= halfHeight)
                --left;

            while (right + 1 < spectrumSize && smoothedSpectrum[static_cast<size_t>(right + 1)] >= halfHeight)
                ++right;

            const float binHz = static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);
            result.widthHz = static_cast<float>(right - left + 1) * binHz;
        }

        result.valid = true;
        return result;
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
        
        // REMOVED: if (centerMag < minLevel) continue; - TOO RESTRICTIVE
        
        // Check if this is a local maximum (using 2 bins each side - more lenient)
        bool isLocalMax = true;
        for (int j = 1; j <= 2; ++j)  // Reduced from 3 to 2
        {
            if (i - j >= 0 && i + j < spectrumSize)
            {
                if (smoothedSpectrum[i] <= smoothedSpectrum[i - j] || 
                    smoothedSpectrum[i] <= smoothedSpectrum[i + j])
                {
                    isLocalMax = false;
                    break;
                }
            }
        }
        
        // Enforce local-maximum: a resonance must actually be a peak. This gate
        // was previously disabled ("SHOW EVEN IF NOT PERFECT LOCAL MAX"), which
        // turned every noise ripple into a candidate and drove the resonance
        // spray / clean-signal false positives. Re-enabled.
       #if JUCE_UNIT_TESTS
        bool isProbeBin = false;
        for (const auto& probe : resonanceDebug.probes)
        {
            if (probe.targetFrequency <= 0.0f)
                continue;

            const float ratio = freq / probe.targetFrequency;
            if (ratio > 0.88f && ratio < 1.12f)
            {
                isProbeBin = true;
                break;
            }
        }
        if (!isLocalMax && !isProbeBin)
            continue;
       #else
        if (!isLocalMax)
            continue;
       #endif

        int windowSize = getAdaptiveWindowSize(freq);
        int halfWindow = windowSize / 2;
        
        // Ensure we don't go out of bounds
        int startBin = juce::jmax(0, i - halfWindow);
        int endBin = juce::jmin(spectrumSize - 1, i + halfWindow);
        
        // Calculate weighted average of surrounding bins (excluding center region)
        // AND a least-squares trend line (dB vs log10 freq) over the SAME bins.
        // The plain mean is dragged off on a steeply tilted spectrum because the bin
        // window is symmetric in BINS but asymmetric in OCTAVES (especially at HF),
        // which inflates peakHeight and manufactures false resonances on clean-but-dark
        // material. The detrended prominence below is tilt-invariant: pure tilt -> ~0,
        // a real peak -> sticks out above the local trend.
        float surroundSum = 0.0f;
        float weightSum = 0.0f;
        double olsN = 0.0, sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
        for (int j = startBin; j <= endBin; ++j)
        {
            if (std::abs(j - i) >= 2)  // Exclude center ±1 bins
            {
                float weight = 1.0f - static_cast<float>(std::abs(j - i)) / static_cast<float>(halfWindow);
                weight = juce::jmax(0.3f, weight);  // Minimum weight
                surroundSum += smoothedSpectrum[j] * weight;
                weightSum += weight;

                const double x = std::log10(static_cast<double>(juce::jmax(1.0f, binToFrequency(j))));
                const double y = static_cast<double>(smoothedSpectrum[j]);
                olsN += 1.0; sumX += x; sumY += y; sumXX += x * x; sumXY += x * y;
            }
        }

        if (weightSum < 0.1f) continue;
        float surroundAvg = surroundSum / weightSum;
        float peakHeight = centerMag - surroundAvg;   // legacy: kept for severity / persist sort

        // Detrended prominence (tilt-invariant). Falls back to peakHeight when the
        // local fit is degenerate (too few bins / no spread in log-freq).
        float prominenceDb = peakHeight;
        {
            const double denom = olsN * sumXX - sumX * sumX;
            if (olsN >= 3.0 && std::abs(denom) > 1e-9)
            {
                const double slope = (olsN * sumXY - sumX * sumY) / denom;
                const double intercept = (sumY - slope * sumX) / olsN;
                const double xc = std::log10(static_cast<double>(juce::jmax(1.0f, freq)));
                const double trendAtCenter = slope * xc + intercept;
                prominenceDb = centerMag - static_cast<float>(trendAtCenter);
            }
        }
        
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
        float minPeakHeight = 3.0f;
        bool passedCandidateGate = prominenceDb > minPeakHeight && prominenceDb > effectiveThreshold;
        float candidateHeightDb = peakHeight;
        float candidateProminenceDb = prominenceDb;
        float candidateBandwidth = 0.0f;
        bool octaveSalienceGate = false;

        if (freq >= 5000.0f)
        {
            const auto hfSalience = computeHFOctaveSalience(i, freq);
            const bool hfBroadBand = hfSalience.valid
                                     && hfSalience.bandExcessDb >= minPeakHeight
                                     && hfSalience.bandExcessDb >= effectiveThreshold;
            const bool hfNarrowPeak = hfSalience.valid
                                      && hfSalience.peakProminenceDb >= 5.5f
                                      && hfSalience.peakProminenceDb >= effectiveThreshold
                                      && hfSalience.widthHz >= 15.0f;

            passedCandidateGate = hfBroadBand || hfNarrowPeak;
            octaveSalienceGate = passedCandidateGate;

            if (passedCandidateGate)
            {
                float hfProminenceDb = 0.0f;
                if (hfBroadBand)
                    hfProminenceDb = std::max(hfProminenceDb, hfSalience.bandExcessDb);
                if (hfNarrowPeak)
                    hfProminenceDb = std::max(hfProminenceDb, hfSalience.peakProminenceDb);

                candidateHeightDb = std::max(candidateHeightDb, hfProminenceDb);
                candidateProminenceDb = std::max(candidateProminenceDb, hfProminenceDb);
                candidateBandwidth = hfSalience.widthHz;
            }
        }
       #if JUCE_UNIT_TESTS
        resonanceDebug.innerSensitivityFactor = sensitivityFactor;
        resonanceDebug.effectiveThreshold = effectiveThreshold;
        for (auto& probe : resonanceDebug.probes)
        {
            if (probe.targetFrequency <= 0.0f)
                continue;

            const float ratio = freq / probe.targetFrequency;
            if (ratio <= 0.88f || ratio >= 1.12f)
                continue;

            const float delta = std::abs(std::log(ratio));
            const float currentDelta = (probe.sampledFrequency > 0.0f)
                ? std::abs(std::log(probe.sampledFrequency / probe.targetFrequency))
                : std::numeric_limits<float>::max();
            if (delta < currentDelta)
            {
                probe.sampledFrequency = freq;
                probe.magnitude = centerMag;
                probe.peakHeight = candidateHeightDb;
                probe.prominenceDb = candidateProminenceDb;
                probe.localMax = isLocalMax;
                probe.passedCandidateGate = isLocalMax && passedCandidateGate;
            }
        }
       #endif
        if (!isLocalMax)
            continue;
        
        // Only detect genuine peaks: must exceed threshold AND be a meaningful resonance.
        // 0.5dB was generating constant false positives on any non-flat material.
        // 3dB is a perceptually meaningful threshold (just-noticeable difference for peaks).
        // Gate on the TILT-INVARIANT prominence, not the tilt-biased plain peakHeight.
        if (passedCandidateGate)
        {
            PeakCandidate peak;
            peak.bin = i;
            peak.frequency = parabolicInterpolation(i);
            peak.magnitude = centerMag;
            peak.peakHeight = candidateHeightDb;
            peak.prominenceDb = candidateProminenceDb;
            peak.bandwidth = candidateBandwidth > 0.0f ? candidateBandwidth : bandwidthFromSpectrum(frame, i);
            peak.calculatedQ = bandwidthToQ(peak.frequency, peak.bandwidth);
            peak.octaveSalienceGate = octaveSalienceGate;
            detectedPeaks.push_back(peak);
           #if JUCE_UNIT_TESTS
            resonanceDebug.rawCandidates.push_back({ peak.frequency, peak.magnitude, peak.peakHeight, peak.prominenceDb });
           #endif
        }
    }

   #if JUCE_UNIT_TESTS
    resonanceDebug.rawCandidateCount = static_cast<int>(resonanceDebug.rawCandidates.size());
   #endif
    
    // Update persistent peaks for temporal stability
    updatePersistentPeaks(detectedPeaks);
   #if JUCE_UNIT_TESTS
    resonanceDebug.persistentCount = static_cast<int>(persistentPeaks.size());
    resonanceDebug.persistentCapHit = persistentPeaks.size() >= 16;
   #endif
    
    // Create corrections from persistent peaks (ALWAYS show if detected, no frame requirement)
    for (const auto& peak : persistentPeaks)
    {
        // Reliability gates: z-score, temporal consensus, contextual whitelist
        int peakBin = juce::jlimit(0, spectrumSize - 1, peak.bin);
        float zScore = computeZScore(smoothedSpectrum, peakBin, 21);
        bool temporalConsensus = hasTemporalConsensus(peak, 3, 2.5f);
        bool contextNormal = isContextuallyNormal(ProblemType::Resonance, peak.frequency);
       #if JUCE_UNIT_TESTS
        ResonanceDebugPeakEval eval;
        eval.frequency = peak.frequency;
        eval.magnitude = peak.magnitude;
        eval.peakHeight = peak.peakHeight;
        eval.prominenceDb = peak.prominenceDb;
        eval.frameCount = peak.frameCount;
        eval.stability = peak.stability;
        eval.consistency = peak.consistency;
        eval.zScore = zScore;
       #endif
        
        // Reject if not an outlier AND not temporally consistent; or if whitelisted content
        if ((zScore < 2.2f && !temporalConsensus && !peak.octaveSalienceGate) ||
            (contextNormal && zScore < 3.0f))
        {
           #if JUCE_UNIT_TESTS
            eval.passedEarlyGate = false;
            resonanceDebug.evaluatedPeaks.push_back(eval);
           #endif
            continue;
        }
       #if JUCE_UNIT_TESTS
        eval.passedEarlyGate = true;
       #endif
        
        // Create correction
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
        
        // Seleziona tipo filtro ottimale
        c.suggestedFilter = selectOptimalFilterType(
            ProblemType::Resonance,
            c.frequency,
            peak.bandwidth,
            peak.peakHeight);
        
        // Severity based on peak height and persistence. The previous forced
        // floor of 0.3 ("ensure visibility") made weak peaks look as severe as
        // real ones; removed so severity reflects the actual measurement.
        float heightSeverity = juce::jlimit(0.0f, 1.0f, peak.peakHeight / 10.0f);
        float persistSeverity = juce::jlimit(0.0f, 0.3f, static_cast<float>(peak.frameCount) * 0.1f);
        c.severity = juce::jlimit(0.0f, 1.0f, heightSeverity + persistSeverity);
        
        // Confidence based on peak prominence, level, persistence, and temporal analysis (MINIMUM 0.4)
        float levelConfidence = juce::jlimit(0.0f, 1.0f, (peak.magnitude + 60.0f) / 50.0f);
        float heightConfidence = juce::jlimit(0.0f, 1.0f, peak.prominenceDb / 8.0f);
        float persistConfidence = juce::jlimit(0.0f, 0.2f, static_cast<float>(peak.frameCount) * 0.05f);
        
        // Boost confidence based on temporal stability and consistency
        float temporalBoost = (peak.stability * 0.15f) + (peak.consistency * 0.10f);
        persistConfidence += temporalBoost;
        
        // Cross-validate detection
        float crossValidationConfidence = crossValidateFromSpectrum(frame, ProblemType::Resonance, peak.frequency, peak.magnitude);
        persistConfidence = (persistConfidence + crossValidationConfidence) * 0.5f;
        
        // FASE 2: Harmonic Analysis - filter out harmonic peaks (legitimate, not problems)
        float fundamentalFreq = fundamentalFromSpectrum(frame, 50.0f, 500.0f);
        bool isHarmonic = false;
        if (fundamentalFreq > 0.0f)
        {
            isHarmonic = isHarmonicPeak(peak.frequency, fundamentalFreq, 0.05f);
            if (isHarmonic)
            {
                // Harmonic peak = legitimate, reduce confidence significantly
                persistConfidence *= 0.3f;  // Heavily penalize harmonics
            }
        }
        
        // FASE 2: Spectral Coherence - pattern matching for resonance
        float coherenceScore = getSpectralPatternScore(ProblemType::Resonance, peak.frequency, peak.bandwidth);
        persistConfidence = (persistConfidence * 0.7f) + (coherenceScore * 0.3f);
        
        // FASE 2: Dynamic Range Normalization - adjust threshold based on DR
        float dynamicRange = calculateDynamicRange();
        float normalizedThreshold = normalizeThresholdByDynamicRange(adaptedThreshold, dynamicRange);
        // If peak doesn't exceed normalized threshold, reduce confidence
        if (peak.peakHeight < (normalizedThreshold - adaptedThreshold))
        {
            persistConfidence *= 0.8f;
        }
        
        // Add z-score contribution
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);  // z>2 -> boost
        
        // Previously floored at 0.4 ("ensure visibility"), which made the
        // downstream confidence gate meaningless — every emitted resonance
        // passed by construction. Removed so confidence is a real discriminator.
        c.confidence = juce::jlimit(0.0f, 1.0f,
                levelConfidence * 0.25f +
                heightConfidence * 0.35f +
                persistConfidence * 0.25f +
                zBoost * 0.15f);
       #if JUCE_UNIT_TESTS
        eval.confidence = c.confidence;
       #endif
        
        // Skip if harmonic (legitimate, not a problem) unless very high confidence
        if (isHarmonic && c.confidence < 0.6f)
        {
           #if JUCE_UNIT_TESTS
            eval.passedConfidenceGate = false;
            resonanceDebug.evaluatedPeaks.push_back(eval);
           #endif
            continue;  // Skip harmonic peaks unless very high confidence
        }

        // Confidence gate. Now that confidence is a real measurement (the 0.4
        // floor was removed in the surgical heuristic fix), a fixed gate cleanly
        // separates genuine resonances from noise ripples. Empirically (AI-Sweep
        // matrix): a true injected resonance scores c≈0.56-0.57, while noise/clean
        // ripples top out at c≈0.39-0.46. A gate at 0.45 removes the resonance
        // "spray" and the clean-signal false positives while keeping the real peak.
        //
        // CRITICAL: this MUST gate before push_back — the UI reads
        // getPendingCorrections() directly, not the filtered helper, so a
        // downstream filter would not affect what the user sees.
        //
        // The gate is INTENTIONALLY independent of sensitivity: sensitivity tunes
        // which real peaks are flagged, but must never re-open the floodgates on a
        // flat/noisy spectrum (the invariant that keeps "clean = no problem" true).
        constexpr float kHeuristicConfidenceGate = 0.45f;
        if (c.confidence < kHeuristicConfidenceGate)
        {
           #if JUCE_UNIT_TESTS
            eval.passedConfidenceGate = false;
            resonanceDebug.evaluatedPeaks.push_back(eval);
           #endif
            continue;
        }

       #if JUCE_UNIT_TESTS
        eval.passedConfidenceGate = true;
        resonanceDebug.evaluatedPeaks.push_back(eval);
       #endif

        // Detailed description with bandwidth info
        juce::String bandName = getBandName(peak.frequency);
        c.description = juce::String::formatted(
            "%s at %.1f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (BW: %.0f Hz, +%.1f dB)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            peak.bandwidth,
            peak.peakHeight);

        pendingCorrections.push_back(c);
       #if JUCE_UNIT_TESTS
        resonanceDebug.emittedCorrections.push_back(c);
       #endif
    }

   #if JUCE_UNIT_TESTS
    lastResonanceDebugForTests = std::move(resonanceDebug);
   #endif
}

void AIEngine::detectHarshness(const std::vector<float>& frame, float threshold)
{
    // B2: all reads share the caller-captured frame
    float energy = bandEnergyFromSpectrum(frame, thresholds.harshnessLow, thresholds.harshnessHigh);
    float overallEnergy = bandEnergyFromSpectrum(frame, 200.0f, 15000.0f);
    float relativeEnergy = energy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.0f * sensitivityMultiplier;
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, threshold);
    
    // Only flag harshness when relative energy is meaningfully elevated
    if (relativeEnergy > adjustedRelativeThreshold && energy > adaptedThreshold)
    {
        // Find the peak frequency within the harshness range
        float peakFreq = findPeakInSpectrum(frame, thresholds.harshnessLow, thresholds.harshnessHigh);
        if (peakFreq <= 0.0f)
            peakFreq = 3500.0f;  // Default if not found
        
        Correction c;
        c.type = ProblemType::Harshness;
        c.frequency = peakFreq > 0 ? peakFreq : 3500.0f;
        c.suggestedGain = -(relativeEnergy * (0.45f + sensitivity * 0.25f));
        c.suggestedGain = juce::jlimit(-12.0f, -1.0f, c.suggestedGain);
        c.suggestedQ = 0.7f + (relativeEnergy * 0.08f);  // Wider Q for broad harshness
        c.suggestedQ = juce::jlimit(0.4f, 2.5f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 7.0f));  // MIN 0.1
        
        // Base confidence with cross-validation
        float baseConfidence = 0.70f + (sensitivity * 0.20f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::Harshness, c.frequency, energy);
        
        // FASE 2: Spectral Coherence - pattern matching for harshness
        float coherenceScore = getSpectralPatternScore(ProblemType::Harshness, c.frequency, 0.0f);
        baseConfidence = (baseConfidence * 0.7f) + (coherenceScore * 0.3f);
        
        // FASE 2: Dynamic Range Normalization
        float dynamicRange = calculateDynamicRange();
        float normalizedThreshold = normalizeThresholdByDynamicRange(adaptedThreshold, dynamicRange);
        if (relativeEnergy < (normalizedThreshold - adaptedThreshold))
        {
            baseConfidence *= 0.85f;
        }
        
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::Harshness, c.frequency, bandwidth, 0.0f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (%.1f dB above average)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectMuddiness(const std::vector<float>& frame, float threshold)
{
    float lowMidEnergy = bandEnergyFromSpectrum(frame, thresholds.muddinessLow, thresholds.muddinessHigh);
    float overallEnergy = bandEnergyFromSpectrum(frame, 100.0f, 10000.0f);
    float relativeEnergy = lowMidEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.0f * sensitivityMultiplier;
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, threshold);
    
    // Only flag muddiness when low-mid energy is genuinely elevated vs overall
    if (relativeEnergy > adjustedRelativeThreshold && lowMidEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the muddiness range
        float peakFreq = findPeakInSpectrum(frame, thresholds.muddinessLow, thresholds.muddinessHigh);
        if (peakFreq <= 0.0f)
            peakFreq = (thresholds.muddinessLow + thresholds.muddinessHigh) / 2.0f;
        
        Correction c;
        c.type = ProblemType::Muddiness;
        c.frequency = peakFreq > 0 ? peakFreq : (thresholds.muddinessLow + thresholds.muddinessHigh) / 2.0f;
        c.suggestedGain = -(relativeEnergy * (0.35f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-10.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.6f + (relativeEnergy * 0.04f);  // Wide Q for broad muddiness
        c.suggestedQ = juce::jlimit(0.4f, 1.5f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 6.0f));  // MIN 0.1
        
        // Base confidence with cross-validation
        float baseConfidence = 0.65f + (sensitivity * 0.20f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::Muddiness, c.frequency, lowMidEnergy);
        
        // FASE 2: Spectral Coherence - pattern matching for muddiness
        float coherenceScore = getSpectralPatternScore(ProblemType::Muddiness, c.frequency, 0.0f);
        baseConfidence = (baseConfidence * 0.7f) + (coherenceScore * 0.3f);
        
        // FASE 2: Dynamic Range Normalization
        float dynamicRange = calculateDynamicRange();
        float normalizedThreshold = normalizeThresholdByDynamicRange(adaptedThreshold, dynamicRange);
        if (relativeEnergy < (normalizedThreshold - adaptedThreshold))
        {
            baseConfidence *= 0.85f;
        }
        
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::Muddiness, c.frequency, bandwidth, relativeEnergy);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (%.1f dB excess)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectBoxyness(const std::vector<float>& frame)
{
    float boxEnergy = bandEnergyFromSpectrum(frame, thresholds.boxyLow, thresholds.boxyHigh);
    float overallEnergy = bandEnergyFromSpectrum(frame, 100.0f, 8000.0f);
    float relativeEnergy = boxEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 3.5f * sensitivityMultiplier;
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, thresholds.boxyThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && boxEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the boxyness range
        float peakFreq = findPeakInSpectrum(frame, thresholds.boxyLow, thresholds.boxyHigh);
        if (peakFreq <= 0.0f)
            peakFreq = (thresholds.boxyLow + thresholds.boxyHigh) / 2.0f;
        
        Correction c;
        c.type = ProblemType::Boxyness;
        c.frequency = peakFreq > 0 ? peakFreq : (thresholds.boxyLow + thresholds.boxyHigh) / 2.0f;
        c.suggestedGain = -(relativeEnergy * (0.40f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-9.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.9f + (relativeEnergy * 0.06f);
        c.suggestedQ = juce::jlimit(0.6f, 3.0f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 8.0f));  // MIN 0.1
        
        // Base confidence with cross-validation
        float baseConfidence = 0.60f + (sensitivity * 0.25f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::Boxyness, c.frequency, boxEnergy);
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::Boxyness, c.frequency, bandwidth, 0.0f);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (+%.1f dB coloration)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectSibilance(const std::vector<float>& frame)
{
    float sibilanceEnergy = bandEnergyFromSpectrum(frame, thresholds.sibilanceLow, thresholds.sibilanceHigh);
    float midEnergy = bandEnergyFromSpectrum(frame, 2000.0f, 5000.0f);
    float relativeEnergy = sibilanceEnergy - midEnergy;
    
    // Use exponential sensitivity curve - sibilance is very sensitivity-dependent
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 2.0f * sensitivityMultiplier;
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, thresholds.sibilanceThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && sibilanceEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the sibilance range
        float peakFreq = findPeakInSpectrum(frame, thresholds.sibilanceLow, thresholds.sibilanceHigh);
        if (peakFreq <= 0.0f)
            peakFreq = (thresholds.sibilanceLow + thresholds.sibilanceHigh) / 2.0f;
        
        Correction c;
        c.type = ProblemType::Sibilance;
        c.frequency = peakFreq > 0 ? peakFreq : 7000.0f;
        c.suggestedGain = -(relativeEnergy * (0.45f + sensitivity * 0.25f));
        c.suggestedGain = juce::jlimit(-12.0f, -1.0f, c.suggestedGain);
        c.suggestedQ = 1.0f + (relativeEnergy * 0.12f);
        c.suggestedQ = juce::jlimit(0.7f, 4.0f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 6.0f));  // MIN 0.1
        
        // Base confidence with cross-validation
        float baseConfidence = 0.70f + (sensitivity * 0.20f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::Sibilance, c.frequency, sibilanceEnergy);
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::Sibilance, c.frequency, bandwidth, relativeEnergy);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (+%.1f dB above presence)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectLowEndBoom(const std::vector<float>& frame)
{
    float subEnergy = bandEnergyFromSpectrum(frame, 30.0f, 100.0f);
    float overallEnergy = bandEnergyFromSpectrum(frame, 100.0f, 5000.0f);
    float relativeEnergy = subEnergy - overallEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 5.0f * sensitivityMultiplier;
    float adaptedThreshold = adaptiveThresholdFromSpectrum(frame, thresholds.lowEndThreshold);
    
    if (relativeEnergy > adjustedRelativeThreshold && subEnergy > adaptedThreshold)
    {
        // Find the peak frequency within the sub-bass range
        float peakFreq = findPeakInSpectrum(frame, 30.0f, 100.0f);
        if (peakFreq <= 0.0f)
            peakFreq = 60.0f;  // Default
        
        Correction c;
        c.type = ProblemType::LowEndBoom;
        c.frequency = peakFreq > 0 ? peakFreq : 60.0f;
        c.suggestedGain = -(relativeEnergy * (0.30f + sensitivity * 0.20f));
        c.suggestedGain = juce::jlimit(-10.0f, -0.5f, c.suggestedGain);
        c.suggestedQ = 0.5f + (relativeEnergy * 0.02f);  // Wide Q for low frequencies
        c.suggestedQ = juce::jlimit(0.4f, 1.2f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 9.0f));  // MIN 0.1
        
        // Base confidence with cross-validation
        float baseConfidence = 0.60f + (sensitivity * 0.25f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::LowEndBoom, c.frequency, subEnergy);
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::LowEndBoom, c.frequency, bandwidth, relativeEnergy);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (+%.1f dB above mix)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectThinSound(const std::vector<float>& frame)
{
    float lowMidEnergy = bandEnergyFromSpectrum(frame, 200.0f, 600.0f);
    float highEnergy = bandEnergyFromSpectrum(frame, 2000.0f, 8000.0f);
    float relativeEnergy = highEnergy - lowMidEnergy;
    
    // Use exponential sensitivity curve for subtle issues
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 7.0f * sensitivityMultiplier;
    
    if (relativeEnergy > adjustedRelativeThreshold)
    {
        // Find where the deficiency is most pronounced
        float deficientFreq = findLowestInSpectrum(frame, 200.0f, 600.0f);
        if (deficientFreq <= 0.0f)
            deficientFreq = 350.0f;  // Default
        
        Correction c;
        c.type = ProblemType::ThinSound;
        c.frequency = deficientFreq > 0 ? deficientFreq : 350.0f;
        c.suggestedGain = relativeEnergy * (0.22f + sensitivity * 0.12f);  // Boost, not cut
        c.suggestedGain = juce::jlimit(0.5f, 6.0f, c.suggestedGain);
        c.suggestedQ = 0.6f + (relativeEnergy * 0.02f);  // Wide shelf-like boost
        c.suggestedQ = juce::jlimit(0.4f, 1.2f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 10.0f));  // MIN 0.1
        
        // Base confidence with cross-validation (for boost corrections, validate deficiency)
        float baseConfidence = 0.50f + (sensitivity * 0.25f);
        // For ThinSound, validate that low-mids are actually deficient
        float lowMidMag = bandEnergyFromSpectrum(frame, 200.0f, 600.0f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::ThinSound, c.frequency, lowMidMag);
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::ThinSound, c.frequency, bandwidth, relativeEnergy);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (%.1f dB below highs)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

void AIEngine::detectDullSound(const std::vector<float>& frame)
{
    float highEnergy = bandEnergyFromSpectrum(frame, 8000.0f, 16000.0f);
    float midEnergy = bandEnergyFromSpectrum(frame, 1000.0f, 4000.0f);
    float relativeEnergy = midEnergy - highEnergy;
    
    // Use exponential sensitivity curve
    float sensitivityMultiplier = getSensitivityMultiplier();
    float adjustedRelativeThreshold = 9.0f * sensitivityMultiplier;
    
    if (relativeEnergy > adjustedRelativeThreshold)
    {
        // Find where to apply the boost
        float airFreq = findLowestInSpectrum(frame, 8000.0f, 14000.0f);
        if (airFreq <= 0.0f)
            airFreq = 10000.0f;  // Default
        
        Correction c;
        c.type = ProblemType::DullSound;
        c.frequency = airFreq > 0 ? airFreq : 10000.0f;
        c.suggestedGain = relativeEnergy * (0.16f + sensitivity * 0.10f);  // Boost, not cut
        c.suggestedGain = juce::jlimit(0.5f, 6.0f, c.suggestedGain);
        c.suggestedQ = 0.5f + (relativeEnergy * 0.015f);  // Very wide high shelf
        c.suggestedQ = juce::jlimit(0.3f, 1.0f, c.suggestedQ);
        c.severity = juce::jmax(0.1f, juce::jlimit(0.0f, 1.0f, relativeEnergy / 12.0f));  // MIN 0.1
        
        // Base confidence with cross-validation (for boost corrections, validate deficiency)
        float baseConfidence = 0.45f + (sensitivity * 0.30f);
        // For DullSound, validate that highs are actually deficient
        float highMag = bandEnergyFromSpectrum(frame, 8000.0f, 16000.0f);
        float crossValidationConf = crossValidateFromSpectrum(frame, ProblemType::DullSound, c.frequency, highMag);
        c.confidence = juce::jmax(0.2f, (baseConfidence + crossValidationConf) * 0.5f);  // MIN 0.2
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence);
        
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        c.suggestedFilter = selectOptimalFilterType(ProblemType::DullSound, c.frequency, bandwidth, relativeEnergy);
        
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - Suggested: %s %.1f dB, Q: %.1f (%.1f dB below mids)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            relativeEnergy);
        
        // Reliability: z-score + contextual whitelist
        float zScore = zScoreAtFrequencyFromSpectrum(frame, c.frequency, 21);
        bool contextNormal = isContextuallyNormal(c.type, c.frequency);
        float zBoost = juce::jlimit(0.0f, 1.0f, (zScore - 2.0f) / 3.0f);
        c.confidence = juce::jlimit(0.0f, 1.0f, c.confidence * 0.7f + zBoost * 0.3f);
        if ((zScore < 2.2f && c.confidence < 0.55f) || (contextNormal && zScore < 3.0f))
        {
            // Skip this correction - not reliable enough
        }
        else
        {
            pendingCorrections.push_back(c);
        }
    }
}

//==============================================================================
// LOCK-FREE: detectGenre() - Uses triple-buffered spectrum
void AIEngine::detectGenre()
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return;
    
    // Calculate band energies from snapshot
    auto calcBandEnergy = [&](float lowFreq, float highFreq) -> float {
        // SAFETY: Validate sample rate to prevent division issues
        if (currentSampleRate <= 0.0)
            return -100.0f;
        
        int lowBin = static_cast<int>(lowFreq * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
        int highBin = static_cast<int>(highFreq * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
        
        // BOUNDS CHECK: Clamp to valid array indices
        lowBin = std::max(0, std::min(lowBin, static_cast<int>(snapshot.bins.size()) - 1));
        highBin = std::max(0, std::min(highBin, static_cast<int>(snapshot.bins.size()) - 1));
        
        if (lowBin >= highBin) return -100.0f;
        
        float sum = 0.0f;
        int count = 0;
        for (int i = lowBin; i <= highBin; ++i)
        {
            // Double-check bounds (defensive)
            if (i >= 0 && i < static_cast<int>(snapshot.bins.size()) && snapshot.bins[static_cast<size_t>(i)] > -100.0f)
            {
                sum += snapshot.bins[static_cast<size_t>(i)];
                ++count;
            }
        }
        return (count > 0) ? (sum / static_cast<float>(count)) : -100.0f;
    };
    
    float subBass = calcBandEnergy(20.0f, 60.0f);
    float bass = calcBandEnergy(60.0f, 200.0f);
    float lowMid = calcBandEnergy(200.0f, 500.0f);
    float mid = calcBandEnergy(500.0f, 2000.0f);
    float highMid = calcBandEnergy(2000.0f, 6000.0f);
    float high = calcBandEnergy(6000.0f, 16000.0f);
    
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

// Find the peak frequency within a given range (LOCK-FREE)
float AIEngine::findPeakInRange(float lowFreq, float highFreq)
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return -1.0f;
    
    int lowBin = frequencyToBin(lowFreq);
    int highBin = frequencyToBin(highFreq);
    
    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);
    
    if (highBin <= lowBin)
        return -1.0f;
    
    float maxMag = -200.0f;
    int maxBin = lowBin;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < numBins)
        {
            if (snapshot.bins[static_cast<size_t>(i)] > maxMag)
            {
                maxMag = snapshot.bins[static_cast<size_t>(i)];
                maxBin = i;
            }
        }
    }
    
    // Apply parabolic interpolation for precise frequency
    if (maxBin > 0 && maxBin < numBins - 1)
    {
        float y0 = snapshot.bins[static_cast<size_t>(maxBin - 1)];
        float y1 = snapshot.bins[static_cast<size_t>(maxBin)];
        float y2 = snapshot.bins[static_cast<size_t>(maxBin + 1)];
        
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

// Find the lowest energy frequency within a given range (LOCK-FREE)
float AIEngine::findLowestInRange(float lowFreq, float highFreq)
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return -1.0f;
    
    int lowBin = frequencyToBin(lowFreq);
    int highBin = frequencyToBin(highFreq);
    
    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);
    
    if (highBin <= lowBin)
        return -1.0f;
    
    float minMag = 100.0f;
    int minBin = lowBin;
    
    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < numBins)
        {
            if (snapshot.bins[static_cast<size_t>(i)] < minMag)
            {
                minMag = snapshot.bins[static_cast<size_t>(i)];
                minBin = i;
            }
        }
    }
    
    return binToFrequency(minBin);
}

// LOCK-FREE version - reads from triple-buffer
float AIEngine::calculateBandEnergy(float lowFreq, float highFreq)
{
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return -100.0f;
    
    // SAFETY: Validate sample rate
    if (currentSampleRate <= 0.0)
        return -100.0f;
    
    int lowBin = static_cast<int>(lowFreq * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    int highBin = static_cast<int>(highFreq * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    
    // BOUNDS CHECK: Clamp to valid array indices
    const int maxIdx = static_cast<int>(snapshot.bins.size()) - 1;
    lowBin = std::max(0, std::min(lowBin, maxIdx));
    highBin = std::max(0, std::min(highBin, maxIdx));
    
    if (lowBin >= highBin)
        return -100.0f;
    
    float sum = 0.0f;
    int count = 0;
    for (int i = lowBin; i <= highBin; ++i)
    {
        // Double-check bounds (defensive)
        if (i >= 0 && i < static_cast<int>(snapshot.bins.size()) && snapshot.bins[static_cast<size_t>(i)] > -100.0f)
        {
            sum += snapshot.bins[static_cast<size_t>(i)];
            ++count;
        }
    }
    return (count > 0) ? (sum / static_cast<float>(count)) : -100.0f;
}

// P4-BUG-001: pure variant of calculateBandEnergy over a CALLER-SUPPLIED spectrum (no
// readSpectrumSnapshot consuming-swap) so all ML-path veto reads share ONE coherent frame.
// Bin formula / -100 dB exclusion gate / mean are IDENTICAL to calculateBandEnergy; the only
// difference is the frame source (bins) and the clamp ceiling (bins.size()-1, the supplied
// vector may be shorter than numBins). NOT routed through frequencyToBin — keeps the inline
// truncating bin math byte-identical to calculateBandEnergy (do NOT unify the two mappings).
float AIEngine::bandEnergyFromSpectrum(const std::vector<float>& bins, float loHz, float hiHz) const
{
    if (bins.empty())
        return -100.0f;

    // SAFETY: Validate sample rate
    if (currentSampleRate <= 0.0)
        return -100.0f;

    int lowBin = static_cast<int>(loHz * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));
    int highBin = static_cast<int>(hiHz * static_cast<float>(fftSize) / static_cast<float>(currentSampleRate));

    // BOUNDS CHECK: clamp to the SUPPLIED vector's indices (NOT numBins)
    const int maxIdx = static_cast<int>(bins.size()) - 1;
    lowBin = std::max(0, std::min(lowBin, maxIdx));
    highBin = std::max(0, std::min(highBin, maxIdx));

    if (lowBin >= highBin)
        return -100.0f;

    float sum = 0.0f;
    int count = 0;
    for (int i = lowBin; i <= highBin; ++i)
    {
        // Double-check bounds (defensive)
        if (i >= 0 && i < static_cast<int>(bins.size()) && bins[static_cast<size_t>(i)] > -100.0f)
        {
            sum += bins[static_cast<size_t>(i)];
            ++count;
        }
    }
    return (count > 0) ? (sum / static_cast<float>(count)) : -100.0f;
}

// P4-BUG-001: pure variant of findPeakInRange over a CALLER-SUPPLIED spectrum (no
// readSpectrumSnapshot consuming-swap). frequencyToBin mapping / argmax / parabolic
// interpolation are IDENTICAL to findPeakInRange; returns <= 0 on failure (preserves the
// Resonance veto's `actualPeak <= 0.0f` early-out). Length-guarded against bins.size() since
// the supplied vector may be shorter than numBins; the NaN guard abs(denom)>1e-10f is verbatim.
float AIEngine::findPeakInSpectrum(const std::vector<float>& bins, float loHz, float hiHz) const
{
    if (bins.empty())
        return -1.0f;

    int lowBin = frequencyToBin(loHz);
    int highBin = frequencyToBin(hiHz);

    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);

    if (highBin <= lowBin)
        return -1.0f;

    float maxMag = -200.0f;
    int maxBin = lowBin;

    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < numBins && i < static_cast<int>(bins.size()))
        {
            if (bins[static_cast<size_t>(i)] > maxMag)
            {
                maxMag = bins[static_cast<size_t>(i)];
                maxBin = i;
            }
        }
    }

    // Apply parabolic interpolation for precise frequency
    if (maxBin > 0 && maxBin < numBins - 1
        && static_cast<size_t>(maxBin) + 1 < bins.size())
    {
        float y0 = bins[static_cast<size_t>(maxBin - 1)];
        float y1 = bins[static_cast<size_t>(maxBin)];
        float y2 = bins[static_cast<size_t>(maxBin + 1)];

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

// ── B2 pure frame-coherent helpers ─────────────────────────────────────────
// Same math as their snapshot-reading counterparts; the ONLY difference is the
// caller-supplied frame. None of them touches scratchTemp (pre-B2, the
// adaptive-threshold and cross-validate helpers rewrote scratchTemp with their
// own fresh snapshot reads, silently clobbering a caller's scratchTemp-backed
// spectrum copy mid-loop — detectResonances was exposed to exactly that).

float AIEngine::adaptiveThresholdFromSpectrum(const std::vector<float>& bins, float baseThreshold) const
{
    if (bins.empty())
    {
        // Same fallback as calculateAdaptiveThresholdPercentile on version==0
        float rmsOffset = (averageRMS - (-40.0f)) * 0.15f;
        float adaptedThreshold = baseThreshold + rmsOffset;
        return adaptedThreshold * getSensitivityMultiplier();
    }

    // calculatePercentile builds its own filtered copy — bins are not mutated.
    float percentile95 = calculatePercentile(bins, 0.95f);
    float percentile50 = calculatePercentile(bins, 0.50f);
    float percentile5  = calculatePercentile(bins, 0.05f);

    float dynamicRange   = percentile95 - percentile50;
    float spectralSpread = percentile95 - percentile5;

    float rangeFactor  = 1.0f + (dynamicRange / 20.0f) * 0.3f;
    float spreadFactor = 1.0f - (spectralSpread < 30.0f ? (30.0f - spectralSpread) / 100.0f : 0.0f);
    float rmsOffset    = (averageRMS - (-40.0f)) * 0.1f;

    float adaptedThreshold = baseThreshold * rangeFactor * spreadFactor + rmsOffset;
    return adaptedThreshold * getSensitivityMultiplier();
}

float AIEngine::zScoreAtFrequencyFromSpectrum(const std::vector<float>& bins, float frequency, int window) const
{
    if (bins.empty())
        return 0.0f;
    const int bin = frequencyToBin(frequency);
    if (bin < 0 || bin >= static_cast<int>(bins.size()))
        return 0.0f;
    return computeZScore(bins, bin, window);
}

float AIEngine::crossValidateFromSpectrum(const std::vector<float>& bins, ProblemType type,
                                          float frequency, float magnitude) const
{
    if (bins.empty())
        return 0.5f;  // Neutral confidence if no spectrum

    int bin = frequencyToBin(frequency);
    if (bin < 0 || bin >= static_cast<int>(bins.size()))
        return 0.5f;

    float confidence = 0.5f;
    int validationCount = 0;
    float confidenceSum = 0.0f;

    // Method 1: magnitude significantly above surrounding bins
    float surroundAvg = 0.0f;
    int surroundCount = 0;
    int window = 5;
    const int specSize = static_cast<int>(bins.size());
    for (int i = juce::jmax(0, bin - window); i <= juce::jmin(specSize - 1, bin + window); ++i)
    {
        if (i != bin && std::abs(i - bin) >= 2)
        {
            surroundAvg += bins[static_cast<size_t>(i)];
            surroundCount++;
        }
    }
    if (surroundCount > 0)
    {
        surroundAvg /= static_cast<float>(surroundCount);
        float prominence = magnitude - surroundAvg;
        float method1Conf = juce::jlimit(0.0f, 1.0f, prominence / 6.0f);
        confidenceSum += method1Conf;
        validationCount++;
    }

    // Method 2: frequency in the expected range for the problem type
    bool inExpectedRange = false;
    switch (type)
    {
        case ProblemType::Resonance:  inExpectedRange = (frequency >= 50.0f    && frequency <= 15000.0f); break;
        case ProblemType::Harshness:  inExpectedRange = (frequency >= 1000.0f  && frequency <= 8000.0f);  break;
        case ProblemType::Muddiness:  inExpectedRange = (frequency >= 150.0f   && frequency <= 500.0f);   break;
        case ProblemType::Sibilance:  inExpectedRange = (frequency >= 5000.0f  && frequency <= 10000.0f); break;
        case ProblemType::LowEndBoom: inExpectedRange = (frequency >= 30.0f    && frequency <= 100.0f);   break;
        case ProblemType::ThinSound:  inExpectedRange = (frequency >= 200.0f   && frequency <= 600.0f);   break;
        case ProblemType::DullSound:  inExpectedRange = (frequency >= 8000.0f  && frequency <= 16000.0f); break;
        case ProblemType::Boxyness:   inExpectedRange = (frequency >= 400.0f   && frequency <= 800.0f);   break;
        default:                      inExpectedRange = true; break;
    }
    confidenceSum += inExpectedRange ? 0.8f : 0.3f;
    validationCount++;

    // Method 3: magnitude above the noise floor
    float noiseFloor = calculatePercentile(bins, 0.10f);
    float aboveNoise = magnitude - noiseFloor;
    confidenceSum += juce::jlimit(0.0f, 1.0f, aboveNoise / 10.0f);
    validationCount++;

    if (validationCount > 0)
        confidence = confidenceSum / static_cast<float>(validationCount);

    return juce::jlimit(0.0f, 1.0f, confidence);
}

float AIEngine::findLowestInSpectrum(const std::vector<float>& bins, float loHz, float hiHz) const
{
    if (bins.empty())
        return -1.0f;

    int lowBin = frequencyToBin(loHz);
    int highBin = frequencyToBin(hiHz);

    lowBin = juce::jlimit(0, numBins - 1, lowBin);
    highBin = juce::jlimit(0, numBins - 1, highBin);

    if (highBin <= lowBin)
        return -1.0f;

    float minMag = 100.0f;
    int minBin = lowBin;

    for (int i = lowBin; i <= highBin; ++i)
    {
        if (i >= 0 && i < numBins && i < static_cast<int>(bins.size()))
        {
            if (bins[static_cast<size_t>(i)] < minMag)
            {
                minMag = bins[static_cast<size_t>(i)];
                minBin = i;
            }
        }
    }

    return binToFrequency(minBin);
}

float AIEngine::fundamentalFromSpectrum(const std::vector<float>& bins, float minFreq, float maxFreq) const
{
    if (bins.empty())
        return -1.0f;

    int minBin = frequencyToBin(minFreq);
    int maxBin = frequencyToBin(maxFreq);

    const int arrSize = static_cast<int>(bins.size());
    minBin = juce::jlimit(0, arrSize - 1, minBin);
    maxBin = juce::jlimit(0, arrSize - 1, maxBin);

    if (maxBin <= minBin || maxBin - minBin < 4)
        return -1.0f;

    // Harmonic-relationship fundamental detection (same as findFundamentalFrequency)
    std::vector<std::pair<float, float>> peaks;  // (frequency, magnitude)

    const int loopStart = std::max(2, minBin + 2);
    const int loopEnd = std::min(arrSize - 3, maxBin - 2);

    for (int i = loopStart; i < loopEnd; ++i)
    {
        if (bins[static_cast<size_t>(i)] > bins[static_cast<size_t>(i-1)] &&
            bins[static_cast<size_t>(i)] > bins[static_cast<size_t>(i+1)] &&
            bins[static_cast<size_t>(i)] > bins[static_cast<size_t>(i-2)] &&
            bins[static_cast<size_t>(i)] > bins[static_cast<size_t>(i+2)])
        {
            float freq = binToFrequency(i);
            float mag = bins[static_cast<size_t>(i)];
            if (mag > -80.0f)
                peaks.push_back({freq, mag});
        }
    }

    if (peaks.empty())
        return -1.0f;

    std::sort(peaks.begin(), peaks.end(),
              [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                  return a.second > b.second;
              });

    for (const auto& candidate : peaks)
    {
        float f0 = candidate.first;
        if (f0 < minFreq || f0 > maxFreq)
            continue;

        int harmonicCount = 0;

        for (int h = 2; h <= 5; ++h)
        {
            float harmonicFreq = f0 * static_cast<float>(h);
            if (harmonicFreq > maxFreq)
                break;

            float minDist = 1000.0f;
            float closestMag = -100.0f;

            for (const auto& peak : peaks)
            {
                float ratio = peak.first / harmonicFreq;
                if (ratio > 0.9f && ratio < 1.1f)
                {
                    float dist = std::abs(peak.first - harmonicFreq);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        closestMag = peak.second;
                    }
                }
            }

            if (closestMag > -80.0f)
                harmonicCount++;
        }

        if (harmonicCount >= 2)
            return f0;
    }

    return -1.0f;
}

float AIEngine::bandwidthFromSpectrum(const std::vector<float>& bins, int peakBin) const
{
    const int specSize = static_cast<int>(bins.size());
    if (specSize == 0 || peakBin < 2 || peakBin >= specSize - 2)
        return 100.0f;  // Default fallback (same as calculateBandwidth)

    float peakMag = bins[static_cast<size_t>(peakBin)];
    float threshold3dB = peakMag - 3.0f;

    int leftBin = peakBin;
    for (int i = peakBin - 1; i >= 0 && i >= peakBin - 50; --i)
    {
        if (i >= 0 && i < specSize && bins[static_cast<size_t>(i)] < threshold3dB)
        {
            leftBin = i;
            break;
        }
        leftBin = i;
    }

    int rightBin = peakBin;
    for (int i = peakBin + 1; i < specSize && i <= peakBin + 50; ++i)
    {
        if (bins[static_cast<size_t>(i)] < threshold3dB)
        {
            rightBin = i;
            break;
        }
        rightBin = i;
    }

    float leftFreq = binToFrequency(leftBin);
    float rightFreq = binToFrequency(rightBin);
    return std::max(10.0f, rightFreq - leftFreq);
}
// ── end B2 pure helpers ────────────────────────────────────────────────────

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

juce::String AIEngine::getFilterTypeName(Correction::FilterType type)
{
    switch (type)
    {
        case Correction::FilterType::Peak:      return "Peak";
        case Correction::FilterType::LowShelf:  return "Low Shelf";
        case Correction::FilterType::HighShelf: return "High Shelf";
        case Correction::FilterType::LowCut:    return "High Pass";
        case Correction::FilterType::HighCut:   return "Low Pass";
        case Correction::FilterType::Notch:     return "Notch";
        default:                                return "Peak";
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
    // Use percentile-based threshold (more robust than RMS-based)
    return calculateAdaptiveThresholdPercentile(baseThreshold);
}

float AIEngine::calculateAdaptiveThresholdPercentile(float baseThreshold) const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    
    if (snapshot.version == 0)
    {
        // Fallback to RMS-based if spectrum not yet written
        float rmsOffset = (averageRMS - (-40.0f)) * 0.15f;
        float adaptedThreshold = baseThreshold + rmsOffset;
        float sensMultiplier = getSensitivityMultiplier();
        return adaptedThreshold * sensMultiplier;
    }
    
    // Convert to vector for percentile calculation (reuse scratch buffer)
    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());
    auto& spectrumCopy = scratchTemp;
    
    // Calculate percentiles for robust threshold adaptation
    float percentile95 = calculatePercentile(spectrumCopy, 0.95f);
    float percentile50 = calculatePercentile(spectrumCopy, 0.50f);
    float percentile5 = calculatePercentile(spectrumCopy, 0.05f);
    
    // Dynamic range: difference between 95th and 50th percentile
    float dynamicRange = percentile95 - percentile50;
    
    // Spectral spread: how spread out the spectrum is
    float spectralSpread = percentile95 - percentile5;
    
    // Adjust threshold based on dynamic range
    // Higher dynamic range = more variation = need higher threshold
    float rangeFactor = 1.0f + (dynamicRange / 20.0f) * 0.3f;  // Up to 30% increase
    
    // Adjust based on spectral spread
    // Narrow spread = focused energy = lower threshold needed
    float spreadFactor = 1.0f - (spectralSpread < 30.0f ? (30.0f - spectralSpread) / 100.0f : 0.0f);
    
    // Combine with RMS for additional context
    float rmsOffset = (averageRMS - (-40.0f)) * 0.1f;  // Reduced weight
    
    float adaptedThreshold = baseThreshold * rangeFactor * spreadFactor + rmsOffset;
    
    // Apply sensitivity with exponential curve
    float sensMultiplier = getSensitivityMultiplier();
    return adaptedThreshold * sensMultiplier;
}

float AIEngine::calculatePercentile(const std::vector<float>& data, float percentile) const
{
    if (data.empty())
        return -100.0f;

    // Filter invalid values first, then use nth_element (O(n)) instead of sort (O(n log n))
    std::vector<float> filtered;
    filtered.reserve(data.size());
    for (float v : data)
        if (v >= -99.0f) filtered.push_back(v);

    if (filtered.empty())
        return -100.0f;

    float index = percentile * static_cast<float>(filtered.size() - 1);
    int lowerIndex = static_cast<int>(std::floor(index));
    lowerIndex = juce::jlimit(0, static_cast<int>(filtered.size()) - 1, lowerIndex);
    int upperIndex = juce::jlimit(0, static_cast<int>(filtered.size()) - 1, lowerIndex + 1);

    // nth_element partially sorts so filtered[lowerIndex] is the correct percentile value
    std::nth_element(filtered.begin(), filtered.begin() + lowerIndex, filtered.end());
    float lower = filtered[static_cast<size_t>(lowerIndex)];

    if (lowerIndex == upperIndex)
        return lower;

    // Need upper value too — nth_element again on the remaining range
    std::nth_element(filtered.begin() + lowerIndex + 1, filtered.begin() + upperIndex, filtered.end());
    float upper = filtered[static_cast<size_t>(upperIndex)];

    float weight = index - static_cast<float>(lowerIndex);
    return lower * (1.0f - weight) + upper * weight;
}

// Calculate local z-score for a bin within a sliding window
float AIEngine::computeZScore(const std::vector<float>& spectrum, int centerBin, int window) const
{
    if (spectrum.empty() || centerBin < 0 || centerBin >= static_cast<int>(spectrum.size()))
        return 0.0f;
    
    int halfWindow = juce::jmax(2, window / 2);
    int start = juce::jmax(0, centerBin - halfWindow);
    int end = juce::jmin(static_cast<int>(spectrum.size()) - 1, centerBin + halfWindow);
    
    float sum = 0.0f;
    int count = 0;
    for (int i = start; i <= end; ++i)
    {
        if (i == centerBin)
            continue;
        float v = spectrum[i];
        if (v > -120.0f)
        {
            sum += v;
            ++count;
        }
    }
    
    if (count < 4)
        return 0.0f;
    
    float mean = sum / static_cast<float>(count);
    
    float var = 0.0f;
    for (int i = start; i <= end; ++i)
    {
        if (i == centerBin)
            continue;
        float v = spectrum[i];
        if (v > -120.0f)
        {
            float d = v - mean;
            var += d * d;
        }
    }
    
    float stddev = std::sqrt((var / static_cast<float>(count)) + 1e-6f);
    float centerVal = spectrum[centerBin];
    return (centerVal - mean) / juce::jmax(1e-6f, stddev);
}

// LOCK-FREE z-score at frequency
float AIEngine::computeZScoreAtFrequency(float frequency, int window) const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return 0.0f;
    
    // Convert to vector for computeZScore (reuse scratch buffer)
    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());
    auto& spectrumCopy = scratchTemp;
    
    int bin = frequencyToBin(frequency);
    bin = juce::jlimit(0, numBins - 1, bin);
    return computeZScore(spectrumCopy, bin, window);
}

// Require multi-frame consensus for stability (spread in last frames must be small)
bool AIEngine::hasTemporalConsensus(const PeakCandidate& peak, int minFrames, float magToleranceDb) const
{
    if (peak.frameCount < minFrames)
        return false;
    if (static_cast<int>(peak.magnitudeHistory.size()) < minFrames)
        return false;
    
    const int take = juce::jmin(minFrames, static_cast<int>(peak.magnitudeHistory.size()));
    float recentMin = 200.0f;
    float recentMax = -200.0f;
    for (int i = static_cast<int>(peak.magnitudeHistory.size()) - take; i < static_cast<int>(peak.magnitudeHistory.size()); ++i)
    {
        float v = peak.magnitudeHistory[static_cast<size_t>(i)];
        recentMin = juce::jmin(recentMin, v);
        recentMax = juce::jmax(recentMax, v);
    }
    
    float spread = recentMax - recentMin;
    bool stableMagnitude = spread <= magToleranceDb;
    bool stablePattern = (peak.stability >= 0.3f) && (peak.consistency >= 0.3f);
    return stableMagnitude && stablePattern;
}

// Simple contextual whitelist to avoid flagging expected content
bool AIEngine::isContextuallyNormal(ProblemType type, float frequency) const
{
    // FIX: Load atomic sourceProfile
    const auto profile = static_cast<SourceProfile>(sourceProfile.load(std::memory_order_relaxed));
    
    switch (profile)
    {
        case SourceProfile::Drums:
        case SourceProfile::Techno:
            if ((type == ProblemType::Resonance || type == ProblemType::LowEndBoom) &&
                frequency >= 40.0f && frequency <= 90.0f)
                return true;  // Kick fundamental usually here
            break;
        case SourceProfile::Bass:
            if ((type == ProblemType::Resonance || type == ProblemType::LowEndBoom) &&
                frequency >= 40.0f && frequency <= 120.0f)
                return true;  // Bass fundamentals
            break;
        case SourceProfile::Vocals:
            if (type == ProblemType::Resonance &&
                frequency >= 180.0f && frequency <= 400.0f)
                return true;  // Vocal formants often here
            break;
        default:
            break;
    }
    return false;
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
    // Use lock-free snapshot to avoid deadlock when called while correctionsWriteMutex is held
    const auto snapshot = readSpectrumSnapshot();
    const auto& spectrum = snapshot.bins;

    // Find -3dB points on either side of peak
    const int specSize = static_cast<int>(spectrum.size());
    if (specSize == 0 || peakBin < 2 || peakBin >= specSize - 2)
        return 100.0f;  // Default fallback

    float peakMag = spectrum[static_cast<size_t>(peakBin)];
    float threshold3dB = peakMag - 3.0f;

    // Search left for -3dB point
    int leftBin = peakBin;
    for (int i = peakBin - 1; i >= 0 && i >= peakBin - 50; --i)
    {
        if (i >= 0 && i < specSize && spectrum[static_cast<size_t>(i)] < threshold3dB)
        {
            leftBin = i;
            break;
        }
        leftBin = i;
    }

    // Search right for -3dB point
    int rightBin = peakBin;
    for (int i = peakBin + 1; i < specSize && i <= peakBin + 50; ++i)
    {
        if (spectrum[static_cast<size_t>(i)] < threshold3dB)
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
                existing.prominenceDb = (existing.prominenceDb + newPeak.prominenceDb) * 0.5f;  // Smooth
                existing.bandwidth = (existing.bandwidth + newPeak.bandwidth) * 0.5f;
                existing.calculatedQ = newPeak.calculatedQ;
                existing.octaveSalienceGate = existing.octaveSalienceGate || newPeak.octaveSalienceGate;
                existing.frameCount++;
                
                // Update magnitude history for temporal analysis
                existing.magnitudeHistory.push_back(newPeak.magnitude);
                if (existing.magnitudeHistory.size() > 10)
                    existing.magnitudeHistory.erase(existing.magnitudeHistory.begin());
                
                // Analyze temporal pattern
                analyzeTemporalPattern(existing);
                
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            // Add new peak
            PeakCandidate peak = newPeak;
            peak.frameCount = 1;
            peak.magnitudeHistory.push_back(newPeak.magnitude);
            peak.stability = 0.5f;  // Initial stability
            peak.consistency = 0.5f;
            peak.attackDecay = 0.0f;
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
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return;

    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());

    // ── FIX: AIEngine stores spectra in dB; MLEngine expects linear magnitude.
    //    Convert before passing to the ML forward pass.
    auto linearSpectrum = convertDbSpectrumToLinearMagnitude(scratchTemp);

    // Set ML context based on source profile
    MLEngine::GenreType mlContext = MLEngine::GenreType::Unknown;

    const auto profile = static_cast<SourceProfile>(sourceProfile.load(std::memory_order_relaxed));

    switch (profile)
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
    const float sens = sensitivity.load(std::memory_order_relaxed);
    mlEngine.setSensitivity(sens);

    // ── Effective thresholds for the audit snapshot (test instrumentation) ──
    // NOTE: the snapshot itself is stored AFTER detectProblems() below, together
    // with the raw probabilities of THAT inference. The previous code stored it
    // here passing lastMLRawProbabilities to itself (self-assignment), so the
    // raw-probability audit hook never carried fresh data.
    std::array<float, MLEngine::numProblemTypes> effectiveThresholds {};
    {
        const auto& base = mlEngine.getBaseThresholds();
        float sensitivityScale = 1.0f - (sens - 0.5f) * 0.6f;
        for (int i = 0; i < MLEngine::numProblemTypes; ++i)
            effectiveThresholds[static_cast<size_t>(i)] = base[static_cast<size_t>(i)] * sensitivityScale;
    }

    // Optional: run TFLite NN to modulate confidence if available
    std::array<float, MLEngine::numProblemTypes> nnConfidence{};
    nnConfidence.fill(1.0f);
    if (enableNeuralNetworks && neuralNetwork && neuralNetwork->isModelLoaded())
    {
        const auto nnResult = neuralNetwork->runInference(linearSpectrum);
        if (nnResult.success && !nnResult.output.empty())
        {
            const size_t count = std::min(nnResult.output.size(), static_cast<size_t>(MLEngine::numProblemTypes));
            for (size_t i = 0; i < count; ++i)
                nnConfidence[i] = juce::jlimit(0.0f, 1.0f, nnResult.output[i]);
        }
        else
        {
            AIEQ_LOG_WARNING("TFLite inference failed or empty output. Falling back to classical ML.");
        }
    }

    // Run ML detection on LINEAR MAGNITUDE spectrum. The optional out-param
    // captures the raw sigmoid outputs of THIS inference (no second forward pass).
    std::array<float, MLEngine::numProblemTypes> rawProbsThisInference {};
    auto mlDetections = mlEngine.detectProblems(linearSpectrum, currentSampleRate,
                                                &rawProbsThisInference);
    storeMLAuditSnapshot(rawProbsThisInference, effectiveThresholds);

   #if JUCE_UNIT_TESTS
    // P4-BUG-001 closure: snapshot the FULL pre-veto ML detection list (test-only populate;
    // the data member itself is unconditional for layout safety — P2-HAZARD-001).
    {
        std::lock_guard<std::mutex> auditLock(mlAuditMutex);
        lastPreVetoMLDetectionsForTests = mlDetections;
    }
   #endif

    // Convert ML detections to AIEngine corrections
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    pendingCorrections.clear();

    // Commit 4B — ML spectral existence check (reality check, external to the model).
    // The model asserts e.g. Resonance@c=0.95 even on a flat spectrum where no peak
    // exists. Before accepting each mapped correction we verify it against the
    // published dB spectrum using the existing thread-safe helpers. Constants are
    // INDEPENDENT of sensitivity (same invariant as 4A / the heuristic gate).
    // Commit 4C — Resonance veto is PEAK PROMINENCE, not z-score. Empirically the
    // z-score on the snapped peak does NOT separate: the resonance's own skirt inflates
    // the local window mean/variance, crushing a genuine +20dB Res@800 to z21~=1.9
    // (min 0.66) which overlaps clean (max ~3.08). Prominence (narrow-band peak level
    // minus a +-1 octave local band, no stddev division) separates cleanly:
    //   real Res@800 prominence 11.99..19.20 dB   vs   clean <=10.06 dB.
    // This is the peak-local twin of the band-excess used for the broad classes, so the
    // whole reality-check stays homogeneous: peak-like -> prominence, broad -> band-excess.
    constexpr float kResonanceProminenceDb = 11.0f; // peak vs +-1 octave local floor
    constexpr float kBandExcessDb = 3.0f;  // dB the problem band must exceed its reference
    constexpr float kBoomLo       = 30.0f; // LowEndBoom has no thresholds range — local span
    constexpr float kBoomHi       = 100.0f;
    // NOTE on the band-excess veto family (Muddiness/Boxyness/Sibilance/LowEndBoom):
    // all use `band - wide_reference >= kBandExcessDb`. This is SYSTEMICALLY biased by the
    // HF rolloff of natural pink tilt: on a steep (-6 dB/decade) but perfectly clean
    // spectrum the low-mid band reads as a multi-dB "excess" and false-positives (measured
    // via the CleanSteep control in AIBackendSweepTest: at cap 3 both Mud and Bxy fire on
    // clean steep tilt). Two candidate fixes were measured and BOTH have costs:
    //   - narrow reference (100..1000): tilt-robust but contaminated by a nearby resonance
    //     -> masks real Mud@250 when Res@800 coexists (recall loss, even at cap 2);
    //   - wide reference + higher threshold (4.5 dB): peak-robust but tilt-FRAGILE -> still
    //     hallucinates Mud on steep clean tilt (the anti-hallucination goal forbids this).
    // Ticket #2: Boxyness and LowEndBoom now use a local log-frequency trend baseline
    // instead of a wide linear-bin average. This preserves the original 3 dB existence
    // rule but makes the measurement tilt-invariant: a monotonic dark/bass-heavy slope
    // predicts a matching baseline at the problem-band centre, while a true local hump
    // rises above it. Follow-up #2 migrates Muddiness to the same trend helper using
    // measured Mud-B references (80..140 + 900..1600), which recover modest mud under
    // Res@800 without false-mudding CleanSteep, CleanBassTilt, or Res@800-only controls.
    // Sibilance keeps its local HF reference until a separate measurement says otherwise.
    auto medianOfValues = [](std::vector<float> values) -> float
    {
        if (values.empty())
            return -100.0f;

        std::sort(values.begin(), values.end());
        const size_t mid = values.size() / 2;
        if ((values.size() & 1u) != 0)
            return values[mid];

        return 0.5f * (values[mid - 1] + values[mid]);
    };

    auto logBucketLevels = [&](float loHz, float hiHz, int buckets)
    {
        std::vector<std::pair<float, float>> levels;
        if (loHz <= 0.0f || hiHz <= loHz || buckets <= 0)
            return levels;

        const float logLo = std::log10(loHz);
        const float logHi = std::log10(hiHz);
        const float binHz = static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);

        for (int bucket = 0; bucket < buckets; ++bucket)
        {
            const float t0 = static_cast<float>(bucket) / static_cast<float>(buckets);
            const float t1 = static_cast<float>(bucket + 1) / static_cast<float>(buckets);
            const float bucketLo = std::pow(10.0f, logLo + (logHi - logLo) * t0);
            const float bucketHi = std::pow(10.0f, logLo + (logHi - logLo) * t1);
            const float center = std::sqrt(bucketLo * bucketHi);

            const int loBin = juce::jlimit(1, numBins - 1, static_cast<int>(std::floor(bucketLo / binHz)));
            const int hiBin = juce::jlimit(1, numBins - 1, static_cast<int>(std::ceil(bucketHi / binHz)));

            std::vector<float> values;
            values.reserve(static_cast<size_t>(juce::jmax(0, hiBin - loBin + 1)));
            for (int bin = loBin; bin <= hiBin && bin < static_cast<int>(scratchTemp.size()); ++bin)
            {
                const float f = binToFrequency(bin);
                if (f >= bucketLo && f <= bucketHi)
                    values.push_back(scratchTemp[static_cast<size_t>(bin)]);
            }

            if (! values.empty())
                levels.push_back({ center, medianOfValues(std::move(values)) });
        }

        return levels;
    };

    auto computeTrendBandExcess = [&](float problemLo,
                                      float problemHi,
                                      std::initializer_list<std::pair<float, float>> referenceBands)
    {
        const auto problemBuckets = logBucketLevels(problemLo, problemHi, 10);
        std::vector<float> problemLevels;
        problemLevels.reserve(problemBuckets.size());
        for (const auto& bucket : problemBuckets)
            problemLevels.push_back(bucket.second);

        const float problemLevelDb = medianOfValues(std::move(problemLevels));

        std::vector<std::pair<float, float>> refs;
        for (const auto& ref : referenceBands)
        {
            auto levels = logBucketLevels(ref.first, ref.second, 8);
            refs.insert(refs.end(), levels.begin(), levels.end());
        }

        if (refs.size() < 2)
            return 0.0f;

        double n = 0.0, sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
        for (const auto& ref : refs)
        {
            const double x = std::log10(static_cast<double>(juce::jmax(1.0f, ref.first)));
            const double y = static_cast<double>(ref.second);
            n += 1.0;
            sumX += x;
            sumY += y;
            sumXX += x * x;
            sumXY += x * y;
        }

        float trendBaselineDb = 0.0f;
        const double denom = n * sumXX - sumX * sumX;
        if (std::abs(denom) < 1e-9)
        {
            std::vector<float> refLevels;
            refLevels.reserve(refs.size());
            for (const auto& ref : refs)
                refLevels.push_back(ref.second);
            trendBaselineDb = medianOfValues(std::move(refLevels));
        }
        else
        {
            const double slope = (n * sumXY - sumX * sumY) / denom;
            const double intercept = (sumY - slope * sumX) / n;
            const float problemCenter = std::sqrt(problemLo * problemHi);
            trendBaselineDb = static_cast<float>(slope * std::log10(problemCenter) + intercept);
        }

        return problemLevelDb - trendBaselineDb;
    };

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
                // Slot-7 semantics are keyed to the LOADED blob schema (M7
                // interim, v2 loader): self-describing product-v2 blobs define
                // slot 7 = DullSound; every legacy/v1 blob keeps the historical
                // Clipping->Harshness mapping byte-identically.
                c.type = (mlEngine.getLoadedProblemSchema() == "product-v2")
                             ? ProblemType::DullSound
                             : ProblemType::Harshness;
                break;
            default:
                c.type = ProblemType::None;
                break;
        }

        if (c.type == ProblemType::None)
            continue;

        c.frequency = mlDet.frequency;

        //----------------------------------------------------------------------
        // Commit 4B: validate this correction against the REAL spectrum before
        // accepting it. Drop (continue) if the asserted problem has no spectral
        // support. Done here, before Q/bandwidth/filter/description are derived,
        // so a snapped Resonance frequency propagates to all of them.
        //----------------------------------------------------------------------
        {
            bool keep = true;
            switch (c.type)
            {
                case ProblemType::Resonance:
                {
                    // Peak-snap FIRST: the ML frequency is biased low (a true
                    // Resonance@800 emerges at ML ~602). findPeakInRange only
                    // LOCATES the window max — it never reports "no peak" — so the
                    // real veto is PROMINENCE, not peak existence. The bounded ~+-1
                    // octave window keeps the candidate near the ML prediction so a
                    // far-off unrelated peak can't rescue an FP.
                    const float lo = juce::jmax(20.0f, c.frequency * 0.5f);
                    const float hi = juce::jmin(static_cast<float>(currentSampleRate) * 0.5f,
                                                c.frequency * 2.0f);
                    // P4-BUG-001: read peak + both prominence bands from the SINGLE captured
                    // frame (scratchTemp), not via consuming readSpectrumSnapshot re-reads.
                    const float actualPeak = findPeakInSpectrum(scratchTemp, lo, hi);
                    if (actualPeak <= 0.0f)
                    {
                        keep = false;
                    }
                    else
                    {
                        // Prominence = narrow-band level at the peak minus the local
                        // +-1 octave band mean. A genuine narrow resonance towers over
                        // its surroundings; a flat/clean window max does not.
                        const float prominence =
                              bandEnergyFromSpectrum(scratchTemp, actualPeak * 0.975f, actualPeak * 1.025f)
                            - bandEnergyFromSpectrum(scratchTemp, actualPeak * 0.5f,   actualPeak * 2.0f);
                        if (prominence < kResonanceProminenceDb)
                            keep = false;             // flat spectrum: window max is no real peak
                        else
                            c.frequency = actualPeak; // snap (also fixes 602->800 localization bias)
                    }
                    break;
                }
                case ProblemType::Muddiness:
                    // Measured Mud-B trend reference: below the low-mid shelf plus a high
                    // skip band above the boxy/resonance region. The below-only legacy veto
                    // was too local and missed modest Mud@250 when a dominant Res@800
                    // coexisted; 900..1600 keeps the slope fit wide without letting the
                    // 800 Hz resonance contaminate the baseline.
                    keep = computeTrendBandExcess(thresholds.muddinessLow, thresholds.muddinessHigh,
                                                   { { 80.0f, 140.0f }, { 900.0f, 1600.0f } }) >= kBandExcessDb;
                    break;
                case ProblemType::Boxyness:
                    keep = computeTrendBandExcess(thresholds.boxyLow, thresholds.boxyHigh,
                                                   { { 150.0f, 280.0f }, { 850.0f, 1600.0f } }) >= kBandExcessDb;
                    break;
                case ProblemType::Sibilance:
                    // HF-local veto: sib band vs 3-5k and 10-14k flanking reference.
                    // The old 2-5k reference is hot in vocals and prunes true sibilance.
                    // P4-BUG-001: all bands from the SINGLE captured frame (scratchTemp).
                    keep = (bandEnergyFromSpectrum(scratchTemp, thresholds.sibilanceLow, thresholds.sibilanceHigh)
                            - 0.5f * (bandEnergyFromSpectrum(scratchTemp, 3000.0f, 5000.0f)
                                    + bandEnergyFromSpectrum(scratchTemp, 10000.0f, 14000.0f))) >= kBandExcessDb;
                    break;
                case ProblemType::LowEndBoom:
                    keep = computeTrendBandExcess(kBoomLo, kBoomHi,
                                                   { { 100.0f, 500.0f } }) >= kBandExcessDb;
                    break;
                default:
                    // Harshness / ThinSound / DullSound: no validated rule yet and
                    // not part of the measured clean floor — leave conservative.
                    break;
            }
            if (! keep)
                continue;
        }

        c.suggestedGain = mlDet.suggestedGain;
        c.suggestedQ = mlDet.suggestedQ;
        if (c.suggestedQ <= 0.0f)
            c.suggestedQ = 1.0f;
        float bandwidth = c.suggestedQ > 0.0f ? c.frequency / c.suggestedQ : 0.0f;
        float peakHeight = std::abs(c.suggestedGain);
        c.suggestedFilter = selectOptimalFilterType(c.type, c.frequency, bandwidth, peakHeight);
        // Shelves render Q as corner resonance; cap to a clean Butterworth slope.
        if (c.suggestedFilter == Correction::FilterType::LowShelf
            || c.suggestedFilter == Correction::FilterType::HighShelf)
            c.suggestedQ = std::min(c.suggestedQ, 0.71f);
        const int typeIndex = static_cast<int>(mlDet.type);
        const float nnConf = (typeIndex >= 0 && typeIndex < MLEngine::numProblemTypes) ? nnConfidence[static_cast<size_t>(typeIndex)] : 1.0f;
        c.severity = mlDet.severity * nnConf;
        c.confidence = mlDet.confidence * nnConf;
        c.approved = false;

        // Generate description
        juce::String bandName = getBandName(c.frequency);
        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - ML Suggested: %s %.1f dB, Q: %.1f (Conf: %.0f%%)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            bandName.toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            c.confidence * 100.0f);

        pendingCorrections.push_back(c);
    }

    // Interim seed22 build: no heuristic Resonance Assist. VOXDIAG1-6 showed that,
    // on vocals, the spectral resonance detector cannot reliably separate formants
    // from true resonances without a reference signal. Resonance-on-voice therefore
    // remains a model-side follow-up rather than a runtime heuristic supplement.

    // Sort by type and frequency first, so std::unique can find all duplicates
    // (std::unique only removes consecutive duplicates)
    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  if (a.type != b.type)
                      return static_cast<int>(a.type) < static_cast<int>(b.type);
                  return a.frequency < b.frequency;
              });

    // Remove duplicates (same type within 10% frequency range)
    auto it = std::unique(pendingCorrections.begin(), pendingCorrections.end(),
        [](const Correction& a, const Correction& b) {
            if (a.type != b.type)
                return false;
            float ratio = a.frequency / b.frequency;
            return ratio > 0.9f && ratio < 1.1f;
        });
    pendingCorrections.erase(it, pendingCorrections.end());

    // Sort by priority (severity * confidence, highest first)
    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  float priorityA = a.severity * a.confidence;
                  float priorityB = b.severity * b.confidence;
                  if (std::abs(priorityA - priorityB) < 0.01f)
                      return a.severity > b.severity;  // Tie-break by severity
                  return priorityA > priorityB;
              });

    // No hard limit - let filtering/merging handle it
}//==============================================================================
// Intelligent Band Assignment - Filtering and Merging
//==============================================================================

std::vector<AIEngine::Correction> AIEngine::getFilteredAndPrioritizedCorrections(
    float minSeverity,
    float minConfidence) const
{
    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    
    std::vector<Correction> filtered;
    filtered.reserve(pendingCorrections.size());
    
    // Filter by thresholds.
    // Normal UI usage stays permissive, but explicit max thresholds (1.0 / 1.0)
    // are treated as a strict request and must not be softened.
    const bool strictThresholds = (minSeverity >= 1.0f || minConfidence >= 1.0f);

    for (const auto& c : pendingCorrections)
    {
        float adjustedMinSeverity = strictThresholds ? minSeverity : (minSeverity * 0.4f);
        float adjustedMinConfidence = strictThresholds ? minConfidence : (minConfidence * 0.5f);
        
        // Type-specific thresholds (some problems need different sensitivity)
        float typeMinSeverity = adjustedMinSeverity;
        float typeMinConfidence = adjustedMinConfidence;
        
        if (!strictThresholds)
        {
            switch (c.type)
            {
                case ProblemType::Resonance:
                case ProblemType::Harshness:
                case ProblemType::Sibilance:
                    // Critical problems: even lower thresholds (more sensitive)
                    typeMinSeverity = adjustedMinSeverity * 0.5f;
                    typeMinConfidence = adjustedMinConfidence * 0.6f;
                    break;
                    
                case ProblemType::ThinSound:
                case ProblemType::DullSound:
                    // Subtle problems: slightly higher but still lenient
                    typeMinSeverity = adjustedMinSeverity * 0.9f;
                    typeMinConfidence = adjustedMinConfidence * 0.8f;
                    break;
                    
                default:
                    break;
            }
        }
        
        // Also accept if priority (severity * confidence) is above a minimum threshold
        // during normal permissive UI usage. In strict mode, respect the caller's
        // thresholds exactly.
        float priority = c.severity * c.confidence;
        const float minPriority = 0.03f;
        const bool passesThresholds = (c.severity >= typeMinSeverity && c.confidence >= typeMinConfidence);
        const bool passesPriorityBypass = (!strictThresholds && priority >= minPriority);
        
        if (passesThresholds || passesPriorityBypass)
        {
            filtered.push_back(c);
        }
    }
    
    // Sort by priority (severity * confidence)
    std::sort(filtered.begin(), filtered.end(),
              [](const Correction& a, const Correction& b) {
                  float priorityA = a.severity * a.confidence;
                  float priorityB = b.severity * b.confidence;
                  if (std::abs(priorityA - priorityB) < 0.01f)
                      return a.severity > b.severity;
                  return priorityA > priorityB;
              });
    
    return filtered;
}

std::vector<AIEngine::Correction> AIEngine::mergeNearbyCorrections(
    const std::vector<Correction>& corrections) const
{
    if (corrections.empty())
        return corrections;
    
    std::vector<Correction> merged;
    merged.reserve(corrections.size());
    
    // Group by problem type first
    std::map<ProblemType, std::vector<Correction>> byType;
    for (const auto& c : corrections)
    {
        byType[c.type].push_back(c);
    }
    
    // Merge within each type
    for (auto& [type, group] : byType)
    {
        // Sort by frequency
        std::sort(group.begin(), group.end(),
                  [](const Correction& a, const Correction& b) {
                      return a.frequency < b.frequency;
                  });
        
        // Merge nearby (within 1/3 octave = ~26% frequency difference)
        for (size_t i = 0; i < group.size(); ++i)
        {
            Correction mergedCorr = group[i];
            int mergeCount = 1;
            
            // Look ahead for nearby corrections
            for (size_t j = i + 1; j < group.size(); ++j)
            {
                float ratio = group[j].frequency / mergedCorr.frequency;
                
                // Within 1/3 octave (0.794 to 1.26)
                if (ratio >= 0.794f && ratio <= 1.26f)
                {
                    // Weighted average (by severity)
                    float totalSeverity = mergedCorr.severity + group[j].severity;
                    if (totalSeverity > 0.0f)
                    {
                        float w1 = mergedCorr.severity / totalSeverity;
                        float w2 = group[j].severity / totalSeverity;
                        
                        mergedCorr.frequency = mergedCorr.frequency * w1 + group[j].frequency * w2;
                        mergedCorr.suggestedGain = mergedCorr.suggestedGain * w1 + group[j].suggestedGain * w2;
                        mergedCorr.suggestedQ = mergedCorr.suggestedQ * w1 + group[j].suggestedQ * w2;
                        mergedCorr.severity = std::max(mergedCorr.severity, group[j].severity);
                        mergedCorr.confidence = (mergedCorr.confidence + group[j].confidence) * 0.5f;
                    }
                    mergeCount++;
                }
                else
                {
                    // Too far, stop looking
                    break;
                }
            }
            
            // Update description
            if (mergeCount > 1)
            {
                mergedCorr.description = mergedCorr.description + " (merged " + juce::String(mergeCount) + ")";
            }
            
            merged.push_back(mergedCorr);
            i += mergeCount - 1;  // Skip merged items
        }
    }
    
    // Re-sort by priority
    std::sort(merged.begin(), merged.end(),
              [](const Correction& a, const Correction& b) {
                  float priorityA = a.severity * a.confidence;
                  float priorityB = b.severity * b.confidence;
                  if (std::abs(priorityA - priorityB) < 0.01f)
                      return a.severity > b.severity;
                  return priorityA > priorityB;
              });
    
    return merged;
}

//==============================================================================
// Advanced Detection Improvements Implementation

void AIEngine::analyzeTemporalPattern(PeakCandidate& peak) const
{
    if (peak.magnitudeHistory.size() < 3)
    {
        // Not enough data yet
        peak.stability = 0.5f;
        peak.consistency = 0.5f;
        peak.attackDecay = 0.0f;
        return;
    }
    
    // Calculate stability: variance of magnitudes (lower variance = higher stability)
    float mean = 0.0f;
    for (float mag : peak.magnitudeHistory)
        mean += mag;
    mean /= static_cast<float>(peak.magnitudeHistory.size());
    
    float variance = 0.0f;
    for (float mag : peak.magnitudeHistory)
    {
        float diff = mag - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(peak.magnitudeHistory.size());
    
    // Convert variance to stability (0-1)
    // Lower variance (more stable) = higher stability
    float stdDev = std::sqrt(variance);
    peak.stability = juce::jlimit(0.0f, 1.0f, 1.0f - (stdDev / 10.0f));  // 10dB std dev = 0 stability
    
    // Calculate consistency: how consistent the peak is across frames
    // Check if peak is consistently above a threshold
    float threshold = mean - 3.0f;  // 3dB below mean
    int aboveThreshold = 0;
    for (float mag : peak.magnitudeHistory)
    {
        if (mag > threshold)
            aboveThreshold++;
    }
    peak.consistency = static_cast<float>(aboveThreshold) / static_cast<float>(peak.magnitudeHistory.size());
    
    // Calculate attack/decay: trend in magnitude over time
    // Positive = stable/increasing, negative = decreasing/transient
    if (peak.magnitudeHistory.size() >= 3)
    {
        float firstThird = 0.0f, lastThird = 0.0f;
        int firstCount = 0, lastCount = 0;
        
        int thirdSize = static_cast<int>(peak.magnitudeHistory.size()) / 3;
        for (int i = 0; i < thirdSize; ++i)
        {
            firstThird += peak.magnitudeHistory[i];
            firstCount++;
        }
        for (int i = static_cast<int>(peak.magnitudeHistory.size()) - thirdSize; 
             i < static_cast<int>(peak.magnitudeHistory.size()); ++i)
        {
            lastThird += peak.magnitudeHistory[i];
            lastCount++;
        }
        
        if (firstCount > 0 && lastCount > 0)
        {
            firstThird /= static_cast<float>(firstCount);
            lastThird /= static_cast<float>(lastCount);
            peak.attackDecay = lastThird - firstThird;  // Positive = stable, negative = transient
        }
    }
}

float AIEngine::crossValidateDetection(ProblemType type, float frequency, float magnitude) const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return 0.5f;  // Neutral confidence if no spectrum
    
    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());
    auto& spectrumCopy = scratchTemp;
    
    // Find bin for this frequency
    int bin = frequencyToBin(frequency);
    if (bin < 0 || bin >= static_cast<int>(spectrumCopy.size()))
        return 0.5f;
    
    // Use existing frequencyToBin which is already implemented
    
    float confidence = 0.5f;  // Base confidence
    int validationCount = 0;
    float confidenceSum = 0.0f;
    
    // Method 1: Check if magnitude is significantly above surrounding bins
    float surroundAvg = 0.0f;
    int surroundCount = 0;
    int window = 5;
    const int specSize = static_cast<int>(spectrumCopy.size());
    for (int i = juce::jmax(0, bin - window); i <= juce::jmin(specSize - 1, bin + window); ++i)
    {
        if (i != bin && std::abs(i - bin) >= 2)  // Exclude center ±1
        {
            surroundAvg += spectrumCopy[static_cast<size_t>(i)];
            surroundCount++;
        }
    }
    if (surroundCount > 0)
    {
        surroundAvg /= static_cast<float>(surroundCount);
        float prominence = magnitude - surroundAvg;
        float method1Conf = juce::jlimit(0.0f, 1.0f, prominence / 6.0f);  // 6dB prominence = full confidence
        confidenceSum += method1Conf;
        validationCount++;
    }
    
    // Method 2: Check if frequency is in expected range for problem type
    bool inExpectedRange = false;
    switch (type)
    {
        case ProblemType::Resonance:
            inExpectedRange = (frequency >= 50.0f && frequency <= 15000.0f);
            break;
        case ProblemType::Harshness:
            inExpectedRange = (frequency >= 1000.0f && frequency <= 8000.0f);
            break;
        case ProblemType::Muddiness:
            inExpectedRange = (frequency >= 150.0f && frequency <= 500.0f);
            break;
        case ProblemType::Sibilance:
            inExpectedRange = (frequency >= 5000.0f && frequency <= 10000.0f);
            break;
        case ProblemType::LowEndBoom:
            inExpectedRange = (frequency >= 30.0f && frequency <= 100.0f);
            break;
        case ProblemType::ThinSound:
            inExpectedRange = (frequency >= 200.0f && frequency <= 600.0f);
            break;
        case ProblemType::DullSound:
            inExpectedRange = (frequency >= 8000.0f && frequency <= 16000.0f);
            break;
        case ProblemType::Boxyness:
            inExpectedRange = (frequency >= 400.0f && frequency <= 800.0f);
            break;
        default:
            inExpectedRange = true;  // No specific range
            break;
    }
    float method2Conf = inExpectedRange ? 0.8f : 0.3f;
    confidenceSum += method2Conf;
    validationCount++;
    
    // Method 3: Check if magnitude is above noise floor (more lenient)
    float noiseFloor = calculatePercentile(spectrumCopy, 0.10f);  // 10th percentile as noise floor
    float aboveNoise = magnitude - noiseFloor;
    float method3Conf = juce::jlimit(0.0f, 1.0f, aboveNoise / 10.0f);  // 10dB above noise = full confidence (was 15dB)
    confidenceSum += method3Conf;
    validationCount++;
    
    // Average of all validation methods
    if (validationCount > 0)
        confidence = confidenceSum / static_cast<float>(validationCount);
    
    return juce::jlimit(0.0f, 1.0f, confidence);
}

//==============================================================================
// FASE 2: Harmonic Analysis, Spectral Coherence, Dynamic Range Normalization
//==============================================================================

float AIEngine::findFundamentalFrequency(float minFreq, float maxFreq) const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return -1.0f;
    
    // Convert frequency range to bins
    int minBin = frequencyToBin(minFreq);
    int maxBin = frequencyToBin(maxFreq);
    
    // BOUNDS CHECK: Clamp to valid array indices
    const int arrSize = static_cast<int>(snapshot.bins.size());
    minBin = juce::jlimit(0, arrSize - 1, minBin);
    maxBin = juce::jlimit(0, arrSize - 1, maxBin);
    
    if (maxBin <= minBin || maxBin - minBin < 4)
        return -1.0f;
    
    // Autocorrelation-based fundamental detection
    // Find peaks in the spectrum and look for harmonic relationships
    std::vector<std::pair<float, float>> peaks;  // (frequency, magnitude)
    
    // Find local maxima in the range
    // BOUNDS CHECK: Ensure i-2 and i+2 are within valid range
    const int loopStart = std::max(2, minBin + 2);
    const int loopEnd = std::min(arrSize - 3, maxBin - 2);
    
    for (int i = loopStart; i < loopEnd; ++i)
    {
        if (snapshot.bins[static_cast<size_t>(i)] > snapshot.bins[static_cast<size_t>(i-1)] && 
            snapshot.bins[static_cast<size_t>(i)] > snapshot.bins[static_cast<size_t>(i+1)] &&
            snapshot.bins[static_cast<size_t>(i)] > snapshot.bins[static_cast<size_t>(i-2)] && 
            snapshot.bins[static_cast<size_t>(i)] > snapshot.bins[static_cast<size_t>(i+2)])
        {
            float freq = binToFrequency(i);
            float mag = snapshot.bins[static_cast<size_t>(i)];
            if (mag > -80.0f)  // Above noise floor
            {
                peaks.push_back({freq, mag});
            }
        }
    }
    
    if (peaks.empty())
        return -1.0f;
    
    // Sort by magnitude (strongest first)
    std::sort(peaks.begin(), peaks.end(),
              [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                  return a.second > b.second;
              });
    
    // Try each peak as potential fundamental
    // Check if other peaks are harmonics (2f, 3f, 4f...)
    for (const auto& candidate : peaks)
    {
        float f0 = candidate.first;
        if (f0 < minFreq || f0 > maxFreq)
            continue;
        
        int harmonicCount = 0;
        float harmonicStrength = 0.0f;
        
        // Check for harmonics up to 5th harmonic
        for (int h = 2; h <= 5; ++h)
        {
            float harmonicFreq = f0 * static_cast<float>(h);
            if (harmonicFreq > maxFreq)
                break;
            
            // Find peak closest to harmonic frequency
            float minDist = 1000.0f;
            float closestMag = -100.0f;
            
            for (const auto& peak : peaks)
            {
                float ratio = peak.first / harmonicFreq;
                if (ratio > 0.9f && ratio < 1.1f)  // Within 10%
                {
                    float dist = std::abs(peak.first - harmonicFreq);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        closestMag = peak.second;
                    }
                }
            }
            
            if (closestMag > -80.0f)
            {
                harmonicCount++;
                harmonicStrength += closestMag;
            }
        }
        
        // If we found at least 2 harmonics, this is likely the fundamental
        if (harmonicCount >= 2)
        {
            return f0;
        }
    }
    
    // Fallback: return strongest peak in range
    return peaks[0].first;
}

bool AIEngine::isHarmonicPeak(float peakFreq, float fundamentalFreq, float tolerance) const
{
    if (fundamentalFreq <= 0.0f || peakFreq <= 0.0f)
        return false;
    
    // Check if peakFreq is a multiple of fundamentalFreq
    float ratio = peakFreq / fundamentalFreq;
    
    // Check for harmonics: 1f, 2f, 3f, 4f, 5f, etc.
    for (int h = 1; h <= 8; ++h)
    {
        float expectedRatio = static_cast<float>(h);
        if (std::abs(ratio - expectedRatio) < tolerance)
        {
            return true;  // This is a harmonic
        }
    }
    
    return false;  // Not a harmonic (spurious peak = potential problem)
}

float AIEngine::calculateDynamicRange() const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return 0.0f;
    
    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());
    auto& spectrumCopy = scratchTemp;
    
    // Calculate dynamic range as difference between 95th and 5th percentile
    float percentile95 = calculatePercentile(spectrumCopy, 0.95f);
    float percentile5 = calculatePercentile(spectrumCopy, 0.05f);
    
    float dynamicRange = percentile95 - percentile5;
    
    // Also consider RMS for additional context
    float rmsContribution = (snapshot.avgRmsLevel - (-60.0f)) * 0.1f;  // Normalize RMS contribution
    
    return juce::jmax(0.0f, dynamicRange + rmsContribution);
}

float AIEngine::normalizeThresholdByDynamicRange(float baseThreshold, float dynamicRange) const
{
    // Normalize threshold based on dynamic range
    // Higher DR = more variation = need higher threshold
    // Lower DR = less variation = can use lower threshold
    
    // Typical DR values: 20-60 dB
    // Normalize to 0-1 scale (assuming 40dB is average)
    float normalizedDR = juce::jlimit(0.0f, 1.0f, (dynamicRange - 20.0f) / 40.0f);
    
    // Adjust threshold: DR 0.0 (low) = 0.8x threshold, DR 1.0 (high) = 1.3x threshold
    float adjustmentFactor = 0.8f + (normalizedDR * 0.5f);
    
    return baseThreshold * adjustmentFactor;
}

float AIEngine::analyzeSpectralCoherence(ProblemType type, float frequency, float bandwidth) const
{
    // LOCK-FREE: Read spectrum from triple-buffer
    const auto snapshot = readSpectrumSnapshot();
    if (snapshot.version == 0)
        return 0.0f;
    
    scratchTemp.resize(snapshot.bins.size());
    std::copy(snapshot.bins.begin(), snapshot.bins.end(), scratchTemp.begin());
    auto& spectrumCopy = scratchTemp;
    
    // Pattern matching for different problem types
    switch (type)
    {
        case ProblemType::Resonance:
        {
            // Resonance: narrow, sharp peak with steep sides
            int centerBin = frequencyToBin(frequency);
            if (centerBin < 2 || centerBin >= numBins - 2)
                return 0.0f;
            
            float centerMag = spectrumCopy[static_cast<size_t>(centerBin)];
            float leftMag = spectrumCopy[static_cast<size_t>(centerBin - 1)];
            float rightMag = spectrumCopy[static_cast<size_t>(centerBin + 1)];
            
            // Check for sharp peak (steep sides)
            float leftSlope = centerMag - leftMag;
            float rightSlope = centerMag - rightMag;
            
            // Sharp peak should have steep slopes on both sides
            float sharpness = (leftSlope + rightSlope) / 2.0f;
            return juce::jlimit(0.0f, 1.0f, sharpness / 6.0f);  // 6dB slope = 1.0
        }
        
        case ProblemType::Harshness:
        {
            // Harshness: diffuse energy in 2-8kHz range
            float harshnessLow = 2000.0f;
            float harshnessHigh = 8000.0f;
            
            int lowBin = frequencyToBin(harshnessLow);
            int highBin = frequencyToBin(harshnessHigh);
            lowBin = juce::jlimit(0, numBins - 1, lowBin);
            highBin = juce::jlimit(0, numBins - 1, highBin);
            
            if (highBin <= lowBin)
                return 0.0f;
            
            // Calculate average energy in harshness range
            float sum = 0.0f;
            int count = 0;
            for (int i = lowBin; i <= highBin; ++i)
            {
                if (spectrumCopy[static_cast<size_t>(i)] > -100.0f)
                {
                    sum += spectrumCopy[static_cast<size_t>(i)];
                    count++;
                }
            }
            
            if (count == 0)
                return 0.0f;
            
            float avgEnergy = sum / static_cast<float>(count);
            // Normalize: -40dB average = 1.0, -80dB = 0.0
            return juce::jlimit(0.0f, 1.0f, (avgEnergy + 80.0f) / 40.0f);
        }
        
        case ProblemType::Muddiness:
        {
            // Muddiness: accumulation of energy in 150-400Hz
            float mudLow = 150.0f;
            float mudHigh = 400.0f;
            
            int lowBin = frequencyToBin(mudLow);
            int highBin = frequencyToBin(mudHigh);
            lowBin = juce::jlimit(0, numBins - 1, lowBin);
            highBin = juce::jlimit(0, numBins - 1, highBin);
            
            if (highBin <= lowBin)
                return 0.0f;
            
            // Calculate total energy in muddiness range
            float totalEnergy = 0.0f;
            int count = 0;
            for (int i = lowBin; i <= highBin; ++i)
            {
                if (spectrumCopy[static_cast<size_t>(i)] > -100.0f)
                {
                    totalEnergy += spectrumCopy[static_cast<size_t>(i)];
                    count++;
                }
            }
            
            if (count == 0)
                return 0.0f;
            
            float avgEnergy = totalEnergy / static_cast<float>(count);
            // Compare to overall energy (calculate directly to avoid nested lock)
            int overallLowBin = frequencyToBin(100.0f);
            int overallHighBin = frequencyToBin(10000.0f);
            overallLowBin = juce::jlimit(0, numBins - 1, overallLowBin);
            overallHighBin = juce::jlimit(0, numBins - 1, overallHighBin);
            float overallSum = 0.0f;
            int overallCount = 0;
            for (int j = overallLowBin; j <= overallHighBin; ++j)
            {
                if (spectrumCopy[static_cast<size_t>(j)] > -100.0f)
                {
                    overallSum += spectrumCopy[static_cast<size_t>(j)];
                    overallCount++;
                }
            }
            float overallEnergy = (overallCount > 0) ? (overallSum / static_cast<float>(overallCount)) : -100.0f;
            float excess = avgEnergy - overallEnergy;
            
            // Normalize: +5dB excess = 1.0, -5dB = 0.0
            return juce::jlimit(0.0f, 1.0f, (excess + 5.0f) / 10.0f);
        }
        
        default:
            return 0.5f;  // Neutral score for other types
    }
}

float AIEngine::getSpectralPatternScore(ProblemType type, float centerFreq, float bandwidth) const
{
    // Wrapper that combines coherence analysis with frequency/bandwidth matching
    float coherenceScore = analyzeSpectralCoherence(type, centerFreq, bandwidth);
    
    // Additional pattern matching based on frequency range
    float frequencyScore = 0.5f;  // Default neutral
    
    switch (type)
    {
        case ProblemType::Resonance:
            // Resonance can occur anywhere, but narrow bandwidth is key
            if (bandwidth > 0.0f && bandwidth < 200.0f)
                frequencyScore = 1.0f;
            else if (bandwidth < 500.0f)
                frequencyScore = 0.7f;
            break;
            
        case ProblemType::Harshness:
            // Harshness typically in 2-8kHz
            if (centerFreq >= 2000.0f && centerFreq <= 8000.0f)
                frequencyScore = 1.0f;
            else if (centerFreq >= 1500.0f && centerFreq <= 10000.0f)
                frequencyScore = 0.6f;
            break;
            
        case ProblemType::Muddiness:
            // Muddiness in 150-400Hz
            if (centerFreq >= 150.0f && centerFreq <= 400.0f)
                frequencyScore = 1.0f;
            else if (centerFreq >= 100.0f && centerFreq <= 500.0f)
                frequencyScore = 0.6f;
            break;
            
        default:
            frequencyScore = 0.5f;
            break;
    }
    
    // Combine scores (weighted average)
    return (coherenceScore * 0.7f) + (frequencyScore * 0.3f);
}

#if defined(AIEQ_ENABLE_MOTORE_V2) && AIEQ_ENABLE_MOTORE_V2
//==============================================================================
// Motore v2 (EXP hybrid) — off-audio-thread CNN on a rolling 32-frame log-mel
// window. reset() + 32 forwards per window == the A3 parity contract (the last
// forward's [17] output is the window prediction). Fed by PerceptualFrontEnd.
//==============================================================================

#if defined(AIEQ_MOTORE_V2_EXP) && AIEQ_MOTORE_V2_EXP
void AIEngine::applyMotoreV2ExpRouting()
{
    struct Route
    {
        int classIndex = 0;
        ProblemType type = ProblemType::None;
        float threshold = 0.5f;
        float loHz = 20.0f;
        float hiHz = 20000.0f;
        float baseGainDb = 0.0f;
        float baseQ = 1.0f;
    };

    // Round5 candidate_s42 class order:
    // Res, Harsh, Mud, Sib, Boom, Thin, Boxy, Dull.
    static constexpr std::array<Route, 4> kRoutes {{
        { 2, ProblemType::Muddiness, 0.3881837725639343f, 100.0f, 400.0f,   -2.5f, 1.0f  },
        { 5, ProblemType::ThinSound, 0.3000000000000000f,  80.0f, 300.0f,   +2.0f, 0.71f },
        { 6, ProblemType::Boxyness,  0.4754117429256439f, 300.0f, 800.0f,   -2.0f, 1.2f  },
        { 7, ProblemType::DullSound, 0.3233138024806976f, 6000.0f, 16000.0f,+3.0f, 0.71f },
    }};

    std::array<float, 17> outputs {};
    bool hasOutput = false;
    {
        std::lock_guard<std::mutex> lk(motoreV2OutMutex);
        hasOutput = motoreV2Ready && motoreV2HasOutput;
        if (hasOutput)
            outputs = motoreV2Outputs;
    }

    auto shouldSuppress = [](ProblemType type)
    {
        return type != ProblemType::Harshness
            && type != ProblemType::Sibilance;
    };

    std::lock_guard<std::mutex> lock(correctionsWriteMutex);
    pendingCorrections.erase(
        std::remove_if(pendingCorrections.begin(), pendingCorrections.end(),
                       [&](const Correction& c) { return shouldSuppress(c.type); }),
        pendingCorrections.end());

    if (! hasOutput)
        return;

    auto sigmoid = [](float x)
    {
        if (x >= 0.0f)
            return 1.0f / (1.0f + std::exp(-x));

        const float ex = std::exp(x);
        return ex / (1.0f + ex);
    };

    auto targetToFrequency = [](float target, float loHz, float hiHz)
    {
        const float t = std::isfinite(target) ? juce::jlimit(0.0f, 1.0f, target) : 0.5f;
        return loHz * std::pow(hiHz / loHz, t);
    };

    for (const auto& route : kRoutes)
    {
        const float probability = sigmoid(outputs[static_cast<size_t>(route.classIndex)]);
        if (probability <= route.threshold)
            continue;

        const float over = (probability - route.threshold) / (1.0f - route.threshold);
        const float severity = juce::jlimit(0.15f, 1.0f, over);
        const float gainScale = 0.45f + 0.55f * severity;

        Correction c;
        c.type = route.type;
        c.frequency = targetToFrequency(outputs[static_cast<size_t>(9 + route.classIndex)],
                                        route.loHz,
                                        route.hiHz);
        c.suggestedGain = route.baseGainDb * gainScale;
        c.suggestedQ = route.baseQ;
        c.severity = severity;
        c.confidence = probability;
        c.approved = false;
        c.suggestedFilter = selectOptimalFilterType(c.type,
                                                    c.frequency,
                                                    c.frequency / juce::jmax(0.1f, c.suggestedQ),
                                                    std::abs(c.suggestedGain));
        if (c.suggestedFilter == Correction::FilterType::LowShelf
            || c.suggestedFilter == Correction::FilterType::HighShelf)
            c.suggestedQ = std::min(c.suggestedQ, 0.71f);

        c.description = juce::String::formatted(
            "%s at %.0f Hz (%s) - MotoreV2 EXP Round5: %s %.1f dB, Q: %.1f (p=%.0f%%)",
            getProblemTypeName(c.type).toRawUTF8(),
            c.frequency,
            getBandName(c.frequency).toRawUTF8(),
            getFilterTypeName(c.suggestedFilter).toRawUTF8(),
            c.suggestedGain,
            c.suggestedQ,
            c.confidence * 100.0f);

        pendingCorrections.push_back(c);
    }

    std::sort(pendingCorrections.begin(), pendingCorrections.end(),
              [](const Correction& a, const Correction& b) {
                  float priorityA = a.severity * a.confidence;
                  float priorityB = b.severity * b.confidence;
                  if (std::abs(priorityA - priorityB) < 0.01f)
                      return a.severity > b.severity;
                  return priorityA > priorityB;
              });
}
#endif

bool AIEngine::loadMotoreV2Model(const juce::File& jsonFile)
{
    motoreV2Ready = jsonFile.existsAsFile()
        && motoreV2Model.loadFromJsonFile(jsonFile.getFullPathName().toStdString());
    motoreV2RingCount = 0;
    motoreV2RingHead  = 0;
    {
        std::lock_guard<std::mutex> lk(motoreV2OutMutex);
        motoreV2Outputs.fill(0.0f);
        motoreV2OutFresh = false;
        motoreV2HasOutput = false;
    }
    return motoreV2Ready;
}

void AIEngine::pushMotoreV2Frame(const float* mel64)
{
    if (! motoreV2Ready || mel64 == nullptr)
        return;

    std::copy(mel64, mel64 + 64,
              motoreV2Ring[static_cast<size_t>(motoreV2RingHead)].begin());
    motoreV2RingHead = (motoreV2RingHead + 1) % 32;
    if (motoreV2RingCount < 32)
    {
        ++motoreV2RingCount;
        return;                                   // window not full yet
    }

    // Window full: stream oldest-first through the stateful CNN.
    motoreV2Model.reset();
    const float* out = nullptr;
    for (int t = 0; t < 32; ++t)
    {
        const int idx = (motoreV2RingHead + t) % 32;  // head == oldest slot
        out = motoreV2Model.forward(
            motoreV2Ring[static_cast<size_t>(idx)].data());
    }
    if (out == nullptr)
        return;

    std::lock_guard<std::mutex> lk(motoreV2OutMutex);
    std::copy(out, out + 17, motoreV2Outputs.begin());
    motoreV2OutFresh = true;
    motoreV2HasOutput = true;
}

bool AIEngine::readMotoreV2Outputs(std::array<float, 17>& out)
{
    std::lock_guard<std::mutex> lk(motoreV2OutMutex);
    if (! motoreV2OutFresh)
        return false;
    out = motoreV2Outputs;
    motoreV2OutFresh = false;                      // consumed
    return true;
}
#endif // AIEQ_ENABLE_MOTORE_V2
