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
    }
    eqParameterIDs.push_back("numActiveBands");
    for (const auto& id : eqParameterIDs)
        apvts.addParameterListener(id, this);

    // Initial IR build
    triggerEQCurveUpdate();

    if (auto* phaseValue = apvts.getRawParameterValue("phaseMode"))
        parameterChanged("phaseMode", phaseValue->load());
    if (auto* msValue = apvts.getRawParameterValue("msMode"))
        parameterChanged("msMode", msValue->load());
    if (auto* osValue = apvts.getRawParameterValue("oversamplingFactor"))
        parameterChanged("oversamplingFactor", osValue->load());

    // Initialize A/B/C/D slots with defaults
    slotA.bands.resize(maxBands, BandState());
    slotB.bands.resize(maxBands, BandState());
    slotC.bands.resize(maxBands, BandState());
    slotD.bands.resize(maxBands, BandState());
    slotA.name = "A";
    slotB.name = "B";
    slotC.name = "C";
    slotD.name = "D";

    // Initialize preset manager
    presetManager = std::make_unique<PresetManager>(apvts);
    
    // Initialize history manager with APVTS reference
    historyManager.initialize(apvts);
    
    // Start IR builder thread (RAII with std::jthread)
    irBuilderThread = std::jthread([this](std::stop_token st) {
        irBuilderThreadFunc(st);
    });
}

//==============================================================================
// IR Builder Thread Function (runs in background)
//==============================================================================
void AIEqualizerAudioProcessor::irBuilderThreadFunc(std::stop_token st)
{
    juce::dsp::FFT fft(LinearPhaseProcessor::fftOrder);
    const size_t fftSize = LinearPhaseProcessor::fftSize;
    const size_t halfSize = fftSize / 2;
    std::vector<std::complex<float>> freqDomain(fftSize, { 0.0f, 0.0f });
    std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
    std::vector<float> irBuf(fftSize, 0.0f);
    std::vector<float> magDB(halfSize, -120.0f);

    while (!st.stop_requested())
    {
        irBuildEvent.wait(-1);
        
        // Check exit flag after wake
        if (st.stop_requested())
            break;

        if (!eqCurveNeedsUpdate.exchange(false))
            continue;

        const double sr = currentSampleRate.load(std::memory_order_relaxed);
        
        // SAFETY: Skip IR building if sample rate not yet initialized
        if (sr <= 0.0)
            continue;
            
        const bool dynEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;

        uint64_t versionStart = 0;
        uint64_t versionEnd = 0;

        do
        {
            versionStart = irCoeffVersion.load(std::memory_order_acquire);
            if (versionStart & 1u)
                continue; // writer in progress, retry

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
            const float magLin = juce::Decibels::decibelsToGain(magDB[k]);
            freqDomain[k] = { magLin, 0.0f };

            if (k > 0 && k < halfSize)
            {
                const size_t mirror = LinearPhaseProcessor::fftSize - k;
                freqDomain[mirror] = { magLin, 0.0f };
            }
        }

        fft.perform(freqDomain.data(), timeDomain.data(), true);

        // IFFT scaling: JUCE FFT doesn't auto-scale, so divide by N
        const float ifftScale = 1.0f / static_cast<float>(fftSize);
        for (size_t n = 0; n < LinearPhaseProcessor::fftSize; ++n)
            irBuf[n] = timeDomain[n].real() * ifftScale;

        // Center (circular shift to create zero-phase / linear-phase IR)
        std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(halfSize), irBuf.end());

        // Apply Hann window for smooth time-domain response
        // Note: Do NOT normalize to maxAbs=1.0 - this destroys EQ gain information!
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n)
                                                    / static_cast<float>(LinearPhaseProcessor::irSize - 1)));
            irBuf[n] *= w;
        }
        
        // Safety: clamp extreme values to prevent blowup, preserve gain levels
        for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
        {
            if (std::isnan(irBuf[n]) || std::isinf(irBuf[n]))
                irBuf[n] = 0.0f;
            else
                irBuf[n] = juce::jlimit(-10.0f, 10.0f, irBuf[n]);
        }

        // Load IR into back processor (double-buffered)
        const int backIdx = 1 - activeIRIndex.load(std::memory_order_relaxed);
        if (backIdx >= 0 && backIdx < static_cast<int>(linearPhaseProcessors.size()))
        {
            if (auto* back = linearPhaseProcessors[static_cast<size_t>(backIdx)].get())
            {
                back->loadImpulseResponse({ irBuf.begin(), irBuf.begin() + LinearPhaseProcessor::irSize }, sr);
                readyIRIndex.store(backIdx, std::memory_order_release);
            }
        }
    }
}

