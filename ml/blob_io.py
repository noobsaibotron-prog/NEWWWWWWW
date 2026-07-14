"""MLEQ weight-blob I/O — v1 (shipped), v2, and v3 lab blobs.

v1 layout (little-endian, matches MLEngine::saveWeights / loadWeights):
    u32 magic = 0x4D4C4551 ("MLEQ")
    u32 version = 1
    f32 payload, fixed order & sizes (24,120 floats / 96,488 bytes total):
        problemNet_fc1 w[128*64] b[128]
        problemNet_fc2 w[64*128] b[64]
        problemNet_fc3 w[8*64]   b[8]
        genreNet_fc1   w[64*64]  b[64]
        genreNet_fc2   w[8*64]   b[8]
        freqNet_fc1    w[32*64]  b[32]
        freqNet_fc2    w[8*32]   b[8]

v2 layout (little-endian; genreNet DROPPED; self-describing + integrity):
    u32 magic = 0x4D4C4551
    u32 version = 2
    u32 featureVersion          (1 = absolute-dB map, 2 = frame-mean-normalized)
    u32 numLayers               (5)
    per layer:
        u32 outSize, u32 inSize
        f32 w[out*in]  (row-major [out][in], same as DenseLayer)
        f32 b[out]
    u32 provenanceLen
    u8  provenanceJson[provenanceLen]   (UTF-8; dataset hash, date, git, metrics)
    u64 fnv1a64                          (over ALL preceding bytes)

The C++ counterpart (MLEngine::loadWeights v2 branch) must stay in lockstep.
v3 is M8 LAB-ONLY until a separate production loader ticket exists.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

import numpy as np

from .model import (M8_ARCH, NUM_PROBLEMS, EQNet, FREQ_SHAPES, GENRE_SHAPES,
                    PROBLEM_SHAPES, TwoStageEQNet, m8_shapes)

MAGIC = 0x4D4C4551
V1_TOTAL_FLOATS = 24_120
V1_TOTAL_BYTES = 8 + V1_TOTAL_FLOATS * 4  # 96,488

# IMPORTANT: MLEngine.cpp uses offset basis 1469598103934665603 — one digit
# short of the standard FNV-1a basis 14695981039346656037. The quirk is
# self-consistent across the C++ loader, MLWeightsProvenanceTest and this
# module; keep all three in lockstep (do not "fix" it in one place only).
FNV_OFFSET = 1469598103934665603
FNV_PRIME = 0x100000001b3
MASK64 = 0xFFFFFFFFFFFFFFFF


def fnv1a64(data: bytes) -> int:
    h = FNV_OFFSET
    for byte in data:
        h ^= byte
        h = (h * FNV_PRIME) & MASK64
    return h


# --------------------------------------------------------------------- v1
def read_v1(path: Path) -> EQNet:
    data = path.read_bytes()
    if len(data) < V1_TOTAL_BYTES:
        raise ValueError(f"v1 blob too small: {len(data)} < {V1_TOTAL_BYTES}")
    if len(data) > V1_TOTAL_BYTES:
        print(f"WARNING: trailing data ({len(data)} bytes vs {V1_TOTAL_BYTES}); "
              "reading first record only")
    magic, version = struct.unpack_from("<II", data, 0)
    if magic != MAGIC:
        raise ValueError(f"bad magic {magic:#x}")
    if version != 1:
        raise ValueError(f"expected v1, got version {version}")

    offset = 8
    layers: list[tuple[np.ndarray, np.ndarray]] = []
    for out_size, in_size in PROBLEM_SHAPES + GENRE_SHAPES + FREQ_SHAPES:
        n_w = out_size * in_size
        w = np.frombuffer(data, dtype="<f4", count=n_w, offset=offset)
        offset += n_w * 4
        b = np.frombuffer(data, dtype="<f4", count=out_size, offset=offset)
        offset += out_size * 4
        layers.append((w.reshape(out_size, in_size).astype(np.float64),
                       b.astype(np.float64)))
    return EQNet.from_v1_blob(layers)


def write_v1(net: EQNet, path: Path) -> None:
    """Export in the SHIPPED v1 format (genre layers zero-filled)."""
    buf = bytearray(struct.pack("<II", MAGIC, 1))
    for w, b in net.layers_in_v1_order():
        buf += w.astype("<f4").tobytes()
        buf += b.astype("<f4").tobytes()
    assert len(buf) == V1_TOTAL_BYTES, len(buf)
    path.write_bytes(bytes(buf))


# --------------------------------------------------------------------- v2
def write_v2(net: EQNet, path: Path, feature_version: int,
             provenance: dict) -> None:
    layers = net.layers_in_v2_order()
    buf = bytearray(struct.pack("<IIII", MAGIC, 2, feature_version, len(layers)))
    for w, b in layers:
        out_size, in_size = w.shape
        buf += struct.pack("<II", out_size, in_size)
        buf += w.astype("<f4").tobytes()
        buf += b.astype("<f4").tobytes()
    prov_json = json.dumps(provenance, sort_keys=True,
                           separators=(",", ":")).encode("utf-8")
    buf += struct.pack("<I", len(prov_json))
    buf += prov_json
    buf += struct.pack("<Q", fnv1a64(bytes(buf)))
    path.write_bytes(bytes(buf))


def read_v2(path: Path) -> tuple[EQNet, int, dict]:
    """Returns (net, feature_version, provenance). Verifies the checksum."""
    data = path.read_bytes()
    if len(data) < 24:
        raise ValueError("v2 blob truncated")
    (stored,) = struct.unpack_from("<Q", data, len(data) - 8)
    if fnv1a64(data[:-8]) != stored:
        raise ValueError("v2 checksum mismatch — corrupt blob")

    magic, version, feature_version, num_layers = struct.unpack_from("<IIII", data, 0)
    if magic != MAGIC or version != 2:
        raise ValueError(f"not a v2 blob (magic={magic:#x}, version={version})")

    offset = 16
    layers: list[tuple[np.ndarray, np.ndarray]] = []
    for _ in range(num_layers):
        out_size, in_size = struct.unpack_from("<II", data, offset)
        offset += 8
        n_w = out_size * in_size
        w = np.frombuffer(data, dtype="<f4", count=n_w, offset=offset)
        offset += n_w * 4
        b = np.frombuffer(data, dtype="<f4", count=out_size, offset=offset)
        offset += out_size * 4
        layers.append((w.reshape(out_size, in_size).astype(np.float64),
                       b.astype(np.float64)))

    (prov_len,) = struct.unpack_from("<I", data, offset)
    offset += 4
    provenance = json.loads(data[offset:offset + prov_len].decode("utf-8"))

    from .model import freq_shapes, problem_shapes
    input_dim = layers[0][0].shape[1]
    # hidden sizes come from the blob itself (self-describing v2 layout);
    # (128, 64) is the shipped default, M7 capacity blobs may differ.
    hidden = (layers[0][0].shape[0], layers[1][0].shape[0])
    expected = [tuple(s) for s in problem_shapes(input_dim, hidden)
                + freq_shapes(input_dim)]
    got = [w.shape for w, _ in layers]
    if got != expected:
        raise ValueError(f"layer shapes {got} != expected {expected}")

    net = EQNet(seed=0, input_dim=input_dim, hidden=hidden)
    (net.p1.w, net.p1.b) = layers[0]
    (net.p2.w, net.p2.b) = layers[1]
    (net.p3.w, net.p3.b) = layers[2]
    (net.f1.w, net.f1.b) = layers[3]
    (net.f2.w, net.f2.b) = layers[4]
    for d in (net.p1, net.p2, net.p3, net.f1, net.f2):
        d.__post_init__()
    return net, feature_version, provenance


# --------------------------------------------------------------------- v3
def write_v3(net: TwoStageEQNet, path: Path, feature_version: int,
             provenance: dict) -> None:
    """Write the M8 two-stage lab format.

    Layout intentionally mirrors v2's self-describing layer records, but the
    architecture is different and must not be read by a v2/production loader.
    """
    prov = dict(provenance)
    prov["schema"] = "mleq-v3"
    prov["architecture"] = M8_ARCH
    prov["trunk"] = list(net.trunk)
    prov["presence_threshold"] = float(net.presence_threshold)
    prov["class_thresholds"] = [float(v) for v in net.class_thresholds]
    # M9.2: emission semantics travel with the blob (provenance JSON is
    # self-describing — no binary layout change, old v3 blobs keep loading
    # with the "legacy" fallback in read_v3).
    prov["emission_mode"] = net.emission_mode
    if net.override_delta is not None:
        prov["override_delta"] = float(net.override_delta)
    # M9.4: the 10 persistence-skip weights travel in the provenance JSON
    # (self-describing, no binary layout change; old blobs without the key
    # load with zeros = pre-M9.4 forward, byte-identical).
    if getattr(net, "_skip_dims", 0):
        prov["res_skip_w"] = [float(v) for v in net.res_skip_w]

    layers = net.layers_in_v3_order()
    buf = bytearray(struct.pack("<IIII", MAGIC, 3, feature_version, len(layers)))
    for w, b in layers:
        out_size, in_size = w.shape
        buf += struct.pack("<II", out_size, in_size)
        buf += w.astype("<f4").tobytes()
        buf += b.astype("<f4").tobytes()
    prov_json = json.dumps(prov, sort_keys=True,
                           separators=(",", ":")).encode("utf-8")
    buf += struct.pack("<I", len(prov_json))
    buf += prov_json
    buf += struct.pack("<Q", fnv1a64(bytes(buf)))
    path.write_bytes(bytes(buf))


def read_v3(path: Path) -> tuple[TwoStageEQNet, int, dict]:
    """Returns (TwoStageEQNet, feature_version, provenance)."""
    data = path.read_bytes()
    if len(data) < 24:
        raise ValueError("v3 blob truncated")
    (stored,) = struct.unpack_from("<Q", data, len(data) - 8)
    if fnv1a64(data[:-8]) != stored:
        raise ValueError("v3 checksum mismatch — corrupt blob")

    magic, version, feature_version, num_layers = struct.unpack_from("<IIII", data, 0)
    if magic != MAGIC or version != 3:
        raise ValueError(f"not a v3 blob (magic={magic:#x}, version={version})")

    offset = 16
    layers: list[tuple[np.ndarray, np.ndarray]] = []
    for _ in range(num_layers):
        out_size, in_size = struct.unpack_from("<II", data, offset)
        offset += 8
        n_w = out_size * in_size
        w = np.frombuffer(data, dtype="<f4", count=n_w, offset=offset)
        offset += n_w * 4
        b = np.frombuffer(data, dtype="<f4", count=out_size, offset=offset)
        offset += out_size * 4
        layers.append((w.reshape(out_size, in_size).astype(np.float64),
                       b.astype(np.float64)))

    (prov_len,) = struct.unpack_from("<I", data, offset)
    offset += 4
    provenance = json.loads(data[offset:offset + prov_len].decode("utf-8"))
    if provenance.get("architecture") != M8_ARCH:
        raise ValueError(f"v3 architecture {provenance.get('architecture')} != {M8_ARCH}")

    input_dim = layers[0][0].shape[1]
    trunk = (layers[0][0].shape[0], layers[1][0].shape[0])
    expected = [tuple(s) for s in m8_shapes(input_dim, trunk)]
    got = [w.shape for w, _ in layers]
    if got != expected:
        raise ValueError(f"layer shapes {got} != expected {expected}")

    presence_threshold = float(provenance.get("presence_threshold", 0.5))
    class_thresholds = np.asarray(
        provenance.get("class_thresholds", [0.5] * NUM_PROBLEMS),
        dtype=np.float64)
    if class_thresholds.shape != (NUM_PROBLEMS,):
        raise ValueError(f"class_thresholds shape {class_thresholds.shape} != ({NUM_PROBLEMS},)")

    emission_mode = provenance.get("emission_mode", "legacy")
    override_delta = provenance.get("override_delta")
    res_skip_w = provenance.get("res_skip_w")   # M9.4 (absent in old blobs)

    net = TwoStageEQNet.from_v3_layers(
        layers, input_dim, trunk, presence_threshold, class_thresholds,
        emission_mode=emission_mode, override_delta=override_delta,
        res_skip_w=res_skip_w)
    return net, feature_version, provenance
