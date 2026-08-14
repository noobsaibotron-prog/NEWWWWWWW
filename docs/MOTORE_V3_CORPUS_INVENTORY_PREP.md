# Motore v3 - Corpus inventory preflight

## Purpose

This package prepares immutable facts about locally available audio before G3
corpus admission. It is intentionally independent from G1c, REV8, model
training, benchmark membership and product runtime.

It records:

- source identity and local root;
- rights status as `verified`, `quarantine` or `unknown`;
- SHA-256 of every rights-evidence file;
- relative audio path, byte size and SHA-256;
- codec, container, channels, sample rate and exact rational duration;
- probe backend and `ffprobe` version when non-WAV formats are present;
- byte-identical duplicates within and across source roots;
- ignored AppleDouble `._*` metadata files, counted explicitly;
- probe failures.

It does **not** assign:

- train, validation, calibration or test roles;
- the controlled `source_family` vocabulary used later by admission and HMAC;
- benchmark families;
- profiles or electronic subgenres;
- labels, annotations, targets or actionability;
- eligibility for commercial training.

Those decisions remain owned by the active V3 contracts and admission process.

## Isolation

The implementation lives under `ml_v3/prep/` as a lab-only preparation tool.
It remains independent from admission, evaluator and product code.
It does not modify:

- `Source/`, `Resources/`, `CMakeLists.txt` or plugin installations;
- `docs/MOTORE_V3_G1_CONTRACT.md` or `docs/MOTORE_V3_PLAN.md`;
- `ml_v3/contracts/`, `ml_v3/benchmark/`, G1 fixtures or `SHA256SUMS`;
- any existing corpus file.

Generated inventories live outside the repository.

## Source map

Copy `ml_v3/prep/source_map.example.json` outside the repository and provide
one row per independent source snapshot.

Source roots must be disjoint. Parent/child roots are rejected because they
would give the same physical files multiple source identities.
Directory and audio-file symlinks are rejected rather than silently omitted.
The output directory must be outside every source root and may contain only the
four managed output files, so a previous or partial run cannot hide stale
artifacts.

`rights_status=verified` is fail-closed: both a non-empty `rights_basis` and at
least one existing, non-symlinked evidence file are required. The tool hashes
the evidence but does not decide whether the legal claim is correct. Sources
that have not been independently reviewed must remain `quarantine` or
`unknown`.

## Run

```bash
cd /Users/marco/Desktop/NEWWWWWWW/.claude/worktrees/ember-core-unified

/Users/marco/aieq_data/motore_v3/env/venv/bin/python \
  -m ml_v3.prep.corpus_inventory \
  --source-map /Users/marco/aieq_data/motore_v3/preflight/source_map.local.json \
  --out /Users/marco/aieq_data/motore_v3/preflight/inventory-current
```

The command exits non-zero if an input contract is invalid, any configured
source is empty, or any audio file cannot be probed. It still writes the
inventory for empty sources and individual audio probe failures, so those
problems can be located without silently admitting them.

Outputs:

- `inventory.jsonl`;
- `sources.lock.json`;
- `summary.json`;
- `SHA256SUMS`.

Re-running with unchanged inputs must produce byte-identical outputs.
The reader rejects duplicate JSON keys and non-finite JSON constants. Audio and
rights-evidence files are fingerprinted before and after hashing/probing; a
concurrent change makes the run NO-GO. For non-WAV containers, a formatted
decimal duration is not mislabeled exact: authoritative stream ticks and a time
base are required. The tool also verifies the three payload hashes and checksum
manifest immediately after writing them.

## Gate

This preflight is usable only when:

1. `error_count == 0`;
2. `empty_source_ids` is empty;
3. every `verified` source has hashed local evidence;
4. cross-source and within-source duplicate groups have been reviewed before
   later split assignment;
5. `SHA256SUMS` verifies;
6. no generated output is committed as a G1/G3 contract artifact without a
   separate review.
