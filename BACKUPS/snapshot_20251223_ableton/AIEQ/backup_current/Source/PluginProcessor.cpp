#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <limits>
#include <algorithm>
#include <complex>

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
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    apvts.addParameterListener("phaseMode", this);
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

    // Initialize A/B slots with defaults
    slotA.bands.resize(maxBands, BandState());
    slotB.bands.resize(maxBands, BandState());

    // Start IR builder thread
    irBuilderThread = std::thread([this]() {
        juce::dsp::FFT fft(LinearPhaseProcessor::fftOrder);
        const size_t fftSize = LinearPhaseProcessor::fftSize;
        const size_t halfSize = fftSize / 2;
        std::vector<std::complex<float>> freqDomain(fftSize, { 0.0f, 0.0f });
        std::vector<std::complex<float>> timeDomain(fftSize, { 0.0f, 0.0f });
        std::vector<float> irBuf(fftSize, 0.0f);
        std::vector<float> magDB(halfSize, -120.0f);

        while (!irBuilderShouldExit.load())
        {
            irBuildEvent.wait(-1);
            if (irBuilderShouldExit.load())
                break;

            if (!eqCurveNeedsUpdate.exchange(false))
                continue;

            const double sr = currentSampleRate;
            const bool dynEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;

            std::fill(magDB.begin(), magDB.end(), -120.0f);

            // Log-spaced sampling ~512 bins
            const size_t logBins = 512;
            const float fMin = 20.0f;
            const float fMax = static_cast<float>(sr * 0.5 * 0.999); // avoid Nyquist
            const float logMin = std::log(fMin);
            const float logMax = std::log(fMax);

            for (size_t i = 0; i < logBins; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(logBins - 1);
                const float freq = std::exp(logMin + t * (logMax - logMin));
                const float binF = freq / static_cast<float>(sr);
                const size_t bin = static_cast<size_t>(juce::jlimit(0.0f, static_cast<float>(halfSize - 1),
                                                                    binF * static_cast<float>(LinearPhaseProcessor::fftSize)));

                float mag = eqProcessor.getMagnitudeForFrequency(freq, sr);
                if (dynEnabled)
                    mag *= dynamicEQProcessor.getMagnitudeForFrequency(freq, sr);
                const float db = juce::Decibels::gainToDecibels(mag, -120.0f);
                magDB[bin] = db;
            }

            // Build IR in freqDomain/timeDomain/irBuf
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

            for (size_t n = 0; n < LinearPhaseProcessor::fftSize; ++n)
                irBuf[n] = timeDomain[n].real();

            // center
            std::rotate(irBuf.begin(), irBuf.begin() + static_cast<long>(halfSize), irBuf.end());

            // Apply Hann + normalize on irSize
            float maxAbs = 0.0f;
            for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
            {
                const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(n)
                                                        / static_cast<float>(LinearPhaseProcessor::irSize - 1)));
                irBuf[n] *= w;
                maxAbs = std::max(maxAbs, std::abs(irBuf[n]));
            }
            if (maxAbs > 0.0f)
            {
                const float inv = 1.0f / maxAbs;
                for (size_t n = 0; n < LinearPhaseProcessor::irSize; ++n)
                    irBuf[n] *= inv;
            }

            // Load IR into back processor
            const int backIdx = 1 - activeIRIndex.load();
            if (backIdx >= 0 && backIdx < static_cast<int>(linearPhaseProcessors.size()))
            {
                if (auto* back = linearPhaseProcessors[static_cast<size_t>(backIdx)].get())
                {
                    back->loadImpulseResponse({ irBuf.begin(), irBuf.begin() + LinearPhaseProcessor::irSize }, sr);
                    readyIRIndex.store(backIdx);
                }
            }
        }
    });
}

