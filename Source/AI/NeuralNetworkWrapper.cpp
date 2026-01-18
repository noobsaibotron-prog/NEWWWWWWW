#include "NeuralNetworkWrapper.h"
#include "../Utils/Logger.h"
#include <fstream>
#include <atomic>
#include <cstring>

#if defined(AIEQ_ENABLE_TFLITE)
#include <tensorflow/lite/c/c_api.h>
#endif

//==============================================================================
// Internal Implementation (will use libtorch if available)
class NeuralNetworkWrapper::Impl
{
public:
    Impl() {}
    ~Impl() {}
    
    bool loadModel(const juce::File& modelFile)
    {
#if defined(AIEQ_ENABLE_TFLITE)
        unloadModel();

        model = TfLiteModelCreateFromFile(modelFile.getFullPathName().toStdString().c_str());
        if (!model)
            return false;

        options = TfLiteInterpreterOptionsCreate();
        if (!options)
        {
            unloadModel();
            return false;
        }

        // Use 1 thread by default (safe on audio machines)
        TfLiteInterpreterOptionsSetNumThreads(options, 1);

        interpreter = TfLiteInterpreterCreate(model, options);
        if (!interpreter)
        {
            unloadModel();
            return false;
        }

        if (TfLiteInterpreterAllocateTensors(interpreter) != kTfLiteOk)
        {
            unloadModel();
            return false;
        }

        return true;
#else
        juce::ignoreUnused(modelFile);
        return false;
#endif
    }
    
    void unloadModel()
    {
#if defined(AIEQ_ENABLE_TFLITE)
        if (interpreter)
        {
            TfLiteInterpreterDelete(interpreter);
            interpreter = nullptr;
        }
        if (options)
        {
            TfLiteInterpreterOptionsDelete(options);
            options = nullptr;
        }
        if (model)
        {
            TfLiteModelDelete(model);
            model = nullptr;
        }
#endif
    }
    
    InferenceResult runInference(const std::vector<float>& input)
    {
        InferenceResult result;

#if defined(AIEQ_ENABLE_TFLITE)
        if (!interpreter)
        {
            result.success = false;
            result.errorMessage = "TFLite interpreter not available";
            return result;
        }

        const TfLiteTensor* inputTensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
        if (!inputTensor)
        {
            result.success = false;
            result.errorMessage = "TFLite input tensor null";
            return result;
        }

        const size_t tensorBytes = TfLiteTensorByteSize(inputTensor);
        const size_t tensorSize = tensorBytes / sizeof(float);
        if (input.size() > tensorSize)
        {
            result.success = false;
            result.errorMessage = "Input size exceeds tensor capacity";
            return result;
        }

        float* inputData = reinterpret_cast<float*>(TfLiteTensorData(inputTensor));
        if (!inputData)
        {
            result.success = false;
            result.errorMessage = "TFLite input data null";
            return result;
        }

        std::fill(inputData, inputData + tensorSize, 0.0f);
        std::memcpy(inputData, input.data(), input.size() * sizeof(float));

        if (TfLiteInterpreterInvoke(interpreter) != kTfLiteOk)
        {
            result.success = false;
            result.errorMessage = "TFLite inference failed";
            return result;
        }

        const int outCount = TfLiteInterpreterGetOutputTensorCount(interpreter);
        if (outCount <= 0)
        {
            result.success = false;
            result.errorMessage = "TFLite has no output tensors";
            return result;
        }

        const TfLiteTensor* outTensor = TfLiteInterpreterGetOutputTensor(interpreter, 0);
        if (!outTensor)
        {
            result.success = false;
            result.errorMessage = "TFLite output tensor invalid";
            return result;
        }

        float* outputData = reinterpret_cast<float*>(TfLiteTensorData(outTensor));
        if (!outputData)
        {
            result.success = false;
            result.errorMessage = "TFLite output data null";
            return result;
        }

        const int outDims = TfLiteTensorNumDims(outTensor);
        if (outDims <= 0)
        {
            result.success = false;
            result.errorMessage = "TFLite output tensor has no dims";
            return result;
        }

        int outSize = 1;
        for (int i = 0; i < outDims; ++i)
            outSize *= TfLiteTensorDim(outTensor, i);

        result.output.assign(outputData, outputData + outSize);
        result.success = true;
        return result;
#else
        juce::ignoreUnused(input);
        result.success = false;
        result.errorMessage = "Torch/TFLite backend not available";
        return result;
#endif
    }
    
    bool startTraining(const std::vector<TrainingSample>& samples, const TrainingConfig& config)
    {
        // TODO: Implement training
        return false;
    }
    
    void stopTraining()
    {
        // TODO: Implement training stop
    }
    
    bool saveModel(const juce::File& path)
    {
        // TODO: Implement model saving
        return false;
    }

#if defined(AIEQ_ENABLE_TFLITE)
private:
    TfLiteModel* model = nullptr;
    TfLiteInterpreterOptions* options = nullptr;
    TfLiteInterpreter* interpreter = nullptr;
#endif
};

