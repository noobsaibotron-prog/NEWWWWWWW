#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utils/Logger.h"
#include <limits>
#include <algorithm>
#include <complex>
#include <cmath>

namespace
{
    // Default band centers for up to 24 bands (used when expanding band count)
    constexpr float defaultBandFrequencies[AIEqualizerAudioProcessor::maxBands] = {
        31.0f,   50.0f,   80.0f,   120.0f,  170.0f,  250.0f,
        350.0f,  500.0f,  700.0f,  1000.0f, 1400.0f, 2000.0f,
        2800.0f, 4000.0f, 5600.0f, 8000.0f, 11000.0f,15000.0f,
        18000.0f,22000.0f,26000.0f,30000.0f,34000.0f,38000.0f
    };

    // M/S encoding/decoding scale factor (1 / sqrt(2))
    // Used in both encodeStereoToMS() and decodeMSToStereo() to normalise
    // mid and side channels to the same RMS level as the input L/R pair.
    constexpr float kInvSqrt2 = 0.7071067811865476f;

    // Band-assignment scoring constants (used in applyAICorrections and
    // applySemanticCorrections to pick the best existing EQ band to reuse).
    // ~1/5 octave: if an existing band is within this ratio of the target
    // frequency it is a candidate for reuse instead of allocating a new band.
    constexpr float kBandReuseThresholdOctave = 0.148f;
    // Score multiplier applied to unused bands — strongly prefers free bands.
    constexpr float kUnusedBandScoreMult = 0.3f;
    // Score multiplier applied when a band is within reuse threshold — even
    // stronger preference for merging near-frequency corrections.
    constexpr float kReuseThresholdScoreMult = 0.2f;
}

//==============================================================================
AIEqualizerAudioProcessor::AIEqualizerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::mono(), false)),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    apvts.addParameterListener("phaseMode", this);
    apvts.addParameterListener("msMode", this);
    apvts.addParameterListener("oversamplingFactor", this);
    // Listen to band parameters to trigger IR regeneration for linear-phase mode
    for (int i = 0; i < maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        eqParameterIDs.push_back(prefix + "Freq");
        eqParameterIDs.push_back(prefix + "Gain");
        eqParameterIDs.push_back(prefix + "Q");
        eqParameterIDs.push_back(prefix + "Type");
        eqParameterIDs.push_back(prefix + "Enabled");
        eqParameterIDs.push_back(prefix + "Slope");

        // FIX: Also listen to Dynamic EQ parameters so that tweaking
        // threshold/ratio/attack/release from the GUI triggers parameterChanged()
        // and marks parametersNeedUpdate = true. Without this, updateEQFromParameters()
        // was never called when the user moved a dynamic EQ knob, so setBandParams()
        // never updated the SmoothedValues - causing the GR meter to freeze.
        eqParameterIDs.push_back(prefix + "DynMode");
        eqParameterIDs.push_back(prefix + "Threshold");
        eqParameterIDs.push_back(prefix + "Ratio");
        eqParameterIDs.push_back(prefix + "Attack");
        eqParameterIDs.push_back(prefix + "Release");
        eqParameterIDs.push_back(prefix + "Range");
        eqParameterIDs.push_back(prefix + "Knee");
    }
    eqParameterIDs.push_back("numActiveBands");
    eqParameterIDs.push_back("dynEqEnabled");
    eqParameterIDs.push_back("dynEqMix");
    for (const auto& id : eqParameterIDs)
        apvts.addParameterListener(id, this);

    // Initial curve refresh + LP IR build if needed later
    triggerLinearPhaseIRUpdate();

    if (auto* phaseValue = apvts.getRawParameterValue("phaseMode"))
        parameterChanged("phaseMode", phaseValue->load());
    if (auto* msValue = apvts.getRawParameterValue("msMode"))
        parameterChanged("msMode", msValue->load());
    if (auto* osValue = apvts.getRawParameterValue("oversamplingFactor"))
        parameterChanged("oversamplingFactor", osValue->load());

    // Initialize A/B/C/D slots with defaults
    slotA.bands.fill(BandState());
    slotB.bands.fill(BandState());
    slotC.bands.fill(BandState());
    slotD.bands.fill(BandState());
    slotA.name = "A";
    slotB.name = "B";
    slotC.name = "C";
    slotD.name = "D";

    semanticBandAssignments.fill(-1);

    // Initialize preset manager
    presetManager = std::make_unique<PresetManager>(apvts);

    // Initialize history manager with APVTS reference
    historyManager.initialize(apvts);

    // OSC parameter server created here but started in prepareToPlay
    // (starting in constructor crashes during VST3 plugin scan)
    oscParamServer = std::make_unique<OSCParameterServer>(apvts, 11100);

    // Start IR builder thread (RAII with std::thread)
    stopIRBuilder.store(false);
    irBuilderThread = std::thread([this]() {
        irBuilderThreadFunc();
    });

    // Start AI analysis thread (off-audio-thread)
    stopAIAnalysis.store(false);
    aiAnalysisThread = std::thread([this]() {
        aiAnalysisThreadFunc();
    });

    // Initialize band targets with defaults (will be updated on first parameter sync)
    for (int i = 0; i < maxBands; ++i)
    {
        targetBandFreq[static_cast<size_t>(i)] = 1000.0f;
        targetBandGain[static_cast<size_t>(i)] = 0.0f;
        targetBandQ[static_cast<size_t>(i)] = 1.0f;
        targetBandType[static_cast<size_t>(i)] = ParametricEQProcessor::Peak;
        targetBandEnabled[static_cast<size_t>(i)] = (i < 8);
        targetBandSolo[static_cast<size_t>(i)] = false;
    }
}

//==============================================================================
// IR Builder Thread Function (runs in background)
//==============================================================================
void AIEqualizerAudioProcessor::irBuilderThreadFunc()
{
    try
    {
    juce::dsp::FFT fft(LinearPhaseProcessor::fftOrder);
    const size_t fftSize = LinearPhaseProcessor::fftSize;
    const size_t halfSize = fftSize / 2;
    std::vector<std::complex<float>> freqDomain(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
    std::vector<float> irBuf(fftSize, 0.0f);
    std::vector<float> magDB(halfSize, -120.0f);

    while (!stopIRBuilder.load())
    {
        irBuildEvent.wait(-1);

        // Check exit flag after wake
        if (stopIRBuilder.load())
            break;

        if (!eqCurveNeedsUpdate.load(std::memory_order_acquire))
            continue;

        // Debounce: wait until no new request has arrived for irBuildDebounceMs.
        // This prevents rebuilding the IR on every mouse-drag event (which would
        // produce rapid IR changes → audible glitches in Linear Phase mode).
        {
            int64_t lastRequest = irBuildRequestedAt.load(std::memory_order_acquire);
            while (!stopIRBuilder.load())
            {
                juce::Thread::sleep(irBuildDebounceMs);
                int64_t nowRequest = irBuildRequestedAt.load(std::memory_order_acquire);
                if (nowRequest == lastRequest)
                    break;  // stable: no new request during the sleep window
                lastRequest = nowRequest;
            }
            if (stopIRBuilder.load()) break;
        }

        if (!eqCurveNeedsUpdate.exchange(false))
            continue;

        const double sr = currentSampleRate.load(std::memory_order_relaxed);

        // SAFETY: Skip IR building if sample rate not yet initialized
        if (sr <= 0.0)
            continue;

        // SAFETY: Skip if prepareToPlay() hasn't allocated the double-buffer yet
        if (pendingFreqIR.buffers[0].empty() || pendingFreqIR.buffers[1].empty())
            continue;

        // SAFETY: Check for null pointer before dereferencing
        bool dynEnabled = false;
        if (auto* dynParam = apvts.getRawParameterValue("dynEqEnabled"))
            dynEnabled = dynParam->load() > 0.5f;

        uint64_t versionStart = 0;
        uint64_t versionEnd = 0;

        do
        {
            versionStart = irCoeffVersion.load(std::memory_order_acquire);
            if (versionStart & 1u)
            {
                std::this_thread::yield(); // FIX BUG #3: Release CPU to writer thread
                continue; // writer in progress, retry
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            std::fill(magDB.begin(), magDB.end(), -120.0f);

            // Use shadow processors (eqProcessorForIR/dynamicEQProcessorForIR) to avoid
            // data race with audio thread. These are updated atomically when coefficients change.

            for (size_t bin = 0; bin < halfSize; ++bin)
            {
                const float freq = static_cast<float>(bin) * static_cast<float>(sr)
                                   / static_cast<float>(LinearPhaseProcessor::fftSize);

                float mag = eqProcessorForIR.getMagnitudeForFrequency(freq, sr);
                if (dynEnabled)
                    mag *= dynamicEQProcessorForIR.getMagnitudeForFrequency(freq, sr);

                magDB[bin] = juce::Decibels::gainToDecibels(mag, -120.0f);

            }

            versionEnd = irCoeffVersion.load(std::memory_order_acquire);
        } while (versionStart != versionEnd || (versionEnd & 1u));

        // Build IR in frequency domain
        std::fill(freqDomain.begin(), freqDomain.end(), std::complex<float>(0.0f, 0.0f));
        std::fill(timeDomain.begin(), timeDomain.end(), std::complex<float>(0.0f, 0.0f));
        std::fill(irBuf.begin(), irBuf.end(), 0.0f);

        for (size_t k = 0; k < magDB.size(); ++k)
        {
            float magLin = juce::Decibels::decibelsToGain(magDB[k]);

            // SAFETY: avoid vanishing bins that generate near-zero IR
            const float magFloor = (k == 0) ? 1.0f : 1.0e-4f; // keep DC at unity, others at -80 dB floor
            if (magLin < magFloor)
                magLin = magFloor;

            freqDomain[k] = { magLin, 0.0f };

            if (k > 0 && k < halfSize)
            {
                const size_t mirror = LinearPhaseProcessor::fftSize - k;
                freqDomain[mirror] = { magLin, 0.0f };
            }
        }

        fft.perform(freqDomain.data(), timeDomain.data(), true);

        //======================================================================
        // FIX #4: IR SCALING DOCUMENTATION AND STABILIZATION
        //======================================================================
        // The IR scaling pipeline has multiple stages that must work together:
        //
        // STAGE 1: IFFT Normalization (ifftScale)
        //   - JUCE FFT doesn't auto-normalize inverse FFT
        //   - Standard normalization: 1/N where N = FFT size
        //   - Reference: Smith, "Mathematics of the DFT", Chapter 7
        //
        // STAGE 2: Global Attenuation Compensation (globalCompensation)
        //   - Problem: EQ curves with all-negative gains produce very small IR
        //   - Solution: Scale up by 1/avgMag to preserve relative shape
        //   - Example: -6dB across all bands → avgMag ≈ 0.5 → compensation = 2.0
        //   - Capped at 1.0 for boost curves (no attenuation of boosted EQs)
        //
        // STAGE 3: Final Safety Scaling (irScale, applied later)
        //   - Rescues IRs that are still too small after stages 1-2
        //   - Threshold: 1e-04 peak = -80dBFS (below noise floor)
        //   - Target: 0.1 peak = -20dBFS (reasonable working level)
        //   - Max boost: 1e6 (60dB) to prevent runaway scaling
        //
        // TOTAL GAIN BOUNDS:
        //   - Minimum IR peak after all stages: 0.1 (-20dBFS)
        //   - Maximum IR sample value: ±10.0 (clamped for safety)
        //======================================================================

        // STAGE 1: IFFT normalization
        // NOTE: JUCE's fft.perform(inverse=true) already applies 1/N scaling internally.
        // Do NOT apply an additional 1/N here - that was causing the IR to be ~N times too quiet.

        // STAGE 2: Global attenuation compensation
        // Calculate average linear magnitude across all bins
        float avgMag = 0.0f;
        for (const auto& mag : magDB)
            avgMag += juce::Decibels::decibelsToGain(mag);
        avgMag /= static_cast<float>(magDB.size());

        // Compensate for globally attenuated EQ curves (all cuts)
        // Only apply when average is below unity (0.99 threshold avoids floating-point noise)
        // Cap compensation at 100x (+40dB) to prevent extreme scaling from very quiet curves
        const float globalCompensation = (avgMag < 0.99f && avgMag > 1e-6f)
                                         ? std::min(1.0f / avgMag, 100.0f)
                                         : 1.0f;

        // Apply STAGE 2 scaling only (STAGE 1 already handled by JUCE IFFT)
        for (size_t n = 0; n < LinearPhaseProcessor::fftSize; ++n)
            irBuf[n] = timeDomain[n].real() * globalCompensation;

        // Center (circular shift to create zero-phase / linear-phase IR)
        //
        // BUG FIX: The previous rotation by halfSize (4096) placed the IR peak at index 4096,
        // which is then EXCLUDED when we load only the first irSize=4096 samples (indices 0..4095).
        // The result: only the tail of the IR was loaded, causing near-zero volume in linear phase mode.
        //
        // CORRECT rotation: shift so the peak lands at irSize/2 = 2048 (center of the loaded window).
        // The IFFT of a real-symmetric spectrum has its peak at index 0.
        // To move it to position irSize/2 within the first irSize samples, we rotate by:
        //   rotateOffset = fftSize - irSize/2 = 8192 - 2048 = 6144
        // After rotate, element that was at index 6144 becomes index 0,
        // so index 0 (the peak) ends up at position fftSize-6144 = 2048. ✓
        //
        // Reference: Oppenheim & Schafer, "Discrete-Time Signal Processing", Chapter 5
        {
            const size_t peakTargetIndex = LinearPhaseProcessor::irSize / 2;              // 2048
            const size_t rotateOffset    = LinearPhaseProcessor::fftSize - peakTargetIndex; // 6144
            std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(rotateOffset), irBuf.end());
        }

        //======================================================================
        // HANN WINDOW APPLICATION
        //======================================================================
        // Purpose: Smooth time-domain truncation to reduce frequency ripple
        // The Hann (raised cosine) window provides -31dB sidelobe suppression
        //
        // Gain compensation: Hann window has coherent gain of 0.5.
        // We compensate with hannGainComp = 2.0f to preserve correct output level,
        // consistent with LinearPhaseProcessor::updateImpulseResponse().
        // Do NOT normalize to maxAbs=1.0 - this would destroy EQ gain information!
        //======================================================================
        // hannGainComp = 1.0: Hann window applied to IR for anti-ringing only, no gain compensation needed.
        // See LinearPhaseProcessor.cpp for full explanation.
        constexpr float hannGainComp = 1.0f;
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n)
                                                    / static_cast<float>(LinearPhaseProcessor::irSize - 1)));
            irBuf[n] *= (w * hannGainComp);
        }

        //======================================================================
        // SAFETY CLAMPING
        //======================================================================
        // Final sanity check to prevent audio blowup
        // Limits: ±10.0 linear (~+20dBFS) is extremely loud but prevents DAW crash
        // NaN/Inf samples are zeroed (indicates numerical instability upstream)
        //======================================================================
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            if (std::isnan(irBuf[n]) || std::isinf(irBuf[n]))
                irBuf[n] = 0.0f;
            else
                irBuf[n] = juce::jlimit(-10.0f, 10.0f, irBuf[n]);
        }

        // Final safety: abort loading if any sample is not finite
        bool finiteIR = true;
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            if (!std::isfinite(irBuf[n]))
            {
                finiteIR = false;
                break;
            }
        }
        if (!finiteIR)
            continue;

        // Measure IR magnitude for STAGE 3 decision
        {
            float maxAbs = 0.0f;
            for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
                maxAbs = std::max(maxAbs, std::abs(irBuf[n]));

            constexpr float minReasonableIR = 1e-02f;   // -40 dBFS threshold
            constexpr float targetIR = 0.1f;            // -20 dBFS target
            constexpr float maxScaleBoost = 1.0e4f;     // +80 dB max

            float irScale = 1.0f;
            if (maxAbs > 0.0f && maxAbs < minReasonableIR)
            {
                irScale = targetIR / maxAbs;
                irScale = std::min(irScale, maxScaleBoost);
            }

            // Apply STAGE 3 scaling
            std::vector<float> scaledIR(LinearPhaseProcessor::irSize);
            for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
                scaledIR[n] = irBuf[n] * irScale;

            // Pre-compute FFT of the IR here (builder thread, NOT audio thread)
            // Then hand off the freq-domain data for the audio thread to pick up.
            std::vector<float> freqBuf(LinearPhaseProcessor::fftSize * 2, 0.0f);
            std::copy(scaledIR.begin(), scaledIR.end(), freqBuf.begin());
            fft.performRealOnlyForwardTransform(freqBuf.data());

            // Lock-free double-buffer write.
            // ABA guard: spin until the audio thread is not reading our target buffer.
            // storeFreqIRDirect is a fast memcpy, so this wait is bounded and negligible
            // compared to the IR build time (tens of ms). Not in the audio thread — safe to spin.
            int wi = pendingFreqIR.writeIndex.load(std::memory_order_relaxed);
            while (wi == pendingFreqIR.readingIndex.load(std::memory_order_acquire))
                wi = 1 - wi;

            // std::copy preserves pre-allocated capacity of buffers[wi] (resize in prepareToPlay).
            // Do NOT use std::move here: it would transfer ownership and lose the pre-allocation.
            jassert(freqBuf.size() == pendingFreqIR.buffers[wi].size());
            std::copy(freqBuf.begin(), freqBuf.end(), pendingFreqIR.buffers[wi].begin());

            pendingFreqIR.readyIndex.store(wi, std::memory_order_release);
            // Release ordering on writeIndex ensures the flip is visible after the data write.
            pendingFreqIR.writeIndex.store(1 - wi, std::memory_order_release);
        }
    }
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("IR builder thread exception: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("IR builder thread unknown exception");
    }
}

