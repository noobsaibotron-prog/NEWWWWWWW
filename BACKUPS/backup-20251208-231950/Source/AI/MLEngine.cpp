#include "MLEngine.h"
#include <algorithm>
#include <numeric>

//==============================================================================
// DenseLayer Implementation
//==============================================================================
MLEngine::DenseLayer::DenseLayer(int inSize, int outSize, bool useBias_)
    : inputSize(inSize), outputSize(outSize), useBias(useBias_)
{
    weights.resize(static_cast<size_t>(outputSize * inputSize), 0.0f);
    if (useBias)
        bias.resize(static_cast<size_t>(outputSize), 0.0f);
}

std::vector<float> MLEngine::DenseLayer::forward(const std::vector<float>& input) const
{
    jassert(static_cast<int>(input.size()) >= inputSize);
    
    std::vector<float> output(static_cast<size_t>(outputSize), 0.0f);
    
    for (int o = 0; o < outputSize; ++o)
    {
        float sum = useBias ? bias[static_cast<size_t>(o)] : 0.0f;
        
        for (int i = 0; i < inputSize; ++i)
        {
            sum += weights[static_cast<size_t>(o * inputSize + i)] * input[static_cast<size_t>(i)];
        }
        
        output[static_cast<size_t>(o)] = sum;
    }
    
    return output;
}

void MLEngine::DenseLayer::setWeights(const std::vector<float>& w)
{
    if (w.size() == weights.size())
        weights = w;
}

void MLEngine::DenseLayer::setBias(const std::vector<float>& b)
{
    if (useBias && b.size() == bias.size())
        bias = b;
}

void MLEngine::DenseLayer::randomize(std::mt19937& rng)
{
    // Xavier/Glorot initialization
    float scale = std::sqrt(2.0f / static_cast<float>(inputSize + outputSize));
    std::normal_distribution<float> dist(0.0f, scale);
    
    for (auto& w : weights)
        w = dist(rng);
    
    if (useBias)
    {
        for (auto& b : bias)
            b = 0.0f;
    }
}

//==============================================================================
// MLEngine Implementation
//==============================================================================
MLEngine::MLEngine()
{
    // Initialize base thresholds for each problem type
    baseThresholds = {{
        0.3f,   // Resonance
        0.35f,  // Harshness
        0.4f,   // Muddiness
        0.35f,  // Sibilance
        0.4f,   // Boominess
        0.35f,  // Thinness
        0.4f,   // BoxyMidrange
        0.2f    // Clipping
    }};
    
    // Problem frequency ranges
    problemFreqRanges = {{
        {100.0f, 5000.0f},    // Resonance - can occur anywhere
        {2000.0f, 8000.0f},   // Harshness
        {100.0f, 400.0f},     // Muddiness
        {5000.0f, 12000.0f},  // Sibilance
        {40.0f, 150.0f},      // Boominess
        {80.0f, 300.0f},      // Thinness (lack of low-mids)
        {300.0f, 800.0f},     // BoxyMidrange
        {20.0f, 20000.0f}     // Clipping
    }};
    
    // Default correction values
    defaultGains = {{
        -4.0f,   // Resonance
        -3.0f,   // Harshness
        -2.5f,   // Muddiness
        -2.0f,   // Sibilance
        -3.0f,   // Boominess
        +2.0f,   // Thinness (boost)
        -2.0f,   // BoxyMidrange
        -6.0f    // Clipping
    }};
    
    defaultQs = {{
        4.0f,    // Resonance - narrow
        1.5f,    // Harshness - medium
        1.0f,    // Muddiness - wide
        2.0f,    // Sibilance
        1.5f,    // Boominess
        0.8f,    // Thinness - wide shelf
        1.2f,    // BoxyMidrange
        0.7f     // Clipping - wide
    }};
}

void MLEngine::initialize()
{
    if (isInitialized)
        return;
    
    // Create network layers
    problemNet_fc1 = std::make_unique<DenseLayer>(melNumBands, 128);
    problemNet_fc2 = std::make_unique<DenseLayer>(128, 64);
    problemNet_fc3 = std::make_unique<DenseLayer>(64, numProblemTypes);
    
    genreNet_fc1 = std::make_unique<DenseLayer>(melNumBands, 64);
    genreNet_fc2 = std::make_unique<DenseLayer>(64, numGenreTypes);
    
    freqNet_fc1 = std::make_unique<DenseLayer>(melNumBands, 32);
    freqNet_fc2 = std::make_unique<DenseLayer>(32, numProblemTypes);
    
    // Initialize with pre-trained weights or random
    if (!loadWeights(juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                        .getSiblingFile("ml_weights.bin")))
    {
        initializeRandomWeights();
    }
    
    isInitialized = true;
}

