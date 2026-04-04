---
name: build-system-audit
description: >
  Use this sub-skill to audit the build system, CMake configuration, and CI/CD scripts of the AIEQ plugin.
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

# Build System Audit (Sub-Skill)

## Core Principle
A reliable build system is the foundation of a stable product. This sub-skill evaluates the CMake configuration, dependency management (JUCE, TFLite), cross-platform build scripts, and CI/CD integration for the AIEQ plugin.

## Forbidden Actions
- Do not evaluate C++ code correctness or DSP algorithms here.
- Do not accept hardcoded paths or platform-specific assumptions in CMake without a fallback.
- Do not validate build scripts that lack error handling.

## Mandatory Grounding Pass
Before judging build scripts:
1. Identify the primary build configuration file (e.g., `CMakeLists.txt`).
2. Identify the target platforms (macOS, Windows, Linux) and formats (VST3, AU, Standalone).
3. Check how external dependencies are fetched and linked.

## Specialized Audits

### Audit 1: CMake Best Practices
- **Trigger:** Reading `CMakeLists.txt` or `.cmake` files.
- **Check:** Are modern CMake targets used (`target_link_libraries`, `target_compile_options`) instead of global variables (`link_directories`, `add_compile_options`)? Are release optimization flags (e.g., `-O3`, `/O2`) correctly applied for the `Release` configuration?
- **Pass:** Uses target-based modern CMake, with explicit optimization flags for Release.
- **Fail:** Uses global CMake variables, or lacks explicit optimization flags for Release builds.

### Audit 2: Dependency Management Robustness
- **Trigger:** Reading dependency fetching logic (e.g., `FetchContent`, `add_subdirectory`).
- **Check:** Are dependencies like JUCE and TFLite pinned to specific versions or commits? Is the fetching process resilient to network failures?
- **Pass:** Dependencies are pinned to specific versions/commits.
- **Fail:** Dependencies use `master` or `main` branches, leading to non-reproducible builds.

### Audit 3: Cross-Platform Build Scripts
- **Trigger:** Reading `build.sh`, `build_mac.sh`, `build_fix.cmd`, or PowerShell scripts.
- **Check:** Do the scripts handle errors (e.g., `set -e` in bash, `$ErrorActionPreference = "Stop"` in PowerShell)? Do they support cross-compilation (e.g., Apple Silicon vs. Intel)?
- **Pass:** Scripts fail fast on errors and support necessary architectures.
- **Fail:** Scripts ignore errors and continue, or hardcode single architectures.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[Build System: <state>]`.

## Promotion Criteria
- A real build failure or deployment issue is found in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
