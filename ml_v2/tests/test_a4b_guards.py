"""A4b Pilastro 0 guard tests: cache fingerprint (0a) + fail-closed loader (0b).

Run:  python3 -m ml_v2.tests.test_a4b_guards        (plain, no pytest needed)
      python3 -m pytest ml_v2/tests/test_a4b_guards.py
"""
from __future__ import annotations

import csv
import json
import tempfile
from pathlib import Path

from ml_v2.dataset_v2 import (ALLOWED_LICENSES, BuildConfig, MANIFEST,
                              cache_path, manifest_rows, pipeline_fingerprint)

HEADER = ["path", "domain", "group", "split", "sr", "channels", "bits",
          "duration_s", "sha256", "license", "license_ok", "upstream_license",
          "attribution", "source", "hf_dead", "crosscheck"]


def _row(**over) -> dict:
    r = {k: "" for k in HEADER}
    r.update(path="real_audio/x.wav", domain="clean_bass", group="src:x",
             split="train", sr="44100", channels="1", bits="16",
             duration_s="1.0", sha256="a" * 64, license="CC0", license_ok="1",
             upstream_license="n/a", attribution="t", source="t",
             hf_dead="0", crosscheck="t")
    r.update(over)
    return r


def _write_manifest(rows: list[dict]) -> Path:
    f = tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False,
                                    newline="")
    w = csv.DictWriter(f, fieldnames=HEADER)
    w.writeheader()
    for r in rows:
        w.writerow(r)
    f.close()
    return Path(f.name)


# ------------------------------------------------------------------ 0a
def test_fingerprint_stable_and_manifest_sensitive():
    fp1 = pipeline_fingerprint(MANIFEST)
    assert fp1 == pipeline_fingerprint(MANIFEST), "fingerprint must be stable"
    other = _write_manifest([_row()])
    assert pipeline_fingerprint(other) != fp1, \
        "different manifest bytes must change the fingerprint"


def test_cache_path_embeds_fingerprint_and_config():
    cfg = BuildConfig(split="train", seed=42)
    p = cache_path(cfg, Path("/tmp/aieq_v2_test_cache"))
    assert pipeline_fingerprint(MANIFEST) in p.name
    assert cfg.key() in p.name
    cfg2 = BuildConfig(split="train", seed=43)
    assert cache_path(cfg2, Path("/tmp/aieq_v2_test_cache")).name != p.name


def test_contract_digest_is_stable_and_content_sensitive():
    from ml_v2.dataset_v2 import split_contract_sha256
    p1 = _write_contract({"pack:a": "train"}, [])
    p2 = _write_contract({"pack:a": "metric"}, [])
    assert split_contract_sha256(p1) == split_contract_sha256(p1)
    assert split_contract_sha256(p1) != split_contract_sha256(p2)


# ------------------------------------------------------------------ 0b
def _expect_valueerror(manifest: Path, needle: str):
    try:
        manifest_rows("train", manifest)
    except ValueError as e:
        assert needle in str(e), f"wrong error: {e}"
        return
    raise AssertionError(f"expected ValueError containing {needle!r}")


def test_loader_fail_closed_license_ok():
    _expect_valueerror(_write_manifest([_row(license_ok="0")]), "license_ok")


def test_loader_fail_closed_license_whitelist():
    _expect_valueerror(_write_manifest([_row(license="CC-BY-NC")]), "whitelist")


def test_loader_fail_closed_empty_group():
    _expect_valueerror(_write_manifest([_row(group="")]), "group")


def test_loader_fail_closed_empty_sha():
    _expect_valueerror(_write_manifest([_row(sha256="")]), "sha256")


def test_loader_accepts_valid_rows_and_dedups():
    m = _write_manifest([_row(), _row(path="real_audio/y.wav"),  # same sha -> dedup
                         _row(path="real_audio/z.wav", sha256="b" * 64)])
    rows = manifest_rows("train", m)
    assert len(rows) == 2, f"expected sha-dedup to 2 rows, got {len(rows)}"