//==============================================================================
// AI Analysis Thread Function (runs off the audio thread)
//==============================================================================
void AIEqualizerAudioProcessor::aiAnalysisThreadFunc()
{
    try
    {
    AISpectrumFrame frame;
    std::vector<float> spectrum;
    spectrum.reserve(aiSpectrumBins);

    while (!stopAIAnalysis.load())
    {
        // Try to pop without blocking
        if (!aiSpectrumQueue.tryPop(frame))
        {
            // Wait briefly to avoid busy-wait; wake on signal
            aiSpectrumEvent.wait(5);
            continue;
        }

        // Convert fixed-size frame to vector for AIEngine API
        spectrum.assign(frame.begin(), frame.end());

        aiEngine.analyzeSpectrum(spectrum);
        aiProblemsChanged.store(true, std::memory_order_release);
    }
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("AI analysis thread exception: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("AI analysis thread unknown exception");
    }
}

AIEqualizerAudioProcessor::~AIEqualizerAudioProcessor()
{
    // Request threads to stop before member teardown
    if (irBuilderThread.joinable())
    {
        stopIRBuilder.store(true);
        irBuildEvent.signal();
        irBuilderThread.join();
    }
    if (aiAnalysisThread.joinable())
    {
        stopAIAnalysis.store(true);
        aiSpectrumEvent.signal();
        aiAnalysisThread.join();
    }

    // Remove parameter listeners
    apvts.removeParameterListener("phaseMode", this);
    apvts.removeParameterListener("msMode", this);
    apvts.removeParameterListener("oversamplingFactor", this);
    for (const auto& id : eqParameterIDs)
        apvts.removeParameterListener(id, this);

    // std::thread automatically requests stop and joins on destruction (RAII)
    // Signal the event to wake the thread so they can check stop flags
    irBuildEvent.signal();
    aiSpectrumEvent.signal();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AIEqualizerAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    // Global dry/wet mix (0 = dry, 100 = fully processed)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"autoGain", 1}, "Auto Gain", false));

    // Quality / latency mode: 0 = Zero Latency, 1 = High Quality (lookahead on)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"qualityMode", 1}, "Quality Mode",
        juce::StringArray{"Zero Latency", "High Quality"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"phaseMode", 1}, "Processing Mode",
        juce::StringArray{"Zero Latency", "Natural Phase", "Linear Phase"}, 0));

    // Mid/Side processing mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"msMode", 1}, "M/S Mode",
        juce::StringArray{"Stereo", "Mid Only", "Side Only", "M/S Linked"}, 0));

    // Oversampling factor
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"oversamplingFactor", 1}, "Oversampling",
        juce::StringArray{"Off", "2x", "4x", "Auto"}, 0));

    // AI controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"aiSensitivity", 1}, "AI Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"aiStrength", 1}, "AI Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"aiEnabled", 1}, "AI Enabled", true));

    // Source Profile (as choice parameter)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"sourceProfile", 1}, "Source Profile",
        juce::StringArray{"Generic", "Vocals", "Drums", "Bass", "Synth", "Master", "EDM"}, 0));

    // Analyzer display options
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showPreSpectrum", 1}, "Show Pre-EQ Spectrum", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showPostSpectrum", 1}, "Show Post-EQ Spectrum", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showDeltaSpectrum", 1}, "Show Delta Spectrum", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerResolution", 1}, "Analyzer FFT",
        juce::StringArray{"Low (1024)", "Medium (2048)", "High (4096)", "Max (8192)"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerSpeed", 1}, "Analyzer Speed",
        juce::StringArray{"Fast", "Medium", "Slow"}, 1));

    // Analyzer slope (visual only): 0 = Flat, 1 = 3 dB/oct, 2 = 4.5 dB/oct
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerSlope", 1}, "Analyzer Slope",
        juce::StringArray{"Flat", "3 dB/oct", "4.5 dB/oct"}, 2)); // default 4.5 dB/oct

    // Peak-hold visualization controls (visual only)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"analyzerPeakHold", 1}, "Analyzer Peak Hold",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.1f), 2.0f)); // seconds
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"analyzerPeakDecay", 1}, "Analyzer Peak Decay",
        juce::NormalisableRange<float>(1.0f, 60.0f, 0.5f), 20.0f)); // dB/sec

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"pianoRollOverlay", 1}, "Piano Roll Overlay", false));

    // Accessibility: High-contrast mode
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"highContrastMode", 1}, "High Contrast Mode", false));

    // User Learning privacy
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"learningEnabled", 1}, "User Learning Enabled", true));

    // Number of active bands (1-24)
    juce::StringArray bandChoices;
    for (int i = 1; i <= AIEqualizerAudioProcessor::maxBands; ++i)
        bandChoices.add(juce::String(i));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"numActiveBands", 1}, "Number of Bands",
        bandChoices, 7)); // default 8 (index 7)

    // EQ Bands (24 bands)
    float defaultFreqs[AIEqualizerAudioProcessor::maxBands] = {
        31.0f, 50.0f, 80.0f, 120.0f, 170.0f, 250.0f, 350.0f, 500.0f,
        700.0f, 1000.0f, 1400.0f, 2000.0f, 2800.0f, 4000.0f, 5600.0f, 8000.0f,
        11000.0f, 15000.0f, 18000.0f, 22000.0f, 26000.0f, 30000.0f, 34000.0f, 38000.0f
    };

    for (int i = 0; i < AIEqualizerAudioProcessor::maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Freq", 1}, "Band " + juce::String(i + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), defaultFreqs[i]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Gain", 1}, "Band " + juce::String(i + 1) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Q", 1}, "Band " + juce::String(i + 1) + " Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));

        // Filter type (per-band choice)
        int defaultType = 2; // Peak
        if (i == 0)
            defaultType = 1; // Low Shelf
        else if (i == maxBands - 1)
            defaultType = 3; // High Shelf

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "Type", 1}, "Band " + juce::String(i + 1) + " Type",
            juce::StringArray{"Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass", "Vintage Low Shelf", "Vintage High Shelf"},
            defaultType));

        const bool enabledDefault = (i < 8);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "Enabled", 1}, "Band " + juce::String(i + 1) + " Enabled", enabledDefault));

        // Solo (per-band)
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{prefix + "Solo", 1}, "Band " + juce::String(i + 1) + " Solo", false));

        // Slope (per-band, LowCut/HighCut only)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "Slope", 1}, "Band " + juce::String(i + 1) + " Slope",
            juce::StringArray{"12 dB/oct", "24 dB/oct", "48 dB/oct"}, 0));

        //----------------------------------------------------------------------
        // DYNAMIC EQ Parameters (FabFilter Pro-Q / TDR Nova style)
        //----------------------------------------------------------------------
        // Dynamic mode: 0=Off, 1=Compress, 2=Expand, 3=Gate
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "DynMode", 1}, "Band " + juce::String(i + 1) + " Dynamic",
            juce::StringArray{"Off", "Compress", "Expand", "Gate"}, 0));

        // Threshold (-60 to 0 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Threshold", 1}, "Band " + juce::String(i + 1) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -20.0f));

        // Ratio (1:1 to 20:1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Ratio", 1}, "Band " + juce::String(i + 1) + " Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 2.0f));

        // Attack (0.1 to 500 ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Attack", 1}, "Band " + juce::String(i + 1) + " Attack",
            juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f, 0.3f), 10.0f));

        // Release (1 to 2000 ms)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Release", 1}, "Band " + juce::String(i + 1) + " Release",
            juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 100.0f));

        // Range (max gain change, 0 to 48 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Range", 1}, "Band " + juce::String(i + 1) + " Range",
            juce::NormalisableRange<float>(0.0f, 48.0f, 0.1f), 24.0f));

        // Knee (0 to 24 dB, 0 = hard knee)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "Knee", 1}, "Band " + juce::String(i + 1) + " Knee",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f));
    }

    //--------------------------------------------------------------------------
    // Global Dynamic EQ controls
    //--------------------------------------------------------------------------
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynEqEnabled", 1}, "Dynamic EQ Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dynEqMix", 1}, "Dynamic EQ Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dynAutoMakeup", 1}, "Dynamic Auto Makeup", false));

    return {params.begin(), params.end()};
}

//==============================================================================
void AIEqualizerAudioProcessor::cacheParameterPointers()
{
    if (parametersCached.load())
        return;

    for (int i = 0; i < maxBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        cachedParams[i].freq = apvts.getRawParameterValue(prefix + "Freq");
        cachedParams[i].gain = apvts.getRawParameterValue(prefix + "Gain");
        cachedParams[i].q = apvts.getRawParameterValue(prefix + "Q");
        cachedParams[i].type = apvts.getRawParameterValue(prefix + "Type");
        cachedParams[i].enabled = apvts.getRawParameterValue(prefix + "Enabled");
        cachedParams[i].solo = apvts.getRawParameterValue(prefix + "Solo");
        cachedParams[i].dynMode = apvts.getRawParameterValue(prefix + "DynMode");
        cachedParams[i].dynThreshold = apvts.getRawParameterValue(prefix + "Threshold");
        cachedParams[i].dynRatio = apvts.getRawParameterValue(prefix + "Ratio");
        cachedParams[i].dynAttack = apvts.getRawParameterValue(prefix + "Attack");
        cachedParams[i].dynRelease = apvts.getRawParameterValue(prefix + "Release");
        cachedParams[i].dynKnee = apvts.getRawParameterValue(prefix + "Knee");
        cachedParams[i].dynRange = apvts.getRawParameterValue(prefix + "Range");
        cachedParams[i].slope = apvts.getRawParameterValue(prefix + "Slope");
    }

    cachedOutputGain = apvts.getRawParameterValue("outputGain");
    cachedDryWet = apvts.getRawParameterValue("dryWet");
    cachedAutoGain = apvts.getRawParameterValue("autoGain");
    cachedDynEqEnabled = apvts.getRawParameterValue("dynEqEnabled");
    cachedNumActiveBands = apvts.getRawParameterValue("numActiveBands");
    cachedBypass = apvts.getRawParameterValue("bypass");
    cachedQualityMode = apvts.getRawParameterValue("qualityMode");
    cachedPhaseModeParam = apvts.getRawParameterValue("phaseMode");
    cachedMSModeParam = apvts.getRawParameterValue("msMode");
    cachedOversamplingParam = apvts.getRawParameterValue("oversamplingFactor");
    cachedAnalyzerResolution = apvts.getRawParameterValue("analyzerResolution");
    cachedAnalyzerSpeed = apvts.getRawParameterValue("analyzerSpeed");
    cachedAIEnabled = apvts.getRawParameterValue("aiEnabled");
    cachedSourceProfile = apvts.getRawParameterValue("sourceProfile");
    cachedShowPostSpectrum = apvts.getRawParameterValue("showPostSpectrum");
    cachedAISensitivity = apvts.getRawParameterValue("aiSensitivity");
    cachedAIStrength = apvts.getRawParameterValue("aiStrength");
    cachedDynEqMix = apvts.getRawParameterValue("dynEqMix");
    cachedDynAutoMakeup = apvts.getRawParameterValue("dynAutoMakeup");

    parametersCached.store(true);
}

//==============================================================================
void AIEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    cacheParameterPointers();

    // FIX 3: Use atomic store
    currentSampleRate.store(sampleRate);

    // Known limitation: LinearPhaseProcessor uses a fixed irSize=4096 / fftSize=8192.
    // At 96 kHz the latency is ~42 ms and frequency resolution halves (2.34 Hz/bin vs 1.17 Hz/bin at 44.1 kHz).
    // At 192 kHz latency is ~21 ms but resolution degrades further.
    // This is acceptable for current release; adaptive irSize is planned for a future version.
    if (sampleRate > 48001.0)
        AIEQ_LOG_WARNING("Sample rate " + juce::String(sampleRate) + " Hz: linear phase IR resolution "
                         "is reduced (fixed irSize=4096). Latency = "
                         + juce::String(static_cast<int>((LinearPhaseProcessor::usePartitioned
                             ? LinearPhaseProcessor::partSize : LinearPhaseProcessor::hopSize) * 1000.0 / sampleRate)) + " ms.");
    currentBlockSize = samplesPerBlock;
    preparedNumInputChannels = getTotalNumInputChannels();
    preallocatedMaxSamples = juce::jmax(samplesPerBlock * 4, 32768);
    blockClampEvents.store(0, std::memory_order_relaxed);
    dryBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    dryBuffer.clear();
    phaseTransitionBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    phaseTransitionBuffer.clear();
    oversamplingTransitionBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    oversamplingTransitionBuffer.clear();
    phaseTransitionFromMode.store(-1, std::memory_order_relaxed);
    phaseTransitionSamplesRemaining.store(0, std::memory_order_relaxed);
    oversamplingTransitionFromEffective.store(-1, std::memory_order_relaxed);
    oversamplingTransitionSamplesRemaining.store(0, std::memory_order_relaxed);
    bypassStateInitialized = false;
    wasBypassed = false;
    bypassCrossfadeRemaining = 0;
    abCrossfadePending.store(false, std::memory_order_relaxed);
    for (int i = 0; i < maxBands; ++i)
        abCrossfadePendingBands[i].store(false, std::memory_order_relaxed);

    // === SOLO ACOUSTIC MONITOR SETUP ===
    soloMonitorFilterL.reset();
    soloMonitorFilterR.reset();
    preProcessingInputCopy.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    preProcessingInputCopy.clear();
    soloOutputBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    soloOutputBuffer.clear();
    soloWarmupBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples, false, true, false);
    soloWarmupBuffer.clear();

    auto toResolution = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Resolution::Low;
            case 1: return SpectrumAnalyzer::Resolution::Medium;
            case 3: return SpectrumAnalyzer::Resolution::Max;
            case 2:
            default: return SpectrumAnalyzer::Resolution::High;
        }
    };

    auto toSpeed = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Speed::Fast;
            case 2: return SpectrumAnalyzer::Speed::Slow;
            case 1:
            default: return SpectrumAnalyzer::Speed::Medium;
        }
    };

    // FIX BUG #2: CORRECT ORDER - Prepare shadow processor BEFORE signaling IR builder
    // This ensures eqProcessorForIR is ready when IR builder thread reads it

    // 1. Allocate bands for all processors (including shadow)
    ensureBandCount(maxBands);

    // 2. CRITICAL: Prepare shadow processor BEFORE updateEQFromParameters()
    //    This sets currentSampleRate and isPrepared flag needed by getMagnitudeForFrequency()
    eqProcessorForIR.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dynamicEQProcessorForIR.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());

    // 3. Synchronize coefficients from APVTS to shadow processor
    updateEQFromParameters();

    // 4. NOW signal IR builder (shadow processor is fully ready!)
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
    irBuildEvent.signal();

    if (auto* res = apvts.getRawParameterValue("analyzerResolution"))
    {
        int resIdx = juce::jlimit(0, 3, static_cast<int>(std::round(res->load())));

        analyzerResolutionCached = resIdx;
        auto resolution = toResolution(analyzerResolutionCached);
        spectrumAnalyzer.setFFTResolution(resolution);
        postEQAnalyzer.setFFTResolution(resolution);
    }

    if (auto* spd = apvts.getRawParameterValue("analyzerSpeed"))
    {
        int spdIdx = juce::jlimit(0, 2, static_cast<int>(std::round(spd->load())));

        analyzerSpeedCached = spdIdx;
        auto speed = toSpeed(analyzerSpeedCached);
        spectrumAnalyzer.setSpeed(speed);
        postEQAnalyzer.setSpeed(speed);
    }

    // Prepare components
    spectrumAnalyzer.prepare(sampleRate, samplesPerBlock);
    postEQAnalyzer.prepare(sampleRate, samplesPerBlock);
    eqProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dynamicEQProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());

    // NOTE: Shadow processors already prepared earlier (before updateEQFromParameters)

    // Mid/Side processors
    eqProcessorMid.prepare(sampleRate, samplesPerBlock, 1);  // Mono for Mid
    eqProcessorSide.prepare(sampleRate, samplesPerBlock, 1); // Mono for Side
    dynamicEQProcessorMid.prepare(sampleRate, samplesPerBlock, 1);
    dynamicEQProcessorSide.prepare(sampleRate, samplesPerBlock, 1);

    // M/S buffer
    if (getTotalNumInputChannels() >= 2)
    {
        msBuffer.setSize(2, preallocatedMaxSamples, false, false, true);
        msBuffer.clear();
    }

    // FIX 5: Pre-allocate M/S processing buffers with generous headroom
    midProcessBuffer.setSize(1, preallocatedMaxSamples, false, false, true);
    sideProcessBuffer.setSize(1, preallocatedMaxSamples, false, false, true);
    midProcessBuffer.clear();
    sideProcessBuffer.clear();

    // HQ (NaturalPhase) path with configurable oversampling
    int osFactor = static_cast<int>(apvts.getRawParameterValue("oversamplingFactor")->load());
    oversamplingFactor.store(osFactor, std::memory_order_relaxed);

    // Always create both oversamplers so Auto mode can select at runtime without re-allocating.
    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(getTotalNumInputChannels()),
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler2x->reset();
    oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));

    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(getTotalNumInputChannels()),
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler4x->reset();
    oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));

    // HQ (NaturalPhase) processors prepared to match effective selection (Off/2x -> 2x, 4x -> 4x)
    // "Off" intentionally shares the same 2x HQ path as 2x to keep the Natural-phase chain coherent.
    const int osMultiplier = (osFactor == 2 ? 4 : 2); // 0,1 -> 2x ; 2 -> 4x ; 3(Auto) -> prepare for 4x worst-case
    const double hqSampleRate = sampleRate * static_cast<double>(osMultiplier);
    const int hqBlockSize = samplesPerBlock * osMultiplier;
    eqProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());
    dynamicEQProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());

    naturalPhaseLatency = static_cast<int>(oversampler2x->getLatencyInSamples());
    const int maxHQSamples = juce::jmax(hqBlockSize * 4, preallocatedMaxSamples * osMultiplier);
    naturalOversampledBuffer.setSize(getTotalNumInputChannels(), maxHQSamples, false, false, true);
    naturalOversampledBuffer.clear();

    // Linear-phase processors (double-buffer)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());
    for (auto& lp : linearPhaseProcessors)
    {
        lp = std::make_unique<LinearPhaseProcessor>();
        lp->prepare(spec);
    }
    for (auto& loaded : linearIRLoaded)
        loaded.store(false, std::memory_order_relaxed);
    consecutiveIRReadyBlocks = 0;
    activeIRIndex.store(0);
    readyIRIndex.store(-1);

    // FIX: Pre-allocate crossfade buffer for smooth IR transitions
    crossfadeBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples);
    crossfadeBuffer.clear();
    crossfadeSamplesRemaining = 0;

    // Pre-allocate double buffers for lock-free IR handoff (builder thread -> audio thread).
    // IMPORTANT: The builder thread reads/writes pendingFreqIR.buffers concurrently.
    // We must pause it before reallocating the vectors, otherwise .assign() can
    // invalidate pointers the builder is using → use-after-free / segfault.
    {
        const bool builderWasRunning = irBuilderThread.joinable()
                                       && !stopIRBuilder.load(std::memory_order_relaxed);
        if (builderWasRunning)
        {
            stopIRBuilder.store(true, std::memory_order_release);
            irBuildEvent.signal();
            irBuilderThread.join();
        }

        const size_t freqBufSize = LinearPhaseProcessor::fftSize * 2;
        pendingFreqIR.buffers[0].assign(freqBufSize, 0.0f);
        pendingFreqIR.buffers[1].assign(freqBufSize, 0.0f);
        pendingFreqIR.readyIndex.store(-1, std::memory_order_relaxed);
        pendingFreqIR.writeIndex.store(0, std::memory_order_relaxed);
        pendingFreqIR.readingIndex.store(-1, std::memory_order_relaxed);

        // Restart the builder thread after safe reallocation
        if (builderWasRunning)
        {
            stopIRBuilder.store(false, std::memory_order_release);
            irBuilderThread = std::thread([this]() { irBuilderThreadFunc(); });
        }
    }

    // Pre-allocate silent spectrum buffer for processBlock fallback
    silentSpectrumBuffer.assign(aiSpectrumBins, -80.0f);
    previousIRIndex = 0;

    // Start OSC parameter server (deferred from constructor to avoid crash during plugin scan)
    if (oscParamServer && !oscParamServer->isRunning())
        oscParamServer->start();

    // Prime band smoothers
    primeBandSmoothers(sampleRate);

    // Linear-phase delay buffer for fallback alignment (no realloc in audio thread)
    const int lpDelaySamples = worstCaseLatencySamples + preallocatedMaxSamples;
    linearPhaseDelayBuffer.setSize(getTotalNumInputChannels(), lpDelaySamples, false, false, true);
    linearPhaseDelayBuffer.clear();
    linearPhaseDelayWritePos = 0;

    // Apply quality/latency mode to dynamic EQ lookahead
    int qualityMode = static_cast<int>(apvts.getRawParameterValue("qualityMode")->load());
    float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms lookahead, Zero-latency: 0ms
    dynamicEQProcessor.setLookahead(lookaheadMs);
    dynamicEQProcessor.updateLookaheadBuffer(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    qualityModeCached = qualityMode;
    aiEngine.prepare(sampleRate, samplesPerBlock);
    referenceMatcher.prepare(sampleRate, samplesPerBlock);

    // Prepare lock-free capture service (replaces old mutex-based capture)
    captureService.prepare(sampleRate, getTotalNumInputChannels(), samplesPerBlock);

    // Initialize bands if not already initialized
    if (eqProcessor.getNumBands() == 0)
    {
        float defaultFreqs[AIEqualizerAudioProcessor::maxBands] = {
            31.0f, 50.0f, 80.0f, 120.0f, 170.0f, 250.0f, 350.0f, 500.0f,
            700.0f, 1000.0f, 1400.0f, 2000.0f, 2800.0f, 4000.0f, 5600.0f, 8000.0f,
            11000.0f, 15000.0f, 18000.0f, 22000.0f, 26000.0f, 30000.0f, 34000.0f, 38000.0f
        };
        int active = numActiveBands.load(std::memory_order_relaxed);
        for (int i = 0; i < active; ++i)
        {
            juce::String prefix = "band" + juce::String(i);
            int type = ParametricEQProcessor::Peak;
            if (auto* typeParam = apvts.getRawParameterValue(prefix + "Type"))
                type = static_cast<int>(typeParam->load());
            else if (i == 0)
                type = ParametricEQProcessor::LowShelf;
            else if (i == active - 1)
                type = ParametricEQProcessor::HighShelf;

            eqProcessor.addBand(defaultFreqs[i], 0.0f, 1.0f, type);
        }
        // If fewer than active bands were added (max limit), adjust numActiveBands
        numActiveBands.store(std::min(active, eqProcessor.getNumBands()), std::memory_order_relaxed);
    }

    // Reset RMS values (atomic)
    preEQRMS.store(0.0f, std::memory_order_relaxed);
    postEQRMS.store(0.0f, std::memory_order_relaxed);
    autoGainCompensation.store(0.0f, std::memory_order_relaxed);
    autoGainBlockCounter = 0;
    parametersNeedUpdate.store(true, std::memory_order_relaxed);
    lastProcessedParameterChangeCounter.store(parameterChangeCounter.load(std::memory_order_relaxed),
                                              std::memory_order_relaxed);

    // FIX: Initialize smoothed output gain (50ms ramp time to prevent zippering)
    smoothedOutputGain.reset(sampleRate, 0.05);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);

    // Pre-compute AI analysis cadence (~10 Hz)
    aiAnalysisIntervalSamples = juce::jmax(static_cast<int>(std::round(sampleRate * 0.1)), samplesPerBlock);
    aiAnalysisSamples = 0;

    // Declare latency based on current phase mode:
    // - LinearPhase: partSize (128) when partitioned, hopSize (4096) when legacy OLA
    // - NaturalPhase: only oversampling latency (much lower, typically 32-64 samples)
    // We must NOT declare linear-phase latency in NaturalPhase - it would desync the DAW timeline.
    const auto currentMode = currentPhaseMode.load(std::memory_order_relaxed);
    const int linearPhaseLatency = (currentMode == PhaseMode::LinearPhase)
                                   ? static_cast<int>(LinearPhaseProcessor::usePartitioned
                                       ? LinearPhaseProcessor::partSize
                                       : LinearPhaseProcessor::hopSize)
                                   : 0;
    int oversamplingLatency = naturalPhaseLatency;
    if (oversampler4x)
        oversamplingLatency = std::max(oversamplingLatency, static_cast<int>(oversampler4x->getLatencyInSamples()));
    if (oversampler2x)
        oversamplingLatency = std::max(oversamplingLatency, static_cast<int>(oversampler2x->getLatencyInSamples()));
    // Report ACTUAL latency for the current mode, not worst-case.
    // This avoids unnecessary delay compensation in ZeroLatency/NaturalPhase modes.
    // When user switches phase mode, we update via parameterChanged().
    // Cache worst-case for reference but report actual.
    {
        juce::dsp::Oversampling<float> tempOversampler(
            static_cast<size_t>(getTotalNumInputChannels()),
            2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        tempOversampler.reset();
        worstCaseOversamplingLatency = std::max(oversamplingLatency, static_cast<int>(tempOversampler.getLatencyInSamples()));
    }
    int actualLatency;
    if (currentMode == PhaseMode::LinearPhase)
        actualLatency = linearPhaseLatency;
    else if (currentMode == PhaseMode::NaturalPhase)
        actualLatency = oversamplingLatency;
    else
        actualLatency = 0; // ZeroLatency: truly zero
    worstCaseLatencySamples = actualLatency;
    setLatencySamples(actualLatency);
    lastReportedLatency = actualLatency;

    // Signal that processor is ready for GUI access
    processorReady.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::releaseResources()
{
    spectrumAnalyzer.reset();
    postEQAnalyzer.reset();
    eqProcessor.reset();
    dynamicEQProcessor.reset();
}

bool AIEqualizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Main input/output must be mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain input (optional) - mono only
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sidechainSet = layouts.getChannelSet(false, 1);
        if (!sidechainSet.isDisabled() && sidechainSet != juce::AudioChannelSet::mono())
            return false;
    }

    return true;
}

