#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <limits>
#include <algorithm>

//==============================================================================
AIEqualizerAudioProcessor::AIEqualizerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    // Initialize A/B slots with defaults
    slotA.bands.resize(maxBands, BandState());
    slotB.bands.resize(maxBands, BandState());
}

AIEqualizerAudioProcessor::~AIEqualizerAudioProcessor()
{
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
    
    // Prepare components
    spectrumAnalyzer.prepare(sampleRate, samplesPerBlock);
    postEQAnalyzer.prepare(sampleRate, samplesPerBlock);
    eqProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dynamicEQProcessor.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    
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
    
    // Apply static EQ processing
    eqProcessor.process(buffer);
    
    // Apply Dynamic EQ processing (FabFilter/TDR Nova style)
    bool dynEqEnabled = apvts.getRawParameterValue("dynEqEnabled")->load() > 0.5f;
    if (dynEqEnabled)
    {
        dynamicEQProcessor.process(buffer);
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
    
    // Update active bands count
    if (auto* nb = apvts.getRawParameterValue("numActiveBands"))
        numActiveBands = juce::jlimit(1, maxBands, static_cast<int>(nb->load()) + 1); // choice index starts at 0
    else
        numActiveBands = 8;
    
    // Update EQ bands
    for (int i = 0; i < maxBands; ++i)
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
    for (int i = numActiveBands; i < eqProcessor.getNumBands(); ++i)
    {
        eqProcessor.setBandEnabled(i, false);
    }
}

//==============================================================================
// Audio capture for analysis
//==============================================================================
void AIEqualizerAudioProcessor::pushToCaptureRing(const juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (captureRingLength == 0 || numSamples == 0)
        return;
    
    std::lock_guard<std::mutex> lock(captureMutex);
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
    
    std::lock_guard<std::mutex> lock(captureMutex);
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
        std::lock_guard<std::mutex> lock(captureMutex);
        monoCopy = capturedAudioMono;
        sr = capturedSampleRate;
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

//==============================================================================
// FIX: applyAICorrections() with undo support and user learning recording
void AIEqualizerAudioProcessor::applyAICorrections()
{
    auto corrections = aiEngine.getApprovedCorrections();
    
    if (corrections.empty())
        return;
    
    // Save current state for undo BEFORE applying corrections
    pushUndoState("AI Correction Applied");
    
    // Find free bands or reuse existing ones
    const int activeBands = numActiveBands;
    for (const auto& corr : corrections)
    {
        // Get scaled correction based on strength
        auto scaled = aiEngine.getScaledCorrection(corr);
        
        // Find the closest band to this frequency
        int bestBand = -1;
        float bestDist = std::numeric_limits<float>::max();
        
        for (int i = 0; i < std::min(maxBands, std::max(activeBands, eqProcessor.getNumBands())); ++i)
        {
            juce::String prefix = "band" + juce::String(i);
            float bandFreq = apvts.getRawParameterValue(prefix + "Freq")->load();
            float bandGain = apvts.getRawParameterValue(prefix + "Gain")->load();
            
            // Prefer bands with zero gain (unused)
            float dist = std::abs(bandFreq - scaled.frequency);
            if (std::abs(bandGain) < 0.5f)
                dist *= 0.5f; // Prefer unused bands
            
            if (dist < bestDist)
            {
                bestDist = dist;
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