def test_real_manifest_passes_fail_closed():
    for split in ("train", "heldout", "test"):
        rows = manifest_rows(split, MANIFEST)
        assert rows, f"real manifest yields no rows for {split}"
        assert all(r["license"] in ALLOWED_LICENSES for r in rows)


def test_owned_requires_ledger():
    _expect_valueerror(
        _write_manifest([_row(license="OWNED", upstream_license="n/a")]),
        "ledger")
    m = _write_manifest([_row(license="OWNED", upstream_license="Loop Pack EULA",
                              attribution="VendorX", crosscheck="receipt#123")])
    assert len(manifest_rows("train", m)) == 1


def test_corrupt_cache_is_deleted_and_rebuilt():
    from ml_v2.dataset_v2 import _load_cache
    bad = Path(tempfile.mkdtemp()) / "windows_train_dead_beef.npz"
    bad.write_bytes(b"this is not an npz")
    msgs = []
    assert _load_cache(bad, log=msgs.append) is None
    assert not bad.exists(), "corrupt cache must be deleted"
    assert any("corrupt" in m for m in msgs)


def test_calib_metric_split_is_group_level():
    from ml_v2.dataset_v2 import heldout_calib_metric_indices
    # 40 windows: 4 groups x 5 files x 2 windows; same-group files MUST land
    # on the same side of the calibration/metric split.
    sources = [f"dom-clean:g{g}/file{f}.wav"
               for g in range(4) for f in range(5) for _ in range(2)]
    calib, metric, meta = heldout_calib_metric_indices(sources, roles={})
    assert meta["heldout_source_groups"] == 4, meta
    side = {}
    for idx in calib:
        side.setdefault(sources[idx].rsplit(":", 1)[-1].split("/", 1)[0], set()).add("c")
    for idx in metric:
        side.setdefault(sources[idx].rsplit(":", 1)[-1].split("/", 1)[0], set()).add("m")
    for g, s in side.items():
        assert len(s) == 1, f"group {g} straddles calib/metric: {s}"


def test_gid_is_injective():
    from ml_v2.dataset_v2 import _gid
    # plain char substitution would collide these; percent-encoding must not
    pairs = [("a:b", "a=b"), ("a:b", "a_b"), ("x/y", "x_y"), ("p%q", "p_q")]
    for g1, g2 in pairs:
        assert _gid({"group": g1}) != _gid({"group": g2}), (g1, g2)
    from urllib.parse import unquote
    assert unquote(_gid({"group": "singer:fem/1%x"})) == "singer:fem/1%x"


def test_contract_roles_win_over_hash():
    from ml_v2.dataset_v2 import _gid, heldout_calib_metric_indices
    gA, gB, gC = (_gid({"group": "pack:A"}), _gid({"group": "pack:B"}),
                  _gid({"group": "legacy"}))
    sources = ([f"dom-clean:{gA}/f{i}.wav" for i in range(4)]
               + [f"dom-clean:{gB}/f{i}.wav" for i in range(4)]
               + [f"dom-clean:{gC}/f{i}.wav" for i in range(4)])
    roles = {"pack:A": "calibration", "pack:B": "metric"}
    calib, metric, meta = heldout_calib_metric_indices(sources, roles=roles)
    assert meta["heldout_role_forced_groups"] == 2, meta
    calib_gids = {sources[i].rsplit(":", 1)[-1].split("/", 1)[0] for i in calib}
    metric_gids = {sources[i].rsplit(":", 1)[-1].split("/", 1)[0] for i in metric}
    assert gA in calib_gids and gA not in metric_gids
    assert gB in metric_gids and gB not in calib_gids