//==============================================================================
void AIEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Guard: if prepareToPlay has not completed yet (e.g. host calls processBlock
    // during plugin scan before prepareToPlay), pass audio through silently and return.
    if (!processorReady.load(std::memory_order_acquire))
    {
        buffer.clear();
        return;
    }

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int inputBlockSamples = buffer.getNumSamples();
    const int blockSamples = juce::jmin(inputBlockSamples, preallocatedMaxSamples);

    // Snapshot all parameters once per block for atomic consistency (partial)
    AIEQCore::ProcessBlockParameters paramsSnapshot;
    loadParameterSnapshot(paramsSnapshot);

    if (pendingReset.exchange(false, std::memory_order_acq_rel))
    {
        if (oversampler2x) oversampler2x->reset();
        if (oversampler4x) oversampler4x->reset();

        for (auto& lp : linearPhaseProcessors)
        {
            if (lp)
                lp->reset();
        }

        readyIRIndex.store(-1, std::memory_order_relaxed);
        activeIRIndex.store(0, std::memory_order_relaxed);
        previousIRIndex = 0;
        crossfadeSamplesRemaining = 0;
        for (auto& loaded : linearIRLoaded)
            loaded.store(false, std::memory_order_relaxed);

        // If we reset while in Linear Phase, force an IR rebuild immediately
        if (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase)
        {
            consecutiveIRReadyBlocks = 0;
            triggerLinearPhaseIRUpdate();
        }

        // Re-prepare eqProcessorHQ / dynamicEQProcessorHQ at the correct oversampled rate.
        // prepare() is zero-allocation (stores atomics, resets POD, recalculates biquad
        // coefficients) — safe to call on the audio thread.
        // Without this, switching oversampling factor at runtime leaves HQ EQ coefficients
        // calculated for the wrong sample rate (e.g. still sr*2 while oversampler is now
        // feeding sr*4), causing wrong frequency response until the next prepareToPlay.
        {
            const int newOsFactor   = oversamplingFactor.load(std::memory_order_relaxed);
            const int newOsMult     = (newOsFactor == 2 ? 4 : 2); // mirrors prepareToPlay logic
            const double newHqRate  = currentSampleRate.load(std::memory_order_relaxed)
                                      * static_cast<double>(newOsMult);
            const int newHqBlock    = preallocatedMaxSamples * newOsMult;
            const int numChs        = getTotalNumInputChannels();
            eqProcessorHQ.prepare(newHqRate, newHqBlock, numChs);
            dynamicEQProcessorHQ.prepare(newHqRate, newHqBlock, numChs);
        }

        // Trigger a dry→wet crossfade to cover the reset transient.
        // Resetting oversamplers/LP processors from non-zero state causes a brief
        // dropout (IIR filters start from zero on a live signal). The bypass crossfade
        // mechanism already has the dry buffer capture and blend logic — reusing it
        // here covers the reset block with a smooth fade rather than an audible gap.
        currentBypassCrossfadeSamples = bypassCrossfadeSamples;
        bypassCrossfadeRemaining = currentBypassCrossfadeSamples;
    }

    // Defensive clamp: if host delivers a block bigger than we pre-allocated for
    // (e.g. Reaper with dynamic block sizes), process the safe portion and silence
    // the remainder to avoid buffer overruns.
    // NOTE: this should never happen in a correctly configured session - if it fires
    // frequently the host is not honouring the maximumBlockSize passed to prepareToPlay.
    if (inputBlockSamples > blockSamples)
    {
        blockClampEvents.fetch_add(1, std::memory_order_relaxed);
        AIEQ_LOG_WARNING("processBlock: host delivered " + juce::String(inputBlockSamples)
                         + " samples, pre-allocated max is " + juce::String(preallocatedMaxSamples)
                         + ". Silencing overflow region.");
       #if JUCE_DEBUG
        jassertfalse; // host is sending blocks larger than maximumBlockSize - investigate
       #endif
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear(ch, blockSamples, inputBlockSamples - blockSamples);
    }

    // Safety: if channel layout changed unexpectedly, bypass processing to avoid overflow
    if (buffer.getNumChannels() != preparedNumInputChannels)
    {
        blockClampEvents.fetch_add(1, std::memory_order_relaxed);
       #if JUCE_DEBUG
        jassertfalse; // layout mismatch: host likely reconfigured without new prepareToPlay
       #endif
        buffer.clear();
        return;
    }

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // Detect solo (audio thread, lock-free) and capture input for audition
    // FIX: Clamp to maxBands to prevent array out-of-bounds access
    const int activeBandsLocal = std::min(paramsSnapshot.numActiveBands, maxBands);
    bool hasSolo = false;
    for (int i = 0; i < activeBandsLocal; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];
        const bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        const bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        if (enabled && solo)
        {
            hasSolo = true;
            break;
        }
    }

    // (solo state reset handled after solo monitor block below)

    if (hasSolo)
    {
        if (preProcessingInputCopy.getNumChannels() >= buffer.getNumChannels()
            && preProcessingInputCopy.getNumSamples() >= blockSamples)
        {
            const int chs = juce::jmin(buffer.getNumChannels(), preProcessingInputCopy.getNumChannels());
            for (int ch = 0; ch < chs; ++ch)
                preProcessingInputCopy.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
        }
    }

    // Read bypass state early (needed to decide dryBuffer capture for crossfade)
    bool bypassed = false;
    if (cachedBypass)
        bypassed = cachedBypass->load(std::memory_order_relaxed) > 0.5f;

    // On the very first block after prepare, seed wasBypassed so we don't trigger
    // a spurious crossfade before the host has had a chance to set bypass state.
    if (!bypassStateInitialized)
    {
        wasBypassed = bypassed;
        bypassStateInitialized = true;
    }

    // Global dry/wet mix / bypass dry reference
    const float dryWetPct = loadParam(cachedDryWet, 100.0f);
    const float wet = juce::jlimit(0.0f, 100.0f, dryWetPct) * 0.01f;
    const float dry = 1.0f - wet;
    const bool needsDry = dry > 0.0001f || bypassed || bypassCrossfadeRemaining > 0;
    if (needsDry)
    {
        jassert(dryBuffer.getNumSamples() >= blockSamples);
        const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);
    }
    const int qualityModeParam = static_cast<int>(std::round(loadParam(cachedQualityMode, static_cast<float>(qualityModeCached))));
    const auto phaseModeSnapshot = static_cast<PhaseMode>(paramsSnapshot.phaseMode);
    const auto msModeSnapshot = static_cast<MSMode>(paramsSnapshot.msMode);
    const int analyzerResParam = static_cast<int>(std::round(loadParam(cachedAnalyzerResolution, static_cast<float>(analyzerResolutionCached))));
    const int analyzerSpdParam = static_cast<int>(std::round(loadParam(cachedAnalyzerSpeed, static_cast<float>(analyzerSpeedCached))));
    const bool autoGainEnabledLocal = loadParam(cachedAutoGain, 0.0f) > 0.5f;
    autoGainEnabled.store(autoGainEnabledLocal, std::memory_order_relaxed);
    const bool dynEqEnabledLocal = paramsSnapshot.dynamicEQEnabled;

    // NOTE: do NOT clear the meter cache here - the GUI reads at 30Hz while processBlock
    // runs at ~86Hz. Clearing every block means the GUI almost always reads 0.
    // Instead, let updateDynamicMeterCacheFrom() overwrite on each block where DynEQ runs,
    // and let the GUI meter decay visually on its own timer when no new data arrives.
    const bool aiEnabledLocal = loadParam(cachedAIEnabled, 1.0f) > 0.5f;
    const int sourceProfileIndex = static_cast<int>(std::round(loadParam(cachedSourceProfile, 0.0f)));
    const bool showPost = loadParam(cachedShowPostSpectrum, 0.0f) > 0.5f;
    float outputGainDB = paramsSnapshot.outputGainDB;

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Capture audio for analysis using lock-free CaptureService (even if bypassed)
    // This replaces the old mutex-based pushToCaptureRing - fully RT-safe
    const int captureDropped = captureService.pushSamples(buffer);
    juce::ignoreUnused(captureDropped);

    // Process any pending AI commands from the lock-free queue
    processAICommands();

    // Check bypass — with crossfade to avoid clicks on toggle
    if (bypassed != wasBypassed)
    {
        currentBypassCrossfadeSamples = bypassCrossfadeSamples;
        bypassCrossfadeRemaining = currentBypassCrossfadeSamples;
    }
    wasBypassed = bypassed;

    // AI correction: widen SmoothedValue ramp for gentler per-block coefficient steps.
    // AI correction: widen SmoothedValue ramp. Per-band crossfade for type changes
    // is handled in applySmoothedBandParams().
    if (aiCorrectionCrossfadePending.exchange(false, std::memory_order_acquire))
    {
        // Widen SmoothedValue ramp for gentler per-block coefficient steps
        const double sr = currentSampleRate.load(std::memory_order_relaxed);
        constexpr double correctionRampSec = 0.08; // 80ms
        for (int i = 0; i < maxBands; ++i)
        {
            auto idx = static_cast<size_t>(i);
            const float curFreq = smoothedBandFreq[idx].getCurrentValue();
            const float curGain = smoothedBandGain[idx].getCurrentValue();
            const float curQ    = smoothedBandQ[idx].getCurrentValue();
            smoothedBandFreq[idx].reset(sr, correctionRampSec);
            smoothedBandGain[idx].reset(sr, correctionRampSec);
            smoothedBandQ[idx].reset(sr, correctionRampSec);
            smoothedBandFreq[idx].setCurrentAndTargetValue(curFreq);
            smoothedBandGain[idx].setCurrentAndTargetValue(curGain);
            smoothedBandQ[idx].setCurrentAndTargetValue(curQ);
        }
        correctionSmoothingActive = true;
    }

    if (bypassed && bypassCrossfadeRemaining <= 0)
    {
        // Steady-state bypass: skip processing entirely
        clearDynamicMeterCache();
        return;
    }
    // If bypassed but still crossfading: fall through, process this block
    if (bypassed)
        clearDynamicMeterCache();

    // Handle quality/latency mode (adjust lookahead dynamically)
    int qualityMode = juce::jlimit(0, 1, qualityModeParam);
    if (qualityMode != qualityModeCached)
    {
        qualityModeCached = qualityMode;
        float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms, Zero-Latency: 0ms
        dynamicEQProcessor.setLookahead(lookaheadMs);
    }

    auto toResolution = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Resolution::Low;
            case 1: return SpectrumAnalyzer::Resolution::Medium;
            case 3: return SpectrumAnalyzer::Resolution::Max;
            case 2:
            default: return SpectrumAnalyzer::Resolution::High;
        }
    };

    auto toSpeed = [](int idx) {
        switch (idx)
        {
            case 0: return SpectrumAnalyzer::Speed::Fast;
            case 2: return SpectrumAnalyzer::Speed::Slow;
            case 1:
            default: return SpectrumAnalyzer::Speed::Medium;
        }
    };

    int resIdx = juce::jlimit(0, 3, analyzerResParam);
    if (resIdx != analyzerResolutionCached)
    {
        analyzerResolutionCached = resIdx;
        auto resolution = toResolution(resIdx);
        spectrumAnalyzer.setFFTResolution(resolution);
        postEQAnalyzer.setFFTResolution(resolution);
    }

    int spdIdx = juce::jlimit(0, 2, analyzerSpdParam);
    if (spdIdx != analyzerSpeedCached)
    {
        analyzerSpeedCached = spdIdx;
        auto speed = toSpeed(spdIdx);
        spectrumAnalyzer.setSpeed(speed);
        postEQAnalyzer.setSpeed(speed);
    }

    // Dispatch pending A/B whole-chain crossfade BEFORE updating coefficients,
    // so beginWholeChainCrossfade() snapshots the OLD coefficients/filter state.
    if (abCrossfadePending.load(std::memory_order_acquire))
    {
        eqProcessor.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorHQ.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorMid.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        eqProcessorSide.beginWholeChainCrossfade(abSwitchCrossfadeSamples);
        abCrossfadePending.store(false, std::memory_order_release);
    }

    // Update EQ parameters only when something actually changed
    const auto currentParamCounter = parameterChangeCounter.load(std::memory_order_acquire);
    const bool needsParamUpdate = parametersNeedUpdate.exchange(false, std::memory_order_acq_rel)
                                  || currentParamCounter != lastProcessedParameterChangeCounter.load(std::memory_order_relaxed);
    if (needsParamUpdate)
    {
        updateEQFromParameters();
        lastProcessedParameterChangeCounter.store(currentParamCounter, std::memory_order_relaxed);
    }

    // Apply smoothed band params (anti-zippering) before processing
    applySmoothedBandParams(blockSamples, needsParamUpdate);

    if (autoGainEnabledLocal)
    {
        float currentPreRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPreRMS = (currentPreRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;

        // FIX: Use atomic load/store for thread-safe access
        float currentRMS = preEQRMS.load(std::memory_order_relaxed);
        preEQRMS.store(currentRMS * rmsSmoothing + currentPreRMS * (1.0f - rmsSmoothing),
                       std::memory_order_relaxed);
    }

    // Feed pre-EQ spectrum analyzer (lock-free, FFT deferred to GUI)
    spectrumAnalyzer.pushSamples(buffer);
    spectrumDataReady.store(true, std::memory_order_release);

    // Skip AI analysis during offline rendering for performance
    bool isOffline = isNonRealtime();
    aiAnalysisSamples = std::min(aiAnalysisSamples + blockSamples, aiAnalysisIntervalSamples);
    const bool shouldRunAI = aiEnabledLocal && !isOffline && aiAnalysisSamples >= aiAnalysisIntervalSamples && aiAnalysisIntervalSamples > 0;

    if (shouldRunAI)
    {
        aiAnalysisSamples = 0;
        // FORCE: Always ensure AI engine is enabled
        aiEngine.setEnabled(true);

        // Update source profile
        aiEngine.setSourceProfile(static_cast<AIEngine::SourceProfile>(juce::jlimit(0, 6, sourceProfileIndex)));

        const auto& spectrum = spectrumAnalyzer.getSmoothedSpectrum();
        if (!spectrum.empty())
        {
            enqueueAISpectrum(spectrum);
        }
        else
        {
            const auto& dummySpectrum = silentSpectrumBuffer;
            enqueueAISpectrum(dummySpectrum);
        }
    }
    else if (!aiEnabledLocal)
    {
        // AI disabled - but still create test problem to verify system
        aiEngine.setEnabled(false);
    }

    const auto mode = phaseModeSnapshot;

    auto resolveNaturalOsEffective = [&]()
    {
        int osUser = oversamplingFactor.load(std::memory_order_relaxed);
        int osEffective = 1; // keep Natural-phase on a coherent 2x/4x HQ path; "Off" maps to 2x.

        if (osUser == 2)
        {
            osEffective = 2;
        }
        else if (osUser == oversamplingAutoIndex)
        {
            // Simple auto heuristic: use 4x if any band has Q > 8 or qualityMode == HQ
            osEffective = (qualityModeCached == 1) ? 2 : 1;
            const int activeBandsForAuto = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);
            for (int i = 0; i < activeBandsForAuto; ++i)
            {
                float qVal = eqProcessor.getBandQ(i);
                if (qVal > 8.0f)
                {
                    osEffective = 2;
                    break;
                }
            }
        }

        return osEffective;
    };

    auto processNaturalStereo = [&](juce::AudioBuffer<float>& targetBuffer,
                                    int osEffective,
                                    bool updateMeters)
    {
        oversamplingEffectiveFactor.store(osEffective, std::memory_order_relaxed);

        juce::dsp::Oversampling<float>* activeOversampler = (osEffective == 2 && oversampler4x)
            ? oversampler4x.get()
            : oversampler2x.get();

        if (activeOversampler)
        {
            juce::dsp::AudioBlock<float> block(targetBuffer.getArrayOfWritePointers(),
                                               static_cast<size_t>(targetBuffer.getNumChannels()),
                                               static_cast<size_t>(targetBuffer.getNumSamples()));
            auto upBlock = activeOversampler->processSamplesUp(block);

            const auto upChannels = static_cast<int>(upBlock.getNumChannels());
            const auto upSamples = static_cast<int>(upBlock.getNumSamples());
            jassert(naturalOversampledBuffer.getNumChannels() >= upChannels);
            jassert(naturalOversampledBuffer.getNumSamples() >= upSamples);
            for (int ch = 0; ch < upChannels; ++ch)
            {
                juce::FloatVectorOperations::copy(naturalOversampledBuffer.getWritePointer(ch),
                                                  upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                                  upSamples);
            }

            juce::AudioBuffer<float> hqProcessBuffer(
                naturalOversampledBuffer.getArrayOfWritePointers(),
                upChannels,
                upSamples);

            eqProcessorHQ.process(hqProcessBuffer);

            if (dynEqEnabledLocal)
            {
                dynamicEQProcessorHQ.process(hqProcessBuffer);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessorHQ);
            }

            for (int ch = 0; ch < upChannels; ++ch)
            {
                juce::FloatVectorOperations::copy(
                    upBlock.getChannelPointer(static_cast<size_t>(ch)),
                    naturalOversampledBuffer.getReadPointer(ch),
                    upSamples);
            }

            activeOversampler->processSamplesDown(block);
        }
        else
        {
            juce::AudioBuffer<float> processView(
                targetBuffer.getArrayOfWritePointers(),
                targetBuffer.getNumChannels(),
                targetBuffer.getNumSamples());
            eqProcessor.process(processView);
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(processView);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    };

    auto processStereoForPhaseMode = [&](juce::AudioBuffer<float>& targetBuffer,
                                         PhaseMode targetMode,
                                         bool updateMeters,
                                         int forcedNaturalOsEffective = 0)
    {
        if (targetMode == PhaseMode::NaturalPhase)
        {
            const int osEffective = forcedNaturalOsEffective > 0
                ? forcedNaturalOsEffective
                : resolveNaturalOsEffective();
            processNaturalStereo(targetBuffer, osEffective, updateMeters);
        }
        else
        {
            eqProcessor.process(targetBuffer);

            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(targetBuffer);
                if (updateMeters)
                    updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    };

    // Encode to M/S if needed (available for all phase modes)
    // Bug N/O fix: Mid-Only and Side-Only modes must NOT use M/S path - they should
    // process the corresponding component through the standard stereo EQ and then
    // reconstruct L/R correctly. The M/S encode+decode path is only correct for
    // MSLinked (process both components independently) because zeroing one component
    // before decode produces -3dB attenuation and/or phase inversion artefacts.
    // Fix: limit useMS to MSLinked only; Mid/Side solo handled after decode.
    const bool useMS = (msModeSnapshot == MSMode::MSLinked) && (totalNumInputChannels >= 2);
    const bool processMid = useMS; // MSLinked always processes both
    const bool processSide = useMS;
    if (useMS)
        encodeMidSide(buffer, blockSamples);

    // Skip zero-latency/Natural processing when in Linear Phase to avoid double-processing
    if (mode != PhaseMode::LinearPhase)
    {
        if (useMS)
        {
            const int samples = blockSamples;

            if (processMid)
                midProcessBuffer.copyFrom(0, 0, buffer, 0, 0, samples);
            else
                midProcessBuffer.clear(0, 0, samples);

            if (processSide)
                sideProcessBuffer.copyFrom(0, 0, buffer, 1, 0, samples);
            else
                sideProcessBuffer.clear(0, 0, samples);

            if (processMid)
            {
                juce::AudioBuffer<float> midView(midProcessBuffer.getArrayOfWritePointers(), 1, samples);
                eqProcessorMid.process(midView);
                if (dynEqEnabledLocal)
                    dynamicEQProcessorMid.process(midView);
            }

            if (processSide)
            {
                juce::AudioBuffer<float> sideView(sideProcessBuffer.getArrayOfWritePointers(), 1, samples);
                eqProcessorSide.process(sideView);
                if (dynEqEnabledLocal)
                    dynamicEQProcessorSide.process(sideView);
            }

            if (processMid)
                buffer.copyFrom(0, 0, midProcessBuffer, 0, 0, samples);
            else
                buffer.clear(0, 0, samples);

            if (processSide)
                buffer.copyFrom(1, 0, sideProcessBuffer, 0, 0, samples);
            else
                buffer.clear(1, 0, samples);

            if (dynEqEnabledLocal)
                updateDynamicMeterCacheFromMS(dynamicEQProcessorMid, dynamicEQProcessorSide, processMid, processSide);
        }
        else
        {
            const int transitionRemaining = phaseTransitionSamplesRemaining.load(std::memory_order_acquire);
            const auto transitionFromMode = static_cast<PhaseMode>(phaseTransitionFromMode.load(std::memory_order_acquire));
            const bool zeroNaturalTransition = transitionRemaining > 0
                && ((transitionFromMode == PhaseMode::ZeroLatency && mode == PhaseMode::NaturalPhase)
                 || (transitionFromMode == PhaseMode::NaturalPhase && mode == PhaseMode::ZeroLatency));

            const int currentNaturalOsEffective = resolveNaturalOsEffective();
            const int osTransitionRemaining = oversamplingTransitionSamplesRemaining.load(std::memory_order_acquire);
            const int osTransitionFromEffective = oversamplingTransitionFromEffective.load(std::memory_order_acquire);
            const bool naturalOversamplingTransition = mode == PhaseMode::NaturalPhase
                && osTransitionRemaining > 0
                && osTransitionFromEffective > 0
                && osTransitionFromEffective != currentNaturalOsEffective;

            if (zeroNaturalTransition
                && phaseTransitionBuffer.getNumChannels() >= buffer.getNumChannels()
                && phaseTransitionBuffer.getNumSamples() >= blockSamples)
            {
                const int chs = juce::jmin(buffer.getNumChannels(), phaseTransitionBuffer.getNumChannels());
                for (int ch = 0; ch < chs; ++ch)
                    phaseTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);

                processStereoForPhaseMode(buffer, mode, true);

                juce::AudioBuffer<float> oldModeView(phaseTransitionBuffer.getArrayOfWritePointers(),
                                                     buffer.getNumChannels(),
                                                     blockSamples);
                processStereoForPhaseMode(oldModeView, transitionFromMode, false);

                const int fadeLen = juce::jmin(transitionRemaining, blockSamples);
                const int fadeProgressStart = phaseTransitionCrossfadeSamples - transitionRemaining;
                for (int ch = 0; ch < chs; ++ch)
                {
                    float* newPtr = buffer.getWritePointer(ch);
                    const float* oldPtr = phaseTransitionBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float tBase = static_cast<float>(fadeProgressStart + s)
                                          / static_cast<float>(phaseTransitionCrossfadeSamples);
                        const float t = juce::jlimit(0.0f, 1.0f, tBase);
                        newPtr[s] = oldPtr[s] * (1.0f - t) + newPtr[s] * t;
                    }
                }

                const int remaining = juce::jmax(0, transitionRemaining - blockSamples);
                phaseTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    phaseTransitionFromMode.store(-1, std::memory_order_release);
            }
            else if (naturalOversamplingTransition
                     && oversamplingTransitionBuffer.getNumChannels() >= buffer.getNumChannels()
                     && oversamplingTransitionBuffer.getNumSamples() >= blockSamples)
            {
                const int chs = juce::jmin(buffer.getNumChannels(), oversamplingTransitionBuffer.getNumChannels());
                for (int ch = 0; ch < chs; ++ch)
                    oversamplingTransitionBuffer.copyFrom(ch, 0, buffer, ch, 0, blockSamples);

                processStereoForPhaseMode(buffer, mode, true, currentNaturalOsEffective);

                juce::AudioBuffer<float> oldOsView(oversamplingTransitionBuffer.getArrayOfWritePointers(),
                                                   buffer.getNumChannels(),
                                                   blockSamples);
                processStereoForPhaseMode(oldOsView, mode, false, osTransitionFromEffective);

                const int fadeLen = juce::jmin(osTransitionRemaining, blockSamples);
                const int fadeProgressStart = oversamplingTransitionCrossfadeSamples - osTransitionRemaining;
                for (int ch = 0; ch < chs; ++ch)
                {
                    float* newPtr = buffer.getWritePointer(ch);
                    const float* oldPtr = oversamplingTransitionBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float tBase = static_cast<float>(fadeProgressStart + s)
                                          / static_cast<float>(oversamplingTransitionCrossfadeSamples);
                        const float t = juce::jlimit(0.0f, 1.0f, tBase);
                        newPtr[s] = oldPtr[s] * (1.0f - t) + newPtr[s] * t;
                    }
                }

                const int remaining = juce::jmax(0, osTransitionRemaining - blockSamples);
                oversamplingTransitionSamplesRemaining.store(remaining, std::memory_order_release);
                if (remaining == 0)
                    oversamplingTransitionFromEffective.store(-1, std::memory_order_release);
            }
            else
            {
                processStereoForPhaseMode(buffer, mode, true);
            }
        }
    } // end if mode != LinearPhase

    // Linear Phase processing (applied only here, zero-latency path skipped above)
    // Note on M/S: when useMS=true (MSLinked), the buffer is already in M/S domain here
    // (ch0=Mid, ch1=Side). LP processes both channels identically with the same EQ curve,
    // which is correct for MSLinked (same EQ on both components). Mid/Side solo modes
    // are handled after decodeMidSide below, so they do not reach this path in M/S domain.
    if (mode == PhaseMode::LinearPhase)
    {
        updateLinearPhaseIRIfNeeded();

        const bool irLoaded = linearIRLoaded[0].load(std::memory_order_acquire);

        if (!irLoaded)
        {
            // No IR yet: fall back to zero-latency EQ
            eqProcessor.process(buffer);
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(buffer);
                updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
        else
        {
            // Single LP processor with internal OLA crossfade (4096-sample, hop-aligned)
            auto* lp = linearPhaseProcessors[0].get();
            if (lp != nullptr)
            {
                juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),
                                                   static_cast<size_t>(buffer.getNumChannels()),
                                                   static_cast<size_t>(blockSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(block);
                lp->process(ctx);
            }
            else
            {
                eqProcessor.process(buffer);
            }

            // Bug fix: Dynamic EQ must run after LP convolution in Linear Phase mode.
            // Previously it was skipped entirely when irLoaded=true, causing:
            // 1. Dynamic compression not applied in LP mode
            // 2. GR meter always showing 0 in LP mode
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(buffer);
                updateDynamicMeterCacheFrom(dynamicEQProcessor);
            }
        }
    }

    // Decode back to L/R if we processed in M/S domain (MSLinked only)
    if (useMS)
        decodeMidSide(buffer, blockSamples);

    // Bug N/O fix: Mid Only and Side Only solo modes.
    // Encode to M/S, keep only the desired component, decode back.
    // This avoids the -3dB / phase-inversion artifacts from the old approach.
    if (totalNumInputChannels >= 2)
    {
        if (msModeSnapshot == MSMode::Mid)
        {
            // Encode → zero side → decode: result is the Mid component as mono-compatible stereo
            encodeMidSide(buffer, blockSamples);
            buffer.clear(1, 0, blockSamples); // zero Side channel
            decodeMidSide(buffer, blockSamples);
        }
        else if (msModeSnapshot == MSMode::Side)
        {
            // Encode → zero mid → decode: result is the Side (difference) signal
            encodeMidSide(buffer, blockSamples);
            buffer.clear(0, 0, blockSamples); // zero Mid channel
            decodeMidSide(buffer, blockSamples);
        }
    }

    // Apply global dry/wet mix
    if (needsDry)
    {
        const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* wetPtr = buffer.getWritePointer(ch);
            const auto* dryPtr = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < blockSamples; ++i)
                wetPtr[i] = dry * dryPtr[i] + wet * wetPtr[i];
        }
    }

    // === SOLO ACOUSTIC MONITOR ===
    constexpr bool enableSoloMonitor = true; // Enabled: allow band audition in Solo mode
    if (hasSolo && enableSoloMonitor)
    {
        float soloFreq = 1000.0f;
        float soloQ = 1.0f;
        bool foundSolo = false;

        for (int i = 0; i < activeBandsLocal && !foundSolo; ++i)
        {
            const auto& p = cachedParams[static_cast<size_t>(i)];
            const bool enabled = loadParam(p.enabled, 0.0f) > 0.5f;
            const bool solo = loadParam(p.solo, 0.0f) > 0.5f;

            if (enabled && solo)
            {
                soloFreq = loadParam(p.freq, 1000.0f);
                soloQ = loadParam(p.q, 1.0f);
                foundSolo = true;
            }
        }

        if (foundSolo)
        {
            const double sr = currentSampleRate.load(std::memory_order_relaxed);
            const int numSamples = buffer.getNumSamples();

            // Update coefficients only when freq/Q actually change.
            // Also reset filter state on first use (wasSoloed == false)
            // to avoid stale IIR state producing a click on first block.
            const bool freqChanged = std::abs(soloFreq - lastSoloFreq) > 0.5f
                                  || std::abs(soloQ    - lastSoloQ)    > 0.01f;
            if (freqChanged || !wasSoloed)
            {
                auto coeffs = juce::IIRCoefficients::makeBandPass(sr, soloFreq, soloQ);

                if (!wasSoloed)
                {
                    // First enable: reset state + warmup + crossfade
                    soloMonitorFilterL.reset();
                    soloMonitorFilterR.reset();
                    soloMonitorFilterL.setCoefficients(coeffs);
                    soloMonitorFilterR.setCoefficients(coeffs);

                    // Warm up filter with pre-EQ input to avoid transient on enable
                    if (preProcessingInputCopy.getNumSamples() >= numSamples)
                    {
                        constexpr int warmupBlocks = 2;
                        const int warmupChs = juce::jmin(soloWarmupBuffer.getNumChannels(), preProcessingInputCopy.getNumChannels());
                        for (int wb = 0; wb < warmupBlocks; ++wb)
                        {
                            for (int ch = 0; ch < warmupChs; ++ch)
                                soloWarmupBuffer.copyFrom(ch, 0, preProcessingInputCopy, ch, 0, numSamples);
                            if (warmupChs > 0)
                                soloMonitorFilterL.processSamples(soloWarmupBuffer.getWritePointer(0), numSamples);
                            if (warmupChs > 1)
                                soloMonitorFilterR.processSamples(soloWarmupBuffer.getWritePointer(1), numSamples);
                        }
                    }

                    soloCrossfadeRemaining = soloCrossfadeSamples;
                }
                else
                {
                    // Already soloed, freq/Q changed during drag:
                    // Update coefficients WITHOUT resetting filter state.
                    // The IIR state adapts naturally to the new coefficients,
                    // avoiding the impulse/crackle caused by a hard reset.
                    soloMonitorFilterL.setCoefficients(coeffs);
                    soloMonitorFilterR.setCoefficients(coeffs);
                }

                lastSoloFreq = soloFreq;
                lastSoloQ    = soloQ;
            }

            // Build solo output using pre-allocated buffer (NO heap allocation)
            const int soloChs = juce::jmin(soloOutputBuffer.getNumChannels(), buffer.getNumChannels());
            if (preProcessingInputCopy.getNumChannels() >= buffer.getNumChannels()
                && preProcessingInputCopy.getNumSamples() >= numSamples)
            {
                for (int ch = 0; ch < soloChs; ++ch)
                    soloOutputBuffer.copyFrom(ch, 0, preProcessingInputCopy, ch, 0, numSamples);
            }
            else
            {
                for (int ch = 0; ch < soloChs; ++ch)
                    soloOutputBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }

            if (soloChs > 0)
                soloMonitorFilterL.processSamples(soloOutputBuffer.getWritePointer(0), numSamples);
            if (soloChs > 1)
                soloMonitorFilterR.processSamples(soloOutputBuffer.getWritePointer(1), numSamples);

            // Apply makeup gain in-place on pre-allocated buffer
            for (int ch = 0; ch < soloChs; ++ch)
                juce::FloatVectorOperations::multiply(soloOutputBuffer.getWritePointer(ch),
                    juce::Decibels::decibelsToGain(soloMakeupGainDB), numSamples);

            // Crossfade from wet EQ output → solo bandpass output to avoid click on enable
            if (soloCrossfadeRemaining > 0)
            {
                const int fadeLen = juce::jmin(soloCrossfadeRemaining, numSamples);
                for (int ch = 0; ch < soloChs; ++ch)
                {
                    float* wet  = buffer.getWritePointer(ch);
                    const float* dryS = soloOutputBuffer.getReadPointer(ch);
                    for (int s = 0; s < fadeLen; ++s)
                    {
                        const float t = static_cast<float>(soloCrossfadeSamples - soloCrossfadeRemaining + s)
                                      / static_cast<float>(soloCrossfadeSamples);
                        wet[s] = wet[s] * (1.0f - t) + dryS[s] * t;
                    }
                    // Rest of block: pure solo
                    for (int s = fadeLen; s < numSamples; ++s)
                        wet[s] = dryS[s];
                }
                soloCrossfadeRemaining = juce::jmax(0, soloCrossfadeRemaining - numSamples);
            }
            else
            {
                // Full solo — copy directly from pre-allocated buffer
                for (int ch = 0; ch < soloChs; ++ch)
                    buffer.copyFrom(ch, 0, soloOutputBuffer, ch, 0, numSamples);
            }

            wasSoloed = true;
        }
    }

    // Solo was active last block but not now — crossfade back to normal
    if (!hasSolo && wasSoloed)
    {
        wasSoloed = false;
        soloCrossfadeRemaining = 0;
        lastSoloFreq = -1.0f;
        lastSoloQ    = -1.0f;
        soloMonitorFilterL.reset();
        soloMonitorFilterR.reset();
    }

    // Feed post-EQ spectrum analyzer
    if (showPost)
    {
        postEQAnalyzer.pushSamples(buffer);
        spectrumDataReady.store(true, std::memory_order_release);
    }

    // Calculate auto-gain compensation
    if (autoGainEnabledLocal)
    {
        float currentPostRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPostRMS = (currentPostRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;

        // FIX: Use atomic load/store for thread-safe access
        float currentRMS = postEQRMS.load(std::memory_order_relaxed);
        postEQRMS.store(currentRMS * rmsSmoothing + currentPostRMS * (1.0f - rmsSmoothing),
                        std::memory_order_relaxed);

        autoGainBlockCounter = (autoGainBlockCounter + 1) % autoGainUpdateStride;
        if (autoGainBlockCounter == 0)
            calculateAutoGain();
    }
    else
    {
        autoGainBlockCounter = 0;
    }

    // Apply output gain (manual + auto-gain compensation) with SMOOTHING to prevent zippering
    float totalGainDB = outputGainDB;

    if (autoGainEnabledLocal)
    {
        // FIX: Use atomic load for thread-safe access
        totalGainDB += autoGainCompensation.load(std::memory_order_relaxed);
    }

    // FIX: Use SmoothedValue to prevent zippering artifacts when gain changes
    float targetGainLinear = juce::Decibels::decibelsToGain(totalGainDB);
    smoothedOutputGain.setTargetValue(targetGainLinear);

    // FIX: Use JUCE's optimized applyGain which handles stereo-linked gain correctly
    // This is more efficient than manual sample-by-sample loop and prevents stereo image shift
    const int numSamples = buffer.getNumSamples();
    smoothedOutputGain.applyGain(buffer, numSamples);

    // Bypass crossfade: blend processed+gained ↔ dry(ungained) to match steady-state behavior
    if (bypassCrossfadeRemaining > 0)
    {
        const int fadeLen = juce::jmin(bypassCrossfadeRemaining, numSamples);
        const int fadeTotal = juce::jmax(1, currentBypassCrossfadeSamples);
        const int fadeProgressStart = fadeTotal - bypassCrossfadeRemaining;
        const int chs = juce::jmin(buffer.getNumChannels(), dryBuffer.getNumChannels());

        for (int ch = 0; ch < chs; ++ch)
        {
            float* out = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);

            for (int s = 0; s < fadeLen; ++s)
            {
                const float t = juce::jlimit(0.0f, 1.0f,
                                static_cast<float>(fadeProgressStart + s)
                              / static_cast<float>(fadeTotal));
                // t: 0→1.  bypassed: wet→dry.  !bypassed: dry→wet.
                if (bypassed)
                    out[s] = out[s] * (1.0f - t) + dry[s] * t;
                else
                    out[s] = dry[s] * (1.0f - t) + out[s] * t;
            }

            // After crossfade ends mid-block: rest is target signal
            if (bypassed)
            {
                for (int s = fadeLen; s < numSamples; ++s)
                    out[s] = dry[s];
            }
        }
        bypassCrossfadeRemaining = juce::jmax(0, bypassCrossfadeRemaining - numSamples);
    }

    // Output peak metering (lock-free, for GUI level meter)
    {
        float peakL = buffer.getMagnitude(0, 0, numSamples);
        float peakR = totalNumInputChannels > 1 ? buffer.getMagnitude(1, 0, numSamples) : peakL;
        outputPeakLeft.store(peakL, std::memory_order_relaxed);
        outputPeakRight.store(peakR, std::memory_order_relaxed);
    }
}

void AIEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Increment the general parameter change counter for host automation, etc.
    parameterChangeCounter.fetch_add(1, std::memory_order_relaxed);

    // --- OPTIMIZATION: Selective EQ Curve Counter Increment ---
    // Only increment the EQ curve counter if a parameter that visually affects
    // the curve has changed. This prevents the expensive EQ image rebuild
    // from being triggered by host polling of non-visual parameters.
    
    bool affectsEQCurve = false;
    if (parameterID.startsWith("band"))
    {
        // A simple string check is faster than parsing the band index.
        // We only care if *any* band's visual parameter has changed.
        if (parameterID.contains("Freq") || parameterID.contains("Gain") || parameterID.contains("Q") ||
            parameterID.contains("Type") || parameterID.contains("Enabled") || parameterID.contains("Slope"))
        {
            affectsEQCurve = true;
        }
    }
    else if (parameterID == "numActiveBands")
    {
        affectsEQCurve = true;
    }

    if (affectsEQCurve)
    {
        eqCurveChangeCounter.fetch_add(1, std::memory_order_relaxed);
    }

    // --- End of Optimization ---

    // Handle specific parameter changes that require immediate action
    if (parameterID == "phaseMode")
    {
        const int modeIdx = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        const auto newMode = static_cast<PhaseMode>(modeIdx);
        const auto oldMode = currentPhaseMode.exchange(newMode);

        if (newMode != oldMode)
        {
            const bool zeroNaturalSwap =
                (oldMode == PhaseMode::ZeroLatency && newMode == PhaseMode::NaturalPhase)
             || (oldMode == PhaseMode::NaturalPhase && newMode == PhaseMode::ZeroLatency);

            if (zeroNaturalSwap)
            {
                phaseTransitionFromMode.store(static_cast<int>(oldMode), std::memory_order_release);
                phaseTransitionSamplesRemaining.store(phaseTransitionCrossfadeSamples, std::memory_order_release);
            }
            else
            {
                phaseTransitionFromMode.store(-1, std::memory_order_release);
                phaseTransitionSamplesRemaining.store(0, std::memory_order_release);
                pendingReset.store(true, std::memory_order_release);
            }
        }

        if (newMode == PhaseMode::LinearPhase)
        {
            consecutiveIRReadyBlocks = 0;
            readyIRIndex.store(-1, std::memory_order_relaxed);
            crossfadeSamplesRemaining = 0;
            triggerLinearPhaseIRUpdate();
        }

        {
            int newLatency = 0;
            if (newMode == PhaseMode::LinearPhase)
                newLatency = static_cast<int>(LinearPhaseProcessor::usePartitioned
                    ? LinearPhaseProcessor::partSize
                    : LinearPhaseProcessor::hopSize);
            else if (newMode == PhaseMode::NaturalPhase)
                newLatency = worstCaseOversamplingLatency;

            if (newLatency != lastReportedLatency)
            {
                worstCaseLatencySamples = newLatency;
                setLatencySamples(newLatency);
                lastReportedLatency = newLatency;
            }
        }
    }
    else if (parameterID == "msMode")
    {
        const int modeIdx = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        currentMSMode.store(static_cast<MSMode>(modeIdx));
    }
    else if (parameterID == "oversamplingFactor")
    {
        const int factor = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        const int oldFactor = oversamplingFactor.exchange(factor);

        auto normalizeNaturalPath = [](int value)
        {
            if (value == 2)
                return 2; // 4x
            if (value == oversamplingAutoIndex)
                return oversamplingAutoIndex;
            return 1; // Off and 2x both share the 2x Natural path
        };

        const int oldEffective = normalizeNaturalPath(oldFactor);
        const int newEffective = normalizeNaturalPath(factor);

        if (oldEffective != newEffective)
        {
            const bool naturalPhaseActive = currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::NaturalPhase;
            const bool canCrossfadeNaturalOversampling = naturalPhaseActive
                && oldEffective > 0
                && newEffective > 0
                && oldEffective != oversamplingAutoIndex
                && newEffective != oversamplingAutoIndex;

            if (canCrossfadeNaturalOversampling)
            {
                oversamplingTransitionFromEffective.store(oldEffective, std::memory_order_release);
                oversamplingTransitionSamplesRemaining.store(oversamplingTransitionCrossfadeSamples,
                                                            std::memory_order_release);
            }
            else
            {
                oversamplingTransitionFromEffective.store(-1, std::memory_order_release);
                oversamplingTransitionSamplesRemaining.store(0, std::memory_order_release);
                pendingReset.store(true, std::memory_order_release);
            }
        }
    }

    // Band changes always dirty the visible EQ/analyzer curve, but only Linear Phase
    // actually needs a background IR rebuild. Decoupling these avoids arming the LP
    // machinery on every live edit in Zero/Natural modes.
    if (parameterID.startsWith("band") || parameterID == "numActiveBands")
    {
        triggerEQCurveUpdate();

        if (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase)
            triggerLinearPhaseIRUpdate();
    }

    // Mark parameters as needing update for the audio thread's next processing block.
    parametersNeedUpdate.store(true, std::memory_order_release);
    markParametersChanged();
}