AIEqualizerAudioProcessor::~AIEqualizerAudioProcessor()
{
    // Remove parameter listeners
    apvts.removeParameterListener("phaseMode", this);
    apvts.removeParameterListener("msMode", this);
    apvts.removeParameterListener("oversamplingFactor", this);
    for (const auto& id : eqParameterIDs)
        apvts.removeParameterListener(id, this);

    // std::jthread automatically requests stop and joins on destruction (RAII)
    // Signal the event to wake the thread so it can check stop_token
    irBuildEvent.signal();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AIEqualizerAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Global controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    
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
        juce::StringArray{"Off", "2x", "4x"}, 0));
    
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
        juce::ParameterID{"showPostSpectrum", 1}, "Show Post-EQ Spectrum", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"showDeltaSpectrum", 1}, "Show Delta Spectrum", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerResolution", 1}, "Analyzer FFT",
        juce::StringArray{"Low (1024)", "Medium (2048)", "High (4096)", "Max (8192)"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"analyzerSpeed", 1}, "Analyzer Speed",
        juce::StringArray{"Fast", "Medium", "Slow"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"analyzerTilt", 1}, "Tilt Compensation", true));

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
    }

    cachedOutputGain = apvts.getRawParameterValue("outputGain");
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

    parametersCached.store(true);
}