AIEqualizerAudioProcessor::~AIEqualizerAudioProcessor()
{
    apvts.removeParameterListener("phaseMode", this);
    for (const auto& id : eqParameterIDs)
        apvts.removeParameterListener(id, this);

    irBuilderShouldExit.store(true);
    irBuildEvent.signal();
    if (irBuilderThread.joinable())
        irBuilderThread.join();
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
            juce::StringArray{"Low Cut", "Low Shelf", "Peak", "High Shelf", "High Cut", "Notch", "Band Pass"},
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
void AIEqualizerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

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
        int resIdx = static_cast<int>(res->load());
        if (auto* resParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("analyzerResolution")))
            resIdx = static_cast<int>(std::round(resParam->convertFrom0to1(res->load())));

        analyzerResolutionCached = resIdx;
        auto resolution = toResolution(analyzerResolutionCached);
        spectrumAnalyzer.setFFTResolution(resolution);
        postEQAnalyzer.setFFTResolution(resolution);
    }

    if (auto* spd = apvts.getRawParameterValue("analyzerSpeed"))
    {
        int spdIdx = static_cast<int>(spd->load());
        if (auto* spdParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("analyzerSpeed")))
            spdIdx = static_cast<int>(std::round(spdParam->convertFrom0to1(spd->load())));

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
    // HQ (NaturalPhase) path with 2x oversampling
    const double hqSampleRate = sampleRate * 2.0;
    const int hqBlockSize = samplesPerBlock * 2;
    eqProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());
    dynamicEQProcessorHQ.prepare(hqSampleRate, hqBlockSize, getTotalNumInputChannels());
    naturalOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(getTotalNumInputChannels()),
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    naturalOversampler->reset();
    naturalOversampler->initProcessing(static_cast<size_t>(samplesPerBlock));
    naturalPhaseLatency = static_cast<int>(naturalOversampler->getLatencyInSamples());
    naturalOversampledBuffer.setSize(getTotalNumInputChannels(), hqBlockSize, false, false, true);
    
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
    
    // Apply quality/latency mode to dynamic EQ lookahead
    int qualityMode = static_cast<int>(apvts.getRawParameterValue("qualityMode")->load());
    float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms lookahead, Zero-latency: 0ms
    dynamicEQProcessor.setLookahead(lookaheadMs);
    dynamicEQProcessor.updateLookaheadBuffer(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    qualityModeCached = qualityMode;
    aiEngine.prepare(sampleRate, samplesPerBlock);
    referenceMatcher.prepare(sampleRate, samplesPerBlock);

    // Prepare capture ring buffer
    const int maxSamples = static_cast<int>(sampleRate * captureMaxSeconds);
    captureRing.setSize(getTotalNumInputChannels(), maxSamples);
    captureRing.clear();
    captureRingLength = maxSamples;
    captureRingWritePos = 0;
    capturedSampleRate = sampleRate;
    
    // Initialize bands if not already initialized
    if (eqProcessor.getNumBands() == 0)
    {
        float defaultFreqs[AIEqualizerAudioProcessor::maxBands] = {
            31.0f, 50.0f, 80.0f, 120.0f, 170.0f, 250.0f, 350.0f, 500.0f,
            700.0f, 1000.0f, 1400.0f, 2000.0f, 2800.0f, 4000.0f, 5600.0f, 8000.0f,
            11000.0f, 15000.0f, 18000.0f, 22000.0f, 26000.0f, 30000.0f, 34000.0f, 38000.0f
        };
        int active = numActiveBands;
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
        numActiveBands = std::min(active, eqProcessor.getNumBands());
    }
    
    // Reset RMS values
    preEQRMS = 0.0f;
    postEQRMS = 0.0f;
    autoGainCompensation = 0.0f;
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
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

//==============================================================================
void AIEqualizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Capture audio for analysis (even if bypassed)
    pushToCaptureRing(buffer);
    
    // Check bypass
    bool bypassed = apvts.getRawParameterValue("bypass")->load() > 0.5f;
    if (bypassed)
        return;

    // Handle quality/latency mode (adjust lookahead dynamically)
    int qualityMode = static_cast<int>(apvts.getRawParameterValue("qualityMode")->load());
    if (qualityMode != qualityModeCached)
    {
        qualityModeCached = qualityMode;
        float lookaheadMs = (qualityMode == 1) ? 5.0f : 0.0f; // HQ: 5ms, Zero-Latency: 0ms
        dynamicEQProcessor.setLookahead(lookaheadMs);
        dynamicEQProcessor.updateLookaheadBuffer(currentSampleRate, currentBlockSize, totalNumInputChannels);
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

    if (auto* res = apvts.getRawParameterValue("analyzerResolution"))
    {
        int resIdx = static_cast<int>(res->load());
        if (auto* resParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("analyzerResolution")))
            resIdx = static_cast<int>(std::round(resParam->convertFrom0to1(res->load())));

        if (resIdx != analyzerResolutionCached)
        {
            analyzerResolutionCached = resIdx;
            auto resolution = toResolution(resIdx);
            spectrumAnalyzer.setFFTResolution(resolution);
            postEQAnalyzer.setFFTResolution(resolution);
        }
    }

    if (auto* spd = apvts.getRawParameterValue("analyzerSpeed"))
    {
        int spdIdx = static_cast<int>(spd->load());
        if (auto* spdParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("analyzerSpeed")))
            spdIdx = static_cast<int>(std::round(spdParam->convertFrom0to1(spd->load())));

        if (spdIdx != analyzerSpeedCached)
        {
            analyzerSpeedCached = spdIdx;
            auto speed = toSpeed(spdIdx);
            spectrumAnalyzer.setSpeed(speed);
            postEQAnalyzer.setSpeed(speed);
        }
    }
    
    // Update EQ parameters from APVTS
    updateEQFromParameters();
    
    // Calculate pre-EQ RMS for auto-gain
    autoGainEnabled = apvts.getRawParameterValue("autoGain")->load() > 0.5f;
    if (autoGainEnabled)
    {
        float currentPreRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPreRMS = (currentPreRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;
        
        preEQRMS = preEQRMS * rmsSmoothing + currentPreRMS * (1.0f - rmsSmoothing);
    }
    
    // Feed pre-EQ spectrum analyzer (for visualization and AI)
    spectrumAnalyzer.process(buffer);
    
    // Skip AI analysis during offline rendering for performance
    bool isOffline = isNonRealtime();
    bool aiEnabled = apvts.getRawParameterValue("aiEnabled")->load() > 0.5f;
    
    if (aiEnabled && !isOffline)
    {
        // Update source profile
        int profileIndex = static_cast<int>(apvts.getRawParameterValue("sourceProfile")->load());
        aiEngine.setSourceProfile(static_cast<AIEngine::SourceProfile>(profileIndex));
        
        const auto& spectrum = spectrumAnalyzer.getSmoothedSpectrum();
        if (!spectrum.empty())
        {
            aiEngine.analyzeSpectrum(spectrum);
        }
    }
    
    const auto mode = currentPhaseMode.load();
    const int desiredLatency = (mode == PhaseMode::LinearPhase) ? 4095
                             : (mode == PhaseMode::NaturalPhase ? naturalPhaseLatency : 0);
    if (desiredLatency != lastReportedLatency)
    {
        setLatencySamples(desiredLatency);
        lastReportedLatency = desiredLatency;
    }
    
    if (mode == PhaseMode::LinearPhase)
    {
        updateLinearPhaseIRIfNeeded(); // swap in pre-built IR if ready
        auto* lp = linearPhaseProcessors[static_cast<size_t>(activeIRIndex.load())].get();
        if (lp)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lp->process(ctx);
        }
    }
    else if (mode == PhaseMode::NaturalPhase)
    {
        if (naturalOversampler)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            auto upBlock = naturalOversampler->processSamplesUp(block);

            // Copy upsampled block into AudioBuffer view
            const auto upChannels = upBlock.getNumChannels();
            const auto upSamples = static_cast<int>(upBlock.getNumSamples());
            if (naturalOversampledBuffer.getNumChannels() < static_cast<int>(upChannels)
                || naturalOversampledBuffer.getNumSamples() < upSamples)
            {
                naturalOversampledBuffer.setSize(static_cast<int>(upChannels), upSamples, false, false, true);
            }
            for (size_t ch = 0; ch < upChannels; ++ch)
                std::memcpy(naturalOversampledBuffer.getWritePointer(static_cast<int>(ch)),
                            upBlock.getChannelPointer(ch),
                            static_cast<size_t>(upSamples) * sizeof(float));

            eqProcessorHQ.process(naturalOversampledBuffer);

            bool dynEqEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;
            if (dynEqEnabled)
            {
                dynamicEQProcessorHQ.process(naturalOversampledBuffer);
            }

            // Copy processed data back into upBlock
            for (size_t ch = 0; ch < upChannels; ++ch)
                std::memcpy(upBlock.getChannelPointer(ch),
                            naturalOversampledBuffer.getReadPointer(static_cast<int>(ch)),
                            static_cast<size_t>(upSamples) * sizeof(float));

            naturalOversampler->processSamplesDown(block);
        }
        else
        {
            // Fallback: standard path
            eqProcessor.process(buffer);
            bool dynEqEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;
            if (dynEqEnabled)
                dynamicEQProcessor.process(buffer);
        }
    }
    else
    {
        // Existing IIR path
        eqProcessor.process(buffer);
        
        bool dynEqEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;
        if (dynEqEnabled)
        {
            dynamicEQProcessor.process(buffer);
        }
    }
    
    // Feed post-EQ spectrum analyzer
    bool showPost = apvts.getRawParameterValue("showPostSpectrum")->load() > 0.5f;
    if (showPost)
    {
        postEQAnalyzer.process(buffer);
    }
    
    // Calculate auto-gain compensation
    if (autoGainEnabled)
    {
        float currentPostRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        if (totalNumInputChannels > 1)
            currentPostRMS = (currentPostRMS + buffer.getRMSLevel(1, 0, buffer.getNumSamples())) * 0.5f;
        
        postEQRMS = postEQRMS * rmsSmoothing + currentPostRMS * (1.0f - rmsSmoothing);
        
        calculateAutoGain();
    }
    
    // Apply output gain (manual + auto-gain compensation)
    float outputGainDB = apvts.getRawParameterValue("outputGain")->load();
    float totalGainDB = outputGainDB;
    
    if (autoGainEnabled)
    {
        totalGainDB += autoGainCompensation;
    }
    
    if (std::abs(totalGainDB) > 0.01f)
    {
        float linearGain = juce::Decibels::decibelsToGain(totalGainDB);
        buffer.applyGain(linearGain);
    }
}

void AIEqualizerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "phaseMode")
    {
        const int modeIdx = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        currentPhaseMode.store(static_cast<PhaseMode>(modeIdx));
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
    const int readyIdx = readyIRIndex.exchange(-1);
    if (readyIdx >= 0 && readyIdx < static_cast<int>(linearPhaseProcessors.size()))
        activeIRIndex.store(readyIdx);
}

void AIEqualizerAudioProcessor::requestIRBuild()
{
    irBuildEvent.signal();
}

//==============================================================================
void AIEqualizerAudioProcessor::calculateAutoGain()
{
    // Avoid division by zero and handle silence
    constexpr float minRMS = 0.0001f;
    if (postEQRMS < minRMS || preEQRMS < minRMS)
    {
        autoGainCompensation = 0.0f;
        return;
    }
    
    // Calculate the dB difference
    float preDB = juce::Decibels::gainToDecibels(preEQRMS, -100.0f);
    float postDB = juce::Decibels::gainToDecibels(postEQRMS, -100.0f);
    
    // Compensation = how much we need to boost to match pre-EQ level
    float targetCompensation = preDB - postDB;
    
    // Limit compensation range (-12 to +12 dB) for safety
    constexpr float maxCompensation = 12.0f;
    targetCompensation = juce::jlimit(-maxCompensation, maxCompensation, targetCompensation);
    
    // Smooth the compensation to avoid sudden jumps (1% per sample at 60Hz = ~0.6s time constant)
    constexpr float smoothingFactor = 0.99f;
    autoGainCompensation = autoGainCompensation * smoothingFactor + targetCompensation * (1.0f - smoothingFactor);
}

