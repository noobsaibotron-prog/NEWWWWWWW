#include "OnlineLearningSystem.h"
#include <algorithm>
#include <random>

//==============================================================================
OnlineLearningSystem::OnlineLearningSystem()
{
    currentConfig = LearningConfig();
}

//==============================================================================
void OnlineLearningSystem::addSample(const LearningSample& sample)
{
    if (!isEnabled.load())
        return;
    
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    LearningSample sampleCopy = sample;
    if (sampleCopy.timestamp == 0)
        sampleCopy.timestamp = juce::Time::currentTimeMillis();
    
    replayBuffer.push_back(sampleCopy);
    
    // Maintain buffer size
    while (static_cast<int>(replayBuffer.size()) > currentConfig.replayBufferSize)
    {
        replayBuffer.pop_front();
    }
    
    stats.totalSamples++;
    stats.samplesInBuffer = static_cast<int>(replayBuffer.size());
}

void OnlineLearningSystem::addSamples(const std::vector<LearningSample>& samples)
{
    for (const auto& sample : samples)
    {
        addSample(sample);
    }
}

void OnlineLearningSystem::clearSamples()
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    replayBuffer.clear();
    stats.samplesInBuffer = 0;
}

//==============================================================================
void OnlineLearningSystem::startLearning(NeuralNetworkWrapper& model, const LearningConfig& config)
{
    if (!isEnabled.load())
        return;
    
    if (isLearning.load())
        return;
    
    currentConfig = config;
    isLearning = true;
    stats.isTraining = true;
    
    // Start training on model
    std::vector<NeuralNetworkWrapper::TrainingSample> trainingSamples;
    
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        for (const auto& sample : replayBuffer)
        {
            NeuralNetworkWrapper::TrainingSample ts;
            ts.input = sample.input;
            ts.target = sample.target;
            ts.weight = sample.weight;
            trainingSamples.push_back(ts);
        }
    }
    
    if (!trainingSamples.empty())
    {
        NeuralNetworkWrapper::TrainingConfig trainConfig;
        trainConfig.learningRate = config.learningRate;
        trainConfig.batchSize = config.batchSize;
        trainConfig.maxEpochs = 1;  // Online learning: single epoch per update
        trainConfig.validationSplit = 0.0f;  // No validation split for online learning
        trainConfig.useGradientClipping = config.useGradientAccumulation;
        trainConfig.gradientClipValue = config.gradientClipValue;
        
        model.startOnlineTraining(trainingSamples, trainConfig);
    }
}

void OnlineLearningSystem::stopLearning()
{
    isLearning = false;
    stats.isTraining = false;
    stats.lastUpdateTime = 0.0f;
}

void OnlineLearningSystem::performUpdate(NeuralNetworkWrapper& model)
{
    if (!isEnabled.load() || !isLearning.load())
        return;
    
    auto currentTime = juce::Time::currentTimeMillis();
    auto timeSinceLastUpdate = (currentTime - lastUpdateTime) / 1000.0;
    
    if (timeSinceLastUpdate < currentConfig.minUpdateInterval)
        return;  // Too soon for another update
    
    // Sample from buffer
    std::vector<LearningSample> samples;
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        samples = sampleFromBuffer(juce::jmin(currentConfig.maxSamplesPerUpdate, 
                                             static_cast<int>(replayBuffer.size())));
    }
    
    if (samples.empty())
        return;
    
    // Convert to training samples
    std::vector<NeuralNetworkWrapper::TrainingSample> trainingSamples;
    for (const auto& sample : samples)
    {
        NeuralNetworkWrapper::TrainingSample ts;
        ts.input = sample.input;
        ts.target = sample.target;
        ts.weight = sample.weight;
        trainingSamples.push_back(ts);
    }
    
    // Perform training update
    NeuralNetworkWrapper::TrainingConfig trainConfig;
    trainConfig.learningRate = currentConfig.learningRate;
    trainConfig.batchSize = currentConfig.batchSize;
    trainConfig.maxEpochs = 1;
    trainConfig.validationSplit = 0.0f;
    trainConfig.useGradientClipping = currentConfig.useGradientAccumulation;
    trainConfig.gradientClipValue = currentConfig.gradientClipValue;
    
    if (model.startOnlineTraining(trainingSamples, trainConfig))
    {
        lastUpdateTime = currentTime;
        stats.updatesPerformed++;
        updateStats(0.0f);  // TODO: Get actual loss from model
    }
}

//==============================================================================
void OnlineLearningSystem::addUserFeedback(const std::vector<float>& input,
                                          const std::vector<float>& userCorrection,
                                          float confidence)
{
    LearningSample sample;
    sample.input = input;
    sample.target = userCorrection;
    sample.weight = confidence;
    sample.source = "user";
    sample.timestamp = juce::Time::currentTimeMillis();
    
    addSample(sample);
}

void OnlineLearningSystem::addAutoFeedback(const std::vector<float>& input,
                                          const std::vector<float>& predictedOutput,
                                          const std::vector<float>& actualOutput,
                                          float confidence)
{
    // Create target as difference between predicted and actual
    std::vector<float> target = actualOutput;
    
    // Weight by confidence
    LearningSample sample;
    sample.input = input;
    sample.target = target;
    sample.weight = confidence;
    sample.source = "auto";
    sample.timestamp = juce::Time::currentTimeMillis();
    
    addSample(sample);
}

//==============================================================================
OnlineLearningSystem::LearningStats OnlineLearningSystem::getStats() const
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    LearningStats statsCopy = stats;
    statsCopy.samplesInBuffer = static_cast<int>(replayBuffer.size());
    return statsCopy;
}

void OnlineLearningSystem::resetStats()
{
    std::lock_guard<std::mutex> lock(bufferMutex);
    stats = LearningStats();
    stats.samplesInBuffer = static_cast<int>(replayBuffer.size());
}

//==============================================================================
bool OnlineLearningSystem::saveModelCheckpoint(const juce::File& path, NeuralNetworkWrapper& model)
{
    // TODO: Implement checkpoint saving
    return model.saveFineTunedModel(path);
}

bool OnlineLearningSystem::loadModelCheckpoint(const juce::File& path, NeuralNetworkWrapper& model)
{
    // TODO: Implement checkpoint loading
    return model.loadModel(path);
}

//==============================================================================
std::vector<OnlineLearningSystem::LearningSample> OnlineLearningSystem::sampleFromBuffer(int count) const
{
    std::vector<LearningSample> samples;
    
    if (replayBuffer.empty() || count <= 0)
        return samples;
    
    // Random sampling from buffer
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(replayBuffer.size()) - 1);
    
    samples.reserve(static_cast<size_t>(count));
    
    for (int i = 0; i < count && i < static_cast<int>(replayBuffer.size()); ++i)
    {
        int index = dis(gen);
        samples.push_back(replayBuffer[index]);
    }
    
    return samples;
}

void OnlineLearningSystem::updateStats(float loss)
{
    // Update running average of loss
    if (stats.updatesPerformed == 0)
    {
        stats.averageLoss = loss;
    }
    else
    {
        float alpha = 0.1f;  // Exponential moving average
        stats.averageLoss = alpha * loss + (1.0f - alpha) * stats.averageLoss;
    }
}