void MLEngine::reset()
{
    // Reset any running state
}

void MLEngine::initializeRandomWeights()
{
    std::mt19937 rng(42); // Fixed seed for reproducibility
    
    problemNet_fc1->randomize(rng);
    problemNet_fc2->randomize(rng);
    problemNet_fc3->randomize(rng);
    
    genreNet_fc1->randomize(rng);
    genreNet_fc2->randomize(rng);
    
    freqNet_fc1->randomize(rng);
    freqNet_fc2->randomize(rng);
    
    // Apply some hand-tuned biases for reasonable initial behavior
    // This makes the model work better out-of-the-box even with random weights
    
    // Problem detection biases - make network more or less sensitive to each type
    std::vector<float> problemBias = {
        -0.5f,   // Resonance
        -0.3f,   // Harshness
        -0.4f,   // Muddiness
        -0.3f,   // Sibilance
        -0.5f,   // Boominess
        -0.6f,   // Thinness
        -0.5f,   // BoxyMidrange
        -1.0f    // Clipping - hard to detect, lower sensitivity
    };
    problemNet_fc3->setBias(problemBias);
}

//==============================================================================
std::vector<MLEngine::ProblemDetection> MLEngine::detectProblems(
    const std::vector<float>& spectrum, double sampleRate)
{
    if (!isInitialized)
        initialize();
    
    std::vector<ProblemDetection> detections;
    
    if (spectrum.empty())
        return detections;
    
    // Preprocess spectrum to mel bands
    auto melSpectrum = extractMelBands(spectrum, sampleRate, melNumBands);
    
    if (melSpectrum.size() != static_cast<size_t>(melNumBands))
        return detections;
    
    //--------------------------------------------------------------------------
    // Problem Detection Network Forward Pass
    //--------------------------------------------------------------------------
    auto h1 = problemNet_fc1->forward(melSpectrum);
    h1 = applyRelu(h1);
    
    auto h2 = problemNet_fc2->forward(h1);
    h2 = applyRelu(h2);
    
    auto problemProbs = problemNet_fc3->forward(h2);
    problemProbs = applySigmoid(problemProbs);
    
    //--------------------------------------------------------------------------
    // Frequency Localization Network Forward Pass
    //--------------------------------------------------------------------------
    auto fh1 = freqNet_fc1->forward(melSpectrum);
    fh1 = applyRelu(fh1);
    
    auto freqOutputs = freqNet_fc2->forward(fh1);
    freqOutputs = applySigmoid(freqOutputs); // 0-1 normalized frequency position
    
    //--------------------------------------------------------------------------
    // Generate detections for problems above threshold
    //--------------------------------------------------------------------------
    float sensitivityScale = 1.0f - (sensitivity - 0.5f) * 0.6f; // Lower = more sensitive
    
    for (int i = 0; i < numProblemTypes; ++i)
    {
        float prob = problemProbs[static_cast<size_t>(i)];
        float threshold = baseThresholds[static_cast<size_t>(i)] * sensitivityScale;
        
        // Adjust threshold based on context (genre)
        threshold = adjustThresholdForContext(threshold, static_cast<ProblemType>(i));
        
        if (prob > threshold)
        {
            ProblemDetection det;
            det.type = static_cast<ProblemType>(i);
            det.confidence = prob;
            det.severity = (prob - threshold) / (1.0f - threshold);
            
            // Calculate frequency from network output and problem range
            const auto& range = problemFreqRanges[static_cast<size_t>(i)];
            float freqNorm = freqOutputs[static_cast<size_t>(i)];
            
            // Also find actual peak in the spectrum for this frequency range
            float actualPeakFreq = findPeakInRange(spectrum, sampleRate, range.minHz, range.maxHz);
            
            // Blend network prediction with actual peak finding
            float predictedFreq = range.minHz * std::pow(range.maxHz / range.minHz, freqNorm);
            det.frequency = 0.3f * predictedFreq + 0.7f * actualPeakFreq;
            det.frequency = juce::jlimit(range.minHz, range.maxHz, det.frequency);
            
            // Calculate bandwidth based on Q
            det.suggestedQ = defaultQs[static_cast<size_t>(i)] * (1.0f + det.severity * 0.5f);
            det.bandwidth = det.frequency / det.suggestedQ;
            
            // Scale suggested gain by severity
            det.suggestedGain = defaultGains[static_cast<size_t>(i)] * (0.5f + det.severity * 0.5f);
            
            detections.push_back(det);
        }
    }
    
    // Sort by severity (highest first)
    std::sort(detections.begin(), detections.end(),
              [](const ProblemDetection& a, const ProblemDetection& b) {
                  return a.severity > b.severity;
              });
    
    return detections;
}