void AIEqualizerAudioProcessor::triggerEQCurveUpdate()
{
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::triggerLinearPhaseIRUpdate()
{
    eqCurveNeedsUpdate.store(true, std::memory_order_release);
    requestIRBuild();
}

void AIEqualizerAudioProcessor::updateLinearPhaseIRIfNeeded()
{
    // Lock-free: atomically claim the ready buffer index.
    int ri = pendingFreqIR.readyIndex.exchange(-1, std::memory_order_acquire);
    if (ri < 0)
        return;  // No new IR data available

    // ABA guard: mark the buffer we are about to read so the builder won't overwrite it.
    // Must be set before we dereference buffers[ri].data().
    pendingFreqIR.readingIndex.store(ri, std::memory_order_release);

    // Feed pre-computed freq-domain IR to LinearPhaseProcessor.
    // LinearPhaseProcessor handles the internal A/B crossfade.
    auto* lp = linearPhaseProcessors[0].get();
    if (lp != nullptr)
        lp->storeFreqIRDirect(pendingFreqIR.buffers[ri].data());

    // Signal that the read is complete so the builder may reuse this buffer.
    pendingFreqIR.readingIndex.store(-1, std::memory_order_release);

    linearIRLoaded[0].store(true, std::memory_order_release);
    activeIRIndex.store(0, std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::requestIRBuild()
{
    // Record timestamp; the IR builder thread checks if debounce has elapsed
    irBuildRequestedAt.store(juce::Time::currentTimeMillis(),
                             std::memory_order_release);
    irBuildEvent.signal();
}

void AIEqualizerAudioProcessor::enqueueAISpectrum(const std::vector<float>& spectrum)
{
    AISpectrumFrame frame;
    frame.fill(-100.0f);

    if (!spectrum.empty())
    {
        const size_t copyCount = std::min(frame.size(), spectrum.size());
        std::memcpy(frame.data(), spectrum.data(), copyCount * sizeof(float));
    }

    const bool pushed = aiSpectrumQueue.tryPush(frame);
    if (pushed)
        aiSpectrumEvent.signal();
}

//==============================================================================
void AIEqualizerAudioProcessor::calculateAutoGain()
{
    // FIX: Use atomic load for thread-safe access (UI may read these values)
    const float currentPreRMS = preEQRMS.load(std::memory_order_relaxed);
    const float currentPostRMS = postEQRMS.load(std::memory_order_relaxed);

    // Avoid division by zero and handle silence
    constexpr float minRMS = 0.0001f;
    if (currentPostRMS < minRMS || currentPreRMS < minRMS)
    {
        autoGainCompensation.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // Calculate the dB difference
    float preDB = juce::Decibels::gainToDecibels(currentPreRMS, -100.0f);
    float postDB = juce::Decibels::gainToDecibels(currentPostRMS, -100.0f);

    // Compensation = how much we need to boost to match pre-EQ level
    float targetCompensation = preDB - postDB;

    // Limit compensation range (-12 to +12 dB) for safety
    constexpr float maxCompensation = 12.0f;
    targetCompensation = juce::jlimit(-maxCompensation, maxCompensation, targetCompensation);

    // Smooth the compensation to avoid sudden jumps (1% per sample at 60Hz = ~0.6s time constant)
    constexpr float smoothingFactor = 0.99f;
    const float currentComp = autoGainCompensation.load(std::memory_order_relaxed);
    autoGainCompensation.store(currentComp * smoothingFactor + targetCompensation * (1.0f - smoothingFactor),
                               std::memory_order_relaxed);
}

//==============================================================================
DynamicEQProcessor::BandMeter AIEqualizerAudioProcessor::getDynamicBandMeter(int bandIndex) const noexcept
{
    DynamicEQProcessor::BandMeter meter;

    if (bandIndex >= 0 && bandIndex < maxBands)
    {
        const auto& entry = dynamicMeterCache[static_cast<size_t>(bandIndex)];
        meter.inputLevel = entry.input.load(std::memory_order_relaxed);
        meter.gainReduction = entry.gainReduction.load(std::memory_order_relaxed);
        meter.outputLevel = entry.output.load(std::memory_order_relaxed);
    }

    return meter;
}

float AIEqualizerAudioProcessor::getDynamicTotalGainReduction() const noexcept
{
    return dynamicTotalGR.load(std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::clearDynamicMeterCache() noexcept
{
    for (auto& entry : dynamicMeterCache)
    {
        entry.input.store(-100.0f, std::memory_order_relaxed);
        entry.gainReduction.store(0.0f, std::memory_order_relaxed);
        entry.output.store(-100.0f, std::memory_order_relaxed);
    }
    dynamicTotalGR.store(0.0f, std::memory_order_relaxed);
}

void AIEqualizerAudioProcessor::updateDynamicMeterCacheFrom(const DynamicEQProcessor& src) noexcept
{
    // FIX: Use minimum of both maxBands to prevent out-of-bounds access
    const int safeMax = std::min(DynamicEQProcessor::maxBands, maxBands);
    for (int i = 0; i < safeMax; ++i)
    {
        const auto meter = src.getBandMeter(i);
        auto& entry = dynamicMeterCache[static_cast<size_t>(i)];
        entry.input.store(meter.inputLevel, std::memory_order_relaxed);
        entry.gainReduction.store(meter.gainReduction, std::memory_order_relaxed);
        entry.output.store(meter.outputLevel, std::memory_order_relaxed);
    }
    dynamicTotalGR.store(src.getTotalGainReduction(), std::memory_order_relaxed);


}

void AIEqualizerAudioProcessor::updateDynamicMeterCacheFromMS(const DynamicEQProcessor& mid,
                                                              const DynamicEQProcessor& side,
                                                              bool includeMid,
                                                              bool includeSide) noexcept
{
    // FIX: Use minimum of both maxBands to prevent out-of-bounds access
    const int safeMax = std::min(DynamicEQProcessor::maxBands, maxBands);
    for (int i = 0; i < safeMax; ++i)
    {
        DynamicEQProcessor::BandMeter selected {};

        if (includeMid)
            selected = mid.getBandMeter(i);

        if (includeSide)
        {
            const auto sideMeter = side.getBandMeter(i);
            if (!includeMid || std::abs(sideMeter.gainReduction) > std::abs(selected.gainReduction))
                selected = sideMeter;
        }

        auto& entry = dynamicMeterCache[static_cast<size_t>(i)];
        entry.input.store(selected.inputLevel, std::memory_order_relaxed);
        entry.gainReduction.store(selected.gainReduction, std::memory_order_relaxed);
        entry.output.store(selected.outputLevel, std::memory_order_relaxed);
    }

    const float midTotal = includeMid ? mid.getTotalGainReduction() : 0.0f;
    const float sideTotal = includeSide ? side.getTotalGainReduction() : 0.0f;
    dynamicTotalGR.store((std::abs(sideTotal) > std::abs(midTotal)) ? sideTotal : midTotal,
                         std::memory_order_relaxed);
}

//==============================================================================
// FIX 8: Report a fixed worst-case latency to avoid host reconfiguration
void AIEqualizerAudioProcessor::updateReportedLatency()
{
    if (worstCaseLatencySamples != lastReportedLatency)
    {
        setLatencySamples(worstCaseLatencySamples);
        lastReportedLatency = worstCaseLatencySamples;
    }
}

void AIEqualizerAudioProcessor::primeBandSmoothers(double sampleRate)
{
    constexpr double rampSeconds = 0.02;  // 20ms for freq/gain
    constexpr double qRampSeconds = 0.03; // 30ms for Q (longer: coeff sensitivity)
    for (int i = 0; i < maxBands; ++i)
    {
        auto idx = static_cast<size_t>(i);
        smoothedBandFreq[idx].reset(sampleRate, rampSeconds);
        smoothedBandGain[idx].reset(sampleRate, rampSeconds);
        smoothedBandQ[idx].reset(sampleRate, qRampSeconds);
        smoothedBandFreq[idx].setCurrentAndTargetValue(targetBandFreq[idx]);
        smoothedBandGain[idx].setCurrentAndTargetValue(targetBandGain[idx]);
        smoothedBandQ[idx].setCurrentAndTargetValue(targetBandQ[idx]);
        prevAppliedBandType[idx] = targetBandType[idx];
    }
    bandSmoothingPrimed = true;
}

void AIEqualizerAudioProcessor::applySmoothedBandParams(int blockSamples, bool paramsChanged)
{
    const int activeBandsLocal = numActiveBands.load(std::memory_order_relaxed);
    const int availableBands = std::min({ activeBandsLocal, maxBands,
                                          eqProcessor.getNumBands(),
                                          eqProcessorHQ.getNumBands(),
                                          eqProcessorMid.getNumBands(),
                                          eqProcessorSide.getNumBands() });

    for (int i = 0; i < availableBands; ++i)
    {
        auto idx = static_cast<size_t>(i);
        const bool isMoving = smoothedBandFreq[idx].isSmoothing()
                           || smoothedBandGain[idx].isSmoothing()
                           || smoothedBandQ[idx].isSmoothing();

        // Advance smoothing by one block
        smoothedBandFreq[idx].skip(blockSamples);
        smoothedBandGain[idx].skip(blockSamples);
        smoothedBandQ[idx].skip(blockSamples);

        if (!isMoving && !paramsChanged)
            continue;

        const float freq = smoothedBandFreq[idx].getCurrentValue();
        const float gain = smoothedBandGain[idx].getCurrentValue();
        const float q = smoothedBandQ[idx].getCurrentValue();
        const int type = targetBandType[idx];
        const int slope = targetBandSlope[idx];
        const bool enabled = targetBandEnabled[idx];
        const bool solo = targetBandSolo[idx];

        // Per-band output crossfade on filter topology change.
        // Processes same input through both old and new filter, blends over 128 samples.
        // This eliminates the transfer function discontinuity (e.g. HighShelf→Notch).
        if (type != prevAppliedBandType[idx])
        {
            if (i < eqProcessor.getNumBands())
                eqProcessor.beginBandCrossfade(i, 128);
            if (i < eqProcessorHQ.getNumBands())
                eqProcessorHQ.beginBandCrossfade(i, 128);
            if (i < eqProcessorMid.getNumBands())
                eqProcessorMid.beginBandCrossfade(i, 128);
            if (i < eqProcessorSide.getNumBands())
                eqProcessorSide.beginBandCrossfade(i, 128);
            prevAppliedBandType[idx] = type;
        }

        if (i < eqProcessor.getNumBands())
        {
            eqProcessor.setBandParameters(i, freq, gain, q, type);
            eqProcessor.setBandSlope(i, slope);
            eqProcessor.setBandEnabled(i, enabled);
            eqProcessor.setBandSolo(i, solo);
        }
        if (i < eqProcessorHQ.getNumBands())
        {
            eqProcessorHQ.setBandParameters(i, freq, gain, q, type);
            eqProcessorHQ.setBandSlope(i, slope);
            eqProcessorHQ.setBandEnabled(i, enabled);
            eqProcessorHQ.setBandSolo(i, solo);
        }
        if (i < eqProcessorMid.getNumBands())
        {
            eqProcessorMid.setBandParameters(i, freq, gain, q, type);
            eqProcessorMid.setBandSlope(i, slope);
            eqProcessorMid.setBandEnabled(i, enabled);
            eqProcessorMid.setBandSolo(i, solo);
        }
        if (i < eqProcessorSide.getNumBands())
        {
            eqProcessorSide.setBandParameters(i, freq, gain, q, type);
            eqProcessorSide.setBandSlope(i, slope);
            eqProcessorSide.setBandEnabled(i, enabled);
            eqProcessorSide.setBandSolo(i, solo);
        }

        if (i < maxBands)
        {
            auto dynParams = targetDynamicBandParams[idx];
            dynParams.frequency = freq;
            dynParams.gain = gain;
            dynParams.q = q;
            dynParams.filterType = type;
            dynParams.enabled = enabled;
            dynamicEQProcessor.setBandParams(i, dynParams);
            dynamicEQProcessorHQ.setBandParams(i, dynParams);
            dynamicEQProcessorMid.setBandParams(i, dynParams);
            dynamicEQProcessorSide.setBandParams(i, dynParams);
        }
    }

    // When AI correction ramp finishes, restore fast ramp for user drag responsiveness
    if (correctionSmoothingActive)
    {
        bool anyStillSmoothing = false;
        for (int i = 0; i < availableBands && !anyStillSmoothing; ++i)
        {
            auto idx = static_cast<size_t>(i);
            anyStillSmoothing = smoothedBandFreq[idx].isSmoothing()
                             || smoothedBandGain[idx].isSmoothing()
                             || smoothedBandQ[idx].isSmoothing();
        }
        if (!anyStillSmoothing)
        {
            const double sr = currentSampleRate.load(std::memory_order_relaxed);
            constexpr double fastRamp = 0.02;  // 20ms for drag
            constexpr double fastQRamp = 0.03; // 30ms for Q
            for (int i = 0; i < maxBands; ++i)
            {
                auto idx = static_cast<size_t>(i);
                const float f = smoothedBandFreq[idx].getCurrentValue();
                const float g = smoothedBandGain[idx].getCurrentValue();
                const float qv = smoothedBandQ[idx].getCurrentValue();
                smoothedBandFreq[idx].reset(sr, fastRamp);
                smoothedBandGain[idx].reset(sr, fastQRamp);
                smoothedBandQ[idx].reset(sr, fastQRamp);
                smoothedBandFreq[idx].setCurrentAndTargetValue(f);
                smoothedBandGain[idx].setCurrentAndTargetValue(g);
                smoothedBandQ[idx].setCurrentAndTargetValue(qv);
            }
            correctionSmoothingActive = false;
        }
    }
}

//==============================================================================
void AIEqualizerAudioProcessor::updateEQFromParameters()
{
    if (!parametersCached.load())
        cacheParameterPointers();

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load() : fallback;
    };

    // Update AI parameters (use cached pointers - NO hash map lookups in audio thread!)
    if (cachedAISensitivity)
    {
        float sensitivity = cachedAISensitivity->load(std::memory_order_relaxed);
        aiEngine.setSensitivity(sensitivity);
    }
    if (cachedAIStrength)
    {
        float strength = cachedAIStrength->load(std::memory_order_relaxed);
        aiEngine.setStrength(strength);
    }

    //--------------------------------------------------------------------------
    // Update Global Dynamic EQ settings (use cached pointers)
    //--------------------------------------------------------------------------
    float dynMix = 1.0f;
    bool dynAutoMakeup = false;
    if (cachedDynEqMix)
        dynMix = cachedDynEqMix->load(std::memory_order_relaxed) / 100.0f;
    if (cachedDynAutoMakeup)
        dynAutoMakeup = cachedDynAutoMakeup->load(std::memory_order_relaxed) > 0.5f;

    dynamicEQProcessor.setGlobalMix(dynMix);
    dynamicEQProcessor.setAutoMakeup(dynAutoMakeup);
    dynamicEQProcessorHQ.setGlobalMix(dynMix);
    dynamicEQProcessorHQ.setAutoMakeup(dynAutoMakeup);

    // Update active bands count (robust against NaN / invalid)
    {
        const float raw = loadParam(cachedNumActiveBands, 7.0f);
        const int idx = std::isfinite(raw) ? static_cast<int>(raw) : 7;
        numActiveBands.store(juce::jlimit(1, maxBands, idx + 1), std::memory_order_relaxed); // choice index starts at 0
    }

    // Clamp active bands to what is actually available in processors
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands(),
                                         eqProcessorMid.getNumBands(), eqProcessorSide.getNumBands() });
    const int currentBands = numActiveBands.load(std::memory_order_relaxed);
    numActiveBands.store(juce::jlimit(1, availableBands, currentBands), std::memory_order_relaxed);

    const int bandsAvailable = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    // FIX: Clamp to maxBands to prevent array out-of-bounds access
    const int activeBandsLocal = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);

    // Detect if any band is soloed (among active and enabled bands)
    bool hasSolo = false;
    for (int i = 0; i < activeBandsLocal; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];
        const bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        const bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        if (enabled && solo)
        {
            hasSolo = true;
            break;
        }
    }

    // Begin seqlock for IR shadow processors
    irCoeffVersion.fetch_add(1, std::memory_order_acq_rel); // mark write in progress (odd)

    // Update EQ bands
    for (int i = 0; i < bandsAvailable; ++i)
    {
        const auto& p = cachedParams[static_cast<size_t>(i)];

        float freq = loadParam(p.freq, 1000.0f);
        float gain = loadParam(p.gain, 0.0f);
        float q = loadParam(p.q, 1.0f);
        bool enabled = loadParam(p.enabled, (i < 8 ? 1.0f : 0.0f)) > 0.5f && (i < activeBandsLocal);
        bool solo = loadParam(p.solo, 0.0f) > 0.5f;
        const bool enabledFiltered = enabled && (!hasSolo || solo);

        int type = static_cast<int>(loadParam(p.type, 2.0f));
        int slopeVal = static_cast<int>(loadParam(p.slope, 0.0f));
        // Fallback for legacy states
        if (p.type == nullptr)
        {
            if (i == 0) type = ParametricEQProcessor::LowShelf;
            else if (i == activeBandsLocal - 1) type = ParametricEQProcessor::HighShelf;
        }

        // Do not slam the active audible EQ processors here.
        // updateEQFromParameters() should only refresh targets + shadow/IR state;
        // applySmoothedBandParams() is the single live-apply point for the audible EQ path.

        //----------------------------------------------------------------------
        // Update Dynamic EQ band parameters
        //----------------------------------------------------------------------
        int dynMode = static_cast<int>(loadParam(p.dynMode, 0.0f));
        float threshold = loadParam(p.dynThreshold, -20.0f);
        float ratio = loadParam(p.dynRatio, 2.0f);
        float attack = loadParam(p.dynAttack, 10.0f);
        float release = loadParam(p.dynRelease, 100.0f);
        float range = loadParam(p.dynRange, 24.0f);
        float knee = loadParam(p.dynKnee, 6.0f);

        DynamicEQProcessor::DynamicBandParams dynParams;
        dynParams.frequency = freq;
        dynParams.gain = gain;
        dynParams.q = q;
        dynParams.filterType = type;
        dynParams.enabled = enabledFiltered;
        dynParams.dynamicMode = dynMode;  // Now int, not enum class
        dynParams.threshold = threshold;
        dynParams.ratio = ratio;
        dynParams.attackMs = attack;
        dynParams.releaseMs = release;
        dynParams.range = range;
        dynParams.knee = knee;

        // Cache band targets for smoothing
        const auto idx = static_cast<size_t>(i);
        targetBandFreq[idx] = freq;
        targetBandGain[idx] = gain;
        targetBandQ[idx] = q;
        targetBandType[idx] = type;
        targetBandSlope[idx] = slopeVal;
        targetBandEnabled[idx] = enabledFiltered;
        targetBandSolo[idx] = solo;

        if (bandSmoothingPrimed)
        {
            smoothedBandFreq[idx].setTargetValue(freq);
            smoothedBandGain[idx].setTargetValue(gain);
            smoothedBandQ[idx].setTargetValue(q);
        }
        else
        {
            smoothedBandFreq[idx].setCurrentAndTargetValue(freq);
            smoothedBandGain[idx].setCurrentAndTargetValue(gain);
            smoothedBandQ[idx].setCurrentAndTargetValue(q);
        }

        targetDynamicBandParams[idx] = dynParams;

        // FIX: Update shadow processors for thread-safe IR building
        // These are read by the IR builder thread without locking
        if (i < eqProcessorForIR.getNumBands())
        {
            eqProcessorForIR.setBandParameters(i, freq, gain, q, type);
            eqProcessorForIR.setBandEnabled(i, enabledFiltered);
            eqProcessorForIR.setBandSolo(i, solo);
            eqProcessorForIR.setBandSlope(i, static_cast<int>(loadParam(p.slope, 0.0f)));
        }
        dynamicEQProcessorForIR.setBandParams(i, dynParams);
    }

    // Complete seqlock for IR shadow processors
    std::atomic_thread_fence(std::memory_order_release);
    irCoeffVersion.fetch_add(1, std::memory_order_release); // mark write complete (even)

    // Signal that new coefficients are ready for IR builder
    irCoefficientsUpdated.store(true, std::memory_order_release);

    bandSmoothingPrimed = true;
}

//==============================================================================
// A/B Comparison Implementation

void AIEqualizerAudioProcessor::setABState(ABState state)
{
    // CRITICAL: Must be called from Message Thread (GUI thread)
    // JUCE's APVTS parameter changes are NOT thread-safe
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        // SAFETY: If called from wrong thread, defer to Message Thread
        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
        juce::MessageManager::callAsync([weakThis, state]()
        {
            if (auto* self = weakThis.get())
                self->setABState(state);
        });
        return;
    }

    const ABState current = currentABState.load(std::memory_order_relaxed);
    if (state == current)
        return;

    // Save current state to current slot BEFORE switching
    saveCurrentStateToSlot(current);

    // Switch to new state atomically
    currentABState.store(state, std::memory_order_release);

    // Load new state (must be on Message Thread for APVTS access)
    loadStateFromSlot(state);
}

