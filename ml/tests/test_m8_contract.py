"""M8 lab contract checks.

Run:
    python3 -m ml.tests.test_m8_contract
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from ml import blob_io
from ml.features import features_v4
from ml.model import NUM_PROBLEMS, TwoStageEQNet


def main() -> int:
    failures = 0
    tmp = Path("/tmp/aieq_m8/test_m8_contract_v3.bin")
    tmp.parent.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(123)
    net = TwoStageEQNet(seed=17)
    net.presence_threshold = 0.63
    net.class_thresholds = np.linspace(0.35, 0.70, NUM_PROBLEMS)
    # M9.2: emission semantics must survive the v3 roundtrip.
    net.emission_mode = "per-class"
    net.override_delta = 0.15
    x = rng.normal(size=(9, 64))
    want_presence, want_probs = net.forward(x)
    want_freq = net.forward_freq(x)

    blob_io.write_v3(net, tmp, 1, {"problem_schema": "product-v2", "seed": 17})
    got, feature_version, provenance = blob_io.read_v3(tmp)
    have_presence, have_probs = got.forward(x)
    have_freq = got.forward_freq(x)

    checks = [
        ("feature-version", feature_version == 1),
        ("architecture", provenance.get("architecture") == "two-stage-per-class"),
        ("presence-threshold", abs(got.presence_threshold - 0.63) < 1e-7),
        ("class-thresholds", np.allclose(got.class_thresholds, net.class_thresholds)),
        ("presence-roundtrip", np.allclose(have_presence, want_presence, atol=2e-7)),
        ("class-roundtrip", np.allclose(have_probs, want_probs, atol=2e-7)),
        ("freq-roundtrip", np.allclose(have_freq, want_freq, atol=2e-7)),
        ("emission-mode-roundtrip", got.emission_mode == "per-class"),
        ("override-delta-roundtrip",
         got.override_delta is not None
         and abs(got.override_delta - 0.15) < 1e-7),
    ]
    for name, ok in checks:
        if not ok:
            print(f"FAIL {name}")
            failures += 1

    got.emission_mode = "legacy"
    got.override_delta = None
    got.presence_threshold = 0.99
    got.class_thresholds = np.full(NUM_PROBLEMS, 0.99)
    if got.emitted(x).any():
        print("FAIL emission-thresholds")
        failures += 1

    # M9.2 semantics: with the presence gate locked shut (0.99) and class
    # thresholds wide open (0.01), legacy must stay silent while per-class
    # must emit — the measured M8 round-6 blindness, fixed by design.
    got.class_thresholds = np.full(NUM_PROBLEMS, 0.01)
    got.emission_mode = "legacy"
    if got.emitted(x, ui_cap=0).any():
        print("FAIL emission-legacy-gate-blocks")
        failures += 1
    got.emission_mode = "per-class"
    if not got.emitted(x, ui_cap=0).any():
        print("FAIL emission-per-class-bypasses-gate")
        failures += 1

    spectrum = np.linspace(1e-4, 0.2, 2049)
    fv4_a = features_v4(spectrum, 44100.0)
    fv4_b = features_v4(spectrum, 44100.0)
    feature_checks = [
        ("feature-v4-shape", fv4_a.shape == (80,)),
        ("feature-v4-finite", np.isfinite(fv4_a).all()),
        ("feature-v4-stable", np.allclose(fv4_a, fv4_b)),
    ]
    for name, ok in feature_checks:
        if not ok:
            print(f"FAIL {name}")
            failures += 1

    # feature v5 (temporal windows): shape, finiteness, determinism, and the
    # core promise — a PERSISTENT narrow peak scores higher persistence than
    # a one-frame transient peak.
    from ml.temporal import V5_DIM, V5_WINDOW, features_v5, temporal_descriptors
    rng5 = np.random.default_rng(5)
    base = np.abs(rng5.normal(0.05, 0.01, 2049))
    win_db = np.stack([20.0 * np.log10(np.maximum(
        base * (1.0 + rng5.normal(0, 0.05, base.shape)), 1e-6))
        for _ in range(V5_WINDOW)])
    bump = 18.0 * np.exp(-0.5 * ((np.arange(2049) - 40) / 4.0) ** 2)
    peaky = win_db + bump[None, :]                # persistent narrow peak
    transient = win_db.copy()
    transient[0] += bump                          # one-frame peak only
    v5_a = features_v5(win_db, 44100.0)
    v5_b = features_v5(win_db, 44100.0)
    pers_persistent = temporal_descriptors(peaky, 44100.0)[7]
    pers_transient = temporal_descriptors(transient, 44100.0)[7]
    v5_checks = [
        ("feature-v5-shape", v5_a.shape == (V5_DIM,)),
        ("feature-v5-finite", np.isfinite(v5_a).all()),
        ("feature-v5-stable", np.allclose(v5_a, v5_b)),
        ("feature-v5-persistence-separates",
         pers_persistent > pers_transient + 0.5),
    ]
    for name, ok in v5_checks:
        if not ok:
            print(f"FAIL {name}")
            failures += 1

    # M9.3 contrastive generator contract:
    # (a) empty root -> empty output, no crash
    # (b) _ring_window on a synthetic transient window produces a Resonance
    #     sample whose persistence descriptors dominate the raw window
    #     (the measured post-check guarantee)
    # (c) determinism: same seed -> identical output
    from ml.temporal import (_D_PERSFRAC, _D_PERSRES, _ring_window,
                             temporal_contrastive)
    from ml.dataset import problem_freq_ranges
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        (Path(td) / "clean_drums").mkdir(parents=True)
        tr_c, he_c = temporal_contrastive(Path(td), seed=1)
        empty_ok = (len(tr_c) == 0 and len(he_c) == 0)

    rng93 = np.random.default_rng(93)
    # transient-style window: broad noise floor with a one-frame hit
    base93 = np.abs(rng93.normal(0.05, 0.01, 2049))
    win93 = np.stack([20.0 * np.log10(np.maximum(
        base93 * (1.0 + rng93.normal(0, 0.05, base93.shape)), 1e-6))
        for _ in range(V5_WINDOW)])
    win93[0] += 12.0 * np.exp(-0.5 * ((np.arange(2049) - 60) / 5.0) ** 2)
    ranges93 = problem_freq_ranges("product-v2")
    ring93 = _ring_window(win93, 44100.0, np.random.default_rng(7),
                          ranges93, "contract")
    ring_ok = ring93 is not None and ring93.problem_targets[0] == 1.0
    ring_dom = False
    if ring93 is not None:
        raw_d = temporal_descriptors(win93, 44100.0)
        # features_v5 = 64 v1 dims + descriptors -> persFrac/persRes at 64+7/64+8
        ring_dom = (ring93.features[64 + _D_PERSFRAC] >= raw_d[_D_PERSFRAC]
                    and ring93.features[64 + _D_PERSRES] > raw_d[_D_PERSRES])
    ring93_b = _ring_window(win93, 44100.0, np.random.default_rng(7),
                            ranges93, "contract")
    ring_det = (ring93 is None and ring93_b is None) or (
        ring93 is not None and ring93_b is not None
        and np.allclose(ring93.features, ring93_b.features))

    m93_checks = [
        ("m93-contrastive-empty-root", empty_ok),
        ("m93-ring-labelled-resonance", ring_ok),
        ("m93-ring-dominates-raw", ring_dom),
        ("m93-ring-deterministic", ring_det),
    ]
    for name, ok in m93_checks:
        if not ok:
            print(f"FAIL {name}")
            failures += 1

    # M9.4 persistence -> Resonance skip-connection contract:
    # (a) after training steps on Resonance windows the skip weights move;
    # (b) at equal trunk input, HIGH persistence descriptors raise the
    #     Resonance prob vs NULL descriptors (the skip biases Res);
    # (c) the other 7 heads are untouched by the descriptor-only change
    #     (skip feeds logit 0 only — verified black-box here);
    # (d) v3 roundtrip preserves the skip; blobs WITHOUT the key load with
    #     zeros (pre-M9.4 forward preserved).
    from ml.temporal import V5_DIM as _V5D
    net94 = TwoStageEQNet(seed=9, input_dim=_V5D)
    rng94 = np.random.default_rng(94)
    w0 = net94.res_skip_w.copy()
    xb = rng94.normal(size=(16, _V5D))
    yb = np.zeros((16, NUM_PROBLEMS)); yb[:8, 0] = 1.0     # Resonance positives
    xb[:8, 64 + 7] = 1.0; xb[:8, 64 + 8] = 2.0             # high persFrac/persRes
    fb = np.zeros((16, NUM_PROBLEMS)); fb[:8, 0] = 0.5
    for _ in range(20):
        net94.train_batch(xb, yb, fb, lr=1e-3)
    skip_moved = not np.allclose(net94.res_skip_w, w0)

    # Isolate the SKIP path at a FIXED trunk output (counter-check fix, Manus +
    # my independent repro): a full forward(x_hi) would also route the raised
    # descriptors through the trunk t1, changing every head legitimately
    # (measured max|delta| ~0.039 on the other heads). Comparing _class_logits
    # at the SAME h2 leaves only the skip term, which feeds logit[0] only ->
    # delta 0.0 exactly on heads 1..7, positive on head 0.
    x_null = rng94.normal(size=(4, _V5D)); x_null[:, 64:] = 0.0
    x_hi = x_null.copy(); x_hi[:, 64 + 7] = 1.0; x_hi[:, 64 + 8] = 2.0
    _, _, _, h2_fixed = net94._forward_trunk(x_null)
    z_null = net94._class_logits(x_null, h2_fixed)
    z_hi = net94._class_logits(x_hi, h2_fixed)
    skip_biases_res = bool(np.all(z_hi[:, 0] > z_null[:, 0]))
    others_untouched = bool(np.allclose(z_hi[:, 1:], z_null[:, 1:]))

    tmp94 = Path("/tmp/aieq_m8/test_m94_skip.bin")
    blob_io.write_v3(net94, tmp94, 1, {"problem_schema": "product-v2"})
    got94, _, _prov94 = blob_io.read_v3(tmp94)
    skip_roundtrip = np.allclose(got94.res_skip_w, net94.res_skip_w)
    old94 = TwoStageEQNet.from_v3_layers(
        [(d.w, d.b) for d in net94.layers()], _V5D, net94.trunk,
        net94.presence_threshold, net94.class_thresholds)
    old_zeroed = bool(np.allclose(old94.res_skip_w, 0.0))

    m94_checks = [
        ("m94-skip-learns", skip_moved),
        ("m94-skip-biases-resonance", skip_biases_res),
        ("m94-other-heads-untouched", others_untouched),
        ("m94-skip-roundtrip", skip_roundtrip),
        ("m94-old-blob-zero-fallback", old_zeroed),
    ]
    for name, ok in m94_checks:
        if not ok:
            print(f"FAIL {name}")
            failures += 1

    total = (len(checks) + 3 + len(feature_checks) + len(v5_checks)
             + len(m93_checks) + len(m94_checks))
    print(f"{total - failures}/{total} M8 contract checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