MLEngine::GenreDetection MLEngine::classifyGenre(const std::vector<float>& spectrum)
{
    if (!isInitialized)
        initialize();
    
    GenreDetection result;
    result.type = GenreType::Unknown;
    result.confidence = 0.0f;
    
    if (spectrum.empty())
        return result;
    
    auto melSpectrum = extractMelBands(spectrum, 44100.0, melNumBands);
    
    if (melSpectrum.size() != static_cast<size_t>(melNumBands))
        return result;
    
    // Genre Network Forward Pass
    auto h1 = genreNet_fc1->forward(melSpectrum);
    h1 = applyRelu(h1);
    
    auto genreProbs = genreNet_fc2->forward(h1);
    genreProbs = softmax(genreProbs);
    
    // Find max probability
    int maxIdx = 0;
    float maxProb = genreProbs[0];
    
    for (int i = 1; i < numGenreTypes; ++i)
    {
        if (genreProbs[static_cast<size_t>(i)] > maxProb)
        {
            maxProb = genreProbs[static_cast<size_t>(i)];
            maxIdx = i;
        }
    }
    
    result.type = static_cast<GenreType>(maxIdx);
    result.confidence = maxProb;
    
    return result;
}

//==============================================================================
// Preprocessing
//==============================================================================
std::vector<float> MLEngine::extractMelBands(const std::vector<float>& spectrum,
                                              double sampleRate,
                                              int numBands) const
{
    std::vector<float> melBands(static_cast<size_t>(numBands), 0.0f);
    
    if (spectrum.empty())
        return melBands;
    
    int fftSize = static_cast<int>(spectrum.size()) * 2;
    float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    
    // Mel scale parameters
    float minMel = hzToMel(20.0f);
    float maxMel = hzToMel(std::min(20000.0f, static_cast<float>(sampleRate) * 0.5f));
    float melStep = (maxMel - minMel) / static_cast<float>(numBands + 1);
    
    for (int band = 0; band < numBands; ++band)
    {
        float melLow = minMel + static_cast<float>(band) * melStep;
        float melCenter = minMel + static_cast<float>(band + 1) * melStep;
        float melHigh = minMel + static_cast<float>(band + 2) * melStep;
        
        float hzLow = melToHz(melLow);
        float hzCenter = melToHz(melCenter);
        float hzHigh = melToHz(melHigh);
        
        int binLow = static_cast<int>(hzLow / binHz);
        int binCenter = static_cast<int>(hzCenter / binHz);
        int binHigh = static_cast<int>(hzHigh / binHz);
        
        binLow = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, binLow);
        binCenter = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, binCenter);
        binHigh = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, binHigh);
        
        float energy = 0.0f;
        int count = 0;
        
        // Triangular filterbank
        for (int bin = binLow; bin <= binHigh; ++bin)
        {
            float weight = 0.0f;
            if (bin < binCenter && binCenter > binLow)
                weight = static_cast<float>(bin - binLow) / static_cast<float>(binCenter - binLow);
            else if (bin >= binCenter && binHigh > binCenter)
                weight = static_cast<float>(binHigh - bin) / static_cast<float>(binHigh - binCenter);
            
            if (bin >= 0 && bin < static_cast<int>(spectrum.size()))
            {
                energy += spectrum[static_cast<size_t>(bin)] * weight;
                ++count;
            }
        }
        
        // Normalize and convert to dB
        if (count > 0)
            energy /= static_cast<float>(count);
        
        // Log compression (simulates dB scale)
        melBands[static_cast<size_t>(band)] = std::log10(energy + 1e-10f) / std::log10(1e-10f) * -1.0f;
        melBands[static_cast<size_t>(band)] = juce::jlimit(0.0f, 1.0f, melBands[static_cast<size_t>(band)]);
    }
    
    return melBands;
}