void AIEqualizerAudioProcessor::saveCurrentStateToSlot(ABState slot)
{
    // CRITICAL: Must be called from Message Thread for APVTS access
    // SAFETY: Runtime check (jassert is disabled in release builds)
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        jassertfalse; // Debug breakpoint
        return; // Fail silently in release to prevent crash
    }

    EQSlot* targetSlot = nullptr;
    switch (slot)
    {
        case ABState::A: targetSlot = &slotA; break;
        case ABState::B: targetSlot = &slotB; break;
        case ABState::C: targetSlot = &slotC; break;
        case ABState::D: targetSlot = &slotD; break;
    }
    if (!targetSlot) return;

    // Save all band states with bounds checking
    for (int i = 0; i < maxBands; ++i)
    {
        targetSlot->bands[static_cast<size_t>(i)] = getBandState(i);
    }

    // Save output gain safely
    if (auto* outputGainParam = apvts.getRawParameterValue("outputGain"))
    {
        targetSlot->outputGain = outputGainParam->load();
    }
    else
    {
        targetSlot->outputGain = 0.0f; // Default if parameter not found
    }

    if (auto* dynEqEnabledParam = apvts.getRawParameterValue("dynEqEnabled"))
        targetSlot->dynEqEnabled = dynEqEnabledParam->load() > 0.5f;
    else
        targetSlot->dynEqEnabled = true;

    if (auto* dynEqMixParam = apvts.getRawParameterValue("dynEqMix"))
        targetSlot->dynEqMix = dynEqMixParam->load();
    else
        targetSlot->dynEqMix = 100.0f;

    if (auto* dynAutoMakeupParam = apvts.getRawParameterValue("dynAutoMakeup"))
        targetSlot->dynAutoMakeup = dynAutoMakeupParam->load() > 0.5f;
    else
        targetSlot->dynAutoMakeup = false;
}

void AIEqualizerAudioProcessor::loadStateFromSlot(ABState slot)
{
    // CRITICAL: Must be called from Message Thread
    // SAFETY: Runtime check (jassert is disabled in release builds)
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        jassertfalse; // Debug breakpoint
        return; // Fail silently in release to prevent crash
    }

    const EQSlot* sourceSlot = nullptr;
    switch (slot)
    {
        case ABState::A: sourceSlot = &slotA; break;
        case ABState::B: sourceSlot = &slotB; break;
        case ABState::C: sourceSlot = &slotC; break;
        case ABState::D: sourceSlot = &slotD; break;
    }
    if (!sourceSlot) return;

    bool anyMaterialChange = false;

    // Load band states — clamp frequency instead of silently skipping the band,
    // so an out-of-range value never causes a band to disappear unexpectedly.
    for (int i = 0; i < maxBands; ++i)
    {
        auto bandState = sourceSlot->bands[static_cast<size_t>(i)];
        bandState.frequency = juce::jlimit(20.0f, 20000.0f, bandState.frequency);

        const auto currentState = getBandState(i);
        const bool materiallyChanged =
            std::abs(currentState.frequency - bandState.frequency) > 1.0f ||
            std::abs(currentState.gain - bandState.gain) > 0.05f ||
            std::abs(currentState.q - bandState.q) > 0.02f ||
            currentState.type != bandState.type ||
            currentState.enabled != bandState.enabled ||
            currentState.solo != bandState.solo ||
            currentState.slope != bandState.slope ||
            currentState.dynMode != bandState.dynMode ||
            std::abs(currentState.dynThreshold - bandState.dynThreshold) > 0.05f ||
            std::abs(currentState.dynRatio - bandState.dynRatio) > 0.02f ||
            std::abs(currentState.dynAttack - bandState.dynAttack) > 0.1f ||
            std::abs(currentState.dynRelease - bandState.dynRelease) > 0.5f ||
            std::abs(currentState.dynRange - bandState.dynRange) > 0.05f ||
            std::abs(currentState.dynKnee - bandState.dynKnee) > 0.05f;

        // Arm per-band crossfade flag BEFORE applying delta (message thread).
        // The audio thread will dispatch beginBandCrossfade() before updating coefficients.
        abCrossfadePendingBands[i].store(materiallyChanged, std::memory_order_relaxed);

        if (materiallyChanged)
        {
            anyMaterialChange = applyBandStateDelta(i, bandState, false) || anyMaterialChange;
        }
    }

    // Load output gain with validation
    if (auto* param = apvts.getParameter("outputGain"))
    {
        const float gain = juce::jlimit(-24.0f, 24.0f, sourceSlot->outputGain);
        if (auto* raw = apvts.getRawParameterValue("outputGain"); raw != nullptr)
        {
            if (std::abs(raw->load() - gain) > 0.01f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(gain));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynEqEnabled"))
    {
        const bool desired = sourceSlot->dynEqEnabled;
        if (auto* raw = apvts.getRawParameterValue("dynEqEnabled"); raw != nullptr)
        {
            const bool current = raw->load() > 0.5f;
            if (current != desired)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(desired ? 1.0f : 0.0f));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynEqMix"))
    {
        const float mix = juce::jlimit(0.0f, 100.0f, sourceSlot->dynEqMix);
        if (auto* raw = apvts.getRawParameterValue("dynEqMix"); raw != nullptr)
        {
            if (std::abs(raw->load() - mix) > 0.01f)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(mix));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    if (auto* param = apvts.getParameter("dynAutoMakeup"))
    {
        const bool desired = sourceSlot->dynAutoMakeup;
        if (auto* raw = apvts.getRawParameterValue("dynAutoMakeup"); raw != nullptr)
        {
            const bool current = raw->load() > 0.5f;
            if (current != desired)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(desired ? 1.0f : 0.0f));
                param->endChangeGesture();
                anyMaterialChange = true;
            }
        }
    }

    // Arm pending A/B whole-chain crossfade for audio thread dispatch.
    // Do NOT arm the dry↔wet bypass crossfade — it would overwrite the whole-chain blend.
    if (anyMaterialChange)
    {
        abCrossfadePending.store(true, std::memory_order_release);
    }
}

void AIEqualizerAudioProcessor::copyAtoB()
{
    saveCurrentStateToSlot(ABState::A);
    slotB = slotA;
}

void AIEqualizerAudioProcessor::copyBtoA()
{
    saveCurrentStateToSlot(ABState::B);
    slotA = slotB;
}

void AIEqualizerAudioProcessor::copyAtoC()
{
    saveCurrentStateToSlot(ABState::A);
    slotC = slotA;
}

void AIEqualizerAudioProcessor::copyAtoD()
{
    saveCurrentStateToSlot(ABState::A);
    slotD = slotA;
}

void AIEqualizerAudioProcessor::copyBtoC()
{
    saveCurrentStateToSlot(ABState::B);
    slotC = slotB;
}

void AIEqualizerAudioProcessor::copyBtoD()
{
    saveCurrentStateToSlot(ABState::B);
    slotD = slotB;
}

void AIEqualizerAudioProcessor::copyCtoD()
{
    saveCurrentStateToSlot(ABState::C);
    slotD = slotC;
}

void AIEqualizerAudioProcessor::swapAB()
{
    const ABState current = currentABState.load(std::memory_order_relaxed);
    saveCurrentStateToSlot(current);
    std::swap(slotA, slotB);
    loadStateFromSlot(current);
}

void AIEqualizerAudioProcessor::swapCD()
{
    const ABState current = currentABState.load(std::memory_order_relaxed);
    saveCurrentStateToSlot(current);
    std::swap(slotC, slotD);
    loadStateFromSlot(current);
}

//==============================================================================
// Source Profile

void AIEqualizerAudioProcessor::setSourceProfile(AIEngine::SourceProfile profile)
{
    aiEngine.setSourceProfile(profile);

    // Also update the parameter
    if (auto* param = apvts.getParameter("sourceProfile"))
    {
        int index = static_cast<int>(profile);
        param->setValueNotifyingHost(static_cast<float>(index) / 6.0f);
    }
}

void AIEqualizerAudioProcessor::setNumActiveBands(int n) noexcept
{
    numActiveBands.store(juce::jlimit(1, maxBands, n), std::memory_order_relaxed);
    ensureBandCount(maxBands);
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    const int currentBands = numActiveBands.load(std::memory_order_relaxed);
    const int clampedBands = juce::jlimit(1, availableBands, currentBands);
    numActiveBands.store(clampedBands, std::memory_order_relaxed);
    for (int i = clampedBands; i < eqProcessor.getNumBands(); ++i)
        eqProcessor.setBandEnabled(i, false);
    for (int i = clampedBands; i < eqProcessorHQ.getNumBands(); ++i)
        eqProcessorHQ.setBandEnabled(i, false);
}

void AIEqualizerAudioProcessor::ensureBandCount(int count)
{
    const int target = juce::jlimit(1, maxBands, count);
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);  // Cache for lambda

    auto addMissing = [this, target, activeBands](ParametricEQProcessor& proc)
    {
        int current = proc.getNumBands();
        for (int i = current; i < target; ++i)
        {
            int type = ParametricEQProcessor::Peak;
            if (i == 0)
                type = ParametricEQProcessor::LowShelf;
            else if (i == maxBands - 1)
                type = ParametricEQProcessor::HighShelf;

            float freq = defaultBandFrequencies[i];
            proc.addBand(freq, 0.0f, 1.0f, type);
            proc.setBandEnabled(i, i < activeBands);
        }
    };

    addMissing(eqProcessor);
    addMissing(eqProcessorHQ);
    addMissing(eqProcessorMid);
    addMissing(eqProcessorSide);

    // FIX: Ensure shadow processor has same band count for thread-safe IR building
    addMissing(eqProcessorForIR);
}

//==============================================================================
// Audio Capture Forwarding to CaptureService
//==============================================================================

// Note: pushToCaptureRing removed - now using captureService.pushSamples() directly in processBlock

void AIEqualizerAudioProcessor::captureAudioSnapshotMs(int lengthMs)
{
    // Delegate to lock-free CaptureService
    captureBufferReady.store(false, std::memory_order_release);
    captureService.captureSnapshotMs(lengthMs);
    captureBufferReady.store(true, std::memory_order_release);
}

bool AIEqualizerAudioProcessor::runCapturedAudioAnalysis()
{
    // Get captured audio from CaptureService (no locks needed - already thread-safe)
    const auto& monoCopy = captureService.getCapturedAudioMono();
    const double sr = captureService.getCapturedSampleRate();

    // Validate data
    if (monoCopy.empty() || sr <= 0.0 || sr > 192000.0 || sr < 8000.0)
        return false;

    try
    {
        // Offline spectrum analysis on captured audio
        SpectrumAnalyzer analyzer;
        analyzer.prepare(sr, 512);

        // Create buffer safely
        const int bufferSize = std::min(static_cast<int>(monoCopy.size()),
                                        static_cast<int>(sr * 60.0)); // Max 60 seconds
        if (bufferSize <= 0)
            return false;

        juce::AudioBuffer<float> tempBuffer(1, bufferSize);

        // Copy data safely
        float* writePtr = tempBuffer.getWritePointer(0);
        if (writePtr != nullptr)
        {
            std::memcpy(writePtr, monoCopy.data(), static_cast<size_t>(bufferSize) * sizeof(float));
        }
        else
        {
            return false;
        }

        // Process in chunks (512 samples)
        int offset = 0;
        const int total = tempBuffer.getNumSamples();
        const int block = 512;

        while (offset < total)
        {
            const int chunk = std::min(block, total - offset);
            if (chunk <= 0)
                break;

            juce::AudioBuffer<float> slice(tempBuffer.getArrayOfWritePointers(), 1, offset, chunk);
            if (slice.getNumSamples() > 0)
                analyzer.pushSamples(slice);

            offset += chunk;
        }

        // Run FFT and analyze
        analyzer.processFFT();
        const auto& spectrum = analyzer.getSmoothedSpectrum();
        if (!spectrum.empty())
        {
            aiEngine.analyzeSpectrum(spectrum, true);
            return true;
        }
    }
    catch (...)
    {
        return false;
    }

    return false;
}

bool AIEqualizerAudioProcessor::analyzeCapturedAudioSnapshot()
{
    captureAnalysisCompleted.store(false, std::memory_order_release);

    if (!captureBufferReady.load(std::memory_order_acquire))
        return false;

    bool expected = false;
    if (!captureAnalysisInFlight.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return false;

    auto finish = [this](bool ok)
    {
        captureAnalysisResult.store(ok, std::memory_order_release);
        captureAnalysisCompleted.store(true, std::memory_order_release);
        captureAnalysisInFlight.store(false, std::memory_order_release);
    };

    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm != nullptr && mm->isThisTheMessageThread())
    {
        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
        captureAnalysisThread = std::thread([this, weakThis, finish]() mutable
        {
                        const bool ok = runCapturedAudioAnalysis();
            finish(ok);

            juce::MessageManager::callAsync([weakThis, ok]()
            {
                if (auto* self = weakThis.get())
                    self->aiProblemsChanged.store(true, std::memory_order_release);
                juce::ignoreUnused(ok);
            });
        });

        return true;
    }

    const bool ok = runCapturedAudioAnalysis();
    finish(ok);
    return ok;
}

bool AIEqualizerAudioProcessor::startManualCapture()
{
    // Delegate to lock-free CaptureService
    captureBufferReady.store(false, std::memory_order_release);
    return captureService.startManualCapture();
}

void AIEqualizerAudioProcessor::stopManualCapture()
{
    // Delegate to lock-free CaptureService
    captureService.stopManualCapture();
    captureBufferReady.store(true, std::memory_order_release);
}

void AIEqualizerAudioProcessor::getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples) const
{
    // Delegate to lock-free CaptureService
    captureService.getManualCapturePreview(outMono, maxSamples);
}

//==============================================================================
// FIX: applyAICorrections() with undo support and user learning recording

