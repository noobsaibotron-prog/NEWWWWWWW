"""Numpy port of the MLEngine network stack (problemNet + freqNet).

Architecture (must stay in lockstep with Source/AI/MLEngine.cpp):
  problemNet: 64 -> 128 (ReLU) -> 64 (ReLU) -> 8 (sigmoid, multi-label)
  freqNet:    64 -> 32 (ReLU) -> 8 (sigmoid, per-class normalized log-freq)
  genreNet:   INTENTIONALLY ABSENT — never trained, never called in production
              (dropped from the v2 blob; the C++ v1 loader keeps reading the
               legacy genre weights only to skip them).

Training improvements over the in-plugin C++ SGD (offline-only, the plugin
stays inference-only):
  - Adam optimizer (deterministic, seeded)
  - mini-batches
  - BCE loss on the problem head with optional per-class weights and a
    false-positive weight multiplier (matches the in-repo training intent)
  - Huber loss on the freq head, masked to the active class (replaces the
    saturating MSE*sigmoid' of the C++ path)

Weight layout note: DenseLayer stores weights row-major [out, in] — identical
to the C++ ``weights[o * inputSize + i]``.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

MEL_BANDS = 64
NUM_PROBLEMS = 8

PROBLEM_NAMES = [
    "Resonance", "Harshness", "Muddiness", "Sibilance",
    "Boominess", "Thinness", "BoxyMidrange", "Clipping",
]

# Problem-slot schemas (M6). legacy-v1 = shipped blob semantics: slot 7 is
# Clipping, which AIEngine remaps to Harshness — so the product class DullSound
# is STRUCTURALLY unreachable under MLOnly. product-v2 assigns slot 7 to
# DullSound (loss of highs). Slots 0-6 are identical in both schemas.
SCHEMAS = {
    "legacy-v1": tuple(PROBLEM_NAMES),
    "product-v2": ("Resonance", "Harshness", "Muddiness", "Sibilance",
                   "Boominess", "Thinness", "BoxyMidrange", "DullSound"),
}


def problem_names(schema: str = "legacy-v1") -> tuple[str, ...]:
    return SCHEMAS[schema]


def problem_shapes(input_dim: int,
                   hidden: tuple[int, int] = (128, 64)) -> list[tuple[int, int]]:
    """Layer shapes (out, in). input_dim: 64 for feature v1, 65 for v2.
    hidden: (h1, h2) — (128, 64) is the shipped size; M7 capacity experiments
    may widen it (the v2 blob is self-describing, so loaders stay compatible)."""
    return [(hidden[0], input_dim), (hidden[1], hidden[0]),
            (NUM_PROBLEMS, hidden[1])]


def freq_shapes(input_dim: int) -> list[tuple[int, int]]:
    return [(32, input_dim), (NUM_PROBLEMS, 32)]


# Fixed v1-blob layouts (legacy compat only)
PROBLEM_SHAPES = problem_shapes(MEL_BANDS)
FREQ_SHAPES = freq_shapes(MEL_BANDS)
GENRE_SHAPES = [(64, MEL_BANDS), (8, 64)]  # legacy v1 blob layout only

# Hand-tuned fc3 bias init from MLEngine.cpp:219-230 (kept for parity when
# training from scratch; a loaded blob overrides it).
FC3_BIAS_INIT = np.array([-0.5, -0.3, -0.4, -0.3, -0.5, -0.6, -0.5, -1.0],
                         dtype=np.float64)


def _xavier(rng: np.random.Generator, out_size: int, in_size: int) -> np.ndarray:
    scale = np.sqrt(2.0 / float(in_size + out_size))
    return rng.normal(0.0, scale, size=(out_size, in_size))


def sigmoid(x: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-x))


@dataclass
class Dense:
    w: np.ndarray  # [out, in]
    b: np.ndarray  # [out]

    # Adam state
    m_w: np.ndarray = field(init=False)
    v_w: np.ndarray = field(init=False)
    m_b: np.ndarray = field(init=False)
    v_b: np.ndarray = field(init=False)

    def __post_init__(self):
        self.m_w = np.zeros_like(self.w)
        self.v_w = np.zeros_like(self.w)
        self.m_b = np.zeros_like(self.b)
        self.v_b = np.zeros_like(self.b)

    def forward(self, x: np.ndarray) -> np.ndarray:
        """x: [batch, in] -> [batch, out]"""
        return x @ self.w.T + self.b

    def adam_step(self, gw: np.ndarray, gb: np.ndarray, lr: float, t: int,
                  beta1: float = 0.9, beta2: float = 0.999, eps: float = 1e-8):
        self.m_w = beta1 * self.m_w + (1 - beta1) * gw
        self.v_w = beta2 * self.v_w + (1 - beta2) * gw * gw
        self.m_b = beta1 * self.m_b + (1 - beta1) * gb
        self.v_b = beta2 * self.v_b + (1 - beta2) * gb * gb
        mw_hat = self.m_w / (1 - beta1 ** t)
        vw_hat = self.v_w / (1 - beta2 ** t)
        mb_hat = self.m_b / (1 - beta1 ** t)
        vb_hat = self.v_b / (1 - beta2 ** t)
        self.w -= lr * mw_hat / (np.sqrt(vw_hat) + eps)
        self.b -= lr * mb_hat / (np.sqrt(vb_hat) + eps)


class EQNet:
    """problemNet + freqNet with shared mel(+level) input."""

    def __init__(self, seed: int = 22, input_dim: int = MEL_BANDS,
                 hidden: tuple[int, int] = (128, 64)):
        rng = np.random.default_rng(seed)
        ps = problem_shapes(input_dim, hidden)
        fs = freq_shapes(input_dim)
        self.input_dim = input_dim
        self.hidden = tuple(hidden)
        self.p1 = Dense(_xavier(rng, *ps[0]), np.zeros(ps[0][0]))
        self.p2 = Dense(_xavier(rng, *ps[1]), np.zeros(ps[1][0]))
        self.p3 = Dense(_xavier(rng, *ps[2]), FC3_BIAS_INIT.copy())
        self.f1 = Dense(_xavier(rng, *fs[0]), np.zeros(fs[0][0]))
        self.f2 = Dense(_xavier(rng, *fs[1]), np.zeros(fs[1][0]))
        self._t = 0  # Adam timestep

    # ------------------------------------------------------------------ fwd
    def forward_problem(self, x: np.ndarray) -> np.ndarray:
        """x: [batch, 64] -> sigmoid probs [batch, 8]"""
        h1 = np.maximum(self.p1.forward(x), 0.0)
        h2 = np.maximum(self.p2.forward(h1), 0.0)
        return sigmoid(self.p3.forward(h2))

    def forward_freq(self, x: np.ndarray) -> np.ndarray:
        h = np.maximum(self.f1.forward(x), 0.0)
        return sigmoid(self.f2.forward(h))

    # ---------------------------------------------------------------- train
    def train_batch(self, x: np.ndarray, y_prob: np.ndarray, y_freq: np.ndarray,
                    lr: float, fp_weight: float = 1.5,
                    pos_weight: float = 6.0,
                    class_weights: np.ndarray | None = None,
                    neg_class_weights: np.ndarray | None = None,
                    freq_loss_weight: float = 0.5,
                    huber_delta: float = 0.1,
                    ranking_weight: float = 0.0,
                    emission_targets: np.ndarray | None = None,
                    rank_margin: float = 0.05) -> dict:
        """One Adam step on a mini-batch.

        y_prob: [batch, 8] multi-label targets in {0,1}
        y_freq: [batch, 8] normalized log-freq targets (only the active class
                column carries signal; inactive columns are masked out)
        fp_weight: extra weight on negative-target errors (specificity bias,
                   mirrors the in-repo "FP weight 1.5x" intent)
        pos_weight: extra weight on POSITIVE-target cells. Each single-label
                    positive sample carries 1 positive vs 7 negative cells, so
                    without this the loss is dominated by "predict 0" and the
                    net collapses to all-negative (measured: macro-F1 = 0).
                    ~6x rebalances pos/neg gradient mass per batch.
        """
        n = x.shape[0]
        self._t += 1

        # ---- problem head forward (keep intermediates) ----
        z1 = self.p1.forward(x); h1 = np.maximum(z1, 0.0)
        z2 = self.p2.forward(h1); h2 = np.maximum(z2, 0.0)
        z3 = self.p3.forward(h2); probs = sigmoid(z3)

        # BCE-through-sigmoid gradient: dL/dz3 = probs - y  (weighted)
        w = np.ones_like(y_prob)
        pos_mask = y_prob >= 0.5
        w[~pos_mask] = fp_weight
        w[pos_mask] = pos_weight
        if class_weights is not None:       # scales POSITIVE cells per class
            w = np.where(pos_mask, w * class_weights[None, :], w)
        if neg_class_weights is not None:   # scales NEGATIVE cells per class
            w = np.where(~pos_mask, w * neg_class_weights[None, :], w)
        d3 = (probs - y_prob) * w / (n * NUM_PROBLEMS)

        # ── M7 product-aware ranking shaping (opt-in; 0.0 = byte-identical) ──
        # The plugin emits a class only if prob >= threshold+margin AND it
        # ranks top-2 by raw prob (MLEngine::detectProblems). BCE optimizes
        # neither. On POSITIVE cells only:
        #  (a) emission hinge: if p_t is below its product operating point
        #      (threshold+margin+slack), push z_t up;
        #  (b) rank hinge: if p_t does not beat the 2nd-highest competitor by
        #      rank_margin, push z_t up AND that competitor down.
        # Diagnosed 2026-07-07: clip06 DullSound lost@margin 288/327, clip03
        # Muddiness lost@rank 228/327, V-SIB dead@threshold — (a)+(b) target
        # exactly those failure modes. Negatives keep plain BCE (clean floor).
        if ranking_weight > 0.0 and emission_targets is not None:
            sig_grad = probs * (1.0 - probs)
            radd = np.zeros_like(d3)
            pos_rows = np.where(pos_mask.any(axis=1))[0]
            for r in pos_rows:
                for t in np.where(pos_mask[r])[0]:
                    p_t = probs[r, t]
                    if p_t < emission_targets[t]:            # (a) emission
                        radd[r, t] -= sig_grad[r, t]
                    order = np.argsort(-probs[r])
                    comp = [j for j in order if j != t][:2]  # top-2 competitors
                    if len(comp) >= 2:
                        o2 = int(comp[1])
                        if p_t < probs[r, o2] + rank_margin:  # (b) rank
                            radd[r, t] -= sig_grad[r, t]
                            radd[r, o2] += sig_grad[r, o2]
            if pos_rows.size:
                d3 = d3 + ranking_weight * radd / (pos_rows.size * NUM_PROBLEMS)

        gw3 = d3.T @ h2
        gb3 = d3.sum(axis=0)
        d2 = (d3 @ self.p3.w) * (z2 > 0.0)
        gw2 = d2.T @ h1
        gb2 = d2.sum(axis=0)
        d1 = (d2 @ self.p2.w) * (z1 > 0.0)
        gw1 = d1.T @ x
        gb1 = d1.sum(axis=0)

        # ---- freq head forward ----
        zf1 = self.f1.forward(x); hf1 = np.maximum(zf1, 0.0)
        zf2 = self.f2.forward(hf1); fpred = sigmoid(zf2)

        # Huber on active-class columns only, gradient through sigmoid
        mask = (y_prob > 0.5).astype(np.float64)
        diff = fpred - y_freq
        huber_grad = np.where(np.abs(diff) <= huber_delta, diff,
                              huber_delta * np.sign(diff))
        active = max(mask.sum(), 1.0)
        df2 = freq_loss_weight * mask * huber_grad * fpred * (1.0 - fpred) / active

        gwf2 = df2.T @ hf1
        gbf2 = df2.sum(axis=0)
        df1 = (df2 @ self.f2.w) * (zf1 > 0.0)
        gwf1 = df1.T @ x
        gbf1 = df1.sum(axis=0)

        # ---- Adam updates ----
        self.p1.adam_step(gw1, gb1, lr, self._t)
        self.p2.adam_step(gw2, gb2, lr, self._t)
        self.p3.adam_step(gw3, gb3, lr, self._t)
        self.f1.adam_step(gwf1, gbf1, lr, self._t)
        self.f2.adam_step(gwf2, gbf2, lr, self._t)

        # ---- losses (reporting only) ----
        eps = 1e-7
        bce = -(w * (y_prob * np.log(probs + eps)
                     + (1 - y_prob) * np.log(1 - probs + eps))).mean()
        ad = np.abs(diff) * mask
        huber = np.where(ad <= huber_delta, 0.5 * ad * ad,
                         huber_delta * (ad - 0.5 * huber_delta)).sum() / active
        return {"bce": float(bce), "freq_huber": float(huber)}

    # ---------------------------------------------------------------- io
    def layers_in_v1_order(self) -> list[tuple[np.ndarray, np.ndarray]]:
        """v1 blob order: p1, p2, p3, genre1, genre2, f1, f2.
        Genre layers are synthesized as zeros (dead ballast in v1).
        Only valid for input_dim == 64 (v1 features) and the shipped 128/64
        hidden sizes (the v1 layout is FIXED — bigger nets need the v2 loader)."""
        if self.input_dim != MEL_BANDS:
            raise ValueError("v1 blob requires input_dim == 64 (feature v1)")
        if self.hidden != (128, 64):
            raise ValueError("v1 blob requires the shipped 128/64 hidden sizes")
        g1 = (np.zeros(GENRE_SHAPES[0]), np.zeros(GENRE_SHAPES[0][0]))
        g2 = (np.zeros(GENRE_SHAPES[1]), np.zeros(GENRE_SHAPES[1][0]))
        return [(self.p1.w, self.p1.b), (self.p2.w, self.p2.b),
                (self.p3.w, self.p3.b), g1, g2,
                (self.f1.w, self.f1.b), (self.f2.w, self.f2.b)]

    def layers_in_v2_order(self) -> list[tuple[np.ndarray, np.ndarray]]:
        """v2 blob order: p1, p2, p3, f1, f2 (genreNet dropped)."""
        return [(self.p1.w, self.p1.b), (self.p2.w, self.p2.b),
                (self.p3.w, self.p3.b),
                (self.f1.w, self.f1.b), (self.f2.w, self.f2.b)]

    @staticmethod
    def from_v1_blob(layers: list[tuple[np.ndarray, np.ndarray]]) -> "EQNet":
        """Build a net from the 7 layers of a parsed v1 blob (genre discarded)."""
        net = EQNet(seed=0)
        (net.p1.w, net.p1.b) = layers[0]
        (net.p2.w, net.p2.b) = layers[1]
        (net.p3.w, net.p3.b) = layers[2]
        # layers[3], layers[4] = genre (discarded)
        (net.f1.w, net.f1.b) = layers[5]
        (net.f2.w, net.f2.b) = layers[6]
        for d in (net.p1, net.p2, net.p3, net.f1, net.f2):
            d.__post_init__()  # reset Adam state to match new shapes
        return net


# ---------------------------------------------------------------------------
# M8 lab architecture: presence gate + independent per-class heads.
#
# This is intentionally separate from EQNet. The shipped/v1/v2 model paths stay
# unchanged, and a future production loader must opt into this architecture.
M8_ARCH = "two-stage-per-class"


def m8_shapes(input_dim: int,
              trunk: tuple[int, int] = (192, 96)) -> list[tuple[int, int]]:
    return [
        (trunk[0], input_dim),
        (trunk[1], trunk[0]),
        (1, trunk[1]),
        (NUM_PROBLEMS, trunk[1]),
        (NUM_PROBLEMS, trunk[1]),
    ]


class TwoStageEQNet:
    """M8 redesign: shared trunk, any-problem gate, independent class heads.

    Emission is intentionally non-competitive:
        presence >= presence_threshold AND class_prob[c] >= class_threshold[c]

    Any display cap is applied only after thresholds. This avoids the M6/M7
    failure mode where one strong class suppresses another through a top-2 gate.
    """

    def __init__(self, seed: int = 22, input_dim: int = MEL_BANDS,
                 trunk: tuple[int, int] = (192, 96)):
        rng = np.random.default_rng(seed)
        shapes = m8_shapes(input_dim, trunk)
        self.input_dim = input_dim
        self.trunk = tuple(trunk)
        self.t1 = Dense(_xavier(rng, *shapes[0]), np.zeros(shapes[0][0]))
        self.t2 = Dense(_xavier(rng, *shapes[1]), np.zeros(shapes[1][0]))
        self.presence = Dense(_xavier(rng, *shapes[2]), np.array([-0.75]))
        self.classes = Dense(_xavier(rng, *shapes[3]),
                             np.full(NUM_PROBLEMS, -0.75))
        self.freqs = Dense(_xavier(rng, *shapes[4]), np.zeros(NUM_PROBLEMS))
        self.presence_threshold = 0.5
        self.class_thresholds = np.full(NUM_PROBLEMS, 0.5, dtype=np.float64)
        # M9.2: emission semantics carried by the model itself (serialized in
        # the v3 provenance). "legacy" keeps M8 behaviour byte-identical;
        # "per-class" drops the global presence gate (measured M8 round-6:
        # the gate blinds healthy class heads on temporally-static problems).
        self.emission_mode = "legacy"
        self.override_delta: float | None = None
        # M9.4: persistence -> Resonance skip-connection. The M8 verdict
        # measured that the Resonance head absorbs the ring into Dull/Harsh
        # because it only sees the trunk mix; the v5 persistence descriptors
        # (persFrac/persRes, inputs 64+7 / 64+8) carry the ring signature
        # directly. The skip adds x[:, -10:] @ w to the RESONANCE LOGIT ONLY —
        # the other 7 heads keep an unchanged path (surgical, axis-08 fix).
        # Active only for v5 inputs (input_dim == 64+10); small non-zero init
        # so the gradient can pick a direction immediately.
        self._skip_dims = 10 if input_dim == MEL_BANDS + 10 else 0
        self.res_skip_w = (rng.normal(0.0, 0.01, self._skip_dims)
                           if self._skip_dims else np.zeros(0))
        self._skip_m = np.zeros_like(self.res_skip_w)   # Adam-lite state
        self._skip_v = np.zeros_like(self.res_skip_w)
        self._t = 0

    def layers(self) -> list[Dense]:
        return [self.t1, self.t2, self.presence, self.classes, self.freqs]

    def layers_in_v3_order(self) -> list[tuple[np.ndarray, np.ndarray]]:
        return [(d.w, d.b) for d in self.layers()]

    def _forward_trunk(self, x: np.ndarray) -> tuple[np.ndarray, ...]:
        z1 = self.t1.forward(x)
        h1 = np.maximum(z1, 0.0)
        z2 = self.t2.forward(h1)
        h2 = np.maximum(z2, 0.0)
        return z1, h1, z2, h2

    def _class_logits(self, x: np.ndarray, h2: np.ndarray) -> np.ndarray:
        """Class logits = trunk heads + the M9.4 Resonance persistence skip.
        Single source of truth: forward() and train_batch() must both use it."""
        z = self.classes.forward(h2)
        if self._skip_dims:
            z[:, 0] = z[:, 0] + x[:, -self._skip_dims:] @ self.res_skip_w
        return z

    def forward(self, x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        _, _, _, h2 = self._forward_trunk(x)
        presence = sigmoid(self.presence.forward(h2))[:, 0]
        probs = sigmoid(self._class_logits(x, h2))
        return presence, probs

    def forward_problem(self, x: np.ndarray) -> np.ndarray:
        """Compatibility with existing raw evaluators: returns class heads only."""
        return self.forward(x)[1]

    def forward_freq(self, x: np.ndarray) -> np.ndarray:
        _, _, _, h2 = self._forward_trunk(x)
        return sigmoid(self.freqs.forward(h2))

    def emitted(self, x: np.ndarray, ui_cap: int = 3,
                override_delta: float | None = None) -> np.ndarray:
        """override_delta (M8 round-6 finding, measured): the global presence
        gate BLINDS confident class heads on temporally-static problems (harsh
        synth clip 05: Harshness head p50 0.76 but presence max 0.43 -> zero
        emission). With override_delta set, a class whose prob clears its own
        threshold by that margin emits even below the presence gate (rescued 05
        to 94-99% in the round-6 measurement). Default None = plan behaviour."""
        presence, probs = self.forward(x)
        # Explicit argument wins over serialized instance state (keeps every
        # existing diagnostic call-site byte-identical in behaviour).
        active_override = (override_delta if override_delta is not None
                           else self.override_delta)
        if self.emission_mode == "per-class":
            # M9.2: no global presence gate — each class emits on its own
            # calibrated threshold.
            hits = probs >= self.class_thresholds[None, :]
        else:
            hits = ((presence >= self.presence_threshold)[:, None]
                    & (probs >= self.class_thresholds[None, :]))
            if active_override is not None:
                hits |= probs >= (self.class_thresholds[None, :]
                                  + active_override)
        if 0 < ui_cap < NUM_PROBLEMS:
            order = np.argsort(-probs, axis=1)
            keep = np.zeros_like(hits)
            rows = np.arange(probs.shape[0])[:, None]
            keep[rows, order[:, :ui_cap]] = True
            hits &= keep
        return hits

    def train_batch(self, x: np.ndarray, y_prob: np.ndarray, y_freq: np.ndarray,
                    lr: float,
                    presence_weight: float = 1.0,
                    class_weight: float = 1.0,
                    freq_loss_weight: float = 0.5,
                    class_pos_weight: float = 4.0,
                    class_weights: np.ndarray | None = None,
                    neg_class_weights: np.ndarray | None = None,
                    margin_weight: float = 0.5,
                    margin_target: float = 0.70,
                    huber_delta: float = 0.1) -> dict:
        n = x.shape[0]
        self._t += 1

        z1, h1, z2, h2 = self._forward_trunk(x)
        y_any = (y_prob.max(axis=1) >= 0.5).astype(np.float64)
        presence = sigmoid(self.presence.forward(h2))[:, 0]
        probs = sigmoid(self._class_logits(x, h2))   # M9.4: includes Res skip
        freqs = sigmoid(self.freqs.forward(h2))

        d_presence = (presence_weight * (presence - y_any) / n)[:, None]

        pos_mask = y_prob >= 0.5
        w = np.ones_like(y_prob)
        w[pos_mask] = class_pos_weight
        if class_weights is not None:
            w = np.where(pos_mask, w * class_weights[None, :], w)
        if neg_class_weights is not None:
            w = np.where(~pos_mask, w * neg_class_weights[None, :], w)
        d_class = class_weight * (probs - y_prob) * w / (n * NUM_PROBLEMS)

        if margin_weight > 0.0:
            violated = pos_mask & (probs < margin_target)
            # Hinge on probability, backpropagated through sigmoid.
            d_class -= (margin_weight * violated * probs * (1.0 - probs)
                        / max(1, int(pos_mask.sum())))

        mask = pos_mask.astype(np.float64)
        diff = freqs - y_freq
        huber_grad = np.where(np.abs(diff) <= huber_delta, diff,
                              huber_delta * np.sign(diff))
        active = max(mask.sum(), 1.0)
        d_freq = (freq_loss_weight * mask * huber_grad * freqs * (1.0 - freqs)
                  / active)

        gw_presence = d_presence.T @ h2
        gb_presence = d_presence.sum(axis=0)
        gw_class = d_class.T @ h2
        gb_class = d_class.sum(axis=0)
        gw_freq = d_freq.T @ h2
        gb_freq = d_freq.sum(axis=0)

        dh2 = (d_presence @ self.presence.w
               + d_class @ self.classes.w
               + d_freq @ self.freqs.w) * (z2 > 0.0)
        gw_t2 = dh2.T @ h1
        gb_t2 = dh2.sum(axis=0)
        dh1 = (dh2 @ self.t2.w) * (z1 > 0.0)
        gw_t1 = dh1.T @ x
        gb_t1 = dh1.sum(axis=0)

        self.t1.adam_step(gw_t1, gb_t1, lr, self._t)
        self.t2.adam_step(gw_t2, gb_t2, lr, self._t)
        self.presence.adam_step(gw_presence, gb_presence, lr, self._t)
        self.classes.adam_step(gw_class, gb_class, lr, self._t)

        # M9.4: Resonance persistence-skip update (Adam-lite on 10 params).
        # d_class is dL/dz_class and the skip feeds z_class[:, 0] directly,
        # so its gradient is d_class[:, 0] @ x_skip — no trunk interaction.
        if self._skip_dims:
            g_skip = d_class[:, 0] @ x[:, -self._skip_dims:]
            self._skip_m = 0.9 * self._skip_m + 0.1 * g_skip
            self._skip_v = 0.999 * self._skip_v + 0.001 * g_skip * g_skip
            m_hat = self._skip_m / (1.0 - 0.9 ** self._t)
            v_hat = self._skip_v / (1.0 - 0.999 ** self._t)
            self.res_skip_w -= lr * m_hat / (np.sqrt(v_hat) + 1e-8)
        self.freqs.adam_step(gw_freq, gb_freq, lr, self._t)

        eps = 1e-7
        bce_presence = -np.mean(y_any * np.log(presence + eps)
                                + (1.0 - y_any) * np.log(1.0 - presence + eps))
        bce_class = -np.mean(w * (y_prob * np.log(probs + eps)
                                  + (1.0 - y_prob) * np.log(1.0 - probs + eps)))
        return {
            "bce": float(bce_class),
            "bce_presence": float(bce_presence),
        }

    @staticmethod
    def from_v3_layers(layers: list[tuple[np.ndarray, np.ndarray]],
                       input_dim: int,
                       trunk: tuple[int, int],
                       presence_threshold: float,
                       class_thresholds: np.ndarray,
                       emission_mode: str = "legacy",
                       override_delta: float | None = None,
                       res_skip_w: list[float] | None = None) -> "TwoStageEQNet":
        if emission_mode not in ("legacy", "per-class"):
            raise ValueError(f"unknown emission_mode: {emission_mode!r}")
        net = TwoStageEQNet(seed=0, input_dim=input_dim, trunk=trunk)
        for dense, (w, b) in zip(net.layers(), layers):
            dense.w = w
            dense.b = b
            dense.__post_init__()
        net.presence_threshold = float(presence_threshold)
        net.class_thresholds = np.asarray(class_thresholds, dtype=np.float64)
        net.emission_mode = emission_mode
        net.override_delta = (None if override_delta is None
                              else float(override_delta))
        # M9.4: restore the persistence skip; OLD v3 blobs (no key) get zeros
        # so their forward stays byte-identical to pre-M9.4 behaviour.
        if net._skip_dims:
            if res_skip_w is not None and len(res_skip_w) == net._skip_dims:
                net.res_skip_w = np.asarray(res_skip_w, dtype=np.float64)
            else:
                net.res_skip_w = np.zeros(net._skip_dims)
            net._skip_m = np.zeros_like(net.res_skip_w)
            net._skip_v = np.zeros_like(net.res_skip_w)
        return net