//==============================================================================
void AIEqualizerAudioProcessor::updateEQFromParameters()
{
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
    if (auto* nb = apvts.getRawParameterValue("numActiveBands"))
    {
        const float raw = nb->load();
        const int idx = std::isfinite(raw) ? static_cast<int>(raw) : 7;
        numActiveBands = juce::jlimit(1, maxBands, idx + 1); // choice index starts at 0
    }
    else
    {
        numActiveBands = 8;
    }
    ensureBandCount(maxBands);

    // Clamp active bands to what is actually available in processors
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    numActiveBands = juce::jlimit(1, availableBands, numActiveBands);
    
    const int bandsAvailable = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });

    // Update EQ bands
    for (int i = 0; i < bandsAvailable; ++i)
    {
        juce::String prefix = "band" + juce::String(i);
        
        float freq = apvts.getRawParameterValue(prefix + "Freq")->load();
        float gain = apvts.getRawParameterValue(prefix + "Gain")->load();
        float q = apvts.getRawParameterValue(prefix + "Q")->load();
        bool enabled = apvts.getRawParameterValue(prefix + "Enabled")->load() > 0.5f && (i < numActiveBands);
        
        int type = ParametricEQProcessor::Peak;
        if (auto* typeParam = apvts.getRawParameterValue(prefix + "Type"))
        {
            type = static_cast<int>(typeParam->load());
        }
        else
        {
            // Fallback for legacy states
            if (i == 0)
                type = ParametricEQProcessor::LowShelf;
            else if (i == numActiveBands - 1)
                type = ParametricEQProcessor::HighShelf;
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
        
        //----------------------------------------------------------------------
        // Update Dynamic EQ band parameters
        //----------------------------------------------------------------------
        int dynMode = static_cast<int>(apvts.getRawParameterValue(prefix + "DynMode")->load());
        float threshold = apvts.getRawParameterValue(prefix + "Threshold")->load();
        float ratio = apvts.getRawParameterValue(prefix + "Ratio")->load();
        float attack = apvts.getRawParameterValue(prefix + "Attack")->load();
        float release = apvts.getRawParameterValue(prefix + "Release")->load();
        float range = apvts.getRawParameterValue(prefix + "Range")->load();
        float knee = apvts.getRawParameterValue(prefix + "Knee")->load();
        
        DynamicEQProcessor::DynamicBandParams dynParams;
        dynParams.frequency = freq;
        dynParams.gain = gain;
        dynParams.q = q;
        dynParams.filterType = type;
        dynParams.enabled = enabled;
        dynParams.dynamicMode = static_cast<DynamicEQProcessor::DynamicMode>(dynMode);
        dynParams.threshold = threshold;
        dynParams.ratio = ratio;
        dynParams.attackMs = attack;
        dynParams.releaseMs = release;
        dynParams.range = range;
        dynParams.knee = knee;
        
        dynamicEQProcessor.setBandParams(i, dynParams);
        dynamicEQProcessorHQ.setBandParams(i, dynParams);
    }
}

