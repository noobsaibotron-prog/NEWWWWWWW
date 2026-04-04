# Example Hierarchical Output — audio-plugin-manager v2.0

> This file demonstrates what a real orchestrated output looks like when `audio-plugin-manager` is applied to a JUCE EQ plugin project.

---

## Executive Summary
The JUCE EQ plugin project was audited across 3 domains (DSP, UI, Licensing). The DSP code is real-time safe and well-structured. The UI uses a deprecated polling pattern instead of APVTS attachments. The project is missing a LICENSE file entirely. The plugin is not ready for release.

## Vectorized Governance State

| Domain | Sub-Skill | State |
|---|---|---|
| DSP | `juce-dsp-audit` | **Validated** |
| UI/UX | `plugin-ui-audit` | **Tested (Failed)** |
| Licensing | `license-compliance-audit` | **Drafted** |
| **Overall** | — | **Drafted** |

**Justification:** The overall state collapses to `Drafted` because the licensing dimension has not been addressed at all. The DSP dimension is independently `Validated`, meaning it can proceed to human approval for that specific domain if desired.

---

## Hierarchical Findings

### 1. DSP Analysis (`juce-dsp-audit` v1.0)

#### ✅ Proven (Grounded Evidence)
- `processBlock` (PluginProcessor.cpp, lines 45-112) contains no `new`, `malloc`, or `std::mutex::lock` calls.
- Filter coefficients are updated via `std::atomic<float>` (line 23).
- Oversampling uses `juce::dsp::Oversampling` with lock-free buffer management (line 67).

#### ❌ Missed / Defects
- None identified.

#### Governance State: **Validated**

---

### 2. UI/UX Analysis (`plugin-ui-audit` v1.0)

#### ✅ Proven (Grounded Evidence)
- `PluginEditor.cpp` exists and contains a `paint` method (line 34).
- The editor constructor initializes 5 sliders (lines 12-28).

#### ❌ Missed / Defects
- **No APVTS Attachments found.** The editor reads parameter values via `processor.getParameter("gain")` inside a `timerCallback` at 30Hz (line 55). This is a deprecated polling pattern that bypasses the thread-safe `AudioProcessorValueTreeState` mechanism.
- The `timerCallback` frequency (30Hz) may cause visible UI lag on high-resolution displays.

#### Governance State: **Tested (Failed Validation)**
**Missing for next state:** Replace polling with `SliderAttachment` objects.

---

### 3. Licensing (`license-compliance-audit` v1.0)

#### ✅ Proven (Grounded Evidence)
- `PluginProcessor.cpp` contains a copyright header on line 1: `// Copyright 2026 Horizon Audio`.

#### ❌ Missed / Defects
- **No LICENSE file found** in the project root directory.
- `PluginEditor.cpp` has **no copyright header** in the first 20 lines.
- The JUCE license type (GPLv3 vs. Commercial) is not declared anywhere in the project.

#### Governance State: **Drafted**
**Missing for next state:** Add a LICENSE file and ensure all source files have consistent copyright headers.

---

## Cross-Module Synthesis

### ⚠️ Inferred Risks
- The DSP module uses 4x oversampling (`juce::dsp::Oversampling`), which increases CPU load. The UI module's `timerCallback` at 30Hz may not be responsive enough to display real-time CPU usage accurately. This is an inferred risk, not a proven defect, because no performance benchmark was provided.

### 🚩 Conflicts
- **`[CONFLICT FLAG]`** The DSP audit validates the code as production-ready, but the licensing audit shows the project cannot be legally distributed without a LICENSE file. These two findings create a governance conflict: the code is technically excellent but legally incomplete. **Resolution requires human decision** on whether to proceed with a limited technical release or block until licensing is resolved.

---

## Next Steps

| Priority | Domain | Action |
|---|---|---|
| **Critical** | Licensing | Add a `LICENSE` file (GPLv3 or Commercial JUCE license). Add copyright headers to all source files. |
| **High** | UI/UX | Replace `timerCallback` polling with `AudioProcessorValueTreeState::SliderAttachment`. |
| **Low** | DSP | No action required. Consider adding a CPU load meter for user visibility. |

`[HITL REQUIRED]` The plugin cannot advance to `Approved` overall. Human decision required on licensing strategy before any distribution.
