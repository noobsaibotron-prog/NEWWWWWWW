"""Isolated startup shim for REV8 O-09 benchmark evidence.

Invoke only as:

    <locked-python> -I -S -B path/to/this_file.py <runner arguments>

``-I`` ignores ``PYTHON*`` import configuration, ``-S`` prevents
``sitecustomize``/``usercustomize`` and ``.pth`` processing, and ``-B``
prevents bytecode writes.  This shim then adds exactly the repository root and
the locked venv's site-packages directory before executing the candidate
runner.  No project module is imported before those startup invariants hold.
"""
from __future__ import annotations

from importlib.machinery import EXTENSION_SUFFIXES
from pathlib import Path
import runpy
import sys

RUNNER_MODULE = "ml_v3.benchmark.run_rev8_o09_candidate"


def _preimport_tree_hygiene(repository_root: Path) -> None:
    """Reject import shadows before the repository enters ``sys.path``."""
    project_root = repository_root / "ml_v3"
    caches = sorted(
        str(path.relative_to(repository_root))
        for path in project_root.rglob("__pycache__")
    )
    pyc_files = sorted(
        str(path.relative_to(repository_root))
        for path in project_root.rglob("*.pyc")
    )
    native_shadows = sorted(
        str(path.relative_to(repository_root))
        for path in project_root.rglob("*")
        if path.is_file() and any(
            path.name.endswith(suffix) for suffix in EXTENSION_SUFFIXES)
    )
    if caches or pyc_files or native_shadows:
        raise SystemExit(
            "REV8 O-09 bootstrap requires an archive-like Python source "
            "tree before import; "
            f"caches={caches[:5]}, pyc={pyc_files[:5]}, "
            f"native_shadows={native_shadows[:5]}")


def main() -> None:
    flags = sys.flags
    if not (
        flags.isolated
        and flags.no_site
        and flags.ignore_environment
        and flags.safe_path
        and sys.dont_write_bytecode
    ):
        raise SystemExit(
            "REV8 O-09 bootstrap requires Python flags -I -S -B")
    if sys.pycache_prefix is not None:
        raise SystemExit("REV8 O-09 bootstrap forbids sys.pycache_prefix")

    repository_root = Path(__file__).resolve().parents[2]
    _preimport_tree_hygiene(repository_root)
    venv_root = Path(sys.executable).parent.parent
    site_packages = (
        venv_root
        / "lib"
        / f"python{sys.version_info.major}.{sys.version_info.minor}"
        / "site-packages"
    )
    if not site_packages.is_dir():
        raise SystemExit(
            f"locked venv site-packages not found: {site_packages}")

    # Do not call site.addsitedir(): .pth execution is deliberately forbidden.
    sys.path.insert(0, str(site_packages))
    sys.path.insert(0, str(repository_root))
    sys.argv[0] = RUNNER_MODULE
    runpy.run_module(RUNNER_MODULE, run_name="__main__", alter_sys=True)


if __name__ == "__main__":
    main()
