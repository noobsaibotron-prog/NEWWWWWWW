#include "Logger.h"
#include <algorithm>

//==============================================================================
AIEQLogger::AIEQLogger()
{
    // Default log file path (user's temp directory)
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    logFilePath = tempDir.getChildFile("AIEqualizerPro.log").getFullPathName();
    
    // Try to open log file
    if (logToFile)
    {
        logFile.open(logFilePath.toRawUTF8(), std::ios::app);
        if (!logFile.is_open())
        {
            logToFile = false;  // Graceful degradation
        }
    }
}

AIEQLogger::~AIEQLogger()
{
    if (logFile.is_open())
        logFile.close();
}

void AIEQLogger::log(Level level, const juce::String& message, const juce::String& component)
{
    if (level < minLevel)
        return;
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    juce::String logLine = getTimestamp() + " [" + levelToString(level) + "]";
    if (component.isNotEmpty())
        logLine += " [" + component + "]";
    logLine += " " + message;
    
    // Add to recent logs
    recentLogs.push_back(logLine);
    if (recentLogs.size() > maxRecentLogs)
        recentLogs.erase(recentLogs.begin());
    
    // Write to console
    if (logToConsole)
    {
        juce::Logger::writeToLog(logLine);
    }
    
    // Write to file
    if (logToFile && logFile.is_open())
    {
        writeToFile(logLine);
    }
}

void AIEQLogger::writeToFile(const juce::String& logLine)
{
    if (logFile.is_open())
    {
        logFile << logLine << std::endl;
        logFile.flush();
        
        // Rotate log if it gets too large (>10MB)
        auto file = juce::File(logFilePath);
        if (file.exists() && file.getSize() > 10 * 1024 * 1024)
        {
            logFile.close();
            auto backupPath = logFilePath + ".old";
            file.moveFileTo(juce::File(backupPath));
            logFile.open(logFilePath.toRawUTF8(), std::ios::app);
        }
    }
}

juce::String AIEQLogger::levelToString(Level level) const
{
    switch (level)
    {
        case Level::Error:   return "ERROR";
        case Level::Warning: return "WARN";
        case Level::Info:    return "INFO";
        case Level::Debug:   return "DEBUG";
        default:             return "UNKNOWN";
    }
}

juce::String AIEQLogger::getTimestamp() const
{
    auto now = juce::Time::getCurrentTime();
    return now.toString(true, true, true, true);
}

void AIEQLogger::logFromRTThread(Level level, const char* msg, const char* component) noexcept
{
    if (level < minLevel || msg == nullptr)
        return;

    RTLogMessage m {};
    m.level = level;
    m.timestamp = static_cast<std::int64_t>(juce::Time::getMillisecondCounterHiRes());

    std::strncpy(m.message, msg, sizeof(m.message) - 1);
    m.message[sizeof(m.message) - 1] = '\0';

    if (component != nullptr)
    {
        std::strncpy(m.component, component, sizeof(m.component) - 1);
        m.component[sizeof(m.component) - 1] = '\0';
    }
    else
    {
        m.component[0] = '\0';
    }

    // FIX #3: Removed SpinLock - SPSCQueue is already lock-free for single producer.
    // SpinLock causes priority inversion and busy-wait CPU waste in RT context.
    // The queue is designed for exactly one producer (RT thread) and one consumer (flush thread).
    if (!rtQueue.tryPush(m))
        droppedRTMessages.fetch_add(1, std::memory_order_relaxed);
}

void AIEQLogger::flushRTLogs()
{
    RTLogMessage m;
    while (rtQueue.tryPop(m))
    {
        log(m.level, juce::String(m.message), juce::String(m.component));
    }
}

std::vector<juce::String> AIEQLogger::getRecentLogs(int maxEntries) const
{
    std::lock_guard<std::mutex> lock(logMutex);
    
    int startIdx = juce::jmax(0, static_cast<int>(recentLogs.size()) - maxEntries);
    std::vector<juce::String> result;
    result.reserve(static_cast<size_t>(maxEntries));
    
    for (int i = startIdx; i < static_cast<int>(recentLogs.size()); ++i)
    {
        result.push_back(recentLogs[static_cast<size_t>(i)]);
    }
    
    return result;
}

