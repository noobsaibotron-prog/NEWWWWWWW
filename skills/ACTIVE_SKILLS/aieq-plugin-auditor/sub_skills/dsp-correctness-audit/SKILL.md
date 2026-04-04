---
name: dsp-correctness-audit
description: >
  Use this sub-skill to audit the numerical accuracy, filter response, phase response, and anti-cramping mechanisms of the AIEQ plugin's DSP algorithms.
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

# DSP Correctness Audit (Sub-Skill)

## Core Principle
DSP algorithms must be mathematically sound, numerically stable, and free from artifacts like aliasing, cramping, or unbounded gain. This sub-skill evaluates the *correctness* of the math, not its thread safety.

## Forbidden Actions
- Do not evaluate real-time safety (e.g., locks, allocations) here; that belongs to `dsp-safety-audit`.
- Do not assume a filter is correct just because it compiles.
- Do not accept implementations that ignore Nyquist limits.

## Mandatory Grounding Pass
Before judging DSP code:
1. Identify the core algorithm (e.g., RBJ biquads, partitioned convolution, dynamic EQ).
2. Identify the numerical precision used (`float` vs `double`).
3. Check how boundary conditions (e.g., frequencies near Nyquist, 0 Hz) are handled.

## Specialized Audits

### Audit 1: Filter Stability and Anti-Cramping
- **Trigger:** Reading filter coefficient calculations (e.g., `BiquadCoefficients.h`).
- **Check:** Are frequencies clamped below Nyquist (e.g., `fs/2 - margin`)? Is there an anti-cramping mechanism (e.g., Orfanidis, decramping filters) for high-frequency bell/shelf filters?
- **Pass:** Frequencies are clamped, and anti-cramping is implemented.
- **Fail:** Frequencies can exceed Nyquist, or high-frequency filters warp asymmetrically.

### Audit 2: Numerical Precision and Denormals
- **Trigger:** Reading recursive filter processing (e.g., IIR filters).
- **Check:** Are denormal numbers (subnormals) actively prevented (e.g., adding a small DC offset, using SSE/AVX flags like FTZ/DAZ)? Is `double` precision used for critical recursive state variables?
- **Pass:** Denormals are prevented, and precision is sufficient.
- **Fail:** Recursive filters accumulate denormals, causing CPU spikes.

### Audit 3: Oversampling and Anti-Aliasing
- **Trigger:** Reading non-linear processing (e.g., Dynamic EQ, saturation).
- **Check:** If oversampling is used, are the upsampling/downsampling filters linear phase (or high-quality minimum phase) with sufficient stopband attenuation? Is the delay correctly compensated?
- **Pass:** High-quality resampling filters with correct delay compensation.
- **Fail:** Aliasing is ignored, or resampling introduces significant phase distortion without compensation.

## Output Format
Follow the standard AIEQ+ Layered Proof Map format for sub-skills. Provide a `Proven` list and a `Missed` list, anchored with exact file names and line numbers. State the local vector state as `[DSP Correctness: <state>]`.

## Promotion Criteria
- A real numerical artifact, aliasing issue, or denormal spike is found in production.
- The weakness is classified.
- A specific audit is added to catch that pattern.
- Re-tested on the failing artifact and a new artifact.