//==============================================================================
void AIEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    cacheParameterPointers();

    // FIX 3: Use atomic store
    currentSampleRate.store(sampleRate);
    currentBlockSize = samplesPerBlock;
    preallocatedMaxSamples = juce::jmax(samplesPerBlock * 4, 32768);

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

    // Ensure EQ engines have all bands allocated upfront
    ensureBandCount(maxBands);

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
    
    // FIX: Prepare shadow processors for thread-safe IR building
    // These are read by the IR builder thread without locking
    eqProcessorForIR.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dynamicEQProcessorForIR.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    
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
    oversamplingFactor.store(osFactor);

    // Natural Phase path always uses at least 2x upsampling via legacy oversampler.
    // User oversampling selection: Off=0, 2x=1, 4x=2
    // Actual multiplier: Off(0)→2x, 2x(1)→2x, 4x(2)→4x
    // Match HQ processors to actual upsampled rate to prevent coefficient warping/distortion.
    const int osMultiplier = (osFactor == 2 ? 4 : 2); // osFactor: 0,1→2x | 2→4x
    const double hqSampleRate = sampleRate * static_cast<double>(osMultiplier);
    const int hqBlockSize = samplesPerBlock * osMultiplier;
    eqProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());
    dynamicEQProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());
    
    // Oversamplers (2x and 4x)
    if (osFactor >= 1)
    {
        oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(getTotalNumInputChannels()),
            1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        oversampler2x->reset();
        oversampler2x->initProcessing(static_cast<size_t>(samplesPerBlock));
    }
    if (osFactor >= 2)
    {
        oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
            static_cast<size_t>(getTotalNumInputChannels()),
            2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        oversampler4x->reset();
        oversampler4x->initProcessing(static_cast<size_t>(samplesPerBlock));
    }
    
    // Legacy naturalOversampler (2x) for backward compatibility
    naturalOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(getTotalNumInputChannels()),
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    naturalOversampler->reset();
    naturalOversampler->initProcessing(static_cast<size_t>(samplesPerBlock));
    naturalPhaseLatency = static_cast<int>(naturalOversampler->getLatencyInSamples());
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
    activeIRIndex.store(0);
    readyIRIndex.store(-1);
    
    // FIX: Pre-allocate crossfade buffer for smooth IR transitions
    crossfadeBuffer.setSize(getTotalNumInputChannels(), preallocatedMaxSamples);
    crossfadeBuffer.clear();
    crossfadeSamplesRemaining = 0;
    previousIRIndex = 0;
    
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
    
    // FIX: Initialize smoothed output gain (50ms ramp time to prevent zippering)
    smoothedOutputGain.reset(sampleRate, 0.05);
    smoothedOutputGain.setCurrentAndTargetValue(1.0f);

    // Pre-compute AI analysis cadence (~10 Hz)
    aiAnalysisIntervalSamples = juce::jmax(static_cast<int>(std::round(sampleRate * 0.1)), samplesPerBlock);
    aiAnalysisSamples = 0;
    
    // Set worst-case latency once to avoid host reconfiguration during automation
    const int linearPhaseLatency = static_cast<int>(LinearPhaseProcessor::irSize / 2);
    int oversamplingLatency = naturalPhaseLatency;
    if (oversampler4x)
        oversamplingLatency = std::max(oversamplingLatency, static_cast<int>(oversampler4x->getLatencyInSamples()));
    else if (oversampler2x)
        oversamplingLatency = std::max(oversamplingLatency, static_cast<int>(oversampler2x->getLatencyInSamples()));
    // Worst-case latency for potential 4x oversampling (even if currently off)
    {
        juce::dsp::Oversampling<float> tempOversampler(
            static_cast<size_t>(getTotalNumInputChannels()),
            2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);
        tempOversampler.reset();
        oversamplingLatency = std::max(oversamplingLatency, static_cast<int>(tempOversampler.getLatencyInSamples()));
    }
    worstCaseLatencySamples = std::max(linearPhaseLatency, oversamplingLatency);
    setLatencySamples(worstCaseLatencySamples);
    lastReportedLatency = worstCaseLatencySamples;
    
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
    
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int blockSamples = buffer.getNumSamples();
    
    auto loadParam = [](std::atomic<float>* ptr, float fallback) -> float
    {
        return ptr ? ptr->load(std::memory_order_relaxed) : fallback;
    };

    const bool bypassed = loadParam(cachedBypass, apvts.getRawParameterValue("bypass")->load()) > 0.5f;
    const int qualityModeParam = static_cast<int>(std::round(loadParam(cachedQualityMode, static_cast<float>(qualityModeCached))));
    const auto phaseModeSnapshot = currentPhaseMode.load(std::memory_order_relaxed);
    const auto msModeSnapshot = currentMSMode.load(std::memory_order_relaxed);
    const int analyzerResParam = static_cast<int>(std::round(loadParam(cachedAnalyzerResolution, static_cast<float>(analyzerResolutionCached))));
    const int analyzerSpdParam = static_cast<int>(std::round(loadParam(cachedAnalyzerSpeed, static_cast<float>(analyzerSpeedCached))));
    const bool autoGainEnabledLocal = loadParam(cachedAutoGain, 0.0f) > 0.5f;
    autoGainEnabled.store(autoGainEnabledLocal, std::memory_order_relaxed);
    const bool dynEqEnabledLocal = loadParam(cachedDynEqEnabled, 1.0f) > 0.5f;
    const bool aiEnabledLocal = loadParam(cachedAIEnabled, 1.0f) > 0.5f;
    const int sourceProfileIndex = static_cast<int>(std::round(loadParam(cachedSourceProfile, 0.0f)));
    const bool showPost = loadParam(cachedShowPostSpectrum, 0.0f) > 0.5f;
    float outputGainDB = loadParam(cachedOutputGain, 0.0f);
    
    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Capture audio for analysis using lock-free CaptureService (even if bypassed)
    // This replaces the old mutex-based pushToCaptureRing - fully RT-safe
    captureService.pushSamples(buffer);
    
    // Process any pending AI commands from the lock-free queue
    processAICommands();
    
    // Check bypass
    if (bypassed)
        return;

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
    
    // Update EQ parameters from APVTS
    updateEQFromParameters();
    
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
        
        // FORCE: Always call analyzeSpectrum, even if spectrum is empty
        // (detectProblems will create test problem if needed)
        if (!spectrum.empty())
        {
            aiEngine.analyzeSpectrum(spectrum);
            aiProblemsChanged.store(true, std::memory_order_release);
        }
        else
        {
            // Spectrum empty - create dummy spectrum to force detection
            std::vector<float> dummySpectrum(2049, -80.0f);  // 4096 FFT = 2049 bins
            aiEngine.analyzeSpectrum(dummySpectrum, true);  // Force analysis
            aiProblemsChanged.store(true, std::memory_order_release);
        }
    }
    else if (!aiEnabledLocal)
    {
        // AI disabled - but still create test problem to verify system
        aiEngine.setEnabled(false);
    }
    
    const auto mode = phaseModeSnapshot;

    // Skip zero-latency/Natural processing when in Linear Phase to avoid double-processing
    if (mode != PhaseMode::LinearPhase)
    {
        // Handle Mid/Side processing
    const auto msMode = msModeSnapshot;
    bool needsMSProcessing = (msMode != MSMode::Stereo) && (totalNumInputChannels >= 2);
    
    if (needsMSProcessing)
    {
        // Encode to M/S manually
        encodeMidSide(buffer);
        
        // FIX 5: Use pre-allocated buffers (ensure they're large enough)
        const int numSamples = buffer.getNumSamples();
        jassert(midProcessBuffer.getNumSamples() >= numSamples);
        jassert(sideProcessBuffer.getNumSamples() >= numSamples);
        
        // Use pre-allocated buffers (reference only the needed samples)
        auto midBuffer = juce::AudioBuffer<float>(midProcessBuffer.getArrayOfWritePointers(), 1, numSamples);
        auto sideBuffer = juce::AudioBuffer<float>(sideProcessBuffer.getArrayOfWritePointers(), 1, numSamples);
        
        midBuffer.copyFrom(0, 0, msBuffer, 0, 0, numSamples);
        if (msBuffer.getNumChannels() > 1)
            sideBuffer.copyFrom(0, 0, msBuffer, 1, 0, numSamples);
        
        // Process Mid and Side separately if linked, or process only selected channel
        if (msMode == MSMode::Mid || msMode == MSMode::MSLinked)
        {
            eqProcessorMid.process(midBuffer);
            if (dynEqEnabledLocal)
                dynamicEQProcessorMid.process(midBuffer);
        }
        
        if (msMode == MSMode::Side || msMode == MSMode::MSLinked)
        {
            eqProcessorSide.process(sideBuffer);
            if (dynEqEnabledLocal)
                dynamicEQProcessorSide.process(sideBuffer);
        }
        
        // Copy processed channels back to M/S buffer
        msBuffer.copyFrom(0, 0, midBuffer, 0, 0, buffer.getNumSamples());
        if (msMode == MSMode::Side || msMode == MSMode::MSLinked)
        {
            if (msBuffer.getNumChannels() > 1)
                msBuffer.copyFrom(1, 0, sideBuffer, 0, 0, buffer.getNumSamples());
        }
        
        // Decode back to L/R
        decodeMidSide(buffer);
    }
    
    // FIX 8: Latency calculation moved to updateReportedLatency() method
    // (called from prepareToPlay() and parameterChanged() instead)
    
    // FIX: Removed unreachable LinearPhase check (we're inside mode != LinearPhase block)
    
    if (mode == PhaseMode::NaturalPhase)
    {
        // Skip EQ processing if M/S was already processed
        if (!needsMSProcessing)
        {
            int osFactor = oversamplingFactor.load();
            juce::dsp::Oversampling<float>* activeOversampler = nullptr;
            
            if (osFactor == 1 && oversampler2x)
                activeOversampler = oversampler2x.get();
            else if (osFactor == 2 && oversampler4x)
                activeOversampler = oversampler4x.get();
            
            if (activeOversampler)
            {
                juce::dsp::AudioBlock<float> block(buffer);
                auto upBlock = activeOversampler->processSamplesUp(block);

                // Copy upsampled block into AudioBuffer view
                const auto upChannels = static_cast<int>(upBlock.getNumChannels());
                const auto upSamples = static_cast<int>(upBlock.getNumSamples());
                jassert(naturalOversampledBuffer.getNumChannels() >= upChannels);
                jassert(naturalOversampledBuffer.getNumSamples() >= upSamples);
                for (int ch = 0; ch < upChannels; ++ch)
                    std::memcpy(naturalOversampledBuffer.getWritePointer(ch),
                                upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                static_cast<size_t>(upSamples) * sizeof(float));

                // FIX: Create a view with ONLY the valid upSamples (not the full pre-allocated buffer)
                // This prevents processing garbage data beyond the valid sample range
                juce::AudioBuffer<float> hqProcessBuffer(
                    naturalOversampledBuffer.getArrayOfWritePointers(),
                    upChannels,
                    upSamples);

                eqProcessorHQ.process(hqProcessBuffer);

                if (dynEqEnabledLocal)
                {
                    dynamicEQProcessorHQ.process(hqProcessBuffer);
                }

                // Copy processed data back into upBlock
                for (int ch = 0; ch < upChannels; ++ch)
                    std::memcpy(upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                naturalOversampledBuffer.getReadPointer(ch),
                                static_cast<size_t>(upSamples) * sizeof(float));

                activeOversampler->processSamplesDown(block);
            }
            else
            {
                // Fallback: standard path with legacy oversampler
                if (naturalOversampler)
                {
                    juce::dsp::AudioBlock<float> block(buffer);
                    auto upBlock = naturalOversampler->processSamplesUp(block);

                    const auto upChannels = static_cast<int>(upBlock.getNumChannels());
                    const auto upSamples = static_cast<int>(upBlock.getNumSamples());
                    jassert(naturalOversampledBuffer.getNumChannels() >= upChannels);
                    jassert(naturalOversampledBuffer.getNumSamples() >= upSamples);
                    for (int ch = 0; ch < upChannels; ++ch)
                        std::memcpy(naturalOversampledBuffer.getWritePointer(ch),
                                    upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                    static_cast<size_t>(upSamples) * sizeof(float));

                    // FIX: Create a view with ONLY the valid upSamples
                    juce::AudioBuffer<float> hqProcessBuffer(
                        naturalOversampledBuffer.getArrayOfWritePointers(),
                        upChannels,
                        upSamples);

                    eqProcessorHQ.process(hqProcessBuffer);

                    if (dynEqEnabledLocal)
                    {
                        dynamicEQProcessorHQ.process(hqProcessBuffer);
                    }

                    for (int ch = 0; ch < upChannels; ++ch)
                        std::memcpy(upBlock.getChannelPointer(static_cast<size_t>(ch)),
                                    naturalOversampledBuffer.getReadPointer(ch),
                                    static_cast<size_t>(upSamples) * sizeof(float));

                    naturalOversampler->processSamplesDown(block);
                }
                else
                {
                    // No oversampling
                    eqProcessor.process(buffer);
                    if (dynEqEnabledLocal)
                        dynamicEQProcessor.process(buffer);
                }
            }
        }
    }
    else
    {
        // Zero Latency path - skip if M/S was already processed
        if (!needsMSProcessing)
        {
            eqProcessor.process(buffer);
            
            if (dynEqEnabledLocal)
            {
                dynamicEQProcessor.process(buffer);
            }
        }
    }
    } // end if mode != LinearPhase
    
    // Linear Phase processing (applied only here, zero-latency path skipped above)
    if (mode == PhaseMode::LinearPhase)
    {
        updateLinearPhaseIRIfNeeded(); // swap in pre-built IR if ready
        
        const int currentIR = activeIRIndex.load();
        const int prevIR = previousIRIndex;
        const bool hasPrev = (prevIR != currentIR) && prevIR >= 0 && prevIR < static_cast<int>(linearPhaseProcessors.size());
        const bool doCrossfade = hasPrev && crossfadeSamplesRemaining > 0 && crossfadeBuffer.getNumSamples() >= blockSamples;

        if (doCrossfade)
        {
            // Copy input for new IR path
            crossfadeBuffer.makeCopyOf(buffer, true);

            if (auto* lpPrev = linearPhaseProcessors[static_cast<size_t>(prevIR)].get())
            {
                juce::dsp::AudioBlock<float> blockPrev(buffer);
                juce::dsp::ProcessContextReplacing<float> ctxPrev(blockPrev);
                lpPrev->process(ctxPrev);
            }

            if (auto* lpNew = linearPhaseProcessors[static_cast<size_t>(currentIR)].get())
            {
                juce::dsp::AudioBlock<float> blockNew(crossfadeBuffer);
                juce::dsp::ProcessContextReplacing<float> ctxNew(blockNew);
                lpNew->process(ctxNew);
            }

            const float fadeOutStart = static_cast<float>(crossfadeSamplesRemaining) / static_cast<float>(irCrossfadeSamples);
            const int remainingAfterBlock = std::max(0, crossfadeSamplesRemaining - blockSamples);
            const float fadeOutEnd = static_cast<float>(remainingAfterBlock) / static_cast<float>(irCrossfadeSamples);
            const float fadeInStart = 1.0f - fadeOutStart;
            const float fadeInEnd = 1.0f - fadeOutEnd;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.applyGainRamp(ch, 0, blockSamples, fadeOutStart, fadeOutEnd);
                crossfadeBuffer.applyGainRamp(ch, 0, blockSamples, fadeInStart, fadeInEnd);
                buffer.addFrom(ch, 0, crossfadeBuffer, ch, 0, blockSamples);
            }

            crossfadeSamplesRemaining = remainingAfterBlock;
        }
        else
        {
            if (auto* lp = linearPhaseProcessors[static_cast<size_t>(currentIR)].get())
            {
                juce::dsp::AudioBlock<float> block(buffer);
                juce::dsp::ProcessContextReplacing<float> ctx(block);
                lp->process(ctx);
            }
            crossfadeSamplesRemaining = 0;
        }
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
}

void AIEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    markParametersChanged();

    if (parameterID == "phaseMode")
    {
        const int modeIdx = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        const auto newMode = static_cast<PhaseMode>(modeIdx);
        const auto oldMode = currentPhaseMode.exchange(newMode);
        
        // FIX: Reset oversamplers when switching modes to clear stale internal state
        // This prevents clicks/glitches when switching from Linear Phase to Natural Phase
        if (newMode != oldMode)
        {
            // Reset all oversamplers to clear internal delay line state
            if (oversampler2x) oversampler2x->reset();
            if (oversampler4x) oversampler4x->reset();
            if (naturalOversampler) naturalOversampler->reset();
            
            // Reset Linear Phase convolvers too
            for (auto& lp : linearPhaseProcessors)
            {
                if (lp) lp->reset();
            }
        }
        
        // FIX 8: Update latency when phase mode changes
        updateReportedLatency();
        return;
    }
    
    if (parameterID == "msMode")
    {
        const int modeIdx = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        currentMSMode.store(static_cast<MSMode>(modeIdx));
        return;
    }
    
    if (parameterID == "oversamplingFactor")
    {
        const int factor = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        oversamplingFactor.store(factor);
        // FIX 8: Update latency when oversampling changes
        updateReportedLatency();
        // Re-prepare if needed (will happen on next processBlock or prepareToPlay)
        return;
    }

    // Mark IR dirty for any band-related parameter
    if (parameterID.startsWith("band") || parameterID == "numActiveBands")
    {
        triggerEQCurveUpdate();
    }
}