// Type-safe mapping from AI suggestion to processor filter enum (avoids fragile integer indices)
static ParametricEQProcessor::FilterType aiFilterTypeToProcessorType(AIEngine::Correction::FilterType filterType)
{
    switch (filterType)
    {
        case AIEngine::Correction::FilterType::LowCut:    return ParametricEQProcessor::FilterType::LowCut;
        case AIEngine::Correction::FilterType::LowShelf:  return ParametricEQProcessor::FilterType::LowShelf;
        case AIEngine::Correction::FilterType::HighShelf: return ParametricEQProcessor::FilterType::HighShelf;
        case AIEngine::Correction::FilterType::HighCut:   return ParametricEQProcessor::FilterType::HighCut;
        case AIEngine::Correction::FilterType::Notch:     return ParametricEQProcessor::FilterType::Notch;
        case AIEngine::Correction::FilterType::Peak:
        default:
            return ParametricEQProcessor::FilterType::Peak;
    }
}

void AIEqualizerAudioProcessor::applyAICorrections()
{
    // FIX: Get ONLY approved corrections (user-approved, not all pending)
    // This ensures that when user clicks to fix a single problem, only that one is applied
    auto approved = aiEngine.getApprovedCorrections();

    if (approved.empty())
        return;

    // Cache numActiveBands for this function (atomic load once)
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);

    // Merge nearby approved corrections to avoid duplicate bands
    auto merged = aiEngine.mergeNearbyCorrections(approved);

    // Limit to available bands (but don't hard-limit to 8)
    const int maxAssignable = std::min(static_cast<int>(merged.size()), activeBands);
    merged.resize(maxAssignable);

    if (merged.empty())
        return;

    // Save current state for undo BEFORE applying corrections (using HistoryManager)
    historyManager.pushUndoState("AI Correction Applied (" + juce::String(merged.size()) + " bands)");

    // Track which bands are already used
    std::vector<bool> bandUsed(maxBands, false);
    std::vector<float> bandFreqs(maxBands, 0.0f);

    // Pre-scan existing bands
    for (int i = 0; i < activeBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        {
            bandFreqs[i] = freqParam->load();
            if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
            {
                // Consider band "used" if gain is significant
                bandUsed[i] = std::abs(gainParam->load()) > 0.5f;
            }
        }
    }

    bool anyMaterialChange = false;

    // Assign corrections to bands intelligently
    for (const auto& corr : merged)
    {
        // Get scaled correction based on strength
        auto scaled = aiEngine.getScaledCorrection(corr);

        // Intelligent band assignment:
        // 1. Try to reuse existing band if within 1/5 octave and compatible type
        // 2. Otherwise find unused band closest to target frequency
        // 3. If no unused band, use closest available band

        int bestBand = -1;
        float bestScore = std::numeric_limits<float>::max();

        const float targetFreq = scaled.frequency;
        const float reuseThreshold = targetFreq * kBandReuseThresholdOctave;

        for (int i = 0; i < activeBands; ++i)
        {
            float dist = std::abs(bandFreqs[i] - targetFreq);
            float score = dist;

            // Prefer unused bands (much lower score)
            if (!bandUsed[i])
            {
                score *= kUnusedBandScoreMult;
            }

            // Prefer bands within reuse threshold (can merge)
            if (dist < reuseThreshold)
            {
                score *= kReuseThresholdScoreMult;
            }

            if (score < bestScore)
            {
                bestScore = score;
                bestBand = i;
            }
        }

        if (bestBand >= 0)
        {
            auto state = getBandState(bestBand);
            state.frequency = scaled.frequency;
            state.gain = scaled.suggestedGain;
            state.q = scaled.suggestedQ;
            state.type = static_cast<int>(aiFilterTypeToProcessorType(scaled.suggestedFilter));
            state.enabled = true;
            state.solo = false;

            anyMaterialChange = applyBandStateDelta(bestBand, state, false) || anyMaterialChange;

            // FIX: Record this to user learning system for better future suggestions
            // Only record if learning is enabled (privacy control)
            bool learningEnabled = apvts.getRawParameterValue("learningEnabled")->load() > 0.5f;
            if (learningEnabled)
            {
                userLearning.recordAISuggestionAccepted(
                    AIEngine::getProblemTypeName(corr.type),
                    scaled.frequency,
                    scaled.suggestedGain,
                    scaled.suggestedQ
                );
            }

            // Mark band as used
            bandUsed[bestBand] = true;
            bandFreqs[bestBand] = scaled.frequency;
        }
    }

    // Trigger dry→wet crossfade only if AI application actually changed band state.
    if (anyMaterialChange)
        aiCorrectionCrossfadePending.store(true, std::memory_order_release);

    // Clear ONLY approved corrections after applying (keep pending for future approval)
    // This allows user to approve more corrections later without losing pending ones
    aiEngine.clearApprovedCorrections();
}

void AIEqualizerAudioProcessor::applySingleCorrection(const AIEngine::Correction& correction)
{
    // Apply ONLY this specific correction (not all approved ones)
    // This is used when user clicks on a single problem to fix it

    // Cache numActiveBands for this function (atomic load once)
    const int activeBands = numActiveBands.load(std::memory_order_relaxed);

    // Get scaled correction based on strength
    auto scaled = aiEngine.getScaledCorrection(correction);

    // Save current state for undo BEFORE applying correction (using HistoryManager)
    historyManager.pushUndoState("AI Correction Applied: " + AIEngine::getProblemTypeName(correction.type) + " @ " + juce::String(correction.frequency, 1) + " Hz");

    // Track which bands are already used
    std::vector<bool> bandUsed(maxBands, false);
    std::vector<float> bandFreqs(maxBands, 0.0f);

    // Pre-scan existing bands
    for (int i = 0; i < activeBands; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        {
            bandFreqs[i] = freqParam->load();
            if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
            {
                // Consider band "used" if gain is significant
                bandUsed[i] = std::abs(gainParam->load()) > 0.5f;
            }
        }
    }

    // Find best band for this single correction
    int bestBand = -1;
    float bestScore = std::numeric_limits<float>::max();

    const float targetFreq = scaled.frequency;
    const float reuseThreshold = targetFreq * kBandReuseThresholdOctave;

    for (int i = 0; i < activeBands; ++i)
    {
        float dist = std::abs(bandFreqs[i] - targetFreq);
        float score = dist;

        // Prefer unused bands (much lower score)
        if (!bandUsed[i])
        {
            score *= kUnusedBandScoreMult;
        }

        // Prefer bands within reuse threshold (can merge)
        if (dist < reuseThreshold)
        {
            score *= kReuseThresholdScoreMult;
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestBand = i;
        }
    }

    bool anyMaterialChange = false;

    if (bestBand >= 0)
    {
        auto state = getBandState(bestBand);
        state.frequency = scaled.frequency;
        state.gain = scaled.suggestedGain;
        state.q = scaled.suggestedQ;
        state.type = static_cast<int>(aiFilterTypeToProcessorType(scaled.suggestedFilter));
        state.enabled = true;
        state.solo = false;

        anyMaterialChange = applyBandStateDelta(bestBand, state, false) || anyMaterialChange;

        // Record this to user learning system for better future suggestions
        // Only record if learning is enabled (privacy control)
        bool learningEnabled = apvts.getRawParameterValue("learningEnabled")->load() > 0.5f;
        if (learningEnabled)
        {
            userLearning.recordAISuggestionAccepted(
                AIEngine::getProblemTypeName(correction.type),
                scaled.frequency,
                scaled.suggestedGain,
                scaled.suggestedQ
            );
        }
    }

    // Trigger dry→wet crossfade only when the AI correction actually changed state.
    if (anyMaterialChange)
        aiCorrectionCrossfadePending.store(true, std::memory_order_release);

    // Remove this correction from pending (user has applied it)
    // Find and remove the matching correction from pendingCorrections
    auto pending = aiEngine.getPendingCorrections();
    for (int i = 0; i < static_cast<int>(pending.size()); ++i)
    {
        const auto& p = pending[i];
        const float freqRatio = std::abs(std::log2(correction.frequency / juce::jmax(20.0f, p.frequency)));
        const bool freqMatch = freqRatio < 0.01f;  // Within 1%
        const bool typeMatch = (p.type == correction.type);
        const bool gainMatch = std::abs(p.suggestedGain - correction.suggestedGain) < 0.5f;  // Within 0.5dB

        if (freqMatch && typeMatch && gainMatch)
        {
            aiEngine.rejectCorrection(i);  // Remove from pending
            break;
        }
    }
}

void AIEqualizerAudioProcessor::applySemanticAdjustments(const std::vector<SemanticEQEngine::SemanticEQAdjustment>& adjustments)
{
    // Must run on message thread for APVTS safety
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
        juce::MessageManager::callAsync([weakThis, adjustments]()
        {
            if (auto* self = weakThis.get())
                self->applySemanticAdjustments(adjustments);
        });
        return;
    }

    if (adjustments.empty())
        return;

    std::array<bool, maxBands> bandClaimed {};
    bandClaimed.fill(false);

    // Rebuild ownership map from current assignments (skip invalid ones)
    for (auto& owner : semanticBandAssignments)
    {
        if (owner < 0 || owner >= maxBands)
            owner = -1;
        else
            bandClaimed[static_cast<size_t>(owner)] = true;
    }

    auto claimSlotForQuality = [&](SemanticEQEngine::SemanticQuality quality) -> int
    {
        const int qIdx = static_cast<int>(quality);
        if (qIdx < 0 || qIdx >= SemanticEQEngine::numQualities)
            return -1;

        // Reuse existing assignment if still valid
        const int existing = semanticBandAssignments[static_cast<size_t>(qIdx)];
        if (existing >= 0 && existing < maxBands)
            return existing;

        int chosen = -1;
        float bestScore = std::numeric_limits<float>::max();

        // Prefer disabled/unused bands starting from the top to avoid touching user bands
        for (int i = maxBands - 1; i >= 0; --i)
        {
            if (bandClaimed[static_cast<size_t>(i)])
                continue;

            auto state = getBandState(i);
            const bool effectivelyUnused = (!state.enabled || std::abs(state.gain) < 0.35f) && !state.solo;
            if (effectivelyUnused)
            {
                chosen = i;
                break;
            }

            // Otherwise, pick the least intrusive slot (smallest |gain|)
            const float score = std::abs(state.gain);
            if (!state.solo && score < bestScore)
            {
                bestScore = score;
                chosen = i;
            }
        }

        if (chosen >= 0)
        {
            semanticBandAssignments[static_cast<size_t>(qIdx)] = chosen;
            bandClaimed[static_cast<size_t>(chosen)] = true;
        }

        return chosen;
    };

    int desiredActiveBands = getNumActiveBands();
    bool anyBandStateChanged = false;

    for (const auto& adj : adjustments)
    {
        const int slot = claimSlotForQuality(adj.sourceQuality);
        if (slot < 0)
            continue;

        desiredActiveBands = std::max(desiredActiveBands, slot + 1);

        auto state = getBandState(slot);
        const BandState previousState = state;
        state.frequency = adj.frequency;
        state.gain = adj.gain;
        state.q = adj.q;
        state.type = adj.filterType;
        state.enabled = adj.enabled;
        state.solo = false; // semantic moves should never toggle solo

        const bool materiallyChanged =
            std::abs(previousState.frequency - state.frequency) > 1.0f ||
            std::abs(previousState.gain - state.gain) > 0.05f ||
            std::abs(previousState.q - state.q) > 0.02f ||
            previousState.type != state.type ||
            previousState.enabled != state.enabled ||
            previousState.solo != state.solo;

        if (materiallyChanged)
        {
            setBandState(slot, state);
            anyBandStateChanged = true;
        }
    }

    const bool needsActiveBandCountUpdate = desiredActiveBands > getNumActiveBands();
    if (!anyBandStateChanged && !needsActiveBandCountUpdate)
        return;

    // Ensure the active band count covers any newly claimed slots
    if (needsActiveBandCountUpdate)
    {
        if (auto* param = apvts.getParameter("numActiveBands"))
        {
            const int clamped = juce::jlimit(1, maxBands, desiredActiveBands);
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(clamped - 1)));
            param->endChangeGesture();
        }
    }
}

//==============================================================================
AIEqualizerAudioProcessor::BandState AIEqualizerAudioProcessor::getBandState(int bandIndex) const
{
    BandState state;

    // SAFETY: Bounds check
    if (bandIndex < 0 || bandIndex >= maxBands)
        return state;

    juce::String prefix = "band" + juce::String(bandIndex);

    // SAFETY: Check for null pointers before dereferencing
    if (auto* freqParam = apvts.getRawParameterValue(prefix + "Freq"))
        state.frequency = freqParam->load();
    else
        state.frequency = 1000.0f; // Default

    if (auto* gainParam = apvts.getRawParameterValue(prefix + "Gain"))
        state.gain = gainParam->load();
    else
        state.gain = 0.0f; // Default

    if (auto* qParam = apvts.getRawParameterValue(prefix + "Q"))
        state.q = qParam->load();
    else
        state.q = 1.0f; // Default

    if (auto* enabledParam = apvts.getRawParameterValue(prefix + "Enabled"))
        state.enabled = enabledParam->load() > 0.5f;
    else
        state.enabled = (bandIndex < 8); // Default: first 8 bands enabled

    if (auto* typeParam = apvts.getRawParameterValue(prefix + "Type"))
    {
        state.type = static_cast<int>(typeParam->load());
    }
    else
    {
        // Legacy fallback
        if (bandIndex == 0)
            state.type = 1; // Low Shelf
        else if (bandIndex == maxBands - 1)
            state.type = 3; // High Shelf
        else
            state.type = 2; // Peak
    }

    if (auto* soloParam = apvts.getRawParameterValue(prefix + "Solo"))
        state.solo = soloParam->load() > 0.5f;
    else
        state.solo = false;

    if (auto* slopeParam = apvts.getRawParameterValue(prefix + "Slope"))
        state.slope = static_cast<int>(std::round(slopeParam->load()));
    else
        state.slope = 0;

    if (auto* dynModeParam = apvts.getRawParameterValue(prefix + "DynMode"))
        state.dynMode = static_cast<int>(std::round(dynModeParam->load()));
    else
        state.dynMode = 0;

    if (auto* dynThresholdParam = apvts.getRawParameterValue(prefix + "Threshold"))
        state.dynThreshold = dynThresholdParam->load();
    else
        state.dynThreshold = -24.0f;

    if (auto* dynRatioParam = apvts.getRawParameterValue(prefix + "Ratio"))
        state.dynRatio = dynRatioParam->load();
    else
        state.dynRatio = 2.0f;

    if (auto* dynAttackParam = apvts.getRawParameterValue(prefix + "Attack"))
        state.dynAttack = dynAttackParam->load();
    else
        state.dynAttack = 10.0f;

    if (auto* dynReleaseParam = apvts.getRawParameterValue(prefix + "Release"))
        state.dynRelease = dynReleaseParam->load();
    else
        state.dynRelease = 100.0f;

    if (auto* dynRangeParam = apvts.getRawParameterValue(prefix + "Range"))
        state.dynRange = dynRangeParam->load();
    else
        state.dynRange = 24.0f;

    if (auto* dynKneeParam = apvts.getRawParameterValue(prefix + "Knee"))
        state.dynKnee = dynKneeParam->load();
    else
        state.dynKnee = 6.0f;

    return state;
}

bool AIEqualizerAudioProcessor::applyBandStateDelta(int bandIndex, const BandState& targetState, bool useGestures)
{
    // CRITICAL: Must be called from Message Thread for APVTS access
    auto* mm = juce::MessageManager::getInstance();
    if (mm == nullptr || !mm->isThisTheMessageThread())
    {
        jassertfalse;
        return false;
    }

    if (bandIndex < 0 || bandIndex >= maxBands)
        return false;

    const BandState currentState = getBandState(bandIndex);
    BandState clampedState = targetState;
    clampedState.frequency = juce::jlimit(20.0f, 20000.0f, clampedState.frequency);
    clampedState.gain = juce::jlimit(-24.0f, 24.0f, clampedState.gain);
    clampedState.q = juce::jlimit(0.1f, 10.0f, clampedState.q);
    clampedState.type = juce::jlimit(0, 8, clampedState.type);
    clampedState.slope = juce::jlimit(0, 2, clampedState.slope);
    clampedState.dynMode = juce::jlimit(0, 3, clampedState.dynMode);
    clampedState.dynThreshold = juce::jlimit(-60.0f, 0.0f, clampedState.dynThreshold);
    clampedState.dynRatio = juce::jlimit(1.0f, 20.0f, clampedState.dynRatio);
    clampedState.dynAttack = juce::jlimit(0.1f, 500.0f, clampedState.dynAttack);
    clampedState.dynRelease = juce::jlimit(1.0f, 2000.0f, clampedState.dynRelease);
    clampedState.dynRange = juce::jlimit(0.0f, 48.0f, clampedState.dynRange);
    clampedState.dynKnee = juce::jlimit(0.0f, 24.0f, clampedState.dynKnee);

    const bool freqChanged = std::abs(currentState.frequency - clampedState.frequency) > 1.0f;
    const bool gainChanged = std::abs(currentState.gain - clampedState.gain) > 0.05f;
    const bool qChanged = std::abs(currentState.q - clampedState.q) > 0.02f;
    const bool typeChanged = currentState.type != clampedState.type;
    const bool enabledChanged = currentState.enabled != clampedState.enabled;
    const bool soloChanged = currentState.solo != clampedState.solo;
    const bool slopeChanged = currentState.slope != clampedState.slope;
    const bool dynModeChanged = currentState.dynMode != clampedState.dynMode;
    const bool dynThresholdChanged = std::abs(currentState.dynThreshold - clampedState.dynThreshold) > 0.05f;
    const bool dynRatioChanged = std::abs(currentState.dynRatio - clampedState.dynRatio) > 0.02f;
    const bool dynAttackChanged = std::abs(currentState.dynAttack - clampedState.dynAttack) > 0.1f;
    const bool dynReleaseChanged = std::abs(currentState.dynRelease - clampedState.dynRelease) > 0.5f;
    const bool dynRangeChanged = std::abs(currentState.dynRange - clampedState.dynRange) > 0.05f;
    const bool dynKneeChanged = std::abs(currentState.dynKnee - clampedState.dynKnee) > 0.05f;

    const bool anyChanged = freqChanged || gainChanged || qChanged || typeChanged || enabledChanged || soloChanged ||
                            slopeChanged || dynModeChanged || dynThresholdChanged || dynRatioChanged ||
                            dynAttackChanged || dynReleaseChanged || dynRangeChanged || dynKneeChanged;
    if (!anyChanged)
        return false;

    juce::String prefix = "band" + juce::String(bandIndex);

    auto applyParam = [useGestures](juce::RangedAudioParameter* param, float normalized)
    {
        if (param == nullptr)
            return;
        if (useGestures) param->beginChangeGesture();
        param->setValueNotifyingHost(normalized);
        if (useGestures) param->endChangeGesture();
    };

    if (freqChanged)
        applyParam(apvts.getParameter(prefix + "Freq"), apvts.getParameter(prefix + "Freq")->convertTo0to1(clampedState.frequency));
    if (gainChanged)
        applyParam(apvts.getParameter(prefix + "Gain"), apvts.getParameter(prefix + "Gain")->convertTo0to1(clampedState.gain));
    if (qChanged)
        applyParam(apvts.getParameter(prefix + "Q"), apvts.getParameter(prefix + "Q")->convertTo0to1(clampedState.q));
    if (typeChanged)
        applyParam(apvts.getParameter(prefix + "Type"), apvts.getParameter(prefix + "Type")->convertTo0to1(static_cast<float>(clampedState.type)));
    if (enabledChanged)
        applyParam(apvts.getParameter(prefix + "Enabled"), clampedState.enabled ? 1.0f : 0.0f);
    if (soloChanged)
        applyParam(apvts.getParameter(prefix + "Solo"), clampedState.solo ? 1.0f : 0.0f);
    if (slopeChanged)
        applyParam(apvts.getParameter(prefix + "Slope"), apvts.getParameter(prefix + "Slope")->convertTo0to1(static_cast<float>(clampedState.slope)));
    if (dynModeChanged)
        applyParam(apvts.getParameter(prefix + "DynMode"), apvts.getParameter(prefix + "DynMode")->convertTo0to1(static_cast<float>(clampedState.dynMode)));
    if (dynThresholdChanged)
        applyParam(apvts.getParameter(prefix + "Threshold"), apvts.getParameter(prefix + "Threshold")->convertTo0to1(clampedState.dynThreshold));
    if (dynRatioChanged)
        applyParam(apvts.getParameter(prefix + "Ratio"), apvts.getParameter(prefix + "Ratio")->convertTo0to1(clampedState.dynRatio));
    if (dynAttackChanged)
        applyParam(apvts.getParameter(prefix + "Attack"), apvts.getParameter(prefix + "Attack")->convertTo0to1(clampedState.dynAttack));
    if (dynReleaseChanged)
        applyParam(apvts.getParameter(prefix + "Release"), apvts.getParameter(prefix + "Release")->convertTo0to1(clampedState.dynRelease));
    if (dynRangeChanged)
        applyParam(apvts.getParameter(prefix + "Range"), apvts.getParameter(prefix + "Range")->convertTo0to1(clampedState.dynRange));
    if (dynKneeChanged)
        applyParam(apvts.getParameter(prefix + "Knee"), apvts.getParameter(prefix + "Knee")->convertTo0to1(clampedState.dynKnee));

    markParametersChanged();
    return true;
}

