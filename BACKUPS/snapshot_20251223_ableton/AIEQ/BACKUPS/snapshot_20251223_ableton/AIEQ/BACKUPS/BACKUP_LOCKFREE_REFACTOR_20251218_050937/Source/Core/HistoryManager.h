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
#include "LockFreeStructures.h"
#include <vector>
#include <functional>

namespace AIEQCore
{

// Note: kMaxBands is already defined in LockFreeStructures.h
static constexpr int kMaxHistorySize = 20;

/**
 * Immutable snapshot of EQ band state
 */
struct BandSnapshot
{
    float frequency = 1000.0f;
    float gain = 0.0f;
    float q = 1.0f;
    int filterType = 2;  // Peak
    bool enabled = true;
    
    // Dynamic EQ parameters
    int dynamicMode = 0;
    float threshold = -20.0f;
    float ratio = 2.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float range = 24.0f;
    float knee = 6.0f;
};

/**
 * Complete EQ state snapshot for undo/redo
 */
struct EQStateSnapshot
{
    std::array<BandSnapshot, kMaxBands> bands;
    int numActiveBands = 8;
    float outputGain = 0.0f;
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
    void initialize(juce::AudioProcessorValueTreeState& apvts)
    {
        this->apvts = &apvts;
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
            undoStack.erase(undoStack.begin());
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
        if (apvts == nullptr)
            return;
        
        for (int i = 0; i < kMaxBands; ++i)
        {
            juce::String prefix = "band" + juce::String(i);
            auto& band = snapshot.bands[static_cast<size_t>(i)];
            
            if (auto* freq = apvts->getRawParameterValue(prefix + "Freq"))
                band.frequency = freq->load();
            if (auto* gain = apvts->getRawParameterValue(prefix + "Gain"))
                band.gain = gain->load();
            if (auto* q = apvts->getRawParameterValue(prefix + "Q"))
                band.q = q->load();
            if (auto* type = apvts->getRawParameterValue(prefix + "Type"))
                band.filterType = static_cast<int>(type->load());
            if (auto* enabled = apvts->getRawParameterValue(prefix + "Enabled"))
                band.enabled = enabled->load() > 0.5f;
            
            // Dynamic EQ
            if (auto* dynMode = apvts->getRawParameterValue(prefix + "DynMode"))
                band.dynamicMode = static_cast<int>(dynMode->load());
            if (auto* thresh = apvts->getRawParameterValue(prefix + "Threshold"))
                band.threshold = thresh->load();
            if (auto* ratio = apvts->getRawParameterValue(prefix + "Ratio"))
                band.ratio = ratio->load();
            if (auto* attack = apvts->getRawParameterValue(prefix + "Attack"))
                band.attackMs = attack->load();
            if (auto* release = apvts->getRawParameterValue(prefix + "Release"))
                band.releaseMs = release->load();
            if (auto* range = apvts->getRawParameterValue(prefix + "Range"))
                band.range = range->load();
            if (auto* knee = apvts->getRawParameterValue(prefix + "Knee"))
                band.knee = knee->load();
        }
        
        if (auto* numBands = apvts->getRawParameterValue("numActiveBands"))
            snapshot.numActiveBands = static_cast<int>(numBands->load()) + 1; // 0-indexed
        if (auto* outputGain = apvts->getRawParameterValue("outputGain"))
            snapshot.outputGain = outputGain->load();
    }
    
    /**
     * Restore EQ state to APVTS
     * Uses beginChangeGesture/endChangeGesture for proper host undo integration
     */
    void restoreState(const EQStateSnapshot& snapshot)
    {
        if (apvts == nullptr)
            return;
        
        for (int i = 0; i < kMaxBands; ++i)
        {
            juce::String prefix = "band" + juce::String(i);
            const auto& band = snapshot.bands[static_cast<size_t>(i)];
            
            setParameter(prefix + "Freq", band.frequency);
            setParameter(prefix + "Gain", band.gain);
            setParameter(prefix + "Q", band.q);
            setParameter(prefix + "Type", static_cast<float>(band.filterType));
            setParameter(prefix + "Enabled", band.enabled ? 1.0f : 0.0f);
            
            // Dynamic EQ
            setParameter(prefix + "DynMode", static_cast<float>(band.dynamicMode));
            setParameter(prefix + "Threshold", band.threshold);
            setParameter(prefix + "Ratio", band.ratio);
            setParameter(prefix + "Attack", band.attackMs);
            setParameter(prefix + "Release", band.releaseMs);
            setParameter(prefix + "Range", band.range);
            setParameter(prefix + "Knee", band.knee);
        }
        
        setParameter("numActiveBands", static_cast<float>(snapshot.numActiveBands - 1));
        setParameter("outputGain", snapshot.outputGain);
    }
    
    /**
     * Helper to set a parameter with proper host notification
     */
    void setParameter(const juce::String& paramID, float value)
    {
        if (auto* param = apvts->getParameter(paramID))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(value));
            param->endChangeGesture();
        }
    }
    
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    std::vector<EQStateSnapshot> undoStack;
    std::vector<EQStateSnapshot> redoStack;
};

} // namespace AIEQCore

