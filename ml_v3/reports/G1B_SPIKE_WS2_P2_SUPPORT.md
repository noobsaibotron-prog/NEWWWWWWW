# G1b Spike WS2 — P2 empty / single-bin support (geometry)

**Status:** evidence note only — not G1 PASS; not gate-4 numbers  
**Date:** 2026-07-25  
**Pin:** P2 ACCEPTED a priori (no v2 nearest-bin; empty → 0; Σw==0 after fuse → 1e-12)  
**Platform:** `/Users/marco/aieq_data/motore_v3/env/venv` CPython 3.12.13 / numpy 2.5.1  
**Source:** `ml_v3.frontend.offline_features.empty_support_report()` at `fs_c=48000`

## Measured counts (triangular log2 weights; not silence)

| Path | FFT | Empty-support bands | Single-bin bands |
|------|-----|---------------------|------------------|
| MAIN | 4096 | **14** | **21** |
| LF | 8192 | **6** | **17** |

### MAIN empty indices (14)

`0, 1, 4, 5, 6, 7, 8, 11, 12, 13, 16, 17, 20, 23`

### MAIN single-bin indices (21)

`2, 3, 9, 10, 14, 15, 18, 19, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 35, 36`

### LF empty indices (6)

`0, 1, 4, 5, 8, 11`

### LF single-bin indices (17)

`2, 3, 6, 7, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23`

## Watch-list

- Single-bin MAIN/LF bands are primary suspects if later gate-4 `max|Δ|` exceeds 0.25 dB.
- Geometry is identical across source rates (analysis always at 48 kHz) → P2 choice largely cancels in cross-rate Δ.
- Empty support must contribute **0** to Σ(w·psd); fused linear 0 → floor `1e-12` before dB/clamp. **No** nearest-bin.

## Reproduce

```bash
/Users/marco/aieq_data/motore_v3/env/venv/bin/python -c \
  'from ml_v3.frontend.offline_features import empty_support_report; import json; print(json.dumps(empty_support_report(), indent=2))'
```
