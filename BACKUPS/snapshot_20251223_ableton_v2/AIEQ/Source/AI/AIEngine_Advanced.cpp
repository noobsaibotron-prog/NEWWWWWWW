#include "AIEngine.h"

//==============================================================================
// Advanced AI Systems Integration
//==============================================================================

void AIEngine::updateMultiTrackAnalysis(const juce::String& trackId, const std::vector<float>& spectrum)
{
    if (!enableMultiTrackUnmasking || !multiTrackUnmasking)
        return;
    
    multiTrackUnmasking->updateTrackSpectrum(trackId, spectrum);
    multiTrackUnmasking->setTrackActive(trackId, true);
}

std::vector<MultiTrackUnmasking::UnmaskingCorrection> AIEngine::getUnmaskingCorrections(double sampleRate)
{
    if (!enableMultiTrackUnmasking || !multiTrackUnmasking)
        return {};
    
    return multiTrackUnmasking->generateCorrections(sampleRate, sensitivity);
}

//==============================================================================
void AIEngine::updateAdaptiveAnalysis(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (!enableAdaptiveProcessing || !adaptiveEngine)
        return;
    
    auto analysis = adaptiveEngine->analyzeSignal(buffer, sampleRate);
    auto config = adaptiveEngine->getAdaptiveConfig(analysis);
    
    // Apply adaptive configuration
    adaptiveEngine->applyAdaptiveConfig(*this, config);
}

AdaptiveAIEngine::AdaptiveConfig AIEngine::getCurrentAdaptiveConfig() const
{
    if (!enableAdaptiveProcessing || !adaptiveEngine)
    {
        AdaptiveAIEngine::AdaptiveConfig defaultConfig;
        return defaultConfig;
    }
    
    // Get latest analysis from adaptive engine
    // Note: This is a simplified version - in practice, you'd want to store the latest analysis
    AdaptiveAIEngine::SignalAnalysis dummyAnalysis;
    return adaptiveEngine->getAdaptiveConfig(dummyAnalysis);
}

//==============================================================================
bool AIEngine::loadNeuralModel(const juce::File& modelFile, NeuralNetworkWrapper::ModelType type)
{
    if (!enableNeuralNetworks || !neuralNetwork)
        return false;
    
    return neuralNetwork->loadModel(modelFile, type);
}

NeuralNetworkWrapper::InferenceResult AIEngine::runNeuralInference(const std::vector<float>& input)
{
    if (!enableNeuralNetworks || !neuralNetwork || !neuralNetwork->isModelLoaded())
    {
        NeuralNetworkWrapper::InferenceResult result;
        result.success = false;
        result.errorMessage = "Neural network not available or model not loaded";
        return result;
    }
    
    return neuralNetwork->runInference(input);
}

//==============================================================================
void AIEngine::addLearningSample(const std::vector<float>& input, const std::vector<float>& target,
                                 const juce::String& source)
{
    if (!enableOnlineLearning || !onlineLearning)
        return;
    
    OnlineLearningSystem::LearningSample sample;
    sample.input = input;
    sample.target = target;
    sample.source = source;
    sample.timestamp = juce::Time::currentTimeMillis();
    sample.weight = 1.0f;
    
    onlineLearning->addSample(sample);
}

void AIEngine::performOnlineLearningUpdate()
{
    if (!enableOnlineLearning || !onlineLearning || !neuralNetwork)
        return;
    
    if (!neuralNetwork->isModelLoaded())
        return;
    
    onlineLearning->performUpdate(*neuralNetwork);
}