def _write_contract(roles: dict, legacy: list,
                    metadata: dict | None = None,
                    batches: dict | None = None) -> Path:
    from ml_v2.dataset_v2 import BATCH_ALLOCATION, batch_group_ids_sha256
    if metadata is None:
        metadata = {
            group: {"primary_domain": "full-mix", "admission_batch": "test-batch",
                    "source_id": "test-source"}
            for group in roles
        }
    if batches is None:
        groups = sorted(roles)
        batches = ({"test-batch": {
            "source_id": "test-source", "primary_domain": "full-mix",
            "allocation": BATCH_ALLOCATION, "group_ids": groups,
            "group_ids_sha256": batch_group_ids_sha256(groups),
        }} if groups else {})
    f = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
    json.dump({"schema": "a4b-split-v2", "salt": "test-salt", "roles": roles,
               "group_metadata": metadata, "batches": batches,
               "legacy_groups": legacy}, f)
    f.close()
    return Path(f.name)


def test_contract_rejects_train_expansion_spelling():
    from ml_v2.dataset_v2 import load_split_contract
    p = _write_contract({"pack:new": "train-expansion"}, [])
    try:
        load_split_contract(p)
    except ValueError as e:
        assert "unknown role" in str(e) and "train-expansion" in str(e)
    else:
        raise AssertionError("'train-expansion' must be rejected (canonical: train)")


def test_contract_rejects_unknown_role_and_schema():
    from ml_v2.dataset_v2 import load_split_contract
    p = _write_contract({"pack:new": "evaluation"}, [])
    try:
        load_split_contract(p)
    except ValueError as e:
        assert "unknown role" in str(e)
    else:
        raise AssertionError("unknown role must be rejected")
    import json as _json
    bad = Path(tempfile.mktemp(suffix=".json"))
    bad.write_text(_json.dumps({"schema": "v999", "roles": {}}))
    try:
        load_split_contract(bad)
    except ValueError as e:
        assert "schema" in str(e)
    else:
        raise AssertionError("unknown schema must be rejected")


def test_contract_requires_complete_immutable_batch_metadata():
    from ml_v2.dataset_v2 import BATCH_ALLOCATION, batch_group_ids_sha256, \
        load_split_contract
    group = "pack:new"
    bad = Path(tempfile.mktemp(suffix=".json"))
    bad.write_text(json.dumps({
        "schema": "a4b-split-v2", "salt": "test", "roles": {group: "train"},
        "group_metadata": {}, "legacy_groups": [], "batches": {},
    }))
    try:
        load_split_contract(bad)
    except ValueError as e:
        assert "identical keys" in str(e)
    else:
        raise AssertionError("role assignment must carry immutable metadata")

    bad_hash = Path(tempfile.mktemp(suffix=".json"))
    bad_hash.write_text(json.dumps({
        "schema": "a4b-split-v2", "salt": "test", "roles": {group: "train"},
        "group_metadata": {group: {"primary_domain": "full-mix",
                                    "admission_batch": "batch-a",
                                    "source_id": "source-a"}},
        "legacy_groups": [], "batches": {"batch-a": {
            "source_id": "source-a", "primary_domain": "full-mix",
            "allocation": BATCH_ALLOCATION, "group_ids": [group],
            "group_ids_sha256": (
                batch_group_ids_sha256([group])[:-1]
                + ("0" if batch_group_ids_sha256([group])[-1] != "0" else "1")),
        }},
    }))
    try:
        load_split_contract(bad_hash)
    except ValueError as e:
        assert "hash mismatch" in str(e)
    else:
        raise AssertionError("batch group hash must be verified")


