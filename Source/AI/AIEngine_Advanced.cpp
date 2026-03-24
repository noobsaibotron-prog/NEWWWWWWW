#include "AIEngine.h"
#include "../Utils/Logger.h"

//==============================================================================
// Advanced AI Systems Integration
//==============================================================================

void AIEngine::updateMultiTrackAnalysis(const juce::String& trackId, const std::vector<float>& spectrum)
{
    if (!enableMultiTrackUnmasking || !multiTrackUnmasking)
        return;

    if (trackId.isEmpty())
    {
        AIEQ_LOG_WARNING("AIEngine::updateMultiTrackAnalysis() — empty trackId, skipping");
        return;
    }

    if (spectrum.empty())
    {
        AIEQ_LOG_WARNING("AIEngine::updateMultiTrackAnalysis() — empty spectrum for track: " + trackId);
        return;
    }

    multiTrackUnmasking->updateTrackSpectrum(trackId, spectrum);
    multiTrackUnmasking->setTrackActive(trackId, true);
}

std::vector<MultiTrackUnmasking::UnmaskingCorrection> AIEngine::getUnmaskingCorrections(double sampleRate)
{
    if (!enableMultiTrackUnmasking || !multiTrackUnmasking)
        return {};

    if (sampleRate <= 0.0)
    {
        AIEQ_LOG_WARNING("AIEngine::getUnmaskingCorrections() — invalid sampleRate: " + juce::String(sampleRate));
        return {};
    }

    return multiTrackUnmasking->generateCorrections(sampleRate, sensitivity);
}

//==============================================================================
void AIEngine::updateAdaptiveAnalysis(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (!enableAdaptiveProcessing || !adaptiveEngine)
        return;

    if (buffer.getNumSamples() == 0 || sampleRate <= 0.0)
        return;

    const auto analysis = adaptiveEngine->analyzeSignal(buffer, sampleRate);
    const auto config   = adaptiveEngine->getAdaptiveConfig(analysis);
    adaptiveEngine->applyAdaptiveConfig(*this, config);
}

AdaptiveAIEngine::AdaptiveConfig AIEngine::getCurrentAdaptiveConfig() const
{
    if (!enableAdaptiveProcessing || !adaptiveEngine)
        return AdaptiveAIEngine::AdaptiveConfig{};

    // Return config based on a neutral signal analysis as a best-effort snapshot.
    // A running average of recent configs would be more accurate but requires
    // storing the latest analysis result — deferred to a future refactor.
    const AdaptiveAIEngine::SignalAnalysis neutralAnalysis{};
    return adaptiveEngine->getAdaptiveConfig(neutralAnalysis);
}

//==============================================================================
bool AIEngine::loadNeuralModel(const juce::File& modelFile, NeuralNetworkWrapper::ModelType type)
{
    if (!enableNeuralNetworks || !neuralNetwork)
    {
        AIEQ_LOG_WARNING("AIEngine::loadNeuralModel() — neural networks not enabled");
        return false;
    }

    if (!modelFile.existsAsFile())
    {
        AIEQ_LOG_WARNING("AIEngine::loadNeuralModel() — file not found: " + modelFile.getFullPathName());
        return false;
    }

    const bool ok = neuralNetwork->loadModel(modelFile, type);
    if (!ok)
        AIEQ_LOG_WARNING("AIEngine::loadNeuralModel() — failed to load: " + modelFile.getFullPathName());
    return ok;
}

NeuralNetworkWrapper::InferenceResult AIEngine::runNeuralInference(const std::vector<float>& input)
{
    NeuralNetworkWrapper::InferenceResult result;

    if (!enableNeuralNetworks || !neuralNetwork)
    {
        result.success = false;
        result.errorMessage = "Neural networks not enabled";
        return result;
    }

    if (!neuralNetwork->isModelLoaded())
    {
        result.success = false;
        result.errorMessage = "No model loaded — call loadNeuralModel() first";
        return result;
    }

    if (input.empty())
    {
        result.success = false;
        result.errorMessage = "Empty input vector";
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

    if (input.empty() || target.empty())
    {
        AIEQ_LOG_WARNING("AIEngine::addLearningSample() — empty input or target, skipping");
        return;
    }

    OnlineLearningSystem::LearningSample sample;
    sample.input     = input;
    sample.target    = target;
    sample.source    = source.isEmpty() ? juce::String("auto") : source;
    sample.timestamp = juce::Time::currentTimeMillis();
    sample.weight    = 1.0f;

    onlineLearning->addSample(sample);
}

void AIEngine::performOnlineLearningUpdate()
{
    if (!enableOnlineLearning || !onlineLearning || !neuralNetwork)
        return;

    if (!neuralNetwork->isModelLoaded())
    {
        // Model not loaded yet — update silently deferred
        return;
    }

    onlineLearning->performUpdate(*neuralNetwork);
}