void AIEqualizerAudioProcessor::triggerEQCurveUpdate()
{
    eqCurveNeedsUpdate = true;
    requestIRBuild();
}

void AIEqualizerAudioProcessor::updateLinearPhaseIRIfNeeded()
{
    // Check if IR builder has a new IR ready
    const int readyIdx = readyIRIndex.exchange(-1);
    if (readyIdx >= 0 && readyIdx < static_cast<int>(linearPhaseProcessors.size()))
    {
        previousIRIndex = activeIRIndex.load(std::memory_order_relaxed);
        pendingIRIndex = readyIdx;
        crossfadeSamplesRemaining = irCrossfadeSamples;
        activeIRIndex.store(readyIdx, std::memory_order_relaxed);
    }
}

void AIEqualizerAudioProcessor::requestIRBuild()
{
    irBuildEvent.signal();
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
// FIX 8: Report a fixed worst-case latency to avoid host reconfiguration
void AIEqualizerAudioProcessor::updateReportedLatency()
{
    if (worstCaseLatencySamples != lastReportedLatency)
    {
        setLatencySamples(worstCaseLatencySamples);
        lastReportedLatency = worstCaseLatencySamples;
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

    // Update AI parameters
    float sensitivity = apvts.getRawParameterValue("aiSensitivity")->load();
    float strength = apvts.getRawParameterValue("aiStrength")->load();
    
    aiEngine.setSensitivity(sensitivity);
    aiEngine.setStrength(strength);
    
    //--------------------------------------------------------------------------
    // Update Global Dynamic EQ settings
    //--------------------------------------------------------------------------
    float dynMix = apvts.getRawParameterValue("dynEqMix")->load() / 100.0f;
    bool dynAutoMakeup = apvts.getRawParameterValue("dynAutoMakeup")->load() > 0.5f;
    
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
    const int activeBandsLocal = numActiveBands.load(std::memory_order_relaxed);  // Cache for loop

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
        
        int type = static_cast<int>(loadParam(p.type, 2.0f));
        // Fallback for legacy states
        if (p.type == nullptr)
        {
            if (i == 0) type = ParametricEQProcessor::LowShelf;
            else if (i == activeBandsLocal - 1) type = ParametricEQProcessor::HighShelf;
        }
        
        if (i < eqProcessor.getNumBands())
        {
            eqProcessor.setBandParameters(i, freq, gain, q, type);
            eqProcessor.setBandEnabled(i, enabled);
        }
        if (i < eqProcessorHQ.getNumBands())
        {
            eqProcessorHQ.setBandParameters(i, freq, gain, q, type);
            eqProcessorHQ.setBandEnabled(i, enabled);
        }
        
        // Sync M/S processors
        if (i < eqProcessorMid.getNumBands())
        {
            eqProcessorMid.setBandParameters(i, freq, gain, q, type);
            eqProcessorMid.setBandEnabled(i, enabled);
        }
        if (i < eqProcessorSide.getNumBands())
        {
            eqProcessorSide.setBandParameters(i, freq, gain, q, type);
            eqProcessorSide.setBandEnabled(i, enabled);
        }
        
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
        dynParams.enabled = enabled;
        dynParams.dynamicMode = dynMode;  // Now int, not enum class
        dynParams.threshold = threshold;
        dynParams.ratio = ratio;
        dynParams.attackMs = attack;
        dynParams.releaseMs = release;
        dynParams.range = range;
        dynParams.knee = knee;
        
        dynamicEQProcessor.setBandParams(i, dynParams);
        dynamicEQProcessorHQ.setBandParams(i, dynParams);
        
        // Sync M/S dynamic processors
        dynamicEQProcessorMid.setBandParams(i, dynParams);
        dynamicEQProcessorSide.setBandParams(i, dynParams);
        
        // FIX: Update shadow processors for thread-safe IR building
        // These are read by the IR builder thread without locking
        if (i < eqProcessorForIR.getNumBands())
        {
            eqProcessorForIR.setBandParameters(i, freq, gain, q, type);
            eqProcessorForIR.setBandEnabled(i, enabled);
        }
        dynamicEQProcessorForIR.setBandParams(i, dynParams);
    }

    // Complete seqlock for IR shadow processors
    irCoeffVersion.fetch_add(1, std::memory_order_release); // mark write complete (even)
    
    // Signal that new coefficients are ready for IR builder
    irCoefficientsUpdated.store(true, std::memory_order_release);
}

//==============================================================================
// A/B Comparison Implementation

void AIEqualizerAudioProcessor::setABState(ABState state)
{
    const ABState current = currentABState.load(std::memory_order_relaxed);
    if (state == current)
        return;
    
    // Save current state to current slot
    saveCurrentStateToSlot(current);
    
    // Switch to new state
    currentABState.store(state, std::memory_order_relaxed);
    
    // Load new state
    loadStateFromSlot(state);
}

void AIEqualizerAudioProcessor::saveCurrentStateToSlot(ABState slot)
{
    EQSlot* targetSlot = nullptr;
    switch (slot)
    {
        case ABState::A: targetSlot = &slotA; break;
        case ABState::B: targetSlot = &slotB; break;
        case ABState::C: targetSlot = &slotC; break;
        case ABState::D: targetSlot = &slotD; break;
    }
    if (!targetSlot) return;
    
    targetSlot->bands.resize(maxBands);
    for (int i = 0; i < maxBands; ++i)
    {
        targetSlot->bands[i] = getBandState(i);
    }
    targetSlot->outputGain = apvts.getRawParameterValue("outputGain")->load();
}

void AIEqualizerAudioProcessor::loadStateFromSlot(ABState slot)
{
    const EQSlot* sourceSlot = nullptr;
    switch (slot)
    {
        case ABState::A: sourceSlot = &slotA; break;
        case ABState::B: sourceSlot = &slotB; break;
        case ABState::C: sourceSlot = &slotC; break;
        case ABState::D: sourceSlot = &slotD; break;
    }
    if (!sourceSlot) return;
    
    const int bandsToLoad = static_cast<int>(sourceSlot->bands.size());
    for (int i = 0; i < bandsToLoad; ++i)
    {
        setBandState(i, sourceSlot->bands[i]);
    }
    
    if (auto* param = apvts.getParameter("outputGain"))
        param->setValueNotifyingHost(param->convertTo0to1(sourceSlot->outputGain));
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
    captureService.captureSnapshotMs(lengthMs);
}

bool AIEqualizerAudioProcessor::analyzeCapturedAudioSnapshot()
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

bool AIEqualizerAudioProcessor::startManualCapture()
{
    // Delegate to lock-free CaptureService
    return captureService.startManualCapture();
}

void AIEqualizerAudioProcessor::stopManualCapture()
{
    // Delegate to lock-free CaptureService
    captureService.stopManualCapture();
}

void AIEqualizerAudioProcessor::getManualCapturePreview(std::vector<float>& outMono, size_t maxSamples) const
{
    // Delegate to lock-free CaptureService
    captureService.getManualCapturePreview(outMono, maxSamples);
}

//==============================================================================
// FIX: applyAICorrections() with undo support and user learning recording

// Converte AIEngine::Correction::FilterType in indice parametro bandXType
static int aiFilterTypeToParameterIndex(AIEngine::Correction::FilterType filterType)
{
    switch (filterType)
    {
        case AIEngine::Correction::FilterType::LowCut:    return 0;  // "Low Cut"
        case AIEngine::Correction::FilterType::LowShelf:  return 1;  // "Low Shelf"
        case AIEngine::Correction::FilterType::Peak:      return 2;  // "Peak"
        case AIEngine::Correction::FilterType::HighShelf: return 3;  // "High Shelf"
        case AIEngine::Correction::FilterType::HighCut:   return 4;  // "High Cut"
        case AIEngine::Correction::FilterType::Notch:     return 5;  // "Notch"
        default:                                          return 2;  // Default Peak
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
        const float reuseThreshold = targetFreq * 0.148f; // ~1/5 octave
        
        for (int i = 0; i < activeBands; ++i)
        {
            float dist = std::abs(bandFreqs[i] - targetFreq);
            float score = dist;
            
            // Prefer unused bands (much lower score)
            if (!bandUsed[i])
            {
                score *= 0.3f;
            }
            
            // Prefer bands within reuse threshold (can merge)
            if (dist < reuseThreshold)
            {
                score *= 0.2f;  // Strong preference for reuse
            }
            
            if (score < bestScore)
            {
                bestScore = score;
                bestBand = i;
            }
        }
        
        if (bestBand >= 0)
        {
            juce::String prefix = "band" + juce::String(bestBand);
            
            // FIX: Use beginChangeGesture/endChangeGesture for proper undo support
            if (auto* freqParam = apvts.getParameter(prefix + "Freq"))
            {
                freqParam->beginChangeGesture();
                freqParam->setValueNotifyingHost(freqParam->convertTo0to1(scaled.frequency));
                freqParam->endChangeGesture();
            }
            
            if (auto* gainParam = apvts.getParameter(prefix + "Gain"))
            {
                gainParam->beginChangeGesture();
                gainParam->setValueNotifyingHost(gainParam->convertTo0to1(scaled.suggestedGain));
                gainParam->endChangeGesture();
            }
            
            if (auto* qParam = apvts.getParameter(prefix + "Q"))
            {
                qParam->beginChangeGesture();
                qParam->setValueNotifyingHost(qParam->convertTo0to1(scaled.suggestedQ));
                qParam->endChangeGesture();
            }

            // Set filter type from AI suggestion
            if (auto* typeParam = apvts.getParameter(prefix + "Type"))
            {
                int typeIndex = aiFilterTypeToParameterIndex(scaled.suggestedFilter);
                typeParam->beginChangeGesture();
                typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(typeIndex)));
                typeParam->endChangeGesture();
            }
            
            // Enable the band
            if (auto* enabledParam = apvts.getParameter(prefix + "Enabled"))
            {
                enabledParam->beginChangeGesture();
                enabledParam->setValueNotifyingHost(1.0f);
                enabledParam->endChangeGesture();
            }
            
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
    const float reuseThreshold = targetFreq * 0.148f; // ~1/5 octave
    
    for (int i = 0; i < activeBands; ++i)
    {
        float dist = std::abs(bandFreqs[i] - targetFreq);
        float score = dist;
        
        // Prefer unused bands (much lower score)
        if (!bandUsed[i])
        {
            score *= 0.3f;
        }
        
        // Prefer bands within reuse threshold (can merge)
        if (dist < reuseThreshold)
        {
            score *= 0.2f;  // Strong preference for reuse
        }
        
        if (score < bestScore)
        {
            bestScore = score;
            bestBand = i;
        }
    }
    
    if (bestBand >= 0)
    {
        juce::String prefix = "band" + juce::String(bestBand);
        
        // Apply parameters with undo support
        if (auto* freqParam = apvts.getParameter(prefix + "Freq"))
        {
            freqParam->beginChangeGesture();
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1(scaled.frequency));
            freqParam->endChangeGesture();
        }
        
        if (auto* gainParam = apvts.getParameter(prefix + "Gain"))
        {
            gainParam->beginChangeGesture();
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(scaled.suggestedGain));
            gainParam->endChangeGesture();
        }
        
        if (auto* qParam = apvts.getParameter(prefix + "Q"))
        {
            qParam->beginChangeGesture();
            qParam->setValueNotifyingHost(qParam->convertTo0to1(scaled.suggestedQ));
            qParam->endChangeGesture();
        }

        // Set filter type from AI suggestion
        if (auto* typeParam = apvts.getParameter(prefix + "Type"))
        {
            int typeIndex = aiFilterTypeToParameterIndex(scaled.suggestedFilter);
            typeParam->beginChangeGesture();
            typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(typeIndex)));
            typeParam->endChangeGesture();
        }
        
        // Enable the band
        if (auto* enabledParam = apvts.getParameter(prefix + "Enabled"))
        {
            enabledParam->beginChangeGesture();
            enabledParam->setValueNotifyingHost(1.0f);
            enabledParam->endChangeGesture();
        }
        
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

