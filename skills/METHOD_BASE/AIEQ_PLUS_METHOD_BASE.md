# AIEQ+ Method Base

## Purpose
AIEQ+ is a cross-domain framework for building, testing, promoting, consolidating, and retiring AI skills with empirical discipline.

It exists to reduce:
- overclaiming
- decorative complexity
- premature closure
- false confidence
- silent boundary violations

Its core belief is simple:

> The same empirical discipline that prevents overclaiming in code review prevents overclaiming in any domain.

---

## Core Principle
No completion claim without freshly generated, verifiable evidence.

Never collapse governance states.
Never treat absence of contradiction as proof of correctness.
Never cross a domain boundary silently.

---

## Skill Categories

### Capacity Uplift
A skill that compensates for something the base model does not do reliably enough.
These skills must be re-tested when the base model materially changes.

### Encoded Preference
A skill that encodes a stable workflow, SOP, governance rule, or reporting convention.
These skills are durable, but only insofar as they remain faithful to the real workflow.

Every skill must declare its category.

---

## Canonical Cycle

1. Create a narrow first version of the skill
2. Apply it to a real artifact
3. Record what it found correctly
4. Record what it missed
5. Record what it overstated
6. Record what remained unverified
7. Classify the weakness before modifying the skill
8. Promote only if the weakness is:
   - real
   - recurring
   - important enough to justify more complexity
9. Re-test the promoted version on:
   - one known artifact
   - one new artifact
10. Consolidate before promoting again

---

## Non-Negotiable Rules

### Rule A — Real artifacts only
Skills must be tested on real artifacts, not imagined use cases.

### Rule B — Promotion must be earned
No module is added “for completeness.”
Every promotion must be justified by test evidence.

### Rule C — Separate local from cross-artifact truth
Every finding must be classified as:
- local
- cross-artifact
- inferred

### Rule D — Separate fix state from governance state
Never collapse:
- drafted
- reviewed
- tested
- validated
- approved

### Rule E — Archive meaningful tests
Tests that teach something reusable must be archived in structured form.

### Rule F — Respect domain boundaries
If analysis crosses a domain outside the skill’s declared scope, emit a `[BOUNDARY FLAG]`.

### Rule G — Account for model decay and redundancy
Capacity uplift skills may become obsolete as base models improve.
Retest them and retire them when they become redundant.

### Rule H — Ban performative compliance
Do not agree before checking.
Do not simulate rigor.
Do not use closure language without fresh evidence.

---

## Weakness Classes

- proof weakness
- boundary weakness
- workload weakness
- lifecycle weakness
- severity weakness
- remediation weakness
- trigger weakness
- compliance weakness
- decay weakness

A skill may be modified only after its weakness has been classified.

---

## Promotion Standard

A skill may be promoted only when:
- at least one strong test demonstrates a real weakness
- the weakness is classified
- the added module is specific
- the promoted version is re-tested on:
  - one known artifact
  - one new artifact

Small, surgical promotion is preferred over broad decorative expansion.

---

## Retirement Standard

A skill should be retired when:
- the base model now performs equivalently without it
- its operational overhead exceeds its value
- its trigger has become too vague to be reliable
- it no longer reflects the real workflow it was built to support

Retired skills are archived, not deleted.

---

## Final Operating Philosophy

Create narrowly.
Test on real artifacts.
Archive what the test teaches.
Promote cautiously.
Re-test after promotion.
Consolidate before further upgrades.
Retire when redundant.

A skill becomes stronger when it refuses to overclaim.
