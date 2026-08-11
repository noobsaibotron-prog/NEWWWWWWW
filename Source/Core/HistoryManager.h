#pragma once

/**
 * HistoryManager - Thread-Safe Undo/Redo for EQ States
 * 
 * Provides undo/redo functionality without blocking the audio thread.
 * Uses a command queue to defer state changes to the message thread.
 * 
 * Architecture:
 * - Audio thread never accesses history directly
 * - GUI thread manages history stack
 * - Parameter changes are applied through APVTS (host-safe)
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include <vector>
#include <cmath>

namespace AIEQCore
{

static constexpr int kMaxHistorySize = 20;

/**
 * One normalized host parameter value. Keeping the stable parameter ID makes
 * snapshots forward/backward tolerant: unknown parameters are ignored and new
 * parameters are captured automatically without extending this class.
 */
struct ParameterValueSnapshot
{
    juce::String parameterID;
    float normalizedValue = 0.0f;
};

/**
 * Complete EQ state snapshot for undo/redo
 */
struct EQStateSnapshot
{
    std::vector<ParameterValueSnapshot> parameters;
    juce::String description;
    juce::int64 timestamp = 0;
    
    EQStateSnapshot() = default;
    
    EQStateSnapshot(const juce::String& desc)
        : description(desc), timestamp(juce::Time::currentTimeMillis())
    {
    }
};

/**
 * HistoryManager - Manages undo/redo state
 * 
 * Must be called from message thread only.
 * Uses APVTS for parameter changes to ensure host compatibility.
 */
class HistoryManager
{
public:
    HistoryManager() = default;
    
    // Non-copyable
    HistoryManager(const HistoryManager&) = delete;
    HistoryManager& operator=(const HistoryManager&) = delete;
    
    /**
     * Initialize with APVTS reference
     */
    void initialize(juce::AudioProcessorValueTreeState& stateIn,
                    juce::AudioProcessor& processorIn)
    {
        apvts = &stateIn;
        processor = &processorIn;
    }
    
    /**
     * Push current state onto undo stack
     * Call this BEFORE making changes (e.g., before applying AI corrections)
     * 
     * @param description Human-readable description of the action
     */
    void pushUndoState(const juce::String& description)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        
        if (apvts == nullptr)
            return;
        
        EQStateSnapshot snapshot(description);
        captureCurrentState(snapshot);
        
        undoStack.push_back(snapshot);
        
        // Limit stack size
        while (undoStack.size() > kMaxHistorySize)
        {
            undoStack.pop_front();
        }
        
        // Clear redo stack when new action is performed
        redoStack.clear();
    }
    
    /**
     * Undo the last action
     */
    void undo()
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        
        if (!canUndo() || apvts == nullptr)
            return;
        
        // Save current state to redo stack
        EQStateSnapshot currentSnapshot("Redo: " + undoStack.back().description);
        captureCurrentState(currentSnapshot);
        redoStack.push_back(currentSnapshot);
        while (redoStack.size() > kMaxHistorySize)
            redoStack.pop_front();
        
        // Restore previous state
        const auto& previousState = undoStack.back();
        restoreState(previousState);
        
        undoStack.pop_back();
    }
    
    /**
     * Redo the last undone action
     */
    void redo()
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        
        if (!canRedo() || apvts == nullptr)
            return;
        
        // Save current state to undo stack
        EQStateSnapshot currentSnapshot(redoStack.back().description);
        captureCurrentState(currentSnapshot);
        undoStack.push_back(currentSnapshot);
        while (undoStack.size() > kMaxHistorySize)
            undoStack.pop_front();
        
        // Restore redo state
        const auto& redoState = redoStack.back();
        restoreState(redoState);
        
        redoStack.pop_back();
    }
    
    /**
     * Check if undo is available
     */
    [[nodiscard]] bool canUndo() const noexcept
    {
        return !undoStack.empty();
    }
    
    /**
     * Check if redo is available
     */
    [[nodiscard]] bool canRedo() const noexcept
    {
        return !redoStack.empty();
    }
    
    /**
     * Get description of next undo action
     */
    [[nodiscard]] juce::String getUndoDescription() const
    {
        if (undoStack.empty())
            return "Nothing to undo";
        return "Undo: " + undoStack.back().description;
    }
    
    /**
     * Get description of next redo action
     */
    [[nodiscard]] juce::String getRedoDescription() const
    {
        if (redoStack.empty())
            return "Nothing to redo";
        return redoStack.back().description;
    }
    
    /**
     * Get undo stack size
     */
    [[nodiscard]] int getUndoStackSize() const noexcept
    {
        return static_cast<int>(undoStack.size());
    }
    
    /**
     * Get redo stack size
     */
    [[nodiscard]] int getRedoStackSize() const noexcept
    {
        return static_cast<int>(redoStack.size());
    }
    
    /**
     * Clear all history
     */
    void clearHistory()
    {
        undoStack.clear();
        redoStack.clear();
    }

private:
    /**
     * Capture current EQ state from APVTS
     */
    void captureCurrentState(EQStateSnapshot& snapshot)
    {
        if (apvts == nullptr || processor == nullptr)
            return;

        snapshot.parameters.clear();
        const auto& hostParameters = processor->getParameters();
        snapshot.parameters.reserve(static_cast<size_t>(hostParameters.size()));

        for (auto* parameter : hostParameters)
        {
            auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
            if (withID == nullptr)
                continue;

            const float normalized = parameter->getValue();
            if (!std::isfinite(normalized))
                continue;

            snapshot.parameters.push_back({ withID->getParameterID(),
                                            juce::jlimit(0.0f, 1.0f, normalized) });
        }
    }
    
    /**
     * Restore EQ state to APVTS
     * Uses beginChangeGesture/endChangeGesture for proper host undo integration
     */
    void restoreState(const EQStateSnapshot& snapshot)
    {
        if (apvts == nullptr)
            return;
        
        for (const auto& saved : snapshot.parameters)
        {
            if (!std::isfinite(saved.normalizedValue))
                continue;

            if (auto* param = apvts->getParameter(saved.parameterID))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(
                    juce::jlimit(0.0f, 1.0f, saved.normalizedValue));
                param->endChangeGesture();
            }
        }
    }
    
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    juce::AudioProcessor* processor = nullptr;
    std::deque<EQStateSnapshot> undoStack;
    std::deque<EQStateSnapshot> redoStack;
};

} // namespace AIEQCore
