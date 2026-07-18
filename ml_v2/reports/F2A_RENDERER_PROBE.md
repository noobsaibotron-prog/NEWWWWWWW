# F2a renderer probe

Verdict: **NO-GO**

GO permits only a separately gated F2b waveform-renderer ablation; it is not evidence of model quality or production readiness.

| Material | Feature RMS median | Feature cases over floor | Qualifying models | Result |
|---|---:|---:|---:|---|
| clean_bass | 0.000240 | 62.5% | 0/3 | FAIL |
| clean_mix | 0.000650 | 62.5% | 0/3 | FAIL |
| clean_synth | 0.000376 | 70.8% | 0/3 | FAIL |
| pink | 0.000212 | 62.5% | 0/3 | FAIL |

| Class | dB effect | Waveform effect | Renderer delta | Delta/effect |
|---|---:|---:|---:|---:|
| Resonance | 0.010897 | 0.010969 | 0.000134 | 1.22% |
| Harshness | 0.016675 | 0.016677 | 0.000005 | 0.03% |
| Muddiness | 0.269387 | 0.268662 | 0.000573 | 0.21% |
| Sibilance | 0.007013 | 0.006992 | 0.000002 | 0.02% |
| Boominess | 0.003528 | 0.003756 | 0.000136 | 3.62% |
| Thinness | 0.079984 | 0.079639 | 0.000221 | 0.28% |
| BoxyMidrange | 0.130254 | 0.130604 | 0.000214 | 0.16% |
| DullSound | 0.002057 | 0.002061 | 0.000004 | 0.18% |

A NO-GO here means the waveform renderer does not enter F2b. The existing dB renderer remains the controlled baseline; other F2b interventions must be tested independently.

## Frozen inputs

- Round 5 seed 42: `3ed7f35d63376b380a7a3eea462781c9a3b6ab8584ae3ecec539a56e438f717a`
- Round 5 seed 1337: `1e477408f21a7eeec99efac56ab16452c85d0b785da5bbe5f94de6cf1c56a314`
- Round 5 seed 2026: `f03b05dd5a68bac86c56211e3d0a81ad6538c5c1e85ef9089127938511debc2c`
- pink: `8a8bc96eb9f3b33080326b3681525d417773789dfdcd3fc748758aaad4ae38a7` (f2a:pink-20260718, 44100 Hz)
- clean_bass: `2f3b40e3810cdbdadc43812324e1259dad80660ab2ef7ccf8547304d227f3350` (fsld:348382, 44100 Hz)
- clean_synth: `1857faeb2dd31a530c20b2bec85bdaf97360b098968c525c44b66ff4b5f7f8cf` (fsld:100902, 44100 Hz)
- clean_mix: `255336885715e80717b92a44164feb25897c40d95105f50f11a3d2c32be81938` (fsld:171596, 44100 Hz)

The complete per-window feature and probability values are in the paired JSON report.
Full JSON SHA-256: `5b2e11a9cf462099244013de2e7c18fca13ae1359f721ce09ae654531b58c687`

## Reproduce

```bash
/Users/marco/aieq_data/env/a4b-venv/bin/python -m ml_v2.renderer_probe \
  --out /tmp/aieq_v2/f2a_renderer_probe/report.json \
  --markdown-out ml_v2/reports/F2A_RENDERER_PROBE.md
```