//==============================================================================
AIEqualizerAudioProcessor::BandState AIEqualizerAudioProcessor::getBandState(int bandIndex) const
{
    BandState state;
    
    if (bandIndex < 0 || bandIndex >= maxBands)
        return state;
    
    juce::String prefix = "band" + juce::String(bandIndex);
    
    state.frequency = apvts.getRawParameterValue(prefix + "Freq")->load();
    state.gain = apvts.getRawParameterValue(prefix + "Gain")->load();
    state.q = apvts.getRawParameterValue(prefix + "Q")->load();
    state.enabled = apvts.getRawParameterValue(prefix + "Enabled")->load() > 0.5f;
    
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
    
    return state;
}

void AIEqualizerAudioProcessor::setBandState(int bandIndex, const BandState& state)
{
    if (bandIndex < 0 || bandIndex >= maxBands)
        return;
    
    juce::String prefix = "band" + juce::String(bandIndex);
    
    if (auto* param = apvts.getParameter(prefix + "Freq"))
        param->setValueNotifyingHost(param->convertTo0to1(state.frequency));
    
    if (auto* param = apvts.getParameter(prefix + "Gain"))
        param->setValueNotifyingHost(param->convertTo0to1(state.gain));
    
    if (auto* param = apvts.getParameter(prefix + "Q"))
        param->setValueNotifyingHost(param->convertTo0to1(state.q));
    
    if (auto* param = apvts.getParameter(prefix + "Type"))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(juce::jlimit(0, 6, state.type))));
    
    if (auto* param = apvts.getParameter(prefix + "Enabled"))
        param->setValueNotifyingHost(state.enabled ? 1.0f : 0.0f);

    markParametersChanged();
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
    auto state = apvts.copyState();
    
    // Add A/B/C/D slot data to state
    state.setProperty("abState", static_cast<int>(currentABState.load(std::memory_order_relaxed)), nullptr);
    // Save slot names
    state.setProperty("slotAName", slotA.name, nullptr);
    state.setProperty("slotBName", slotB.name, nullptr);
    state.setProperty("slotCName", slotC.name, nullptr);
    state.setProperty("slotDName", slotD.name, nullptr);
    
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

