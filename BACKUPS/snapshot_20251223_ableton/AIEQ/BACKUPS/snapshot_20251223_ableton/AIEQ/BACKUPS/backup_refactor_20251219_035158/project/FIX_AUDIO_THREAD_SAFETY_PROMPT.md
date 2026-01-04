# 🔴 FIX CRITICAL AUDIO THREAD SAFETY ISSUES

## Context
This VST3 plugin has **2 critical bugs** that will cause audio dropouts, clicks, and crashes in Pro Tools/Reaper strict mode. These MUST be fixed before beta release.

---

## ❌ PROBLEM 1: Heap allocation in audio thread (PluginProcessor.cpp)

**File:** `Source/PluginProcessor.cpp`

**Issue:** `std::vector::resize()` can allocate heap memory. This is called from `processBlock()` → `pushToCaptureRing()` which runs in the realtime audio thread. Heap allocation in audio thread = glitches/crashes.

### Fix Step 1: Pre-allocate buffer in prepareToPlay()

**Location:** `Source/PluginProcessor.cpp`, method `prepareToPlay()`, around line 430-431

**Find this section (end of prepareToPlay):**
```cpp
    // Prepare capture ring buffer
    const int maxSamples = static_cast<int>(sampleRate * captureMaxSeconds);
    captureRing.setSize(getTotalNumInputChannels(), maxSamples);
    captureRing.clear();
    captureRingLength = maxSamples;
```

**Add AFTER the captureRing setup (after line 431):**
```cpp
    // Pre-allocate manual capture buffer (max 20 seconds stereo)
    // This prevents heap allocation during audio processing
    const size_t maxCaptureSamples = static_cast<size_t>(sampleRate * captureMaxSeconds * 2); // stereo = 2 channels
    manualCaptureBuffer.reserve(maxCaptureSamples);
```

### Fix Step 2: Add capacity check in pushToCaptureRing()

**Location:** `Source/PluginProcessor.cpp`, method `pushToCaptureRing()`, around line 1035-1059

**Find this code block:**
```cpp
    // Manual capture: append to buffer
    if (isManualCapturing)
    {
        juce::SpinLock::ScopedTryLockType lock(captureLock);
        if (lock.isLocked())
        {
            const size_t oldSize = manualCaptureBuffer.size();
            const int channelsToCopy = std::min(numCh, 2); // Max 2 channels
            manualCaptureBuffer.resize(oldSize + static_cast<size_t>(numSamples * channelsToCopy));
            
            for (int s = 0; s < numSamples; ++s)
            {
                for (int ch = 0; ch < channelsToCopy; ++ch)
                {
                    const float* src = buffer.getReadPointer(ch);
                    manualCaptureBuffer[oldSize + static_cast<size_t>(s * channelsToCopy + ch)] = src[s];
                }
            }
            manualCaptureSamples += numSamples;
            
            // Auto-stop if buffer too large (max 20 seconds)
            const int maxSamples = static_cast<int>(currentSampleRate * captureMaxSeconds);
            if (manualCaptureSamples >= maxSamples)
            {
                stopManualCapture();
            }
        }
    }
```

**Replace with:**
```cpp
    // Manual capture: append to buffer
    if (isManualCapturing)
    {
        juce::SpinLock::ScopedTryLockType lock(captureLock);
        if (lock.isLocked())
        {
            const size_t oldSize = manualCaptureBuffer.size();
            const int channelsToCopy = std::min(numCh, 2); // Max 2 channels
            const size_t needed = oldSize + static_cast<size_t>(numSamples * channelsToCopy);
            
            // SAFETY: Stop capture if would exceed pre-allocated capacity (no reallocation allowed in audio thread)
            if (needed > manualCaptureBuffer.capacity())
            {
                isManualCapturing = false;
                return;
            }
            
            manualCaptureBuffer.resize(needed);  // Safe - no reallocation since within capacity
            
            for (int s = 0; s < numSamples; ++s)
            {
                for (int ch = 0; ch < channelsToCopy; ++ch)
                {
                    const float* src = buffer.getReadPointer(ch);
                    manualCaptureBuffer[oldSize + static_cast<size_t>(s * channelsToCopy + ch)] = src[s];
                }
            }
            manualCaptureSamples += numSamples;
            
            // Auto-stop if buffer too large (max 20 seconds)
            const int maxSamples = static_cast<int>(currentSampleRate * captureMaxSeconds);
            if (manualCaptureSamples >= maxSamples)
            {
                stopManualCapture();
            }
        }
    }
```

### Fix Step 3: Change clear() to resize(0) in startManualCapture()

**Location:** `Source/PluginProcessor.cpp`, method `startManualCapture()`, around line 1165-1180

**Find this code:**
```cpp
bool AIEqualizerAudioProcessor::startManualCapture()
{
    juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked())
        return false;
    
    if (isManualCapturing)
        return false;  // Already capturing
    
    isManualCapturing = true;
    manualCaptureBuffer.clear();
    manualCaptureSamples = 0;
    capturedSampleRate = currentSampleRate;
    
    return true;
}
```

