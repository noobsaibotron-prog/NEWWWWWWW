#pragma once

#include <juce_core/juce_core.h>
#include <mutex>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <atomic>

#include "../Core/LockFreeStructures.h"

//==============================================================================
/**
 * Centralized logging system for AI Equalizer Pro
 * 
 * Features:
 * - Thread-safe logging
 * - Multiple log levels (Error, Warning, Info, Debug)
 * - File logging with rotation
 * - Console output
 * - Graceful degradation if file system unavailable
 */
class AIEQLogger
{
public:
    enum class Level
    {
        Error = 0,
        Warning,
        Info,
        Debug
    };
    
    static AIEQLogger& getInstance()
    {
        static AIEQLogger instance;
        return instance;
    }
    
    void log(Level level, const juce::String& message, const juce::String& component = {});
    
    void logError(const juce::String& message, const juce::String& component = {})
    {
        log(Level::Error, message, component);
    }
    
    void logWarning(const juce::String& message, const juce::String& component = {})
    {
        log(Level::Warning, message, component);
    }
    
    void logInfo(const juce::String& message, const juce::String& component = {})
    {
        log(Level::Info, message, component);
    }
    
    void logDebug(const juce::String& message, const juce::String& component = {})
    {
        log(Level::Debug, message, component);
    }
    
    // ML-specific logging
    void logMLError(const juce::String& message, const juce::String& modelName = {})
    {
        logError("ML Engine: " + message, modelName.isEmpty() ? "MLEngine" : modelName);
    }
    
    void logMLInfo(const juce::String& message, const juce::String& modelName = {})
    {
        logInfo("ML Engine: " + message, modelName.isEmpty() ? "MLEngine" : modelName);
    }
    
    // File loading errors
    void logFileError(const juce::String& filePath, const juce::String& reason)
    {
        logError("Failed to load file: " + filePath + " - " + reason, "FileIO");
    }
    
    // Configuration
    void setLogToFile(bool enable) { logToFile = enable; }
    void setLogToConsole(bool enable) { logToConsole = enable; }
    void setMinLevel(Level level) { minLevel = level; }
    void setLogFilePath(const juce::String& path) { logFilePath = path; }
    
    // Get recent log entries (for debugging UI)
    std::vector<juce::String> getRecentLogs(int maxEntries = 50) const;
    
    void clearLogs() { recentLogs.clear(); }

    // Real-time safe logging (non-blocking, lock-free)
    void logFromRTThread(Level level, const char* msg, const char* component = nullptr) noexcept;

    // Drain RT queue on non-RT thread (call from message/timer thread)
    void flushRTLogs();

private:
    AIEQLogger();
    ~AIEQLogger();
    
    void writeToFile(const juce::String& logLine);
    juce::String levelToString(Level level) const;
    juce::String getTimestamp() const;
    
    mutable std::mutex logMutex;
    bool logToFile = true;
    bool logToConsole = true;
    Level minLevel = Level::Info;
    juce::String logFilePath;
    std::ofstream logFile;
    std::vector<juce::String> recentLogs;
    static constexpr int maxRecentLogs = 100;

    struct RTLogMessage
    {
        Level level { Level::Info };
        char message[256] { 0 };
        char component[64] { 0 };
        std::int64_t timestamp { 0 };
    };

    // FIX #3: Removed SpinLock - SPSCQueue is inherently lock-free for SPSC scenarios.
    // SpinLock was causing priority inversion and CPU waste in RT context.
    AIEQCore::SPSCQueue<RTLogMessage, 256> rtQueue;
    std::atomic<uint32_t> droppedRTMessages { 0 };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIEQLogger)
};

// Convenience macros
#define AIEQ_LOG_ERROR(msg) AIEQLogger::getInstance().logError(msg, __FUNCTION__)
#define AIEQ_LOG_WARNING(msg) AIEQLogger::getInstance().logWarning(msg, __FUNCTION__)
#define AIEQ_LOG_INFO(msg) AIEQLogger::getInstance().logInfo(msg, __FUNCTION__)
#define AIEQ_LOG_DEBUG(msg) AIEQLogger::getInstance().logDebug(msg, __FUNCTION__)

#define AIEQ_LOG_ML_ERROR(msg) AIEQLogger::getInstance().logMLError(msg)
#define AIEQ_LOG_ML_INFO(msg) AIEQLogger::getInstance().logMLInfo(msg)

#define AIEQ_LOG_FILE_ERROR(path, reason) AIEQLogger::getInstance().logFileError(path, reason)