void AIEqualizerAudioProcessor::encodeMidSide(juce::AudioBuffer<float>& buffer)
{
    // M/S Encoding: Mid = (L + R) / sqrt(2), Side = (L - R) / sqrt(2)
    if (buffer.getNumChannels() < 2 || msBuffer.getNumSamples() < buffer.getNumSamples())
    {
        jassert(msBuffer.getNumSamples() >= buffer.getNumSamples());
    }
    
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left;
    float* mid = msBuffer.getWritePointer(0);
    float* side = msBuffer.getWritePointer(1);
    
    const int numSamples = buffer.getNumSamples();
    const float sqrt2Inv = 0.7071067811865476f; // 1/sqrt(2) for proper normalization
    
    for (int i = 0; i < numSamples; ++i)
    {
        mid[i] = (left[i] + right[i]) * sqrt2Inv;   // Mid channel
        side[i] = (left[i] - right[i]) * sqrt2Inv;  // Side channel
    }
}

void AIEqualizerAudioProcessor::decodeMidSide(juce::AudioBuffer<float>& buffer)
{
    // M/S Decoding: L = (Mid + Side) / sqrt(2), R = (Mid - Side) / sqrt(2)
    if (buffer.getNumChannels() < 2 || msBuffer.getNumSamples() < buffer.getNumSamples())
        return;
    
    const float* mid = msBuffer.getReadPointer(0);
    const float* side = msBuffer.getReadPointer(1);
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    
    const int numSamples = buffer.getNumSamples();
    const float sqrt2Inv = 0.7071067811865476f; // 1/sqrt(2) for proper normalization
    
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = (mid[i] + side[i]) * sqrt2Inv;   // Left channel
        if (buffer.getNumChannels() > 1)
            right[i] = (mid[i] - side[i]) * sqrt2Inv;  // Right channel
    }
}