//==============================================================================
// A/B Comparison Implementation

void AIEqualizerAudioProcessor::setABState(ABState state)
{
    if (state == currentABState)
        return;
    
    // Save current state to current slot
    saveCurrentStateToSlot(currentABState);
    
    // Switch to new state
    currentABState = state;
    
    // Load new state
    loadStateFromSlot(currentABState);
}

void AIEqualizerAudioProcessor::saveCurrentStateToSlot(ABState slot)
{
    EQSlot& targetSlot = (slot == ABState::A) ? slotA : slotB;
    
    targetSlot.bands.resize(maxBands);
    for (int i = 0; i < maxBands; ++i)
    {
        targetSlot.bands[i] = getBandState(i);
    }
    targetSlot.outputGain = apvts.getRawParameterValue("outputGain")->load();
}

void AIEqualizerAudioProcessor::loadStateFromSlot(ABState slot)
{
    const EQSlot& sourceSlot = (slot == ABState::A) ? slotA : slotB;
    const int bandsToLoad = static_cast<int>(sourceSlot.bands.size());
    for (int i = 0; i < bandsToLoad; ++i)
    {
        setBandState(i, sourceSlot.bands[i]);
    }
    
    if (auto* param = apvts.getParameter("outputGain"))
        param->setValueNotifyingHost(param->convertTo0to1(sourceSlot.outputGain));
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

void AIEqualizerAudioProcessor::swapAB()
{
    saveCurrentStateToSlot(currentABState);
    std::swap(slotA, slotB);
    loadStateFromSlot(currentABState);
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

void AIEqualizerAudioProcessor::setNumActiveBands(int n)
{
    numActiveBands = juce::jlimit(1, maxBands, n);
    ensureBandCount(maxBands);
    const int availableBands = std::min({ maxBands, eqProcessor.getNumBands(), eqProcessorHQ.getNumBands() });
    numActiveBands = juce::jlimit(1, availableBands, numActiveBands);
    for (int i = numActiveBands; i < eqProcessor.getNumBands(); ++i)
        eqProcessor.setBandEnabled(i, false);
    for (int i = numActiveBands; i < eqProcessorHQ.getNumBands(); ++i)
        eqProcessorHQ.setBandEnabled(i, false);
}

void AIEqualizerAudioProcessor::ensureBandCount(int count)
{
    const int target = juce::jlimit(1, maxBands, count);
    
    auto addMissing = [this, target](ParametricEQProcessor& proc)
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
            proc.setBandEnabled(i, i < numActiveBands);
        }
    };
    
    addMissing(eqProcessor);
    addMissing(eqProcessorHQ);
}