def test_contract_delta_is_append_only():
    from ml_v2.dataset_v2 import validate_split_contract_delta
    base_roles = {"old:group": "g3-external"}
    base_metadata = {"old:group": {
        "primary_domain": "full-mix", "admission_batch": "batch-old",
        "source_id": "source-old"}}
    from ml_v2.dataset_v2 import BATCH_ALLOCATION, batch_group_ids_sha256
    base_batches = {"batch-old": {
        "source_id": "source-old", "primary_domain": "full-mix",
        "allocation": BATCH_ALLOCATION, "group_ids": ["old:group"],
        "group_ids_sha256": batch_group_ids_sha256(["old:group"]),
    }}
    previous = _write_contract(base_roles, [], base_metadata, base_batches)

    candidate_roles = {**base_roles, "new:group": "g3-external"}
    candidate_metadata = {**base_metadata, "new:group": {
        "primary_domain": "full-mix", "admission_batch": "batch-new",
        "source_id": "source-new"}}
    candidate_batches = {**base_batches, "batch-new": {
        "source_id": "source-new", "primary_domain": "full-mix",
        "allocation": BATCH_ALLOCATION, "group_ids": ["new:group"],
        "group_ids_sha256": batch_group_ids_sha256(["new:group"]),
    }}
    candidate = _write_contract(candidate_roles, [], candidate_metadata,
                                candidate_batches)
    validate_split_contract_delta(previous, candidate)

    reassigned = _write_contract({**candidate_roles, "old:group": "metric"}, [],
                                 candidate_metadata, candidate_batches)
    try:
        validate_split_contract_delta(previous, reassigned)
    except ValueError as e:
        assert "role" in str(e)
    else:
        raise AssertionError("existing groups must remain immutable")

    mutated_legacy = _write_contract(candidate_roles, ["legacy:new"],
                                     candidate_metadata, candidate_batches)
    try:
        validate_split_contract_delta(previous, mutated_legacy)
    except ValueError as e:
        assert "legacy_groups" in str(e)
    else:
        raise AssertionError("legacy baseline must remain immutable")


def test_batch_assignment_is_deterministic_and_small_batches_are_eval_only():
    from ml_v2.contract_tools import assign_batch_roles
    groups = ["source:g1", "source:g2", "source:g3", "source:g4"]
    first = assign_batch_roles("salt", "batch", groups)
    assert first == assign_batch_roles("salt", "batch", list(reversed(groups)))
    assert set(first.values()) == {"train", "calibration", "metric", "g3-external"}
    small = assign_batch_roles("salt", "small", groups[:3])
    assert "train" not in small.values()
    assert set(assign_batch_roles("salt", "one", groups[:1]).values()) == {"g3-external"}


def test_new_group_without_role_aborts_in_heldout():
    from ml_v2.dataset_v2 import _gid, heldout_calib_metric_indices
    g_new = _gid({"group": "pack:brandnew"})
    g_old = _gid({"group": "legacy:known"})
    sources = [f"dom-clean:{g_new}/f{i}.wav" for i in range(3)] \
        + [f"dom-clean:{g_old}/f{i}.wav" for i in range(3)]
    try:
        heldout_calib_metric_indices(sources, roles={},
                                     legacy={"legacy:known"})
    except ValueError as e:
        assert "without a contract role" in str(e)
    else:
        raise AssertionError("un-roled new group in heldout must abort")


def test_role_split_mismatch_rejected():
    from ml_v2.dataset_v2 import validate_split_contract
    m = _write_manifest([_row(group="pack:x", split="train")])
    c = _write_contract({"pack:x": "g3-external"}, [])  # g3 requires split=test
    try:
        validate_split_contract(m, c)
    except ValueError as e:
        assert "requires split" in str(e)
    else:
        raise AssertionError("role/split mismatch must be rejected")
    c2 = _write_contract({}, [])  # group neither roled nor legacy
    try:
        validate_split_contract(m, c2)
    except ValueError as e:
        assert "no contract role" in str(e)
    else:
        raise AssertionError("group outside roles+legacy must be rejected")


def test_contract_rejects_cross_split_group_and_sha_leakage():
    from ml_v2.dataset_v2 import validate_split_contract

    group_leak = _write_manifest([
        _row(group="legacy:shared", split="train", sha256="a" * 64),
        _row(path="real_audio/y.wav", group="legacy:shared", split="heldout",
             sha256="b" * 64),
    ])
    legacy = _write_contract({}, ["legacy:shared"])
    try:
        validate_split_contract(group_leak, legacy)
    except ValueError as e:
        assert "spans splits" in str(e)
    else:
        raise AssertionError("one group must never cross manifest splits")

    from ml_v2.dataset_v2 import ROLE_TO_SPLIT, assign_batch_roles
    assigned = assign_batch_roles("test-salt", "test-batch",
                                  ["new:left", "new:right"])
    names = sorted(assigned)
    sha_leak = _write_manifest([
        _row(group=names[0], split=ROLE_TO_SPLIT[assigned[names[0]]], sha256="c" * 64),
        _row(path="real_audio/y.wav", group=names[1],
             split=ROLE_TO_SPLIT[assigned[names[1]]], sha256="c" * 64),
    ])
    roles = _write_contract(assigned, [])
    try:
        validate_split_contract(sha_leak, roles)
    except ValueError as e:
        assert "sha256" in str(e) and "boundaries" in str(e)
    else:
        raise AssertionError("one sha256 must never cross group/split/role boundaries")


