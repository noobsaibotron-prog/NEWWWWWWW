"""A4b Pilastro 0 guard tests: cache fingerprint (0a) + fail-closed loader (0b).

Run:  python3 -m ml_v2.tests.test_a4b_guards        (plain, no pytest needed)
      python3 -m pytest ml_v2/tests/test_a4b_guards.py
"""
from __future__ import annotations

import csv
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


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"  PASS {fn.__name__}")
    print(f"{len(fns)}/{len(fns)} guard tests passed")
