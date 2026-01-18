#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

#if defined(AIEQ_ENABLE_TFLITE)
#include <tensorflow/lite/c/c_api.h>
#endif

//==============================================================================
/**
 * Neural Network Wrapper for PyTorch/libtorch Integration
 * 
 * Provides interface for loading and running pre-trained PyTorch models
 * for advanced audio analysis tasks:
 * - Unmasking detection
 * - Extended profile classification
 * - Dynamic signal adaptation
 * - Problem detection enhancement
 * 
 * Supports:
 * - TorchScript (.pt) models
 * - ONNX models (via conversion)
 * - Real-time inference
 * - Model hot-swapping
 */
class NeuralNetworkWrapper
{
public:
    //==========================================================================
    enum class ModelType
    {
        Unmasking,          // Multi-track unmasking model
        ProfileExtended,    // Extended profile classification
        DynamicAdaptive,    // Adaptive processing for extreme dynamics
        ProblemDetection,   // Enhanced problem detection
        Custom              // User-provided model
    };
    
    struct ModelInfo
    {
        ModelType type = ModelType::Custom;
        juce::String modelPath;
        juce::String modelName;
        std::vector<int> inputShape;        // Input tensor shape
        std::vector<int> outputShape;       // Output tensor shape
        bool isLoaded = false;
        juce::int64 loadTime = 0;
    };
    
    struct InferenceResult
    {
        bool success = false;
        std::vector<float> output;          // Model output (flattened)
        float inferenceTimeMs = 0.0f;       // Inference time in milliseconds
        juce::String errorMessage;
    };
    
    //==========================================================================
    NeuralNetworkWrapper();
    ~NeuralNetworkWrapper();
    
    //==========================================================================
    // Model Management
    bool loadModel(const juce::File& modelFile, ModelType type = ModelType::Custom);
    bool loadModel(const juce::String& modelPath, ModelType type = ModelType::Custom);
    void unloadModel();
    bool isModelLoaded() const { return currentModel.isLoaded; }
    ModelInfo getModelInfo() const { return currentModel; }
    
    //==========================================================================
    // Inference
    InferenceResult runInference(const std::vector<float>& input);
    InferenceResult runInference(const std::vector<std::vector<float>>& input);  // 2D input
    
    //==========================================================================
    // Online Learning (Fine-tuning)
    struct TrainingConfig
    {
        float learningRate = 0.001f;
        int batchSize = 32;
        int maxEpochs = 10;
        float validationSplit = 0.2f;
        bool useGradientClipping = true;
        float gradientClipValue = 1.0f;
    };
    
    struct TrainingSample
    {
        std::vector<float> input;
        std::vector<float> target;          // Expected output
        float weight = 1.0f;                // Sample weight for training
    };
    
    bool startOnlineTraining(const std::vector<TrainingSample>& samples, 
                            const TrainingConfig& config);
    void stopOnlineTraining();
    bool isTraining() const { return isTrainingActive.load(); }
    float getTrainingProgress() const { return trainingProgress.load(); }
    
    //==========================================================================
    // Model Export/Save
    bool saveFineTunedModel(const juce::File& outputPath);
    bool exportToONNX(const juce::File& outputPath);
    
    //==========================================================================
    // Configuration
    void setUseGPU(bool useGPU) { useGPUAcceleration = useGPU; }
    bool getUseGPU() const { return useGPUAcceleration.load(); }
    
    void setInferenceThreads(int numThreads) { inferenceThreads = numThreads; }
    int getInferenceThreads() const { return inferenceThreads; }
    
    void setMaxBatchSize(int maxSize) { maxBatchSize = maxSize; }
    int getMaxBatchSize() const { return maxBatchSize; }
    
    //==========================================================================
    // Utility
    static juce::String getModelTypeName(ModelType type);
    static bool isTorchAvailable();
    static juce::String getTorchVersion();
    
private:
    //==========================================================================
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> pImpl;
    // Serialize model load and inference across UI/worker threads (never call from audio thread)
    std::mutex inferenceMutex;
    
    //==========================================================================
    // State
    ModelInfo currentModel;
    mutable std::mutex modelMutex;
    
    std::atomic<bool> useGPUAcceleration{false};
    std::atomic<int> inferenceThreads{1};
    std::atomic<int> maxBatchSize{64};
    
    std::atomic<bool> isTrainingActive{false};
    std::atomic<float> trainingProgress{0.0f};
    
    // Model cache
    juce::int64 lastInferenceTime = 0;
    static constexpr juce::int64 inferenceCacheTimeMs = 16;  // ~60 FPS
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralNetworkWrapper)
};

