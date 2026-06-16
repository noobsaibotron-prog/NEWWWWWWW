#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <cmath>
#include <fstream>
#include <random>

//==============================================================================
/**
 * Simple Neural Network Engine for Audio Problem Detection
 * 
 * This is a lightweight ML implementation without external dependencies.
 * It uses pre-trained weights for:
 * - Resonance detection
 * - Harshness classification
 * - Muddiness detection
 * - Sibilance detection
 * - Genre classification
 * 
 * The network architecture is:
 * - Input: FFT spectrum bins (normalized)
 * - Hidden layers: Dense with ReLU
 * - Output: Problem probabilities (sigmoid)
 */
class MLEngine
{
public:
    //==========================================================================
    // Problem types that ML can detect
    enum class ProblemType
    {
        Resonance = 0,
        Harshness,
        Muddiness,
        Sibilance,
        Boominess,
        Thinness,
        BoxyMidrange,
        Clipping,
        NumProblems
    };
    
    static constexpr int numProblemTypes = static_cast<int>(ProblemType::NumProblems);
    
    // Genre types for context-aware analysis
    enum class GenreType
    {
        Unknown = 0,
        Vocals,
        Drums,
        Bass,
        Synth,
        Master,
        EDM,
        Acoustic,
        NumGenres
    };
    
    static constexpr int numGenreTypes = static_cast<int>(GenreType::NumGenres);
    
    //==========================================================================
    struct ProblemDetection
    {
        ProblemType type;
        float confidence;        // 0.0 to 1.0
        float severity;          // 0.0 to 1.0
        float frequency;         // Hz - center frequency of problem
        float bandwidth;         // Hz - width of problem area
        float suggestedGain;     // dB
        float suggestedQ;
    };
    
    struct GenreDetection
    {
        GenreType type;
        float confidence;
    };
    
    //==========================================================================
    MLEngine();
    ~MLEngine() = default;
    
    void initialize();
    void reset();
    
    //==========================================================================
    // Main inference methods
    // rawProbabilitiesOut (optional): receives the raw sigmoid outputs of THIS
    // inference, before threshold/margin/rank filtering. Audit/observability
    // only — passing nullptr (default) keeps every existing caller unchanged
    // and adds zero work. Avoids a second forward pass for diagnostics.
    std::vector<ProblemDetection> detectProblems(const std::vector<float>& spectrum,
                                                  double sampleRate,
                                                  std::array<float, numProblemTypes>* rawProbabilitiesOut = nullptr);

    /** Forward pass only — returns raw sigmoid probabilities for all 8 classes,
        without any threshold filtering. Used for threshold calibration. */
    std::array<float, numProblemTypes> forwardRawProbabilities(
        const std::vector<float>& spectrum, double sampleRate);
    
    GenreDetection classifyGenre(const std::vector<float>& spectrum, double sampleRate = 44100.0);
    
    //==========================================================================
    // Model management
    bool loadWeights(const juce::File& modelFile);
    bool saveWeights(const juce::File& modelFile) const;
    void initializeRandomWeights(); // For training

    // Observability of the last SUCCESSFUL loadWeights() (audit/log/tests only;
    // does not change search order or fallback behavior). Empty/0/false when the
    // engine is still on random-initialized weights.
    bool areWeightsLoadedFromFile() const { return weightsLoadedFromFile; }
    juce::String getLoadedWeightsPath() const { return loadedWeightsPath; }
    juce::int64 getLoadedWeightsBytes() const { return loadedWeightsBytes; }
    juce::String getLoadedWeightsChecksum() const { return loadedWeightsChecksum; }
    
    //==========================================================================
    // Basic training / fine-tuning
    struct TrainingSample
    {
        std::vector<float> melSpectrum;                              // melNumBands elements
        std::array<float, numProblemTypes> problemTargets{};         // 0..1 desired probabilities
        std::array<float, numProblemTypes> frequencyTargets{};       // 0..1 normalized within problem range
    };
    
    void trainOnDataset(const std::vector<TrainingSample>& dataset,
                        int epochs = 3,
                        float learningRate = 0.001f);
    
    bool trainOnSyntheticData(int epochs = 3,
                              int samplesPerProblem = 8,
                              float learningRate = 0.0015f,
                              double sampleRate = 44100.0,
                              int fftSize = 2048,
                              const juce::File& saveTo = {});
    
    std::vector<TrainingSample> generateSyntheticDataset(int samplesPerProblem,
                                                         double sampleRate = 44100.0,
                                                         int fftSize = 2048);
    
    //==========================================================================
    // Configuration
    void setSensitivity(float sens) { sensitivity = juce::jlimit(0.0f, 1.0f, sens); }
    float getSensitivity() const { return sensitivity; }
    
    void setContext(GenreType genre) { currentContext = genre; }
    GenreType getContext() const { return currentContext; }

    /** Read-only access to base thresholds (for diagnostic/test instrumentation). */
    const std::array<float, numProblemTypes>& getBaseThresholds() const { return baseThresholds; }

   #if JUCE_UNIT_TESTS
    void setTopKOverrideForTests(int topK) { topKOverrideForTests = topK; }
    void clearTopKOverrideForTests() { topKOverrideForTests = 0; }
   #endif