//==============================================================================
// Audio capture for analysis
//==============================================================================
void AIEqualizerAudioProcessor::pushToCaptureRing(const juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;
    
    // Calculate current energy level for auto-capture
    float currentEnergy = 0.0f;
    if (autoCaptureEnabled && autoCaptureCooldown <= 0)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                currentEnergy += data[s] * data[s];
            }
        }
        currentEnergy = std::sqrt(currentEnergy / static_cast<float>(numCh * numSamples));
        
        // Trigger auto-capture on energy peak (sudden increase)
        if (currentEnergy > energyThreshold && currentEnergy > lastEnergyLevel * 1.5f)
        {
            if (!isManualCapturing)
            {
                startManualCapture();
                autoCaptureCooldown = static_cast<int>(currentSampleRate * 2.0); // 2 second cooldown
            }
        }
        lastEnergyLevel = currentEnergy;
    }
    
    if (autoCaptureCooldown > 0)
        autoCaptureCooldown -= numSamples;
    
    // Manual capture: append to buffer
    if (isManualCapturing)
    {
        juce::SpinLock::ScopedTryLockType lock(captureLock);
        if (lock.isLocked())
        {
            const size_t oldSize = manualCaptureBuffer.size();
            const int channelsToCopy = std::min(numCh, 2); // Max 2 channels
            manualCaptureBuffer.resize(oldSize + static_cast<size_t>(numSamples * channelsToCopy));
            
            for (int s = 0; s < numSamples; ++s)
            {
                for (int ch = 0; ch < channelsToCopy; ++ch)
                {
                    const float* src = buffer.getReadPointer(ch);
                    manualCaptureBuffer[oldSize + static_cast<size_t>(s * channelsToCopy + ch)] = src[s];
                }
            }
            manualCaptureSamples += numSamples;
            
            // Auto-stop if buffer too large (max 20 seconds)
            const int maxSamples = static_cast<int>(currentSampleRate * captureMaxSeconds);
            if (manualCaptureSamples >= maxSamples)
            {
                stopManualCapture();
            }
        }
    }
    
    // Ring buffer (always running for retroactive capture)
    if (captureRingLength == 0)
        return;
    
    juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked())
        return;
    const int channelsToCopy = std::min(numCh, captureRing.getNumChannels());
    
    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        float* dst = captureRing.getWritePointer(ch);
        
        int writePos = captureRingWritePos;
        int remaining = numSamples;
        int srcOffset = 0;
        
        while (remaining > 0)
        {
            const int chunk = std::min(remaining, captureRingLength - writePos);
            std::memcpy(dst + writePos, src + srcOffset, static_cast<size_t>(chunk) * sizeof(float));
            remaining -= chunk;
            srcOffset += chunk;
            writePos = (writePos + chunk) % captureRingLength;
        }
    }
    
    captureRingWritePos = (captureRingWritePos + numSamples) % captureRingLength;
}