float MLEngine::hzToMel(float hz) const
{
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
}

float MLEngine::melToHz(float mel) const
{
    return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
}

//==============================================================================
// Activation Functions
//==============================================================================
std::vector<float> MLEngine::softmax(const std::vector<float>& x)
{
    std::vector<float> result(x.size());
    
    // Find max for numerical stability
    float maxVal = *std::max_element(x.begin(), x.end());
    
    float sum = 0.0f;
    for (size_t i = 0; i < x.size(); ++i)
    {
        result[i] = std::exp(x[i] - maxVal);
        sum += result[i];
    }
    
    if (sum > 0.0f)
    {
        for (auto& v : result)
            v /= sum;
    }
    
    return result;
}

std::vector<float> MLEngine::applyRelu(const std::vector<float>& x)
{
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        result[i] = relu(x[i]);
    return result;
}

std::vector<float> MLEngine::applySigmoid(const std::vector<float>& x)
{
    std::vector<float> result(x.size());
    for (size_t i = 0; i < x.size(); ++i)
        result[i] = sigmoid(x[i]);
    return result;
}

//==============================================================================
// Helper Functions
//==============================================================================
float MLEngine::adjustThresholdForContext(float threshold, ProblemType type) const
{
    // Adjust thresholds based on source type context
    switch (currentContext)
    {
        case GenreType::Vocals:
            if (type == ProblemType::Sibilance) return threshold * 0.8f; // More sensitive
            if (type == ProblemType::Muddiness) return threshold * 0.9f;
            break;
            
        case GenreType::Drums:
            if (type == ProblemType::Boominess) return threshold * 0.85f;
            if (type == ProblemType::BoxyMidrange) return threshold * 0.9f;
            break;
            
        case GenreType::Bass:
            if (type == ProblemType::Muddiness) return threshold * 0.85f;
            if (type == ProblemType::Boominess) return threshold * 1.2f; // Less sensitive (expected)
            break;
            
        case GenreType::EDM:
            if (type == ProblemType::Harshness) return threshold * 1.1f; // Allow more brightness
            if (type == ProblemType::Boominess) return threshold * 1.1f;
            break;
            
        case GenreType::Acoustic:
            if (type == ProblemType::Resonance) return threshold * 0.85f; // More sensitive
            break;
            
        default:
            break;
    }
    
    return threshold;
}

float MLEngine::findPeakInRange(const std::vector<float>& spectrum, double sampleRate,
                                 float minHz, float maxHz) const
{
    if (spectrum.empty())
        return (minHz + maxHz) * 0.5f;
    
    int fftSize = static_cast<int>(spectrum.size()) * 2;
    float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    
    int minBin = static_cast<int>(minHz / binHz);
    int maxBin = static_cast<int>(maxHz / binHz);
    
    minBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, minBin);
    maxBin = juce::jlimit(0, static_cast<int>(spectrum.size()) - 1, maxBin);
    
    float maxVal = -100.0f;
    int maxIdx = minBin;
    
    for (int i = minBin; i <= maxBin; ++i)
    {
        if (spectrum[static_cast<size_t>(i)] > maxVal)
        {
            maxVal = spectrum[static_cast<size_t>(i)];
            maxIdx = i;
        }
    }
    
    return static_cast<float>(maxIdx) * binHz;
}

