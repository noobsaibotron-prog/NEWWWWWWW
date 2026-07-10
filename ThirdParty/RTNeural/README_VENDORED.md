# RTNeural (vendored subset) — Motore v2

Header-only neural-net inference for real-time audio. Used by the **Motore v2**
detection engine to run a model exported from Python (PyTorch) as JSON.

- **Upstream:** https://github.com/jatinchowdhury18/RTNeural
- **Pinned commit:** see `.upstream-sha` (`1fb1f075…`, fetched 2026-07-10)
- **License:** BSD-3-Clause (see `LICENSE`) — compatible with commercial distribution.

## What is vendored (and why a subset)

- `RTNeural/` — the header-only library (`.h`/`.tpp`), all layer types (needed
  because the runtime JSON parser must be able to instantiate any layer).
- `modules/json/json.hpp` — nlohmann/json single header, the **only** external
  include RTNeural needs at runtime (`RTNeural/model_loader.h` → `../modules/json/json.hpp`).

NOT vendored: `modules/Eigen`, `modules/xsimd` (only needed for the Eigen/XSIMD
backends), tests, benchmarks, examples, docs, Python export tooling.

## Backend

We use the **STL backend** (no `RTNEURAL_USE_EIGEN` / `RTNEURAL_USE_XSIMD`
define). It is pure header-only, has zero extra dependencies, and — measured in
A0 — costs ~0.4 µs per forward on the dummy model, far inside the analysis
thread's duty cycle. If profiling on the real (larger) model ever demands it,
the Eigen backend can be added later without changing the JSON format.

## How to consume

Add `ThirdParty/RTNeural` to the include path and `#include <RTNeural/RTNeural.h>`.
Define `RTNEURAL_DEFAULT_ALIGNMENT=16` to silence the alignment warning. This is
done in `CMakeLists.txt` only when `AIEQ_ENABLE_MOTORE_V2=ON` (default OFF), so
the shipped build never compiles RTNeural.

## Updating

Re-fetch upstream at a new commit, copy `RTNeural/` and `modules/json/json.hpp`,
update `.upstream-sha`, and re-run the A0 parity test
(`ml_v2/a0_gen_dummy.py` → build with `-DAIEQ_ENABLE_MOTORE_V2=ON` → run the
"Motore v2 A0 Parity" test).