void AIEqualizerAudioProcessor::captureAudioSnapshotMs(int lengthMs)
{
    if (captureRingLength == 0 || lengthMs <= 0)
        return;
    
    const int samplesRequested = static_cast<int>(capturedSampleRate * (static_cast<double>(lengthMs) / 1000.0));
    const int samplesToCopy = std::min(samplesRequested, captureRingLength);
    
    std::vector<float> mono(static_cast<size_t>(samplesToCopy));
    
    juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked())
        return;
    const int start = (captureRingWritePos - samplesToCopy + captureRingLength) % captureRingLength;
    const int channelsToCopy = captureRing.getNumChannels();
    
    for (int i = 0; i < samplesToCopy; ++i)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            const float* src = captureRing.getReadPointer(ch);
            const int idx = (start + i) % captureRingLength;
            sum += src[idx];
        }
        mono[static_cast<size_t>(i)] = sum / static_cast<float>(channelsToCopy);
    }
    
    capturedAudioMono = std::move(mono);
}

bool AIEqualizerAudioProcessor::analyzeCapturedAudioSnapshot()
{
    std::vector<float> monoCopy;
    double sr = 0.0;
    {
        juce::SpinLock::ScopedTryLockType lock(captureLock);
        if (lock.isLocked())
        {
            monoCopy = capturedAudioMono;
            sr = capturedSampleRate;
        }
    }
    
    if (monoCopy.empty() || sr <= 0.0)
        return false;
    
    // Offline spectrum analysis on captured audio
    SpectrumAnalyzer analyzer;
    analyzer.prepare(sr, 512);
    
    juce::AudioBuffer<float> tempBuffer(1, static_cast<int>(monoCopy.size()));
    std::memcpy(tempBuffer.getWritePointer(0), monoCopy.data(), monoCopy.size() * sizeof(float));
    
    // Process in chunks (512 samples)
    int offset = 0;
    const int total = tempBuffer.getNumSamples();
    const int block = 512;
    while (offset < total)
    {
        const int chunk = std::min(block, total - offset);
        juce::AudioBuffer<float> slice(tempBuffer.getArrayOfWritePointers(), 1, offset, chunk);
        analyzer.process(slice);
        offset += chunk;
    }
    
    const auto& spectrum = analyzer.getSmoothedSpectrum();
    aiEngine.analyzeSpectrum(spectrum, true);
    return true;
}

bool AIEqualizerAudioProcessor::startManualCapture()
{
    juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked())
        return false;
    
    if (isManualCapturing)
        return false;  // Already capturing
    
    isManualCapturing = true;
    manualCaptureBuffer.clear();
    manualCaptureSamples = 0;
    capturedSampleRate = currentSampleRate;
    
    return true;
}

