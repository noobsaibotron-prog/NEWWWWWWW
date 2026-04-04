---
name: state-management-audit
description: >
  Use this sub-skill to audit the state management, preset serialization, undo/redo, and remote control (OSC) of the AIEQ plugin.
metadata:
  category: encoded-preference
  domain_boundaries:
    primary: ENGINEERING
    excluded:
      - LEGAL
      - MARKETING
  version: "1.0"
  promotion_history:
    - v1.0: Initial version
  model_requirements:
    context_window: 128k
    tool_use: optional
    reasoning_depth: high
---

# State Management Audit (Sub-Skill)

## Core Principle
Plugin state is the user's data. It must be serialized atomically, restored deterministically, and never corrupted during preset switching, undo/redo operations, or A/B/C/D slot changes.

## Forbidden Actions
- Do not evaluate DSP audio processing or GUI rendering here.
- Do not assume `ValueTree` operations are thread-safe without checking.
- Do not validate state restoration if it can cause audio clicks or deadlocks.

## Mandatory Grounding Pass
Before judging state code:
1. Identify where state is stored (e.g., `juce::AudioProcessorValueTreeState`).
2. Identify how state is serialized/deserialized (`getStateInformation`, `setStateInformation`).
3. Check for thread synchronization around state changes.

## Specialized Audits

### Audit 1: Serialization Safety and Determinism
- **Trigger:** Reading `getStateInformation` and `setStateInformation`.
- **Check:** Does `setStateInformation` use transactional locks (e.g., `std::recursive_mutex` or lock-free queues) to prevent partial state reads by the audio thread? Is the XML/JSON parsing robust against malformed data?
- **Pass:** State is restored atomically and safely.
- **Fail:** Audio thread can read partial state during restoration, or parsing can crash.

### Audit 2: Undo/Redo Robustness
- **Trigger:** Reading `HistoryManager.h` or undo-related code.
- **Check:** Are all parameter changes correctly bracketed with `beginGesture` and `endGesture`? Is the history stack bounded to prevent memory exhaustion?
- **Pass:** Gestures are complete, and memory is bounded.
- **Fail:** Missing gesture brackets, leading to corrupted undo steps.

### Audit 3: Remote Control (OSC) Safety
- **Trigger:** Reading `OSCParameterServer.h` or network code.
- **Check:** Are incoming OSC messages validated before applying to the APVTS? Are they dispatched on the correct thread (Message Thread)?
- **Pass:** OSC messages are sanitized and dispatched asynchronously.
- **Fail:** OSC messages directly modify state from the network thread, causing race conditions.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[State Mgt: <state>]`.

## Promotion Criteria
- A real state corruption or recall issue is found in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
