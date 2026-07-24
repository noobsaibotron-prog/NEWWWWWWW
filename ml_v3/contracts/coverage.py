"""Calibration / measurement coverage floors (G1a).

Contract §10.4, §8.1 and §11.1 @ 6d254d0a.

The independent unit of support is ALWAYS `group_id`. Cells, events, crops,
derivatives and multiple assets of the same group increase the number of
examples but never the independent support. This module counts unique group ids
and returns a deterministic report; it never fits anything and never lowers a
floor. Insufficient support yields N/A, never PASS.

Fail-closed before floors: positive∩negative must be empty for a family;
family/profile strata must be pairwise disjoint and equal the declared sets.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable

from .constants import (ANOMALY_CLASSES, CONDITIONING_PROFILES,
                        FINAL_TEST_ELECTRONIC_MIN_FRACTION, PROFILE_ROLE_FLOORS)

__all__ = [
    "CoverageError", "CoverageCheck", "CoverageReport",
    "TONAL_FLOORS", "ANOMALY_FLOORS", "DEV_FINAL_FLOORS", "FAMILY_FLOORS",
    "check_calibration_coverage", "check_dev_final_support",
    "check_profile_role_floors", "check_final_test_electronic_share",
    "check_stratum_calibrator_allowed",
]

# --- section 10.4 table, line 611 (tonal row)
TONAL_FLOORS = {
    "actionable_groups": 50,
    "actionable_groups_with_positive_cell": 20,
    "actionable_groups_with_negative_cell": 20,
    "clean_groups": 50,
    "per_profile_actionable": 3,
    "per_profile_clean": 3,
}

# --- section 10.4 table, lines 612-614 (one identical row per anomaly class)
ANOMALY_FLOORS = {
    "positive_groups": 30,
    "negative_groups": 50,
    "negative_groups_clean": 30,
    "positive_source_families": 3,
    "positive_groups_per_family": 5,
    "max_family_share": 0.50,
    "negatives_per_profile": 3,
}

# --- section 10.4, lines 629-633
DEV_FINAL_FLOORS = {
    "tonal_actionable_groups": 30,
    "clean_groups_for_actionable_rate": 149,
    "anomaly_positive_groups": 30,
    "clean_safety_groups": 149,
}

# --- section 11.1 table, lines 695-698
FAMILY_FLOORS = {
    "tonal-controlled": {"parent_groups_per_profile": 5, "groups_per_region_direction": 10},
    "tonal-natural": {"clean_groups_per_profile": 5, "actionable_groups_per_profile": 5},
    "anomaly-natural": {"positive_groups_per_class": 30},
    "clean-safety": {"total_groups": 149, "groups_per_profile": 10,
                     "eligible_minute_per_class": 1},
    "electronic-stratified": {"final_test_fraction": FINAL_TEST_ELECTRONIC_MIN_FRACTION},
}

# A stratum-specific calibrator/threshold is forbidden below this support in
# the calibration split (section 10.4, lines 623-627).
STRATUM_CALIBRATOR_MIN_POSITIVE = 30
STRATUM_CALIBRATOR_MIN_NEGATIVE = 30


class CoverageError(ValueError):
    """Raised on malformed coverage input (never on insufficient support)."""


@dataclass(frozen=True)
class CoverageCheck:
    """One floor evaluation. `status` is PASS or N/A — never a silent PASS."""
    name: str
    required: float
    observed: float
    status: str

    @property
    def ok(self) -> bool:
        return self.status == "PASS"


@dataclass
class CoverageReport:
    checks: list[CoverageCheck] = field(default_factory=list)

    def add(self, name: str, required: float, observed: float) -> None:
        status = "PASS" if observed >= required else "N/A"
        self.checks.append(CoverageCheck(name, required, observed, status))

    @property
    def ok(self) -> bool:
        return all(check.ok for check in self.checks)

    def failures(self) -> list[CoverageCheck]:
        return [check for check in self.checks if not check.ok]

    def to_dict(self) -> dict:
        return {
            "ok": self.ok,
            "checks": [
                {"name": check.name, "required": check.required,
                 "observed": check.observed, "status": check.status}
                for check in self.checks
            ],
        }


def _unique(values: Iterable[str]) -> int:
    return len(set(values))


def _as_group_set(values: Iterable[str], field: str) -> set[str]:
    if not isinstance(values, (list, tuple, set, frozenset)):
        raise CoverageError(f"{field} must be an iterable of group_id strings")
    out: set[str] = set()
    for item in values:
        if not isinstance(item, str) or not item:
            raise CoverageError(f"{field} contains a non-string or empty group_id")
        out.add(item)
    return out


def _require_disjoint(left: set[str], right: set[str], left_name: str,
                      right_name: str) -> None:
    overlap = left & right
    if overlap:
        sample = sorted(overlap)[:5]
        raise CoverageError(
            f"{left_name} and {right_name} must be disjoint; "
            f"overlap sample={sample} count={len(overlap)}")


def _require_partition_match(declared: set[str], by_key: dict, field: str,
                             declared_name: str) -> None:
    """Fail-closed: strata are pairwise disjoint and union == declared set."""
    if not isinstance(by_key, dict):
        raise CoverageError(f"{field} must be a mapping")
    union: set[str] = set()
    for key, groups in by_key.items():
        subset = _as_group_set(groups, f"{field}[{key!r}]")
        extras = subset - declared
        if extras:
            sample = sorted(extras)[:5]
            raise CoverageError(
                f"{field}[{key!r}] contains groups not in {declared_name}: "
                f"{sample}")
        overlap = union & subset
        if overlap:
            sample = sorted(overlap)[:5]
            raise CoverageError(
                f"{field} strata are not disjoint; overlap sample={sample}")
        union |= subset
    missing = declared - union
    if missing:
        sample = sorted(missing)[:5]
        raise CoverageError(
            f"{field} is inconsistent with {declared_name}: "
            f"missing from strata sample={sample} count={len(missing)}")


def _require_tonal_profile_partition(declared: set[str], by_profile: dict,
                                     field: str, declared_name: str) -> None:
    """Tonal per-profile maps: exact 7 canonical keys + partition match."""
    if not isinstance(by_profile, dict):
        raise CoverageError(f"{field} must be a mapping")
    expected_keys = set(CONDITIONING_PROFILES)
    actual_keys = set(by_profile)
    missing_keys = expected_keys - actual_keys
    if missing_keys:
        raise CoverageError(
            f"{field} missing canonical profile keys: {sorted(missing_keys)}")
    extra_keys = actual_keys - expected_keys
    if extra_keys:
        raise CoverageError(
            f"{field} has non-canonical profile keys: {sorted(extra_keys)}")
    _require_partition_match(declared, by_profile, field, declared_name)


def check_calibration_coverage(tonal: dict, anomaly: dict) -> CoverageReport:
    """Global calibration coverage (§10.4).

    Fail-closed invariants (before floors):
      - same annotation cannot be positive and negative for one family;
      - positive_groups_by_family union == positive_groups;
      - negative_groups_by_profile union ⊆ negative_groups and covers them;
      - tonal actionable∩clean empty; pos/neg cell groups ⊆ actionable;
      - tonal per_profile_actionable / per_profile_clean: exact 7 canonical
        profile keys, pairwise disjoint, union == actionable_groups /
        clean_groups respectively.

    `tonal` keys: actionable_groups, actionable_positive_cell_groups,
    actionable_negative_cell_groups, clean_groups (iterables of group_id), plus
    per_profile_actionable / per_profile_clean mapping profile -> group ids.

    `anomaly` maps each of the three classes to a dict with: positive_groups,
    negative_groups, negative_clean_groups, positive_groups_by_family,
    negative_groups_by_profile.
    """
    if not isinstance(tonal, dict):
        raise CoverageError("tonal coverage payload must be a dict")
    if not isinstance(anomaly, dict):
        raise CoverageError("anomaly coverage payload must be a dict")

    report = CoverageReport()

    actionable = _as_group_set(tonal.get("actionable_groups", []), "tonal.actionable_groups")
    pos_cells = _as_group_set(
        tonal.get("actionable_positive_cell_groups", []),
        "tonal.actionable_positive_cell_groups")
    neg_cells = _as_group_set(
        tonal.get("actionable_negative_cell_groups", []),
        "tonal.actionable_negative_cell_groups")
    clean = _as_group_set(tonal.get("clean_groups", []), "tonal.clean_groups")
    _require_disjoint(actionable, clean, "tonal.actionable_groups", "tonal.clean_groups")
    extras = (pos_cells | neg_cells) - actionable
    if extras:
        raise CoverageError(
            "tonal positive/negative cell groups must be subsets of actionable_groups; "
            f"extras sample={sorted(extras)[:5]}")

    per_profile_actionable = tonal.get("per_profile_actionable", {})
    per_profile_clean = tonal.get("per_profile_clean", {})
    # Exact 7 canonical profile keys; pairwise disjoint; union == declared sets.
    _require_tonal_profile_partition(
        actionable, per_profile_actionable,
        "tonal.per_profile_actionable", "tonal.actionable_groups")
    _require_tonal_profile_partition(
        clean, per_profile_clean,
        "tonal.per_profile_clean", "tonal.clean_groups")

    report.add("tonal.actionable_groups", TONAL_FLOORS["actionable_groups"],
               len(actionable))
    report.add("tonal.actionable_with_positive_cell",
               TONAL_FLOORS["actionable_groups_with_positive_cell"],
               len(pos_cells))
    report.add("tonal.actionable_with_negative_cell",
               TONAL_FLOORS["actionable_groups_with_negative_cell"],
               len(neg_cells))
    report.add("tonal.clean_groups", TONAL_FLOORS["clean_groups"], len(clean))
    for profile in CONDITIONING_PROFILES:
        report.add(f"tonal.actionable[{profile}]", TONAL_FLOORS["per_profile_actionable"],
                   _unique(per_profile_actionable.get(profile, [])))
        report.add(f"tonal.clean[{profile}]", TONAL_FLOORS["per_profile_clean"],
                   _unique(per_profile_clean.get(profile, [])))

    for klass in ANOMALY_CLASSES:
        data = anomaly.get(klass, {})
        if not isinstance(data, dict):
            raise CoverageError(f"anomaly[{klass}] must be a dict")
        positives = _as_group_set(data.get("positive_groups", []),
                                  f"{klass}.positive_groups")
        negatives = _as_group_set(data.get("negative_groups", []),
                                  f"{klass}.negative_groups")
        neg_clean = _as_group_set(data.get("negative_clean_groups", []),
                                  f"{klass}.negative_clean_groups")
        # §10.4: same annotation cannot be positive and negative for one family.
        _require_disjoint(positives, negatives,
                          f"{klass}.positive_groups", f"{klass}.negative_groups")
        extras_neg_clean = neg_clean - negatives
        if extras_neg_clean:
            raise CoverageError(
                f"{klass}.negative_clean_groups must be a subset of negative_groups")

        by_family = data.get("positive_groups_by_family", {})
        _require_partition_match(
            positives, by_family, f"{klass}.positive_groups_by_family",
            f"{klass}.positive_groups")
        by_profile = data.get("negative_groups_by_profile", {})
        _require_partition_match(
            negatives, by_profile, f"{klass}.negative_groups_by_profile",
            f"{klass}.negative_groups")

        report.add(f"{klass}.positive_groups", ANOMALY_FLOORS["positive_groups"],
                   len(positives))
        report.add(f"{klass}.negative_groups", ANOMALY_FLOORS["negative_groups"],
                   len(negatives))
        report.add(f"{klass}.negative_clean_groups",
                   ANOMALY_FLOORS["negative_groups_clean"],
                   len(neg_clean))

        qualifying = {family: set(groups) for family, groups in by_family.items()
                      if len(set(groups)) >= ANOMALY_FLOORS["positive_groups_per_family"]}
        report.add(f"{klass}.positive_source_families",
                   ANOMALY_FLOORS["positive_source_families"], len(qualifying))
        if positives:
            largest = max((len(set(groups)) for groups in by_family.values()), default=0)
            share = largest / len(positives)
            report.add(f"{klass}.family_share_headroom",
                       1.0 - ANOMALY_FLOORS["max_family_share"], 1.0 - share)
        for profile in CONDITIONING_PROFILES:
            report.add(f"{klass}.negatives[{profile}]",
                       ANOMALY_FLOORS["negatives_per_profile"],
                       _unique(by_profile.get(profile, [])))
    return report


def check_dev_final_support(tonal_actionable_groups: Iterable[str],
                            clean_groups: Iterable[str],
                            anomaly_positive_groups: dict[str, Iterable[str]],
                            clean_safety_groups: Iterable[str]) -> CoverageReport:
    """development-metric / final-test minimum support (lines 629-637)."""
    report = CoverageReport()
    report.add("dev_final.tonal_actionable_groups",
               DEV_FINAL_FLOORS["tonal_actionable_groups"],
               _unique(tonal_actionable_groups))
    report.add("dev_final.clean_groups_for_actionable_rate",
               DEV_FINAL_FLOORS["clean_groups_for_actionable_rate"],
               _unique(clean_groups))
    for klass in ANOMALY_CLASSES:
        report.add(f"dev_final.{klass}.positive_groups",
                   DEV_FINAL_FLOORS["anomaly_positive_groups"],
                   _unique(anomaly_positive_groups.get(klass, [])))
    report.add("dev_final.clean_safety_groups",
               DEV_FINAL_FLOORS["clean_safety_groups"], _unique(clean_safety_groups))
    return report


def check_profile_role_floors(groups_by_profile_role: dict[str, dict[str, Iterable[str]]]
                              ) -> CoverageReport:
    """General per-profile split floors (section 8.1, lines 285-287)."""
    report = CoverageReport()
    for profile in CONDITIONING_PROFILES:
        per_role = groups_by_profile_role.get(profile, {})
        for role, floor in PROFILE_ROLE_FLOORS.items():
            report.add(f"split.{profile}.{role}", floor,
                       _unique(per_role.get(role, [])))
    return report


def check_final_test_electronic_share(final_test_groups: Iterable[str],
                                      electronic_groups: Iterable[str]) -> CoverageReport:
    """At least 40% of the whole final-test must be electronic (line 287)."""
    report = CoverageReport()
    total = set(final_test_groups)
    electronic = set(electronic_groups) & total
    share = (len(electronic) / len(total)) if total else 0.0
    report.add("final_test.electronic_share",
               FINAL_TEST_ELECTRONIC_MIN_FRACTION, share)
    return report


def check_stratum_calibrator_allowed(positive_groups: Iterable[str],
                                     negative_groups: Iterable[str]) -> CoverageReport:
    """A per-profile/domain/subgenre calibrator needs 30 positives AND 30
    negatives in the calibration split (lines 623-627); otherwise N/A."""
    report = CoverageReport()
    report.add("stratum_calibrator.positive_groups",
               STRATUM_CALIBRATOR_MIN_POSITIVE, _unique(positive_groups))
    report.add("stratum_calibrator.negative_groups",
               STRATUM_CALIBRATOR_MIN_NEGATIVE, _unique(negative_groups))
    return report