//==============================================================================
NeuralNetworkWrapper::NeuralNetworkWrapper()
    : pImpl(std::make_unique<Impl>())
{
}

NeuralNetworkWrapper::~NeuralNetworkWrapper()
{
}

//==============================================================================
bool NeuralNetworkWrapper::loadModel(const juce::File& modelFile, ModelType type)
{
    std::lock_guard<std::mutex> lock(modelMutex);
    // Also serialize with inference mutex to prevent races with concurrent runInference
    std::lock_guard<std::mutex> infLock(inferenceMutex);
    
    if (!modelFile.existsAsFile())
        return false;
    
    currentModel.type = type;
    currentModel.modelPath = modelFile.getFullPathName();
    currentModel.modelName = modelFile.getFileNameWithoutExtension();
    currentModel.isLoaded = pImpl->loadModel(modelFile);
    currentModel.loadTime = juce::Time::currentTimeMillis();
    
    if (!currentModel.isLoaded)
    {
        static std::atomic<bool> warnedTorchUnavailable { false };
        const bool firstWarning = !warnedTorchUnavailable.exchange(true, std::memory_order_acq_rel);
        if (firstWarning)
        {
            AIEQ_LOG_WARNING("Neural model not loaded (Torch backend unavailable). "
                             "AI features will fall back to classical ML.");
        }
    }
    
    return currentModel.isLoaded;
}

bool NeuralNetworkWrapper::loadModel(const juce::String& modelPath, ModelType type)
{
    return loadModel(juce::File(modelPath), type);
}

void NeuralNetworkWrapper::unloadModel()
{
    std::lock_guard<std::mutex> lock(modelMutex);
    
    pImpl->unloadModel();
    currentModel.isLoaded = false;
    currentModel.modelPath = juce::String();
    currentModel.modelName = juce::String();
}

//==============================================================================
NeuralNetworkWrapper::InferenceResult NeuralNetworkWrapper::runInference(const std::vector<float>& input)
{
    // Serialize inference to guard non-thread-safe interpreters (TFLite/libtorch)
    std::lock_guard<std::mutex> infLock(inferenceMutex);
    std::lock_guard<std::mutex> lock(modelMutex);
    
    if (!currentModel.isLoaded)
    {
        InferenceResult result;
        result.success = false;
        result.errorMessage = "No model loaded";
        
        static std::atomic<bool> warnedNoModel { false };
        const bool firstWarning = !warnedNoModel.exchange(true, std::memory_order_acq_rel);
        if (firstWarning)
        {
            AIEQ_LOG_WARNING("Neural inference skipped: no model loaded. "
                             "Load a Torch/TFLite model or disable NN features.");
        }
        return result;
    }
    
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    auto result = pImpl->runInference(input);
    auto endTime = juce::Time::getMillisecondCounterHiRes();
    
    result.inferenceTimeMs = static_cast<float>(endTime - startTime);
    return result;
}

NeuralNetworkWrapper::InferenceResult NeuralNetworkWrapper::runInference(const std::vector<std::vector<float>>& input)
{
    // Flatten 2D input to 1D
    std::vector<float> flattened;
    for (const auto& row : input)
    {
        flattened.insert(flattened.end(), row.begin(), row.end());
    }
    
    return runInference(flattened);
}

//==============================================================================
bool NeuralNetworkWrapper::startOnlineTraining(const std::vector<TrainingSample>& samples,
                                               const TrainingConfig& config)
{
    std::lock_guard<std::mutex> lock(modelMutex);
    
    if (!currentModel.isLoaded)
        return false;
    
    if (isTrainingActive.load())
        return false;
    
    isTrainingActive = true;
    trainingProgress = 0.0f;
    
    return pImpl->startTraining(samples, config);
}

void NeuralNetworkWrapper::stopOnlineTraining()
{
    std::lock_guard<std::mutex> lock(modelMutex);
    
    pImpl->stopTraining();
    isTrainingActive = false;
    trainingProgress = 0.0f;
}

//==============================================================================
bool NeuralNetworkWrapper::saveFineTunedModel(const juce::File& outputPath)
{
    std::lock_guard<std::mutex> lock(modelMutex);
    
    if (!currentModel.isLoaded)
        return false;
    
    return pImpl->saveModel(outputPath);
}

bool NeuralNetworkWrapper::exportToONNX(const juce::File& outputPath)
{
    // TODO: Implement ONNX export
    return false;
}

//==============================================================================
juce::String NeuralNetworkWrapper::getModelTypeName(ModelType type)
{
    switch (type)
    {
        case ModelType::Unmasking: return "Unmasking";
        case ModelType::ProfileExtended: return "Profile Extended";
        case ModelType::DynamicAdaptive: return "Dynamic Adaptive";
        case ModelType::ProblemDetection: return "Problem Detection";
        case ModelType::Custom: return "Custom";
        default: return "Unknown";
    }
}

bool NeuralNetworkWrapper::isTorchAvailable()
{
    // TODO: Check if libtorch is linked
    return false;
}

juce::String NeuralNetworkWrapper::getTorchVersion()
{
    // TODO: Return actual torch version
    return "Not Available";
}