    //==========================================================================
    // Utility
    static juce::String getProblemName(ProblemType type);
    static juce::String getGenreName(GenreType type);

private:
    // Metadata of the last successful loadWeights() — observability only.
    bool weightsLoadedFromFile = false;
    juce::String loadedWeightsPath;
    juce::int64 loadedWeightsBytes = 0;
    juce::String loadedWeightsChecksum;

    //==========================================================================
    // Simple Dense Layer
    class DenseLayer
    {
    public:
        DenseLayer(int inputSize, int outputSize, bool useBias = true);
        
        std::vector<float> forward(const std::vector<float>& input) const;
        
        void setWeights(const std::vector<float>& w);
        void setBias(const std::vector<float>& b);
        
        const std::vector<float>& getWeights() const { return weights; }
        const std::vector<float>& getBias() const { return bias; }
        
        int getInputSize() const { return inputSize; }
        int getOutputSize() const { return outputSize; }
        bool hasBias() const { return useBias; }
        
        void randomize(std::mt19937& rng);
        void applyGradients(const std::vector<float>& gradWeights,
                            const std::vector<float>& gradBias,
                            float learningRate);
        
    private:
        int inputSize;
        int outputSize;
        bool useBias;
        std::vector<float> weights;  // [outputSize * inputSize]
        std::vector<float> bias;     // [outputSize]
    };
    
    //==========================================================================
    // Activation functions
    static float relu(float x) { return std::max(0.0f, x); }
    static float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }
    static float tanh_act(float x) { return std::tanh(x); }
    
    static std::vector<float> softmax(const std::vector<float>& x);
    static std::vector<float> applyRelu(const std::vector<float>& x);
    static std::vector<float> applySigmoid(const std::vector<float>& x);
    static float reluDerivative(float x) { return x > 0.0f ? 1.0f : 0.0f; }
    static float sigmoidDerivative(float y) { return y * (1.0f - y); }
    
    //==========================================================================
    // Preprocessing
    std::vector<float> preprocessSpectrum(const std::vector<float>& spectrum,
                                           double sampleRate) const;
    
    std::vector<float> extractMelBands(const std::vector<float>& spectrum,
                                        double sampleRate,
                                        int numBands = 64) const;
    
    float hzToMel(float hz) const;
    float melToHz(float mel) const;
    
    float adjustThresholdForContext(float threshold, ProblemType type) const;
    float findPeakInRange(const std::vector<float>& spectrum, double sampleRate,
                          float minHz, float maxHz) const;
    TrainingSample createSyntheticSample(ProblemType type, double sampleRate, int fftSize);
    std::vector<float> buildSyntheticSpectrum(ProblemType type, double sampleRate,
                                              int fftSize, float targetFreq, float strength) const;
    void trainStep(const TrainingSample& sample, float learningRate);
    std::vector<float> matMul(const DenseLayer& layer, const std::vector<float>& input) const;
    
    //==========================================================================
    // Network architecture
    // Problem Detection Network
    std::unique_ptr<DenseLayer> problemNet_fc1;  // 64 -> 128
    std::unique_ptr<DenseLayer> problemNet_fc2;  // 128 -> 64
    std::unique_ptr<DenseLayer> problemNet_fc3;  // 64 -> numProblemTypes
    
    // Genre Classification Network
    std::unique_ptr<DenseLayer> genreNet_fc1;    // 64 -> 64
    std::unique_ptr<DenseLayer> genreNet_fc2;    // 64 -> numGenreTypes
    
    // Frequency Localization Network (predicts problem center frequency)
    std::unique_ptr<DenseLayer> freqNet_fc1;     // 64 -> 32
    std::unique_ptr<DenseLayer> freqNet_fc2;     // 32 -> numProblemTypes
    
    //==========================================================================
    // State
    float sensitivity = 0.5f;
    GenreType currentContext = GenreType::Unknown;

    // Test-only override of the top-K cap. UNCONDITIONAL (not #if JUCE_UNIT_TESTS)
    // ON PURPOSE: gating it as a DATA member shifted the object layout of every
    // member below it (baseThresholds, problemFreqRanges, ...) between the plugin
    // SharedCode (macro OFF) and test TUs (macro ON). That corrupted inline
    // accessors such as getBaseThresholds() when read from the IntegrationTests
    // binary — P2-HAZARD-001 (a 0.20 threshold read as 0.30, Boxyness as 100.0).
    // Cost in production: one unused int. The read/write code paths stay gated.
    int topKOverrideForTests = 0;

    // Pre-computed mel filterbank
    std::vector<std::vector<float>> melFilterbank;
    int melNumBands = 64;
    
    // Detection thresholds per problem type (adjusted by context)
    std::array<float, numProblemTypes> baseThresholds;
    
    // Problem frequency ranges (for localization)
    struct FrequencyRange { float minHz, maxHz; };
    std::array<FrequencyRange, numProblemTypes> problemFreqRanges;
    
    // Default correction suggestions
    std::array<float, numProblemTypes> defaultGains;
    std::array<float, numProblemTypes> defaultQs;
    
    std::atomic<bool> isInitialized { false };
    std::once_flag    initFlag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MLEngine)
};