//==============================================================================
// AI Command Queue (Lock-Free Communication)
//==============================================================================

void AIEqualizerAudioProcessor::queueAICommand(const AIEQCore::AICommand& command) noexcept
{
    // Queue command for audio thread (lock-free SPSC queue)
    // If queue is full, command is dropped (prevents blocking)
    aiCommandQueue.tryPush(command);
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
    
    params.numActiveBands = numActiveBands.load(std::memory_order_relaxed);
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
        
        band.frequency = loadParam(cached.freq, 1000.0f);
        band.gain = loadParam(cached.gain, 0.0f);
        band.q = loadParam(cached.q, 1.0f);
        band.filterType = static_cast<int>(loadParam(cached.type, 2.0f));
        band.enabled = loadParam(cached.enabled, 1.0f) > 0.5f;
        
        // Dynamic EQ
        band.dynamicMode = static_cast<int>(loadParam(cached.dynMode, 0.0f));
        band.threshold = loadParam(cached.dynThreshold, -20.0f);
        band.ratio = loadParam(cached.dynRatio, 2.0f);
        band.attackMs = loadParam(cached.dynAttack, 10.0f);
        band.releaseMs = loadParam(cached.dynRelease, 100.0f);
        band.range = loadParam(cached.dynRange, 24.0f);
        band.knee = loadParam(cached.dynKnee, 6.0f);
    }
    
    // Increment version for change detection
    params.version++;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AIEqualizerAudioProcessor();
}