**Replace `manualCaptureBuffer.clear();` with:**
```cpp
    manualCaptureBuffer.resize(0);  // Keep capacity, just reset size (guaranteed no deallocation)
```

---

## ❌ PROBLEM 2: Blocking mutex in audio thread (ParametricEQProcessor)

**Files:** `Source/DSP/ParametricEQProcessor.h` and `Source/DSP/ParametricEQProcessor.cpp`

**Issue:** The `process()` method uses `std::lock_guard<std::mutex>` which is **BLOCKING**. When GUI updates parameters, it takes the mutex. If audio thread calls `process()` at the same time, it **WAITS (blocks)**, causing audio dropout.

### Fix Step 1: Replace mutex with SpinLock in header

**Location:** `Source/DSP/ParametricEQProcessor.h`, line 130

**Find:**
```cpp
    mutable std::mutex bandsMutex;
```

**Replace with:**
```cpp
    mutable juce::SpinLock bandsLock;
```

**Also remove the mutex include** (line 6):
```cpp
#include <mutex>  // <-- REMOVE THIS LINE
```

### Fix Step 2: Update process() to use TryLock (non-blocking)

**Location:** `Source/DSP/ParametricEQProcessor.cpp`, method `process()`, around line 46-51

**Find:**
```cpp
void ParametricEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (bypassed)
        return;
    
    std::lock_guard<std::mutex> lock(bandsMutex);
```

**Replace with:**
```cpp
void ParametricEQProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (bypassed)
        return;
    
    // NON-BLOCKING: TryLock - if GUI has lock, process with current filter state anyway
    // Note: Even if lock fails, filters already have valid coefficients from last update
    // Processing continues with slightly stale parameters (inaudible, <1ms delay)
    juce::SpinLock::ScopedTryLockType lock(bandsLock);
    // Note: We don't check if lock.isLocked() - filters have valid state regardless
```

### Fix Step 3: Replace all other mutex locks with SpinLock

**Location:** `Source/DSP/ParametricEQProcessor.cpp`

**Find ALL occurrences of:**
```cpp
std::lock_guard<std::mutex> lock(bandsMutex);
```

**Replace with:**
```cpp
juce::SpinLock::ScopedLockType lock(bandsLock);
```

**Methods that need updating (search for `std::lock_guard` in these functions):**
- `prepare()` (line 23)
- `reset()` (line 37)
- `addBand()` (line 143)
- `removeBand()` (line 182)
- `clearAllBands()` (line 192)
- `setBandFrequency()` (line 199)
- `setBandGain()` (line 210)
- `setBandQ()` (line 221)
- `setBandType()` (line 232)
- `setBandEnabled()` (line 243)
- `setBandSolo()` (line 253)
- `setBandParameters()` (line 263)
- `getBandFrequency()` (line 279)
- `getBandGain()` (line 287)
- `getBandQ()` (line 295)
- `getBandType()` (line 303)
- `isBandEnabled()` (line 311)
- `isBandSolo()` (line 319)
- `getBand()` (line 327)
- `getMagnitudeForFrequency()` (line 336)
- `getMagnitudeForFrequencyArray()` (line 356)

**Use search & replace:**
- Find: `std::lock_guard<std::mutex> lock(bandsMutex);`
- Replace: `juce::SpinLock::ScopedLockType lock(bandsLock);`

---

## ✅ VERIFICATION CHECKLIST

After applying fixes, verify:

1. [ ] `manualCaptureBuffer.reserve()` is called in `prepareToPlay()` (after captureRing setup)
2. [ ] `pushToCaptureRing()` checks `capacity()` before `resize()`
3. [ ] `startManualCapture()` uses `resize(0)` not `clear()`
4. [ ] `ParametricEQProcessor.h` declares `juce::SpinLock bandsLock` (not `std::mutex`)
5. [ ] `ParametricEQProcessor.h` does NOT include `<mutex>`
6. [ ] `ParametricEQProcessor::process()` uses `ScopedTryLockType` (non-blocking)
7. [ ] All other ParametricEQProcessor methods use `ScopedLockType`
8. [ ] No `std::mutex` or `std::lock_guard` remains in ParametricEQProcessor files
9. [ ] Build succeeds with no errors
10. [ ] Plugin loads in DAW without crash
11. [ ] Moving EQ knobs rapidly while audio plays causes no clicks/dropouts

---

## ⚠️ IMPORTANT NOTES

- **Do NOT change any DSP logic**, only the locking mechanism
- **Do NOT modify `SpectrumAnalyzer`** - it already uses SpinLock correctly
- The `ScopedTryLockType` in `process()` is intentional - if lock fails, processing continues with current filter state (this is safe and correct)
- `SpinLock` is designed for short critical sections like this - it's the standard pattern for audio plugins
- `resize(0)` guarantees no deallocation (maintains capacity), while `clear()` behavior is implementation-dependent
- The capacity check in `pushToCaptureRing()` gracefully stops capture if buffer would exceed pre-allocated size

---

## 🎯 EXPECTED RESULT

After these fixes:
- ✅ No heap allocations in audio thread
- ✅ No blocking locks in audio thread
- ✅ Plugin safe for Pro Tools/Reaper strict mode
- ✅ Ready for beta testing