void AIEqualizerAudioProcessor::setBandState(int bandIndex, const BandState& state)
{
    juce::ignoreUnused(applyBandStateDelta(bandIndex, state, true));
}

//==============================================================================
const juce::String AIEqualizerAudioProcessor::getName() const { return JucePlugin_Name; }
bool AIEqualizerAudioProcessor::acceptsMidi() const { return false; }
bool AIEqualizerAudioProcessor::producesMidi() const { return false; }
bool AIEqualizerAudioProcessor::isMidiEffect() const { return false; }
double AIEqualizerAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AIEqualizerAudioProcessor::getNumPrograms() { return 1; }
int AIEqualizerAudioProcessor::getCurrentProgram() { return 0; }
void AIEqualizerAudioProcessor::setCurrentProgram(int) {}
const juce::String AIEqualizerAudioProcessor::getProgramName(int) { return {}; }
void AIEqualizerAudioProcessor::changeProgramName(int, const juce::String&) {}
bool AIEqualizerAudioProcessor::hasEditor() const { return true; }

//==============================================================================
void AIEqualizerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Snapshot the active slot directly from APVTS atomics — thread-safe regardless of caller thread.
    // Some DAWs (Pro Tools, some VST3 hosts) call getStateInformation off the message thread,
    // which causes saveCurrentStateToSlot() to bail out silently. Since getBandState() and
    // getRawParameterValue()->load() only read atomics, they are safe from any thread.
    {
        const ABState activeSlot = currentABState.load(std::memory_order_relaxed);
        EQSlot* slot = nullptr;
        switch (activeSlot)
        {
            case ABState::A: slot = &slotA; break;
            case ABState::B: slot = &slotB; break;
            case ABState::C: slot = &slotC; break;
            case ABState::D: slot = &slotD; break;
        }
        if (slot != nullptr)
        {
            for (int i = 0; i < maxBands; ++i)
                slot->bands[static_cast<size_t>(i)] = getBandState(i);
            if (auto* p = apvts.getRawParameterValue("outputGain"))
                slot->outputGain = p->load();
        }
    }

    auto state = apvts.copyState();

    // Add A/B/C/D slot data to state
    state.setProperty("abState", static_cast<int>(currentABState.load(std::memory_order_relaxed)), nullptr);
    // Save slot names
    state.setProperty("slotAName", slotA.name, nullptr);
    state.setProperty("slotBName", slotB.name, nullptr);
    state.setProperty("slotCName", slotC.name, nullptr);
    state.setProperty("slotDName", slotD.name, nullptr);

    // Persist full slot contents (bands + output gain)
    auto addSlotTree = [this, &state](const EQSlot& slot, const juce::Identifier& id)
    {
        state.removeChild(state.getChildWithName(id), nullptr);

        juce::ValueTree slotTree(id);
        slotTree.setProperty("name", slot.name, nullptr);
        slotTree.setProperty("outputGain", slot.outputGain, nullptr);
        slotTree.setProperty("dynEqEnabled", slot.dynEqEnabled, nullptr);
        slotTree.setProperty("dynEqMix", slot.dynEqMix, nullptr);
        slotTree.setProperty("dynAutoMakeup", slot.dynAutoMakeup, nullptr);

        for (int i = 0; i < maxBands; ++i)
        {
            const auto& band = slot.bands[static_cast<size_t>(i)];
            juce::ValueTree bandTree("band" + juce::String(i));
            bandTree.setProperty("freq", band.frequency, nullptr);
            bandTree.setProperty("gain", band.gain, nullptr);
            bandTree.setProperty("q", band.q, nullptr);
            bandTree.setProperty("type", band.type, nullptr);
            bandTree.setProperty("enabled", band.enabled, nullptr);
            bandTree.setProperty("solo", band.solo, nullptr);
            bandTree.setProperty("slope", band.slope, nullptr);
            bandTree.setProperty("dynMode", band.dynMode, nullptr);
            bandTree.setProperty("dynThreshold", band.dynThreshold, nullptr);
            bandTree.setProperty("dynRatio", band.dynRatio, nullptr);
            bandTree.setProperty("dynAttack", band.dynAttack, nullptr);
            bandTree.setProperty("dynRelease", band.dynRelease, nullptr);
            bandTree.setProperty("dynRange", band.dynRange, nullptr);
            bandTree.setProperty("dynKnee", band.dynKnee, nullptr);
            slotTree.addChild(bandTree, -1, nullptr);
        }

        state.addChild(std::move(slotTree), -1, nullptr);
    };

    addSlotTree(slotA, "SlotA");
    addSlotTree(slotB, "SlotB");
    addSlotTree(slotC, "SlotC");
    addSlotTree(slotD, "SlotD");

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AIEqualizerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

            // Restore A/B/C/D state
            auto loadedState = apvts.state;
            if (loadedState.hasProperty("abState"))
            {
                int abIdx = static_cast<int>(loadedState.getProperty("abState"));
                currentABState.store(static_cast<ABState>(juce::jlimit(0, 3, abIdx)), std::memory_order_relaxed);
            }
            // Restore slot names
            if (loadedState.hasProperty("slotAName"))
                slotA.name = loadedState.getProperty("slotAName").toString();
            if (loadedState.hasProperty("slotBName"))
                slotB.name = loadedState.getProperty("slotBName").toString();
            if (loadedState.hasProperty("slotCName"))
                slotC.name = loadedState.getProperty("slotCName").toString();
            if (loadedState.hasProperty("slotDName"))
                slotD.name = loadedState.getProperty("slotDName").toString();

            // Restore full slot contents (bands + output gain)
            auto restoreSlot = [this, &loadedState](const juce::Identifier& id, EQSlot& slot)
            {
                auto slotTree = loadedState.getChildWithName(id);
                if (!slotTree.isValid())
                    return;

                if (slotTree.hasProperty("name"))
                    slot.name = slotTree.getProperty("name").toString();
                if (slotTree.hasProperty("outputGain"))
                    slot.outputGain = static_cast<float>(slotTree.getProperty("outputGain"));
                if (slotTree.hasProperty("dynEqEnabled"))
                    slot.dynEqEnabled = static_cast<bool>(slotTree.getProperty("dynEqEnabled"));
                if (slotTree.hasProperty("dynEqMix"))
                    slot.dynEqMix = static_cast<float>(slotTree.getProperty("dynEqMix"));
                if (slotTree.hasProperty("dynAutoMakeup"))
                    slot.dynAutoMakeup = static_cast<bool>(slotTree.getProperty("dynAutoMakeup"));

                for (int i = 0; i < maxBands; ++i)
                {
                    auto bandTree = slotTree.getChildWithName("band" + juce::String(i));
                    if (!bandTree.isValid())
                        continue;

                    auto& band = slot.bands[static_cast<size_t>(i)];
                    if (bandTree.hasProperty("freq"))
                        band.frequency = static_cast<float>(bandTree.getProperty("freq"));
                    if (bandTree.hasProperty("gain"))
                        band.gain = static_cast<float>(bandTree.getProperty("gain"));
                    if (bandTree.hasProperty("q"))
                        band.q = static_cast<float>(bandTree.getProperty("q"));
                    if (bandTree.hasProperty("type"))
                        band.type = static_cast<int>(bandTree.getProperty("type"));
                    if (bandTree.hasProperty("enabled"))
                        band.enabled = static_cast<bool>(bandTree.getProperty("enabled"));
                    if (bandTree.hasProperty("solo"))
                        band.solo = static_cast<bool>(bandTree.getProperty("solo"));
                    if (bandTree.hasProperty("slope"))
                        band.slope = static_cast<int>(bandTree.getProperty("slope"));
                    if (bandTree.hasProperty("dynMode"))
                        band.dynMode = static_cast<int>(bandTree.getProperty("dynMode"));
                    if (bandTree.hasProperty("dynThreshold"))
                        band.dynThreshold = static_cast<float>(bandTree.getProperty("dynThreshold"));
                    if (bandTree.hasProperty("dynRatio"))
                        band.dynRatio = static_cast<float>(bandTree.getProperty("dynRatio"));
                    if (bandTree.hasProperty("dynAttack"))
                        band.dynAttack = static_cast<float>(bandTree.getProperty("dynAttack"));
                    if (bandTree.hasProperty("dynRelease"))
                        band.dynRelease = static_cast<float>(bandTree.getProperty("dynRelease"));
                    if (bandTree.hasProperty("dynRange"))
                        band.dynRange = static_cast<float>(bandTree.getProperty("dynRange"));
                    if (bandTree.hasProperty("dynKnee"))
                        band.dynKnee = static_cast<float>(bandTree.getProperty("dynKnee"));
                }
            };

            restoreSlot("SlotA", slotA);
            restoreSlot("SlotB", slotB);
            restoreSlot("SlotC", slotC);
            restoreSlot("SlotD", slotD);

            // Make sure the active slot struct reflects the APVTS state (for legacy presets).
            // setStateInformation can be called from any thread (Pro Tools, some VST3 hosts call
            // it off the message thread). Use callAsync to always run on the message thread.
            // Also trigger an IR rebuild if the loaded state has Linear Phase active - without
            // this the LP processor starts silent until the user touches a parameter.
            {
                const int slotIdx = static_cast<int>(currentABState.load(std::memory_order_relaxed));
                const bool needsIRRebuild = (currentPhaseMode.load(std::memory_order_relaxed) == PhaseMode::LinearPhase);

                // Use WeakReference to prevent use-after-free if processor is
                // destroyed before the async callback fires (matches pattern
                // used elsewhere in this file, e.g. lines 2391, 2717, 3076).
                juce::WeakReference<AIEqualizerAudioProcessor> weakThis(this);
                auto doRestore = [weakThis, slotIdx, needsIRRebuild]()
                {
                    auto* self = weakThis.get();
                    if (self == nullptr)
                        return;
                    // No processorReady guard here: slot sync must always happen after restore,
                    // regardless of whether the processor has finished initialising yet.
                    self->saveCurrentStateToSlot(static_cast<ABState>(slotIdx));

                    if (needsIRRebuild)
                    {
                        for (auto& loaded : self->linearIRLoaded)
                            loaded.store(false, std::memory_order_relaxed);
                        self->triggerLinearPhaseIRUpdate();
                    }
                };

                if (auto* mm = juce::MessageManager::getInstance())
                {
                    if (mm->isThisTheMessageThread())
                        doRestore();
                    else
                        juce::MessageManager::callAsync(doRestore);
                }
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* AIEqualizerAudioProcessor::createEditor()
{
    return new AIEqualizerAudioProcessorEditor(*this);
}

//==============================================================================
// Undo/Redo Implementation (now delegated to HistoryManager)
//==============================================================================

// Note: undo(), redo(), canUndo(), canRedo(), getUndoDescription(), getRedoDescription()
// are now inline in the header, delegating to historyManager.
// pushUndoState() is also inline in the header.

//==============================================================================
// Mid/Side Encoding/Decoding Implementation
//==============================================================================

void AIEqualizerAudioProcessor::encodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // M/S Encoding in-place: write Mid/Side into the buffer (ch0=Mid, ch1=Side)
    if (buffer.getNumChannels() < 2)
        return;

    const int samples = juce::jmin(numSamples, msBuffer.getNumSamples(), buffer.getNumSamples());
    if (samples <= 0)
        return;

    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);
    float* midTmp = msBuffer.getWritePointer(0);
    float* sideTmp = msBuffer.getWritePointer(1);

    juce::FloatVectorOperations::add(midTmp, left, right, samples);
    juce::FloatVectorOperations::multiply(midTmp, kInvSqrt2, samples);

    juce::FloatVectorOperations::copy(sideTmp, left, samples);
    juce::FloatVectorOperations::subtract(sideTmp, right, samples);
    juce::FloatVectorOperations::multiply(sideTmp, kInvSqrt2, samples);

    buffer.copyFrom(0, 0, midTmp, samples);
    buffer.copyFrom(1, 0, sideTmp, samples);
}

void AIEqualizerAudioProcessor::decodeMidSide(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // M/S Decoding in-place: buffer has Mid/Side (ch0/ch1), convert to L/R
    if (buffer.getNumChannels() < 2)
        return;

    const int samples = juce::jmin(numSamples, msBuffer.getNumSamples(), buffer.getNumSamples());
    if (samples <= 0)
        return;

    const float* mid = buffer.getReadPointer(0);
    const float* side = buffer.getReadPointer(1);
    float* left = msBuffer.getWritePointer(0);
    float* right = msBuffer.getWritePointer(1);

    juce::FloatVectorOperations::add(left, mid, side, samples);
    juce::FloatVectorOperations::multiply(left, kInvSqrt2, samples);

    juce::FloatVectorOperations::copy(right, mid, samples);
    juce::FloatVectorOperations::subtract(right, side, samples);
    juce::FloatVectorOperations::multiply(right, kInvSqrt2, samples);

    buffer.copyFrom(0, 0, left, samples);
    buffer.copyFrom(1, 0, right, samples);
}

//==============================================================================
// AI Command Queue (Lock-Free Communication)
//==============================================================================

void AIEqualizerAudioProcessor::queueAICommand(const AIEQCore::AICommand& command) noexcept
{
    // Queue command for audio thread (lock-free SPSC queue)
    // If queue is full, command is dropped (prevents blocking)
    const bool queued = aiCommandQueue.tryPush(command);
    juce::ignoreUnused(queued);
}

void AIEqualizerAudioProcessor::processAICommands() noexcept
{
    // Process all pending AI commands (RT-SAFE, lock-free)
    // Called at the start of each processBlock

    AIEQCore::AICommand cmd;
    while (aiCommandQueue.tryPop(cmd))
    {
        switch (cmd.type)
        {
            case AIEQCore::AICommandType::ApplyCorrection:
            {
                // Apply correction to the specified band
                const int bandIdx = cmd.bandIndex;
                if (bandIdx >= 0 && bandIdx < maxBands)
                {
                    // Update EQ processor directly (RT-safe, no APVTS access)
                    if (bandIdx < eqProcessor.getNumBands())
                    {
                        eqProcessor.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                        eqProcessor.setBandEnabled(bandIdx, true);
                    }
                    if (bandIdx < eqProcessorHQ.getNumBands())
                    {
                        eqProcessorHQ.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                        eqProcessorHQ.setBandEnabled(bandIdx, true);
                    }

                    // Mark parameters changed for GUI update
                    markParametersChanged();
                }
                break;
            }

            case AIEQCore::AICommandType::ClearCorrections:
            {
                // Reset all bands to neutral (0 dB gain)
                for (int i = 0; i < eqProcessor.getNumBands(); ++i)
                {
                    eqProcessor.setBandParameters(i, eqProcessor.getBandFrequency(i), 0.0f,
                                                  eqProcessor.getBandQ(i), eqProcessor.getBandType(i));
                }
                markParametersChanged();
                break;
            }

            case AIEQCore::AICommandType::SetBandParameter:
            {
                // Single parameter update
                const int bandIdx = cmd.bandIndex;
                if (bandIdx >= 0 && bandIdx < eqProcessor.getNumBands())
                {
                    eqProcessor.setBandParameters(bandIdx, cmd.frequency, cmd.gain, cmd.q, cmd.filterType);
                }
                break;
            }

            case AIEQCore::AICommandType::TriggerAnalysis:
            {
                // Signal AI to run analysis on next frame
                // This is handled by the GUI timer, just mark dirty flag
                aiProblemsChanged.store(true, std::memory_order_release);
                break;
            }

            case AIEQCore::AICommandType::None:
            default:
                break;
        }
    }
}

//==============================================================================
// Parameter Snapshot Loading (for per-block caching)
//==============================================================================

void AIEqualizerAudioProcessor::loadParameterSnapshot(AIEQCore::ProcessBlockParameters& params) noexcept
{
    // Load all parameters once at block start to avoid repeated atomic loads in sample loops
    // This is a key optimization for professional audio plugin performance

    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    // FIX: Clamp to maxBands to prevent array out-of-bounds access in loops
    params.numActiveBands = std::min(numActiveBands.load(std::memory_order_relaxed), maxBands);
    params.outputGainDB = loadParam(cachedOutputGain, 0.0f);
    params.autoGainEnabled = autoGainEnabled.load(std::memory_order_relaxed);
    params.autoGainCompensationDB = autoGainCompensation.load(std::memory_order_relaxed);
    params.dynamicEQEnabled = loadParam(cachedDynEqEnabled, 1.0f) > 0.5f;
    params.phaseMode = static_cast<int>(currentPhaseMode.load(std::memory_order_relaxed));
    params.msMode = static_cast<int>(currentMSMode.load(std::memory_order_relaxed));
    params.oversamplingFactor = oversamplingFactor.load(std::memory_order_relaxed);

    // Load band parameters
    for (int i = 0; i < params.numActiveBands && i < AIEQCore::kMaxBands; ++i)
    {
        const auto& cached = cachedParams[static_cast<size_t>(i)];
        auto& band = params.bands[static_cast<size_t>(i)];

        band.frequency = juce::jlimit(10.0f, 24000.0f, loadParam(cached.freq, 1000.0f));
        band.gain = juce::jlimit(-48.0f, 48.0f, loadParam(cached.gain, 0.0f));
        band.q = juce::jlimit(0.05f, 36.0f, loadParam(cached.q, 1.0f));
        const int type = static_cast<int>(loadParam(cached.type, 2.0f));
        band.filterType = juce::jlimit(0, 6, type);
        band.enabled = loadParam(cached.enabled, 1.0f) > 0.5f;

        // Dynamic EQ (clamped for safety)
        band.dynamicMode = juce::jlimit(0, 2, static_cast<int>(loadParam(cached.dynMode, 0.0f)));
        band.threshold = juce::jlimit(-120.0f, 24.0f, loadParam(cached.dynThreshold, -20.0f));
        band.ratio = juce::jlimit(0.1f, 20.0f, loadParam(cached.dynRatio, 2.0f));
        band.attackMs = juce::jlimit(0.05f, 500.0f, loadParam(cached.dynAttack, 10.0f));
        band.releaseMs = juce::jlimit(1.0f, 4000.0f, loadParam(cached.dynRelease, 100.0f));
        band.range = juce::jlimit(0.0f, 48.0f, loadParam(cached.dynRange, 24.0f));
        band.knee = juce::jlimit(0.0f, 24.0f, loadParam(cached.dynKnee, 6.0f));
    }

    // Increment version for change detection
    params.version++;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    try
    {
        auto proc = std::make_unique<AIEqualizerAudioProcessor>();
        return proc.release();
    }
    catch (const std::exception& e)
    {
        AIEQ_LOG_ERROR("Failed to create plugin: " + juce::String(e.what()));
    }
    catch (...)
    {
        AIEQ_LOG_ERROR("Failed to create plugin: unknown error");
    }

    return nullptr;
}
