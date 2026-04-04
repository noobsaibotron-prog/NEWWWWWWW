# AIEQ+ Release Verdict Engine — Scoring & Verdict Model

This document defines the strict, mathematical rules for calculating the Commercial Rating, the 10-Dimensional State Vector, and the Final Release Verdict.

## 1. Commercial Rating Calculation

The Commercial Rating is a weighted average of the 10 domain scores, scaled from 0.0 to 10.0.
Each domain score is calculated based on the percentage of CLEAN files vs. CRITICAL/HIGH/MEDIUM files in that domain.

**Weights (calibrated for commercial audio plugins):**
- DSP Safety: 2.0x
- DSP Correctness: 1.5x
- Plugin Compliance: 1.5x
- GUI Performance: 1.0x
- State Management: 1.0x
- Parameter Architecture: 1.0x
- AI Integration: 1.0x
- Build System: 0.5x
- Test Quality: 0.5x
- Code Hygiene: 0.5x

**Commercial Threshold:** A rating of **7.0/10.0** is the minimum acceptable threshold for a commercial release.

## 2. The 10-Dimensional State Vector

Each domain is assigned a state based on its internal score and the presence of CRITICAL files.

- **Validated:** Score >= 9.0 AND zero CRITICAL files AND zero HIGH files.
- **Tested:** Score >= 7.0 AND zero CRITICAL files.
- **Reviewed:** Score >= 5.0 AND max 1 CRITICAL file.
- **Drafted:** Score < 5.0 OR > 1 CRITICAL file.

The **Overall State** is strictly the lowest state among all 10 domains. A single `Drafted` domain forces the entire project into `Drafted`.

## 3. The Release Verdict Matrix

The Final Verdict is a non-negotiable declaration based on the Overall State and the Commercial Rating.

| Condition | Verdict | Meaning |
|---|---|---|
| Overall State is `Validated` AND Rating >= 9.0 | **RELEASE-SAFE (ELITE)** | Ready for immediate deployment. Exceeds commercial standards. |
| Overall State is `Tested` AND Rating >= 7.0 | **RELEASE-SAFE** | Ready for deployment. Meets commercial standards. |
| Overall State is `Tested` AND Rating < 7.0 | **RELEASE-CONDITIONAL** | Technically safe, but UX/Polish is sub-commercial. Release only as Beta. |
| Overall State is `Reviewed` | **RELEASE-RISKY** | High probability of user complaints or edge-case crashes. Do not release. |
| Overall State is `Drafted` | **DO-NOT-RELEASE** | Guaranteed failure in production environments. |

## 4. Cross-Domain Conflict Penalties

If the Orchestrator emits a `[CONFLICT FLAG]`:
- **CRITICAL Conflict:** Automatically forces the involved domains to `Drafted`, regardless of their individual scores.
- **HIGH Conflict:** Caps the involved domains at `Tested` (cannot be `Validated`).
- **MEDIUM Conflict:** -0.5 penalty to the Commercial Rating.

## 5. Maximum ROI Remediation Ordering

Remediation steps MUST be ordered using the following algorithm:
1. **Wave 1 (P0):** Fix files that are part of a CRITICAL Cross-Domain Conflict AND have > 10 transitive dependents (e.g., `AIEngine.cpp`, `PluginProcessor.cpp`).
2. **Wave 2 (P1):** Fix isolated CRITICAL files that are keeping a domain in the `Drafted` state.
3. **Wave 3 (P2):** Fix HIGH/MEDIUM files in domains with a high weight (e.g., DSP Safety, Compliance) to boost the Commercial Rating above 7.0.