def test_contract_rejects_invalid_manifest_identity_before_build():
    from ml_v2.dataset_v2 import validate_split_contract
    contract = _write_contract({"new:g3": "g3-external"}, [])
    for bad, needle in [({"group": "", "sha256": "a" * 64}, "empty group"),
                        ({"group": "new:g3", "split": "test", "sha256": ""},
                         "empty sha256"),
                        ({"group": "new:g3", "split": "unknown"}, "unknown split")]:
        manifest = _write_manifest([_row(**bad)])
        try:
            validate_split_contract(manifest, contract)
        except ValueError as e:
            assert needle in str(e)
        else:
            raise AssertionError(f"invalid identity must reject: {bad}")


def test_contract_allows_duplicate_sha_only_within_one_group_and_split():
    from ml_v2.dataset_v2 import validate_split_contract
    manifest = _write_manifest([
        _row(group="new:g3", split="test", sha256="d" * 64),
        _row(path="real_audio/y.wav", group="new:g3", split="test",
             sha256="d" * 64),
    ])
    contract = _write_contract({"new:g3": "g3-external"}, [])
    validate_split_contract(manifest, contract)


def test_contract_rejects_roles_overlapping_legacy_baseline():
    from ml_v2.dataset_v2 import load_split_contract
    contract = _write_contract({"legacy:x": "train"}, ["legacy:x"])
    try:
        load_split_contract(contract)
    except ValueError as e:
        assert "role-assigned" in str(e)
    else:
        raise AssertionError("frozen legacy group must not gain a new role")


def test_build_or_load_validates_contract_before_cache_lookup():
    from ml_v2.dataset_v2 import build_or_load
    manifest = _write_manifest([_row(group="new:unroled", split="train")])
    contract = _write_contract({}, [])
    cache_dir = Path(tempfile.mkdtemp())
    cfg = BuildConfig(split="train", seed=42)
    cache = cache_path(cfg, cache_dir, manifest)
    sentinel = b"this cache must never be read or deleted"
    cache.write_bytes(sentinel)
    try:
        build_or_load(cfg, cache_dir, manifest=manifest, contract_path=contract)
    except ValueError as e:
        assert "no contract role" in str(e)
    else:
        raise AssertionError("un-roled new train group must abort before cache lookup")
    assert cache.read_bytes() == sentinel, "cache lookup ran before contract validation"


def test_real_contract_loads_and_validates():
    from ml_v2.dataset_v2 import (MANIFEST, SPLIT_ROLES_PATH,
                                  load_split_contract, validate_split_contract)
    roles, legacy = load_split_contract(SPLIT_ROLES_PATH)
    assert legacy is not None and len(legacy) > 0, "contract file must exist"
    validate_split_contract(MANIFEST, SPLIT_ROLES_PATH)


def test_contamination_role_in_heldout_aborts():
    from ml_v2.dataset_v2 import _gid, heldout_calib_metric_indices
    g = _gid({"group": "pack:X"})
    sources = [f"dom-clean:{g}/f{i}.wav" for i in range(4)] \
        + [f"dom-clean:other/f{i}.wav" for i in range(4)]
    for bad_role in ("train", "g3-external"):
        try:
            heldout_calib_metric_indices(sources, roles={"pack:X": bad_role})
        except ValueError as e:
            assert "contamination" in str(e)
        else:
            raise AssertionError(f"role {bad_role} in heldout must abort")


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"  PASS {fn.__name__}")
    print(f"{len(fns)}/{len(fns)} guard tests passed")
