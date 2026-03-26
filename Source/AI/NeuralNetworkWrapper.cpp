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
        originalModelPath = modelFile;
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
        if (samples.empty())
            return false;

        // TFLite C API (v2.x) does not expose backpropagation or fine-tuning of
        // FlatBuffer models at runtime. Gradient-based training requires the
        // TFLite Model Maker Python library or a custom training delegate.
        // We log the intent so the caller knows why this returns false, and the
        // outer AIEngine will fall back to heuristic adjustments instead.
        juce::ignoreUnused(config);
#if defined(AIEQ_ENABLE_TFLITE)
        AIEQ_LOG_WARNING("NeuralNetworkWrapper: TFLite C API does not support online "
                         "fine-tuning. Use TFLite Model Maker offline to retrain.");
#else
        AIEQ_LOG_WARNING("NeuralNetworkWrapper: No ML backend compiled in — "
                         "training unavailable. Build with AIEQ_ENABLE_TFLITE to enable inference.");
#endif
        trainingActive.store(false, std::memory_order_release);
        return false;
    }

    void stopTraining()
    {
        trainingActive.store(false, std::memory_order_release);
    }

    bool saveModel(const juce::File& path)
    {
#if defined(AIEQ_ENABLE_TFLITE)
        // TFLite C API provides no API to serialise an interpreter back to a FlatBuffer.
        // The best we can do is copy the original file that was loaded.
        if (!model || !originalModelPath.existsAsFile())
        {
            AIEQ_LOG_WARNING("NeuralNetworkWrapper::saveModel() — no model file to copy");
            return false;
        }
        const bool ok = originalModelPath.copyFileTo(path);
        if (!ok)
            AIEQ_LOG_WARNING("NeuralNetworkWrapper::saveModel() — file copy failed: " + path.getFullPathName());
        return ok;
#else
        juce::ignoreUnused(path);
        return false;
#endif
    }

#if defined(AIEQ_ENABLE_TFLITE)
    TfLiteModel* model = nullptr;
    TfLiteInterpreterOptions* options = nullptr;
    TfLiteInterpreter* interpreter = nullptr;
#endif

private:
    juce::File originalModelPath;
    std::atomic<bool> trainingActive { false };
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
    // C++17 scoped_lock: acquires both mutexes atomically, deadlock-free
    std::scoped_lock lock(inferenceMutex, modelMutex);

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
    // C++17 scoped_lock: acquires both mutexes atomically, deadlock-free
    std::scoped_lock lock(inferenceMutex, modelMutex);

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
    // Runtime ONNX export requires libtorch's onnx::export() or the ONNX Runtime SDK.
    // TFLite FlatBuffer models can be converted offline via the 'tf2onnx' Python tool:
    //   python -m tf2onnx.convert --tflite model.tflite --output model.onnx
    // In-plugin export is not supported without a torch build.
    juce::ignoreUnused(outputPath);
    AIEQ_LOG_WARNING("NeuralNetworkWrapper::exportToONNX() is not supported at runtime. "
                     "Convert the TFLite model offline with tf2onnx.");
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
#if defined(AIEQ_ENABLE_TFLITE)
    return true;   // TFLite C API compiled in — inference is available
#else
    return false;  // No ML backend compiled; build with AIEQ_ENABLE_TFLITE
#endif
}

juce::String NeuralNetworkWrapper::getTorchVersion()
{
#if defined(AIEQ_ENABLE_TFLITE)
    // TFLite C API does not expose a runtime version string.
    // We return a compile-time label so callers can reason about the backend.
    return "TFLite C API (version embedded in flatbuffer)";
#else
    return "Unavailable — build with -DAIEQ_ENABLE_TFLITE=ON to enable ML inference";
#endif
}