void AIEqualizerAudioProcessor::stopManualCapture()
{
    juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked())
        return;
    
    if (!isManualCapturing)
        return;
    
    isManualCapturing = false;
    
    // Convert to mono and store
    if (!manualCaptureBuffer.empty() && manualCaptureSamples > 0)
    {
        capturedAudioMono.resize(static_cast<size_t>(manualCaptureSamples));
        
        // Convert stereo/multi-channel to mono
        const int numChannels = static_cast<int>(manualCaptureBuffer.size()) / manualCaptureSamples;
        for (int i = 0; i < manualCaptureSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                sum += manualCaptureBuffer[static_cast<size_t>(i * numChannels + ch)];
            }
            capturedAudioMono[static_cast<size_t>(i)] = sum / static_cast<float>(numChannels);
        }
    }
    
    manualCaptureBuffer.clear();
    manualCaptureSamples = 0;
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
    // Get filtered and prioritized corrections (with severity/confidence thresholds)
    auto filtered = aiEngine.getFilteredAndPrioritizedCorrections(0.25f, 0.55f);
    
    if (filtered.empty())
        return;
    
    // Merge nearby corrections to avoid duplicate bands
    auto merged = aiEngine.mergeNearbyCorrections(filtered);
    
    // Limit to available bands (but don't hard-limit to 8)
    const int maxAssignable = std::min(static_cast<int>(merged.size()), numActiveBands);
    merged.resize(maxAssignable);
    
    if (merged.empty())
        return;
    
    // Save current state for undo BEFORE applying corrections
    pushUndoState("AI Correction Applied (" + juce::String(merged.size()) + " bands)");
    
    // Track which bands are already used
    std::vector<bool> bandUsed(maxBands, false);
    std::vector<float> bandFreqs(maxBands, 0.0f);
    
    // Pre-scan existing bands
    for (int i = 0; i < numActiveBands; ++i)
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
        
        for (int i = 0; i < numActiveBands; ++i)
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
            userLearning.recordAISuggestionAccepted(
                AIEngine::getProblemTypeName(corr.type),
                scaled.frequency,
                scaled.suggestedGain,
                scaled.suggestedQ
            );
            
            // Mark band as used
            bandUsed[bestBand] = true;
            bandFreqs[bestBand] = scaled.frequency;
        }
    }
    
    // Clear approved corrections after applying
    aiEngine.clearCorrections();
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
    
    // Add A/B slot data to state
    state.setProperty("abState", static_cast<int>(currentABState), nullptr);
    
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
            
            // Restore A/B state
            auto loadedState = apvts.state;
            if (loadedState.hasProperty("abState"))
            {
                currentABState = static_cast<ABState>(static_cast<int>(loadedState.getProperty("abState")));
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
// Undo/Redo Implementation
//==============================================================================

void AIEqualizerAudioProcessor::pushUndoState(const juce::String& description)
{
    EQSnapshot snapshot;
    
    snapshot.bands.resize(maxBands);
    for (int i = 0; i < maxBands; ++i)
    {
        snapshot.bands[i] = getBandState(i);
    }
    snapshot.description = description;
    snapshot.timestamp = juce::Time::currentTimeMillis();
    
    undoHistory.push_back(snapshot);
    
    // Limit history size
    while (undoHistory.size() > maxHistorySize)
    {
        undoHistory.erase(undoHistory.begin());
    }
    
    // Clear redo stack when new action is performed
    redoHistory.clear();
}

void AIEqualizerAudioProcessor::undo()
{
    if (undoHistory.empty())
        return;
    
    // Save current state to redo stack
    EQSnapshot currentSnapshot;
    currentSnapshot.bands.resize(maxBands);
    for (int i = 0; i < maxBands; ++i)
    {
        currentSnapshot.bands[i] = getBandState(i);
    }
    currentSnapshot.description = "Redo: " + undoHistory.back().description;
    currentSnapshot.timestamp = juce::Time::currentTimeMillis();
    redoHistory.push_back(currentSnapshot);
    
    // Restore previous state
    const auto& previousState = undoHistory.back();
    const int bandsToRestore = static_cast<int>(previousState.bands.size());
    for (int i = 0; i < bandsToRestore; ++i)
    {
        setBandState(i, previousState.bands[i]);
    }
    
    undoHistory.pop_back();
}

void AIEqualizerAudioProcessor::redo()
{
    if (redoHistory.empty())
        return;
    
    // Save current state to undo stack
    EQSnapshot currentSnapshot;
    currentSnapshot.bands.resize(maxBands);
    for (int i = 0; i < maxBands; ++i)
    {
        currentSnapshot.bands[i] = getBandState(i);
    }
    currentSnapshot.description = redoHistory.back().description;
    currentSnapshot.timestamp = juce::Time::currentTimeMillis();
    undoHistory.push_back(currentSnapshot);
    
    // Restore redo state
    const auto& redoState = redoHistory.back();
    const int bandsRedo = static_cast<int>(redoState.bands.size());
    for (int i = 0; i < bandsRedo; ++i)
    {
        setBandState(i, redoState.bands[i]);
    }
    
    redoHistory.pop_back();
}

juce::String AIEqualizerAudioProcessor::getUndoDescription() const
{
    if (undoHistory.empty())
        return "Nothing to undo";
    return "Undo: " + undoHistory.back().description;
}

juce::String AIEqualizerAudioProcessor::getRedoDescription() const
{
    if (redoHistory.empty())
        return "Nothing to redo";
    return redoHistory.back().description;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AIEqualizerAudioProcessor();
}