//==============================================================================
// Model Persistence
//==============================================================================
bool MLEngine::loadWeights(const juce::File& modelFile)
{
    if (!modelFile.existsAsFile())
        return false;
    
    try
    {
        juce::FileInputStream stream(modelFile);
        if (!stream.openedOk())
            return false;
        
        // Read magic number
        uint32_t magic = static_cast<uint32_t>(stream.readInt());
        if (magic != 0x4D4C4551) // "MLEQ"
            return false;
        
        // Read version
        uint32_t version = static_cast<uint32_t>(stream.readInt());
        if (version != 1)
            return false;
        
        auto readVector = [&stream](std::vector<float>& vec, size_t size) {
            vec.resize(size);
            for (size_t i = 0; i < size; ++i)
                vec[i] = stream.readFloat();
        };
        
        // Read problem network weights
        std::vector<float> w1, b1, w2, b2, w3, b3;
        readVector(w1, 128 * 64);
        readVector(b1, 128);
        readVector(w2, 64 * 128);
        readVector(b2, 64);
        readVector(w3, numProblemTypes * 64);
        readVector(b3, numProblemTypes);
        
        problemNet_fc1->setWeights(w1);
        problemNet_fc1->setBias(b1);
        problemNet_fc2->setWeights(w2);
        problemNet_fc2->setBias(b2);
        problemNet_fc3->setWeights(w3);
        problemNet_fc3->setBias(b3);
        
        // Read genre network weights
        std::vector<float> gw1, gb1, gw2, gb2;
        readVector(gw1, 64 * 64);
        readVector(gb1, 64);
        readVector(gw2, numGenreTypes * 64);
        readVector(gb2, numGenreTypes);
        
        genreNet_fc1->setWeights(gw1);
        genreNet_fc1->setBias(gb1);
        genreNet_fc2->setWeights(gw2);
        genreNet_fc2->setBias(gb2);
        
        // Read frequency network weights
        std::vector<float> fw1, fb1, fw2, fb2;
        readVector(fw1, 32 * 64);
        readVector(fb1, 32);
        readVector(fw2, numProblemTypes * 32);
        readVector(fb2, numProblemTypes);
        
        freqNet_fc1->setWeights(fw1);
        freqNet_fc1->setBias(fb1);
        freqNet_fc2->setWeights(fw2);
        freqNet_fc2->setBias(fb2);
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool MLEngine::saveWeights(const juce::File& modelFile) const
{
    if (!isInitialized)
        return false;
    
    try
    {
        juce::FileOutputStream stream(modelFile);
        if (!stream.openedOk())
            return false;
        
        // Write magic number
        stream.writeInt(static_cast<int>(0x4D4C4551)); // "MLEQ"
        
        // Write version
        stream.writeInt(1);
        
        auto writeVector = [&stream](const std::vector<float>& vec) {
            for (float v : vec)
                stream.writeFloat(v);
        };
        
        // Write problem network
        writeVector(problemNet_fc1->getWeights());
        writeVector(problemNet_fc1->getBias());
        writeVector(problemNet_fc2->getWeights());
        writeVector(problemNet_fc2->getBias());
        writeVector(problemNet_fc3->getWeights());
        writeVector(problemNet_fc3->getBias());
        
        // Write genre network
        writeVector(genreNet_fc1->getWeights());
        writeVector(genreNet_fc1->getBias());
        writeVector(genreNet_fc2->getWeights());
        writeVector(genreNet_fc2->getBias());
        
        // Write frequency network
        writeVector(freqNet_fc1->getWeights());
        writeVector(freqNet_fc1->getBias());
        writeVector(freqNet_fc2->getWeights());
        writeVector(freqNet_fc2->getBias());
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

//==============================================================================
// Utility
//==============================================================================
juce::String MLEngine::getProblemName(ProblemType type)
{
    switch (type)
    {
        case ProblemType::Resonance:    return "Resonance";
        case ProblemType::Harshness:    return "Harshness";
        case ProblemType::Muddiness:    return "Muddiness";
        case ProblemType::Sibilance:    return "Sibilance";
        case ProblemType::Boominess:    return "Boominess";
        case ProblemType::Thinness:     return "Thinness";
        case ProblemType::BoxyMidrange: return "Boxy Midrange";
        case ProblemType::Clipping:     return "Clipping";
        default:                        return "Unknown";
    }
}

juce::String MLEngine::getGenreName(GenreType type)
{
    switch (type)
    {
        case GenreType::Unknown:  return "Unknown";
        case GenreType::Vocals:   return "Vocals";
        case GenreType::Drums:    return "Drums";
        case GenreType::Bass:     return "Bass";
        case GenreType::Synth:    return "Synth";
        case GenreType::Master:   return "Master";
        case GenreType::EDM:      return "EDM";
        case GenreType::Acoustic: return "Acoustic";
        default:                  return "Unknown";
    }
}

