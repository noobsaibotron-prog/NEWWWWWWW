# AIEQ+ Glossary

## Purpose
This document defines the core terminology used throughout the AIEQ+ framework. A shared, precise vocabulary is essential to prevent overclaiming, silent boundary crossings, and the collapse of governance states.

---

## Core Concepts

### Artifact
A real, tangible output that can be evaluated. This includes source code files, architectural documents, legal contracts, meeting transcripts, or datasets. AIEQ+ skills must be tested against real artifacts, never against imagined use cases or abstract summaries.

### Grounding
The practice of anchoring every claim, finding, or conclusion to a specific, verifiable piece of evidence within an artifact. A claim without grounding is considered "unverified" or "overstated."

### Overclaiming
The AI behavior of stating a conclusion with higher confidence or broader scope than the provided evidence supports. Examples include saying "The feature is fully secure" when only one specific vulnerability was checked.

### Performative Compliance
The AI behavior of agreeing with the user, flattering a claim, or using reassuring closure language ("Resolved", "Verified") without actually performing the necessary verification or having the evidence to support it.

---

## Proof Levels

### Local Evidence
Proof that is entirely contained within a single artifact or file. It does not rely on assumptions about how other parts of the system work.

### Cross-Artifact Evidence
Proof that requires synthesizing information from multiple artifacts (e.g., verifying that a function call in File A matches the API definition in File B).

### Inferred Point
A conclusion drawn by logical deduction or assumption rather than direct evidence in the artifact. Inferred points must always be labeled as such and never treated as proven facts.

---

## Skill Types

### Capacity-Uplift Skill
A skill designed to compensate for a specific task that the base AI model cannot perform reliably on its own (e.g., complex multi-file logic tracing). These skills are temporary scaffolds and should be retired when the base model improves enough to handle the task natively.

### Encoded-Preference Skill
A skill designed to enforce a specific organizational workflow, standard operating procedure (SOP), or reporting format (e.g., a specific code review checklist). These skills are durable as long as the underlying workflow remains relevant.

---

## Governance Actions

### Promote
To upgrade a skill to a new version by adding a specific module or tightening a rule. Promotion is only allowed when a documented test proves a recurring weakness in the current version.

### Consolidate
To simplify, refactor, or optimize a skill without changing its core capabilities. Usually done after a promotion to integrate the new module cleanly and reduce operational overhead.

### Rollback
To revert a skill to a previous version because a recent promotion caused unintended regressions, boundary drift, or excessive overhead.

### Retire
To remove a skill from active use and archive it, usually because the base model has improved (Decay) or the organizational workflow has changed.
