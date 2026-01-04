#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include "NeuralNetworkWrapper.h"

//==============================================================================
/**
 * Online Learning System for Real-Time Model Updates
 * 
 * Implements incremental learning to update neural network models
 * based on user feedback and real-time audio analysis.
 * 
 * Features:
 * - Incremental gradient updates
 * - Experience replay buffer
 * - User feedback integration
 * - Automatic fine-tuning
 * - Model versioning
 */
class OnlineLearningSystem
{
public:
    //==========================================================================
    struct LearningSample
    {
        std::vector<float> input;          // Input features
        std::vector<float> target;          // Target output
        float weight = 1.0f;                 // Sample weight
        juce::int64 timestamp = 0;          // When sample was collected
        juce::String source;                 // Source of sample (user, auto, etc.)
    };
    
    struct LearningConfig
    {
        float learningRate = 0.0001f;       // Learning rate for online updates
        int replayBufferSize = 1000;        // Size of experience replay buffer
        int batchSize = 16;                  // Batch size for updates
        float minUpdateInterval = 1.0f;      // Minimum seconds between updates
        bool useGradientAccumulation = true; // Accumulate gradients over batches
        float gradientClipValue = 1.0f;      // Gradient clipping value
        int maxSamplesPerUpdate = 100;      // Maximum samples to use per update
    };
    
    struct LearningStats
    {
        int totalSamples = 0;
        int samplesInBuffer = 0;
        int updatesPerformed = 0;
        float averageLoss = 0.0f;
        float lastUpdateTime = 0.0f;
        bool isTraining = false;
    };
    
    //==========================================================================
    OnlineLearningSystem();
    ~OnlineLearningSystem() = default;
    
    //==========================================================================
    // Sample Collection
    void addSample(const LearningSample& sample);
    void addSamples(const std::vector<LearningSample>& samples);
    void clearSamples();
    
    //==========================================================================
    // Learning Control
    void startLearning(NeuralNetworkWrapper& model, const LearningConfig& config);
    void stopLearning();
    void performUpdate(NeuralNetworkWrapper& model);
    
    //==========================================================================
    // User Feedback Integration
    void addUserFeedback(const std::vector<float>& input,
                        const std::vector<float>& userCorrection,
                        float confidence = 1.0f);
    
    void addAutoFeedback(const std::vector<float>& input,
                        const std::vector<float>& predictedOutput,
                        const std::vector<float>& actualOutput,
                        float confidence = 0.5f);
    
    //==========================================================================
    // Configuration
    void setEnabled(bool enabled) { isEnabled = enabled; }
    bool getEnabled() const { return isEnabled; }
    
    void setConfig(const LearningConfig& config) { currentConfig = config; }
    LearningConfig getConfig() const { return currentConfig; }
    
    //==========================================================================
    // Statistics
    LearningStats getStats() const;
    void resetStats();
    
    //==========================================================================
    // Model Management
    bool saveModelCheckpoint(const juce::File& path, NeuralNetworkWrapper& model);
    bool loadModelCheckpoint(const juce::File& path, NeuralNetworkWrapper& model);
    
    juce::String getModelVersion() const { return modelVersion; }
    void setModelVersion(const juce::String& version) { modelVersion = version; }
    
private:
    //==========================================================================
    // Experience Replay Buffer
    std::deque<LearningSample> replayBuffer;
    mutable std::mutex bufferMutex;
    
    //==========================================================================
    // Learning State
    std::atomic<bool> isEnabled{true};
    std::atomic<bool> isLearning{false};
    LearningConfig currentConfig;
    LearningStats stats;
    
    juce::int64 lastUpdateTime = 0;
    juce::String modelVersion = "1.0.0";
    
    // Gradient accumulation
    std::vector<float> accumulatedGradients;
    int gradientAccumulationCount = 0;
    
    //==========================================================================
    // Internal methods
    std::vector<LearningSample> sampleFromBuffer(int count) const;
    void updateStats(float loss);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnlineLearningSystem)
};

